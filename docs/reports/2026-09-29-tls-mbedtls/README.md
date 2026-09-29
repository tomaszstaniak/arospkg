---
title: "apkg on Mbed TLS 3.6.7, on AROS One"
date: 2026-09-29
machine: pool slot v11-1, AROS One 1.3 x86_64 (ABIv11), QEMU/TCG, -cpu qemu64
apkg_sha256: 9814f72e4929c335fd2a03ea80d1b3fd54e2bf4e47c74ccdb32855125efda33a
---

# apkg on Mbed TLS 3.6.7

TLS moved from the SDK's OpenSSL 1.1.0h to Mbed TLS 3.6.7 (LTS), compiled
into the program (`third_party/mbedtls`, release SHA-256 `a7e8bcbe…`).
`src/libpkg/tls.h` is the interface; `src/tls/mbedtls.c` is the default and
`TLS=openssl` still builds the old one.

| | OpenSSL 1.1.0h | Mbed TLS 3.6.7 |
|---|---|---|
| apkg | 3.6 MB | 869 KB |
| PkgManager | 3.7 MB | 932 KB |
| TLS 1.3 | no | yes |
| randomness | the SDK's generator | `src/tls/entropy_aros.c` |

## Randomness

AROS One has no `getentropy()` and no `entropy.resource`. The source is
timing jitter: for each sample, the loop iterations until the cycle counter
changes, together with the change. It is folded through SHA-256, RDRAND is
added when the CPU has it (QEMU's `qemu64` does not), and SP 800-90B
repetition-count and adaptive-proportion tests run on every sample. The
credit is 1/16 bit per sample, a provisional figure.

| file | method | most-common-value min-entropy, bits/sample |
|---|---|---|
| `mb1-raw-first-method.bin`, 200,000 samples | first attempt: time a short memory walk | 0.145; median delta 0, so under TCG the counter hardly moves in that time |
| `mb2-raw50k.bin`, 50,000 | iterations until the counter changes | 0.665 |
| `mb3-raw.bin`, 50,000 | the same, final build | 1.185 |

Under TCG the counter advances in steps of 1000; about 30 iterations fit
between steps, and the number varies. The first method failed its health
check and the client refused to connect ("no good randomness on this
machine; refusing to connect"), which is the intended failure. Analyse any
file with `tests/entropy/estimate.py`.

**These figures are not a security margin.** They are one estimator (most
common value) on a few runs under one QEMU configuration. They do not rule
out a predictable sequence, and say nothing about dependence between
samples, restarts, cloned VM snapshots, other loads or real hardware.
Passing the health tests does not make the source validated under SP
800-90B. The source is provisional: see the open work in the branch notes.
The path without RDRAND is the one measured here (QEMU's `qemu64` has no
RDRAND); a CPU with RDRAND has not been tested.

## TLS on the machine (`mb3-*.txt`)

- `probe POLL` three times: PASS, different output each time.
- `apkg update` from raw.githubusercontent.com; `install zaphod` from AROS
  Archives through its redirect; `install micropolis` from a GitHub release
  through release-assets (2.5 MB in about 4 s); `list`; both removed.
- `--verify-name wrong.example.com`: refused, "certificate for
  wrong.example.com rejected: The certificate Common Name (CN) does not match
  with the expected CN".
- CA bundle renamed away: refused before connecting.

## Not covered

- The 0.3 regression suites were not run on this build.
- Mainline (ABIv1), i386 and aarch64 builds.
- KVM and real hardware entropy.
