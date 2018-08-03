//Prototypes for Strings
String.prototype.trim = function()
	{ return (this.replace(/^[\s\xA0]+/, "").replace(/[\s\xA0]+$/, "")) }
String.prototype.startsWith = function(str) 
	{ return (this.match("^"+str)==str) }
String.prototype.endsWith = function(str) 
	{ return (this.match(str+"$")==str) }
String.prototype.replaceAll = function(str, str2)
	{ return (this.replace(new RegExp(str, 'g'), str2)) }
String.prototype.pad = function(length) {
	var str = '' + this;
    while (str.length < length) {
        str = '0' + str;
    }
    return str;
}
Array.prototype.clean = function(deleteValue) {
  for (var i = 0; i < this.length; i++) {
    if (this[i] == deleteValue) {         
      this.splice(i, 1);
      i--;
    }
  }
  return this;
};
function addS(nb) {
	if(nb > 1)
		return "s";
	return "";
}
 
 
 
//Réseau
function Network() {
    this.URI                = "";
    this.websocket          = null;
    this.intervalId         = null;
    this.disconnectionAsked = false;
}
Network.prototype.connect = function(URI) {
    this.disconnectionAsked = false;
    if (typeof URI !== "undefined")
        this.URI = URI;
 
    try {
        if (this.websocket) {
            if (this.connected())
                this.websocket.close();
            delete this.websocket;
        }
         
        if (typeof MozWebSocket === 'function')
            WebSocket = MozWebSocket;
             
        this.websocket = new WebSocket(this.URI);
         
        this.websocket.onopen = function(evt) {
            websocketChanged(this.websocket);
        }.bind(this);
         
        this.websocket.onclose = function(evt) {
            websocketChanged(this.websocket);
            if (!this.disconnectionAsked) {
                setTimeout(this.connect.bind(this), 500);
            }
            delete this.websocket;
        }.bind(this);
         
        this.websocket.onmessage = function(evt) {
			//Cleaning des messages
			var message = evt.data.split(new RegExp(" +(?=(?:[^\']*\'[^\']*\')*[^\']*$)"));
			for(var i = 0 ; i < message.length ; i++) {
				if(message[i].indexOf("'") >= 0)
					message[i] = message[i].replaceAll("'", "");
			}
			websocketReception(message);
        }.bind(this);
        this.websocket.onerror = function(evt) {
            console.warn("Websocket error:", evt.data);
        };
        websocketChanged(this.websocket);
    }
    catch(exception) {
        console.error("Websocket fatal error, maybe your browser can't use websockets. You can look at the javascript console for more details on the error.");
        console.error("Websocket fatal error", exception);
    }
}
 
Network.prototype.connected = function() {
    if (this.websocket && this.websocket.readyState == 1)
        return true;
    return false;
};
 
Network.prototype.reconnect = function() {
    if (this.connected())
        this.disconnect();
    this.connect();
}
 
Network.prototype.disconnect = function() {
    this.disconnectionAsked = true;
    if (this.connected()) {
        this.websocket.close();
        websocketChanged(this.websocket);
    }
}
 
Network.prototype.send = function(message) {
    if (this.connected())
        this.websocket.send(message);
};
 
Network.prototype.checkSocket = function() {
    if (this.websocket) {
        var stateStr;
        switch (this.websocket.readyState) {
            case 0:
                stateStr = "CONNECTING";
                break;
            case 1:
                stateStr = "OPEN";
                break;
            case 2:
                stateStr = "CLOSING";
                break;
            case 3:
                stateStr = "CLOSED";
                break;
            default:
                stateStr = "UNKNOW";
                break;
        }
        console.log("Websocket state : " + this.websocket.readyState + " (" + stateStr + ")");
    }
    else
        console.log("Websocket is not initialised");
}
var readyStateBefore = -1;
var websocketIsOpened = false;
function websocketChanged(websocket) {
    if(websocket != null) {
        var stateStr;
        if(readyStateBefore != websocket.readyState) {
            readyStateBefore = websocket.readyState;
            switch (websocket.readyState) {
                case 0:
                    stateStr = "CONNECTING";
                    break;
                case 1:
                    stateStr = "OPEN";
                    websocketIsOpened = true;
                    try {
                        websocketOpened();
                    }
                    catch(e) {}
                    break;
                case 2:
                    stateStr = "CLOSING";
                    break;
                case 3:
                    stateStr = "CLOSED";
                    try {
                        websocketClosed();
                    }
                    catch(e) {}
                    websocketIsOpened = false;
                    break;
                default:
                    stateStr = "UNKNOW";
                    break;
            }
            console.log("Socket state changed : " + websocket.readyState + " (" + stateStr + ")");
        }
    }
}
//Envoi websockets
var network = new Network(), messagePrefix = "", lastMessage;
function sendWebsocketsArray(messageArray, ipServeur) {
    var message = "";
    for(var i = 0 ; i < messageArray.length ; i++) {
        if((messageArray[i].indexOf(" ") == -1) && (messageArray[i].length))
            message += " " + messageArray[i];
        else
            message += " \"" + messageArray[i] + "\"";
    }
    sendWebsockets(message.trim(), ipServeur);
}
function sendWebsockets(message, ipServeur) {
    if(network) {
        if((ipServeur != undefined) && (!network.connected())) {
            //if(ipServeur == undefined)    ipServeur = window.location.hostname;
            if(ipServeur == undefined)  ipServeur = "127.0.0.1";
            network.connect("ws://" + ipServeur + ":5553");
        }
        else if((ipServeur == undefined) && (websocketIsOpened) && (message != undefined)) {
            lastMessage = messagePrefix + message;
            network.send(lastMessage);
        }
    }
}
 
function getQueryVariable(variable) {
    var query = window.location.search.substring(1);
    var vars = query.split("&");
    for (var i=0;i<vars.length;i++) {
        var pair = vars[i].split("=");
        if (pair[0] == variable)
            return pair[1];
    } 
}
