---
title: "jitterentropy-library 3.7.0 as the entropy source on AROS One"
date: 2026-09-29
machine: pool slot v11-1, AROS One 1.3 x86_64 (ABIv11), QEMU/TCG, -cpu qemu64 (no RDRAND)
apkg_sha256: f38c2098df22debd20468ad4bc4a42991ff8da6e0c26061cda7dca93ff9d79d1
---

# jitterentropy as the entropy source

The first increment of replacing the project's own jitter code with a
maintained implementation. `third_party/jitterentropy` is the 3.7.0 release
unchanged except that its platform header is replaced by
`src/tls/jent-aros/jitterentropy-base-user.h`; it is compiled with `-O0` as
upstream requires. `src/tls/entropy_jent.c` feeds Mbed TLS from it; there
is no fallback to anything else. Mainline keeps `getentropy()`
(`src/tls/entropy_getentropy.c`). The project's own jitter code
(`entropy_aros.c`) is gone from every build.

## Two boots from the same disk image (`b1-*`, `b2-*`)

Each boot is a fresh power-on of the pool machine from its disk. It is **not**
a restore of a RAM snapshot, which would also restore the generator's state
and is a different case, not tested here.

| check | b1 | b2 |
|---|---|---|
| `jent_entropy_init()` | 0: the QEMU TCG timer was accepted | 0 |
| three reads through `mbedtls_hardware_poll` | PASS, different each time | PASS, different from b1 |
| health tests (APT, RCT, RCT memory, lag) | no failure reported | no failure |
| configuration chosen by the library | OSR 3, 256 KiB memory block, no internal timer | same |
| forcing the internal timer, which is not built | refused (rc 1), no data | refused |
| `apkg update`, `install zaphod`, `remove` over TLS | worked | worked |

## What this does not establish

- The entropy rate. Upstream's raw-noise collection and SP 800-90B analysis
  (`tests/raw-entropy/` in the release) has not been run; that is the next
  step, and its result, not the library's acceptance of the timer, is what
  can support a claim.
- Behaviour after a RAM snapshot restore, under load, on a CPU with RDRAND,
  or on real hardware.
- i386 and aarch64: both builds link the library; neither was run with it.
  Upstream notes that on Apple M1 the aarch64 system counter is too coarse
  and uses another clock there, so hosted Macaros may be refused.
