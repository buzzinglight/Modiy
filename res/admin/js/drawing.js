var app = {
	baseColors: {},
	baseColorsScreen: {
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
	},
	baseColorsPrint: {
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
	}
};

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
			$("#canvas").html("");
			console.log("Modules refreshed (" + cache.force + " / " + (cache.modulesSerialized != modulesSerializedTmp) + " / " + cache.modulesSerialized.length + "," + modulesSerializedTmp.length + ")");
			
			//End of refresh
			delete cacheTmp.jals;
			
			//Copy into modules
			cache.modulesSerialized = modulesSerializedTmp;
			cache.modules = JSON.parse(cache.modulesSerialized);

			//Calculate electronic/hardware needs
			calculatePrices();
		
			//Drawing
			cache.items = [];		
			$.each(cache.modules, function(index, module) {
				//Image de fond
				if(app.baseColors.image)
					$("#canvas").addClass("image");
				else
					$("#canvas").removeClass("image");
			
				//Canvas for drawing
				module.app = new PIXI.Application(1, 1, {
					transparent:  true,
					resolution:   window.devicePixelRatio,
					sharedTicker: true,
					sharedLoaded: true,
					forceCanvas:  true
				});
				$("#canvas").append("<div class='module' id='module" + index + "'><img class='panel' src='' /></div>");
				module.app.view.dom = $("#canvas>#module" + index);
				module.app.view.dom.append(module.app.view);

				//Graphical container for module
				module.container = new PIXI.Container();
				module.app.stage.addChild(module.container);
			
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
				
				//Canvas resize to match modules
				module.app.renderer.resize(module.container.width, module.container.height);
				$("#canvasWaiting").hide();
				$("#canvas").css({display: 'inline-block'});
				
				//Panel
				if(module.panel) {
					var img = $(module.app.view.dom).find("img");
					img.attr("src", "/?file=" + module.panel).css({
						width: 	module.container.drawing.width,
						height: module.container.drawing.height,
					});
					img.attr("src", "/?file=" + module.panel);
					if(app.print != 1)
						img.show();
					else
						img.hide();
				}
				if(app.print != 2)
					$(module.app.view).show();
				else
					$(module.app.view).hide();
				
				module.app.view.dom.css({width: module.app.renderer.width / module.app.renderer.resolution, height: module.app.renderer.height / module.app.renderer.resolution});;
				$(module.app.view).css({width: module.app.view.dom.width, height: module.app.view.dom.height()});
			});
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
		if(app.showPins) {
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