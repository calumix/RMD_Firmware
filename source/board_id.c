/*
 * board_id.c
 *
 * Reads the LPC5514's factory-programmed unique ID. The chip holds nothing else
 * that is unique, so a board that cannot read it reports 32 zeros and publishes
 * no USB serial number at all: Windows tells devices apart by that number and
 * mishandles two that claim the same one.
 *
 * The ID is four 32-bit words in the flash's factory area (the NMPA page, which
 * NXP programs at manufacture and which is never erased), at FLASH_NMPA's
 * UUID_ARRAY: address 0x0003FC70 on this chip. It is read as plain memory.
 *
 * NOT through the ROM's flash API (FLASH_Init / FFR_Init / FFR_GetUUID in
 * fsl_iap). That was the first attempt, on 2026-11-19, and a board flashed with
 * it never reached USB: the firmware runs its core at 150 MHz, the SDK's
 * FLASH_Init tells the ROM the clock is 96 MHz, and the SDK's own comments put a
 * 100 MHz ceiling on the ROM's flash operations. A board in that state cannot
 * even be sent $POEI,ISP,ENTER, so it has to be rescued with the bootload
 * jumper (JP1). Do not call that API from this firmware at boot.
 */
#include <string.h>
#include "fsl_device_registers.h"
#include "board_id.h"

static char s_hex[BOARD_ID_HEX_LEN + 1] = "00000000000000000000000000000000";
static bool s_valid = false;

static bool _all_bytes_are(const uint8_t * b, uint8_t v){
	int i;
	for(i=0;i<BOARD_ID_BYTES;i++){
		if(b[i] != v) return false;
	}
	return true;
}

/* The 16 bytes as they are stored. `blhost get-property 18` prints the same 16
 * bytes in the same order, and the flashing record lists a board's ID that way. */
static void BoardIdReadRaw(uint8_t * uuid){
	_Static_assert(sizeof(FLASH_NMPA->UUID_ARRAY) == BOARD_ID_BYTES, "the chip's ID is 16 bytes");
	memcpy(uuid,(const void *)&FLASH_NMPA->UUID_ARRAY[0],BOARD_ID_BYTES);
}

void BoardIdInit(void){
	static const char digits[] = "0123456789abcdef";
	uint8_t uuid[BOARD_ID_BYTES];
	int i;

	BoardIdReadRaw(uuid);

	/* An ID of all zeros or all ones is a blank field, not an ID: two such
	 * boards would share it. Treated as "could not be read". */
	if(_all_bytes_are(uuid,0x00) || _all_bytes_are(uuid,0xFF)) return;	/* s_hex stays all zeros */

	for(i=0;i<BOARD_ID_BYTES;i++){
		s_hex[2*i]   = digits[uuid[i] >> 4];
		s_hex[2*i+1] = digits[uuid[i] & 0x0F];
	}
	s_hex[BOARD_ID_HEX_LEN] = '\0';
	s_valid = true;
}

const char * BoardIdHex(void){
	return s_hex;
}

bool BoardIdValid(void){
	return s_valid;
}
