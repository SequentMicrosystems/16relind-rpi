# 16relind-rpi
This is the command line to control the [Sixteen RELAYS 8-Layer Stackable HAT for Raspberry Pi](https://sequentmicrosystems.com/collections/all-io-cards/products/sixteen-relays-8-layer-stackable-hat-for-raspberry-pi).

Don't forget to enable I2C communication:
```bash
sudo raspi-config
```
## Usage
```bash
cd
git clone https://github.com/SequentMicrosystems/16relind-rpi.git
cd 16relind-rpi/
sudo make install
```
Now you can access all the functions of the relays board through the command "16relind". Use -h option for help:
```bash
16relind -h
```
If you clone the repository, any updates can be made with the following commands:
```bash
cd
cd 16relind-rpi/  
git pull
sudo make install
```
