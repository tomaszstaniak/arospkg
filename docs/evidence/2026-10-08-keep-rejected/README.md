---
title: "--keep-rejected: keeping a download that fails verification"
date: 2026-10-08
machine: pool slot v11-3 (AROS One 2026.09 base, ABIv11), QEMU/TCG, AROSTCP started by hand
apkg_sha256_tested: 2530e8bfe2d46561611cf7b48fb3b495321be93ca8f6ebd4d70490633c6aefa1 (x86_64 ABIv11 build)
apkg_sha256_pi: 967a72bb938190bcb824561fb44bca5dd7819b8bb0996a971634de4827154b89 (aarch64-aros-raspi, NOT run)
---

# --keep-rejected

## Why

A user's `apkg install folio` on a real Raspberry Pi (native AROS, aarch64,
ABIv1, apkg 0.4.0) reached 100 % and then failed with "the download does
not match the index", with a different wrong SHA-256 on different attempts
(`da486088…`, `b18b5c2c…`). Afterwards `SYS:Packages/cache/folio.zip` did
not exist. The archive itself is correct (28,848,659 bytes, `693ebe24…`;
see `~/Work/AROS/local/pi-hash-20261005/`). QEMU did not reproduce it; the
Pi QEMU slot has no network, so the download path on Pi was never run here.

## What the code did (apkg 0.4.0)

- `net_fetch` (`src/libpkg/net.c`) reads the whole HTTPS response into
  memory, then writes it with one `fwrite` to `<root>/cache/<id>.<ext>.part`
  (`SYS:Packages/cache/folio.zip.part`).
- `fetch_verified` (`src/libpkg/ops.c`) checks the size, then the SHA-256
  of the `.part`. On either failure it **deletes the `.part`** and returns.
  Only a verified file is renamed to `folio.zip`. So "object not found" for
  `folio.zip` is the designed outcome, and the failing bytes are gone.
- No option kept them. Size was right in the user's case (otherwise the
  message would have been "wrong size"), so the corruption is in content.
- Separate defect found on the way: `sha256_file` did not check `ferror`.
  A read that failed half way hashed the part before it, and the caller
  reported "does not match the index" instead of a read failure (and with
  an uninitialised hash in the combined condition). Fixed; host test
  `tests/test_sha256.c` fails before (empty-input hash for an unreadable
  file) and passes after. Whether this happened on the Pi is not known.

## The change

`--keep-rejected` (advanced option). On a size or hash failure after a
download, the `.part` is renamed to `<archive>.rejected` instead of deleted
(a later one replaces it) and apkg prints the path, size, expected SHA-256,
and three received ones: the body in memory before the write, the file as
verification read it, and a second read. Without the flag nothing changes.
The file stays refused: no install, no change to the catalogue, and the next
install downloads again (`.rejected` is never looked up as a cache entry).

Built from HEAD without the change, `src/build-raspi.sh` reproduces the
published Pi binary byte for byte (`562dc286…`), so the diagnostic build
differs from 0.4.0 only by this change. Undefined symbols identical.

## Tests

| where | what | result |
|---|---|---|
| host (macOS) | `tests/run-host-tests.sh`, `test_help.py`, new read-error case | pass |
| v11-3, ABIv11 build | T1 index hash wrong, `--keep-rejected` (`kr1008-t1.txt`) | refused; kept as `folio.zip.rejected`, 28,848,659 B; memory = disk = re-read = `693ebe24…` |
| v11-3 | T2 same without the flag (`kr1008-t2.txt`) | refused, 0.4.0 wording, `.rejected` untouched |
| v11-3 | T3 index size wrong, `--keep-rejected` (`kr1008-t3.txt`) | "wrong size"; kept, replaced the older `.rejected` |
| v11-3 | T4 correct archive, `--keep-rejected`, `.rejected` present (`kr1008-t4*.txt`) | installed from a fresh download; `folio.zip` cached beside `.rejected`; `verify`: 15 as installed |
| QEMU rpi-1 | – | not run (no network; slot not ready) |
| real Pi | – | not run; the build is for the user |

The guest runs downloaded the Pi archive's bytes into an x86_64 test root
through test indexes (`bad.json`, `size.json`); T4 installed the x86_64
ABIv11 Folio. Wrapped hashes in screenshots are console wrapping, not data.
The first three script runs (`kr1008-a/b/c`) are empty: the network was not
up (a/b) and output redirected around `Execute` did not reach the file (c).

## Open

The cause on the Pi is not established. The kept file plus the three
hashes separate network/TLS/memory (memory hash wrong), write path (memory
right, disk wrong) and an unstable read (disk ≠ re-read).

## Provenance

Copied on 2026-10-11 from the git-ignored `docs/reports/2026-10-08-keep-rejected/`
so that the evidence is tracked with the change it describes. The change was
committed onto the 0.4.1 base on 2026-10-11; the binaries tested above were
built from the 0.4.0 base on 2026-10-08. The diagnostic archive handed to the
user (`local/pi-keep-rejected-20261008/apkg-0.4.0-keeprejected1.aarch64-aros-raspi.zip`,
SHA-256 f5e3db00c5e32239246f630d829b79ed8bd78c2a29e8df51c0477493ab3632fb) is
unchanged. The checksum failure on the real Pi remains unexplained.
