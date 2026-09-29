---
title: "v0.3 evidence: every suite, on the pair in the release archive"
date: 2026-09-29
machine: pool slot v11-2, AROS One 1.3 x86_64 (ABIv11), QEMU/TCG
apkg_sha256: 40c2ba9350bc10e6da294d0cf0675aefb8f3dc86c519fd8c992bb4bf675609e0
pkgmanager_sha256: 8345cec7d65a7fb9889974cd7a92d1cbbc58936f217c21f668fadc900d2d594b
archive_sha256: 887b7536329feab75185527dc6bf7fc839f3dbfb70e342148615af39715e909a
abi: v11
---

# v0.3 evidence

The whole evidence set on the two binaries in
`dist/arospkg.x86_64-aros-v11.zip`: the CLI suites carried over from 0.1
and 0.2, the window, the ARexx port, the ARexx examples and `apkg show`.

## The binaries

| file | SHA-256 |
|---|---|
| release archive `arospkg.x86_64-aros-v11.zip` | `887b7536…909a` (`archive.sha256`) |
| `apkg` in the archive | `40c2ba9350bc10e6da294d0cf0675aefb8f3dc86c519fd8c992bb4bf675609e0` |
| `PkgManager` in the archive | `8345cec7d65a7fb9889974cd7a92d1cbbc58936f217c21f668fadc900d2d594b` |

`archive-SHA256SUMS` is the archive's own list. Every other run used
binaries byte-identical to these (`cli-gui/payload-binaries.sha256`,
`arexx/stage.sha256`, `show/stage.sha256`). `ACCEPT` unpacked the archive
and ran the `apkg` in it, as the README tells a user to. Every CLI report
names the binary that wrote it. Each suite was graded with `tools/grade.py
--main 40c2ba93…`, which refuses a report from any other binary. The
exception is REL1G, which uses two on purpose: its run `g2a` is pinned to
the 2026-09-12 binary `37b2774a…`, rebuilt by `tests/make-payload.sh` from
commit `87708fc`, which writes the old-format entry that this `apkg`
removes.

## What passed

| suite | result |
|---|---|
| UPGRADE, from a Shell at `Stack 40960` | 16/16, 61 PASS |
| SOLUP | 4/4, 11 PASS |
| VERIFY | 16/16 |
| JSONTEST | 7/7 |
| REL1 | 46/46, 64 PASS |
| REL2, after a power cycle | 6/6, 8 PASS |
| REL1G | 2/2 runs, 4 PASS, each run from the binary pinned to it |
| LHATEST | 4/4 |
| TRIALA, 30 installs over TLS | 30/30 |
| TRIALB, the 30 started | console in `cli-gui/trialb-console.txt` |
| TRIALC, the 30 removed | 30/30 |
| TRIALD, the three packages published before 0.1 | 6/6; all three running after 15 s |
| ACCEPT, from the release archive | 3/3 |
| the window (install, upgrade, rollback, a refused conflict, upgrade once it is resolved, rollback unavailable without its archive) | complete: plans from the library as in 0.2 (`add=1 replace=1`; rollback `replace=1 remove=1`; `conflicts=1` naming `Soliton.guide`; then `add=2`; then `can_rollback=0` naming the missing archive). The registry is at revision 2 at the end, Soliton was running after each of three starts, and the window quit cleanly (`cli-gui/gui-*`) |
| ARexx acceptance AX9 | 194 guest checks, 0 failed; 29/29 host checks (`arexx/grade-AX9.md`) |
| ARexx examples EX8 | all eight invocations as documented (`examples/grade-EX8.md`) |
| `apkg show` SH6 | 54 checks, 0 failed (`show/grade-SH6.txt`) |

All the grader output is in `cli-gui/grades.txt`.

SH6 covers what 0.3 changed in how packages are judged and chosen:
- compatibility (`native`, `incompatible`, `undetermined`) is kept apart from
  requirements;
- an install for another CPU or ABI is refused before any download;
- two builds for one target are refused, not picked from;
- the entry chosen does not change when the index is reversed;
- an upgrade stays on the installed target when the index offers a newer
  revision for another;
- `show` changes no file's content, checked file by file;
- ARexx `NOINDEX` and `TOOLONG` are provoked.

## Not tested

- Nothing was built or run on mainline AROS (ABIv1), on aarch64 or on real
  hardware.
- No 32-bit ABIv0 program: EmuV0 did not start any program on this
  configuration (`docs/spikes/emuv0/README.md`).
- Installing through a package's own installer, and system components, are
  not in 0.3.

## Attempts that did not count

The CLI part ran in two drivings of `tests/run-regression-on-pool.sh` on the
same binaries: UPGRADE to LHATEST first, then trials A to D, ACCEPT and the
window. Three earlier tries at the trials are kept apart in
`attempts-that-did-not-count/`. None of them was a product failure.

1. The LHA suite leaves Micropolis running, and its window covered the
   Shell the driver typed into, so trial A never started. The driver now
   quits it with Esc.
2. A program from trial B opened its own screen in front of Workbench, and
   trial C's commands went to it. The driver now brings Workbench back
   (Amiga+N) and opens a new Shell.
3. One boot's network stalled partway through trial A's downloads. Two
   packages were not installed and the results never reached the host.
   From the host, the same files downloaded in half a second.

Since then, every boot proves the network with a file sent to the host, and
restarts up to three times if the file does not arrive. Before each suite,
the Shell has to acknowledge a line naming the run and the suite, or the
suite is recorded as not run. Every suite has a time limit, and a screenshot
is taken when it runs out.

In the full chain, trial D ran out of its time limit, with no screenshot
then. It installs, starts and removes its own three packages and needs
nothing from trials A to C, so it was run again alone from a fresh boot,
with screenshots during the run. It passed in 90 seconds.

A receiver left over from the 0.2 run was still listening on port 8765 and
took the first two files of this run. Nothing tracked was overwritten. The
driver now refuses to start if the port is taken.
