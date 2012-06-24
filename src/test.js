var net = require('net');

var server = net.createServer(function (socket) {
  socket.write('Echo server\r\n');
  socket.pipe(socket);
});

server.listen(1337, '127.0.0.1');

var addon = require('./build/Release/addon.node');

config = '{"username": "oclife", "package": "packages/pg", "modules": {"sell": {"active": "0", "amount": "0", "continuous": "1", "price": "50"}, "collect": {"active": "1"}}, "server": "http://berlin.pennergame.de", "proxy": "", "password": "blabla", "wait_time_factor": "1"}'

function callback(id, k, v) {
  console.log("[[[callback: [" + id + "][" + k + "]: " + v + "]]]");
}

var motor = new addon.BotMotor(2);
var bot = new addon.Bot(motor, config);
bot.setCallback(callback);
bot.execute("collect_set_active", "1");