# The `$POEI` commands a board accepts, and what its version number means

What firmware `2.0.0` and later answers over its USB serial port, how a command is read, what the three version numbers promise, and the things in this firmware that were paid for on the bench and must not be undone. Written 2026-11-19, when `2.0.0` was built and checked on RMD #1 and RMD #2.

The other half of this contract is SPARC CS's parser and encoder, `poei_protocol` in `SPARC_CS_DATA_MODELS`. A change to a line below is a change in two repos.

## A line

- A line starts with `$`, ends with CR or LF, and is at most 128 characters between them. Fields are separated by commas. The first field is the talker, `POEI`; a line for another talker is ignored without a reply.
- Every line a board sends ends with `*XX` before the line ending: two hexadecimal digits in capitals, the XOR of every byte after the `$` and before the `*`.
- **A command may carry the same checksum, and if it does, it is checked.** The sum covers the bytes exactly as sent: a lower-case letter counts as lower case. The two digits may be in either case. If a command holds a `*` at all, the text after its last `*` must be exactly two hexadecimal digits and the end of the command; anything else is a wrong checksum. A wrong checksum is answered `$POEI,ERROR,BAD CHECKSUM` and the command is not run.
- A command with no checksum is accepted. Bench scripts and a person at a terminal send none.
- After the checksum is checked and cut off, the whole command is turned to upper case, so `$poei,ver` works, and `ECHO` echoes in capitals.
- ⚠ **The order matters and must stay: checksum first, upper case second.** SPARC CS sends `on`, `off` and its numbers in lower case (`1e-05`) and sums what it sends. `POEI,CH,1,off,0.0,10` sums to `45` as sent and to `65` in capitals. A firmware that upper-cased first would refuse every `CH … off`, and a bench test with `on` alone would not show it, because `on` sums the same both ways. `source/message.c` stores a command as it arrived for this reason.
- Command names are matched whole. `VERSION` is not `VER`.

## Commands

`<…>` is a value. Replies are shown without `$POEI,` in front and `*XX` behind.

| Command | Reply | Notes |
|---|---|---|
| `VER` | `VER,2.0.0` | The version, exactly `X.Y.Z` (below). |
| `ID` | `ID,<id>,<version>,<number>` | `<id>`: the board's permanent ID, 32 hexadecimal digits in lower case; 32 zeros if the chip's ID read as blank. `<version>`: as `VER`. `<number>`: the sticker number, 0 to 9999, or `NONE`. More fields may be added at the end later. |
| `BOARD` | `BOARD,<number>` or `BOARD,NONE` | Reads the number from the EEPROM again. |
| `BOARD,SET,<n>` | `BOARD,<n>`, or `BOARD,ERR,STORE`, or `BOARD,?` | Writes the number and answers from what was read back. `<n>` is one to four digits. `ERR,STORE`: the chip did not keep it. `?`: not a number from 0 to 9999, or any other form of the command. 0 is a real board, never "none". |
| `CH,<ch>` | `CH,<ch>,<on\|off>,<bias>,<range>` | Reads channel 1 to 4. |
| `CH,<ch>,<on\|off>,<bias V>,<range µA>` | the same | Sets it. `on` and `off` are whole words. The reply's bias is read back from the bias chip, printed with six decimals, and can differ a little from the number sent. The reply's range is read back from the IO expander. **A range of 0 in the reply** says the channel's IO expander did not answer: the channel is held off whatever was asked. `off` still sets the bias and range it is given. |
| `IOEXP` | `IOEXP,<a1>,<a2>,<a3>,<a4>` | The I2C address of each channel's IO expander (`0x38`… for the TCA9554A, `0x20`… for the TCA9554), or `NONE`. |
| `BLINK` | `BLINK,OK` | Answered at once; the LEDs then flash for 5 seconds (below). `BLINK,ERR,NOLED` and `BLINK,ERR,BUSY` are part of the protocol and SPARC CS handles them, but `2.0.0` sends neither in practice. |
| `ECHO,<word>` | `ECHO,<WORD>` | For testing. |
| `ENV,SAVE` · `ENV,DEFAULT` · `ENV,LOAD` | `ENV,SAVE,OK` · `ENV,DEFAULT,OK` · `ENV,LOAD,OK` or `ENV,LOAD,DEF` | The small settings record on the EEPROM. Nothing in the firmware uses its one value (`UART_BAUD`). `DEF`: no valid record was stored. |
| `ENV,UART_BAUD` · `ENV,UART_BAUD,?` · `ENV,UART_BAUD,<baud>` | `ENV,UART_BAUD,<baud>` | Read, read, set. |
| `ENV` alone, or an unknown second word | `ENV,?` | |
| `ISP,ENTER` | `ISP,ENTER,OK`, then the port goes away | Hands the board to the chip's ROM bootloader for flashing (`RMD_FIRMWARE_FLASHING.md` in `SPARC_CS_DOCUMENTATION`). Anything else after `ISP` answers `ISP,?` and does nothing. |

What a board sends without being asked, and nothing else: for every channel that is on,

- `MEAS,<ch>,<board time ms>,<resistance Ω>,<voltage V>,<current A>`
- `RTD,<ch>,<board time ms>,<temperature °C>`

Errors, each only ever as the answer to a command:

- `ERROR,BAD CHECKSUM`: the command carried a wrong checksum.
- `ERROR,INVALID COMMAND,'<NAME>'`: no such command. `$POEI` alone answers `ERROR,INVALID COMMAND,''`.
- `ERROR,COMMAND PARSE ERROR`: the command exists and its fields are wrong (`CH` with a channel outside 1 to 4, a word that is neither `on` nor `off`, or a number of fields that is neither a read nor a set; `ECHO` with nothing to echo).

## What the version number promises

`FW_VERSION` in `source/version.h` is the one definition. `VER` and `ID` print it with `%s`: the field is those characters and nothing else, no quote marks, no spaces, no `v`. SPARC CS refuses a board whose version it cannot read as `X.Y.Z`.

- **X** changes only when a message SPARC CS relies on changes in a way an older SPARC CS cannot read. SPARC CS uses a board only when X is one it knows (2).
- **Y** changes when a command or a field is added that an older SPARC CS still works with.
- **Z** changes for a fix that changes no message.

Inside `2.y.z`: `MEAS` and `RTD` keep their fields exactly, in number and in order, because every installed SPARC CS matches those lines by their field count. No other reply changes its fields either, except `ID`, which may gain fields at its end. What Y may add is a new command with its own reply.

## Firmware before `2.0.0`, as SPARC CS and the bench scripts meet it

Tags `v1.0.0` (Omnitech's original) and `v1.0.1` (the build that detects both IO expander parts).

- `VER` answers `VER,TEST`. No streamed line carries a version, so a board's version is known only by asking.
- `ID`, `BOARD` and `BLINK` answer `ERROR,INVALID COMMAND,'<NAME>'`. When the command carried a checksum, that firmware repeats it inside the quotes, so the reply holds two `*`: `$POEI,ID*32` is answered `$POEI,ERROR,INVALID COMMAND,'ID*32'*77`.
- A command's checksum is never checked and never cut off: it stays stuck to the last field. So `ISP,ENTER` must be sent WITHOUT a checksum to `v1.0.1` (with one it answers `ISP,?`), and `ECHO,ab*11` comes back as `ECHO,AB*11`.
- Names match by prefix: `VERSION` runs `VER`.
- It publishes no USB serial number. A board flashed from it to `2.0.0` therefore takes a new COM number on Windows.
- `ENV,SAVE` answers `OK` and saves nothing usable (the EEPROM addressing fault, below).
- A board that keeps its power while its link to the PC drops never answers again until it is power cycled (below).

## The board's identity

- **The ID** is the LPC5514's factory-programmed 128-bit number, read at boot as plain memory from the flash's factory area: `FLASH_NMPA->UUID_ARRAY`, address `0x0003FC70` (`source/board_id.c`). Printed as its 16 bytes in stored order, which is the order `blhost get-property 18` prints and the flashing record lists. An ID of all zeros or all ones counts as blank.
- ⚠ **Never read it through the ROM's flash API at boot** (`FLASH_Init`, `FFR_Init`, `FFR_GetUUID` in `drivers/fsl_iap.c`). The first test build did, and the board never reached USB: no port, nothing for Windows to list. The firmware runs its core at 150 MHz, `FLASH_Init` tells the ROM the clock is 96 MHz, and the SDK's own comments put a 100 MHz ceiling on the ROM's flash operations. A board hung that way cannot be sent `ISP,ENTER`; it is recovered by bridging the bootload jumper JP1 and pressing RESET.
- **The USB serial number** is the same 32 digits (`source/usb_device_descriptor.c`, string 4, filled in by `USBInit`). Windows shows it in capitals and keys the COM number on it, so a board keeps its COM number in any socket and across a re-flash. A board with a blank ID publishes no serial number at all: two boards claiming the same one are mishandled by Windows.
- **The sticker number** is a six-byte record on the settings EEPROM at address `0x0040`, on a page of its own (`source/board_number.c`): `R`, `N`, the number low byte then high byte, then the same two bytes inverted. A blank chip or a half-written record reads `NONE`. Flashing firmware does not touch the EEPROM, so the number survives an update. The flashing procedure writes it; SPARC CS only reads it, from `ID`.
- ⚠ **The EEPROM (I2C `0x50`) takes a two-byte address.** `source/env.c` sent one byte until `2.0.0`, so a save landed somewhere else and a load read whatever the chip pointed at. Proved on a 2026-batch board: bytes written at `0x0100` and `0x0200` with two-byte addresses read back from those two addresses and nowhere else. The environment record is at address 0.

## Blink

- For 5 seconds, every channel's red and green LED light in turn (2 flashes a second) and D1, the board's own red LED, lights with the reds. Then the LEDs go back to their one resting state: green lit on every channel whose IO expander answers, the reds and D1 dark. A second `BLINK` inside the 5 seconds starts them again.
- D1 is on the processor pin `LED_ONn` (PIO0_8), lit by driving it LOW. No firmware drove that pin before `2.0.0`; what it does was found by toggling it and looking at a board.
- It runs in a task of its own (`source/blink.c`), created last and not essential: commands are answered during the flashing.
- ⚠ **Every change to an IO expander's output register takes one lock** (`source/tca9554.c`). A channel's two LEDs are pins of the same chip that drives that channel's relay, bias polarity and range, and a pin is changed by reading all eight and writing them back. Two tasks do that since `2.0.0`. Without the lock a blink landing between a `CH` command's read and its write would put back an old relay, polarity or range pin.
- **It does not change a reading.** Measured with a 1 kΩ resistor on the 10 µA range and a 10 kΩ on the 1 µA range, 414 readings with the flashing and 414 without, each way round: medians within 4 parts in a million and the same spread. (Voltage and current read about 20 parts in a million apart between the first and the second window after a channel comes on, whichever of the two is the flashing one: that is the bias still settling, not the LEDs.)

## The USB link

- With the port closed on the PC, a board waits: the line it is sending stays pending, every task backs up behind it, nothing is measured and no command is read. When a program opens the port, it carries on, and the first lines can be ones that were waiting. `v1.0.1` behaves the same.
- **When the PC resets the link** (a cable pulled between a hub with its own power supply and the PC, and put back: the board never loses power), the sending task is released and drops what it held, then drops everything until the PC has set the board up again (`USBLinkDown`, `USBLinkReady` in `source/usb_port.c`). Before `2.0.0` nothing released it: the send it waited for had been thrown away by the reset, and the board answered nothing until it was power cycled.
- A channel that was on stays on through all of this. The board cannot know the PC has gone, and a `CH … off` sent while the link is away is lost. Switching such channels off when the board returns is SPARC CS's job.
- The sending is held back by the "set up by a PC" flag (`attach`), not by clearing the "a program opened the port" flag: a PC that resets the link while a program keeps the port open gets its data back without opening the port again.

## Reply times

Measured on RMD #1 with all four channels streaming, 60 rounds, from the end of the PC's write to the reply line: `ID` and `BLINK` under 1 ms; a `CH` read 3 ms; a `CH` set that moves no relay 6 ms; a `CH` that switches a channel on or off 24 ms, because the relay is pulsed for 20 ms before the reply. A reader that waits a quarter of a second for a reply has ten times the margin.

## Checking a board

In `SPARC_CS_DOCUMENTATION/tools/rmd_flashing/`, each run as `uv run --with pyserial python -I <script>`:

- `poei_v2_check.py [COMn]`: the whole table above against one board, including SPARC CS's own encoder's bytes, wrong checksums and Blink. `--latency N` measures reply times; `--blink-noise S` measures a known resistor with and without the flashing.
- `poei_link_drop_test.py --serial <id>`: the pulled-hub-cable test. It needs a person to pull the cable.
- `poei_multi_board_stream.py --board <id>:<channels> …`: several boards streaming at once, every line checked.

Name a board by its ID (`--serial`), not by its COM number.

## Rules for the next change

- **Nothing new runs at boot until a bench command has shown it safe on a board.** A fault at boot means no USB, and no USB means the JP1 jumper.
- **A new build goes onto one board first,** never onto the last board that still answers.
- A board may carry real sensors on some jacks. Ask what is plugged in before a test switches a channel on.
