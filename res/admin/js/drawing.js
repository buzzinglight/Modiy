var app = {
	pxScale: 1.28,
	baseColors: {},
	baseColorsScreen: {
		image: true,
		0: {
			stroke: 	0xFFFFFF,
			background: 0x000000,
			html: 		"#FFFFFF"
		},
		1: {
			stroke: 	0x4AC99F,
			background: 0x000000,
			html: 		"#4AC99F"
		},
		2: {
			stroke: 	0xF2911B,
			background: 0x000000,
			html: 		"#F2911B"
		},
		audiocard: {
			stroke: 	0xF3374D,
			background: 0x000000,
			html: 		"#F3374D"
		}
	},
	baseColorsPrint: {
		0: {
			stroke: 	0x000000,
			background: 0xFFFFFF,
			html: 		"#000000"
		},
		1: {
			stroke: 	0x4AC99F,
			background: 0xFFFFFF,
			html: 		"#4AC99F"
		},
		2: {
			stroke: 	0xF2911B,
			background: 0xFFFFFF,
			html: 		"#F2911B"
		},
		audiocard: {
			stroke: 	0xF3374D,
			background: 0xFFFFFF,
			html: 		"#F3374D"
		}
	}
};

//Update cache
function updateCache(step) {
	app.margins = mm2px(5);
	
	$("#update").text("Syncing…");
	
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
			if(cache.modulesSerialized == undefined)
				cache.modulesSerialized = "";
			console.log("Modules refreshed (" + cache.force + " / " + (cache.modulesSerialized != modulesSerializedTmp) + " / " + cache.modulesSerialized.length + "," + modulesSerializedTmp.length + ")");
			
			//End of refresh
			delete cacheTmp.jals;
			
			//Copy into modules
			cache.modulesSerialized = modulesSerializedTmp;
			cache.modules = JSON.parse(cache.modulesSerialized);
		}

		//Do drawing and other operations
		updateCacheFinished();
	}
}

function updateCacheFinished() {
	$("#update").text("Sync with VCV Rack");
	$(".module").remove();
	$("#modules, #panels .ruler, #panels .bunch .rotation .woodPanel").html("");
	var woodHeight = mm2px(100);
	$("#panels .bunch .rotation .woodPanel").css({ width: woodHeight });
	$("#panels .bunch .rotation").css({ transform: "rotate(-90deg) translate(-" + woodHeight + "px,0px)" });
	$("#panels .bunch").css({ height: woodHeight });
	cache.refresh = "";
	cache.force = false;
	
	//Calculate electronic/hardware needs
	calculatePrices();

	//Drawing
	cache.items = [];		
	$.each(cache.modules, function(index, module) {
		//Canvas for drawing
		module.app = new PIXI.Application(1, 1, {
			transparent:  true,
			resolution:   window.devicePixelRatio,
			sharedTicker: true,
			sharedLoaded: true,
			forceCanvas:  true
		});
		$("#modules").append("<div class='module module" + index + "'><img class='panel' src='' /></div>");
		$("#panels .bunch .rotation .woodPanel").append("<div class='module module" + index + "' rotated='true'><img class='panel' src='' /></div>");
		module.app.view.domModule = $("#modules .module" + index);
		module.app.view.domPanel  = $("#panels  .module" + index);
		module.app.view.domPanels = $(".module" + index);
		module.app.view.domModule.append(module.app.view);

		//Graphical container for module
		module.container = new PIXI.Container();
		module.container.position.x = module.container.position.y = app.margins;
		module.app.stage.addChild(module.container);
		
		//Two sub-containers : infos and mounting
		module.container.infos   = new PIXI.Container();
		module.container.mouting = new PIXI.Container();
		module.container.addChild(module.container.infos);
		module.container.addChild(module.container.mouting);
	
		//White rectangle for module
		var traitCoupe = app.margins * 0.75;
		module.container.drawing = new PIXI.Graphics();
		module.container.drawing.alpha = module.container.drawing.alphaBefore = 0.5;
		module.container.drawing.lineStyle(1, app.baseColors[module.wiring.board%3].stroke);
		module.container.drawing.moveTo(-traitCoupe, 0); module.container.drawing.lineTo(module.size.px.width+traitCoupe, 0);
		module.container.drawing.moveTo(-traitCoupe, module.size.px.height); module.container.drawing.lineTo(module.size.px.width+traitCoupe, module.size.px.height);
		module.container.drawing.moveTo(0, -traitCoupe); module.container.drawing.lineTo(0, module.size.px.height+traitCoupe);
		module.container.drawing.moveTo(module.size.px.width, -traitCoupe); module.container.drawing.lineTo(module.size.px.width, module.size.px.height+traitCoupe);
		module.container.infos.addChild(module.container.drawing);

		//Text on buttons
		module.container.drawingText  = new PIXI.Text(module.nameInRack, {fontFamily : "revilo san", fontSize: 10 * app.pxScale, fill : app.baseColors[module.wiring.board%3].stroke});
		module.container.drawingText2 = new PIXI.Text(round(module.size.eurorack.width,1) + "HP / " + round(module.size.eurorack.height,1) + "U" + ((module.price.total)?(" — " + round(module.price.total) + "€"):("")) + "\n" + round(module.size.mm.width,1) + " × " + round(module.size.mm.height,1) + " mm", {fontFamily : "revilo san", fontSize: 9 * app.pxScale, fill : app.baseColors[module.wiring.board%3].stroke});
		module.container.drawingText.rotation = module.container.drawingText2.rotation = -PI/2;
		module.container.drawingText .position.x = module.size.px.width  + traitCoupe/4;
		module.container.drawingText .position.y = module.size.px.height - traitCoupe/2;
		module.container.drawingText2.position.x = module.container.drawingText.position.x + module.container.drawingText.height;
		module.container.drawingText2.position.y = module.container.drawingText.position.y;
		module.container.drawingText2.alpha = module.container.drawingText2.alphaBefore = 0.75;
		module.container.infos.addChild(module.container.drawingText);
		module.container.infos.addChild(module.container.drawingText2);
		
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
		module.app.renderer.resize(module.container.width + app.margins/2, module.container.height + app.margins/2);
		$("#modulesWaiting").hide();
		$("#modules").show();
		
		//Panel
		if(module.panel) {
			$(module.app.view.domPanels).find("img").each(function(index, dom) {
				dom = $(dom);
				dom.attr("src", "/?file=" + module.panel).css({
					width: 	   module.size.px.width,
					height:    module.size.px.height,
					top:       app.margins,
					left: 	   app.margins
				});
				dom.attr("src", "/?file=" + module.panel);
				if(index == 0) {
					if(app.print != 1)
						dom.show();
					else
						dom.hide();
				}
			});
		}
		if(app.print != 2)
			module.container.mouting.visible = true;
		else
			module.container.mouting.visible = false;
		
		module.app.view.domModule.css({width: module.app.renderer.width / module.app.renderer.resolution, height: module.app.renderer.height / module.app.renderer.resolution});;
		$(module.app.view).css({width: module.app.view.domModule.width, height: module.app.view.domModule.height()});
	});
	
	var totalWidth = $("#panels .bunch .rotation").height();
	$("#panels>*").css({ width: totalWidth });
	for(var i = 0 ; i < totalWidth ; i += mm2px(25)) {
		$("#panels .ruler").append("<div class='tip' style='left: " + i + "px;'>" + round(px2mm(i)/10) + "cm</div>");
	}
	
}

//Draw a potentiometer, a LED or a jack
function drawItem(module, item) {
	if(item) {
		//Graphical container
		item.container = new PIXI.Container();
		item.container.position.x = item.pos.x * app.pxScale;
		item.container.position.y = item.pos.y * app.pxScale;
		module.container.mouting.addChild(item.container);
		
		//ModiySync case
		if(module.slug == "ModiySync") {
			if(item.inputOrOutputId != undefined)
				item.container.position.y = map(item.inputOrOutputId, 0,8, 65, 395) * app.pxScale;
			if(item.type == "jacks_audio_input")
				item.container.position.x -= 5;
			else if(item.type == "jacks_audio_output")
				item.container.position.x += 5;
		}
	
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
		item.container.drawingText = new PIXI.Text(mainID, {fontFamily : "OpenSans", fontWeight: 500, fill : textColor, fontSize: textSize * app.pxScale});
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


//Scale functions
function px2mm(px) { // 3U = 3 × 44.45mm = 133.35mm = 380px in VCV // But 133.35 is theorical, 128.5 is applyed
	return (px / 380 * 128.5) / app.pxScale;
}
function mm2px(mm) {
	return (mm * 380 / 128.5) * app.pxScale;
}
function mm2rack(size) {
	return {
		width: 	size.width  / 5.08,  //HP
		height: size.height / (128.5/3), //U
	};
}
