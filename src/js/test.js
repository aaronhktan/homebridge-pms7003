const PMS7003 = require('bindings')('homebridge-pms7003');

function read() {
  console.log(PMS7003.read());
}

function sleep(ms) {
  return new Promise(resolve => setTimeout(resolve, ms));
}

async function test() {
  PMS7003.init();

  for (i = 0; i < 30; i++) {
    console.log(PMS7003.read(1000));
    await sleep(1000);
  }

  PMS7003.deinit();
}

test();

