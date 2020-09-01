#define LIQUIDX

#include <cmath>
#include "../dappservices/cron.hpp"
#include "../dappservices/oracle.hpp"

#define DAPPSERVICES_ACTIONS()                                                 \
  XSIGNAL_DAPPSERVICE_ACTION                                                   \
  CRON_DAPPSERVICE_ACTIONS                                                     \
  ORACLE_DAPPSERVICE_ACTIONS

#define DAPPSERVICE_ACTIONS_COMMANDS()                                         \
  CRON_SVC_COMMANDS()                                                          \
  ORACLE_SVC_COMMANDS()


#define CONTRACT_NAME() pricefeed

#define TEST_ACTIONS

// define custom filter
// #undef ORACLE_HOOK_FILTER
// #define ORACLE_HOOK_FILTER(uri, data) filter_result(uri, data);

struct timer_payload {
  symbol_code symbolCode;
};

float stofloat(std::string s) {
  std::size_t i = s.find(".");
  int digits = s.length();
  double toDivideBy = 1.0;
  if (i != std::string::npos) {
    digits = s.length() - i - 1;
    s.erase(i, 1);
    toDivideBy = pow(10, digits);
  }
  return atoi(s.c_str()) / toDivideBy;
}

enum AvergageStrategy : uint8_t {
  AVERAGE = 0,
  MEDIAN = 1,
};

CONTRACT_START()
void filter_result(std::vector<char> uri, std::vector<char> data) {
  // eosio::check(data.size() > 3, "result too small");
}

// use singleton to hold minimum amount of DSPs that need to be heard from to
// consider a response valid
TABLE threshold {
  uint32_t threshold = 1; // defaults to 1 so if 1 DSP returns a value, the
                          // oracle fetch is deemed valid
};
typedef eosio::singleton<"threshold"_n, threshold> threshold_t;

TABLE tokenconfig {
  std::vector<char> uri;
  int32_t average_over = (days(1).count() / minutes(5).count());
  eosio::microseconds fetch_interval = minutes(5);
  bool active = false;
  uint8_t strategy = 0;
};
typedef eosio::singleton<"tokenconfig"_n, tokenconfig> tokenconfig_t;

TABLE priceaverage {
  double price = 0.0;
  eosio::time_point_sec updated_at = eosio::time_point_sec(0);
};
typedef eosio::singleton<"priceaverage"_n, priceaverage> priceaverage_t;

TABLE prices {
  uint64_t counter = 0;
  double price = 0.0;
  eosio::time_point_sec time = eosio::time_point_sec(0);

  uint64_t primary_key() const { return counter; }
  double get_price() const { return price; }
};
typedef eosio::multi_index<
    "pricefeed"_n, prices,
    indexed_by<"byprice"_n,
               const_mem_fun<prices, double, &prices::get_price>>>
    pricefeed_t;

bool timer_callback(name timer, std::vector<char> payload, uint32_t seconds) {
  // timer should only be called as deferred transaction with our auth
  // TODO: enable auth check ? or are SVC commands secured automatically

  timer_payload unpacked_payload = eosio::unpack<timer_payload>(payload);

  tokenconfig_t cfg_table(get_self(), unpacked_payload.symbolCode.raw());
  if (!cfg_table.exists())
    return false;
  tokenconfig cfg = cfg_table.get();
  if (!cfg.active)
    return false;

  priceaverage_t priceavg_table(get_self(), unpacked_payload.symbolCode.raw());

  if (priceavg_table.exists()) {
    priceaverage price = priceavg_table.get();
    const auto diff_us = eosio::current_time_point() - price.updated_at;
    if (diff_us < cfg.fetch_interval) {
      // fetch interval since last update not reached yet
      schedule_timer(name(unpacked_payload.symbolCode.raw()), payload,
                     diff_us.to_seconds() + 1);
      return false;
    }
  }

  invokeoracle(unpacked_payload.symbolCode);

  // reschedule manually because fetch_interval might have changed
  schedule_timer(name(unpacked_payload.symbolCode.raw()), payload,
                 cfg.fetch_interval.to_seconds());
  return false;
}

ACTION settokencfg(symbol_code symbolCode, std::vector<char> uri,
                   uint32_t average_over, uint32_t fetch_interval_seconds,
                   bool active, uint8_t strategy, double initialPrice) {
  require_auth(get_self());

  tokenconfig_t cfg_table(get_self(), symbolCode.raw());
  tokenconfig cfg = cfg_table.get_or_default(tokenconfig{});
  if (uri.size() > 0)
    cfg.uri = uri;
  if (average_over > 0)
    cfg.average_over = average_over;
  if (fetch_interval_seconds > 0)
    cfg.fetch_interval = seconds(fetch_interval_seconds);
  cfg.active = active;
  cfg.strategy = strategy;

  cfg_table.set(cfg, get_self());

  if (cfg.active) {
    timer_payload payload = timer_payload{.symbolCode = symbolCode};
    std::vector<char> packed_payload = eosio::pack(payload);
    schedule_timer(name(payload.symbolCode.raw()), packed_payload, 1);
  }

  if (initialPrice > 0.0) {
    priceaverage_t priceavg_table(get_self(), symbolCode.raw());
    priceaverage priceavg = priceaverage{
        .updated_at = eosio::current_time_point(), .price = initialPrice};
    priceavg_table.set(priceavg, get_self());
  }
}

ACTION unstuck(symbol_code symbolCode) {
  require_auth(get_self());

  tokenconfig_t cfg_table(get_self(), symbolCode.raw());
  tokenconfig cfg = cfg_table.get_or_default(tokenconfig{});

  if (cfg.active) {
    priceaverage_t priceavg_table(get_self(), symbolCode.raw());

    if (priceavg_table.exists()) {
      priceaverage price = priceavg_table.get();
      const auto diff_us = eosio::current_time_point() - price.updated_at;
      if (diff_us > cfg.fetch_interval) {
        timer_payload payload = timer_payload{.symbolCode = symbolCode};
        std::vector<char> packed_payload = eosio::pack(payload);
        schedule_timer(name(payload.symbolCode.raw()), packed_payload, 1);
        return;
      }
    }
  }

  check(false, "no need to unstuck");
}

void invokeoracle(symbol_code symbolCode) {
  tokenconfig_t cfg_table(get_self(), symbolCode.raw());
  // always exists at this point
  check(cfg_table.exists(), "invokeoracle: should never get here");
  tokenconfig cfg = cfg_table.get();

  const auto &oracle_result =
      getURI(cfg.uri, [&](auto &results) { return results[0].result; });
  check(oracle_result.size() > 0, "empty price");
  print("\noracle:\'", std::string(oracle_result.begin(), oracle_result.end()),
        "\'", std::to_string(oracle_result.size()));
  double price =
      stofloat(std::string(oracle_result.begin(), oracle_result.end()));
  eosio::print("\nprice: ", std::to_string(price), "\n");

  if (price <= 0.000001)
    return;

  pricefeed_t pricefeed_table(get_self(), symbolCode.raw());
  pricefeed_table.emplace(get_self(), [&](auto &p) {
    p.price = price;
    p.time = eosio::current_time_point();
    p.counter = pricefeed_table.available_primary_key();
  });

  // delete oldest ones
  // amount of elements - average_over
  int32_t to_delete =
      static_cast<int32_t>(get_feed_length(symbolCode)) - cfg.average_over;
  auto itr = pricefeed_table.begin();
  for (int32_t i = 0; i < to_delete; i++) {
    itr = pricefeed_table.erase(itr);
  }

  auto priceavg = cfg.strategy == AVERAGE ? compute_average(symbolCode)
                                          : compute_median(symbolCode);

  priceaverage_t priceavg_table(get_self(), symbolCode.raw());
  priceavg_table.set(priceavg, get_self());
}

priceaverage compute_average(const eosio::symbol_code &symbolCode) {
  pricefeed_t pricefeed_table(get_self(), symbolCode.raw());

  double averaged_price = 0.0;
  uint32_t amount_of_existing_prices = 0;
  for (auto itr = pricefeed_table.lower_bound(0); itr != pricefeed_table.end();
       itr++) {
    amount_of_existing_prices++;
    averaged_price += itr->price;
  }
  averaged_price /= amount_of_existing_prices;
  return priceaverage{.updated_at = eosio::current_time_point(),
                      .price = averaged_price};
}

priceaverage compute_median(const eosio::symbol_code &symbolCode) {
  pricefeed_t pricefeed_table = pricefeed_t(get_self(), symbolCode.raw());
  int64_t feed_length = get_feed_length(symbolCode);

  auto feed_by_price = pricefeed_table.get_index<name("byprice")>();

  int64_t count = 0;
  auto median_index = (feed_length / 2);
  auto itr = feed_by_price.begin();
  while (itr != feed_by_price.end() && count < median_index) {
    itr++;
    count++;
  }

  double median_price = itr->price;
  // even => average
  if (feed_length % 2 == 0) {
    --itr;
    median_price += itr->price;
    median_price /= 2;
  }
  return priceaverage{.updated_at = eosio::current_time_point(),
                      .price = median_price};
}

int64_t get_feed_length(const eosio::symbol_code &symbolCode) {
  pricefeed_t pricefeed_table = pricefeed_t(get_self(), symbolCode.raw());
  auto oldest_price = pricefeed_table.begin();
  auto newest_price = --pricefeed_table.end();
  return newest_price->counter - oldest_price->counter + 1;
}

#ifdef TEST_ACTIONS
// just a test action should be removed
ACTION triggeroracle(symbol_code symbolCode) {
  require_auth(get_self());
  invokeoracle(symbolCode);
}

ACTION triggercron(symbol_code symbolCode) {
  require_auth(get_self());
  timer_payload payload = timer_payload{.symbolCode = symbolCode};
  std::vector<char> packed_payload = eosio::pack(payload);
  schedule_timer(name(payload.symbolCode.raw()), packed_payload, 3);
}
#endif

CONTRACT_END((settokencfg)(unstuck)
#ifdef TEST_ACTIONS
                 (triggeroracle)(triggercron)
#endif
)
