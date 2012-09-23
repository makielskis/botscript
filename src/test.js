var net = require('net');


var addon = require('../build/Release/addon.node');

bots = {};
function callback(id, k, v) {
  if (k == 'log') {
    console.log(v.substring(0, v.length - 1));
  }
}
function pull_updates() {
  var updates = addon.updates();
  for (var i in updates) {
    var id = updates[i].id;
    var k = updates[i].key;
    var v = updates[i].value;
    callback(id, k, v);
  }
}
setInterval(pull_updates, 100, this);

var server = net.createServer(function (socket) {
  socket.write('Echo!\r\n');
  bots['bot'].shutdown(function() {
    console.log("removing bot");
    bots['bot'] = undefined;
    delete bots['bot'];
    if (global.gc) {
      console.log("GC!");
      global.gc();
    }
  });
  socket.pipe(socket);
});

server.listen(1337, '127.0.0.1');

addon.loadPackages("packages", function(err, packages) {
  var packages_nice = JSON.stringify(JSON.parse(packages), ' ', 3);
  //console.log("\nPACKAGES: " + packages_nice + "\n");
});

console.log("go!");
addon.createIdentifier("oclife", "packages/pg", "http://www.pennergame.de", function(err, id) {
  console.log("creating " + id + " ...");
  config = JSON.stringify({
    username: 'oclife',
    password: 'blabla',
    package: 'packages/pg',
    server: 'http://www.pennergame.de'
  });
  motor = new addon.BotMotor(2);
  bots['bot'] = new addon.Bot(motor, callback);

  console.log("loading...");
  bots['bot'].load(config, function (err, success) {
    if (!err) {
      bots['bot'].execute("sell_set_amount", "1");
      bots['bot'].execute("sell_set_price", "1000");
      bots['bot'].execute("sell_set_active", "1");
      console.log("yeah! " + bots['bot'].identifier());
      bots['bot'].configuration(function(err, config) {
        var config_nice = JSON.stringify(JSON.parse(config), ' ', 3);
        // console.log("\nCONFIGURATION: " + config_nice + "\n");
      });
      //console.log("\nLOG: " + bot.log() + "\n");
    } else {
      console.log("error: " + err);
    }
  });
});
