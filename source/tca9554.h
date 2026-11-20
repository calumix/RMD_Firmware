/*
 * tca9554.h
 *
 *  Created on: Mar. 31, 2026
 *      Author: slocke
 */

#ifndef TCA9554_H_
#define TCA9554_H_
#include <stdint.h>
#include "peripherals.h"

struct tca9554 {
	uint8_t address;
	i2c_rtos_handle_t * i2c_dev;
};

/* Every call returns 1 on success and 0 when the I2C transfer failed (typically
 * an address NAK: no expander at dev->address). Callers that need a value MUST
 * check the return before using it -- an unchecked read leaves *io untouched. */
int tca9554_probe(struct tca9554 * dev);
int tca9554_read(struct tca9554 * dev, uint8_t * io);
int tca9554_write(struct tca9554 * dev, uint8_t io);
int tca9554_direction(struct tca9554 * dev, uint8_t direction);
int tca9554_set(struct tca9554 * dev, uint8_t mask);
int tca9554_clear(struct tca9554 * dev, uint8_t mask);
/* Both at once, in one read-modify-write: set_mask pins high, clear_mask pins low. */
int tca9554_update(struct tca9554 * dev, uint8_t set_mask, uint8_t clear_mask);

/* Creates the lock that every output change takes (tca9554.c says why). Call it
 * once, with the kernel running, before any other call here. */
void tca9554_init(void);

#endif /* TCA9554_H_ */
