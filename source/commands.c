/*
 * commands.c
 *
 *  Created on: Feb. 3, 2025
 *      Author: slocke
 */
//#include <string.h>
#include <ctype.h>
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
#include "version.h"
#include "board_id.h"
#include "board_number.h"
#include "blink.h"

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
	/* ENV with no second word: there is no argv[1] to compare. */
	if(argc < 2){
		MessageSendFormat(msg, "%s,?",cmd->str);
		return 0;
	}

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

	/* ENV,UART_BAUD and ENV,UART_BAUD,? read the value; ENV,UART_BAUD,<baud> sets it.
	 * argc is tested before argv[2] is touched, and a '?' never reaches strtoul
	 * (it used to, and set the value to 0 where it was meant to read it). */
	if(strcmp(argv[1],"UART_BAUD")==0 && (argc <= 3 || argv[2][0] == '?')){
		if(argc == 3 && argv[2][0] != '?'){
			env.uart_baud = strtoul(argv[2],NULL,10);
		}
		MessageSendFormat(msg, "%s,%s,%i",cmd->str,argv[1],
					env.uart_baud);
		return 0;
	}


	MessageSendFormat(msg, "%s,?",cmd->str);

	return 0;
}



/*
 * The firmware's version.
 * VER
 * -> VER,2.0.0
 * The field is FW_VERSION's characters and nothing else (version.h says why).
 * Firmware before 2.0.0 answers VER,TEST.
 */
int VerCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	MessageSendFormat(msg, "%s,%s",cmd->str,FW_VERSION);
	return 0;
}

/*
 * Who this board is, in one reply.
 * ID
 * -> ID,<id>,<version>,<number>
 * <id>      the board's permanent ID: 32 hexadecimal digits in lower case, the
 *           same text as its USB serial number. 32 zeros on a board that could
 *           not read it.
 * <version> the same text VER returns.
 * <number>  the board's sticker number, or NONE when the board holds none.
 * More fields may be appended later; a reader ignores fields it does not know.
 * Firmware before 2.0.0 answers ERROR,INVALID COMMAND,'ID'.
 */
int IdCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	char number[8];
	MessageSendFormat(msg, "%s,%s,%s,%s",cmd->str,BoardIdHex(),FW_VERSION,
			BoardNumberText(BoardNumberGet(),number));
	return 0;
}

/*
 * The board's sticker number, kept on the settings EEPROM so that it survives a
 * firmware update. The flashing script writes it; SPARC CS only reads it, from
 * the ID reply.
 * BOARD
 * -> BOARD,<number>     0 to 9999, or NONE when the board holds none
 * BOARD,SET,<number>
 * -> BOARD,<number>     what the chip holds after the write, read back from it
 * -> BOARD,ERR,STORE    the chip did not keep it
 * -> BOARD,?            the number is not 0 to 9999, or anything else was sent
 * 0 is a real board (RMD #0), so it never stands for "none".
 */
int BoardCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	char number[8];
	unsigned long want;
	size_t digits;

	if(argc == 1){
		MessageSendFormat(msg,"%s,%s",cmd->str,BoardNumberText(BoardNumberRead(),number));
		return 0;
	}

	if(argc == 3 && strcmp(argv[1],"SET")==0){
		/* One to four digits and nothing else: no sign, no space, no "0x". */
		digits = strspn(argv[2],"0123456789");
		if(digits >= 1 && digits <= 4 && argv[2][digits] == '\0'){
			want = strtoul(argv[2],NULL,10);
			if(BoardNumberSet((int)want) == (int)want){
				MessageSendFormat(msg,"%s,%s",cmd->str,BoardNumberText((int)want,number));
			}else{
				MessageSendFormat(msg,"%s,ERR,STORE",cmd->str);
			}
			return 0;
		}
	}

	MessageSendFormat(msg,"%s,?",cmd->str);
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
	unsigned long ch_num;
	uint8_t ch;
	struct channel_state st;
	bool isolate = 0;
	uint16_t range = 0;

	if(argc >= 2){
		/* Range-checked before it is narrowed: as a uint8_t, channel 257 wrapped
		 * round to channel 1. */
		ch_num = strtoul(argv[1],NULL,0);
		if(ch_num < 1 || ch_num > NUM_CHANNELS){
			return 1;
		}
		ch = ch_num-1;
	}else{
		return 1;
	}

	if(argc == 5){
		/* The whole word, on or off. HandleMessage has already turned the letters
		 * to upper case. A prefix match here took "ONLY" for on. */
		if(strcmp(argv[2],"ON")==0) isolate = 0;
		else if(strcmp(argv[2],"OFF")==0) isolate = 1;
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
 * Flash this board's LEDs for 5 seconds, so a person can see which board it is:
 * every channel's red and green in turn, and the board's own red LED, D1.
 * Answered at once; the flashing then runs by itself and a second BLINK inside
 * the 5 seconds starts them again. Safe at any time: a board either flashes
 * without changing a reading, or refuses.
 * BLINK
 * -> BLINK,OK          the LEDs are flashing
 * -> BLINK,ERR,NOLED   the board has no LED it can flash
 * -> BLINK,ERR,BUSY    a channel is measuring, and this build will not flash
 *                      while it does
 * 2.0.0 sends only OK in practice: D1 is on every board, and flashing was
 * measured not to change a reading (blink.c). The two refusals stay in the
 * protocol for a later build or board that needs them.
 * Firmware before 2.0.0 answers ERROR,INVALID COMMAND,'BLINK'.
 */
int BlinkCommand(struct Command * cmd,struct Message * msg,int argc, char * argv[]){
	switch(BlinkStart()){
		case BLINK_OK:
			MessageSendFormat(msg,"%s,OK",cmd->str);
			break;
		case BLINK_ERR_BUSY:
			MessageSendFormat(msg,"%s,ERR,BUSY",cmd->str);
			break;
		default:
			MessageSendFormat(msg,"%s,ERR,NOLED",cmd->str);
			break;
	}
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
		{"ID",IdCommand},
		{"BOARD",BoardCommand},
		{"ENV",EnvCommand},
		{"CH",ChCommand},
		{"IOEXP",IoExpCommand},
		{"BLINK",BlinkCommand},
		{"ISP",IspCommand},
		//{"SN",SNCommand},

};

static int HexDigit(char c){
	if(c >= '0' && c <= '9') return c - '0';
	if(c >= 'a' && c <= 'f') return c - 'a' + 10;
	if(c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

/*
 * A command may end with *XX, a checksum of two hexadecimal digits: the XOR of
 * every byte after the '$' and before the last '*', exactly as sent. It is the
 * same sum this board puts on its own lines. `text` is the command as it
 * arrived, without its '$' and its line ending, and with no letter changed.
 *
 * Returns 1 when the command carries no checksum, or carries a good one (which
 * is then cut off, so the fields that follow are clean). Returns 0 when it
 * carries a wrong one: a sum that does not match, or a '*' that is not followed
 * by exactly two hexadecimal digits and the end of the command. The two digits
 * are accepted in either case. So a field of a command sent without a checksum
 * cannot hold a '*'.
 */
static int StripChecksum(char * text){
	char * star = strrchr(text,'*');
	char * p;
	int hi,lo;
	uint8_t sum = 0;

	if(star == NULL) return 1;	//no checksum: bench scripts and terminals send none

	hi = HexDigit(star[1]);
	if(hi < 0) return 0;		//also catches a '*' at the very end
	lo = HexDigit(star[2]);
	if(lo < 0) return 0;
	if(star[3] != '\0') return 0;

	for(p=text;p<star;p++) sum ^= (uint8_t)*p;
	if(sum != (uint8_t)((hi << 4) | lo)) return 0;

	*star = '\0';
	return 1;
}

static void HandleMessage(struct Message * msg){
	char * argv[COMMAND_MAX_ARGS+1];
	char * id;
	char * p;
	int argc;
	int i;

	/* The checksum first, over the bytes as they arrived. SPARC CS sends "on",
	 * "off" and its numbers in lower case and sums what it sends, so the check
	 * must come before any letter is changed: a command with an odd number of
	 * lower-case letters sums differently once it is in capitals. A command
	 * that fails is answered and NOT run, so one damaged on the cable can no
	 * longer set a wrong bias or range. */
	if(!StripChecksum(msg->message)){
		MessageSendFormat(msg,"ERROR,BAD CHECKSUM");
		return;
	}

	/* Only now: commands and their words are matched without regard to case. */
	for(p=msg->message;*p != '\0';p++) *p = toupper((unsigned char)*p);

	//The first argument is the Talker ID of the board.  Does it match ours?
	id = strtok(msg->message,COMMAND_DELIM);
	if(id == NULL || *id == '\0') return;	//no ID!

	if(strcmp(id,TALKER_ID) != 0) return;	//message not for us

	/* argv[argc] is always NULL, as main()'s is. */
	argc=0;
	while(argc < COMMAND_MAX_ARGS && (argv[argc] = strtok(NULL,COMMAND_DELIM)) != NULL) argc++;
	argv[argc] = NULL;

	/* A message with no command name: there is no argv[0] to match. */
	if(argc == 0){
		MessageSendFormat(msg,"ERROR,INVALID COMMAND,''");
		return;
	}

	for(i=0;i<ARRAY_SIZE(commands);i++){
		/* The whole name. A prefix match here ran VER for VERSION, and CH for any
		 * name that began with CH. */
		if(strcmp(argv[0],commands[i].str) == 0){
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


