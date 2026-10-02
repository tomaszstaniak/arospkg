# "the download is the wrong size" with --slow 200: a reset by the server (2026-10-02)

Seen first in the joint presentation test (Limpet, binary `6f9ee12c`):
`install micropolis --slow 200` failed with "the download is the wrong
size". Reproduced here without the terminal, in an ordinary Shell.

## Set-up

- AROS One 1.3 x86_64 (ABIv11), pool slot `v11-2`, QEMU TCG, AROSTCP.
- `apkg-diag` (`82c7a22d...`): commit `57018a8` plus `diag.patch`, which
  only prints one `DIAG` line after each HTTP read loop: how the loop ended
  (`end_n`), the last Mbed TLS return code, the socket's `Errno()`, the bytes
  received and the declared `Content-Length`. Not a release build.
- Commands: `t.script`, each install on its own root with an empty cache.

## Results

| run | result | how the read ended |
|---|---|---|
| micropolis (GitHub, 2,593,505 B), `--slow 200`, redirected | wrong size, rc 10 | `tls_ret -0x6c00`, `Errno 54` (ECONNRESET), body 2,523,122 B |
| the same, second time | wrong size, rc 10 | `-0x6c00`, `Errno 54`, body 2,519,040 B |
| micropolis, no `--slow` | installed | `-0x7880` (close_notify), body 2,593,505 B |
| micropolis, `--slow 50` | installed | close_notify, complete |
| gmore (Archives, 35,787 B), `--slow 200` | installed | close_notify, complete |
| micropolis, `--slow 200`, typed in the CON: window | installed | close_notify, complete (`con-slow200.png`) |

## What it shows

- The server (`release-assets.githubusercontent.com`) **reset the
  connection** about 97% of the way through a download that `--slow 200`
  stretched to about 40 seconds. The client did not close anything; the
  terminal is not involved (it failed with output redirected to a file).
- It does not happen every time (the run in the CON: window completed),
  nor without the slowing, nor for a small file.
- `-0x6c00` is `MBEDTLS_ERR_SSL_INTERNAL_ERROR`, which apkg's own receive
  callback returns for every socket error; the real error is only in
  `Errno()`.

## The defect in apkg

The download is refused, which is right, but for the wrong stated reason:

1. the receive callback (`src/tls/mbedtls.c`, `bio_recv`) turns any socket
   error into one Mbed TLS code and drops `Errno()`;
2. `tls_read` turns any error into `-1`;
3. the HTTP read loop (`src/libpkg/net.c`) stops on `n <= 0`, so a broken
   connection is treated like the end of the response, and the size check
   then reports "the download is the wrong size".

A user on a slow line who meets the same reset is told the file is the
wrong size instead of "the connection was reset by the server after
2,523,122 of 2,593,505 bytes". The same code is in 0.3.1-rc1.
