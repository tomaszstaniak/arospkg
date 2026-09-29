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
Run on pool slot `v0-2` (ABIv0 20250313-1, QEMU TCG, `qemu32`), a new
i386 machine made for this from the clean i386 baseline.

| step | result (`i386-*.txt`) |
|---|---|
| `--version` | `abi v0`, `tls Mbed TLS 3.6.7`, GCC 6.5.0 |
| entropy | PASS twice; 0.270 bits/sample over 50,000 samples, 4x the credit: less margin than on x86_64 (19x) |
| `update` | fetched over TLS |
| `install zaphod` (x86_64) | refused: wrong CPU |
| `show ghostscript` (x86_64, ABIv1) | "undetermined, this build does not know its own ABI" |
| `--verify-name wrong.example.com` | refused: CN does not match |

**Gap found:** the client does not know the ABI tag `v0`, so it treats its
own ABI as unknown, and then `search` hides nothing, not even packages for
another CPU (`i386-net.txt` lists all 27 x86_64 packages). `install` still
refuses them on the CPU check. The fix: hide on the CPU even when the ABI
is unknown, and teach the client and `mkindex.py` the tag `v0`.
