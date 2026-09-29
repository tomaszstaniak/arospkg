---
title: "One i386/ABIv0 package through the whole client"
date: 2026-09-29
machine: pool slot v0-2, AROS ABIv0 20250313-1 pc-i386, QEMU/TCG qemu32
apkg_sha256: c1a988938bea1626c12986770782edf71496c004fc481300fbf5164337a6fd8c
---

# One i386/ABIv0 package through the whole client

After the target rules changed: the CPU is checked before the ABI, so an
unknown ABI never widens what is offered, and `v0` is a known ABI in the
client (`pkg_abi_known`) and in `tools/mkindex.py` (`ABIS`, and `ARCHES`
with `i386`), one list in both. The build states its ABI from its target
(`src/build-i386.sh` passes `v0`).

| step | result |
|---|---|
| `update`, `search` on the published catalogue | 27 x86_64 packages hidden, none listed |
| test index, both variants of `dirtree`, i386 first (`idxA.json`) | `search` lists the i386 one and hides one; `show` describes i386/v0, native, and names both variants |
| `install` | downloaded from AROS Archives through its redirect, verified, installed |
| registry (`iv-a-info.txt`) | `arch i386`, `abi v0` |
| start | `DirTree SYS:IvA/dirtree` drew the tree, rc 0 |
| `remove`, `list` | removed; nothing installed |
| the same index reversed (`idxB.json`) | the same variant shown and installed; registry i386/v0 |

`dirtree.i386.toml` is a test manifest; the package is not published.

Found: `--all-abi` labels an x86_64 package on this machine "[other ABI]";
the reason is the CPU.
