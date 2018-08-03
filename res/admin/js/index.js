//Démarrage du système
var app;
$(document).ready(function() {
	//Cache of images
	var loader = PIXI.Loader.shared;
	loader.add('icn_jack_audio_input',  'img/icn_jack_audio_input.png')
		  .add('icn_jack_audio_output', 'img/icn_jack_audio_output.png')
		  .add('icn_jack_input', 		'img/icn_jack_input.png')
		  .add('icn_jack_output', 		'img/icn_jack_output.png')
		  .add('icn_knob', 				'img/icn_knob.png')
		  .add('icn_led', 				'img/icn_led.png');
	loader.load((loader, resources) => {
		cache.resources = resources;
		
		//Sync with Arduino sketch (for pinouts)
		$.ajax("files/Modiy-Arduino/Modiy-Arduino.ino").done(function(arduino) {
			var arduinoCode = arduino.split("\n").clean("");
			$.each(arduinoCode, function(index, codeLine) {
				if((codeLine.indexOf("jackPinout[] = ") > 0) || (codeLine.indexOf("analogPinout[] = ") > 0) || (codeLine.indexOf("ledPinout[] = ") > 0)) {
					var codeLineValues = codeLine.split(" = ").clean("");
					if(codeLineValues.length > 0) {
						codeLineValues = codeLineValues[1].replaceAll(";", "").replaceAll("{", "").replaceAll("}", "").split(",").clean("");
						$.each(codeLineValues, function(index, codeLineValue) {
							codeLineValues[index] = codeLineValues[index].trim();
						});
					
						if     (codeLine.indexOf("jackPinout[] = ") > 0)
							cache.arduino.jacks = codeLineValues;
						else if(codeLine.indexOf("analogPinout[] = ") > 0)
							cache.arduino.potentiometers = codeLineValues;
						else if(codeLine.indexOf("ledPinout[] = ") > 0)
							cache.arduino.leds = codeLineValues;
					}
				}
			});
		}).always(function() {
			//Websockets init
			var ip = (window.location+"").split("/")[2].split(":")[0];
			if((ip.match(/\./g)||[]).length == 3)
				sendWebsockets(undefined, ip);
			else
				sendWebsockets(undefined, "127.0.0.1");
			messagePrefix = "/osc 127.0.0.1 57130 ";
		
			//LEDs refresh
			setInterval(function() {
				updateCache("refresh");
			}, 1000);

			//Refresh test
			$("#print").click(function() {
				cache.print = true;
				updateCache();
				setTimeout(function() {
					window.print();
					cache.print = false;
					updateCache();
				}, 1000);
			});
		});
	});
});




//Cache update
var cache = {print: false, arduino: {leds: [], jacks: [], potentiometers: []}, modules: []};
function updateCache(step) {
	if(step == undefined) {
		//Complete refresh with redrawing
		cache.refresh = "complete";
		sendWebsockets("/dump/modules");
	}
	else if(step == "refresh") {
		//Quick refresh (LEDs only)
		cache.refresh = "quick";
		sendWebsockets("/dump/leds");
	}
	else if(step == "end") {
		//End of refresh
		delete cache.jal;		
		if(cache.refresh == "quick")
			return;
		
		//Calculate electronic/hardware needs
		if(cache.modules.length) {
			var jal = cache.modules[cache.modules.length-1].jal.out;
			cache.partList = {
				jacks: 			floor(jal.jacks / cache.arduino.jacks.length),
				potentiometers: floor(jal.potentiometers / cache.arduino.potentiometers.length),
				leds:  			floor(jal.leds / cache.arduino.leds.length),
			};
			cache.partList.boards         = max(max(cache.partList.jacks, cache.partList.potentiometers), cache.partList.leds) + 1;
			cache.partList.jacks          = jal.jacks;
			cache.partList.potentiometers = jal.potentiometers;
			cache.partList.leds           = jal.leds;
			cache.partList.wires          = (1*cache.partList.jacks + 3*cache.partList.potentiometers + 2*cache.partList.leds) + max(0,cache.partList.boards-1)*4;
			$("#nbBoards")        .text("× " + cache.partList.boards);
			$("#nbJacks")         .text("× " + cache.partList.jacks);
			$("#nbPotentiometers").text("× " + cache.partList.potentiometers);
			$("#nbLEDs")          .text("× " + cache.partList.leds);
			$("#nbWires")         .text("× " + cache.partList.wires);
		}
	
		//Canvas for drawing
		if(app == undefined) {
			app = new PIXI.Application(1, 1, {transparent: true, resolution: window.devicePixelRatio, autoDensity: true});
			$("#canvas").append(app.view);
			app.container = new PIXI.Container();
			app.stage.addChild(app.container);
		}
		
		//Clear/clean container and children
		while(app.stage.children.length > 0) {
			var child = app.stage.getChildAt(0);
			app.stage.removeChild(child);
		}
		app.container = new PIXI.Container();
		app.stage.addChild(app.container);
		if(cache.print)
			app.baseColors = {
				stroke: 	0x000000,
				background: 0xFFFFFF
			};
		else
			app.baseColors = {
				stroke: 	0xFFFFFF,
				background: 0x154F8B,
				image: 		true 
			};
		
		//Image de fond
		if(app.baseColors.image)
			$("#canvas").addClass("image");
		else
			$("#canvas").removeClass("image");
		
		
		//Module
		var rack = {pos: {x: 9999999, y: 9999999}, size: {width: 0, height: 0}};
		$.each(cache.modules, function(index, module) {
			//Graphical container for module
			module.container = new PIXI.Container();
			module.container.position.x = module.pos.x;
			module.container.position.y = module.pos.y + floor(module.pos.y / 380) * 60;
			app.container.addChild(module.container);
			
			//White rectangle for module
			module.container.drawing = new PIXI.Graphics();
			module.container.drawing.alpha = 0.5;
			module.container.drawing.lineStyle(1, app.baseColors.stroke);
			module.container.drawing.drawRect(0, 0, module.size.width, module.size.height);
			module.container.addChild(module.container.drawing);

			//Text on buttons
			var introText = module.name;
			if(module.slug != module.name)
				introText = module.name + " (" + module.slug + ")";
			module.container.drawingText = new PIXI.Text(introText, {fontFamily : "revilo san", fontSize: 11, fill : app.baseColors.stroke});
			module.container.drawingText2 = new PIXI.Text(
				module.jal.in  .jacks + "-" + module.jal.in  .potentiometers + "-" + module.jal.in  .leds + "\n" +
				module.jal.size.jacks + "-" + module.jal.size.potentiometers + "-" + module.jal.size.leds + "\n" + 
				module.jal.out .jacks + "-" + module.jal.out .potentiometers + "-" + module.jal.out .leds + ""
				, {fontFamily : "Courier New", fontSize: 9, fill : app.baseColors.stroke});
			module.container.drawingText .position.y = 3 + module.container.height;
			module.container.drawingText2.position.y = 3 + module.container.drawingText.position.y + module.container.drawingText.height;
			module.container.drawingText2.alpha = 0.5;
			module.container.addChild(module.container.drawingText);
			module.container.addChild(module.container.drawingText2);
			
			//Input jacks
			$.each(module.inputs, function(index, item) {
				drawItem(module, item);
			});
			
			//Output jacks
			$.each(module.outputs, function(index, item) {
				drawItem(module, item);
			});
			
			//LEDs
			$.each(module.lights, function(index, item) {
				drawItem(module, item);
			});
			
			//Potentiometers
			$.each(module.parameters, function(index, item) {
				drawItem(module, item);
			});
			
			//Special case : audio jacks
			if(module.audio) {
				$.each(module.audio.inputs, function(index, item) {
					drawItem(module, item);
				});
				$.each(module.audio.outputs, function(index, item) {
					drawItem(module, item);
				});
			}
			
			//Dynamic canvas size
			rack.pos.x = min(rack.pos.x, module.container.position.x);
			rack.pos.y = min(rack.pos.y, module.container.position.y);
			rack.size.width  = max(rack.size.width,  module.container.position.x + module.container.width);
			rack.size.height = max(rack.size.height, module.container.position.y + module.container.height);
		});
		
		//Canvas resize to match modules
		var margins = 30;
		app.container.position.x = -rack.pos.x + margins;
		app.container.position.y = -rack.pos.y + margins;
		app.renderer.resize(rack.size.width + app.container.position.x + margins, rack.size.height + app.container.position.y + margins);
		app.ticker.update();
		$("#canvasWaiting").hide();
		$("#canvas").css({display: 'inline-block'});
	}
}


//Draw a potentiometer, a LED or a jack
function drawItem(module, item) {
	//Graphical container
	item.container = new PIXI.Container();
	module.container.addChild(item.container);
	
	//Visual icon + color + offsets
	var color = app.baseColors.stroke, textColor = app.baseColors.background, textOffset = {x: 0, y: 0}, textSize = 10;
	if     (item.type == "potentiometer") {
		textColor = color;
		item.container.drawing = PIXI.Sprite.from(cache.resources.icn_knob.texture);
		item.container.drawing.width = item.container.drawing.height = 40;
		textOffset.y = -item.container.drawing.height * 0.62;
	}
	else if((item.type == "jack_input") || (item.type == "jack_output") || (item.type == "jack_audio_input") || (item.type == "jack_audio_output")) {
		if(item.type == "jack_input")
			item.container.drawing = PIXI.Sprite.from(cache.resources.icn_jack_input.texture);
		else if(item.type == "jack_audio_input")
 			item.container.drawing = PIXI.Sprite.from(cache.resources.icn_jack_audio_input.texture);
 		else if(item.type == "jack_audio_output") {
 			item.container.drawing = PIXI.Sprite.from(cache.resources.icn_jack_audio_output.texture);
 			textColor = color;
		}
		else {
			item.container.drawing = PIXI.Sprite.from(cache.resources.icn_jack_output.texture);
			textColor = color;
		}
		item.container.drawing.width = item.container.drawing.height = 30;
		textOffset.y += -item.container.drawing.height * 0.69;
	}
	else if(item.type == "led") {
		item.container.drawing = PIXI.Sprite.from(cache.resources.icn_led.texture);
		textSize = 7;
		item.container.drawing.width = item.container.drawing.height = 10;
		textOffset = {x: 0, y: -item.container.drawing.height * 0.9};
	}
	
	//Draw stuff
	item.container.drawing.tint = color;
	item.container.drawing.position.x = item.pos.x - item.container.drawing.width /2;
	item.container.drawing.position.y = item.pos.y - item.container.drawing.height/2;
	item.container.addChild(item.container.drawing);
	
	
	//Add absolute ID
	item.container.drawingText = new PIXI.Text(item.id, {fontFamily : "OpenSans", fontWeight: 500, fill : textColor, fontSize: textSize});
	item.container.drawingText.position.x = item.pos.x - item.container.drawingText.width/2 + textOffset.x;
	item.container.drawingText.position.y = item.pos.y + item.container.drawing.height/2 + textOffset.y;
	item.container.addChild(item.container.drawingText);

	//Add wiring ID
	if(item.wiring.board == "audiocard")
		item.container.drawingText2 = new PIXI.Text("Audio" + item.wiring.pin, {fontFamily : module.container.drawingText.style.fontFamily, fontWeight: 200, fontSize: max(7, textSize-3), fill : app.baseColors.stroke});
	else
		item.container.drawingText2 = new PIXI.Text(item.wiring.pin + " (" + (item.wiring.board+1) + ")", {fontFamily : module.container.drawingText.style.fontFamily, fontWeight: 200, fontSize: max(7, textSize-3), fill : app.baseColors.stroke});
	item.container.drawingText2.position.x = item.pos.x - item.container.drawingText2.width/2;
	item.container.drawingText2.position.y = item.pos.y + item.container.drawing.height/2 + 1;
	item.container.drawingText2.alpha = 0.75;
	item.container.addChild(item.container.drawingText2);
}

//Websockets reception
function websocketReception(message) {
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
			if(message[1] == "start") {
				cache.modules = [];
				cache.jal = {jacks: 0, potentiometers: 0, leds: 0};
			}
			else if(message[1] == "end")
				sendWebsockets("/dump/jacks");
			else {
				//OSC deserialization
				var module = {
					type: "module",
					id:   parseInt(message[1], 10),
					slug: message[2],
					name: message[3],
					pos:  {x:     parseFloat(message[4]), y:      parseFloat(message[5])},
					size: {width: parseFloat(message[6]), height: parseFloat(message[7])},
					nbInputs: 	  parseInt(message[8], 10),
					nbOutputs: 	  parseInt(message[9], 10),
					nbParameters: parseInt(message[10], 10),
					nbLights: 	  parseInt(message[11], 10),
					inputs: 	  [], 
					outputs: 	  [], 
					parameters:	  [], 
					lights:	      [], 
					jal: 		  {}
				};
				module.jal.size = {jacks: (module.nbInputs+module.nbOutputs), potentiometers: module.nbParameters, leds: module.nbLights};
				module.jal.in   = JSON.parse(JSON.stringify(cache.jal));
			
				//Calculate jacks, potentiometers and LEDs quantities
				cache.jal.jacks 		 += module.jal.size.jacks;
				cache.jal.potentiometers += module.jal.size.potentiometers;
				cache.jal.leds  		 += module.jal.size.leds;

				//Post-processing
				module.jal.out   = JSON.parse(JSON.stringify(cache.jal));

				//Add in container
				cache.modules.push(module);
			}
		}
		else if(message[0] == "/dump/jacks") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				sendWebsockets("/dump/potentiometers");
			else {
				//OSC deserialization
				var jack = {
					type: 		     "jack",
					id:   	  		 parseInt(message[1], 10),
					moduleId: 		 parseInt(message[2], 10),
					inputOrOutputId: parseInt(message[3], 10),
					pos:  	  {x: parseFloat(message[4]), y: parseFloat(message[5])},
					active:   (parseInt(message[6], 10) > 0.5),
					value: 	  parseFloat(message[7]),
					isInput:  (parseInt(message[8], 10) > 0.5)
				};

				//Module reference
				var module = cache.modules[jack.moduleId];

				//Jack type (input or output)
				if(jack.isInput)	{ module.inputs [jack.inputOrOutputId] = jack; jack.type = "jack_input";  }
				else				{ module.outputs[jack.inputOrOutputId] = jack; jack.type = "jack_output"; }
				
				//Wiring
				jack.wiring = {board: floor(jack.id/cache.arduino.jacks.length), pin: cache.arduino.jacks[jack.id % cache.arduino.jacks.length]};
				
				//Modidy Physical Sync module : audio send/receive case
				if(module.slug == "ModiySync") {
					if(module.audio == undefined)	module.audio = {inputs: [], outputs: []};
					var jack = JSON.parse(JSON.stringify(jack));
					jack.isInput = !jack.isInput;
					if(jack.isInput) {
						jack.pos.x -= 30;
						module.audio.inputs[jack.inputOrOutputId] = jack;
						jack.type = "jack_audio_input";
					}
					else {
						jack.pos.x += 30;
						module.audio.outputs[jack.inputOrOutputId] = jack;
						jack.type = "jack_audio_output";
					}
					delete jack.wiring;
					jack.id = jack.inputOrOutputId+1;
					jack.wiring = {board: "audiocard", pin: jack.id};
				}
			}
		}
		else if(message[0] == "/dump/potentiometers") {
			if(message[1] == "start") {}
			else if(message[1] == "end")
				sendWebsockets("/dump/leds");
			else {
				//OSC deserialization
				var potentiometer = {
					type: 	  "potentiometer",
					id:   	  parseInt(message[1], 10),
					moduleId: parseInt(message[2], 10),
					paramId:  parseInt(message[3], 10),
					pos:  	  {x: parseFloat(message[4]), y: parseFloat(message[5])},
					value: 	  {absolute: parseFloat(message[6]), normalized: parseFloat(message[7])}
				};

				//Module reference
				var module = cache.modules[potentiometer.moduleId];

				//Wiring
				potentiometer.wiring = {board: floor(potentiometer.id / cache.arduino.potentiometers.length), pin: cache.arduino.potentiometers[potentiometer.id % cache.arduino.potentiometers.length]};

				//Add in container
				module.parameters[potentiometer.paramId] = potentiometer;
			}
		}
		else if(message[0] == "/dump/leds") {
			if(message[1] == "start") {}
			else if(message[1] == "end") {
				updateCache("end");
			}
			else {
				//OSC deserialization
				var led = {
					type: 	  "led",
					id:   	  parseInt(message[1], 10),
					moduleId: parseInt(message[2], 10),
					lightId:  parseInt(message[3], 10),
					pos:  	  {x: parseFloat(message[4]), y: parseFloat(message[5])},
					value: 	  {absolute: parseFloat(message[6]), normalized: parseFloat(message[7])}
				};

				//Module reference
				var module = cache.modules[led.moduleId];

				//Wiring
				led.wiring = {board: floor(led.id / cache.arduino.leds.length), pin: cache.arduino.leds[led.id % cache.arduino.leds.length]};

				//Add in container
				module.lights[led.lightId] = led;
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