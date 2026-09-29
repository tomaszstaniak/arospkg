---
title: "apkg on mainline AROS (ABIv1)"
date: 2026-09-29
machine: pool slot v1-1, mainline v1-20260923, QEMU/TCG, network configured on 2026-09-29
apkg_sha256: d3670bf670993c34f80d29545c9cb3e141c2399f7b16dc0d7b8590ad7b35ca7c
---

# apkg on mainline AROS (ABIv1)

Built with `src/build-mainline.sh` (mainline toolchain and SDK, Mbed TLS
3.6.7, the same sources as the ABIv11 build). Two runs, each from a fresh
boot; the second finished the steps a crashing program had cut short.

| step | result |
|---|---|
| `--version` | `abi v1`, `tls Mbed TLS 3.6.7` |
| entropy (`probe POLL`, `v1b-raw.bin`) | PASS twice; 0.939 bits/sample over 50,000 samples, 15x the credit |
| `update` from the published catalogue | fetched over TLS 1.3 |
| `install zaphod` (an ABIv11 package) | refused before download: "wrong ABI for this system" |
| `show ghostscript` (test index, ABIv1) | native, requirements none stated |
| `install ghostscript` from AROS Archives | 12.5 MB downloaded, verified, installed |
| `gs --version` | **crashed**: Illegal address access in `stdc.library` `strlen` (`gs-crash.png`) |
| `remove ghostscript`, `list` | removed; nothing installed |
| `--verify-name wrong.example.com update` | refused: CN does not match |

Ghostscript 10.0.0 on AROS Archives is a mainline build from February 2023.
Mainline does not keep binary compatibility between nightlies, so it does
not run on the 2026-09-23 base. That is a finding about the package, not
about apkg; it will not be approved.

Output redirected to a file lost its first line in places (`v1b-rm.txt`
starts with `removed`, not with the first `list`), and a newline in
`v1-poll.txt`. This matches the known mainline issue with stdout on handles
not yet in write mode, fixed upstream in `da9bbd01b5` after this base.
