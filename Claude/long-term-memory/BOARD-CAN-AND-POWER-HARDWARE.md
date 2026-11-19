# The RMD board's CAN, power and USB hardware, as fitted

What is physically on a 2026-batch RMD board for CAN, external power and USB, and what the firmware already does with the CAN controller. Read this before planning anything that puts boards on a CAN bus, powers a board without its USB cable, or wires several boards together.

**Source and limits.** Read on 2026-11-10 from two photos of one 2026-batch board (silkscreen and fitted parts), and from this repo at commit `e27e105`. Nothing here was tested electrically. The Omnitech manual says only that CAN and external power are "included but not tested" and documents none of these parts; the schematic (`2733-002-01-SCH`) is a separate Omnitech deliverable that Calumix did not hold at the time. The 2026 manufacturer has substituted at least one part on this batch (the I/O expander), so check another board before assuming it matches.

## What is on the board

| Reference | Printed on the board | What it is | Fitted on the board photographed |
|---|---|---|---|
| J3 | PWR/CAN In | Green four-position pluggable screw-terminal block at the board edge. Carries the supply and the CAN bus together. | Yes, with its screw plug |
| J5 | PWR/CAN Out | The matching "out" connector, a row of four holes just above J3. | No |
| U3 | (between test points TD, TP9, and RD, TP10) | 8-pin chip in the CAN transceiver position: it turns the MCU's logic signals into the two bus wires. D4, a small protection part, sits between it and the connector holes. | Yes. Part marking not legible |
| R14 with JP2 | 1200 | A 120 Ω bus termination resistor beside a two-pin jumper. Fitting a jumper on JP2 switches the termination in. | Resistor yes; no jumper on JP2 |
| J2 | 9-24VDC | Barrel-jack footprint for the external supply. | No, so an external supply comes in through J3 |
| J4 | (none read) | Micro-USB socket. | Yes |
| Test points | VSYS, 5V25, 3V3, USB VBUS | An on-board regulator chain fed from either USB or the external supply. | Yes |

The firmware has a `USB_POWERED` sense pin (PIO0_26 in `board/pin_mux.c`), and the USB descriptor declares the board self-powered with 100 mA (`source/usb_device_descriptor.c`, `source/usb_device_descriptor.h`).

## What is NOT known

1. **The order of the four pins on J3 and J5.** Nothing printed on the board says which terminal is supply, ground, CAN-H or CAN-L. **Do not apply power to J3 until it is known.**
2. Whether every board carries J3 with its plug, U3 and R14.
3. The transceiver's part number, which decides whether CAN FD is possible.
4. One board's current draw, which sizes a shared supply.

**Finding the pin order with a multimeter** (board unpowered, USB unplugged): ground is the terminal with continuity to any post printed GND. For the bus pair, fit a jumper on JP2; the two terminals that read about 120 Ω between them are CAN-H and CAN-L. The remaining terminal is the supply, and it should show a connection to diode D2 directly behind J3. Which of the pair is H and which is L does not matter between boards wired terminal to terminal; at a PC adapter, swapping the two wires is harmless and fixes a silent bus.

## What it means for wiring

- One four-wire cable (supply, ground, the two bus wires) can run from board to board, so a board on the bus needs no USB cable. There is no special CAN cable.
- With J5 unfitted, the bus continues by putting two wires under each screw of J3, or by soldering a connector into J5.
- A jumper goes on JP2 on the two boards at the ends of the chain and on no others, unless the PC adapter sits at one end and has its own 120 Ω.

## What the firmware already does with CAN

- **The CAN controller is switched on at every boot** and nothing uses it. `board/peripherals.c` (the `CAN0_config` block and `CAN0_init`) sets up CAN0 at 125 kbit/s with classic 8-byte frames, transmit on PIO1_2 and receive on PIO1_3 (`board/pin_mux.c`). NXP's driver `drivers/fsl_mcan.c` is compiled in. The two CAN interrupts are configured but disabled, and no code sends or receives a frame.
- **Its clock comes from the 16 MHz crystal** through the PLL (`board/clock_config.c`, `BOARD_BootClockPLL150M`), so bit timing is accurate. (The FreeRTOS tick does not: it runs from the internal 32.768 kHz oscillator, `configSYSTICK_CLOCK_HZ` in `source/FreeRTOSConfig.h`, which is why a board's millisecond timestamps drift.)
- **The CAN message memory is tiny and hand-placed.** `CAN0_MESSAGE_RAM_SIZE` is 9 words (`board/peripherals.h`): one received frame and one frame to send. The buffer is declared in `source/main.c` and pinned to the SRAMX memory bank with 64 KB alignment, because the generated placement landed outside RAM; the comment there explains it. Any CAN work must grow this memory and keep that placement.
- **`board/peripherals.c` and `board/pin_mux.c` are generated** from `2733-005-00_Channel.mex` by MCUXpresso ConfigTools. A hand edit must also be made in the `.mex`, or the next regeneration loses it.

## Related reading

- `SPARC_CS_DOCUMENTATION/CSP_RMD_CAN_FOUNDATIONS.md`: a from-zero CAN primer written for Ryan. Its open questions about the connector, the transceiver, termination and external power are the ones the table above answers.
- `SPARC_CS_DOCUMENTATION/firmware_upgrade_proposal.md`: a proposed CAN frame layout, node IDs and build order.
- `SPARC_CS_DOCUMENTATION/RMD_FIRMWARE_FLASHING.md`: board names and chip IDs, and how to flash.
