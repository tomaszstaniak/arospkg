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

## Raw-noise measurement, upstream procedure (2026-09-29/30)

Configuration: pool slot `v11-1`, AROS One 1.3 x86_64 (ABIv11), QEMU TCG on
macOS (Apple Silicon host), one boot, nothing else started in the guest.
Program: upstream `tests/raw-entropy/recording_userspace/jitterentropy-hashtime.c`
from jitterentropy-library 3.7.0, unchanged, built -O0 against the same
library as the SDK package (binary SHA-256 `ca39e0e4...`). Commands as in
`raw-t.script`:

- runtime: `hashtime 1000000 1 RESULTS:jent-raw-noise --max-mem 0`
  (one million time deltas of the hash loop);
- restart: `hashtime 1000 1000 RESULTS:jent-raw-noise-restart --max-mem 0`,
  where a "restart" is a new collector instance (`jent_entropy_collector_alloc`)
  inside the same process and the same boot, not a reboot of AROS.

The host limited the run to 14400 s. The runtime series took 21:18 to 23:33.
The restart series had reached **470 of 1000** restarts at the limit.
Hashes of all data files: `raw-data.sha256`. The data themselves (8.4 MB,
471 files) are kept outside Git in
`arospkg-jent-raw-20260929-v11-1.tar.xz`, SHA-256
`61d7f46ff0602028aa1fbdf1909d21102fa1cd268d36de54b4e745523bb06c03`,
with the analysis output and the guest script.

### Runtime result

Upstream `validation-runtime/processdata.sh`, mask `FF` (8 bits), NIST
SP800-90B_EntropyAssessment `ea_non_iid` (commit 87c104d0), full output in
`raw-runtime-ea_non_iid.txt`:

| | value |
|---|---|
| H_original (8-bit symbols) | 4.672794 |
| H_bitstring | 0.276656 |
| min(H_original, 8 x H_bitstring) | **2.213248 bits per sample** |
| needed for the library's default oversampling (OSR 3) | 1/3 = 0.333 |

Estimate 2.21 bits per sample; threshold 0.333; the runtime criterion is
**met in the configuration tested**. The ratio between the two is not a
safety margin.

Limits of this result:

- Under TCG every delta is a multiple of 1000 (QEMU's clock), so the low byte
  takes only 32 values. The estimate is for that byte as sampled; the GCD is
  not divided out, as upstream does not either.
- One machine, one boot, idle guest, one host. Not measured: KVM, real
  hardware, load, snapshot restore.
- No SP 800-90B certification is claimed or intended; the thresholds and the
  data were not changed.

### Restart result

**Not evaluated.** `ea_restart` needs the full 1000 x 1000 matrix; 470 rows
are not a valid input and were not analysed. A complete restart series needs
about 4 hours on this setup and was not started again.

The SP 800-90B style assessment is therefore **not complete**: the runtime
part passed, and a positive runtime result says nothing about the restart
part.

## What this does not establish

- The restart entropy (see above), and anything beyond the runtime figure
  for this one configuration.
- Behaviour after a RAM snapshot restore, under load, on a CPU with RDRAND,
  or on real hardware.
- i386 and aarch64: both builds link the library; neither was run with it.
  Upstream notes that on Apple M1 the aarch64 system counter is too coarse
  and uses another clock there, so hosted Macaros may be refused.
