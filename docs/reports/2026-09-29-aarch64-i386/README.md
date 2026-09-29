---
title: "apkg for aarch64 and i386"
date: 2026-09-29
---

# apkg for aarch64 and i386

Both builds use the same sources as the x86_64 ones, with Mbed TLS 3.6.7
and the entropy source in `src/tls/entropy_aros.c`. Neither is released.
The catalogue has no aarch64 or ABIv0 packages yet.

## aarch64: `src/build-aarch64.sh`

Clang cross tools and the AROS build of the local Macaros workspace, with
`-ffixed-x18`, linking posixc; 1.0 MB, no undefined symbols. Run on hosted
Macaros (native aarch64 on the Mac, not emulated), driven by `aros-ctl`.
That image had no `posixc.library`; it was built from the same tree
(`make compiler-posixc`) and copied into the image's `Libs/`.

| step | result (`aarch64-*.txt`) |
|---|---|
| `--version` | `tls Mbed TLS 3.6.7` |
| entropy | PASS twice; 1.313 bits/sample over 50,000 samples, 21x the credit (`cntvct_el0`) |
| `show zaphod` (x86_64 package) | incompatible: built for x86_64, this machine is aarch64; requirements not checked |
| `install zaphod` | refused: wrong CPU |
| `update` | no network: hosted Macaros has no `bsdsocket.library` |

Not checked: downloads (no network in hosted Macaros), native aarch64 AROS.
The ABI tag is `v1`, as for mainline; whether aarch64 needs its own is open.

## i386: `src/build-i386.sh`

The ABIv0 i386 toolchain and SDK; zlib compiled from the build's own
1.2.13 sources with its SIMD stub; 530 KB, 32-bit, no undefined symbols.
**Not run.** The only i386 machine in the pool is not marked ready and is
in use for another port. The client does not know the ABI tag `v0`, so it
treats its own ABI as unknown and hides nothing; `mkindex.py` does not
accept `v0` either. Supporting ABIv0 packages is a design step of its own.
