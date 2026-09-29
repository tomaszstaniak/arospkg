# 0.3.1-rc1 regression on AROS One (2026-09-30)

## What was tested

- Release: `v0.3.1-rc1`, commit `0e4a1f6c00db17cacd705208babace2f6dc94f9e`.
- Archive: `arospkg-0.3.1-rc1.x86_64-aros-v11.zip`, SHA-256
  `01d5bdd904fe52a8e24c1abc17fa0cb591998bc6e672be68c7e4314c8d0f325f`.
- `apkg` SHA-256 `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae`,
  `PkgManager` SHA-256 `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`
  (the files in that archive; the payload was built from it by
  `tests/make-payload.sh` with `RELZIP`/`RELDIR`).
- System: AROS One 1.3 x86_64 (ABIv11), pool slot `v11-2`, QEMU TCG on
  macOS, AROSTCP up. The first CLI suites run at the default 40960-byte
  Shell stack.
- Driver: `tests/run-regression-on-pool.sh` at commit `c4a26b1`; commands per
  suite are the scripts in `tests/` named in `grades.txt`.
- Time: 2026-09-29 23:55 to 2026-09-30 01:53.

Results from the earlier candidate (apkg `09b299af...`) are auxiliary and not
counted here.

## Results

Graded from the report files with `tools/grade.py --main e4583d6a...`
(`grades.txt`). Every report names the binary that wrote it; a missing report
counts as a failure.

| suite | result |
|---|---|
| S/UPGRADE | pass (61 PASS lines) |
| S/SOLUP | pass |
| S/VERIFY | pass |
| S/JSONTEST | pass |
| R/REL1 | pass (64 PASS lines) |
| R/REL2 | pass |
| R/REL1G | pass (run `g2a` pinned to the 2026-09-12 binary `37b2774a...`, by design) |
| R/LHATEST | pass |
| R/TRIALA | pass (30 PASS lines) |
| R/TRIALB | as in the v0.1 and v0.2 records: 15 of 17 windowed programs alive after 15 s; `isomaker` and `isotool` exit on their own, as before |
| R/TRIALC | pass (30 PASS lines) |
| R/TRIALD | pass |
| R/ACCEPT | pass; first checks `PASS assert-hash` of the archive and of the unpacked `apkg`, then install, update, remove from the unpacked release |
| PkgManager window flow | completed; same event sequence as the earlier candidate, including the deliberate conflict refusal on a changed `Soliton.guide` (`gui-events.log`, `shot-run*.png`, `shot-g9.png`) |

This is the first run of PkgManager `732ecd9b...`.

## The power cycle before trial A

LHATEST finished and wrote its reports at 00:08:21. Its Micropolis window
does not close on Ctrl-C, stayed in front of the Shell, and took the keys
meant for the Shell (`shot-no-shell-before-R-TRIALA.png`). The driver's
check that the Shell answers (an acknowledgement file) failed before trial A
was typed, so the driver power-cycled the machine and trial A then ran from
its beginning on a fresh boot. No suite was interrupted or repeated, and no
result was changed by the restart. Trials A, B and C then ran in one boot as
designed.

## Files

Only the files this run sent to its own receiver directory are here, listed
with hashes in `MANIFEST.sha256` (`tools/check-reports.py` checks it).
`driver.txt` is the driver's log, `grades.txt` the grader's output.
