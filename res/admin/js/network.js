//Websockets reception
function websocketReception(message) {
	//console.log(message);
	if(message.length) {
		//Pulse activity of VCV-Rack
		if(message[0] == "/pulse") {
			if(parseFloat(message[1]) > 0.5)
				$("#pulse").addClass("light");
			else
				$("#pulse").removeClass("light");
		}		

		//Dumps reception
		else if(message[0] == "/dump/modules") {
			if(message[1] == "start")
				cacheTmp.modules = [];
			else if(message[1] == "end")
				sendWebsockets("/dump/jacks");
			else {
				//OSC deserialization
				var module = {
					type:       "module",
					id:         parseInt(message[1], 10),
					slug:       message[2],
					name:       message[3],
					author:     message[4],
					nameInRack: message[5],
					pos:  			  {px: {x:     parseFloat(message[6]), y:      parseFloat(message[7])}},
					size: 			  {px: {width: parseFloat(message[8]), height: parseFloat(message[9])}},
					nbInputs: 	      parseInt(message[10], 10),
					nbOutputs: 	      parseInt(message[11], 10),
					nbPotentiometers: parseInt(message[12], 10),
					nbSwitches:       parseInt(message[13], 10),
					nbLights: 	      parseInt(message[14], 10),
					wiring:           {board: undefined, boards: {}},
					inputs: 	      [], 
					outputs: 	      [], 
					potentiometers:	  [], 
					switches:	      [], 
					lights:	          [], 
					jals: 		      {}
				};
				
				//Eurorack size
				module.size.mm = { 
					width: 		px2mm(module.size.px.width),
					height: 	px2mm(module.size.px.height)
				};
				module.size.eurorack = mm2rack(module.size.mm);

				//Add in container
				cacheTmp.modules.push(module);
			}
		}
		else if(message[0] == "/dump/jacks") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				sendWebsockets("/dump/potentiometers");
			else {
				//OSC deserialization
				var jack = {
					type: 		     "jacks",
					id:   	  		 parseInt(message[1], 10),
					moduleId: 		 parseInt(message[2], 10),
					inputOrOutputId: parseInt(message[3], 10),
					pos:  	  		{x: parseFloat(message[4]), y: parseFloat(message[5])},
					active:   		(parseInt(message[6], 10) > 0.5),
					isInput:  		(parseInt(message[7], 10) > 0.5)
				};

				//Module reference
				var module = cacheTmp.modules[jack.moduleId];
				if(module) {
					//Jack type (input or output)
					if(jack.isInput)	{ module.inputs [jack.inputOrOutputId] = jack; jack.type = "jacks_input";  }
					else				{ module.outputs[jack.inputOrOutputId] = jack; jack.type = "jacks_output"; }
				
					//Wiring
					jack.wiring = {board: floor(jack.id/cache.arduino.jacks.length), pin: cache.arduino.jacks[jack.id % cache.arduino.jacks.length]};
				
					//Calculate module board
					if(module.wiring.boards[jack.wiring.board] == undefined)
						module.wiring.boards[jack.wiring.board] = 0;
					module.wiring.boards[jack.wiring.board]++;
				
					//Modidy Physical Sync module : audio send/receive case
					if(module.slug == "ModiySync") {
						if(module.audio == undefined)	module.audio = {inputs: [], outputs: []};
						var jack = JSON.parse(JSON.stringify(jack));
						jack.isInput = !jack.isInput;
						if(jack.isInput) {
							jack.pos.x -= 30;
							module.audio.inputs[jack.inputOrOutputId] = jack;
							jack.type = "jacks_audio_input";
						}
						else {
							jack.pos.x += 30;
							module.audio.outputs[jack.inputOrOutputId] = jack;
							jack.type = "jacks_audio_output";
						}
						delete jack.wiring;
						jack.id = jack.inputOrOutputId+1;
						jack.wiring = {board: "audiocard", pin: jack.id};
					}
				}
			}
		}
		else if(message[0] == "/dump/potentiometers") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				sendWebsockets("/dump/switches");
			else {
				//OSC deserialization
				var potentiometer = {
					type: 	          "potentiometers",
					id:   	          parseInt(message[1], 10),
					moduleId:         parseInt(message[2], 10),
					potentiometerId:  parseInt(message[3], 10),
					pos:  	          {x: parseFloat(message[4]), y: parseFloat(message[5])}
				};

				//Module reference
				var module = cacheTmp.modules[potentiometer.moduleId];
				if(module) {
					//Wiring
					potentiometer.wiring = {board: floor(potentiometer.id / cache.arduino.potentiometers.length), pin: cache.arduino.potentiometers[potentiometer.id % cache.arduino.potentiometers.length]};
				
					//Calculate module board
					if(module.wiring.boards[potentiometer.wiring.board] == undefined)
						module.wiring.boards[potentiometer.wiring.board] = 0;
					module.wiring.boards[potentiometer.wiring.board]++;

					//Add in container
					module.potentiometers[potentiometer.potentiometerId] = potentiometer;
				}
			}
		}
		else if(message[0] == "/dump/switches") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				sendWebsockets("/dump/leds");
			else {
				//OSC deserialization
				var button = {
					type: 	  "switchesM",
					id:   	  parseInt(message[1], 10),
					moduleId: parseInt(message[2], 10),
					switchId: parseInt(message[3], 10),
					pos:  	  {x: parseFloat(message[4]), y: parseFloat(message[5])},
					isToggle: parseInt(message[6], 10),
					hasLED:   parseInt(message[7], 10)
				};
				if(button.isToggle)
					button.type = "switchesT";

				//Module reference
				var module = cacheTmp.modules[button.moduleId];
				if(module) {
					//Wiring
					button.wiring = {board: floor(button.id / cache.arduino.switches.length), pin: cache.arduino.switches[button.id % cache.arduino.switches.length]};
				
					//Calculate module board
					if(module.wiring.boards[button.wiring.board] == undefined)
						module.wiring.boards[button.wiring.board] = 0;
					module.wiring.boards[button.wiring.board]++;

					//Add in container
					module.switches[button.switchId] = button;
				}
			}
		}
		else if(message[0] == "/dump/leds") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				updateCache("end");
			else {
				//OSC deserialization
				var led = {
					type: 	  "leds",
					id:   	  parseInt(message[1], 10),
					moduleId: parseInt(message[2], 10),
					lightId:  parseInt(message[3], 10),
					pos:  	  {x: parseFloat(message[4]), y: parseFloat(message[5])},
				};

				//Module reference
				var module = cacheTmp.modules[led.moduleId];
				if(module) {
					//Wiring
					led.wiring = {board: floor(led.id / cache.arduino.leds.length), pin: cache.arduino.leds[led.id % cache.arduino.leds.length]};
				
					//Calculate module board
					if(module.wiring.boards[led.wiring.board] == undefined)
						module.wiring.boards[led.wiring.board] = 0;
					module.wiring.boards[led.wiring.board]++;

					//Add in container
					module.lights[led.lightId] = led;
				}
			}
		}
	}
}

//Websocket status
function websocketOpened() {
	console.log("Connected");
	updateCache()
}
function websocketClosed() {
	console.log("Disconnected");
}