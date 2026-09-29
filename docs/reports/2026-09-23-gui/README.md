---
title: "PkgManager, first increment: search, install, remove, cancel, without a Shell"
date: 2026-09-23
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG
binary: PkgManager 90cb35d9d0bbeaa0 (sha256 prefix), ABIv11, src/build-gui.sh
driver: tests/gui-accept.sh (host side, clicks by coordinate), tests/gui-launch.script (guest side)
---

# PkgManager on AROS One

The acceptance from `docs/zune-frontend-brief.md`, run as written: a user
searches for a package, installs it and removes it without a Shell; the window
stays responsive; a cancelled download leaves nothing behind; the Cancel button
is disabled from the extract phase on.

The window was driven from the host (`tools/vmctl.py` clicks at coordinates
read off a screenshot of the fixed first placement). **The evidence is the
window's own event log** (`--log`, a test aid in the spirit of `apkg
--progress`) plus what `apkg --json list` says about the registry afterwards,
both sent off the guest by `putfile`. The screenshots in `docs/spikes/gui/`
illustrate and prove nothing.

## What the log shows

`gui-events.log`, condensed (the 22 and 31 download events are elided):

```
update pressed
done update  status=0
install pressed for soliton
event soliton download 0/223945     can_cancel=1 cancel_button=enabled
  … 22 download events, all can_cancel=1 …
event soliton verify   1/1          can_cancel=1 cancel_button=enabled
event soliton extract  0/223945     can_cancel=0 cancel_button=disabled
event soliton extract  1/1          can_cancel=0 cancel_button=disabled
event soliton publish  0/1          can_cancel=0 cancel_button=disabled
event soliton publish  1/1          can_cancel=0 cancel_button=disabled
done install soliton status=0
remove pressed for soliton
done remove soliton status=0
install pressed for xrick
event xrick download 0/2043720      can_cancel=1 cancel_button=enabled
  … 31 download events, last at 296960/2043720 …
cancel pressed for xrick
done install xrick status=14 download cancelled
quit
```

| acceptance point | evidence |
|---|---|
| index fetched from the window | `done update status=0`; the list then shows 23 packages (`g1`) |
| install without a Shell | `done install soliton status=0`; `gui-list1.txt` (from `apkg --json list`, run in the Shell only to *read* the registry) has `soliton … installed: true` |
| remove without a Shell | `done remove soliton status=0`; `gui-list2.txt` is `{ "rows": [], "hidden": 0 }` |
| Cancel follows `can_cancel`, not the phase name | `cancel_button=enabled` on every event with `can_cancel=1`, `disabled` on every event with `can_cancel=0`, and the switch is at `extract` |
| a cancelled download leaves nothing | Cancel pressed at 296 960 of 2 043 720 bytes → `status=14 download cancelled`; `gui-cache.txt` lists only `soliton.zip`, no `xrick` and no `.part`; `gui-list2.txt` shows nothing installed |
| the window stays responsive | the Cancel click was delivered and acted on mid-download, on the same task the window runs on; the worker was a separate process (`CreateNewProc`) the whole time |
| clean exit | `quit` after the close gadget; the log was closed by the program |

## Two runs

`superseded/*-binary-a63a53a0.*` is the first full run, identical in every
outcome. The binary was rebuilt for two things a screenshot showed: an
error's detail line was on a second line the one-line status object does not
display (now ` -- ` on one line), and the log said "done installed xrick
status=14": the word chosen before the outcome was known. Nothing in the
worker, the contract or the event handling changed between the two.

## What the earlier, unlogged run found

Before `--log` existed, a first run of the same window (screenshots
`s2`–`s9`, not kept) found: the worker sent to a message port the window
did not wait on (a second `CreateMsgPort` in `start()`, fixed before any
click); the list lost its selection on refill so Install came back enabled
with nothing selected (the id is re-selected now); and the outcome message
was overwritten by the refill's package count (the outcome is written after
the refill now). All three were visible in one screenshot each and none
would have been found by reading.

## Measured, and worth keeping

- **A `CreateNewProc` worker running `libpkg` works on AROS One 1.3**: TLS
  downloads through `bsdsocket.library`, file I/O into `SYS:Packages`, and
  the C library from a second process of the same program. This was the
  open question in the brief's execution contract; it is now a measurement.
- Downloads from AROS Archives complete in under two seconds for anything in
  the index on this host, so a cancel test has to click within the same
  second as Install. The 1 s delay of the first attempt landed after
  `verify` and was correctly ignored, which is itself the contract working.
