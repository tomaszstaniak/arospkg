# Broken downloads, development build (2026-10-02)

Development results for the transport fix; not release acceptance (that is
on the frozen rc2 binaries). AROS One 1.3 x86_64, pool slot `v11-2`, QEMU
TCG. Binaries in `stage.sha256` (apkg `b358b87d...`, PkgManager
`c7606691...`), built from the working tree of `mbedtls` before the
commit that adds this report.

The failure is injected with `--break-download-at` (a reset socket: the TLS
adapter's next `recv` fails with `ECONNRESET`) and `--corrupt-download-at`
(the next `recv` gets corrupted bytes), so it returns through the adapter
like a real one.

| part | result |
|---|---|
| CLI (`nbd4-cli.txt`) | 19 of 19: a reset reports "connection reset" with bytes of total, not "wrong size"; corrupted data reports a TLS error, not a reset; nothing in the cache, no registry entry, lock released; the next install succeeds; an interrupted update leaves the index byte for byte as it was; a cancel still reads as a cancel |
| window (`nbd4-gui.txt`) | 10 of 10: the soliton download breaks at 51,200 of 223,945 bytes with "connection reset"; nothing cached, lock released; the next install in the same window succeeds |
| close (`nbd4-gone.txt`) | 2 of 2: port gone, no lock left |

Earlier runs, kept apart:

- `nbd1` (15 of 15 CLI) used an earlier injection that stopped the loop in
  `net.c` itself; it does not test the adapter. Its window part timed out
  with a 2.5 MB retry still running; the window log was not collected, so
  the cause is not established.
- `nbd2` never ran: the guest stood in GRUB's entry editor, where the
  harness's Return only added a line (fixed in `tests/arexx/pool-lib.sh`).
- `nbd3`: CLI 19 of 19; the window part was a test error. The break was set
  below the index's size, so it hit the window's index update instead of
  the archive, and the script did not check that INSTALL was accepted
  (`nbd3-gui-invalid.txt`). The message for the broken update was right.
