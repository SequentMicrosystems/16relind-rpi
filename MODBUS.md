[16reind-rpi](https://sequentmicrosystems.com)

# Modbus

The [Sixteen Relays HAT](https://sequentmicrosystems.com/products/sixteen-relays-8-layer-stackable-hat-for-raspberry-pi) and [Sixteen Solid-State Relay HAT](https://sequentmicrosystems.com/products/solid-state-relays-8-layer-stackable-hat-for-raspberry-pi) can be accessed through Modbus RTU protocol over the RS-485 port.
You can configure the RS-485 port using the **16relind** command.

Example:
```bash
~$ 16relind 0 cfg485wr 1 9600 1 0 1
```
Set Modbus RTU, Baudrate: 9600bps, 1 Stop Bit,  parity: None, slave address offset: 1
```bash
~$ 16relind -h cfg485wr
```
display the full set of options

## Slave Address
The slave address is added with the "stack level" dipswitches. For example, the dipswitches configuration for stack level 1 (one switch ON in position ID0) means a slave address offset of 1 corresponds to slave address 2.

## Modbus object types
All Modbus RTU object types with standard addresses are implemented : Coils, Discrete Inputs, Input registers, Holding registers. Only coils have valid addresses, so any other object access will return errors (conform to the protocol specifications).

### Coils

Access level Read/Write, Size 1 bit

| Register Name | Register Address | Modbus Address | Function |
| --- | --- | --- | --- |
| COIL_R1 | 0001 | 0x00 | Relay 1 Control |
| COIL_R2 | 0002 | 0x01 | Relay 2 Control |
| COIL_R3 | 0003 | 0x02 | Relay 3 Control |
| COIL_R4 | 0004 | 0x03 | Relay 4 Control |
| COIL_R5 | 0005 | 0x04 | Relay 5 Control |
| COIL_R6 | 0006 | 0x05 | Relay 6 Control |
| COIL_R7 | 0007 | 0x06 | Relay 7 Control |
| COIL_R8 | 0008 | 0x07 | Relay 8 Control |
| COIL_R9 | 0009 | 0x08 | Relay 9 Control |
| COIL_R10 | 0010 | 0x09 | Relay 10 Control |
| COIL_R11 | 0011 | 0x0a | Relay 11 Control |
| COIL_R12 | 0012 | 0x0b | Relay 12 Control |
| COIL_R13 | 0013 | 0x0c | Relay 13 Control |
| COIL_R14 | 0014 | 0x0d | Relay 14 Control |
| COIL_R15 | 0015 | 0x0e | Relay 15 Control |
| COIL_R16 | 0016 | 0x0f | Relay 16 Control |


### Discrete Inputs

Access level Read Only, Size 1 bit

| Device function | Register Address | Modbus Address |
| --- | --- | --- |



### Input registers

Access level Read Only, Size 16 bits

| Device function | Register Address | Description | Measurement Unit |
| --- | --- | --- | --- |

### Holding registers

Access level Read/Write, Size 16 bits

| Device function | Register Address | Modbus Address | Measurement Unit | Range |
| --- | --- | --- | --- | --- |

## Function codes implemented

* Read Coils (0x01)
* Read Discrete Inputs (0x02)
* Read Holding Registers (0x03)
* Read Input Registers (0x04)
* Write Single Coil (0x05)
* Write Single Register (0x06)
* Write Multiple Coils (0x0f)
* Write Multiple registers (0x10)

