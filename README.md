# 16relind-rpi
This is the command line to control the [Sixteen RELAYS 8-Layer Stackable HAT for Raspberry Pi](https://sequentmicrosystems.com/collections/all-io-cards/products/sixteen-relays-8-layer-stackable-hat-br-for-raspberry-pi?variant=42476428296444).

Don't forget to enable I2C communication:
```bash
~$ sudo raspi-config
```
## Usage
```bash
~$ git clone https://github.com/SequentMicrosystems/16relind-rpi.git
~$ cd 16relind-rpi/
~/16relind-rpi$ sudo make install
```
Now you can access all the functions of the relays board through the command "16relind". Use -h option for help:
```bash
~$ 16relind -h
```
If you clone the repository any update can be made with the following commands:
```bash
~$ cd 16relind-rpi/  
~/16relind-rpi$ git pull
~/16relind-rpi$ sudo make install
```
