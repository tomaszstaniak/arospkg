---
title: "Rows and progress: the front-end contract, on the target"
date: 2026-09-23
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG
binary_sha256: a7120db95cf30513e035acf4cf9452992a5158f938a6793895d49e5f74aae668
abi: v11
script: tests/json-progress.script
---

# Rows and progress

The first post-v0.1 feature: a structured accessor (`pkg_query`,
`pkg_installed`) and a progress/cancel callback (`pkg_set_progress`), the two
things a Zune front end needs from `libpkg` before its first line is written.
Chosen after reviewing jonx's `pkg`, whose `MACHINE` output is the same idea
for the same reason. The comparison's other outcomes are in
`docs/STATUS.md`, *What comes next*.

Files came off the guest over the network (`putfile` → `recv.py`). Grading is
`tools/grade.py`: 7 reports, 7 expectations, one binary, **7 PASS, 0 FAIL**.

## What the run shows

| check | result |
|---|---|
| `search game` text equals the 2026-09-22 acceptance output, line for line | **identical** (host diff, 9 lines): search is now formatted from the rows, and the rows are sorted by id whatever order the index file had |
| `--json search` / `--json list` parse, and `installed` follows the registry | 5 blocks parsed; `soliton` flips to `installed: true` after install and the list is empty after removal |
| `--progress` on an install | `download 0/223945 … 223945/223945 cancel=yes`, `verify 1/1 cancel=yes`, then `extract` and `publish` with `cancel=no`; the redirect hop reports nothing |
| `--cancel-at download` (the check before the transfer) | status 14, no `.part`, nothing installed |
| `--cancel-at download-mid` (inside the transport, at 10240/34054) | status 14, no `.part`, nothing installed |
| `--cancel-at verify` | status 14, archive **cached**, nothing installed |
| `--cancel-at extract` | **ignored**, install completes with status 0, as `pkg.h` promises once a transaction exists |

## Three runs, three binaries

`superseded/` holds the two earlier runs. `6912c319` passed the same seven
checks; it was replaced the same day because the event gained `id` and
`can_cancel` (review: a Cancel button must grey out when the library stops
taking the request, and must not have to infer that from the phase name).
The library now enforces it in one place: a call site that says
`can_cancel` 0 gets no cancel back whatever the caller set.

## What the first run found

`superseded/json-*-binary-8eca4578.txt` is the first run. It passed
its six checks and showed two things worth fixing before the contract was
called done:

1. **The mid-download cancel was never exercised.** `--cancel-at download`
   fired at the callback *before* the transfer, so the path inside `net.c`
   that abandons a stream had no evidence. `download-mid` cancels only once
   bytes are flowing; the table above is the first time that path ran.
2. **The redirect hop reported its own body as a download**:
   `progress download 362/362`, then `0/223945`. A progress bar would have
   jumped to full and back. Only a 2xx response is reported now.

And one thing the host diff found: the staged test index was in a different
order from the published one, so `search` output depended on which index a
machine held. Rows are sorted in the accessor, not trusted from the file.

## Host side

`tests/test_entries.c` covers the row reader with no AROS involved: ABI
filtering and the `hidden` count, `show_all`, a build with no recorded ABI
hiding nothing, term matching, refusal of text without a `packages` array,
registry parsing, and the sort. `tests/run-host-tests.sh`: all pass.
