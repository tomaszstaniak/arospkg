---
title: "v0.1 evidence: what was run, on which binary, with what result"
date: 2026-09-19/20, re-run 2026-09-22 after the rename to apkg
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG, persistent
binary_sha256: ca0104403dc50394686c9afc29180e74d6ea348948a9b612632733e81d3466f1
source_id: b8f2d2ca5fa9d72a
abi: v11
---

# v0.1 evidence

Every file here came off the guest over the network (`tests/tools/putfile` →
`tests/tools/recv.py`), with a SHA-256 both ends agreed on. Screenshots are in
`docs/spikes/`; they illustrate and prove nothing. Grading is by
`tools/grade.py`, which reads the expectations out of the scripts and the
outcomes out of the reports, and counts a missing report as a failure.

**All results below are from one binary, `ca010440`,** which is the `apkg` in
`dist/arospkg.x86_64-aros-v11.zip` (`53a4956d`). Runs made on earlier binaries
are kept in `superseded/`, named by binary, because two of them found bugs and
the history is worth more than the tidiness. `superseded/68bdffe5/` is the
complete set from 2026-09-20, the last binary called `pkg`; the command was
renamed to `apkg` on 2026-09-22 (which changes the help text, hence the
binary, hence this re-run) and nothing else in the code changed between them.

| suite | what it covers | result |
|---|---|---|
| `rel1-*` | drawer icon (6 cases), the three fixes, every interruption point 1–8 with recovery, write faults, user data, recovery interrupted and resumed, an empty drawer | **46/46 reports, 64 PASS, 0 FAIL** |
| `rel2-*` | after a real reboot: the interrupted install rolled back; TLS refused for two wrong names, plain http and a missing CA bundle, with a positive control | **6/6, 8 PASS, 0 FAIL** |
| `rel1g-*` | a registry written by the 2026-09-12 binary, removed by this one | **4 PASS, 0 FAIL** |
| `triala-*` | 30 candidates installed from AROS Archives over TLS | **30/30** |
| `trialc-*` | the same 30 removed; root left with only `db`, `cache`, `tmp` | **30/30** |
| `triald-*` | the three packages published before today: install, start, remove | **6/6** |
| `accept-*` | the README's own instructions, from the release archive, against the published index | **3/3, 0 FAIL** |
| `lha-*` | LHA on the target: `hex2` fetched from AROS Archives as an LHA and installed; our own Micropolis LHA installed, its drawer icon placed, `.arospkg/` kept out, and the game started | **4/4, 0 FAIL** |
| `trialb-console-binary-cb4aa189.txt` | each of the 30 candidates started | 17 windowed programs alive after 15 s; 13 shell tools ran, 10 with captured output |

Two binary hashes appear across these reports and both belong there.
`ca010440` is the release binary. `37b2774a` appears only in `rel1g-*`: it is
the 2026-09-12 binary, rebuilt from its commit to write a registry entry in the
old format, which the release binary then removes. Its rebuild came out
byte-identical to the file recorded in that day's reports, a week later.

`trialb` is the one file from another binary, deliberately: it starts programs
that are already on disk and does not exercise `apkg` at all. The files it
starts were put there by `apkg` and verified by hash.

## Things the runs found that reading had not

1. **Recovery lost the leftovers report.** When a removal was refused because
   its report of kept files could not be written, the next run's recovery
   rolled the removal forward and deleted the registry without writing that
   report, so a file the user had changed stayed on disk with nothing
   recording whose it was. Fixed; `f2x` in `rel1` covers it.
2. **The icon extraction refused five of the first thirty packages.** The
   single-member extraction applied its 1 MB limit to every member of the
   archive, so any package with a file over 1 MB failed. Fixed, with a host
   test (`tests/test_zip.c`) built from an archive shaped like those five.
3. **The icon extraction refused any archive holding a file over 1 MB**: see
   above; and once LHA arrived, a fourth: the unary tail in LHA's code-length
   table needs its terminating zero consumed. Without it the first members of
   an archive decoded perfectly and the first file with a long code walked off
   the rails. Caught by comparing against the system `lha`, not by reading.

4. **An installed `pkg` could not hash itself.** Run as `pkg` from `C:` (the command's name at the time), the
   way the README tells a user to install it: `GetProgramName()` returns a
   bare name, so every run report said `binary unknown`. That is the field this
   project's evidence rule depends on. Fixed via `PROGDIR:`; found by the
   acceptance run, the first test that installed pkg the documented way.

## And three faults in the method, not the code

Recorded because each produced a confident wrong answer, or nearly did.

- `Execute >file script` does not capture a script's output on AROS, and
  neither does `Run >file Execute`. The first two release runs produced empty
  transcripts. Every command now appends to the log itself.
- The list-sort test first installed packages in the one order where sorted
  and filesystem order agree, so it could not have failed. Fixed before it
  counted for anything.
- The payload CD grew to 14 files in one directory. This repository's own notes
  say a thirteenth file in one CD directory hangs AROS; it did, and cost a
  wedged guest and a lost run. The payload is now split across directories.
- AROSTCP comes up asynchronously after a boot, and the driver started trial A
  before it had (2026-09-22). Every download was refused, `putfile` could not
  send the reports, and the driver waited on a file that would never arrive.
  The 2026-09-20 run had simply been lucky. The driver now waits 75 s after
  every reboot; the acceptance script had already waited on its own.
