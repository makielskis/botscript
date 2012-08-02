var net = require('net');

var server = net.createServer(function (socket) {
  socket.write('Echo server\r\n');
  socket.pipe(socket);
});

server.listen(1337, '127.0.0.1');

var addon = require('../build/Release/addon.node');

function callback(id, k, v) {
  if (k == 'log') {
    console.log(v.substring(0, v.length - 1));
  }
}

addon.loadPackages("packages", function(err, packages) {
  var packages_nice = JSON.stringify(JSON.parse(packages), ' ', 3);
  console.log("\nPACKAGES: " + packages_nice + "\n");
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
  bot = new addon.Bot(motor, callback);
  console.log("loading...");
  bot.load(config, function(err, success) {
    if (!err) {
      bot.execute("collect_set_active", "1");
      console.log("yeah! " + bot.identifier());
      bot.configuration(function(err, config) {
        var config_nice = JSON.stringify(JSON.parse(config), ' ', 3);
        console.log("\nCONFIGURATION: " + config_nice + "\n");
      });
      console.log("\nLOG: " + bot.log() + "\n");
    } else {
      console.log("error: " + err);
    }
  });
});
