/*
 * blink.h
 *
 * $POEI,BLINK: flash the board's LEDs for a few seconds, so a person holding
 * several boards can see which one SPARC CS is talking to.
 */

#ifndef BLINK_H_
#define BLINK_H_

enum blink_result {
	BLINK_OK = 0,
	BLINK_ERR_NOLED,	/* the board has no LED it can flash (in 2.0.0: the blink task could not be made) */
	BLINK_ERR_BUSY,		/* a channel is measuring and this build will not flash while it does */
};

/* Creates the blink task. Call once, after ChannelInit and after every other
 * task has been created (blink.c says why it goes last). */
void BlinkInit(void);

/* Start the flashing, or start its time again if it is already running. Returns
 * at once; the flashing runs in its own task, so commands are still answered
 * while it lasts. */
enum blink_result BlinkStart(void);

#endif /* BLINK_H_ */
