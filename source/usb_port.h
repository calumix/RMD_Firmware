/*
 * usb.h
 *
 *  Created on: Feb. 7, 2025
 *      Author: slocke
 */

#ifndef USB_PORT_H_
#define USB_PORT_H_

#include "FreeRTOS.h"
#include "stream_buffer.h"

void USBInit(StreamBufferHandle_t * usb_tx_stream, StreamBufferHandle_t * usb_rx_stream);

/* Drop off the bus (controller disabled, pull-up released). Register-level only, no kernel
 * calls, so it is safe with interrupts already torn down -- see IspEnterRomBootloader. */
void USBStop(void);


#endif /* USB_PORT_H_ */
