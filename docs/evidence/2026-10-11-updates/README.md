---
title: "Updates model: apkg outdated, PkgManager Updates list, PkgUpdates"
date: 2026-10-11
branch: updates-model (worktree arospkg-wt-updates), base origin/main d50a8e0 (0.4.1)
---

# Updates model: what was run, where, and what it showed

## Builds

| target | toolchain / SDK | binaries | run? |
|---|---|---|---|
| x86_64 ABIv11 | `~/Work/AROS/toolchain` GCC 10.5, `~/Work/AROS/sdk` | apkg, PkgManager, PkgUpdates | yes, QEMU |
| x86_64 ABIv1 (mainline) | `/Volumes/arosmain/toolchain-mainline`, mainline SDK | apkg | yes, QEMU |
| aarch64 Raspberry Pi, ABIv1 | `~/Work/AROS/toolchains/raspi-aarch64` Clang 20, Pi SDK | apkg, PkgManager, PkgUpdates | **no**: built only |

Run 1 used the commits up to `b6467f8` plus the CLI wording in the working
tree; run 2 (v11) used the packaged binaries of commit `86303ef`
(`local/updates-20261011/dist`, see BUILD.txt there). The ScreenBar host
was the iteration 3 package `aros-screenbar-iteration3-x86_64-abiv11.zip`
(protocol 1, the same SDK files as the public checkout used for the
build). Limpet was the 0.2.1 release archive (SHA256SUMS verified).

## Tests

**Host (macOS):** `tests/run-host-tests.sh` passes, including
`test_updates.c` (every state of the model, counts, JSON parsed by Python),
`test_release.c` (SHA256SUMS reading, a target with no archive),
`test_rexxcmd.c` (OUTDATED parsing and answer), `test_hint.c` (root quoting),
`test_sha256.c` (read error is a failure).

**Fixtures** (`tests/make-updates-fixtures.py`): nine packages installed from
one catalogue, then a second catalogue gives each a different state:
available (new version; new revision with a conflicting user edit),
current, newer-installed, blocked (missing library), incomparable, not in
the catalogue, ambiguous, other target only. No network: archives sit in
the cache. Every step writes its own RESULTS: file with `rc=`.

### AROS One 1.3, pool slot v11-1, QEMU TCG, `-vga std` 1024x768

Run 1 (`v11-1-run1/`) and run 2 with the final binaries (`v11-1-run2/`):

- `outdated`, `--all`, `--plain`, `--json`, redirected to RESULTS: (FAT32):
  states and counts as expected (3 newer, 2 ready, 1 blocked); JSON parses;
  exit 0. No catalogue: exit 10, text and JSON say `missing`. Unreadable
  catalogue: exit 10. Extra argument: exit 5. A root with a space is quoted
  in every printed command (`space-root`).
- `--dry-run upgrade --all`, then `upgrade --all`: upa upgraded (user edit
  of config.txt kept, `hash-a-cfg` PASS), upb refused for a conflict (user
  edit of Prog intact, `hash-b-prog` PASS), the rest skipped with reasons;
  exit 10 because one failed. The model after upgrade, rollback and remove
  (`after-*`) changes as expected.
- `update` against the real catalogue (network) ends with the update count.
- `self-update --check` against the real release: up to date, with the
  PkgManager note. With a local SHA256SUMS that has no archive for the
  target: `--check` says so, exit 0; `self-update` refuses, exit 10.
- In Limpet 0.2.1: `outdated --all` in colour and wrapped (`screens/lim1`);
  output redirected from Limpet is complete (`ls/lo/lj.txt`); Ctrl-C during
  extraction (`--slow 1000 --progress`) was reported as too late and the
  install finished (`screens/lim5`).
- PkgManager (`gui*.log`, `screens/gui*`): `--updates` opens on "Updates (2)";
  catalogue line and states shown; Upgrade from the list leads to the
  existing preview (first without the archive in the cache: refused with
  the reason; then confirm) and the counts refresh to "Updates (1)"; ARexx
  `OUTDATED SHOW` from the widget refills the list; About > Check for a
  newer version: "0.4.1 is the latest stable release"; without ARexx
  (`--rexxlib none.library`) and without a catalogue: "Catalogue: none yet";
  Update index then refreshes the line and counts.
- PkgUpdates with ScreenBar (`sb*.log`, `screens/sb*`): item shows the
  model's count; action with PkgManager running sent OUTDATED SHOW (rc 0);
  with none running it started `PkgManager --updates --root "SYS:PkgUpd"`;
  ScreenBar stopped and restarted: the item registered again; after `apkg
  update` changed the catalogue the item recounted without fetching; Break
  ended it cleanly (unregistered).

### Mainline v1-20260923, pool slot v1-1, QEMU TCG

- Before the fix (`v1-1-before-fix/`): `apkg search >file` empty (also on
  RAM:), `--json outdated >file` empty, dry runs shortened; `show`, `list`,
  plain `outdated`, `--json search` complete.
- Probe (`v1-1-probe/`, `tests/stdout-probe/stdoutprobe.c`): fputs, fwrite,
  putchar lost; printf, puts kept; printf then fputs keeps only the printf;
  fputs then printf keeps both; fputs then fflush kept.
- After the fix (`v1-1-after-fix/`, commit `a9531d6`): every redirected
  output complete; the whole fixture script passes as on v11 (exit codes,
  hashes, JSON).

## 0.4.2 release runs (same day, after review)

A review of the branch found that `upgrade --all` went on after a Ctrl-C
that came while a package was changing files and then exited 0; that the
group's dry run and `--report` ignored failures; that PkgUpdates ran on the
caller's stack and waited synchronously for PkgManager's ARexx answer; and
that a failed scan read as an empty machine in PkgManager. All fixed
(commits d8ea3bb..bbefb2f), and `apkg update` became cancellable. The
archives in release 0.4.2 were built from bbefb2f with tools/make-rc.sh;
every run below used the archive's own binaries unless it says otherwise.

| run | system | binaries | what |
|---|---|---|---|
| `v11-2-rel042/` | AROS One 1.3, pool v11-2 | apkg/PkgManager of 20d5e09, PkgUpdates of b15be7e | updates suite; Ctrl-C in `upgrade --all`, in `apkg update` and in a package download; a network install (sdlpop); PkgManager Version column and Cancel during Update index; PkgUpdates with the real ScreenBar and `tests/holdport` holding the ARexx answer 60 s |
| `v1-1-rel042/` | mainline v1-20260923, pool v1-1 | apkg of bbefb2f | updates suite; `apkg update`; Ctrl-C during it (before bbefb2f it read "socket error 4", `screens/eintr-before-fix.png`) ; redirected `--all-abi search` |
| `v0-2-rel042/` | AROS i386 ABIv0, pool v0-2 | apkg of bbefb2f | updates suite; `apkg update`; Ctrl-C during a download; a network install (antiword) |
| `v11-1-rel042-final/` | AROS One 1.3, pool v11-1 | apkg and PkgManager of bbefb2f | updates suite; Ctrl-C in `apkg update`, in a download and in `upgrade --all`; PkgManager Updates (2), Upgrade from the list with preview and confirm, then Updates (1) |

PkgUpdates is unchanged between b15be7e and bbefb2f (same binary hash,
30d12fef...). What holdport showed: with the answer held 61 s the item stayed
on the strip and was never re-registered; a second click while waiting was
ignored; Ctrl-C while waiting left the request allocated after 10 s and the
program ended cleanly; holdport replied to it later and the system went on
working (`upd-r3-sb.log`).

In every suite run the counts were the same: 29 steps exit 0, 4 exit 10
(no catalogue twice, unreadable catalogue, `upgrade --all` with the
conflict), 1 exit 5 (usage), and all four hash checks PASS.

## What these tests did NOT show

- Nothing ran on a Raspberry Pi, real or emulated; the Pi build is checked
  only statically (ELF aarch64, imports identical to published 0.4.0 apkg).
  That the empty redirected `search` on the Pi has the cause measured on
  mainline is likely (same pre-fix stdcio) but not confirmed.
- The Pi checksum failure is not explained; `--keep-rejected` only collects
  evidence.
- Ctrl-C was pressed in the CON Shell for the 0.4.2 runs; in Limpet only
  the late case was tried, with the earlier build.
- PkgManager and PkgUpdates were not run on mainline v1 or i386 (neither
  is built for them). PkgUpdates is not in the release archives.
- Real network upgrades were not part of the fixture runs (archives from
  the cache); the real catalogue was only fetched.
- Nothing on real hardware; no performance measurement.
- Screenshots are PNG from `vmctl.py shot` (presentational conversions);
  the text results and logs are the evidence.

## Platform review

`platform-review.md` (same date): mainline and deadwood2 changes since
2026-09-20 that bear on this work. Most relevant: mainline `da9bbd01b5`
fixes the stdout loss above but is newer than our v1 base and the Pi image;
the ABIv11 e1000 heap fix is in no release, so network-heavy tests on v11
stay short.
