---
title: "v0.2 evidence: every suite, on one pair of binaries"
date: 2026-09-25
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG, persistent
apkg_sha256: a513aea7d8b066911a88466ac83d166f9c30578fb80768b0961d14df426040fc
pkgmanager_sha256: 34033ab6800dfd93fc4ea94da0e5a2ecb41df47a2ff98068c2d100338544e6df
first_freeze: apkg ba9a94d9…, PkgManager 346f3113… (every suite passed; withdrawn for the stack bug below; its files are in first-freeze-ba9a94d9/)
abi: v11
payload: tests/make-payload.sh (payloadV02full3.iso)
---

# v0.2 evidence

The whole v0.1 evidence set and the three v0.2 suites, re-run on the
binaries that `dist/arospkg.x86_64-aros-v11.zip` carries, in one sitting.
Every file came off the guest by `putfile` → `recv.py` with a SHA-256 both
ends agreed on. Grading is `tools/grade.py`: expectations from the scripts,
outcomes from the reports, a missing report counted as a failure.
Screenshots illustrate and prove nothing. `docs/spikes/gui4/` is the first freeze's window run, `gui5/` the counted one.

`a513aea7` is `apkg 0.2`; `34033ab6` is `PkgManager 0.2`. They differ from
the first freeze (`ba9a94d9`, `346f3113`) by one thing, `src/libpkg/bigstack.c`
(below); every suite had passed on the first pair and was run again on the
second. One other hash appears where it belongs: `37b2774a` is the 2026-09-12 binary, rebuilt from
commit `87708fc` by `tests/make-payload.sh` (byte-identical to the file in
that day's reports), and it writes the old-format registry entry that
`rel1g` removes.

| suite | what it covers | result |
|---|---|---|
| `up-*` | upgrade and rollback on a controlled package: plan, kept files, a refused conflict, every interruption point 9–10 with recovery, rollback bookkeeping, a version change refused; **run from a Shell set to `Stack 40960`** (`stack/regression-upgrade-suite-at-40960.png`) | **16/16, 61 PASS, 0 FAIL** |
| `sol-*` | the same on Soliton, revision 1 → 2 → back; the program started before, between and after | **4/4, 11 PASS, 0 FAIL**; `Status` shows Soliton alive at all three points |
| `vf-*` | `verify` on a pristine and an edited drawer; `--dry-run` for remove, upgrade without and with the archive, install over a leftover drawer; nothing changed, nothing recovered | **16/16, 16 PASS, 0 FAIL** |
| `json-*` | `--json` rows and the progress callback, three cancel outcomes | **7/7, 7 PASS, 0 FAIL** |
| `rel1-*` | drawer icon (6 cases), the three fixes, interruption points 1–8 with recovery, write faults, user data, recovery interrupted and resumed | **46/46, 64 PASS, 0 FAIL** |
| `rel2-*` | after a real reboot: the interrupted install rolled back; TLS refused for two wrong names, plain http and a missing CA bundle, with a positive control | **6/6, 8 PASS, 0 FAIL** |
| `rel1g-*` | a registry written by the 2026-09-12 binary, removed by this one | **4 PASS, 0 FAIL** (two binaries, by design) |
| `triala-*` | 30 candidates installed from AROS Archives over TLS | **30/30** |
| `trialb-console.txt` | each of the 30 started | 17 windowed programs behave exactly as in the v0.1 record (15 alive after 15 s, `isomaker` and `isotool` exit on their own); 13 shell tools ran |
| `trialc-*` | the same 30 removed | **30/30, 30 PASS, 0 FAIL**; root left with `db`, `cache`, `tmp` and the `untangle` drawer holding the game's own state file (see below; reproduced in both rounds) |
| `triald-*` | the three packages published before v0.1: install, start, remove | **6/6, 6 PASS, 0 FAIL**; all three alive after 15 s |
| `accept-*` | the README's own instructions, from the 0.2 release archive (`dist/arospkg.x86_64-aros-v11.zip`, `0bd1befa…`), against the published index | **3/3, 3 PASS, 0 FAIL** |
| `lha-*` | `hex2` fetched as an LHA; the Micropolis LHA installed, its icon placed, the game started | **4/4, 4 PASS, 0 FAIL**; Micropolis alive after 20 s |
| `gui-*` | `PkgManager`: install, upgrade, rollback, a refused conflict, an upgrade after the conflict is resolved, rollback unavailable once the previous archive is gone; Soliton started after each step | **complete** (fourth attempt; the earlier three are below): every press logged with its plan from the library (`add=1 replace=1`, rollback `replace=1 remove=1`, conflict `conflicts=1` with the file named, then `add=2` once resolved, then `can_rollback=0` with the missing archive named), registry at revision 2 at the end, Soliton alive after each of three starts, no process left after `quit`. Screenshots: `docs/spikes/gui5/` |

## Found by running, not by reading

**`apkg` overran a plain Shell's stack.** A peer's test of the first freeze
ran `apkg install` from a fresh Shell (40960 bytes) and it died before any
output. Reproduced here with `Stack 40960`: `ba9a94d9 install zaphod` ended
in a Software Failure requester (Illegal address access in `tlsf_freevec`,
the overrun having corrupted memory), and the guest stopped taking input a
minute later. Every suite in this directory had run from the Amiga+W Shell on
`onetest`, which reports 262144 bytes (on a fresh AROS One 1.3 pool slot the
same Shell reports 40960; why this machine differs is not yet found), so
none of them could see it. The
ABIv11 runtime has no `__stack` mechanism; both programs now run their main
through exec's `NewStackSwap` on 256 KB when the task has less. After a
fresh boot, same 40960-byte Shell, `a513aea7`: TLS install, LHA install from
the cache, two removals, all status 0 (`stack/`). From now on the first CLI
suite of a regression runs at `Stack 40960`.


**A file a program writes into its own drawer is kept on removal, and the
console does not say so.** Untangle, stopped with Ctrl-C in TRIALB, wrote
`Untangle.state` (44 bytes) into `SYS:PkgV01/untangle`. `apkg remove
untangle` deleted its six files, kept the one it had not installed, left the
drawer, and wrote `db/doctor/untangle-6ab6d2f9.json`; `apkg doctor` lists
that record. All of that is the designed behaviour. Two things are not
good enough: the console printed only `removed untangle`, and the record's
`kept` list holds only inventory files, so it says `"kept": [ ]` about a
drawer that still has a file in it. `untangle-left.txt` and
`untangle-doctor.json` in `trial-round1-…/` are the evidence. Not changed
in 0.2 (the binary is frozen); queued for 0.3: name unknown files kept, in
the console and in the record.

## Attempts that did not count, and why

Kept because each is a trap that the next run will meet again.

- `rel1-attempt1-stack-not-up/`: 43/46. The three runs that need the
  network failed with `network error` because the driver started the suite
  30 s after the reboot and AROSTCP was not up. The 2026-09-19 README
  already records this trap; the driver now waits 120 s after every reboot.
  Nothing in the binary changed between the attempts.
- `lha-attempt1-wrong-fixture-hash/`: `hex2` passed, Micropolis was refused
  as `network error` (the cached copy did not match the index, so `apkg`
  tried the placeholder URL). The fixture index had been re-hashed against
  a newer local Micropolis build instead of the archive the published
  manifest names (`c46bbc14…`, 2 593 505 bytes). Fixed in
  `tests/lha-index.json`; the payload is rebuilt from `.cache/archives/`.
- `trialc-attempt1-no-apkg-after-reboot/`: the driver rebooted after
  TRIALB (the v0.1 runs never did) and TRIALC found no `RAM:apkg`. Thirty
  `object not found` lines, nothing else.
- `trial-round1-reboot-between-b-and-c/`: TRIALA 30/30 and TRIALB as in
  the table above, then TRIALC after that same reboot: every removal
  answered `ok as expected`, but `RAM:trial/` was gone with the reboot, so
  no run report was written. The three trials were then run again in one
  go. This round found one thing worth keeping (below).
- `gui-attempt2-shell-commands-lost/` (re-frozen pair): install, upgrade
  and rollback complete, then the Shell lines that stage the second archive
  for the conflict step never ran, and both Upgrade presses ended in the
  correct refusal `cannot fetch the archive … cannot resolve
  example.invalid`. Right behaviour for the state the driver left; the
  conflict path itself was not exercised, so the flow ran again.
- `gui-attempt3-shell-lines-lost/` (re-frozen pair): the conflict refusal
  itself, with the file named, twice; the Shell line that clears the
  conflict did not run, so the upgrade after it and "rollback unavailable"
  were not reached. The driver now types every Shell line twice (each is
  idempotent), which is what the counted run used.
- Two boots out of about twelve came up with AROSTCP unable to resolve or
  connect to anything (`putfile: cannot connect to 10.0.2.2:8765`, `cannot
  resolve raw.githubusercontent.com`), with the socket library open. Both
  times a reboot cured it. A suite that ran on such a boot kept its files
  in `RAM:`; the counted window run's files were copied to `SYS:guiev/`
  and sent after a reboot.
- `gui-attempt1-lost-click/` (first freeze): install, upgrade, rollback and the refused
  conflict all behaved, then the Upgrade press after the conflict was
  resolved never reached the button. The window logs every press and none
  was logged; the pointer sat over the button in the screenshot. A lost
  injected click, not a program fault, and not evidence either way: the
  whole flow was run again.
