---
title: "0.3.1-rc1: each archive run as published"
date: 2026-09-29
commit: 7f619de608595602a4a6260405bce53906f6f5f1
---

# 0.3.1-rc1: each archive run as published

Each archive from `tools/make-rc.sh 0.3.1-rc1` was unpacked on its target
and run from its own drawer, with a separate root (`RAM:RcTest`). Tests of
the earlier candidate (`09b299af`) are not counted for these binaries.

| archive | binary | where | runs | TLS | full cycle |
|---|---|---|---|---|---|
| x86_64-aros-v11 apkg | `e4583d6a…` | pool v11-2, AROS One 1.3 (`one-*`) | yes | yes: catalogue fetched, expired certificate refused | yes: zaphod installed from AROS Archives, running in `Status`, removed |
| same | same | pool v11-3, ABIv11 2026.09 base (`x8664arosv11-*`) | yes | not checked: that base had no network running | no |
| x86_64-aros-v11 PkgManager | `732ecd9b…` | none | not run | no | no |
| x86_64-aros-v1 apkg | `5023191a…` | pool v1-1, mainline (`x8664arosv1-*`) | yes | yes | no: an ABIv11 package was refused, as it must be; no mainline package to install |
| i386-aros-v0 apkg | `c3ed67e3…` | pool v0-2, ABIv0 (`i386arosv0-*`) | yes | yes | yes, with a test index: dirtree installed, run (rc 0), removed |
| aarch64-aros apkg | `c21f17da…` | hosted Macaros, with `posixc.library` added (`aarch64-*`) | yes | no network | no |

The release regression was run on the previous candidate, not on these
binaries (`local`, not in this report). Its trials A to D did not run: the
LHA suite's Micropolis window covered the Shell.
