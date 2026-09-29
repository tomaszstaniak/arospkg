---
title: "Two more of our own programs in the catalogue: GrafX2 and Folio"
date: 2026-09-26
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG, persistent
binary_sha256: a513aea7d8b066911a88466ac83d166f9c30578fb80768b0961d14df426040fc
script: tests/new-packages.script
---

# GrafX2 2.9 (package revision 4) and Folio 0.3.8

Both are GitHub release assets of their own projects, checked against the
release before the manifests were written: GrafX2 `b037cb10…` (4 207 684
bytes, the same hash as the release's SHA256SUMS), Folio `e46f8c09…`
(29 363 297 bytes, the same as the project's local dist). Both archives carry
an embedded `.arospkg/manifest.toml`; the catalogue entries agree with it.
Folio's embedded manifest states no requirements; the entry adds the ABIv11
runtime (`crt`, `m`, `stdlib`) found in the binary. `openurl.library` is left
out on purpose: Folio opens it only when a link is clicked and says so if it
is missing.

**Result: 4 reports, 4 expectations met, 4 PASS, 0 FAIL, one binary.**

| step | GrafX2 | Folio |
|---|---|---|
| `requires` | crt, m, stdlib satisfied | the same |
| `install` over HTTPS through GitHub's redirect | verified and cached, installed with its drawer icon | the same |
| running 20 s after `Run` with an 8 MiB stack | process 10 | process 11 |
| `remove` | removed; the drawer stays with the settings GrafX2 wrote (`gfx2.ini`, `gfx2-sdl2.cfg`) and an empty `gfx2.lck` left by Ctrl-C, recorded in `db/doctor` | removed; nothing left |

`apkg` itself ran from a Shell at the plain 40960-byte stack throughout.

## The first attempt hung the machine, and that was the test's fault

Both programs document an 8 MiB stack (GrafX2's ReadMe: `stack 8388608`;
Folio's README: the icon carries 8 MB and a small stack crashes it). The first
attempt started GrafX2 with `Run` from the 40960-byte Shell, which hands the
program that stack. The whole machine hung at 100% CPU with the screen title
bar overwritten (`attempt1-grafx2-at-40960-hung.png`); `system_reset` had no
effect and QEMU had to be quit. The script now raises the stack for each `Run`
and puts it back for `apkg`. Starting from the drawer icon needs nothing.

The `grafx2` leftover is the known 0.3 item from the 0.2 regression (Untangle):
removal keeps files a program wrote into its own drawer, correctly, but does
not name them on the console or in the record.

Screenshots illustrate: `grafx2-running.png`, `folio-running.png` (Folio
showing the project page printed to PDF, reinstalled from the cache for the
picture).
