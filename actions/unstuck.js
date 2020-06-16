const initEnvironment = require(`eosiac`);

const network = process.env.EOSIAC_ENV || `kylin`
const { sendTransaction, env } = initEnvironment(
  network,
  { verbose: true }
);

const accounts = Object.keys(env.accounts);

const CONTRACT_ACCOUNT = accounts[accounts.length - 1]

async function action() {
  try {
    await sendTransaction([
      {
        account: CONTRACT_ACCOUNT,
        name: `unstuck`,
        authorization: [
          {
            actor: CONTRACT_ACCOUNT,
            permission: `active`
          }
        ],
        data: {
          symbolCode: `WAX`
        }
      }
    ]);
    process.exit(0);
  } catch (error) {
    // ignore
    process.exit(1);
  }
}

action();
