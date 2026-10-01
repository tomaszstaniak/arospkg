# apkg presentation on CON: (2026-10-01)

First guest check of the presentation code (branch `presentation`), on the
path every AROS One user has: the ordinary Shell window, CON:, which does not
answer the terminal's presentation query (operation 13). Not tested here:
a terminal that answers it (aros-xterm), mainline, i386, aarch64.

System: AROS One 1.3 x86_64 (ABIv11), pool slot `v11-1`, QEMU TCG.
`apkgrc1` is the published 0.3.1-rc1 `apkg` (`e4583d6a...`).

## Redirected output is unchanged

`pres1-a-*` (rc1) and `pres1-b-*` (presentation build `d47e5032...`) are the
same four commands, `update`, `show gmore`, `search`, `install gmore`, each
redirected to a file, on two fresh roots. After replacing the root's name
(`SYS:PresA`, `SYS:PresB`), every pair is byte-identical (126, 558, 8144 and
252 bytes), and no file contains an escape character.

## CON: does not hang and gets plain text

The query on the Shell window's `Output()` is refused by the console at
once; `show` and `search` print as before (`con-show.png`).

## Progress lines in a plain window

The first build printed `verify`, `extract` and `publish` at 100% (1 of 1)
on an install from the cache (`con-install-before-fix.png`), and also with
`--plain`, starting at 0%. Fixed: only the download, a line at each 10% step
reached, nothing with `--plain`. The build after the fix (`70c22890...`):
`con-install-and-plain.png`, a 38 KB download in lines at 26, 53, 79 and
100%, then an install with `--plain` without any.
