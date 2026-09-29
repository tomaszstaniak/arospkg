---
title: "Where arospkg stands"
status: living
updated: 2026-09-29
---

# Where arospkg stands

> **2026-09-29: 0.3 released.** apkg `40c2ba93`, PkgManager `8345cec7`,
> archive `887b7536`. Every suite passed on this pair, and it added the ARexx
> port and `apkg show`. Evidence:
> [`reports/2026-09-29-v03/`](reports/2026-09-29-v03/README.md). A build
> from the release commit gives the same two binaries. After the freeze only
> documentation and host tools changed: the packaging guide, the metadata
> reference and the RFCs, and a stricter `tools/mkindex.py`
> ([`reports/2026-09-29-final-index/`](reports/2026-09-29-final-index/README.md)).
> The approved manifests now live only in arospkg-index.
>
> **2026-09-19: settled. v0.1 targets ABIv11 (AROS One x86_64).** Not mainline.
> That is where the software is: 171 ABIv11 candidates against 7 for mainline.
> `src/build.sh` builds ABIv11 and writes `apkg`; the mainline build moved to
> `src/build-mainline.sh`, is still maintained, and does not gate a release.
> The first release is the command-line client; a graphical front end is the
> next stage, with no date attached. See
> [`release/v0.1-plan.md`](release/v0.1-plan.md).
>
> **2026-09-11, now historical: the scope question that this answered.** `-v11` in an archive filename marks **ABIv11**, for distributions like
> AROS One. Unmarked means **ABIv1**, which is mainline. We target mainline and
> our importer had selected 168 ABIv11 packages: the ones that install
> perfectly and crash on startup. Filtered correctly, the entire catalogue
> offers **seven** packages for mainline on our architectures, of which one
> x86_64 application. The MVP's twenty-package criterion cannot be met on the
> target we chose. See [`spikes/do-they-run/the-answer.md`](spikes/do-they-run/the-answer.md).

A single page answering one question: **how far is this from something a person
could actually use?** Kept honest rather than encouraging.

## The end-to-end chain

What has to happen for someone to go from wanting software to having it
installed, and where each step is:

| # | step | state |
|---|---|---|
| 1 | find out what exists | **done**: `apkg search` over id, summary and category |
| 2 | get the index onto the machine | **done**: `apkg update` fetches it from the public index repository |
| 3 | download the archive | **done**: `apkg install` fetches over TLS, follows cross-host redirects, verifies size and SHA-256, and only then publishes into the cache |
| 4 | verify it | **done**: SHA-256 and size checked against the index before anything is unpacked |
| 5 | unpack it safely | **done**: own ZIP **and LHA** readers, picked by the file's first bytes rather than its name, every member validated first, including the AROS rule that a `:` anywhere is an absolute path. A real upload in the catalogue (`dclock`) trips exactly that rule and is refused |
| 6 | record what was installed | **done**: per-file inventory with hashes in `db/installed/<id>.json` |
| 7a | check what the **machine** must provide | **done**: `requires_system`, checked before the download, with satisfied / missing / undetermined; one real package (`sdllopan`) installs and runs on AROS One on the strength of it |
| 7b | resolve **package** dependencies | **deferred, not missing**: `mkindex.py` closes them over the index; the client does not resolve them because no package-to-package dependency was found in 32 of 173 sampled candidates. See [`spikes/dependencies/README.md`](spikes/dependencies/README.md) for what that claim covers and what it does not |
| 8 | install files outside the package directory | **deferred**: `LIBS:`, `Fonts:` designed, not built. Of 30 sampled ABIv11 archives none writes to a system directory, so this blocks nothing in the tested set; the archives' *layout* is not proof, so each candidate's documentation was read too |
| 9 | give it a Workbench icon | **done**: `<root>/<id>.info` is installed, recorded, protected if it was already there or has been changed, removed on uninstall and reconciled by recovery |
| 10 | remove it without destroying user files | **done**: hashes on disk first, modified and unknown files kept, leftovers reported |
| 11 | survive an interrupted operation | **done**: journal, plan-driven recovery, measured at every interruption point the code defines |
| 12 | diagnose a stuck root | **done, minimal**: `apkg doctor` and `doctor --retry` |
| 13 | upgrade | **done, 2026-09-25**: same upstream version, higher revision; a printed plan, changed files protected, conflicts refused before any change, rollback to the previous revision, recovery at every state. Remove-then-install is still **not** a substitute: removal keeps files you changed, and an install over a directory holding them is refused, with a message saying why (tested) |
| 14 | a GUI | **done, 2026-09-23/25**: `PkgManager`: list, filters, details, install, upgrade, roll back, remove, progress and cancel, on a worker process through the same library |
| 15 | verify and dry run | **done, 2026-09-25**: `apkg verify` classifies each installed file against the registry (ok, changed locally, missing, not the package's) and repairs nothing; `--dry-run` prints the same plan the operation would execute, changes nothing, recovers nothing, and says when the plan is incomplete for want of the archive (`--fetch` allows that download only) |
| 16 | scripting | **done, 2026-09-27, unreleased**: an ARexx port in `PkgManager` (`PKGMANAGER`): queries, SHOW, and install, remove, update, upgrade and rollback as numbered jobs on the window's own worker, with WAIT, CANCEL where libpkg allows it, and confirmation kept in the window. See [`arexx.md`](arexx.md) |
| 17 | describe one package | **done, 2026-09-28, unreleased** -- `apkg show <id>`: what it is, its target, whether it runs here and why, its requirements probed now, download and hash, source and licence when given, what is installed and whether it can be upgraded or rolled back. It changes nothing. The same verdict is in the window's panel and in ARexx `INFO` (`runs`). One rule picks which of several targets for an id a command means, for install, upgrade, requirements and show alike |

The dangerous half (verify, unpack, record, remove, recover, diagnose) has
**passed the interruption tests described below on one package**. That is a
narrower guarantee than "done", and the earlier wording overstated it: one
package with no dependencies that writes nothing outside its own directory is
the easy case, and controlled interruptions are not crashes.

## What is actually proven

Measured on mainline AROS x86_64 in QEMU, with run reports carrying the
binary's own SHA-256, the toolchain and the SDK:

- install, list, remove of a real package from the real archive;
- removal keeping a file the user edited (49 removed, 1 kept), with a report;
- interruption at all six points the code defines, each followed by recovery;
- recovery interrupted and finished by a plain re-run;
- `005-commit` refused → transaction kept, no false success;
- the leftovers report refused → registry kept, nothing unattributable;
- an interrupted publish leaving a file nobody registered **untouched**.

On AROS One (ABIv11), 2026-09-12, in `docs/reports/2026-09-12-requires-system-onetest.md`:

- `sdllopan` installed with its three system requirements checked against the
  live machine and found satisfied, then **started and drew its board**. The
  first package this project installed whose requirements were verified rather
  than assumed;
- a requirement no machine provides **refused before the download**, naming it,
  on a machine of the right ABI; so the ABI gate cannot be what refused it;
- a requirement the client cannot decide **installed anyway**, warned, and left
  `"state":"undetermined"` in the registry entry;
- a control requirement every AROS meets, so the refusal above is not the field
  refusing everything;
- removal, with the system libraries still in place afterwards.

## Rough edges found by reading the output as a stranger would

Collecting verbatim output for the project page surfaced three things that
reviewing the code never did:

- **`install <unknown>` printed the id twice**, because it was passed as both
  the message subject and its detail. Fixed: the detail now says the index only
  lists reviewed packages and points at `apkg search`.
- **`apkg list` does not sort.** It prints the registry directory in whatever
  order the filesystem returns, so the output looks alphabetical by accident
  and is not. A list a person reads should be sorted; not fixed yet.
- **A corrupted archive in the cache blocks that package until the file is
  deleted by hand.** `apkg` refuses every attempt and does not discard it and
  re-fetch. Refusing is right; not recovering is a rough edge, and the honest
  behaviour would be to discard a cached file that fails its hash and download
  it again. Not fixed yet.

## What is claimed and not proven

- **aarch64.** Everything runs on x86_64. The two aarch64 candidates have never
  been fetched, let alone installed.
- **Power loss**, and a real process kill. Every interruption so far is a
  controlled return from a function.
- **Two packages installed and removed cleanly**, not one: `sdlpop` was
  installed cold from the index and removed on 2026-09-10, and it is a much
  better exercise than zaphod -- **1074 files** against zaphod's 50, each one
  hashed into the registry and hashed again on the way out. Interruption and
  recovery, though, have still only been exercised on `zaphod`.
- **Neither has dependencies**, and neither writes anything outside its own
  directory. Both are still the easy case in the ways that matter to the
  unbuilt parts.
- **That any installed package RUNS.** Measured 2026-09-10 and the answer so
  far is no: **eight archive packages across eight categories**, two compilers,
  install cleanly and then crash on startup. **They crash the same way on
  the official AROS nightly, which we did not build, while a program built with
  our own SDK runs there, and the very same binaries **run on AROS One**,
  which puts the compatibility boundary between the distribution and mainline**, in the way the platform notes describe as an ABI mismatch between
  a distribution build and mainline. See
  [`../docs/spikes/do-they-run/README.md`](spikes/do-they-run/README.md). The
  installer is not implicated; what the catalogue's binaries are built for is.
- **The catalogue at large.** 2 of 168 candidates are approved; the four-package
  sample suggests roughly half of ordinary uploads need work or cannot be
  expressed at all.

## End-to-end, measured 2026-09-10

**Acceptance run: a clean root, no index placed by hand, an empty cache, the
system CA store**, on mainline x86_64. Every step gated with `--expect`:

```
apkg update            -> ok     index fetched from arospkg-index
apkg search zaphod     -> ok     zaphod 1.3 x86_64  A simple binary file editor
apkg install zaphod    -> ok     fetched over TLS, redirect followed, verified
apkg list              -> ok     zaphod
apkg remove zaphod     -> ok     removed
apkg list              -> ok     nothing installed
```

with the failure paths measured in the same run: a 404 reported as a network
error (12), a 200 whose body is not an index reported as invalid content (9),
and in both cases the good index left in place and `search` still working.

`SYS:PkgE2E/cache/zaphod.zip` is 218782 bytes, put there by `apkg` itself. The
run report carries the binary's own SHA-256, `c842e242...3727c153`, which
matches the file on the Mac.

`apkg update` was pointed at a URL that is reachable but is not an index: it
fetched it, refused the contents, and left the existing index in place:
`"the downloaded index is not a valid index"`.

**The index is published**, since 2026-09-10:
[`tomaszstaniak/arospkg-index`](https://github.com/tomaszstaniak/arospkg-index),
public, holding the generated index and the approved manifests and nothing
else. The code stays private, the binaries stay in AROS Archives, and the
client carries no access token. The 166 unreviewed skeletons were deliberately
**not** published: a candidate is not an installable package, and a list of
things that fail to install is worse than a short list that works.

Two failure modes were separated before any of this is exposed, because they
send a reader to completely different places:

```
404 from the server        -> status 12 "network error"
                              "the index could not be fetched"
200 whose body is not JSON -> status  9 "i/o error"
                              "the downloaded index is not a valid index"
```

In both cases the index already on disk is untouched. A 404's error page never
reaches the parser, so "the server does not have this" can no longer be
reported as "your index is corrupt".

After that, in order of how much they block real packages: dependency
resolution (7), external files (8), the drawer icon (9). Upgrade (13) and the
GUI (14) follow.

## The current cost

`apkg` is **8.0 MB**, up from 155 KB, because that is what TLS costs here:
97.9% of the binary is OpenSSL, measured by relinking without it
([`spikes/abiv11-build/README.md`](spikes/abiv11-build/README.md)). Note which
way round the two targets sit: **mainline carries the newer OpenSSL (4.0.1)
and is therefore the bigger build**; the ABIv11 SDK's 1.1.0h from 2018 is what
makes that build 3.5 MB. The
decision was to take it and finish the first full run rather than open a
shared-library rebuild, which would bring its own ABI, its own installation and
its own updating.

Worth stating plainly, because an earlier version of this file implied
otherwise: **a static `libpkg` shares source, not linkage.** When `PkgManager`
arrives it will link its own copy of OpenSSL, and the two binaries will each
carry ~7.8 MB. Architecture A buys one implementation of the logic; it does not
buy one copy of the library. If that duplication matters, the answer is a
shared library, and that is a measurement to take when there is a GUI to
measure.

## What comes next, in this order

Re-planned 2026-09-22 after v0.1 was tagged and jonx's `pkg` was reviewed
(`docs/reports/2026-09-23-rows/README.md` has the comparison's outcome).
The 2026-09-10 list below it is kept as history: its first item is done and
its second and third are now items 4 and 5 here.

1. **Rows and progress: done 2026-09-23.** `pkg_query`/`pkg_installed`
   return fixed-size rows; `apkg search` is formatted from them, so no front
   end can read a different index. `pkg_set_progress` reports download,
   verify, extract and publish; cancelling is honoured only where it costs
   nothing. `--json` prints the rows. Verified on AROS One, including all
   three cancel outcomes.
2. **`apkg upgrade` + `rollback`: done 2026-09-25.** Same upstream
   version, higher revision; an explicit printed plan; the user's changed and
   unknown files kept, a conflict refused before any change; rollback from
   the kept previous archive and registry entry, protecting edits made after
   the upgrade; recovery of either operation interrupted at any state. On a
   controlled package and on soliton: `docs/reports/2026-09-25-upgrade/`.
   **In the window too, the same day:** Upgrade and Roll back buttons that
   follow the library's answers, the plan shown as data and confirmed, a
   refused conflict and a missing rollback archive both explained in the
   panel: `docs/reports/2026-09-25-gui-upgrade/`. Not in the v0.1 archive.
3. **`verify` and `dry-run`: done 2026-09-25.** Entry points to code that
   existed: the per-file hashes recorded at install, the planner behind
   install, remove, upgrade and rollback. `verify` reports and repairs
   nothing; `--dry-run` never fetches unless told (`--fetch`), never runs
   recovery, and says when the plan is incomplete. Taken from jonx's command
   set, because the reasons hold here too. Measured on AROS One:
   `docs/reports/2026-09-25-v02/`.
4. **A Zune front end: second increment done 2026-09-24.** Category and
   state filters, a detail panel with the requirements probed on the machine,
   search and selection kept across operations, closing the window during a
   job handled in both phases, a concurrent CLI attempt refused by the lock,
   and a slowed transfer (`--slow`) so none of it depends on timing luck:
   `docs/reports/2026-09-24-gui2/`. First increment 2026-09-23: `PkgManager`
   (`src/pkgmanager/`, `src/build-gui.sh`): list, search, details, Update
   index, Install, Remove, a gauge and a Cancel button that follows
   `can_cancel`, with the work on a worker process of its own through
   `libpkg`. Verified on AROS One by driving the window from the host:
   `docs/reports/2026-09-23-gui/`. Not in the v0.1 release archive.
   **The first 0.2 freeze (apkg `ba9a94d9`) did not hold.** Every suite
   passed on it, and then a separate test from a plain Shell crashed
   `apkg install` before any output: a plain Shell gives a program 40960
   bytes of stack, the Shell every suite here ran from (on the test machine
   `onetest`) reports 262144, and a TLS download needs more than the former. The ABIv11 runtime has no
   `__stack` mechanism, so both programs now run their main through exec's
   `NewStackSwap` on 256 KB when the task has less (`src/libpkg/bigstack.c`),
   and the first CLI suite of every regression now runs at `Stack 40960`.
   The re-frozen pair and the full re-run: `docs/reports/2026-09-25-v02/`.
   Publication is a separate decision.
5. **`install.external` to `LIBS:`/`Fonts:`/`SYS:`**, author-declared per
   file, never a global switch: backup of what was there, ownership of
   shared files, library version never downgraded, honesty about a library
   already in memory. v0.3-sized.

6. **ARexx: done 2026-09-27, in 0.3.** PkgManager serves
   the port `PKGMANAGER` from its own event loop, with its own dispatcher
   (Zune's answers unknown commands with RC 0 and matches prefixes).
   Scripts can do the following:
   - query the catalogue and the installed packages, and probe a package's
     requirements;
   - bring a package up in the window;
   - start update, install, remove, upgrade and rollback. Each gets a job
     number; `JOB` and `WAIT` read its state and outcome, and `CANCEL` is
     honoured only while libpkg reports `can_cancel`.
   The jobs run on the same worker the buttons use, under the same rules:
   one at a time (`BUSY`), the root lock, conflicts, recovery. An upgrade or
   a rollback still waits for Proceed in a confirmation window. That window
   replaced the modal requester, which would have stopped the port from
   answering while it was open.
   Two platform facts shaped the contract. Current ABIv11 Regina puts RESULT's
   text into RC. A host that sets RC2 with `SetRexxVar` crashes RexxMast once
   scripts call it from inside a PROCEDURE. So no variable is ever set in the
   caller; `LASTERROR` gives the reason, per client
   (`docs/spikes/arexx/README.md`).
   A review of the first build found a real ordering bug: the window
   counted a job as finished when the worker's flag said so, before its
   outcome had been read, so a new job could start in between and be handed
   the old outcome. The window now counts a job as running until the outcome
   is recorded; worker messages carry the job's number; the final message is
   allocated before the work. A build with the old busy test fails the test
   written for it.
   A second review found two more problems. The worker ran one priority
   below the window, so a script polling without a pause starved it, and the
   install never finished. And `NOMEM` could arrive for a job already
   started. Now the worker runs at the window's priority, the port takes a
   few commands per turn, and a command that changes something allocates
   its answer before it acts. The previous build fails the polling test.
   On AROS One, binary `7fdb7c80`: 194 checks made by the guest pass, and 29
   more made on the host against the window's own logs. Among them:
   - jobs sent back to back;
   - a script polling without a pause while the window handles a search;
   - forced allocation failures;
   - a full `UPDATE`.
   The five examples in `examples/arexx/` run as documented:
   `docs/reports/2026-09-27-arexx/`. Host tests cover the parser and the
   reply formats (`tests/test_rexxcmd.c`). Not done: mainline (ABIv1),
   aarch64, starting PkgManager on demand, SDK or shared library, Lua.

7. **`apkg show` -- done 2026-09-28, in 0.3.** See row 17. Found
   on the way: with two targets for one id in the index, install and the
   requirement probe took whichever came first, while `show` described the
   native one. All four now choose through one function, and the show run
   installs `rescode` with its i386 entry listed first. Report:
   `docs/reports/2026-09-28-show/`.
   Before the freeze, a review asked for four things, and all are in:
   - compatibility and requirements shown as separate verdicts, where
     compatibility is not a claim that the program runs;
   - a refusal before download when no build is for this machine, and when
     more than one is;
   - an entry choice that does not depend on the index's order, and an
     upgrade that stays on the installed target;
   - `show` proven to change no file's content.
8. **ABIv0 programs through EmuV0 -- measured, not usable yet.** AROS One
   1.3 ships EmuV0 1.10 configured, with 27 32-bit libraries. On our test
   machine (QEMU TCG), every program tried failed in the emulator's start.
   That covered both 1.10 and 1.12, and three programs: a Shell program that
   only prints, Sudoku and ResCode. The configuration is recorded as
   unsupported for now. arospkg already keeps a package's target apart from
   the way it would run, and an ABIv0 entry reads "undetermined", never
   native. Everything measured, and what it means for the client:
   `docs/spikes/emuv0/README.md`.
9. **System components and distribution upgrades -- a placeholder only**:
   `docs/backlog/system-components.md`.

Held: index signing (needs a key-management document first; jonx has
solved it once for packages and is worth talking to), own hosting.

Two small things for 0.3, found on the day 0.2 was frozen: `remove` says
nothing on the console about a file it kept because it never installed it
(Untangle writes `Untangle.state` into its drawer), and the leftovers record
lists inventory files only, so it names no file; and a registry entry with no
revision (only the test machines have any: 0.1 was never published) compares
as older than an index entry stating `revision = 1`, which the manifest guide calls the first release, so the
planner offers an upgrade to the identical archive. Absent and 1 should
compare equal.

### The 2026-09-10 list, historical

1. **Run the second approved package, and start it.** Done: every package in
   the index has been installed, started and removed on AROS One.
2. **One application with one available dependency.** Not yet; see 5.
3. **Writing to `LIBS:`, separately.** See 5.

### A rule for the public index

**`arospkg-index` carries only what the current client can handle.** A manifest
for a package with dependencies does not go in it until dependencies work; one
that installs into `LIBS:` waits for external files. Otherwise the index starts
advertising behaviour the client does not have, and the first person to try it
gets a failure that looks like a broken package rather than an unfinished
manager.

Harder cases live as **local test manifests** until then. They are exactly as
useful for testing there, and they promise nothing to anyone else.

### The project page

Being drafted at `~/Work/tomaszstaniak.com`, in the manner of the `aros-term`
page but not a copy of it. Framing decided 2026-09-10: arospkg is presented as
unreleased work in progress with **no download offered**, its purpose stated as
intent, and the "works today" tier claiming only what has been measured: update,
search, verified download, validation, install, registry, safe removal, recovery,
doctor. No sentence claims an installed program runs, and the install animation
ends at `installed zaphod`.

The launch crashes are not featured, on the reasoning that nothing is published
so nobody is affected. **That reasoning is load-bearing and expires.** The
omission is safe only while there is no download; the moment a release exists a
reader can act on the page, and the launch behaviour has to be settled or stated
before then. That condition is recorded with the site's own provenance
rules.

GUI and upgrade are not part of this round.
