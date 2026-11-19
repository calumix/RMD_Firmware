/*
 * board_id.h
 *
 * The board's permanent ID: the LPC5514's factory-programmed 128-bit unique ID,
 * read once at boot. It is the USB serial number and the first field of the
 * $POEI,ID reply.
 */

#ifndef BOARD_ID_H_
#define BOARD_ID_H_

#include <stdbool.h>
#include <stdint.h>

#define BOARD_ID_BYTES		16
#define BOARD_ID_HEX_LEN	(2 * BOARD_ID_BYTES)

/* Read the ID from the chip. Call once, before USB starts. A board whose ID
 * reads as blank boots with an ID of all zeros. */
void BoardIdInit(void);

/* The ID as 32 hexadecimal digits in lower case, in the order `blhost
 * get-property 18` prints them. 32 zeros when the ID could not be read. */
const char * BoardIdHex(void);

/* false when the ID could not be read (BoardIdHex() is then all zeros). */
bool BoardIdValid(void);

#endif /* BOARD_ID_H_ */
