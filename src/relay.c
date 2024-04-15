/*
 * relay.c:
 *	Command-line interface to the Raspberry
 *	Pi's 8-Relay Industrial board.
 *	Copyright (c) 2016-2021 Sequent Microsystem
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
#define VERSION_MINOR	(int)1

#define UNUSED(X) (void)X      /* To avoid gcc/g++ warnings */
#define CMD_ARRAY_SIZE	8

#define THREAD_SAFE

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

CliCmdType gCmdArray[CMD_ARRAY_SIZE];

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
	"	       Copyright (c) 2016-2020 Sequent Microsystems\n"
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
		exit(1);
	}

	dev = doBoardInit(atoi(argv[1]));
	if (dev <= 0)
	{
		exit(1);
	}
	if (argc == 5)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range\n");
			exit(1);
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
				exit(1);
			}
			state = (OutStateEnumType)atoi(argv[4]);
		}

		retry = RETRY_TIMES;

		while ( (retry > 0) && (stateR != state))
		{
			if (OK != relayChSet(dev, pin, state))
			{
				printf("Fail to write relay\n");
				exit(1);
			}
			if (OK != relayChGet(dev, pin, &stateR))
			{
				printf("Fail to read relay\n");
				exit(1);
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
			exit(1);
		}
	}
	else
	{
		val = atoi(argv[3]);
		if (val < 0 || val > 255)
		{
			printf("Invalid relay value\n");
			exit(1);
		}

		retry = RETRY_TIMES;
		valR = -1;
		while ( (retry > 0) && (valR != val))
		{

			if (OK != relaySet(dev, val))
			{
				printf("Fail to write relay!\n");
				exit(1);
			}
			if (OK != relayGet(dev, &valR))
			{
				printf("Fail to read relay!\n");
				exit(1);
			}
		}
		if (retry == 0)
		{
			printf("Fail to write relay!\n");
			exit(1);
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
		exit(1);
	}

	if (argc == 4)
	{
		pin = atoi(argv[3]);
		if ( (pin < CHANNEL_NR_MIN) || (pin > RELAY_CH_NR_MAX))
		{
			printf("Relay number value out of range!\n");
			exit(1);
		}

		if (OK != relayChGet(dev, pin, &state))
		{
			printf("Fail to read!\n");
			exit(1);
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
			exit(1);
		}
		printf("%d\n", val);
	}
	else
	{
		printf("Usage: %s read relay value\n", argv[0]);
		exit(1);
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
			exit(1);
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
			exit(1);
		}
		if (0 > i2cMem8Write(dev, I2C_MEM_LED_MODE, buff, 1))
		{
			printf(
				"Fail to write, check if your card version supports the command\n");
			exit(1);
		}

	}
	else
	{
		printf("%s", CMD_LED_BLINK.usage1);
		exit(1);
	}
}

static void doHelp(int argc, char *argv[])
{
	int i = 0;
	if (argc == 3)
	{
		for (i = 0; i < CMD_ARRAY_SIZE; i++)
		{
			if ( (gCmdArray[i].name != NULL))
			{
				if (strcasecmp(argv[2], gCmdArray[i].name) == 0)
				{
					printf("%s%s%s%s", gCmdArray[i].help, gCmdArray[i].usage1,
						gCmdArray[i].usage2, gCmdArray[i].example);
					break;
				}
			}
		}
		if (CMD_ARRAY_SIZE == i)
		{
			printf("Option \"%s\" not found\n", argv[2]);
			for (i = 0; i < CMD_ARRAY_SIZE; i++)
			{
				printf("%s", gCmdArray[i].help);
			}
		}
	}
	else
	{
		for (i = 0; i < CMD_ARRAY_SIZE; i++)
		{
			printf("%s", gCmdArray[i].help);
		}
	}
}

static void doVersion(int argc, char *argv[])
{
	UNUSED(argc);
	UNUSED(argv);
	printf("16relind v%d.%d.%d Copyright (c) 2016 - 2020 Sequent Microsystems\n",
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
		exit(1);
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
					exit(1);
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
					exit(1);
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

static void cliInit(void)
{
	int i = 0;

	memset(gCmdArray, 0, sizeof(CliCmdType) * CMD_ARRAY_SIZE);

	memcpy(&gCmdArray[i], &CMD_HELP, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_WAR, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_LIST, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_WRITE, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_READ, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_LED_BLINK, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_TEST, sizeof(CliCmdType));
	i++;
	memcpy(&gCmdArray[i], &CMD_VERSION, sizeof(CliCmdType));

}

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

	cliInit();

	if (argc == 1)
	{
		for (i = 0; i < CMD_ARRAY_SIZE; i++)
		{
			printf("%s", gCmdArray[i].help);
		}
		return 1;
	}
#ifdef THREAD_SAFE
	sem_t *semaphore = sem_open("/SMI2C_SEM", O_CREAT, 0000666, 3);
	waitForI2C(semaphore);
#endif
	for (i = 0; i < CMD_ARRAY_SIZE; i++)
	{
		if ( (gCmdArray[i].name != NULL) && (gCmdArray[i].namePos < argc))
		{
			if (strcasecmp(argv[gCmdArray[i].namePos], gCmdArray[i].name) == 0)
			{
				gCmdArray[i].pFunc(argc, argv);
#ifdef THREAD_SAFE
				releaseI2C(semaphore);
#endif
				return 0;
			}
		}
	}
	printf("Invalid command option\n");
	for (i = 0; i < CMD_ARRAY_SIZE; i++)
	{
		printf("%s", gCmdArray[i].help);
	}
#ifdef THREAD_SAFE
	releaseI2C(semaphore);
#endif
	return 0;
}
