/*
 * commands.c
 *
 *  Created on: Feb. 3, 2025
 *      Author: slocke
 */
//#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "commands.h"
#include "env.h"
#include "message.h"
#include "peripherals.h"
#include <math.h>
#include "channel.h"
#include "isp.h"

/*
 * Echos the first argument back (for testing)
 */
int EchoCommand(struct Command * cmd,struct Message * msg, int argc, char * argv[]){
	if(argc >= 2) MessageSendFormat(msg, "%s,%s",cmd->str, argv[1]);
	else return -1;
	return 0;
}

/*
 * ENV,SAVE
 * ENV,DEFAULT
 * ENV,NMEA_BAUD,<baud>
 * ENV,ECHO_BAUD,<baud>
 * ENV,THERMISTOR,<r25>,<a>,<b>,<c>,<d>
 */

int EnvCommand(struct Command * cmd,struct Message * msg, int argc, char * argv[]){
	if(strcmp(argv[1],"SAVE")==0){
		EnvWrite();
		MessageSendFormat(msg, "%s,%s,OK",cmd->str,argv[1]);
		return 0;
	}

	if(strcmp(argv[1],"DEFAULT")==0){
		EnvSetDefaults();
		MessageSendFormat(msg, "%s,%s,OK",cmd->str,argv[1]);
		return 0;
	}

	if(strcmp(argv[1],"LOAD")==0){
		EnvInit();
		MessageSendFormat(msg, "%s,%s,%s",cmd->str,argv[1],env.flags&ENV_FLAGS_DEFAULT ? "DEF" : "OK");
		return 0;
	}

	if(strcmp(argv[1],"UART_BAUD")==0 && (argc == 3 || argv[2][0] == '?' || argc == 2)){
		if(argc == 3){
			env.uart_baud = strtoul(argv[2],NULL,10);
		}
		MessageSendFormat(msg, "%s,%s,%i",cmd->str,argv[1],
					env.uart_baud);
		return 0;
	}


	MessageSendFormat(msg, "%s,?",cmd->str);

	return 0;
}



#define xstr(a) str(a)
#define str(a) #a
int VerCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	//MessageSendFormat(reply_stream, "%s," xstr(FW_VERSION),cmd->str);
	MessageSendFormat(msg, "%s," "TEST",cmd->str);
	return 0;
}


/*
 * Set parameters of the ping
 * PING,start
 * Ping,stop
 * ping,len,<microseconds>
 * ping,per,<microseconds>
 * ping,lna,[10,12.5,15,20]
 * ping,power,[0|1]
 * ping,q,[2-5]
 */
const float lna_values[] = {
		15, 10, 20, 12.5
};
const uint8_t q_values[] = {
		4,5,2,3
};

/*
 * To write SN:
 * SN,W,ABC123,<serial_number>
 * To read SN:
 * SN
 */
/*
int SNCommand(struct Command * cmd,StreamBufferHandle_t reply_stream,int argc, char * argv[]){
	if(argc == 4 && argv[1][0] == 'W' && strcmp(argv[2],"ABC123")==0){
		strncpy(env.sn,argv[3],ENV_MAX_SN_LEN-1);
	}

	MessageSendFormat(reply_stream, "%s,%s",cmd->str,env.sn);

	return 0;
}
*/

/*
 * Set channel parameters (voltage/TIA amplification/isolation
 * To read:
 * CH,[ch_num]
 * To write:
 * CH, [ch_num],[on/off],[voltage],[1|10|100|1000]
 */
int ChCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	uint8_t ch;
	struct channel_state st;
	bool isolate = 0;
	uint16_t range = 0;

	if(argc >= 2){
		ch = strtoul(argv[1],NULL,0)-1;
		if(ch >= NUM_CHANNELS){
			return 1;
		}
	}else{
		return 1;
	}

	if(argc == 5){
		//Set range
		if(strncasecmp(argv[2],"on",2)==0) isolate = 0;
		else if(strncasecmp(argv[2],"off",3)==0) isolate = 1;
		else return 1;

		st.voltage = strtof(argv[3],NULL);
		range = strtoul(argv[4],NULL,0);
		st.range = ChannelRangeUaToNum(range);
		ChannelSetState(ch,&st,isolate);

	}else if(argc != 2){
		//not a set, not a read. syntax error
		return 1;
	}
	isolate = ChannelGetState(ch,&st);
	/* Range 0 is not a real range (1/10/100/1000): it says this channel's IO
	 * expander (or, rarely, its bias pot) did not answer the read-back, so the
	 * bias and TIA gain reported here would be inventions. A channel with no
	 * expander at all is also held isolated ("off"). Same field count as always,
	 * so no parser downstream sees a new shape. */
	MessageSendFormat(msg,"%s,%u,%s,%f,%u",cmd->str,
			ch+1,
			isolate ? "off" : "on",
			st.io_exp_ok ? st.voltage : 0.0f,
			st.io_exp_ok ? ChannelRangeNumToUa(st.range) : 0
			);
	return 0;
}

/*
 * Report which IO expander ChannelInit found for each channel, so a board whose
 * expanders sit at an unexpected address, or are missing, says so instead of
 * streaming numbers with an unknown scale. Purely additive to the protocol.
 * IOEXP
 * -> IOEXP,0x20,0x21,0x22,0x23   TCA9554 fitted
 * -> IOEXP,0x38,0x39,0x3A,0x3B   TCA9554A fitted
 * -> IOEXP,NONE,...              no expander answered on that channel
 */
int IoExpCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	char fields[NUM_CHANNELS][5];
	uint8_t addr;
	int i;

	/* The format string below spells out one %s per channel. */
	_Static_assert(NUM_CHANNELS == 4, "IOEXP reply format hard-codes four channels");

	for(i=0;i<NUM_CHANNELS;i++){
		addr = ChannelIoExpAddress(i);
		if(addr) snprintf(fields[i],sizeof(fields[i]),"0x%02X",addr);
		else strncpy(fields[i],"NONE",sizeof(fields[i]));
	}
	MessageSendFormat(msg,"%s,%s,%s,%s,%s",cmd->str,fields[0],fields[1],fields[2],fields[3]);
	return 0;
}

/*
 * Hand the board to the ROM bootloader so new firmware can be flashed with blhost over the
 * same USB cable, without the bootload jumper (JP1). After the ack the CDC port disappears
 * and the board re-enumerates as the ROM's USB HID device (VID 0x1FC9, PID 0x0022); blhost
 * `reset`, or a power cycle, boots whatever is in flash. The second word is deliberate: a
 * bare ISP must never be enough to take a measuring board away from its operator.
 * ISP,ENTER
 * -> ISP,ENTER,OK   then the port is gone
 * -> ISP,?          anything else, and nothing happens
 */
int IspCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	if(argc != 2 || strcmp(argv[1],"ENTER") != 0){
		MessageSendFormat(msg,"%s,?",cmd->str);
		return 0;
	}
	MessageSendFormat(msg,"%s,%s,OK",cmd->str,argv[1]);
	/* Let DataTxTask and the USB stack push the ack out before the port is torn down. */
	vTaskDelay(pdMS_TO_TICKS(200));
	IspEnterRomBootloader();
	return 0;	/* not reached */
}

/*
 * This table has our message commands and the function to execute for each
 */
static struct Command commands[] = {

		{"ECHO",EchoCommand},
		{"VER",VerCommand},
		{"ENV",EnvCommand},
		{"CH",ChCommand},
		{"IOEXP",IoExpCommand},
		{"ISP",IspCommand},
		//{"SN",SNCommand},

};

static void HandleMessage(struct Message * msg){
	char * argv[COMMAND_MAX_ARGS];
	char * id;
	int argc;
	int i;

	//The first argument is the Talker ID of the board.  Does it match ours?
	id = strtok(msg->message,COMMAND_DELIM);
	if(id == NULL || *id == '\0') return;	//no ID!

	if(strcmp(id,TALKER_ID) != 0) return;	//message not for us

	argc=0;
	do{
		argv[argc] = strtok(NULL,COMMAND_DELIM);
	}while(argv[argc++] != NULL && argc < COMMAND_MAX_ARGS);
	argc--;
	for(i=0;i<ARRAY_SIZE(commands);i++){
		if(strncmp(argv[0],commands[i].str,strlen(commands[i].str)) == 0){
			//match
			if(commands[i].Func != NULL){
				if(commands[i].Func(&commands[i],msg, argc, argv)){
					MessageSendFormat(msg,"ERROR,COMMAND PARSE ERROR");
				}
			}
			break;//out of FOR loop
		}
	}
	if(i==ARRAY_SIZE(commands)) MessageSendFormat(msg,"ERROR,INVALID COMMAND,'%s'",argv[0]);
}


void CommandParserTask(void*params){
	QueueHandle_t * msg_queue = (QueueHandle_t*)params;
	struct Message msg;

	while(1){
		xQueueReceive(*msg_queue,&msg,portMAX_DELAY);
		HandleMessage(&msg);
	}
}

void CommandParserInit(QueueHandle_t * message_rx_queue){
	*message_rx_queue = xQueueCreate(3,sizeof(struct Message));
	vQueueAddToRegistry(*message_rx_queue,"CMD");
	xTaskCreate(CommandParserTask,"CMD",384,message_rx_queue,1,NULL);

}


