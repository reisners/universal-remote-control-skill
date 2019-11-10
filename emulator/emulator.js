const WebSocket = require('ws');
const log4js = require('log4js');
const logger = log4js.getLogger('emulator');
logger.level = 'debug';
logger.info("Emulator starting up");

function WebSocketClient(){
	this.number = 0;	// Message number
	this.autoReconnectInterval = 5*1000;	// ms
}

WebSocketClient.prototype.open = function(url, options){
	this.url = url;
	this.options = options;
	this.instance = new WebSocket(this.url, options);
	this.instance.on('open',()=>{
		this.onopen();
	});
	this.instance.on('message',(data,flags)=>{
		this.number ++;
		this.onmessage(data,flags,this.number);
	});
	this.instance.on('close',(e)=>{
		switch (e.code){
		case 1000:	// CLOSE_NORMAL
			console.log("WebSocket: closed");
			break;
		default:	// Abnormal closure
			this.reconnect(e);
			break;
		}
		this.onclose(e);
	});
	this.instance.on('error',(e)=>{
		switch (e.code){
		case 'ECONNREFUSED':
			this.reconnect(e);
			break;
		default:
			this.onerror(e);
			break;
		}
	});
}
WebSocketClient.prototype.send = function(data,option){
	try{
		this.instance.send(data,option);
	}catch (e){
		this.instance.emit('error',e);
	}
}
WebSocketClient.prototype.reconnect = function(e){
	logger.info("WebSocketClient: retry in %d ms: %s", this.autoReconnectInterval, e);
        this.instance.removeAllListeners();
	var that = this;
	setTimeout(function(){
		logger.info("WebSocketClient: reconnecting...");
		that.open(that.url, that.options);
	},this.autoReconnectInterval);
}
WebSocketClient.prototype.onopen = function(e){	console.log("WebSocketClient: open",arguments);	}
WebSocketClient.prototype.onmessage = function(data,flags,number){	console.log("WebSocketClient: message",arguments);	}
WebSocketClient.prototype.onerror = function(e){	console.log("WebSocketClient: error",arguments);	}
WebSocketClient.prototype.onclose = function(e){	console.log("WebSocketClient: closed",arguments);	}

var wsc = new WebSocketClient();
wsc.open('wss://ord61b4er1.execute-api.eu-west-1.amazonaws.com:443/beta', {
	headers : {
		URCID: "URC123xyz567"
	}
  });
wsc.onopen = function(e){
	logger.info("WebSocketClient connected: %s", e);
	this.send(JSON.stringify({'msg':'Aloha'}));
}
wsc.onmessage = function(data,flags,number){
	logger.info('WebSocketClient message #%d: %s', number,data);
}
