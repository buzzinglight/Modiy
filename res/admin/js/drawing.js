//Update cache
function updateCache(step) {
	if(step == "force") {
		cache.force = true;
		step = undefined;
	}
	if(step == undefined) {
		//Complete refresh with redrawing
		cache.refresh = "complete";
		sendWebsockets("/dump/modules");
	}
	else if(step == "refresh") {
		//Quick refresh to look for changes
		cache.refresh = "quick";
		sendWebsockets("/dump/modules");
	}
	else if(step == "end") {
		var modulesSerializedTmp = JSON.stringify(cacheTmp.modules);
		if((cache.refresh == "complete") && ((cache.force) || (cache.modulesSerialized != modulesSerializedTmp))) {
			cache.refresh = "";
			cache.force = false;
			if(cache.modulesSerialized == undefined)	cache.modulesSerialized = "";
			console.log("Modules refreshed (" + cache.force + " / " + (cache.modulesSerialized != modulesSerializedTmp) + " / " + cache.modulesSerialized.length + "," + modulesSerializedTmp.length + ")");
			
			//End of refresh
			delete cacheTmp.jals;
			
			//Copy into modules
			cache.modulesSerialized = modulesSerializedTmp;
			cache.modules = JSON.parse(cache.modulesSerialized);
			
			//Canvas for drawing
			if(app == undefined) {
				app = new PIXI.Application(1, 1, {transparent: true, resolution: window.devicePixelRatio});
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
			
			//Palette
			if(cache.print)
				app.baseColors = {
					0: {
						stroke: 	0x000000,
						background: 0xFFFFFF,
						html: 		"#000000"
					},
					1: {
						stroke: 	0x3D9773,
						background: 0xFFFFFF,
						html: 		"#3D9773"
					},
					2: {
						stroke: 	0x7C507F,
						background: 0xFFFFFF,
						html: 		"#7C507F"
					},
					audiocard: {
						stroke: 	0x9A5E5F,
						background: 0xFFFFFF,
						html: 		"#9A5E5F"
					}
				};
			else
				app.baseColors = {
					image: true,
					0: {
						stroke: 	0xFFFFFF,
						background: 0x154F8B,
						html: 		"#FFFFFF"
					},
					1: {
						stroke: 	0x61EDB6,
						background: 0x154F8B,
						html: 		"#61EDB6"
					},
					2: {
						stroke: 	0xE392E9,
						background: 0x154F8B,
						html: 		"#E392E9"
					},
					audiocard: {
						stroke: 	0xED9092,
						background: 0x154F8B,
						html: 		"#ED9092"
					}
				};
			
			//Calculate electronic/hardware needs
			if(cache.modules.length) {
				//Calculate JALS
				cache.jals        = {jacks: 0, potentiometers: 0, switches: 0, leds: 0, audioJacks: 0, switchesM: 0, switchesT: 0, wiresMM: 0, wiresFF: 0};
				cache.consumption = {jacks: 0, potentiometers: 0, switches: 0, leds: 0, total: 0};
				$.each(cache.modules, function(index, module) {
					//In + Size
					module.jals.in = JSON.parse(JSON.stringify(cache.jals));
					module.jals.size = {
						jacks: 			(module.nbInputs+module.nbOutputs),
						potentiometers:  module.nbPotentiometers,
						switches: 		 module.nbSwitches,
						leds: 			 module.nbLights,
						audioJacks: 	 (module.audio)?(module.audio.inputs.length + module.audio.outputs.length):(0),
						switchesM: 	 	 0,
						switchesT: 	 	 0,
						wiresMM:         0,
						wiresFF:         ((module.nbPotentiometers > 0) || (module.nbLights > 0))?(2):(0),
						boards: 		 0, 
					};
					module.jals.consumption = {
						jacks: 			module.jals.size.jacks          / cache.arduino.jacks.length,
						potentiometers: module.jals.size.potentiometers / cache.arduino.potentiometers.length,
						switches: 		module.jals.size.switches       / cache.arduino.switches.length,
						leds: 			module.jals.size.leds           / cache.arduino.leds.length,
					};
					
					//Wires
					module.jals.size.wiresMM = (1*module.jals.size.jacks + 3*module.jals.size.potentiometers + 2*module.jals.size.leds) + module.jals.size.wiresFF; // +wiresFF for power

					//Add switches or buttons
					$.each(module.switches, function(index, button) {
						if(button.isToggle)	module.jals.size.switchesT++;
						else				module.jals.size.switchesM++;
					});

					//Calculate jacks, potentiometers and LEDs quantities
					$.each(cache.jals, function(type, value) {
						cache.jals[type] += module.jals.size[type];
					});

					//Post-processing
					module.jals.out = JSON.parse(JSON.stringify(cache.jals));
					
					//Add a part of boards
					$.each(module.jals.consumption, function(type, value) {
						if(type != "total")
							cache.consumption[type] += module.jals.consumption[type];
					});
				});
				
				//List of items to buy
				cache.partList = {
					jacks: 			{value: cache.jals.jacks},
					potentiometers: {value: cache.jals.potentiometers},
					leds: 			{value: cache.jals.leds},
					switchesT:      {value: cache.jals.switchesT},
					switchesM:      {value: cache.jals.switchesM},
					audioJacks:     {value: cache.jals.audioJacks},
					audioInterface: {value: ceil(cache.jals.audioJacks / 16)}, //<— based on a 16-channel audio interface
					usb: 			{value: 1},
					wiresMM:        {value: cache.jals.wiresMM},
					wiresFF:        {value: cache.jals.wiresFF},
					boards:         {value: 0},
				};
				cache.partList.boards.value  += max(max(floor(cache.jals.jacks / cache.arduino.jacks.length), floor(cache.jals.potentiometers / cache.arduino.potentiometers.length)), floor(cache.jals.leds / cache.arduino.leds.length)) + 1;
				cache.partList.wiresMM.value += max(0,cache.partList.boards.value-1) * 4;
			
				//Avg board ID for each module + consumption
				$.each(cache.modules, function(index, module) {
					//ID
					module.wiring.board = 0;
					var maxBoard = {index: 0, value: 0};
					$.each(module.wiring.boards, function(index, number) {
						if(number > maxBoard.value) {
							maxBoard.index = index;
							maxBoard.value = number;
						}
					});
					module.wiring.board = maxBoard.index;
					
					module.jals.size.boards = 0;
					$.each(module.jals.consumption, function(type, value) {
						if(type != "total") {
							if(cache.consumption[type] > 0)
								module.jals.consumption[type] /= cache.consumption[type];
							module.jals.size.boards += module.jals.consumption[type];
						}
					});
					cache.consumption.total += module.jals.size.boards;
				});
				
				//Affect a price to each component regarding its quantity
				$.each(cache.partList, function(type, part) {
					part.remaining = part.value;
					part.price = [];
					if((prices[type]) && (prices[type].length)) {
						for(var i = 0 ; i < prices[type].length ; i++) {
							var currentPriceMin = "";
							$.each(prices[type][i].prices, function(priceMin, price) {
								if((currentPriceMin == "") || (part.remaining >= parseFloat(priceMin)))
									currentPriceMin = priceMin;
							});
							if(currentPriceMin != "") {
								part.price.push({
									name:     prices[type][i].name,
									source:   prices[type][i].source,
									optional: prices[type][i].optional,
									unit:     prices[type][i].prices[currentPriceMin],
									total:    0
								});
							}
						}
					}
				});
				
				//Cost by module
				var totalPrice = {modules: 0, shared: 0, optional: 0};
				$.each(cache.modules, function(index, module) {
					module.price = {total: 0, details: {}};
					
					//For each part…
					$.each(cache.partList, function(type, part) {
						module.price.details[type] = 0;
						if(module.jals.size[type]) {
							for(var i = 0 ; i < part.price.length ; i++) {
								var price = part.price[i].unit * module.jals.size[type];
								module.price.details[type]  = price;
								part.price[i].total        += price;
								module.price.total         += price;
							}
							cache.partList[type].remaining -= module.jals.size[type];
						}
					});

					//Total price
					totalPrice.modules += module.price.total;
				});

				//Shared costs
				$.each(cache.partList, function(type, part) {
					if(part.remaining) {
						for(var i = 0 ; i < part.price.length ; i++) {
							var price = part.price[i].unit * part.remaining;
							part.price[i].total += price;
							if(part.price[i].optional)
								totalPrice.optional += price;
							else
								totalPrice.shared   += price;
						}
						cache.partList[type].remaining -= part.remaining;
					}
				});
				totalPrice.total = totalPrice.shared + totalPrice.modules;
				
				//Check if there is no remaining/forgotten parts
				console.log(cache.modules);
				console.log(totalPrice, JSON.parse(JSON.stringify(cache.partList)));
				
				//Price display by parts
				$.each(cache.partList, function(type, part) {
					var html = "×" + part.value;
					for(var i = 0 ; i < prices[type].length ; i++) {
						if(i > 0)
							html += "&nbsp;+";
						html += "&nbsp;<a class='price' title='" + part.price[i].name + "' href='" + part.price[i].source + "' target='_blank'>" + ceil(part.price[i].total) + "€</a>";
						$("#nb" + type + ">span").html(html);
					}
				});
				if(totalPrice.total > 0)
					$("h2 .price").html(ceil(totalPrice.total) + "€");
				else
					$("h2 .price").html("");

				//Boards legend
				var html = "";
				for(var i = 0 ; i < cache.partList.boards.value ; i++)
					html += "<div class='legend'><div class='color' style='background-color: " + app.baseColors[i%3].html + "'></div><div class='name'>To Board #" + (i+1) + "</div></div>";
				if(cache.partList.audioJacks)
					html += "<div class='legend'><div class='color' style='background-color: " + app.baseColors["audiocard"].html + "'></div><div class='name'>To Audiocard</div></div>";
				$("#legend").html(html);
			}
		
			//Image de fond
			if(app.baseColors.image)
				$("#canvas").addClass("image");
			else
				$("#canvas").removeClass("image");
		
			//Module
			var rack = {
				pos: {x: 9999999, y: 9999999},
				paper: {
					current: {x: 0, y: 0},
					size: {width: mm2px(210 - 0), height: mm2px(297 - 0)} //A4 sheet of paper
				},
				size: {width: 0, height: 0},
				scale: 1
			};
			//133mm = 380px (VCV)
			cache.items = [];		
			$.each(cache.modules, function(index, module) {
				//Check if module can fit the space (print only)
				if(cache.print) {
					if((rack.paper.current.x + module.size.px.width) > rack.paper.size.width) {
						rack.paper.current.x = 0;
						rack.paper.current.y += module.size.px.height;
						rack.paper.current.y = ceil(rack.paper.current.y / (rack.paper.size.height/2)) * (rack.paper.size.height/2);
					}
				}
				
				//Graphical container for module
				module.container = new PIXI.Container();
				if(cache.print) {
					module.container.position.x = rack.paper.current.x;
					module.container.position.y = rack.paper.current.y;
				}
				else {
					module.container.position.x = rack.scale * (module.pos.px.x);
					module.container.position.y = rack.scale * (module.pos.px.y + floor(module.pos.px.y / module.size.px.height) * 40);
				}
				module.container.scale.x = rack.scale;
				module.container.scale.y = rack.scale;
				app.container.addChild(module.container);
				
				//Shit for next module
				rack.paper.current.x += module.size.px.width * module.container.scale.x;
			
				//White rectangle for module
				module.container.drawing = new PIXI.Graphics();
				module.container.drawing.alpha = module.container.drawing.alphaBefore = 0.5;
				module.container.drawing.lineStyle(1, app.baseColors[module.wiring.board%3].stroke);
				module.container.drawing.drawRect(0, 0, module.size.px.width, module.size.px.height);
				module.container.addChild(module.container.drawing);

				//Text on buttons
				module.container.drawingText  = new PIXI.Text(module.nameInRack, {fontFamily : "revilo san", fontSize: 10, fill : app.baseColors[module.wiring.board%3].stroke});
				module.container.drawingText2 = new PIXI.Text(round(module.size.eurorack.width,1) + "HP / " + round(module.size.eurorack.height,1) + "U" + ((module.price.total)?(" — " + round(module.price.total) + "€"):("")) + "\n" + round(module.size.mm.width,1) + " × " + round(module.size.mm.height,1) + " mm", {fontFamily : "revilo san", fontSize: 9, fill : app.baseColors[module.wiring.board%3].stroke});
				module.container.drawingText .position.y = 3 + module.container.height / module.container.scale.y;
				module.container.drawingText2.position.y = module.container.drawingText.position.y + module.container.drawingText.height;
				module.container.drawingText2.alpha = module.container.drawingText2.alphaBefore = 0.75;
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
		
				//Potentiometers
				$.each(module.potentiometers, function(index, item) {
					drawItem(module, item);
				});
		
				//Switches
				$.each(module.switches, function(index, item) {
					drawItem(module, item);
				});
		
				//LEDs
				$.each(module.lights, function(index, item) {
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
			
			//Fit canvas to paper size
			if(cache.print) {
				console.log(rack.size, rack.paper.size);
				rack.size.width  = rack.paper.size.width;
				rack.size.height = ceil(rack.size.height / rack.paper.size.height) * rack.paper.size.height;
			}
		
			//Canvas resize to match modules
			var margins = (cache.print)?(0):(30);
			app.container.position.x = -rack.pos.x + margins;
			app.container.position.y = -rack.pos.y + margins;
			app.renderer.resize(rack.size.width + app.container.position.x + margins, rack.size.height + app.container.position.y + margins);
			$("#canvasWaiting").hide();
			$("#canvas").css({display: 'inline-block'});
			$(app.view).css({width: app.renderer.width/app.renderer.resolution, height: app.renderer.height/app.renderer.resolution});
		}
	}
}


//Draw a potentiometer, a LED or a jack
function drawItem(module, item) {
	if(item) {
		//Graphical container
		item.container = new PIXI.Container();
		item.container.position.x = item.pos.x;
		item.container.position.y = item.pos.y;
		module.container.addChild(item.container);
	
		var palette = app.baseColors[item.wiring.board%3];
		if(palette == undefined)
			palette = app.baseColors[item.wiring.board];
	
		//Items to draw
		var itemTypeSimplified = item.type.split("_")[0];
		item.container.drawing = PIXI.Sprite.from(cache.resources[item.type].texture);
		item.container.drawing.width = item.container.drawing.height = mm2px(prices[itemTypeSimplified][0].size.width) + mm2px(prices[itemTypeSimplified][0].size.height);
	
		//Visual icon + color + offsets
		var color = palette.stroke;
		var textColor = ((item.type == "potentiometers") || (item.type == "switchesT") || (item.type == "switchesM") || (item.type.indexOf("output") >= 0))?(palette.stroke):(palette.background);
		var textOffset = {x: 0, y: (item.isToggle)?(20):(0)}, textSize = 18;
	
		//Draw stuff
		item.container.drawing.tint = color;
		item.container.drawing.position.x = -item.container.drawing.width /2;
		item.container.drawing.position.y = -item.container.drawing.height/2;
		item.container.addChild(item.container.drawing);
		item.container.drawing.item = item;
		
		//Occupation
		item.container.space = new PIXI.Graphics();
		item.container.space.alpha = item.container.space.alphaBefore = 0.33;
		item.container.space.lineStyle(1, color);
		item.container.space.drawRect(-mm2px(prices[itemTypeSimplified][0].size.width), -mm2px(prices[itemTypeSimplified][0].size.height), mm2px(prices[itemTypeSimplified][0].size.width)*2, mm2px(prices[itemTypeSimplified][0].size.height)*2);
		item.container.addChild(item.container.space);
	
		//IDs
		var mainID, secondId
		if(cache.showPins) {
			mainID   = item.wiring.pin;
			secondId = (item.wiring.board == "audiocard")?("Audio " + item.id):("#" + item.id + " (" + (item.wiring.board+1) + ")")
		}
		else {
			mainID   = item.id;
			secondId = (item.wiring.board == "audiocard")?("Audio " + item.wiring.pin):(item.wiring.pin + " (" + (item.wiring.board+1) + ")");
		}
	
		//Add main ID
		item.container.drawingText = new PIXI.Text(mainID, {fontFamily : "OpenSans", fontWeight: 500, fill : textColor, fontSize: textSize});
		item.container.drawingText.position.x = -item.container.drawingText.width/2 + textOffset.x;
		item.container.drawingText.position.y = textOffset.y - item.container.drawingText.height/2 + 1;
		item.container.addChild(item.container.drawingText);

		//Events
		item.container.scale.x = item.container.scale.y = 0.5;
		cache.items.push(item.container);
		item.container.drawing.interactive = true;
		item.container.drawing.buttonMode = true;
		item.container.drawing.on('pointerdown', function() {
			var clickedItem = this.item.container;
			$.each(cache.items, function(index, item) {
				if(item != clickedItem)
					item.alpha = 0.1;
				else
					item.scale.x = item.scale.y = 1;
			});
		});
		item.container.drawing.on('pointerup', function() {
			$.each(cache.items, function(index, item) {
				if(item.alphaBefore != undefined)
					item.alpha = item.alphaBefore;
				else
					item.alpha = 1;
				item.scale.x = item.scale.y = 0.5;
			});
		});
	}
}

function px2mm(px) { // 3U = 3 × 44.45mm = 133.35mm = 380px in VCV // But 133.35 is theorical, 128.5 is applyed
	return px / 380 * 128.5;
}
function mm2px(mm) {
	return mm * 380 / 128.5;
}
function mm2rack(size) {
	return {
		width: 	size.width  / 5.08,  //HP
		height: size.height / (128.5/3), //U
	};
}