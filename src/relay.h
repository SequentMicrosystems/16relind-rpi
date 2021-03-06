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

#define RELAY16_HW_I2C_BASE_ADD	0x20
#define RELAY16_HW_I2C_ALTERNATE_BASE_ADD 0x38
typedef uint8_t u8;
typedef uint16_t u16;

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

#endif //RELAY_H_
