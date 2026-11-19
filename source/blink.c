/*
 * blink.c
 *
 * The flashing runs in a task of its own so that the command task keeps
 * answering during it. It drives two kinds of LED:
 *
 *  - each channel's green and red LED, on two pins of that channel's IO
 *    expander, through ChannelSetLeds, which changes them in one
 *    read-modify-write under the expander lock. The measuring task only reads
 *    the expanders, so a blink cannot change a range, a polarity or a relay;
 *  - D1, the board's own red LED, on a processor pin (LED_ONn, PIO0_8). No
 *    firmware drove that pin before 2.0.0; what it does was found on the bench
 *    on 2026-11-19 by toggling it and looking. It shares nothing with the
 *    channels, so it flashes even on a board whose expanders do not answer.
 *
 * Measured on the bench the same day: a known resistor reads the same with the
 * LEDs flashing as without (1 kOhm on the 10 uA range and 10 kOhm on the 1 uA
 * range, 414 readings each way, medians within 4 parts in a million and the
 * same spread). So a blink is allowed at any time.
 */
#include <stdbool.h>
#include "FreeRTOS.h"
#include "task.h"
#include "pin_mux.h"
#include "fsl_gpio.h"
#include "blink.h"
#include "channel.h"

/* How long the LEDs flash after a $POEI,BLINK. A second BLINK inside this time
 * starts it again. */
#define BLINK_DURATION_MS	5000
/* One step of the pattern. Two steps make one flash, so this is 2 flashes a second. */
#define BLINK_STEP_MS		250

/* Set to 1 to refuse BLINK while any channel is on (answered BLINK,ERR,BUSY).
 * 0 allows it at any time. Which one a release carries is decided on the bench:
 * a known resistor is read with and without the flashing, and the flashing is
 * refused during a measurement only if it shows in the readings. It does not
 * (the measurement is at the top of this file), so this is 0. */
#define BLINK_REFUSE_WHILE_MEASURING	0

/* D1 is lit by driving its pin LOW; the pin is high from boot, D1 dark. */
static void BoardLed(bool lit){
	GPIO_PinWrite(BOARD_INITPINS_LED_ONn_GPIO,BOARD_INITPINS_LED_ONn_PORT,BOARD_INITPINS_LED_ONn_PIN,lit ? 0U : 1U);
}

static TaskHandle_t s_task = NULL;
/* The tick at which the flashing ends. Written by the command task, read by the
 * blink task; a single 32-bit store, so no lock. */
static volatile TickType_t s_until = 0;

static bool _time_left(void){
	/* Signed difference, so the tick counter may wrap. */
	return (int32_t)(s_until - xTaskGetTickCount()) > 0;
}

static void BlinkTask(void * params){
	bool step;
	int i;

	while(1){
		ulTaskNotifyTake(pdTRUE,portMAX_DELAY);

		/* Every channel's red and green in turn, and D1 lit with the reds. A
		 * channel with no expander is skipped (ChannelSetLeds refuses it), and
		 * the rest of the board still flashes. */
		step = false;
		while(_time_left()){
			for(i=0;i<NUM_CHANNELS;i++){
				ChannelSetLeds(i,step,!step);
			}
			BoardLed(!step);
			step = !step;
			vTaskDelay(pdMS_TO_TICKS(BLINK_STEP_MS));
		}

		/* Back to how they were: the LEDs have one resting state (green lit on
		 * every channel that works, the reds and D1 dark), and nothing else
		 * ever changes them. */
		BoardLed(false);
		ChannelLedsRest();
	}
}

void BlinkInit(void){
	/* The command task's priority: a blink waits its turn like a command does,
	 * and both run ahead of the measuring tasks only while they have work.
	 * Not asserted: this task is the least important one on the board, and it is
	 * created last, so that if memory were ever short it is Blink that goes
	 * missing (BLINK then answers ERR,NOLED) and never USB or a measurement. */
	if(xTaskCreate(BlinkTask,"BLINK",256,NULL,1,&s_task) != pdPASS){
		s_task = NULL;
	}
}

enum blink_result BlinkStart(void){
	/* D1 is on every board, so "no LED" now only means that the task that
	 * would flash it does not exist. */
	if(s_task == NULL) return BLINK_ERR_NOLED;
#if BLINK_REFUSE_WHILE_MEASURING
	if(ChannelAnyOn()) return BLINK_ERR_BUSY;
#endif
	s_until = xTaskGetTickCount() + pdMS_TO_TICKS(BLINK_DURATION_MS);
	xTaskNotifyGive(s_task);
	return BLINK_OK;
}
