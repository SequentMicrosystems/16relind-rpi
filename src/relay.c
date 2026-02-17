/*
 * relay.c:
 *	Command-line interface to the Raspberry
 *	Pi's 816-Relay Industrial board.
 *	Copyright (c) 2016-2026 Sequent Microsystem
 *	<http://www.sequentmicrosystem.com>
 ***********************************************************************
 *	Author: Alexandru Burcea
 ***********************************************************************
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "relay.h"
#include "comm.h"
#include "thread.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>

#define VERSION_BASE	(int)1
#define VERSION_MAJOR	(int)1
#define VERSION_MINOR	(int)5

#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */
#define CMD_ARRAY_SIZE	8

#define THREAD_SAFE
//#define DEBUG_SEM

#define TIMEOUT_S 3

const u16 relayMaskRemap[16] = {0x8000, 0x4000, 0x2000, 0x1000, 0x800, 0x400,
	0x200, 0x100, 0x80, 0x40, 0x20, 0x10, 0x8, 0x4, 0x2, 0x1};
const int relayChRemap[16] = {15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
	0};

int relayChSet(int dev, u8 channel, OutStateEnumType state);
int relayChGet(int dev, u8 channel, OutStateEnumType *state);
u16 relayToIO(u16 relay);
u16 IOToRelay(u16 io);

static void doHelp(int argc, char *argv[]);
const CliCmdType CMD_HELP =
	{"-h", 1, &doHelp,
		"\t-h           Display the list of command options or one command option details\n",
		"\tUsage:       16relind -h    Display command options list\n",
		"\tUsage:       16relind -h <param>   Display help for <param> command option\n",
		"\tExample:     16relind -h write    Display help for \"write\" command option\n"};

static void doVersion(int argc, char *argv[]);
const CliCmdType CMD_VERSION = {"-v", 1, &doVersion,
	"\t-v           Display the version number\n",
	"\tUsage:       16relind -v\n", "",
	"\tExample:     16relind -v  Display the version number\n"};

static void doWarranty(int argc, char *argv[]);
const CliCmdType CMD_WAR = {"-warranty", 1, &doWarranty,
	"\t-warranty    Display the warranty\n",
	"\tUsage:       16relind -warranty\n", "",
	"\tExample:     16relind -warranty  Display the warranty text\n"};

static void doList(int argc, char *argv[]);
const CliCmdType CMD_LIST =
	{"-list", 1, &doList,
		"\t-list:       List all 16relind boards connected, returnsb oards no and stack level for every board\n",
		"\tUsage:       16relind -list\n", "",
		"\tExample:     16relind -list display: 1,0 \n"};

static void doRelayWrite(int argc, char *argv[]);
const CliCmdType CMD_WRITE = {"write", 2, &doRelayWrite,
	"\twrite:       Set relays On/Off\n",
	"\tUsage:       16relind <id> write <channel> <on/off>\n",
	"\tUsage:       16relind <id> write <value>\n",
	"\tExample:     16relind 0 write 2 On; Set Relay #2 on Board #0 On\n"};

static void doRelayRead(int argc, char *argv[]);
const CliCmdType CMD_READ = {"read", 2, &doRelayRead,
	"\tread:        Read relays status\n",
	"\tUsage:       16relind <id> read <channel>\n",
	"\tUsage:       16relind <id> read\n",
	"\tExample:     16relind 0 read 2; Read Status of Relay #2 on Board #0\n"};

static void doRelayFailsafeEnWrite(int argc, char *argv[]);
const CliCmdType CMD_FAILSAFE_EN_WRITE = {"fsenwr", 2, &doRelayFailsafeEnWrite,
	"\tfsenwr:       Enable/disable the failsafe state for a relay\n",
	"\tUsage:       16relind <id> fsenwr <channel> <on/off>\n",
	"\tUsage:       16relind <id> fsenwr <value>\n",
	"\tExample:     16relind 0 fsenwr 2 On; Enable failsafe state for Relay #2 on Board #0 \n"};

static void doRelayFailsafeEnRead(int argc, char *argv[]);
const CliCmdType CMD_FAILSAFE_EN_READ = {"fsenrd", 2, &doRelayFailsafeEnRead,
	"\tfsenrd:       Read the failsafe state enable for a relay\n",
	"\tUsage:       16relind <id> fsenrd <channel>\n",
	"\tUsage:       16relind <id> fsenrd\n",
	"\tExample:     16relind 0 fsenrd 2; Read if failsafe state is enabled for Relay #2 on Board #0 \n"};

static void doRelayFailsafeStateWrite(int argc, char *argv[]);
const CliCmdType CMD_FAILSAFE_STATE_WRITE = {"fsvwr", 2, &doRelayFailsafeStateWrite,
	"\tfsvwr:       Enable/disable the failsafe state for a relay\n",
	"\tUsage:       16relind <id> fsvwr <channel> <on/off>\n",
	"\tUsage:       16relind <id> fsvwr <value>\n",
	"\tExample:     16relind 0 fsvwr 2 On; Set failsafe state for Relay #2 on Board #0 to ON\n"};
	
static void doRelayFailsafeStateRead(int argc, char *argv[]);
const CliCmdType CMD_FAILSAFE_STATE_READ = {"fsvrd", 2, &doRelayFailsafeStateRead,
	"\tfsvrd:       Read the failsafe state for a relay\n",
	"\tUsage:       16relind <id> fsvrd	 <channel>\n",
	"\tUsage:       16relind <id> fsvrd\n",
	"\tExample:     16relind 0 fsvrd 2; Read failsafe state for Relay #2 on Board #0 \n"};	

static void doLedSet(int argc, char *argv[]);
const CliCmdType CMD_LED_BLINK = {"pled", 2, &doLedSet,
	"\tpled:        Set the power led mode (blink | on | off) \n",
	"\tUsage:       16relind <id> pled <blink/off/on>\n", "",
	"\tExample:     16relind 0 pled on; Set power led to always on state \n"};

static void doTest(int argc, char *argv[]);
const CliCmdType CMD_TEST = {"test", 2, &doTest,
	"\ttest:        Turn ON and OFF the relays until press a key\n",
	"\tUsage:       16relind <id> test\n", " ",
	"\tExample:     16relind 0 test\n"};

char *usage = "Usage:	 16relind -h <command>\n"
	"         16relind -v\n"
	"         16relind -warranty\n"
	"         16relind -list\n"
	"         16relind <id> write <channel> <on/off>\n"
	"         16relind <id> write <value>\n"
	"         16relind <id> read <channel>\n"
	"         16relind <id> read\n"
	"         16relind <id> test\n"
	"Where: <id> = Board level id = 0..7\n"
	"Type 16relind -h <command> for more help"; // No trailing newline needed here.

char *warranty =
	"	       Copyright (c) 2016-2026 Sequent Microsystems\n"
		"                                                             \n"
		"		This program is free software; you can redistribute it and/or modify\n"
		"		it under the terms of the GNU Leser General Public License as published\n"
		"		by the Free Software Foundation, either version 3 of the License, or\n"
		"		(at your option) any later version.\n"
		"                                    \n"
		"		This program is distributed in the hope that it will be useful,\n"
		"		but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
		"		MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
		"		GNU Lesser General Public License for more details.\n"
		"			\n"
		"		You should have received a copy of the GNU Lesser General Public License\n"
		"		along with this program. If not, see <http://www.gnu.org/licenses/>.";
u16 relayToIO(u16 relay)
{
	u8 i;
	u16 val = 0;
	for (i = 0; i < 16; i++)
	{
		if ( (relay & (1 << i)) != 0)
			val += relayMaskRemap[i];
	}
	return val;
}

u16 IOToRelay(u16 io)
{
	u8 i;
	u16 val = 0;
	for (i = 0; i < 16; i++)
	{
		if ( (io & relayMaskRemap[i]) != 0)
		{
			val += 1 << i;
		}
	}
	return val;
}

int relayChSet(int dev, u8 channel, OutStateEnumType state)
{
	int resp;
	u8 buff[2];
	u16 val = 0;

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, RELAY16_INPORT_REG_ADD, buff, 2))
	{
		return FAIL;
	}
	memcpy(&val, buff, 2);
	switch (state)
	{
	case OFF:
		val &= ~ (1 << relayChRemap[channel - 1]);
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, RELAY16_OUTPORT_REG_ADD, buff, 2);
		break;
	case ON:
		val |= 1 << relayChRemap[channel - 1];
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, RELAY16_OUTPORT_REG_ADD, buff, 2);
		break;
	default:
		printf("Invalid relay state!\n");
		return ERROR;
		break;
	}
	return resp;
}

int relayChGet(int dev, u8 channel, OutStateEnumType *state)
{
	u8 buff[2];
	u16 val;

	if (NULL == state)
	{
		return ERROR;
	}

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}

	if (FAIL == i2cMem8Read(dev, RELAY16_INPORT_REG_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&val, buff, 2);
	if (val & (1 << relayChRemap[channel - 1]))
	{
		*state = ON;
	}
	else
	{
		*state = OFF;
	}
	return OK;
}

int relaySet(int dev, int val)
{
	u8 buff[2];
	u16 rVal = 0;

	rVal = relayToIO(0xffff & val);
	memcpy(buff, &rVal, 2);

	return i2cMem8Write(dev, RELAY16_OUTPORT_REG_ADD, buff, 2);
}

int relayGet(int dev, int *val)
{
	u8 buff[2];
	u16 rVal = 0;

	if (NULL == val)
	{
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, RELAY16_INPORT_REG_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&rVal, buff, 2);
	*val = IOToRelay(rVal);
	return OK;
}
// enable failsafe state for each relay, 0 = off, 1 = on
int relayFailsafeEnChSet(int dev, u8 channel, OutStateEnumType state)
{
	int resp;
	u8 buff[2];
	u16 val = 0;

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2))
	{
		return FAIL;
	}
	memcpy(&val, buff, 2);
	switch (state)
	{
	case OFF:
		val &= ~ (1 << relayChRemap[channel - 1]);
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2);
		break;
	case ON:
		val |= 1 << relayChRemap[channel - 1];
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2);
		break;
	default:
		printf("Invalid relay state!\n");
		return ERROR;
		break;
	}
	return resp;
}

int relayFailsafeEnChGet(int dev, u8 channel, OutStateEnumType *state)
{
	u8 buff[2];
	u16 val;

	if (NULL == state)
	{
		return ERROR;
	}

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}

	if (FAIL == i2cMem8Read(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&val, buff, 2);
	if (val & (1 << relayChRemap[channel - 1]))
	{
		*state = ON;
	}
	else
	{
		*state = OFF;
	}
	return OK;
}

int relayFailsafeEnSet(int dev, int val)
{
	u8 buff[2];
	u16 rVal = 0;

	rVal = relayToIO(0xffff & val);
	memcpy(buff, &rVal, 2);

	return i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2);
}

int relayFailsafeEnGet(int dev, int *val)
{
	u8 buff[2];
	u16 rVal = 0;

	if (NULL == val)
	{
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, I2C_MEM_RELAY_FAILSAFE_EN_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&rVal, buff, 2);
	*val = IOToRelay(rVal);
	return OK;
}

// set/get failsafe state for each relay, 0 = off, 1 = on
int relayFailsafeStateChSet(int dev, u8 channel, OutStateEnumType state)
{
	int resp;
	u8 buff[2];
	u16 val = 0;

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, 	I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2))
	{
		return FAIL;
	}
	memcpy(&val, buff, 2);
	switch (state)
	{
	case OFF:
		val &= ~ (1 << relayChRemap[channel - 1]);
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2);
		break;
	case ON:
		val |= 1 << relayChRemap[channel - 1];
		memcpy(buff, &val, 2);
		resp = i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2);
		break;
	default:
		printf("Invalid relay state!\n");
		return ERROR;
		break;
	}
	return resp;
}

int relayFailsafeStateChGet(int dev, u8 channel, OutStateEnumType *state)
{
	u8 buff[2];
	u16 val;

	if (NULL == state)
	{
		return ERROR;
	}

	if ( (channel < CHANNEL_NR_MIN) || (channel > RELAY_CH_NR_MAX))
	{
		printf("Invalid relay nr!\n");
		return ERROR;
	}

	if (FAIL == i2cMem8Read(dev, I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&val, buff, 2);
	if (val & (1 << relayChRemap[channel - 1]))
	{
		*state = ON;
	}
	else
	{
		*state = OFF;
	}
	return OK;
}

int relayFailsafeStateSet(int dev, int val)
{
	u8 buff[2];
	u16 rVal = 0;

	rVal = relayToIO(0xffff & val);
	memcpy(buff, &rVal, 2);

	return i2cMem8Write(dev, I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2);
}

int relayFailsafeStateGet(int dev, int *val)
{
	u8 buff[2];
	u16 rVal = 0;

	if (NULL == val)
	{
		return ERROR;
	}
	if (FAIL == i2cMem8Read(dev, I2C_MEM_RELAY_FAILSAFE_VAL_ADD, buff, 2))
	{
		return ERROR;
	}
	memcpy(&rVal, buff, 2);
	*val = IOToRelay(rVal);
	return OK;
}



int doBoardInit(int stack)
{
	int dev = 0;
	int add = 0;
	uint8_t buff[8];

	if ( (stack < 0) || (stack > 7))
	{
		printf("Invalid stack level [0..7]!");
		return ERROR;
	}
	add = (stack + RELAY16_HW_I2C_BASE_ADD) ^ 0x07;
	dev = i2cSetup(add);
	if (dev == -1)
	{
		return ERROR;

	}
	if (ERROR == i2cMem8Read(dev, RELAY16_CFG_REG_ADD, buff, 1))
	{
		add = (stack + RELAY16_HW_I2C_ALTERNATE_BASE_ADD) ^ 0x07;
		dev = i2cSetup(add);
		if (dev == -1)
		{
			return ERROR;
		}
		if (ERROR == i2cMem8Read(dev, RELAY16_CFG_REG_ADD, buff, 1))
		{
			printf("16relind board id %d not detected\n", stack);
			return ERROR;
		}
	}
	if (buff[0] != 0) //non initialized I/O Expander
	{
		// make all I/O pins output
		buff[0] = 0;
		buff[1] = 0;
		if (0 > i2cMem8Write(dev, RELAY16_CFG_REG_ADD, buff, 2))
		{
			return ERROR;
		}
		// put all pins in 0-logic state
		buff[0] = 0;
		if (0 > i2cMem8Write(dev, RELAY16_OUTPORT_REG_ADD, buff, 2))
		{
			return ERROR;
		}
	}

	return dev;
}

int boardCheck(int hwAdd)
{
	int dev = 0;
	uint8_t buff[8];

	hwAdd ^= 0x07;
	dev = i2cSetup(hwAdd);
	if (dev == -1)
	{
		return FAIL;
	}
	if (ERROR == i2cMem8Read(dev, RELAY16_CFG_REG_ADD, buff, 1))
	{
		return ERROR;
	}
	return OK;
}

/*
 * doRelayWrite:
 *	Write coresponding relay channel
 **************************************************************************************
 */
static void doRelayWrite(int argc, char *argv[])
{
	int pin = 0;
	OutStateEnumType state = STATE_COUNT;
	int val = 0;
	int dev = 0;
	OutStateEnumType stateR = STATE_COUNT;
	int valR = 0;
	int retry = 0;

	if ( (argc != 5) && (argc != 4))
	{
		printf("Usage: 16relind <id> write <relay number> <on/off> \n");
		printf("Usage: 16relind <id> write <relay reg value> \n");
		return;
	}

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}
	if (argc == 5)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range\n");
			return;
		}

		/**/if ( (strcasecmp(argv[4], "up") == 0)
			|| (strcasecmp(argv[4], "on") == 0))
			state = ON;
		else if ( (strcasecmp(argv[4], "down") == 0)
			|| (strcasecmp(argv[4], "off") == 0))
			state = OFF;
		else
		{
			if ( (atoi(argv[4]) >= STATE_COUNT) || (atoi(argv[4]) < 0))
			{
				printf("Invalid relay state!\n");
				return;
			}
			state = (OutStateEnumType)atoi(argv[4]);
		}

		retry = RETRY_TIMES;

		while ( (retry > 0) && (stateR != state))
		{
			if (OK != relayChSet(dev, pin, state))
			{
				printf("Fail to write relay\n");
				return;
			}
			if (OK != relayChGet(dev, pin, &stateR))
			{
				printf("Fail to read relay\n");
				return;
			}
			retry--;
		}
#ifdef DEBUG_I
		if(retry < RETRY_TIMES)
		{
			printf("retry %d times\n", 3-retry);
		}
#endif
		if (retry == 0)
		{
			printf("Fail to write relay\n");
			return;
		}
	}
	else
	{
		val = atoi(argv[3]);
		if (val < 0 || val > 255)
		{
			printf("Invalid relay value\n");
			return;
		}

		retry = RETRY_TIMES;
		valR = -1;
		while ( (retry > 0) && (valR != val))
		{

			if (OK != relaySet(dev, val))
			{
				printf("Fail to write relay!\n");
				return;
			}
			if (OK != relayGet(dev, &valR))
			{
				printf("Fail to read relay!\n");
				return;
			}
		}
		if (retry == 0)
		{
			printf("Fail to write relay!\n");
			return;
		}
	}
}

/*
 * doRelayRead:
 *	Read relay state
 ******************************************************************************************
 */
static void doRelayRead(int argc, char *argv[])
{
	int pin = 0;
	int val = 0;
	int dev = 0;
	OutStateEnumType state = STATE_COUNT;

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range!\n");
			return;
		}

		if (OK != relayChGet(dev, pin, &state))
		{
			printf("Fail to read!\n");
			return;
		}
		if (state != 0)
		{
			printf("1\n");
		}
		else
		{
			printf("0\n");
		}
	}
	else if (argc == 3)
	{
		if (OK != relayGet(dev, &val))
		{
			printf("Fail to read!\n");
			return;
		}
		printf("%d\n", val);
	}
	else
	{
		printf("Usage: %s read relay value\n", argv[0]);
		return;
	}
}

void doRelayFailsafeEnWrite(int argc, char *argv[])
{
	int pin = 0;
	OutStateEnumType state = STATE_COUNT;
	int val = 0;
	int dev = 0;
	
	int valR = 0;
	int retry = 0;

	if ( (argc != 5) && (argc != 4))
	{
		printf("Usage: 16relind <id> fsenwr <relay number> <on/off> \n");
		printf("Usage: 16relind <id> fsenwr <relay reg value> \n");
		return;
	}

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}
	if (argc == 5)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range\n");
			return;
		}

		/**/if ( (strcasecmp(argv[4], "up") == 0)
			|| (strcasecmp(argv[4], "on") == 0))
			state = ON;
		else if ( (strcasecmp(argv[4], "down") == 0)
			|| (strcasecmp(argv[4], "off") == 0))
			state = OFF;
		else
		{
			if ( (atoi(argv[4]) >= STATE_COUNT) || (atoi(argv[4]) < 0))
			{
				printf("Invalid relay state!\n");
				return;
			}
			state = (OutStateEnumType)atoi(argv[4]);
		}

		
		if (OK != relayFailsafeEnChSet(dev, pin, state))
		{
			printf("Fail to write relay failsafe enable\n");
			return;
		}
	}
	else
	{
		val = atoi(argv[3]);
		if (val < 0 || val > 255)
		{
			printf("Invalid relay value\n");
			return;
		}

		retry = RETRY_TIMES;
		valR = -1;
		while ( (retry > 0) && (valR != val))
		{

			if (OK != relayFailsafeEnSet(dev, val))
			{
				printf("Fail to write relay failsafe enable!\n");
				return;
			}
			if (OK != relayFailsafeEnGet(dev, &valR))
			{
				printf("Fail to read relay failsafe enable!\n");
				return;
			}
		}
		if (retry == 0)
		{
			printf("Fail to write relay failsafe enable!\n");
			return;
		}
	}
}

void doRelayFailsafeEnRead(int argc, char *argv[])
{
	int pin = 0;
	int val = 0;
	int dev = 0;
	OutStateEnumType state = STATE_COUNT;

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range!\n");
			return;
		}

		if (OK != relayFailsafeEnChGet(dev, pin, &state))
		{
			printf("Fail to read!\n");
			return;
		}
		if (state != 0)
		{
			printf("1\n");
		}
		else
		{
			printf("0\n");
		}
	}
	else if (argc == 3)
	{
		if (OK != relayFailsafeEnGet(dev, &val))
		{
			printf("Fail to read!\n");
			return;
		}
		printf("%d\n", val);
	}
	else
	{
		printf("Usage: %s fsenrd relay failsafe enable value\n", argv[0]);
		return;
	}
}

void doRelayFailsafeStateWrite(int argc, char *argv[])
{
	int pin = 0;
	OutStateEnumType state = STATE_COUNT;
	int val = 0;
	int dev = 0;
	

	if ( (argc != 5) && (argc != 4))
	{
		printf("Usage: 16relind <id> fstwr <relay number> <on/off> \n");
		printf("Usage: 16relind <id> fstwr <relay reg value> \n");
		return;
	}

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}
	if (argc == 5)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range\n");
			return;
		}

		/**/if ( (strcasecmp(argv[4], "up") == 0)
			|| (strcasecmp(argv[4], "on") == 0))
			state = ON;
		else if ( (strcasecmp(argv[4], "down") == 0)
			|| (strcasecmp(argv[4], "off") == 0))
			state = OFF;
		else
		{
			if ( (atoi(argv[4]) >= STATE_COUNT) || (atoi(argv[4]) < 0))
			{
				printf("Invalid relay state!\n");
				return;
			}
			state = (OutStateEnumType)atoi(argv[4]);
		}

		
		if (OK != relayFailsafeStateChSet(dev, pin, state))
		{
			printf("Fail to write relay failsafe state\n");
			return;
		}
	}
	else
	{
		val = atoi(argv[3]);
		if (val < 0 || val > 255)
		{
			printf("Invalid relay value\n");
			return;
		}

		
		if (OK != relayFailsafeStateSet(dev, val))
		{
			printf("Fail to write relay failsafe state!\n");
			return;
		}
		
	
	}
}

void doRelayFailsafeStateRead(int argc, char *argv[])
{
	int pin = 0;
	int val = 0;
	int dev = 0;
	OutStateEnumType state = STATE_COUNT;

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range!\n");
			return;
		}

		if (OK != relayFailsafeStateChGet(dev, pin, &state))
		{
			printf("Fail to read!\n");
			return;
		}
		if (state != 0)
		{
			printf("1\n");
		}
		else
		{
			printf("0\n");
		}
	}
	else if (argc == 3)
	{
		if (OK != relayFailsafeStateGet(dev, &val))
		{
			printf("Fail to read!\n");
			return;
		}
		printf("%d\n", val);
	}
	else
	{
		printf("Usage: %s fstrd relay failsafe state value\n", argv[0]);
		return;
	}
}

static void doLedSet(int argc, char *argv[])
{
	int dev = 0;
	uint8_t buff[2];

	if (argc == 4)
	{
		dev = doBoardInit(atoi(argv[1]));
		if (dev <= 0)
		{
			return;
		}
		if (strcasecmp(argv[3], "on") == 0)
		{
			buff[0] = 1;
		}
		else if (strcasecmp(argv[3], "off") == 0)
		{
			buff[0] = 2;
		}
		else if (strcasecmp(argv[3], "blink") == 0)
		{
			buff[0] = 0;
		}
		else
		{
			printf("Invalid led mode (blink/on/off)\n");
			return;
		}
		if (0 > i2cMem8Write(dev, I2C_MEM_LED_MODE, buff, 1))
		{
			printf(
				"Fail to write, check if your card version supports the command\n");
			return;
		}

	}
	else
	{
		printf("%s", CMD_LED_BLINK.usage1);
		return;
	}
}


void doBoard(int argc, char *argv[]);
const CliCmdType CMD_BOARD =
	{"board", 2, &doBoard,
		"\tboard:      Display the board firmware version\n",
		"\tUsage:      16relind <id> board\n", "",
		"\tExample:    16relind 0 board; Display the Board #0 firmware version\n"};
void doBoard(int argc, char *argv[])
{
	int dev = 0;
	uint8_t buff[8];

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;//return;
	}

	if (argc == 3)
	{
		if (OK != i2cMem8Read(dev, I2C_MEM_REVISION_MAJOR_ADD, buff, 2))
		{
			printf("Fail to read board version!\n");
			return;//return;
		}
		
		printf("Board Firmware Version: %02d.%02d\n", buff[0], buff[1]);
		
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_BOARD.usage1);
		return;//return;
	}
}
//************************************************************* WDT *************************************************

void doWdtReload(int argc, char *argv[]);
const CliCmdType CMD_WDT_RELOAD =
	{"wdtr", 2, &doWdtReload,
		"\twdtr:		Reload the watchdog timer and enable the watchdog if is disabled\n",
		"\tUsage:		16relind <stack> wdtr\n", "",
		"\tExample:		16relind 0 wdtr; Reload the watchdog timer on Board #0 with the period \n"};

void doWdtReload(int argc, char *argv[])
{
	int dev = 0;
	u8 buff[2] = {0, 0};

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 3)
	{
		buff[0] = WDT_RESET_SIGNATURE;
		if (OK != i2cMem8Write(dev, I2C_MEM_WDT_RESET_ADD, buff, 1))
		{
			printf("Fail to write watchdog reset key!\n");
			return;
		}
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_RELOAD.usage1);
		return;
	}
}

void doWdtSetPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_SET_PERIOD =
	{"wdtpwr", 2, &doWdtSetPeriod,
		"\twdtpwr:		Set the watchdog period in seconds, reload command must be issue in this interval to prevent Raspberry Pi power off\n",
		"\tUsage:		16relind <stack> wdtpwr <val> \n", "",
		"\tExample:		16relind 0 wdtpwr 10; Set the watchdog timer period on Board #0 at 10 seconds \n"};

void doWdtSetPeriod(int argc, char *argv[])
{
	int dev = 0;
	u16 period;
	u8 buff[2] = {0, 0};

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		period = (u16)atoi(argv[3]);
		if (0 == period)
		{
			printf("Invalid period!\n");
			return;
		}
		memcpy(buff, &period, 2);
		if (OK != i2cMem8Write(dev, I2C_MEM_WDT_INTERVAL_SET_ADD, buff, 2))
		{
			printf("Fail to write watchdog period!\n");
			return;
		}
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_SET_PERIOD.usage1);
		return;
	}

}

void doWdtGetPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_GET_PERIOD =
	{"wdtprd", 2, &doWdtGetPeriod,
		"\twdtprd:		Get the watchdog period in seconds, reload command must be issue in this interval to prevent Raspberry Pi power off\n",
		"\tUsage:		16relind <stack> wdtprd \n", "",
		"\tExample:		16relind 0 wdtprd; Get the watchdog timer period on Board #0\n"};

void doWdtGetPeriod(int argc, char *argv[])
{
	int dev = 0;
	u16 period;
	u8 buff[2];

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 3)
	{
		if (OK != i2cMem8Read(dev, I2C_MEM_WDT_INTERVAL_GET_ADD, buff, 2))
		{
			printf("Fail to read watchdog period!\n");
			return;
		}
		memcpy(&period, buff, 2);
		printf("%d\n", (int)period);
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_GET_PERIOD.usage1);
		return;
	}

}

void doWdtSetInitPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_SET_INIT_PERIOD =
	{"wdtipwr", 2, &doWdtSetInitPeriod,
		"\twdtipwr:	Set the watchdog initial period in seconds, This period is loaded after power cycle, giving Raspberry time to boot\n",
		"\tUsage:		16relind <stack> wdtipwr <val> \n", "",
		"\tExample:		16relind 0 wdtipwr 10; Set the watchdog timer initial period on Board #0 at 10 seconds \n"};

void doWdtSetInitPeriod(int argc, char *argv[])
{
	int dev = 0;
	u16 period;
	u8 buff[2] = {0, 0};

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		period = (u16)atoi(argv[3]);
		if (0 == period)
		{
			printf("Invalid period!\n");
			return;
		}
		memcpy(buff, &period, 2);
		if (OK != i2cMem8Write(dev, I2C_MEM_WDT_INIT_INTERVAL_SET_ADD, buff, 2))
		{
			printf("Fail to write watchdog period!\n");
			return;
		}
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_SET_INIT_PERIOD.usage1);
		return;
	}

}

void doWdtGetInitPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_GET_INIT_PERIOD =
	{"wdtiprd", 2, &doWdtGetInitPeriod,
		"\twdtiprd:	Get the watchdog initial period in seconds. This period is loaded after power cycle, giving Raspberry time to boot\n",
		"\tUsage:		16relind <stack> wdtiprd \n", "",
		"\tExample:		16relind 0 wdtiprd; Get the watchdog timer initial period on Board #0\n"};

void doWdtGetInitPeriod(int argc, char *argv[])
{
	int dev = 0;
	u16 period;
	u8 buff[2];

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 3)
	{
		if (OK != i2cMem8Read(dev, I2C_MEM_WDT_INIT_INTERVAL_GET_ADD, buff, 2))
		{
			printf("Fail to read watchdog period!\n");
			return;
		}
		memcpy(&period, buff, 2);
		printf("%d\n", (int)period);
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_GET_INIT_PERIOD.usage1);
		return;
	}

}

void doWdtSetOffPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_SET_OFF_PERIOD =
	{"wdtopwr", 2, &doWdtSetOffPeriod,
		"\twdtopwr:	Set the watchdog off period in seconds (max 48 days), This is the time that watchdog mantain Raspberry turned off \n",
		"\tUsage:		16relind <stack> wdtopwr <val> \n", "",
		"\tExample:		16relind 0 wdtopwr 10; Set the watchdog off interval on Board #0 at 10 seconds \n"};

void doWdtSetOffPeriod(int argc, char *argv[])
{
	int dev = 0;
	u32 period;
	u8 buff[4] = {0, 0, 0, 0};

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 4)
	{
		period = (u32)atoi(argv[3]);
		if ( (0 == period) || (period > WDT_MAX_OFF_INTERVAL_S))
		{
			printf("Invalid period!\n");
			return;
		}
		memcpy(buff, &period, 4);
		if (OK
			!= i2cMem8Write(dev, I2C_MEM_WDT_POWER_OFF_INTERVAL_SET_ADD, buff, 4))
		{
			printf("Fail to write watchdog period!\n");
			return;
		}
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_SET_OFF_PERIOD.usage1);
		return;
	}

}

void doWdtGetOffPeriod(int argc, char *argv[]);
const CliCmdType CMD_WDT_GET_OFF_PERIOD =
	{"wdtoprd", 2, &doWdtGetOffPeriod,
		"\twdtoprd:	Get the watchdog off period in seconds (max 48 days), This is the time that watchdog mantain Raspberry turned off \n",
		"\tUsage:		16relind <stack> wdtoprd \n", "",
		"\tExample:		16relind 0 wdtoprd; Get the watchdog off period on Board #0\n"};

void doWdtGetOffPeriod(int argc, char *argv[])
{
	int dev = 0;
	u32 period;
	u8 buff[4];

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 3)
	{
		if (OK
			!= i2cMem8Read(dev, I2C_MEM_WDT_POWER_OFF_INTERVAL_GET_ADD, buff, 4))
		{
			printf("Fail to read watchdog period!\n");
			return;
		}
		memcpy(&period, buff, 4);
		printf("%d\n", (int)period);
	}
	else
	{
		printf("Invalid params number:\n %s", CMD_WDT_GET_OFF_PERIOD.usage1);
		return;
	}

}

//********************************************** RS485 *******************************************************

int cfg485Set(int dev, u8 mode, u32 baud, u8 stopB, u8 parity, u8 add)
{
	ModbusSetingsType settings;
	u8 buff[5];

	if (mode > 1)
	{
		printf("Invalid RS485 mode : 0 = disable, 1= Modbus RTU (Slave)!\n");
		return ERROR;
	}

	if (baud > 921600 || baud < 1200)
	{
		if (mode == 0)
		{
			baud = 38400;
		}
		else
		{
			printf("Invalid RS485 Baudrate [1200, 921600]!\n");
			return ERROR;
		}
	}

	if (stopB < 1 || stopB > 2)
	{
		if (mode == 0)
		{
			stopB = 1;
		}
		else
		{
			printf("Invalid RS485 stop bits [1, 2]!\n");
			return ERROR;
		}
	}

	if (parity > 2)
	{
		if (mode == 0)
		{
			parity = 0;
		}
		else
		{
			printf("Invalid RS485 parity 0 = none; 1 = even; 2 = odd! \n");
			return ERROR;
		}
	}
	if (add < 1)
	{
		if (mode == 0)
		{
			add = 1;
		}
		else
		{
			printf("Invalid MODBUS device address: [1, 255]!\n");
			return ERROR;
		}
	}
	settings.mbBaud = baud;
	settings.mbType = mode;
	settings.mbParity = parity;
	settings.mbStopB = stopB;
	settings.add = add;

	memcpy(buff, &settings, sizeof(ModbusSetingsType));
	if (OK != i2cMem8Write(dev, I2C_MODBUS_SETINGS_ADD, buff, 5))
	{
		printf("Fail to write RS485 settings!\n");
		return ERROR;
	}
	return OK;
}

int cfg485Get(int dev)
{
	ModbusSetingsType settings;
	u8 buff[5];

	if (OK != i2cMem8Read(dev, I2C_MODBUS_SETINGS_ADD, buff, 5))
	{
		printf("Fail to read RS485 settings!\n");
		return ERROR;
	}
	memcpy(&settings, buff, sizeof(ModbusSetingsType));
	printf("<mode> <baudrate> <stopbits> <parity> <add> %d %d %d %d %d\n",
		(int)settings.mbType, (int)settings.mbBaud, (int)settings.mbStopB,
		(int)settings.mbParity, (int)settings.add);
	return OK;
}

void doRs485Read(int argc, char *argv[]);
const CliCmdType CMD_RS485_READ = {"cfg485rd", 2, &doRs485Read,
	"\tcfg485rd:    Read the RS485 communication settings\n",
	"\tUsage:      16relind <id> cfg485rd\n", "",
	"\tExample:		16relind 0 cfg485rd; Read the RS485 settings on Board #0\n"};

void doRs485Read(int argc, char *argv[])
{
	int dev = 0;

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}

	if (argc == 3)
	{
		if (OK != cfg485Get(dev))
		{
			return;
		}
	}
	else
	{
		printf("%s", CMD_RS485_READ.usage1);
		return;
	}

}

void doRs485Write(int argc, char *argv[]);
const CliCmdType CMD_RS485_WRITE =
	{"cfg485wr", 2, &doRs485Write,
		"\tcfg485wr:    Write the RS485 communication settings\n",
		"\tUsage:      16relind <id> cfg485wr <mode> <baudrate> <stopBits> <parity> <slaveAddr>\n",
		"",
		"\tExample:		 16relind 0 cfg485wr 1 9600 1 0 1; Write the RS485 settings on Board #0 \n\t\t\t(mode = Modbus RTU; baudrate = 9600 bps; stop bits one; parity none; modbus slave address = 1)\n"};

void doRs485Write(int argc, char *argv[])
{
	int dev = 0;
	u8 mode = 0;
	u32 baud = 1200;
	u8 stopB = 1;
	u8 parity = 0;
	u8 add = 0;

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}
	if (argc == 8)
	{
		mode = 0xff & atoi(argv[3]);
		baud = atoi(argv[4]);
		stopB = 0xff & atoi(argv[5]);
		parity = 0xff & atoi(argv[6]);
		add = 0xff & atoi(argv[7]);
		if (OK != cfg485Set(dev, mode, baud, stopB, parity, add))
		{
			return;
		}
		printf("done\n");
	}
	else
	{
		printf("%s", CMD_RS485_WRITE.usage1);
		return;
	}
}

const CliCmdType *gCmdArray[] = {&CMD_HELP, &CMD_WAR, &CMD_VERSION, &CMD_LIST,
	&CMD_WRITE, &CMD_READ, &CMD_TEST, &CMD_FAILSAFE_EN_READ, &CMD_FAILSAFE_STATE_READ, 
	&CMD_FAILSAFE_EN_WRITE, &CMD_FAILSAFE_STATE_WRITE, &CMD_LED_BLINK, &CMD_WDT_GET_INIT_PERIOD,
	&CMD_WDT_GET_OFF_PERIOD, &CMD_WDT_GET_PERIOD, &CMD_WDT_RELOAD,
	&CMD_WDT_SET_INIT_PERIOD, &CMD_WDT_SET_OFF_PERIOD, &CMD_WDT_SET_PERIOD,
	&CMD_RS485_READ, &CMD_RS485_WRITE,&CMD_BOARD,
	NULL, };

static void doHelp(int argc, char *argv[])
{
	int i = 0;
	if (argc == 3)
	{
		while (NULL != gCmdArray[i])
		{
			if ( (gCmdArray[i]->name != NULL))
			{
				if (strcasecmp(argv[2], gCmdArray[i]->name) == 0)
				{
					printf("%s%s%s%s", gCmdArray[i]->help, gCmdArray[i]->usage1,
						gCmdArray[i]->usage2, gCmdArray[i]->example);
					break;
				}
			}
			i++;
		}
		if (CMD_ARRAY_SIZE == i)
		{
			printf("Option \"%s\" not found\n", argv[2]);
			for (i = 0; i < CMD_ARRAY_SIZE; i++)
			{
				printf("%s", gCmdArray[i]->help);
			}
		}
	}
	else
	{
		while (NULL != gCmdArray[i])
		{
			printf("%s", gCmdArray[i]->help);
			i++;
		}
	}
}

static void doVersion(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	printf("16relind v%d.%d.%d Copyright (c) 2016 - 2026 Sequent Microsystems\n",
	VERSION_BASE, VERSION_MAJOR, VERSION_MINOR);
	printf("\nThis is free software with ABSOLUTELY NO WARRANTY.\n");
	printf("For details type: 16relind -warranty\n");

}

static void doList(int argc, char *argv[])
{
	int ids[8];
	int i;
	int cnt = 0;

	UNUSED(argc);
	UNUSED(argv);

	for (i = 0; i < 8; i++)
	{
		if (boardCheck(RELAY16_HW_I2C_BASE_ADD + i) == OK)
		{
			ids[cnt] = i;
			cnt++;
		}
		else
		{
			if (boardCheck(RELAY16_HW_I2C_ALTERNATE_BASE_ADD + i) == OK)
			{
				ids[cnt] = i;
				cnt++;
			}
		}
	}
	printf("%d board(s) detected\n", cnt);
	if (cnt > 0)
	{
		printf("Id:");
	}
	while (cnt > 0)
	{
		cnt--;
		printf(" %d", ids[cnt]);
	}
	printf("\n");
}

/* 
 * Self test for production
 */
static void doTest(int argc, char *argv[])
{
	int dev = 0;
	int i = 0;
	int retry = 0;
	int relVal;
	int valR;
	int relayResult = 0;
	FILE *file = NULL;
	const u8 relayOrder[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
		16};

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		return;
	}
	if (argc == 4)
	{
		file = fopen(argv[3], "w");
		if (!file)
		{
			printf("Fail to open result file\n");
			//return -1;
		}
	}
//relay test****************************
	if (strcasecmp(argv[2], "test") == 0)
	{
		relVal = 0;
		printf(
			"Are all relays and LEDs turning on and off in sequence?\nPress y for Yes or any key for No....");
		startThread();
		while (relayResult == 0)
		{
			for (i = 0; i < RELAY_CH_NR_MAX; i++)
			{
				relayResult = checkThreadResult();
				if (relayResult != 0)
				{
					break;
				}
				valR = 0;
				relVal = (u16)1 << (relayOrder[i] - 1);

				retry = RETRY_TIMES;
				while ( (retry > 0) && ( (valR & relVal) == 0))
				{
					if (OK != relayChSet(dev, relayOrder[i], ON))
					{
						retry = 0;
						break;
					}

					if (OK != relayGet(dev, &valR))
					{
						retry = 0;
					}
				}
				if (retry == 0)
				{
					printf("Fail to write relay\n");
					if (file)
						fclose(file);
					return;
				}
				busyWait(150);
			}

			for (i = 0; i < RELAY_CH_NR_MAX; i++)
			{
				relayResult = checkThreadResult();
				if (relayResult != 0)
				{
					break;
				}
				valR = 0xffff;
				relVal = (u16)1 << (relayOrder[i] - 1);
				retry = RETRY_TIMES;
				while ( (retry > 0) && ( (valR & relVal) != 0))
				{
					if (OK != relayChSet(dev, relayOrder[i], OFF))
					{
						retry = 0;
					}
					if (OK != relayGet(dev, &valR))
					{
						retry = 0;
					}
				}
				if (retry == 0)
				{
					printf("Fail to write relay!\n");
					if (file)
						fclose(file);
					return;
				}
				busyWait(150);
			}
		}
	}
	if (relayResult == YES)
	{
		if (file)
		{
			fprintf(file, "Relay Test ............................ PASS\n");
		}
		else
		{
			printf("Relay Test ............................ PASS\n");
		}
	}
	else
	{
		if (file)
		{
			fprintf(file, "Relay Test ............................ FAIL!\n");
		}
		else
		{
			printf("Relay Test ............................ FAIL!\n");
		}
	}
	if (file)
	{
		fclose(file);
	}
	relaySet(dev, 0);
}

static void doWarranty(int argc UNU, char* argv[] UNU)
{
	printf("%s\n", warranty);
}

//static void cliInit(void)
//{
//	int i = 0;
//
//	memset(gCmdArray, 0, sizeof(CliCmdType) * CMD_ARRAY_SIZE);
//
//	memcpy(&gCmdArray[i], &CMD_HELP, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_WAR, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_LIST, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_WRITE, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_READ, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_LED_BLINK, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_TEST, sizeof(CliCmdType));
//	i++;
//	memcpy(&gCmdArray[i], &CMD_VERSION, sizeof(CliCmdType));
//
//}

int waitForI2C(sem_t *sem)
{
	int semVal = 2;
	struct timespec ts;
	int s = 0;

#ifdef DEBUG_SEM
	sem_getvalue(sem, &semVal);
	printf("Semaphore initial value %d\n", semVal);
	semVal = 2;
#endif
	while (semVal > 0)
	{
		if (clock_gettime(CLOCK_REALTIME, &ts) == -1)
		{
			/* handle error */
			printf("Fail to read time \n");
			return -1;
		}
		ts.tv_sec += TIMEOUT_S;
		while ( (s = sem_timedwait(sem, &ts)) == -1 && errno == EINTR)
			continue; /* Restart if interrupted by handler */
		sem_getvalue(sem, &semVal);
	}
#ifdef DEBUG_SEM
	sem_getvalue(sem, &semVal);
	printf("Semaphore after wait %d\n", semVal);
#endif
	return 0;
}

int releaseI2C(sem_t *sem)
{
	int semVal = 2;
	sem_getvalue(sem, &semVal);
#ifdef DEBUG_SEM
	sem_getvalue(sem, &semVal);
	printf("Semaphore before post %d\n", semVal);
#endif
	if (semVal < 1)
	{
		if (sem_post(sem) == -1)
		{
			printf("Fail to post SMI2C_SEM \n");
			return -1;
		}
	}
#ifdef DEBUG_SEM
	sem_getvalue(sem, &semVal);
	printf("Semaphore after post %d\n", semVal);
#endif
	return 0;
}

int main(int argc, char *argv[])
{
	int i = 0;

	//cliInit();

	if (argc == 1)
	{
		while (NULL != gCmdArray[i])
		{
			printf("%s", gCmdArray[i]->help);
			i++;
		}
		return 1;
	}
#ifdef THREAD_SAFE
	sem_t *semaphore = sem_open("/SMI2C_SEM", O_CREAT, 0000666, 3);
	waitForI2C(semaphore);
#endif
	i = 0;
	while (NULL != gCmdArray[i])
	{
		if ( (gCmdArray[i]->name != NULL) && (gCmdArray[i]->namePos < argc))
		{
			if (strcasecmp(argv[gCmdArray[i]->namePos], gCmdArray[i]->name) == 0)
			{
				gCmdArray[i]->pFunc(argc, argv);
#ifdef THREAD_SAFE
				releaseI2C(semaphore);
#endif
				return 0;
			}
		}
		i++;
	}
	printf("Invalid command option\n");
	i = 0;
	while (NULL != gCmdArray[i])
	{
		printf("%s", gCmdArray[i]->help);
		i++;
	}
#ifdef THREAD_SAFE
	releaseI2C(semaphore);
#endif
	return 0;
}
