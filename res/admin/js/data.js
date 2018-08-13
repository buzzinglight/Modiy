//Cache update
var cache = {force: false, arduino: {leds: [], jacks: [], potentiometers: [], switches: []}, modules: []}, cacheTmp = {};
var prices = {
	jacks: [
		{
			name:   "Jacks",
			source: "http://fr.farnell.com/cliff-electronic-components/fc681374v/connecteur-audio-jack-3-5mm-3pos/dp/2431939",
			size: 	{width: 8, height: 10.5}, //mm / ø6mm hole
			prices: {
				"1":    0.998,
				"50":   0.58,
				"100":  0.504,
				"250":  0.471,
				"500":  0.438,
				"1500": 0.416
			}
		}
	],
	audioJacks: [
		{
			name:   "Jacks",
			source: "http://fr.farnell.com/multicomp/mj-074n/embase-jack-3-5mm-3p/dp/1267374",
			size: 	{width: 8, height: 10.5}, //mm / ø6mm hole
			prices: {
				"1":   1.15,
				"25":  0.954,
				"75":  0.79,
				"150": 0.674,
				"250": 0.587,
				"500": 0.521
			}
		}
	],
	potentiometers: [
		{
			name:      "Potentiometer",
			source:    "http://fr.farnell.com/bi-technologies-tt-electronics/p160knp-0qc20b10k/potentiometre-rotatif-10k-20mm/dp/1760793",
			sourceAlt: "http://fr.farnell.com/bi-technologies-tt-electronics/p160knp-0ec15b10k/potentiometre-rotatif-10k-15mm/dp/1684813",
			size: 		{width: 17, height: 20}, //mm / ø6mm hole
			prices: {
				"1":    0.954,
				"10":   0.673,
				"25":   0.545,
				"50":   0.502,
				"100":  0.427,
				"300":  0.353,
				"1500": 0.333
			}
		},
		{
			name:   "Plastic knobs",
			source: "http://fr.farnell.com/multicomp/mc21063/bouton-molete-rond-13mm-rouge/dp/2543082",
			prices: {
				"5":    0.561,
				"75":   0.454,
				"150":  0.323,
				"250":  0.289,
				"500":  0.258,
				"1500": 0.217
			}
		}
	],
	leds: [
		{
			name:   "LEDs",
			source:     "http://fr.farnell.com/kingbright/l-7083cgdk/led-5mm-green-100mcd-570nm/dp/2449735",
			sourceAltR: "http://fr.farnell.com/kingbright/l-7083surdk/led-5mm-red-200mcd-630nm/dp/2449738",
			sourceAltG: "http://fr.farnell.com/kingbright/l-7083cgdk/led-5mm-green-100mcd-570nm/dp/2449735",
			sourceAltO: "http://fr.farnell.com/kingbright/l-7083sedk/led-5mm-orange-400mcd-601nm/dp/2449737",
			size: 	{width: 5, height: 5}, //mm / ø5mm hole
			prices: {
				"5":    0.151,
				"25":   0.121,
				"100":  0.101,
				"250":  0.0848,
				"500":  0.075,
				"1000": 0.0668,
			}
		},
		{
			name:   "470Ω Resistor",
			source: "http://fr.farnell.com/multicomp/mcre000033/resistance-couche-carbon-125mw/dp/1700232?st=470%20ohm%20resistance",
			prices: {
				"5":     0.0198,
				"50":    0.0171,
				"250":   0.0143,
				"500":   0.0129,
				"1000":  0.0115,
				"2000":  0.0091,
				"10000": 0.0058
			}
		}		
	],
	switchesT: [
		{
			name:   "Toggle switches",
			source: "http://fr.farnell.com/multicomp/1ms1t1b5m1qe/interrupteur-spdt/dp/9473378",
			size: 	{width: 6.86, height: 12.7}, //m
			prices: {
				"5":   1.92,
				"25":  1.52,
				"75":  1.07,
				"150": 0.962,
				"250": 0.874,
				"500": 0.721
			}
		}		
	],
	switchesM: [
		{
			name:   "Momentary switches",
			source: "http://fr.farnell.com/multicomp/r13-24a-05-bb/switch-spst-3a-125v-solder/dp/1634687",
			size: 	{width: 7.2, height: 7.2}, //mm
			prices: {
				"5":    1.30,
				"25":   1.05,
				"100":  0.747,
				"150":  0.667,
				"250":  0.595,
				"1000": 0.501
			}
		}		
	],
	boards: [
		{
			name:   "Board",
			source: "https://www.amazon.fr/ELEGOO-ATMEGA-Contrôleur-Module-Arduino/dp/B06XNPKSDK/ref=sr_1_1_sspa?s=computers&rps=1&ie=UTF8&qid=1533744358&sr=1-1-spons&keywords=Arduino+MEGA&refinements=p_76%3A437878031&psc=1",
			prices: {
				"1": 12.99,
			}
		},
		{
			name:   "Enclosure",
			source: "https://www.amazon.fr/Boîtier-acrylique-transparent-brillant-Arduino/dp/B01CS5RQ7O/ref=sr_1_5?ie=UTF8&qid=1533746020&sr=8-5&keywords=Arduino+Mega+case",
			prices: {
				"1": 2.69,
			}
		}
		
	],
	wiresMM: [
		{
			name:   "Wires Male-Male",
			source: "https://www.amazon.fr/Daorier-Multicolore-Breadboard-Arduino-Male-Male/dp/B0727QSPR7/ref=sr_1_10?s=computers&ie=UTF8&qid=1533744616&sr=1-10&keywords=Arduino+cable+DuPont",
			prices: {
				"1": 1.61 / (40*3),
			}
		}
	],
	wiresFF: [
		{
			name:   "Wires Female header",
			source: "https://www.amazon.fr/Aihasd-Femelle-Cavalier-Arduino-Breadboard/dp/B072Q9WFRK/ref=sr_1_1?ie=UTF8&qid=1534169099&sr=8-1&keywords=dupont%2Bcable%2B30cm&th=1",
			prices: {
				"1": 2.85 / 40,
			}
		}
	],
	audioInterface: [
		{
			name:   "Audio interfaces",
			optional: true,
			source: "https://www.amazon.fr/Firepower-BEHRINGER-FCA1616-Interface-compatible/dp/B00E87OK1G/ref=pd_cp_267_1?_encoding=UTF8&pd_rd_i=B00E87OK1G&pd_rd_r=ade416d4-9b2d-11e8-9d26-3fe4c7b0301a&pd_rd_w=amqPT&pd_rd_wg=WcnoM&pf_rd_i=desktop-dp-sims&pf_rd_m=A1X6FK5RDHNB96&pf_rd_p=2171515611131751452&pf_rd_r=F9YH3QZE272RV2A33TGM&pf_rd_s=desktop-dp-sims&pf_rd_t=40701&psc=1&refRID=F9YH3QZE272RV2A33TGM",
			prices: {
				"1": 225,
			}
		}		
	],
	usb: [
		{
			name:   "USB Pass Through",
			optional: true,
			source: "http://fr.farnell.com/cliff-electronic-components/cp30207nm/adaptateur-usb-2-0-type-b-type/dp/2518195?ost=CP30207NM&ddkey=http%3Afr-FR%2FElement14_France%2Fsearch",
			prices: {
				"1":   7.82,
				"5":   5.69,
				"10":  5.31,
				"25":  4.94,
				"50":  4.69,
				"100": 4.60
			}
		}		
	],
	wood: [
		{
			name:   "Wood panel",
			source: "https://www.leroymerlin.fr/v3/p/produits/predecoupe-contreplaque-peuplier-ep-5-mm-l-80-x-l-40-cm-e1401453621?queryredirect=a_fp_predecoupe_contreplaque_peuplier__ep_5_mm_l_80_x_l_40_cm",
			prices: {
				"1":   5.75 / 3200,// 80cm x 40cm = 3200cm² = 5.75
			}
		}
	]
}

function calculatePrices() {
	if(cache.modules.length) {
		//Calculate JALS
		cache.jals        = {jacks: 0, potentiometers: 0, switches: 0, leds: 0, audioJacks: 0, switchesM: 0, switchesT: 0, wiresMM: 0, wiresFF: 0, wood: 0};
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
				wiresFF:         ((module.nbPotentiometers > 0) || (module.nbLights > 0))?(1):(0),
				boards: 		 0,
				wood: 			 (module.size.mm.width * module.size.mm.height) / 100
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
			wood: 			{value: ceil(cache.jals.wood)},
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
		
		//Price display by parts
		$.each(cache.partList, function(type, part) {
			var html;
			if(type == "wood")	html = part.value + "cm²";
			else				html = "×" + part.value;
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
}