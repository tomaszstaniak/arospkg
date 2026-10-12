# apkg 0.5.0 in Limpet 0.3.0 and in CON: (2026-10-12)

## Setup

- **Machine:** pool slot `v11-3`, ABIv11 2026.09 (`Kickstart 51.51, Workbench 40.0`), QEMU TCG, display none. Session owner `claude-limpet-ctrlc-20261012`.
- **apkg:** the `apkg` from `dist/rc/0.5.0/arospkg-0.5.0.x86_64-aros-v11.zip`. On the guest, `apkg --version` reported binary `e7432440…25b9`, the required hash, and source `6f4b0b9ff57d17bc` (`guest/c050-version.txt`).
- **Limpet:** the published `Limpet-0.3.0-abiv11.zip`, started from RAM: (`Limpet 0.3.0 (10/11/26)`).
- **Staged files:** `STAGE.sha256`.
- **Guest writes:** everything ran under RAM: (root `RAM:t`). The only files written to RESULTS: are the `c050-*` files (copies in `guest/`, hashes in `guest/COLLECT.sha256`).
- **Network:** AROSTCP was started by hand with `Run >NIL: Execute SYS:System/Network/AROSTCP/S/startnet`. The first fetch worked about 3 minutes later.
- **Screens:** `screens/NN-*.png` are full-screen vmctl PNG conversions; `c-*` are pamcut crops of them.

## Results

| # | Case | CON: | Limpet 0.3.0 |
|---|---|---|---|
| 1 | `update`, `search game`, `show folio`: colour and wrapping | **Pass.** No colour, as expected in CON:. The console breaks long lines mid-word; nothing is cut (`c-02`, `c-03`, `c-04`). | **Pass.** Colours present. **Narrow window (57 columns):** search switches to a stacked layout, and search and show wrap at word boundaries; nothing is cut (`c-22`, `c-23`, `c-24`). One exception: the `update` summary "…110 packages for this syst / em" is broken mid-word by the terminal, so apkg did not word-wrap that line. **Wide window (about 118 columns):** table layout, nothing cut (`25`, `26`, `27`). |
| 2 | `search game >RAM:s.txt`, then `Type` | **Pass.** 3,769 bytes, 51 lines (47 packages and the footer), no ESC or CR bytes (`guest/c050-con-search-redir.txt`). | **Pass.** Byte-identical to the CON: file (`guest/c050-lim-search-redir.txt`); `Type` output is plain (`28`). |
| 3 | `outdated --json`, `run --check --json grep` | **Pass.** Valid JSON (checked with Python), no escape codes. rc 0 for `outdated`. rc 5 for `run --check` while grep had no description (`c-06`, `guest/c050-con-*.json`). | **Pass.** Valid JSON, no colour on screen or in the files, rc 0 (`29`, `guest/c050-lim-*.json`). |
| 4 | Ctrl-C during `--slow 500 update` | **Pass.** At 2 s and 5 s after start: "Catalogue update cancelled", rc 10, one prompt (`c-13`). During the transfer (after 96%): the same (`c-43`). | **Pass.** At 2 s, 5 s and 22 s: "cancelled", rc 10, one prompt each (`31`). During the transfer (after 71–83%): the same (`c-33`). |
| 5 | `run grep -c a S:Startup-Sequence`; `run grep` with no arguments | **Fails as specified; passes with a hand-written description** (see below): "49 matches.", "grep ended with return code 0", `$RC`=0. A no-match pattern gave `$RC`=5. With no arguments, grep's usage text and `$RC`=10 (`c-11`). | Same as CON: (`30`). |
| 6 | Ctrl-C into grep started with `apkg run grep xyz`, grep reading stdin | grep echoed the matching line. After Ctrl-C it went on waiting in Read, took the next typed line (`Echo "rc=$RC"`) as input, then ended with "return code 0" and one prompt (`c-40`, `c-41`). This is how CON: handles a break during a pending Read. | `^C` ended grep at once: "grep ended with return code 0", rc 0, one prompt (`c-35`). |

### Case 4: Ctrl-C during the name lookup, connect or handshake

On this guest the first progress line appeared only about 45 s after start; under TCG the TLS setup is slow. So the presses at 2, 5 and 22 s all came before the first byte of data. All of them gave "Catalogue update cancelled", rc 10, and one prompt, in both terminals.

Compare 0.4.2 on the same machine (2026-10-12, Limpet ctrl-c evidence `aros-xterm/docs/evidence/2026-10-12-ctrlc-double-prompt`):
- in CON:, a press during the handshake gave "TLS handshake … failed (-0x6c00)";
- in Limpet, a press during the lookup gave "cannot resolve …".

Which of lookup, connect and handshake each press hit was not instrumented. What was shown is "before data flows".

### Case 5: `apkg run grep` with the published catalogue

The published catalogue (`arospkg-index` `index-v2.json`, local checkout on `submit/arospkg-0.5.0`) has **no `launch` entry for any package**. The grep 2.0 archive has no `application.json` either. With the catalogue as published:
- `apkg run grep …` gives "Could not start grep: it does not say how to start it", rc 10 (`c-07`);
- `run --check --json` reports `"state": "no-description"`, rc 5.

To test the run path itself, I wrote a minimal `RAM:t/grep/application.json` on the guest (`guest/c050-grep-application.json`: schema 1, id, version, targets, `launch` = `Grep`, `shell`). `run --check` then reported `ready`, `mode: shell`, `stack: 8192` (`c-10`), and the results in the table followed.

For case 5 to pass with the real catalogue, the grep entry needs `[launch] executable = "Grep", mode = "shell"`.

## Not tested

- `upgrade --all` with Ctrl-C between packages.
- Mainline v1, i386 and AROS One 1.3 in this run.
- A physical keyboard: all keys were injected over QMP.
- The exact phase of each early press (see case 4).
- A Workbench-mode `apkg run`.
- Case 6 with a long-running program that does not read input.

## Driving error (no effect on the results)

While switching windows I clicked the CON: window's close gadget, which ended that Shell (`c-37`). Cases 6 and the CON: mid-download cancel ran in a new Shell opened with Amiga+W.
