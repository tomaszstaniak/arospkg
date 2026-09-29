---
title: "Choosing twenty packages, and what 'tested' means"
status: complete
state: measured
created: 2026-09-19
updated: 2026-09-20
---

# Twenty packages for v0.1

The MVP's success criterion is twenty packages installing without error. The
user raised the bar on 2026-09-19: **a hash and a clean unpack do not qualify a
package.** Each one must install, start and be removed on the target.

## What was checked, in order

1. **The catalogue.** 171 ABIv11 candidates: 152 ZIP, 16 LHA, 3 tar. At the
   time of this selection the client read ZIP only, so the twenty are all ZIP.
   **LHA has been supported since 2026-09-20** and those 16 are now candidates
   like any other; tar is still out.
2. **Layout**, from the archive: one drawer, an icon beside it, nothing written
   to a system directory. Of 30 sampled ZIPs, 27 are applications in a single
   drawer and **none** writes to `LIBS:`, `FONTS:` or `C:`; three are developer
   SDK packages, which the MVP already excludes.
3. **Documentation**, because layout proves nothing about need. `tools/survey.py`
   reads every readme and guide in each archive for installation instructions.
   This is what found ZapHod's optional extra fonts for `FONTS:` and its German
   catalog for `LOCALE:`, both optional, both recorded in its manifest as not
   installed, and ZapHod runs without them.
4. **References**, from the binaries: every `.library`, `.device`, `.datatype`,
   `.mcc` and `.font` name, checked against AROS One 1.3's own Libs, Classes,
   Devs, Fonts and the kickstart's ROM modules. What AROS One ships and mainline
   does not becomes `requires_system`: for most packages `crt.library` and
   `stdlib.library`, the ABIv11 C runtime; for two SDL games also `SDL.library`.
5. **Running it.** Each candidate was installed by `pkg`, started, and removed,
   on AROS One 1.3 x86_64. Windowed programs had to still be running 15 seconds
   after launch; shell tools had to produce output and a return code.

Steps 1–4 are aids. Step 5 is the evidence.

## The ten that were not published, with reasons

| package | why not |
|---|---|
| `targa_datatype` | a datatype: installs into `Classes/DataTypes` and `Devs/DataTypes`, which v0.1 does not do |
| `playsid-gui` | drives `SYS:C/PlaySid`, which AROS One does not have (it ships TinySID) |
| `ffmpegvideotool` | needs an ffmpeg binary; AROS One ships ffmpeg's data directory but no such command |
| `smb2-gui` | duplicates the SMB2 tools AROS One already ships, and needs a network share to exercise |
| `amigakeyremaper` | remaps the keyboard, which is how the test driver types into the guest; not put on a shared testbench |
| `cls`, `timeit`, `lzop` | ran, but produced no output a log could capture (`CLS` clears a console; the other two printed nothing to a redirected stream), so the evidence is too weak to publish on |
| `joytest`, `screentest`, `zapperng`, `diskusage`, `gisplita`, `unmount`, `tinysid` | started and ran fine; simply not needed to reach twenty. They are candidates for the next round, not rejects |

## What "tested" does not mean

The programs were started, not exercised. Lopan draws its board and prints
`Failed to load sound NN` for sounds its own archive does not contain: its
author's business, recorded in the manifest so nobody reports it as a packaging
fault. A game that opens its window may still be unplayable at level 3. The
index says what was measured and no more.

## One measurement that is not about packages

Removing a package whose program is **still running** leaves the drawer behind:
every file is deleted, but the drawer cannot be, because the running program
holds it as its current directory. `pkg` reports success, which is right: the
package is gone from the registry. Installing over that empty drawer is allowed
(it is not user data); installing over one with files in it is refused.
