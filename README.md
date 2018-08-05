# Modiy
Modiy is an open-source hardware interface for modular synthesis

## From release binary
- Download release https://github.com/buzzinglight/Modiy/releases
- Unzip and copy `Modiy` folder into the `Rack/plugins/` folder in your Documents location

## From source code
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
### Protocol Distpacher
- Copy *Protocol Distpacher* binary (https://github.com/buzzinglight/Protocol-Dispatcher/releases) into `Modiy/res/` folder
- Launch *Protocol Dispatcher*
### Arduino source code
- From the webpage opened by *Protocol Dispatcher*, download *Arduino source code* at the bottom of the page and upload it into an Arduino Mega board.
- After uploading, quit Arduino and open *Settings* page from the traybar icon of *Protocol Dispatcher* and select the corresponding Arduino serial port in the list
### VCV Rack integration
- Launch *VCV Rack* with an instance of *Modiy* plugin on your rack and refresh the webpage (you can launch it again in the menu of *Protocol Dispatcher* from the traybar icon)

## Wiring
TODO


## OSC commands
If you want to control VCV Rack from OSC (instead of Arduino), here are the main messages to be sent on `57130` port. IDs of potentiometers, switches or patching jacks can be found on the admin page of Modiy (right-click —> *Open webpage* then select "Show IDs").

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

