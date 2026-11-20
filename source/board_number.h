/*
 * board_number.h
 *
 * The board's sticker number (RMD #1 is 1), kept on the settings EEPROM so that
 * it survives a firmware update. SPARC CS names a board from it; a board that
 * holds none is named from its ID instead.
 */

#ifndef BOARD_NUMBER_H_
#define BOARD_NUMBER_H_

#define BOARD_NUMBER_NONE	(-1)
#define BOARD_NUMBER_MAX	9999

/* Read the number from the EEPROM once. Call at boot, with the kernel running. */
void BoardNumberInit(void);

/* The number as last read from the chip: 0 to BOARD_NUMBER_MAX, or
 * BOARD_NUMBER_NONE. 0 is a real board (RMD #0), never "none". No I2C traffic. */
int BoardNumberGet(void);

/* Read it from the chip again (and remember it). */
int BoardNumberRead(void);

/* Write it, then read it back. Returns the number as read back, which equals
 * `number` only if the chip kept it. */
int BoardNumberSet(int number);

/* The number as text, for a reply: its digits, or NONE. `text` holds 8 bytes. */
const char * BoardNumberText(int number, char * text);

#endif /* BOARD_NUMBER_H_ */
