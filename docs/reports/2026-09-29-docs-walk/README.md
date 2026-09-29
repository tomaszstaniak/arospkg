---
title: "The packaging guide, walked on Micropolis 0.1.0-rc3"
date: 2026-09-29
machine: pool slot v11-1, AROS One 1.3 x86_64 (ABIv11), QEMU/TCG
apkg_sha256: 40c2ba9350bc10e6da294d0cf0675aefb8f3dc86c519fd8c992bb4bf675609e0
archive_sha256: c46bbc14274af53fb4452f59081fbcf28f1ce5fffb534c06d169b0b1911cd124
---

# The packaging guide, walked on Micropolis

The run that `docs/guide/packaging.md` quotes. It covers the published
Micropolis 0.1.0-rc3 release archive and `apkg` 0.3 taken from
`dist/arospkg.x86_64-aros-v11.zip` (`887b7536…`).

## Host

`manifest.toml` is the guide's manifest. `tools/mkindex.py` rejected it with
`status = "skeleton"` and published it with `status = "approved"`, with
`subdir` and `icon` checked against the cached archive. The resulting
`index.json` was staged with `apkg` and the two scripts (`stage.sha256`).

## Guest

From a Shell at the default 40960-byte stack, root `SYS:Walk`,
`--index RAM:rxt/index.json`:

| step | file | result |
|---|---|---|
| version | `w0-version.txt` | `apkg 0.3`, binary `40c2ba93…` |
| show | `w1-show.txt` | native; three requirements satisfied; not installed |
| install | `w2-install.txt` | downloaded from the GitHub release through its redirect, verified, installed |
| registry | `w3-info.txt` | 130 files, icon `installed` |
| root | `w3-root.txt` | drawer `micropolis` and `micropolis.info` |
| start | `s3-running.png` | `Run >NIL: <NIL: Micropolis CITY.CTY` from the installed drawer; the city window open |
| save | `s4-save-requester.png`, `s7-saved.png` | the save requester opened on `SYS:Micropolis`; the drawer was changed to `SYS:Walk/micropolis`, and the title bar read `saved: SYS:Walk/micropolis/MyTown.cty` |
| quit | `w4-status.txt` | Esc; Micropolis no longer in the process list |
| change | | `Echo >>SYS:Walk/micropolis/ReadMe.txt "my own note"` |
| before removal | `w5-before.txt` | `MyTown.cty`, `MyTown.cty.info` and the changed `ReadMe.txt` among the package files |
| remove | `w5-remove.txt`, `micropolis-6abb87aa.json` | 129 removed, `ReadMe.txt` kept and listed |
| after | `w6-after.txt`, `w6-list.txt`, `s10-after-remove.png` | `MyTown.cty`, `MyTown.cty.info` and `ReadMe.txt` left in the drawer; drawer icon removed; archive still in `cache/`; nothing installed |

Files are as the guest wrote them, with carriage returns removed.
`w4-status.txt` was converted from Latin-1.

## Found

- `show` cuts the licence at 63 characters.
- Removal keeps files the package never installed, such as the saved city,
  and does not list them. Only the changed installed file is listed.
- Micropolis's save requester starts in `SYS:Micropolis`, which does not
  exist when the game is installed by arospkg. It is fixed in the port
  (`src/game.cpp`, `SAVE_DIR`). This is a matter for the Micropolis port, not
  for arospkg.

## Not covered

- Starting from the icon in Wanderer. The walk started from a Shell.
- An upgrade: there is only one revision of this version.
- Mainline (ABIv1), aarch64, real hardware.
