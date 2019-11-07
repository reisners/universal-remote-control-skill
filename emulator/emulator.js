const WebSocket = require('ws');
const log4js = require('log4js');
const logger = log4js.getLogger('emulator');
logger.level = 'debug';
//logger.info("Emulator starting up");

const ws = new WebSocket('wss://ord61b4er1.execute-api.eu-west-1.amazonaws.com:443/beta', {
	headers : {
		URCID: "URC123xyz567"
	}
  });

ws.on('open', function open() {
	  ws.send("{'msg':'Aloha'}");
	  console.log("connected");
	  //logger.info("connected, aloha message sent");
	});

ws.on('message', function incoming(json) {
	try {
		var data = JSON.parse(json);
		//logger.info("received "+JSON.stringify(data));
		console.log("received "+data);	
	} catch (err) {
		console.log("error: "+err);
	}
});
