# Releasing a firmware version

How a build of this firmware becomes a released version: what a release consists of, the order that worked, and what proves that the image on a board is the released one. Written 2026-11-20, the day `2.0.0` was released. That release is the worked example throughout.

What the three version numbers promise SPARC CS is in `POEI-COMMANDS-AND-VERSIONS.md` beside this file. How to flash a board is `RMD_FIRMWARE_FLASHING.md` in the `SPARC_CS_DOCUMENTATION` repo, whose `tools/rmd_flashing/` folder holds every script named here.

## What a release consists of

- **An annotated tag `vX.Y.Z` on the commit the image was built from.** `v2.0.0` is on `3a1ee25`. Not on a later commit that only changes documents, and not on the merge commit.
- **The image, committed in `SPARC_CS_DOCUMENTATION/tools/rmd_flashing/fw/`** as `rmd-fw-X.Y.Z.hex` and `rmd-fw-X.Y.Z.bin`, with both SHA-256 sums in `SHA256SUMS.txt` there. This repo ignores its build output, so that folder is the only place an image is kept.
- **The work branch merged into `main` with a merge commit** in this repo (`v1.0.1` and `2.0.0` were both merged that way). The documentation repo is fast-forwarded.
- **The version itself** is `FW_VERSION` in `source/version.h`.

## The order that worked

1. **Build the candidate from a clean, committed tree** on a work branch, with `build_fw.ps1`, and make the flat binary with `hex2bin.py`. Record both SHA-256 sums. A second build of the same tree gives the same bytes, which is the way to prove a build. The build flashes a "Project SDK management" popup on the desktop for a few seconds; the script closes it, and nobody should click it.
2. **Flash the candidate onto one bench board first,** then the next (`flash_board.py`). The flash reads the image back and compares its SHA-256 with the flat binary's. That sum is what ties a board to an image from then on.
3. **Run the bench checks on the candidate:** `poei_v2_check.py` on each board (69 checks), `poei_query.py`, `poei_ch_checkpoint.py`, a known resistor against its last reading, `poei_multi_board_stream.py` for ten minutes with two boards, and `poei_link_drop_test.py` with a person pulling the hub's cable. Check that a board's sticker number is still there after a re-flash and after its cable is unplugged.
4. **Let SPARC CS check the wire before any tag.** The Claude in `SPARC_CS_CLIENT` sends every command SPARC CS sends, built by SPARC CS's own code, to the real boards and reads every reply with its own readers. Ask it by SPARC Fleet Link, with each board's ID and port. If it finds a fault: fix, rebuild, re-flash, and repeat the checks the fix touches. The version number stays the same, because no board has left the bench.
5. **Release the candidate as it is. Do not build again.** Give the candidate's two files their release names, compare their sums with the recorded ones, add them to `SHA256SUMS.txt`, and commit. Convert the hex with `hex2bin.py` once more: it must give the flat binary's sum, which proves the two files are one image. Then tag, merge and push, at Ryan's word.
6. **Update the documents in the same sitting:** the image section and the record in `RMD_FIRMWARE_FLASHING.md`, and the note at the top of `CSP_RMD_FIRMWARE_CONTROL_NOTES.md`. If a reply changed, `POEI-COMMANDS-AND-VERSIONS.md` changes in the same commit as the code.

A board that was flashed with the candidate needs no second flash at the release: its read-back already matched the released file's sum.

## Rules that came out of it

- **A board leaves the bench only on a tagged image.** A board on a test build stays on the bench.
- **Every commit, tag and push waits for Ryan's word.**
- **The bench PC can be shared with SPARC CS and its Claude.** One program holds a board's port at a time. Check that no SPARC CS is running, and say so by Fleet Link before a script or a flash opens a port.
- **Ask what is on a board's jacks before a check switches a channel on.** A bench board can carry real sensors.
- **A fix that changes no message is `X.Y.(Z+1)`,** and needs no SPARC CS update.

## Traps paid for

- **Git must store the images byte for byte.** `fw/.gitattributes` in the documentation repo says so (`*.hex -text`, `*.bin binary`). Before it existed, git on Windows stored a hex with its line endings converted: the stored file's SHA-256 was not the recorded one, and a clone on a Mac failed the check the flashing guide asks for. A new kind of image file needs a line there.
- **Only firmware `2.0.0` or later can write a board's sticker number.** The number sits on the EEPROM, which a flash does not touch. So a board that must run older firmware is flashed with a `2.y.z` image first (`flash_board.py --rom --number N`) and then with the older one: it keeps its number and reports it again when it returns to `2.y.z`. RMD #3 was set up that way on 2026-11-20.
- **Older images stay in `fw/`.** SPARC CS must refuse a board on `v1.0.1`, and testing that needs such a board. A board moves between images without hands, because `v1.0.1` and later enter the bootloader on command.
- **A board takes a new COM number when it crosses `2.0.0` in either direction.** From `2.0.0` Windows keys the number on the board's ID; before it, on the hub socket.
- **Read the clock before writing a time into a record.** On release day two times were written from memory and were half an hour wrong.
