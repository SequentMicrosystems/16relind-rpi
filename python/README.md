# sm_16relind

This is the python library to control the [Sixteen Relays 8-Layer Stackable HAT for Raspberry Pi](https://sequentmicrosystems.com/collections/all-io-cards/products/sixteen-relays-8-layer-stackable-hat-for-raspberry-pi).

## Install

```bash
~$ sudo apt-get update
~$ sudo apt-get install build-essential python-pip python-dev python-smbus git
~$ git clone https://github.com/SequentMicrosystems/16relind-rpi.git
~$ cd 16relind-rpi/python/
~/16relind-rpi/python$ sudo python setup.py install
```
If you use python3.x repace the last line with:
```
~/16relind-rpi/python$ sudo python3 setup.py install
```
## Update

```bash
~$ cd 16relind-rpi/
~/16relind-rpi$ git pull
~$ cd 16relind-rpi/python
~/16relind-rpi/python$ sudo python setup.py install
```
If you use python3.x repace the last line with:
```
~/16relind-rpi/python$ sudo python3 setup.py install
```
## Usage example

```bash
~$ python
Python 3.10.7 (main, Nov  7 2022, 22:59:03) [GCC 8.3.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
>>> import sm_16relind
>>> rel = sm_16relind.SM16relind(0)
>>> rel.set(1, 1)
>>> rel.get_all()
1
>>>
```

More usage example in the [examples](examples/) folder

## Functions prototype

### *class sm_16relind.SM16relind(stack = 0, i2c = 1)*
* Description
  * Init the SM16relind object and check the card presence 
* Parameters
  * stack : Card stack level [0..7] set by the jumpers
  * i2c : I2C port number, 1 - Raspberry default , 7 - rock pi 4, etc.
* Returns 
  * card object

#### *set(relay, val)*
* Description
  * Set one relay state
* Parameters
  * *relay*: The relay number 1 to 16
  * *val*: The new state of the relay 0 = turn off else turn on
* Returns
  * none
  
#### *set_all(val)*
* Description
  * Set the state of all relays as a 16 bits bit-map
* Parameters
  * *val*: The new state of all 16 relays, 0 => all off, 15 => all on
* Returns
  * none
  
#### *get(relay)*
* Description
  * Read one relay state
* Parameters
  * *relay* relay number [1..16]
* Returns
  * the state of the relay 0 or 1

#### *get_all()*
* Description
  * Read the state of all 16 relays 
* Parameters
  * none
* Returns
  * relays state as bitmap [0..65535]

