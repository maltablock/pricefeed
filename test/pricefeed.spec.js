require('mocha');
const { assert } = require('chai'); // Using Assert style
const { getTestContract, getLocalDSPEos } = require('../extensions/tools/eos/utils');
const artifacts = require('../extensions/tools/eos/artifacts');
const deployer = require('../extensions/tools/eos/deployer');
const { genAllocateDAPPTokens } = require('../extensions/tools/eos/dapp-services');
var contractCode = 'pricefeed';
var ctrt = artifacts.require(`./${contractCode}/`);

const delay = ms => new Promise(res => setTimeout(res, ms));
const AverageStrategies = {
  'AVERAGE': 0,
  'MEDIAN': 1,
}

describe(`Debt Getter Test`, () => {
  var testcontract;
  const code = 'test1';
  var dspeos;

  before(done => {
    (async() => {
      try {
        var deployedContract = await deployer.deploy(ctrt, code);
        const services = [`cron`, `oracle`];
        for (const service of services) {
          console.log(`allocating`, service);
          await genAllocateDAPPTokens(deployedContract, service);
        }
        // await genAllocateDAPPTokens(deployedContract, "oracle", "pprovider1", "default");
        // await genAllocateDAPPTokens(deployedContract, "oracle", "pprovider2", "foobar");
        testcontract = await getTestContract(code);
        dspeos = await getLocalDSPEos(code);
        done();
      }
      catch (e) {
        done(e);
      }
    })();
  });
 
  var account = code;
  
  // it('Debt HTTPS Get', done => {
  //   (async() => {
  //     try {
  //       var res = await testcontract.getdebt({
  //         // action arguments would go here, but getdebt takes none
    
  //       }, {
  //         authorization: `${code}@active`,
  //         broadcast: true,
  //         sign: true
  //       });
  //       done();
  //     }
  //     catch (e) {
  //       done(e);
  //     }
  //   })();
  // });

  it("Oracle - feed every 2 seconds + average", done => {
    (async () => {
      try {
        const apiEndpoint = `min-api.cryptocompare.com/data/price?fsym=EOS&tsyms=USD`;
        const uri = `https+json://USD/${apiEndpoint}`;
        let res = await testcontract.settokencfg(
          {
            symbolCode: `EOS`,
            uri: Buffer.from(uri, "utf8"),
            average_over: 3,
            fetch_interval_seconds: 3,
            active: true,
            strategy: AverageStrategies.AVERAGE,
            initialPrice: 0,
          },
          {
            authorization: `${code}@active`,
            broadcast: true,
            sign: true
          }
        );

        const tokenConfig = (await dspeos.getTableRows({
          json: true,
          scope: `EOS`,
          code: code,
          table: "tokenconfig",
          limit: 1
        })).rows[0];
        // console.log(tokenConfig);
        assert.deepInclude(
          tokenConfig,
          {
            average_over: 3,
            fetch_interval: { _count: 3 * 1e6 },
            active: 1,
            strategy: 0,
          },
          "wrong tokenconfig"
        );
        assert.equal(
          Buffer.from(tokenConfig.uri, `hex`).toString(`utf8`),
          uri,
          "wrong tokenconfig uri"
        );

        // wait for all three oracle actions to run
        await delay(23000);

        const priceFeeds = (await dspeos.getTableRows({
          json: true,
          scope: `EOS`,
          code: code,
          table: "pricefeed",
          limit: 100
        })).rows;
        console.log(`priceFeeds`, priceFeeds);
        const priceAverage = (await dspeos.getTableRows({
          json: true,
          scope: `EOS`,
          code: code,
          table: "priceaverage",
          limit: 1
        })).rows[0];
        console.log(`priceAverage`, priceAverage);

        assert.equal(priceFeeds.length, 3, "wrong priceFeed length");

        const expectedAverage =
          priceFeeds.reduce((acc, p) => acc + Number.parseFloat(p.price), 0.0) /
          priceFeeds.length;
        console.log({ expectedAverage, receivedAverage: priceAverage.price });
        assert.closeTo(
          Number.parseFloat(priceAverage.price),
          expectedAverage,
          0.00001,
          "price average is incorrect"
        );

        done();
      } catch (e) {
        done(e);
      }
    })();
  });

  it("Oracle - feed every 2 seconds + median", done => {
    (async () => {
      try {
        const apiEndpoint = `min-api.cryptocompare.com/data/price?fsym=EOS&tsyms=USD`;
        const uri = `random://1024/1`;
        const AVERAGE_OVER = 7
        let res = await testcontract.settokencfg(
          {
            symbolCode: `EOSTWO`,
            uri: Buffer.from(uri, "utf8"),
            average_over: AVERAGE_OVER,
            fetch_interval_seconds: 3,
            active: true,
            strategy: AverageStrategies.MEDIAN,
            initialPrice: 0,
          },
          {
            authorization: `${code}@active`,
            broadcast: true,
            sign: true
          }
        );

        const tokenConfig = (await dspeos.getTableRows({
          json: true,
          scope: `EOSTWO`,
          code: code,
          table: "tokenconfig",
          limit: 1
        })).rows[0];
        // console.log(tokenConfig);
        assert.deepInclude(
          tokenConfig,
          {
            average_over: AVERAGE_OVER,
            fetch_interval: { _count: 3 * 1e6 },
            active: 1,
            strategy: 1,
          },
          "wrong tokenconfig"
        );
        assert.equal(
          Buffer.from(tokenConfig.uri, `hex`).toString(`utf8`),
          uri,
          "wrong tokenconfig uri"
        );

        // wait for all three oracle actions to run
        await delay((AVERAGE_OVER * 4) * 1000);

        const priceFeeds = (await dspeos.getTableRows({
          json: true,
          scope: `EOSTWO`,
          code: code,
          table: "pricefeed",
          limit: 100
        })).rows;
        const priceAverage = (await dspeos.getTableRows({
          json: true,
          scope: `EOSTWO`,
          code: code,
          table: "priceaverage",
          limit: 1
        })).rows[0];
        console.log(priceAverage);
        
        assert.equal(priceFeeds.length, AVERAGE_OVER, "wrong priceFeed length");
        
        const sortedFeeds = priceFeeds.sort((a,b) => b.price - a.price)
        console.log(`sortedFeeds`, sortedFeeds);
        let expectedMedian = Number.parseFloat(sortedFeeds[Math.floor(sortedFeeds.length/2)].price)
        // even => average
        if(sortedFeeds.length % 2 === 0) {
          expectedMedian += Number.parseFloat(sortedFeeds[Math.floor(sortedFeeds.length/2) - 1].price)
          expectedMedian /= 2;
        }
        console.log({ expectedMedian, receivedAverage: priceAverage.price });

        assert.closeTo(
          Number.parseFloat(priceAverage.price),
          expectedMedian,
          0.00001,
          "price median is incorrect"
        );

        done();
      } catch (e) {
        done(e);
      }
    })();
  });

  it("set initial price works", done => {
    (async () => {
      try {
        const initialPrice = 0.02
        let res = await testcontract.settokencfg(
          {
            symbolCode: `PHOENIX`,
            uri: Buffer.from(``, "utf8"),
            average_over: 0,
            fetch_interval_seconds: 0,
            active: false,
            strategy: AverageStrategies.AVERAGE,
            initialPrice: initialPrice,
          },
          {
            authorization: `${code}@active`,
            broadcast: true,
            sign: true
          }
        );

        const tokenConfig = (await dspeos.getTableRows({
          json: true,
          scope: `PHOENIX`,
          code: code,
          table: "tokenconfig",
          limit: 1
        })).rows[0];
        console.log(tokenConfig);
        assert.deepInclude(
          tokenConfig,
          {
            active: 0,
            uri: '',
          },
          "wrong tokenconfig"
        );

        const priceAverage = (await dspeos.getTableRows({
          json: true,
          scope: `PHOENIX`,
          code: code,
          table: "priceaverage",
          limit: 1
        })).rows[0];
        console.log(priceAverage);

        assert.closeTo(
          Number.parseFloat(priceAverage.price),
          initialPrice,
          0.00001,
          "price average is incorrect"
        );


        done();
      } catch (e) {
        done(e);
      }
    })();
  });
});