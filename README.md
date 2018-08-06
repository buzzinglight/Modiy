# <img src="https://github.com/buzzinglight/Modiy/raw/master/res/admin/img/logo_red.png" alt="logo" width="250px"/>
Modiy is an open-source hardware interface for modular synthesis

## Download binaries
- Download release https://github.com/buzzinglight/Modiy/releases
- Unzip and copy `Modiy` folder into the `Rack/plugins/` folder in your Documents location

## Build from source code
- Clone repository into the `plugins/` folder of VCV Rack source code (https://github.com/VCVRack) :

```
cd plugins
git clone https://github.com/buzzinglight/Modiy.git
```

- Build plugin (see https://github.com/VCVRack/Rack#building-plugins for help)

```
cd Modiy
make
```

## For all versions
### Realtime Message Broker
- Copy *Realtime Message Broker* binary (https://github.com/buzzinglight/RTBroker/releases) into `Modiy/res/` folder
- Launch *Realtime Message Broker*
### Arduino source code
- From the webpage opened by *Realtime Message Broker*, download *Arduino source code* at the bottom of the page and upload it into an Arduino Mega board.
- After uploading, quit Arduino and open *Settings* page from the traybar icon of *Realtime Message Broker* and select the corresponding Arduino serial port in the list

### Wiring
- Launch *VCV Rack* with an instance of *Modiy* plugin on your rack (*Realtime Message Broker* will be automaticaly launched if you closed it).
- Pin number for each potentiometers, switches or patching jacks can be found on the admin page of Modiy : right-click —> *Open webpage* then select *Show pin number* (selected by default). Wire components in this way (and add as many Arduino MEGA as needed by Modiy) :

![alt text](https://github.com/buzzinglight/Modiy/raw/master/res/arduino/Wiring%20help%20-%20wiring.png "Wiring schematic")


![alt text](https://github.com/buzzinglight/Modiy/raw/master/res/arduino/Wiring%20help%20-%20schematic.png "Electronic schematic")

## OSC commands
If you want to control VCV Rack from OSC (instead of Arduino), here are the main messages to be sent on `57130` port. IDs of potentiometers, switches or patching jacks can be found on the admin page of Modiy : right-click —> *Open webpage* then select *Show IDs*.

### Potentiometers
```
/potentiometer/set/absolute <potentiometerId> <abolsute value>
/potentiometer/set/norm <potentiometerId> <value between 0 and 1>
/potentiometer/add/absolute <potentiometerId> <absolute value to be added>
/potentiometer/add/norm <potentiometerId> <value between -1 and 1 to be added>
/potentiometer/reset <potentiometerId>
```

### Switches
```
/switch <switchId> <0|1 : switch off or on>
```

### Wires
```
/link <jackId source> <jackId destination> <0|1 : remove or add a wire>
/link/clear
```

