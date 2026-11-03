# SPARC Fleet Link — the inbox

This folder is the mailbox for **SPARC Fleet Link**: one Claude instance handing a message or instructions to the Claude working this repo on another device. The full convention — including the send protocol — lives in the workspace map: `~/SPARC/CLAUDE.md` → *Claude memory & SPARC Fleet Link*.

- `inbox.md` (gitignored, device-local) is the incoming message. It is delivered by `scp` over Tailscale from another device, landing here and overwriting the previous message, and is read on this device with the `/read-inbox` skill, which stamps `Read @ <date>` on the first line as a read receipt.
- A sender must **check the read stamp before overwriting**: an existing `inbox.md` whose first lines carry no `Read @` stamp is UNREAD mail — overwriting it loses a message. The send protocol in the workspace map runs this pre-flight.
- This `readme.md` is tracked so the folder exists on every device's clone (git cannot track an empty directory) — `scp` therefore always has a destination and never needs a remote `mkdir`.

If you are a Claude instance and `inbox.md` is present without a read stamp: tell the user you have unread mail and offer to run `/read-inbox`.
