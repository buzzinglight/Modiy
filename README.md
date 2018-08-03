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

`make`

## For all versions
- Copy *Protocol Distpacher* binary (https://github.com/buzzinglight/Protocol-Dispatcher/releases) into `Modiy/res/` folder
- Launch *Protocol Dispatcher*
- Launch *VCV Rack* with an instance of *Modiy* plugin on your rack

Then :
- From the webpage opened by Protocol Dispatcher, download Arduino source code and upload it into an Arduino Mega board.
- Open Settings page into Protocol Dispatcher and select the available Arduino Serial Port.
