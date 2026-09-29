---
title: "Upgrade and rollback: a controlled package, then a real one"
date: 2026-09-25
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG
binary_sha256: 6dcf483579f3c0ab2996b58e65ed39870557f562e767f3735bbb4f272935e1c7
abi: v11
scripts: tests/upgrade.script (from tests/make-upgrade-fixtures.py), tests/upgrade-real.script (from tests/make-real-upgrade-fixture.py)
---

# Upgrade and rollback

`apkg upgrade <id>` replaces an installed package by the index's newer
revision of the **same upstream version**; `apkg rollback <id>` goes back to
the revision the last upgrade replaced. Scope of this increment: the
package's own drawer and its drawer icon. No `LIBS:`, no `Fonts:`, no
dependency resolution, no rule ordering one upstream version against
another: that is refused, not guessed.

Both files came off the guest by `putfile`; grading is `tools/grade.py`.
**Controlled package: 16 reports, 16 expectations, 61 PASS, 0 FAIL. Real
package: 4/4, 11 PASS, 0 FAIL.** One binary, `6dcf4835`, which is the one
with the reversible rollback bookkeeping; the run on `4e4c2664` before it
passed the same checks less the three added for that.

## The rules, and where each is proven

The planner (`src/libpkg/upgrade.c`) decides every path from three
inventories (the registry's, the target tree's, the drawer's) and is
host-tested on all its cases (`tests/test_upgrade.c`, 20 checks). On the
target:

| rule | evidence (`up-console.txt`) |
|---|---|
| the plan is explicit and printed before anything moves | `plan for SYS:PkgUp/up: 1 add, 1 replace, 1 remove, 0 kept, 2 unchanged` then `add NewFile / remove OldFile / replace Prog` |
| changed upstream, untouched here → replaced | `Up/Prog` hashes to revision 2 after the upgrade |
| dropped upstream → removed; added upstream → added | `Up/OldFile absent`, `Up/NewFile` present |
| a file the package did not change stays as the user edited it | `config.txt` keeps `cfg\nuser edit\n` through the upgrade **and** the rollback |
| a file the user changed that the new revision changes → refused, nothing touched | `apkg: a file you changed would be overwritten by the new revision … Prog`, status 7; every hash then still revision 1's, no transaction left |
| rollback restores the previous revision and protects edits made after the upgrade | `NewFile` edited after the upgrade → `keep (changed by the user) NewFile`; `Prog` and `OldFile` back to revision 1; the icon back |
| there is no rollback of a rollback | `apkg: nothing to roll back to`, status 4 |
| a different upstream version is refused | index says `2.0` against installed `1.0`: status 7, `no rule orders one upstream version against another yet` |
| recovery of an interrupted upgrade, at each state | `--interrupt-at 9` (old tree moved aside): `drawer moved back into place`; `10` (new tree published) and `5` (before commit): `drawer, icon and registry restored`; revision 1's hashes intact after each |
| recovery of an interrupted rollback | `--interrupt-at 10 rollback`: `rollback without commit -- rolling back … restored`; revision 2 intact; and `current archive kept`, `previous archive kept`, `previous registry entry kept`: what the rollback displaces waits under the transaction until commit |
| the revision is in the registry; the previous revision's entry and archive are kept for the way back | `info up` after the upgrade: `"revision": 2`, `"previous": { "registry": "SYS:PkgUp/db/previous/up.json", "archive": "SYS:PkgUp/cache/previous/up.zip" }` |

Then the real program (`sol-console.txt`): the published `soliton` archive
as revision 1 (the index states none, read as 0) and a repackaged copy as
revision 2: one file changed, one added, the program itself byte-identical.
`plan for SYS:PkgSol/soliton: 1 add, 1 replace, 0 remove, 0 kept, 32
unchanged`. Soliton was started before the upgrade, after it, and after the
rollback: `10` (a process number) each time.

## What the runs found before the last one

Two earlier runs are in `superseded/`, by binary:

1. `f3c817f9`: the rollback re-extracted the previous archive without the
   `subdir` cut the install had applied, because the registry did not record
   it; the tree came back with the archive's root layout and everything after
   cascaded. **The registry now records `subdir`.** Everything before that
   point (install, plan, upgrade, user-file protection, conflict refusal)
   had passed.
2. `754c6fd5`: 16/16 reports, 54 PASS, and four `config.txt` failures with
   one cause: after an upgrade the registry recorded the *result* tree, in
   which the user's edited config had been carried over, so a rollback read
   that edit as revision 2's own content and "replaced" it. **The registry
   now records what the package delivered** (the target tree's own hashes)
   while the transaction's plan keeps the result tree, because that is what
   an undo has to take back out. The recoveries in that run were already
   correct.

## Not remove-then-install

Both earlier findings are the reason the pair is not an upgrade. A removal
keeps the user's files and forgets whose they were; an install then refuses
the non-empty drawer. The swap keeps the drawer, the user's files and the
attribution, in one transaction with one commit bit.

## Limits, stated

- Same upstream version only. `1.0` → `2.0` needs a rule this document does
  not invent.
- A rollback needs the previous archive in the cache; an upgrade whose old
  archive was not cached says so (`rollback will not be available`).
- No `[[files]] kind` yet: "config" is what the hash says the user changed.
