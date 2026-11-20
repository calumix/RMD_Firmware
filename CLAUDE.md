# RMD Firmware

> **Part of the SPARC workspace.** The project-wide map — all repos, hardware fleet, and data flows — lives in `~/SPARC/CLAUDE.md`. Read it for cross-repo context; this file is RMD-Firmware-specific.

Firmware for the RMD (Resistance Measurement) board — the LPC5514 board whose `$POEI` stream `SPARC_CS_CLIENT` acquires from. How to build it (MCUXpresso, the NXP LPC5514 SDK, mainline FreeRTOS as a git submodule) is in `README.md`; the board and firmware docs live in the sibling `SPARC_CS_DOCUMENTATION` repo, and the `$POEI` wire format in `SPARC_CS_DATA_MODELS`'s `poei_protocol`.

⚠ **A fault in code that runs at boot leaves a board with no USB at all, and then only the JP1 jumper recovers it** (it cannot be sent `$POEI,ISP,ENTER`). It happened on 2026-11-19 with a call into the chip's ROM flash API. Before changing anything that runs at boot, a command or a reply, the version, the USB code or the LED code, read `Claude/long-term-memory/POEI-COMMANDS-AND-VERSIONS.md`. Try a new build on one board first, never on the last board that still answers, and ask what is plugged into a board's jacks before a test switches a channel on.

FLEET's Dev → Release Plans → **RMD Firmware** tab (release-plan pipeline id `rmdfirmware`) opens its Claude drawer in this repo, so the release plans for firmware work live in that tab and their hub dev plans are written from here.

## Claude memory & fleet-link (`Claude/`)

This repo carries the SPARC-standard `Claude/` tree; the convention — layout, the skills that manage it, the Fleet Link send protocol — is defined once in the workspace map (`~/SPARC/CLAUDE.md` → *Claude memory & SPARC Fleet Link*). Locally: `Claude/long-term-memory/` is the tracked deep-dive documentation folder, indexed by its `README.md`; `Claude/working-memory/` (gitignored, device-local) holds this device's notes on the active dev effort (`working-memory.md`, whose line 1 `Dev plan ref:` names its hub dev plan) and the one-sentence `current-goal.md`; `Claude/sparc-fleet-link/inbox.md` (gitignored) is where `/read-inbox` drains the mail that Claude instances on other devices left for this repo on the fleet hub (or, during a hub outage, where they `scp` it directly) — if it exists with no `Read @` stamp in its first three lines, tell the user and offer `/read-inbox`.
