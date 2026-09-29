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

## aarch64: the posixc.library used

The aarch64 build ran only on Macaros (native aarch64 AROS hosted on macOS),
and only with a posixc.library added to its `Libs/`, because that Macaros
build did not include one. The file used:

- `posixc.library`, 404672 bytes,
  SHA-256 `e0c4f8ba6c87c5f8a1500b9c75ce6069beefed025b0859c81bf18b27f72e62b4`
- built with `make compiler-posixc` from a clean AROS tree at commit
  `6a55c079cc6abbd02c00f0290cebef4f0b17637d` (2026-08-30), Macaros
  darwin-aarch64 configuration;
- path in that build: `bin/darwin-aarch64/AROS/Libs/posixc.library`.

To reproduce: configure Macaros at that commit, run `make compiler-posixc`,
copy the library into the system's `Libs/`. Whether an aarch64 AROS that ships
its own posixc.library runs this apkg is untested.
