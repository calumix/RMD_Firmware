# Long-term memory — index

Each tracked deep-dive doc for this repo lives in this folder, named for the artefact it describes, and gets one row in this index saying when to read it.

| Read this | Before you |
|---|---|
| `POEI-COMMANDS-AND-VERSIONS.md` | change a command, a reply, the version, anything that runs at boot, the USB code or the LED code, or test a board on firmware `2.0.0` or later. It lists every command and reply, how a command's checksum is read (and the order that must not change), what the three version numbers promise SPARC CS, where the board's ID and sticker number come from, how Blink and the USB link behave, what firmware before `2.0.0` does differently, and the bench rules that came out of hanging a board at boot. |
| `BOARD-CAN-AND-POWER-HARDWARE.md` | plan anything that puts boards on a CAN bus, powers a board without its USB cable, or wires several boards together. It lists what is fitted on a 2026-batch board (the power-and-CAN connector, the transceiver, the termination jumper, the supply range), what is still unknown (the connector's pin order), and what the firmware already does with the CAN controller. |
