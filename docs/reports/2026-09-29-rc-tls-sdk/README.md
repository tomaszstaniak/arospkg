---
title: "Developer packages accepted, and the TLS refusals of the release candidate"
date: 2026-09-29
machine: pool slot v11-2, AROS One 1.3 x86_64 (ABIv11), QEMU/TCG
apkg_sha256: 09b299af6451fdaa518742de8316b3601b97b015dc15ad4cc244d5e7d44018c2
pkgmanager_sha256: 618e94e2ae0021dae4a6bc8e175d7b055504bea45d72a535e0a45d5d36cad4ed
---

# Developer packages, and the candidate's TLS refusals

## The packages (`sdk-archives.sha256`)

`sdk/make-packages.sh` builds `MbedTLS-3.6.7-aros1.x86_64-aros-v11.zip` and
`JitterEntropy-3.7.0-aros1.x86_64-aros-v11.zip`. Acceptance: both were
unpacked into an empty directory, and the two examples were built from
their contents and the declared toolchain only (compiler and SDK system
headers), with no path into this repository. The first attempt found a
real gap: `https_get` calls `mbedtls_strerror()`, which the profile did not
include; `MBEDTLS_ERROR_C` was added and the packages rebuilt.

| run on AROS One | result |
|---|---|
| `jent_read` | 32 bytes, rc 0 |
| `https_get raw.githubusercontent.com /` | TLS 1.3, TLS1-3-CHACHA20-POLY1305-SHA256, `HTTP/1.1 301`, rc 0 |
| `https_get expired.badssl.com /` | handshake refused, verify flags 0x1 (expired), rc 10 |

## The candidate's refusals (`rc-tls.txt`)

`apkg` `09b299af…` with `--index-url` pointed at each case:

| case | result |
|---|---|
| expired certificate (expired.badssl.com) | refused: validity has expired |
| untrusted root (untrusted-root.badssl.com) | refused: not signed by a trusted CA |
| self-signed (self-signed.badssl.com) | refused: not signed by a trusted CA |
| wrong host (wrong.host.badssl.com) | refused: CN does not match |
| plain `http://` | refused: not an https URL |
| HTTPS redirecting to `http://` (httpbin.org) | refused: not an https URL |
| forced entropy failure (`apkg-entropyfail`, a test build with `-DAROS_JENT_FORCE_FAIL`, `ed983c97…`) | refused: no good randomness; no connection made |
| the real catalogue | fetched |

A missing CA bundle was refused in the earlier run
(`docs/reports/2026-09-29-tls-mbedtls/`). Not tested: a certificate that is
not yet valid (no public test host serves one); it goes through the same
validity-period check as the expired case.
