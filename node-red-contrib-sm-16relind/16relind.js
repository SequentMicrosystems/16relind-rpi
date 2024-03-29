module.exports = function(RED) {
    "use strict";
    var I2C = require("i2c-bus");
    const DEFAULT_HW_ADD = 0x20;
    const ALTERNATE_HW_ADD = 0x38;
    const OUT_REG = 0x02;
    const CFG_REG = 0x06;
    const mask = new ArrayBuffer(16);
	mask[0] = 0x8000;
	mask[1] = 0x4000;
	mask[2] = 0x2000;
	mask[3] = 0x1000;
	mask[4] = 0x800;
	mask[5] = 0x400;
	mask[6] = 0x200;
	mask[7] = 0x100;
	mask[8] = 0x80;
	mask[9] = 0x40;
	mask[10] = 0x20;
	mask[11] = 0x10;
	mask[12] = 0x8;
	mask[13] = 0x4;
	mask[14] = 0x2;
	mask[15] = 0x1;
    // The relay Node
    function RelayNode(n) {
        RED.nodes.createNode(this, n);
        this.stack = parseInt(n.stack);
        this.relay = parseInt(n.relay);
        this.payload = n.payload;
        this.payloadType = n.payloadType;
        var node = this;
		var buffer = Buffer.alloc(2);
 
        node.port = I2C.openSync( 1 );
        node.on("input", function(msg) {
            var myPayload;
            var stack = node.stack;
            if (isNaN(stack)) stack = msg.stack;
            stack = parseInt(stack);
            var relay = msg.relay; // 
            if (isNaN(relay)) relay = node.relay;
            relay = parseInt(relay);
            //var buffcount = parseInt(node.count);
            if (isNaN(stack + 1)) {
                this.status({fill:"red",shape:"ring",text:"Stack level ("+stack+") value is missing or incorrect"});
                return;
            } else if (isNaN(relay) ) {
                this.status({fill:"red",shape:"ring",text:"Relay number  ("+relay+") value is missing or incorrect"});
                return;
            } else {
                this.status({});
            }
            var hwAdd = DEFAULT_HW_ADD;
            var found = 1;
            if(stack < 0){
                stack = 0;
            }
            if(stack > 7){
              stack = 7;
            }
            //check the type of io_expander
            hwAdd += stack ^ 0x07;
            var direction = 0xaa;
            try{
                direction = node.port.readWordSync(hwAdd, CFG_REG );
            }catch(err) {
                hwAdd = ALTERNATE_HW_ADD;
                hwAdd += stack ^ 0x07;
                try{
                    direction = node.port.readWordSync(hwAdd, CFG_REG );
                }catch(err) {
                    found = 0;
                    this.error(err,msg);
                }
            }
            
            if(1 == found){
            try {
                if (this.payloadType == null) {
                    myPayload = this.payload;
                } else if (this.payloadType == 'none') {
                    myPayload = null;
                } else {
                    myPayload = RED.util.evaluateNodeProperty(this.payload, this.payloadType, this,msg);
                }
                //node.log('Direction ' + String(direction));   
                if(direction != 0x00){
					
                    node.port.writeWordSync(hwAdd, OUT_REG, 0x00);
                    //node.log('First update output');   
                    node.port.writeWordSync(hwAdd, CFG_REG, 0x00);
                    //node.log('First update direction');  
                }
                var relayVal = 0;    
                
                //node.log('Relays ' + String(relayVal));
                if(relay < 0){
                  relay = 0;
                }
                if(relay > 16){
                  relay = 16;
                }
                if(relay > 0)
                {
                  relayVal = node.port.readWordSync(hwAdd, OUT_REG);
                  relay-= 1;//zero based
                  if (myPayload == null || myPayload == false || myPayload == 0 || myPayload == 'off') {
                    relayVal &= ~mask[relay];
                  } else {
                    relayVal |= mask[relay];
                  }
                }
                else if(myPayload >= 0 && myPayload < 65536)
                {
                  var i = 0; 
                  for(i = 0; i<16; i++){
                    if(( (1 << i) & myPayload) != 0){
                        relayVal |= mask[i];
                    }                   
                  }
                }
                node.port.writeWord(hwAdd, OUT_REG, relayVal,  function(err) {
                    if (err) { node.error(err, msg);
                    } else {
                      node.send(msg);
                    }
                });
            } catch(err) {
                this.error(err,msg);
            }
          }
        });

        node.on("close", function() {
            node.port.closeSync();
        });
    }
    RED.nodes.registerType("16relind", RelayNode);

    // The relay read Node
    function RelayReadNode(n) {
        RED.nodes.createNode(this, n);
        this.stack = parseInt(n.stack);
        this.relay = parseInt(n.relay);
        this.payload = n.payload;
        this.payloadType = n.payloadType;
        var node = this;
 
        node.port = I2C.openSync( 1 );
        node.on("input", function(msg) {
            var myPayload;
            var stack = node.stack;
            if (isNaN(stack)) stack = msg.stack;
            stack = parseInt(stack);
            var relay = msg.relay;
            if (isNaN(relay)) relay = node.relay;
            relay = parseInt(relay);
            //var buffcount = parseInt(node.count);
            if (isNaN(stack + 1)) {
                this.status({fill:"red",shape:"ring",text:"Stack level ("+stack+") value is missing or incorrect"});
                return;
            } else if (isNaN(relay) ) {
                this.status({fill:"red",shape:"ring",text:"Relay number  ("+relay+") value is missing or incorrect"});
                return;
            } else {
                this.status({});
            }
            var hwAdd = DEFAULT_HW_ADD;
            var found = 1;
            if(stack < 0){
                stack = 0;
            }
            if(stack > 7){
              stack = 7;
            }
            //check the type of io_expander
            hwAdd += stack ^ 0x07;
            var direction = 0xaa;
            try{
                direction = node.port.readWordSync(hwAdd, CFG_REG );
            }catch(err) {
                hwAdd = ALTERNATE_HW_ADD;
                hwAdd += stack ^ 0x07;
                try{
                    direction = node.port.readWordSync(hwAdd, CFG_REG );
                }catch(err) {
                    found = 0;
                    this.error(err,msg);
                }
            }
            
            if(1 == found){
            try {
                if (this.payloadType == null) {
                    myPayload = this.payload;
                } else if (this.payloadType == 'none') {
                    myPayload = null;
                } else {
                    myPayload = RED.util.evaluateNodeProperty(this.payload, this.payloadType, this,msg);
                }
                
                if(direction != 0x00){
                    node.port.writeWordSync(hwAdd, OUT_REG, 0x00);
                     
                    node.port.writeWordSync(hwAdd, CFG_REG, 0x00);
                }
                var relayVal = 0;    
                relayVal = node.port.readWordSync(hwAdd, OUT_REG);
                if(relay < 0){
                  relay = 0;
                }
                if(relay > 16){
                  relay = 16;
                }
                if(relay > 0){
                  relay-= 1;//zero based
                  if(relayVal & mask[relay])
                  {
                    msg.payload = 1;
                  }
                  else
                  {
                    msg.payload = 0;
                  }
                }
                else {
                  var i;
                  var outVal;
                  outVal = 0;
                  for(i = 0; i< 16; i++)
                  {
                    if( relayVal & mask[i])
                    {
                      outVal += 1 << i;
                    }
                  }
                  msg.payload = outVal;                 
                }               
                node.send(msg);               
            } catch(err) {
                this.error(err,msg);
            }
          }
        });

        node.on("close", function() {
            node.port.closeSync();
        });
    }
    RED.nodes.registerType("16relindrd", RelayReadNode);

}
