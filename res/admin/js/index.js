//Démarrage du système
$(document).ready(function() {
	//Cache of images
	var loader = PIXI.loaders.shared;
	loader.add('jacks_audio_input',  	'img/icn_jack_audio_input.png')
		  .add('jacks_audio_output', 	'img/icn_jack_audio_output.png')
		  .add('jacks_input', 			'img/icn_jack_input.png')
		  .add('jacks_output', 			'img/icn_jack_output.png')
		  .add('potentiometers', 		'img/icn_knob.png')
		  .add('switchesT', 			'img/icn_switchT.png')
		  .add('switchesM', 			'img/icn_switchM.png')
		  .add('leds', 					'img/icn_led.png');
	loader.load((loader, resources) => {
		cache.resources = resources;
		
		//Sync with Arduino sketch (for pinouts)
		$.ajax("/arduino/Modiy/Modiy.ino").done(function(arduino) {
			var arduinoCode = arduino.split("\n").clean("");
			$.each(arduinoCode, function(index, codeLine) {
				if((codeLine.indexOf("jackPinout[] = ") > 0) || (codeLine.indexOf("analogPinout[] = ") > 0) || (codeLine.indexOf("ledPinout[] = ") > 0) || (codeLine.indexOf("switchPinout[] = ") > 0)) {
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
						else if(codeLine.indexOf("switchPinout[] = ") > 0)
							cache.arduino.switches = codeLineValues;
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
			
			//Colors
			app.baseColors = app.baseColorsScreen;
		
			//LEDs refresh
			setInterval(function() {
				updateCache();
			}, 2000);
			
			//Show pins
			$("#showPins").click(function() {
				if($(this).hasClass("checked"))
					$(this).removeClass("checked").text("Show pin numbers");
				else
					$(this).addClass("checked").text("Show IDs")
				app.showPins = $(this).hasClass("checked");
				updateCache("force");
			});
			$("#showPins").trigger("click");

			//Refresh test
			function printCommon() {
				app.baseColors = app.baseColorsPrint;
				updateCache("force");
				//return;
				setTimeout(function() {
					window.print();
					app.print = 0;
					app.baseColors = app.baseColorsScreen;
					updateCache("force");
				}, 1000);
			}
			$("#print1").click(function() {
				app.print = 1;
				printCommon();
			});
			$("#print2").click(function() {
				app.print = 2;
				printCommon();
			});
		});
	});
});
