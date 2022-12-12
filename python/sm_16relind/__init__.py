import smbus2

__version__ = "1.0.5"
_CARD_BASE_ADDRESS = 0x20
_INPORT_REG_ADD = 0x00
_OUTPORT_REG_ADD = 0x02
_POLINV_REG_ADD = 0x04
_CFG_REG_ADD = 0x06
_STACK_LEVEL_MAX = 7
_RELAY_COUNT = 16

relayMaskRemap =[0x8000, 0x4000, 0x2000, 0x1000, 0x800,	0x400, 0x200, 0x100, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2,0x1]

relayChRemap = [15,	14,	13,	12,	11,	10,	9,	8,	7,	6,	5,	4,	3,	2,	1,	0]

class SM16relind:
    def __init__(self, stack = 0, i2c = 1):
        if stack < 0 or stack > _STACK_LEVEL_MAX:
            raise ValueError('Invalid stack level!')
        self._hw_address_ = _CARD_BASE_ADDRESS + (0x07 ^ stack)
        self._i2c_bus_no = i2c
        bus = smbus2.SMBus(self._i2c_bus_no)
        try:
            val = bus.read_word_data(self._hw_address_, _CFG_REG_ADD)
            if val != 0:
                val = 0
                bus.write_word_data(self._hw_address_, _CFG_REG_ADD, val)
        except Exception as e:
            bus.close()
            raise Exception("Fail to init the card with exception " + str(e))
        bus.close()

    def set(self, relay, state):
        if relay < 1 or relay > _RELAY_COUNT:
            raise ValueError('Invalid relay number!')
        bus = smbus2.SMBus(self._i2c_bus_no)
        try:
            oldVal = bus.read_word_data(self._hw_address_, _OUTPORT_REG_ADD)
            oldVal =IOToRelay(oldVal)
            if state == 0:
                oldVal = oldVal & (~(1 << (relay - 1)))
            else:
                oldVal = oldVal | (1 << (relay - 1))
            oldVal = relayToIO(oldVal)
            bus.write_word_data(self._hw_address_, _OUTPORT_REG_ADD, oldVal)
        except Exception as e:
            bus.close()
            raise Exception("Fail set relay with exception " + str(e))
        bus.close()

    def set_all(self, val):
        bus = smbus2.SMBus(self._i2c_bus_no)
        val = relayToIO(val)
        try:
            bus.write_word_data(self._hw_address_, _OUTPORT_REG_ADD, val)
        except Exception as e:
            bus.close()
            raise Exception("Fail set relay with exception " + str(e))
        bus.close()

    def get(self, relay):
        if relay < 1 or relay > _RELAY_COUNT:
            raise ValueError('Invalid relay number!')
        bus = smbus2.SMBus(self._i2c_bus_no)
        try:
            oldVal = bus.read_word_data(self._hw_address_, _OUTPORT_REG_ADD)
        except Exception as e:
            bus.close()
            raise Exception("Fail set relay with exception " + str(e))
        bus.close()
        oldVal = IOToRelay(oldVal)
        if (1 << (relay - 1)) & oldVal :
            return 1
        return 0

    def get_all(self):
        bus = smbus2.SMBus(self._i2c_bus_no)
        try:
            oldVal = bus.read_word_data(self._hw_address_, _OUTPORT_REG_ADD)
        except Exception as e:
            bus.close()
            raise Exception("Fail set relay with exception " + str(e))
        bus.close()
        oldVal = IOToRelay(oldVal)
        return oldVal


def relayToIO(relay):
    val = 0
    for i in range(0, _RELAY_COUNT):
        if (relay & (1 << i)) != 0:
            val = val + relayMaskRemap[i]
    return val


def IOToRelay(iov):
    val = 0
    for i in range(0, _RELAY_COUNT):
        if (iov & relayMaskRemap[i]) != 0:
            val = val + (1 << i)
    return val
