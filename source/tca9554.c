/*
 * tca9554.c
 *
 *  Created on: Mar. 31, 2026
 *      Author: slocke
 */
#include "tca9554.h"
#include "peripherals.h"
#include "FreeRTOS.h"
#include "semphr.h"

#define CTRL_REG_INPUT_PORT		0x00
#define CTRL_REG_OUTPUT_PORT	0x01
#define CTRL_REG_POLARITY		0x02
#define CTRL_REG_DIRECTION		0x03

static int tca9554_write_reg(struct tca9554 * dev, uint8_t reg, uint8_t val){
	int status;
	i2c_master_transfer_t masterXfer;

    masterXfer.slaveAddress   = dev->address;
    masterXfer.direction      = kI2C_Write;
    masterXfer.subaddress     = reg;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = &val;
    masterXfer.dataSize       = 1;
    masterXfer.flags          = kI2C_TransferDefaultFlag;

    status = I2C_RTOS_Transfer(dev->i2c_dev, &masterXfer);
    if (status != kStatus_Success){
        return 0;
    }
    return 1;
}

static int tca9554_read_reg(struct tca9554 * dev, uint8_t reg, uint8_t * val){
	int status;
	i2c_master_transfer_t masterXfer;

    masterXfer.slaveAddress   = dev->address;
    masterXfer.direction      = kI2C_Read;
    masterXfer.subaddress     = reg;
    masterXfer.subaddressSize = 1;
    masterXfer.data           = val;
    masterXfer.dataSize       = 1;
    masterXfer.flags          = kI2C_TransferDefaultFlag;

    status = I2C_RTOS_Transfer(dev->i2c_dev, &masterXfer);
    if (status != kStatus_Success){
        return 0;
    }
    return 1;
}

/* One lock for every change to an expander's output register. A pin is changed
 * by reading all eight and writing them back, as two separate I2C transfers, and
 * since 2.0.0 two tasks do that: the command task (relay, polarity, range) and
 * the blink task (the two LED pins of the same chip). Without the lock a blink
 * landing between a CH command's read and its write would put back an old relay,
 * polarity or range pin. One lock for all four chips is enough: the changes are
 * rare and short. */
static SemaphoreHandle_t s_output_lock = NULL;

void tca9554_init(void){
	if(s_output_lock == NULL){
		s_output_lock = xSemaphoreCreateMutex();
		configASSERT(s_output_lock != NULL);
	}
}

/* Read-modify-write of the output register, under the lock: the pins in
 * set_mask go high, then the pins in clear_mask go low, in one write. If the
 * read-back fails there is nothing to modify: writing back an uninitialised
 * byte is how an expander that never answered went unnoticed (the board
 * streamed numbers whose TIA gain and polarity it had never actually set). */
int tca9554_update(struct tca9554 * dev, uint8_t set_mask, uint8_t clear_mask){
	uint8_t current_set;
	int ok = 0;

	xSemaphoreTake(s_output_lock,portMAX_DELAY);
	if(tca9554_read_reg(dev,CTRL_REG_OUTPUT_PORT,&current_set)){
		current_set |= set_mask;
		current_set &= ~clear_mask;
		ok = tca9554_write_reg(dev,CTRL_REG_OUTPUT_PORT,current_set);
	}
	xSemaphoreGive(s_output_lock);
	return ok;
}

int tca9554_set(struct tca9554 * dev, uint8_t mask){
	return tca9554_update(dev,mask,0);
}

int tca9554_clear(struct tca9554 * dev, uint8_t mask){
	return tca9554_update(dev,0,mask);
}

/* Does anything ACK at dev->address? A read of the input port is harmless on a
 * TCA9554 / TCA9554A and changes no pin state. Used by ChannelInit to find out
 * which variant the assembler fitted (0x20+n vs 0x38+n). */
int tca9554_probe(struct tca9554 * dev){
	uint8_t io;
	return tca9554_read_reg(dev,CTRL_REG_INPUT_PORT,&io);
}


int tca9554_read(struct tca9554 * dev, uint8_t * io){
    return tca9554_read_reg(dev,CTRL_REG_INPUT_PORT, io);
}

/* A write of all eight output pins at once. Under the same lock: landing between
 * another task's read and its write, it would be undone by that write. */
int tca9554_write(struct tca9554 * dev, uint8_t io){
	int ok;
	xSemaphoreTake(s_output_lock,portMAX_DELAY);
	ok = tca9554_write_reg(dev,CTRL_REG_OUTPUT_PORT, io);
	xSemaphoreGive(s_output_lock);
	return ok;
}

int tca9554_direction(struct tca9554 * dev, uint8_t direction){
	return tca9554_write_reg(dev,CTRL_REG_DIRECTION, direction);
}


