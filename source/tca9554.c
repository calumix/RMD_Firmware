/*
 * tca9554.c
 *
 *  Created on: Mar. 31, 2026
 *      Author: slocke
 */
#include "tca9554.h"
#include "peripherals.h"
#include "FreeRTOS.h"

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

/* Read-modify-write of the output register. If the read-back fails there is
 * nothing to modify: writing back an uninitialised byte is how an expander
 * that never answered went unnoticed (the board streamed numbers whose TIA
 * gain and polarity it had never actually set). */
int tca9554_set(struct tca9554 * dev, uint8_t mask){
	uint8_t current_set;
	if(!tca9554_read_reg(dev,CTRL_REG_OUTPUT_PORT,&current_set)) return 0;
	current_set |= mask;
	return tca9554_write_reg(dev,CTRL_REG_OUTPUT_PORT,current_set);
}

int tca9554_clear(struct tca9554 * dev, uint8_t mask){
	uint8_t current_set;
	if(!tca9554_read_reg(dev,CTRL_REG_OUTPUT_PORT,&current_set)) return 0;
	current_set &= ~mask;
	return tca9554_write_reg(dev,CTRL_REG_OUTPUT_PORT,current_set);
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

int tca9554_write(struct tca9554 * dev, uint8_t io){
    return tca9554_write_reg(dev,CTRL_REG_OUTPUT_PORT, io);
}

int tca9554_direction(struct tca9554 * dev, uint8_t direction){
	return tca9554_write_reg(dev,CTRL_REG_DIRECTION, direction);
}


