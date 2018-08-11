//Cache update
var cache = {print: false, showPins: true, force: false, arduino: {leds: [], jacks: [], potentiometers: [], switches: []}, modules: []}, cacheTmp = {};
var prices = {
	jacks: [
		{
			name:   "Jacks",
			source: "http://fr.farnell.com/cliff-electronic-components/fc681374v/connecteur-audio-jack-3-5mm-3pos/dp/2431939",
			size: 	{width: 8, height: 10.5}, //mm
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
			size: 	{width: 8, height: 10.5}, //mm
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
			size: 		{width: 17, height: 20}, //mm
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
			source: "http://fr.farnell.com/kingbright/l-1503gc/led-5mm-vert-100mcd-568nm/dp/2335730?MER=bn_level5_5NP_EngagementRecSingleItem_4",
			size: 	{width: 5, height: 5}, //mm
			prices: {
				"5":    0.0739,
				"25":   0.0709,
				"100":  0.0679,
				"250":  0.0649,
				"500":  0.062,
				"1000": 0.0589
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
			name:   "Wires Female-Female",
			source: "https://www.amazon.fr/Ganvol-40-câbles-20-dexpérimentation-dordinateur/dp/B01LWAXJJS/ref=sr_1_15?ie=UTF8&qid=1533917655&sr=8-15&keywords=dupont+connecteur",
			prices: {
				"1": 3.95 / 40,
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
	]
}