# SDK packages, port revision aros2 (2026-09-30)

## What changed from aros1

- The libraries and headers are the same: every object in the six `.a`
  files and every header is byte-identical to aros1 (compared member by
  member).
- New: the documentation for other developers (maintenance scope, the three
  layers of the jitterentropy package, the clock check and the absence of a
  fallback, exact rebuild steps, the limits of the entropy assessment),
  `LICENSE.port` (MIT) for the port's own files, and a `BUILD.txt` that
  names the toolchain's origin and the full source commit.
- `examples/https_get.c` now checks the TLS set-up, the host name, the
  write and the read, and leaves through one error path. Because the
  example changed, it was run again.
- `sdk/make-packages.sh` refuses to build when `sdk/`, `src/tls/` or
  `third_party/` has uncommitted changes.

## Test

- Binaries built only from the unpacked aros2 packages, with the declared
  toolchain: `https_get` SHA-256
  `247af8b5c6b6cc46456b94d1203052cc0607a7c2513e4f4c0c69c84c34659fb0`,
  `jent_read` `2dfd1e9f715dd30b2195b03fd4d3d1b53e8c8473379925f39af031eab249e19f`
  (unchanged from aros1). Hashes of everything staged: `stage.sha256`.
- System: AROS One 1.3 x86_64 (ABIv11), pool slot `v11-2`, QEMU TCG,
  AROSTCP up; `Version` printed `Kickstart 51.51, Workbench 40.0`
  (`sdk2-system.txt`).
- Commands: `t.script`.

| command | result |
|---|---|
| `jent_read` | 32 bytes, rc 0 |
| `https_get raw.githubusercontent.com /` | TLSv1.3, TLS1-3-CHACHA20-POLY1305-SHA256, `HTTP/1.1 301`, rc 0 |
| `https_get expired.badssl.com /` | handshake refused, verify flags 0x1 (expired), rc 10 |
| `https_get wrong.host.badssl.com /` | handshake refused, verify flags 0x4 (name mismatch), rc 10 |
| `https_get untrusted-root.badssl.com /` | handshake refused, verify flags 0x8 (not trusted), rc 10 |
| `https_get self-signed.badssl.com /` | handshake refused, verify flags 0x8 (not trusted), rc 10 |

Files: only this run's (`sdk2-` prefix), listed in `MANIFEST.sha256`.
