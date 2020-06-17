# Malta Block Pricefeed

A pricefeed contract using Oracle Service from LiquidApps.
Works on WAX network through LiquidX.

## Dev

To compile for WAX set the sidechain option:

```bash
zeus compile pricefeed --sidechain liquidxxxwax
```

## Pricefeed Table

The latest aggregated price can be found in the `priceaverage` table using the token symbol as scope (f.i., `WAX`):

https://wax.bloks.io/account/globaloracle?loadContract=true&tab=Tables&table=priceaverage&account=globaloracle&scope=WAX&limit=100

## License

Malta Block Pricefeed is [MIT licensed](./LICENSE).

