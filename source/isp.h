/*
 * isp.h
 *
 * Software entry into the LPC5514's ROM bootloader (USB HID ISP), so firmware can be
 * reloaded over the one USB cable without the bootload jumper.
 */

#ifndef ISP_H_
#define ISP_H_

/* Hand the chip to the ROM bootloader. Does not return. The board leaves the bus as a
 * CDC port and re-enumerates as the ROM's HID device (VID 0x1FC9, PID 0x0022); blhost
 * can then flash it, and `blhost reset` or a power cycle boots the application again. */
void IspEnterRomBootloader(void) __attribute__((noreturn));

#endif /* ISP_H_ */
