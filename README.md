# Malta Block Pricefeed

A pricefeed contract using [LiquidOracle](https://docs.liquidapps.io/en/stable/developers/harmony-getting-started.html) and [LiquidScheduler](https://docs.liquidapps.io/en/stable/developers/cron-getting-started.html) services from LiquidApps.

It works cross-chain on WAX by using LiquidX to stake DAPP from a WAX account to an EOS DSP.

## Creating a new pricefeed for an asset

The pricefeed contract allows adding a feed for any asset. The required parameters are:
1. API endpoint for the current price
1. An interval in seconds to repeatedly fetch the price
1. The strategy to use for price aggregation: Possible strategies are median or a mean computation over the latest X trailing prices from the feed.
The `settokencfg` action is used to create a new price feed or change an existing pricefeed's parameters.

#### settokencfg parameters

1. `symbolCode`: The symbol code used to identify this price feed.
1. `uri`: The API endpoint for fetching the latest price. Supports the https+JSON syntax for extracting prices from a JSON response.
1. `fetch_interval_seconds`: The number of seconds after which a new price is fetched.
1. `strategy`: The strategy to use for aggregation. 0 for mean computation, 1 for median computation.
1. `average_over`: The number of price points to apply the strategy over for price aggregation.
1. `initialPrice`: To set the initial price of the asset until price points are available.
1. `active`: Used for (de-)activating new price data fetches.

#### Example

These parameters can be used to create a pricefeed for WAX with prices being fetched every 5 minutes from [coingecko](https://coingecko.com), taking the median of the price data over the last 24 hours.

```js
{
    symbolCode: "WAX",
    uri: Buffer.from(
      "https+json://wax.usd/api.coingecko.com/api/v3/simple/price?ids=wax&vs_currencies=usd",
      "utf8"
    ),
    average_over: 288, // 288 * 5 minutes = 24 hours
    fetch_interval_seconds: 5 * 60, // 5 minutes
    active: true,
    initialPrice: 0,
    strategy: 1, // median
}
```


## Reading the latest price on chain

The latest aggregated price can be found in the `priceaverage` table using the token symbol as scope (f.i., `WAX`):

https://wax.bloks.io/account/globaloracle?loadContract=true&tab=Tables&table=priceaverage&account=globaloracle&scope=WAX&limit=100

#### priceaverage

**Scope**: The scope is the `symbolCode` that was used in the `settokencfg` action. The latest price is the single entry of that table.

**Row Result:**
1. `price`: The aggregated price
1. `updated_at`: The last time the price was aggregated due to receiving a new spot price


#### Example

```js
{
    "price": "0.05311600118875504",
    "updated_at": "2020-06-22T09:52:00"
}
```

## Development

Needs to be unpacked in a zeus environment: https://docs.liquidapps.io/en/v2.0/developers/zeus-getting-started.html#create-your-own-contract

To compile for WAX, enable the `LIQUIDX` option and set the sidechain option:

```bash
# in pricefeed.cpp
#define LIQUIDX

# compile using the sidechain option
zeus compile pricefeed --sidechain liquidxxxwax
```

To set up LiquidApps services on WAX follow [this article](https://medium.com/the-liquidapps-blog/using-liquidoracles-on-eos-wax-telos-liquidapps-dapp-network-developer-walkthroughs-e810087e58e2).

## License

Malta Block Pricefeed is [MIT licensed](./LICENSE).

