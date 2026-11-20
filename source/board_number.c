/*
 * board_number.c
 *
 * A six-byte record on the settings EEPROM, on a page of its own so that a save
 * of the environment (address 0) never touches it:
 *
 *   'R' 'N'  number, low byte then high byte  the same two bytes inverted
 *
 * A blank chip reads FF FF FF FF FF FF, which is not a record, so a board that
 * was never given a number reads NONE. The inverted copy catches a record that
 * was only half written. Flashing new firmware does not touch this chip.
 */
#include <stdio.h>
#include <stdint.h>
#include "env.h"
#include "board_number.h"

#define RECORD_ADDR		0x0040	/* the third 32-byte page; the environment is on the first */
#define RECORD_BYTES	6

static int s_number = BOARD_NUMBER_NONE;

int BoardNumberRead(void){
	uint8_t r[RECORD_BYTES];
	uint16_t number,inverted;

	s_number = BOARD_NUMBER_NONE;
	if(!EepromRead(RECORD_ADDR,r,RECORD_BYTES)) return s_number;	/* no chip answered */
	if(r[0] != 'R' || r[1] != 'N') return s_number;

	number = (uint16_t)(r[2] | (r[3] << 8));
	inverted = (uint16_t)(r[4] | (r[5] << 8));
	if((uint16_t)(number ^ inverted) != 0xFFFFU) return s_number;
	if(number > BOARD_NUMBER_MAX) return s_number;

	s_number = number;
	return s_number;
}

void BoardNumberInit(void){
	BoardNumberRead();
}

int BoardNumberGet(void){
	return s_number;
}

int BoardNumberSet(int number){
	uint8_t r[RECORD_BYTES];
	uint16_t n = (uint16_t)number;
	uint16_t inv = (uint16_t)~n;

	r[0] = 'R';
	r[1] = 'N';
	r[2] = (uint8_t)(n & 0xFF);
	r[3] = (uint8_t)(n >> 8);
	r[4] = (uint8_t)(inv & 0xFF);
	r[5] = (uint8_t)(inv >> 8);
	/* The write's own result proves nothing (a chip acknowledges a write it
	 * then does not keep): what counts is what comes back. */
	(void)EepromWrite(RECORD_ADDR,r,RECORD_BYTES);
	return BoardNumberRead();
}

const char * BoardNumberText(int number, char * text){
	if(number < 0 || number > BOARD_NUMBER_MAX) snprintf(text,8,"NONE");
	else snprintf(text,8,"%d",number);
	return text;
}
