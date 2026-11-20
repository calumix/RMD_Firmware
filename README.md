# Firmware for Resistance Measurement board

## Building
The project is designed for MCUXpresso v24.12.148. Other versions will likely work, but may require project setting changes. MCUXpresso (as well as older versions) can be downloaded from [NXP](https://www.nxp.com/design/design-center/software/development-software/mcuxpresso-software-and-tools-/mcuxpresso-integrated-development-environment-ide:MCUXpresso-IDE)

An SDK for the LPC5514 is necessary for the project, and should be installed in MCUXpresso. The default SDK (from the [SDK Builder](https://mcuxpresso.nxp.com/) website) was used.

## FreeRTOS
The NXP SDK contains an older version of FreeRTOS, and is not used in this project. Mainline FreeRTOS is included as a git submodule to make updates easier. Clone the directory with `git clone --recurse-submodules <repository>` can be used to pull both this repository and the FreeRTOS code (needed to build the project). Alternatively, if you've already cloned this project, you can also do `git submodule update --init --recursive` to pull the FreeRTOS code.

## Version and commands
The firmware's version is `FW_VERSION` in `source/version.h`. A board reports it over its USB serial port: `$POEI,VER` answers `$POEI,VER,2.0.0`, and `$POEI,ID` answers the board's permanent ID, the same version and its sticker number. Builds before `2.0.0` (tags `v1.0.0` and `v1.0.1`) answer `$POEI,VER,TEST`.

The first number changes only when a message SPARC CS relies on changes in a way an older SPARC CS cannot read; the second when a command or a field is added; the third for a fix that changes no message.

Every command, every reply, how a command's checksum is checked, and what must not be changed without care are in `Claude/long-term-memory/POEI-COMMANDS-AND-VERSIONS.md`. How to flash a board and check it is in `RMD_FIRMWARE_FLASHING.md` in the `SPARC_CS_DOCUMENTATION` repository.
