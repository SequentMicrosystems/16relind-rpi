#ifndef RELAY_H_
#define RELAY_H_

#include <stdint.h>

#define RETRY_TIMES	10
#define RELAY16_INPORT_REG_ADD	0x00
#define RELAY16_OUTPORT_REG_ADD	0x02
#define RELAY16_POLINV_REG_ADD	0x04
#define RELAY16_CFG_REG_ADD		0x06

#define CHANNEL_NR_MIN		1
#define RELAY_CH_NR_MAX		16

#define ERROR	-1
#define OK		0
#define FAIL	-1

#define WDT_RESET_SIGNATURE 	0xCA
#define WDT_MAX_OFF_INTERVAL_S 4147200 //48 days

#define RELAY16_HW_I2C_BASE_ADD	0x20
#define RELAY16_HW_I2C_ALTERNATE_BASE_ADD 0x38
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;


enum
{
	I2C_INPORT_REG_ADD,
	I2C_OUTPORT_REG_ADD = 2,
	I2C_POLINV_REG_ADD = 4,
	I2C_CFG_REG_ADD = 6,
	I2C_SW_MOM_ADD = 8,
	I2C_SW_INT_ADD,
	I2C_SW_INT_EN_ADD,
	I2C_MEM_DIAG_3V3_MV_ADD,
	I2C_MEM_DIAG_TEMPERATURE_ADD = I2C_MEM_DIAG_3V3_MV_ADD +2,
	I2C_MEM_DIAG_5V_ADD,
	I2C_MEM_WDT_RESET_ADD = I2C_MEM_DIAG_5V_ADD + 2,
	I2C_MEM_WDT_INTERVAL_SET_ADD,
	I2C_MEM_WDT_INTERVAL_GET_ADD = I2C_MEM_WDT_INTERVAL_SET_ADD + 2,
	I2C_MEM_WDT_INIT_INTERVAL_SET_ADD = I2C_MEM_WDT_INTERVAL_GET_ADD + 2,
	I2C_MEM_WDT_INIT_INTERVAL_GET_ADD = I2C_MEM_WDT_INIT_INTERVAL_SET_ADD + 2,
	I2C_MEM_WDT_RESET_COUNT_ADD = I2C_MEM_WDT_INIT_INTERVAL_GET_ADD + 2,
	I2C_MEM_WDT_CLEAR_RESET_COUNT_ADD = I2C_MEM_WDT_RESET_COUNT_ADD + 2,
	I2C_MEM_WDT_POWER_OFF_INTERVAL_SET_ADD,
	I2C_MEM_WDT_POWER_OFF_INTERVAL_GET_ADD = I2C_MEM_WDT_POWER_OFF_INTERVAL_SET_ADD + 4,
	I2C_MODBUS_SETINGS_ADD  = I2C_MEM_WDT_POWER_OFF_INTERVAL_GET_ADD + 4,//5 bytes
	I2C_MEM_RELAY_FAILSAFE_EN_ADD = I2C_MODBUS_SETINGS_ADD + 5,//16 bits
	I2C_MEM_RELAY_FAILSAFE_VAL_ADD = I2C_MEM_RELAY_FAILSAFE_EN_ADD + 2,//16 bits
	
	I2C_MEM_CPU_RESET = 0xaa,
	I2C_MEM_REVISION_HW_MAJOR_ADD ,
	I2C_MEM_REVISION_HW_MINOR_ADD,
	I2C_MEM_REVISION_MAJOR_ADD,
	I2C_MEM_REVISION_MINOR_ADD,
	I2C_MEM_LED_MODE = 254,
	SLAVE_BUFF_SIZE = 255
};


typedef enum
{
	OFF = 0,
	ON,
	STATE_COUNT
} OutStateEnumType;

typedef struct
{
 const char* name;
 const int namePos;
 void(*pFunc)(int, char**);
 const char* help;
 const char* usage1;
 const char* usage2;
 const char* example;
}CliCmdType;

typedef struct
	__attribute__((packed))
	{
		unsigned int mbBaud :24;
		unsigned int mbType :4;
		unsigned int mbParity :2;
		unsigned int mbStopB :2;
		unsigned int add:8;
	} ModbusSetingsType;


#endif //RELAY_H_
