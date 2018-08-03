# Modiy
Modiy is an open-source hardware interface for modular synthesis

## From source code
- Download/clone source code
- Compile plugin with VCV Rack (see https://vcvrack.com/manual/PluginDevelopmentTutorial.html for help)

## From release binary
- Download release https://github.com/buzzinglight/Modiy/releases

## For all versions
- Copy *Protocol Distpacher* binary (https://github.com/buzzinglight/Protocol-Dispatcher/releases) into *res* folder
- Launch Protocol Dispatcher
- Launch VCV Rack with an instance of *Modiy* plugin on your rack

Then :
- From the webpage opened by Protocol Dispatcher, download Arduino source code and upload it into an Arduino Mega board.
- Open Settings page into Protocol Dispatcher and select the available Arduino Serial Port.
