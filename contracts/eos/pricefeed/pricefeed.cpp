#include "../dappservices/oracle.hpp"

#define DAPPSERVICES_ACTIONS() \
  XSIGNAL_DAPPSERVICE_ACTION \
  ORACLE_DAPPSERVICE_ACTIONS
#define DAPPSERVICE_ACTIONS_COMMANDS() \
  ORACLE_SVC_COMMANDS() 

#define CONTRACT_NAME() pricefeed 
// define custom filter
#undef ORACLE_HOOK_FILTER
#define ORACLE_HOOK_FILTER(uri, data) filter_result(uri, data);
CONTRACT_START()
  void filter_result(std::vector<char> uri, std::vector<char> data){
    //eosio::check(data.size() > 3, "result too small");
  }
  // use singleton to hold minimum amount of DSPs that need to be heard from to consider a response valid
  TABLE threshold {
      uint32_t   threshold = 1; // defaults to 1 so if 1 DSP returns a value, the oracle fetch is deemed valid
  };
  typedef eosio::singleton<"threshold"_n, threshold> threshold_t;

   ACTION getdebt() {
    string url ="https://www.treasurydirect.gov/NP_WS/debt/current?format=json";
    vector<char> urlv(url.begin(), url.end());
    auto debt = getURI(urlv, [&]( auto& results ) {
      eosio::check(results.size() > 0, "no results returned");
      auto itr = results.begin();
      return itr->result;
    });
 
    string debtstr(debt.begin(), debt.end());
    uint8_t start_loc = debtstr.find("totalDebt") + 11;
    string debtsubstr = debtstr.substr(start_loc, debtstr.length() - start_loc - 1);
    eosio::check(false,"The current debt is " + debtsubstr);
  }

CONTRACT_END((getdebt))
