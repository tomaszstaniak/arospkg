---
title: "First working run: install, registry, remove, recover"
status: complete
state: measured
created: 2026-09-08
updated: 2026-09-08
---

# First working run

The point at which arospkg stopped being a design. Run on **mainline AROS
x86_64 in QEMU**, 2026-09-08, with `pkg` built from `src/` against the mainline
SDK.

## What was run, and what happened

```
RAM:pkg --root RAM:Packages list
  nothing installed

RAM:pkg --root RAM:Packages --index "Qemu Vvfat:index.json" install zaphod
  installed zaphod

RAM:pkg --root RAM:Packages list
  zaphod

List RAM:Packages/zaphod
  AUTHORS 1645, Catalogs/, ChangeLog 921, COPYING 17982, Docs/, fonts/,
  icons/, locale/, Makefile.aos4, Makefile.aros, Makefile.mos, README 592,
  src/, Zaphod 230872, Zaphod.info 11957
  9 files - 6 directories

RAM:pkg --root RAM:Packages remove zaphod
  removed zaphod
```

The archive came from `cache/`, was verified against the `sha256` in
`index/index.json` before anything was unpacked, and was extracted by the
library's own ZIP reader. `Zaphod.info` is there, so the package is visible in
Workbench.

## The interruption test, which is the part that matters

`--interrupt-at 4` stops after the registry is written and **before the commit
marker exists**: the state section 2 spends most of its length on.

```
RAM:pkg ... --interrupt-at 4 install zaphod
  pkg: stopped on purpose
       zaphod
       PKG_STOP_AFTER_REGISTRY

RAM:pkg --root RAM:Packages list
  zaphod                                  <- the machine LOOKS installed

List RAM:Packages/db/transactions
  6aa05ed0-zaphod   Dir                   <- but nothing committed
```

The next mutating command finds it:

```
RAM:pkg --root RAM:Packages remove zaphod
  recovery: 1 transaction(s) to resolve
    6aa05ed0-zaphod: install without commit -- rolling back
        50 file(s) removed, 0 left because changed
  recovered 1 interrupted transaction(s)
  pkg: not installed

RAM:pkg --root RAM:Packages list
  nothing installed

List RAM:Packages
  db, cache, tmp                          <- the package directory is gone
```

Three things this actually demonstrates, rather than asserts:

- **A read-only command does not recover.** `list` opened the root read-only,
  took no lock, and reported what was on disk: a package that was never
  committed. That is correct behaviour and it is what makes the inconsistent
  state visible instead of quietly repaired.
- **Recovery is plan-driven.** The rollback deleted 50 files by matching each
  one's hash against the inventory in `000-plan.json`, not by trusting markers.
  The plan could carry that inventory because staging happens before the
  journal is written.
- **Rollback left nothing behind.** `0 left because changed`, and had a file
  differed, it would have been kept and the transaction left unresolved.

## One bug, found by running it

The first attempt failed with `cannot write or re-read the plan` and, correctly,
`Nothing outside staging has been touched.` The plan is closed, reopened and
re-parsed before any mutation; that re-parse used a **fixed 64-token buffer**,
and a plan carrying one object per installed file needs hundreds. ZapHod has 50.

It failed safely, which is the design working, but it would have failed for
every real package, and no amount of review had caught it. Fixed in
`core.c:txn_write_plan`.

## What this is not

- **No networking.** The archive must already be in `cache/`; `pkg` does not
  download. The transport exists (`../network-download/`) but is not wired in.
- **No external files.** Nothing is installed to `LIBS:` or `Fonts:` yet, so
  the ownership table, `_pre` backups and the drawer-icon rule are unexercised.
- **No dependency resolution.** `zaphod` has none, which is why it was chosen.
- **One interruption point tested of six.** `--interrupt-at` offers 1–6; only 4
  has been run. Crash injection at every stage, in both directions, remains
  owed.
- **Power loss is untested**, and is a different failure from a killed process.
- Everything ran with the root in `RAM:`, so nothing was tested across a reboot.

## Stabilisation run (2026-09-09)

Same package, this time on a **persistent volume** (`SYS:PkgTest`, not `RAM:`),
driven by `tests/run-on-aros.script`, with every step writing a machine-readable
report to `SYS:PkgReports/` carrying a **run id** and the **build id**. That is
this repo's evidence rule, met properly for the first time: the build id is the
SHA-256 of the sources, compiled in, so a result cannot be attributed to the
wrong binary.

Build `9f48b58ee678abeb`. 25 reports.

| runs | what was checked | result |
|---|---|---|
| r01 | clean install | `ok` |
| r02 | remove with a user-modified file | `ok`, 49 removed, **1 kept** |
| r03, r03b | reinstall and remove | `ok` |
| i1–i5 | interruption at each of the five install points | `interrupted on purpose` |
| i1r–i5r | read-only view after each | `ok`, and the pending-transaction warning |
| i1x, i1y | install and remove after recovery | `ok` |
| b0–b4 | **recovery itself interrupted**, then re-run | rolled forward on the next run |
| c0–c3 | remove interrupted after files are deleted | rolled **forward**, as `remove` must |

The modified-file case produced exactly the doctor report section 2 requires:

```json
{ "package":"zaphod", "when":"2026-09-09T10:44:43Z", "dir":"SYS:PkgTest/zaphod",
  "removed":49, "kept_modified":1, "kept": ["README"] }
```

### Three fixes this run forced

- **`005-commit` was written without checking.** `txn_marker()` ignored both
  `fopen` and `fclose` failures, and install then deleted the journal and
  reported success. For the audit markers that is a small risk; for the marker
  that *decides* recovery it means a finished install can be rolled back later.
  It now returns a status, and a failed commit keeps the transaction.
- **The doctor report was optional in the code.** A serialisation or write
  failure did not stop the registry being deleted, which is exactly how files
  become unattributable. It is now a required step: the removal aborts with the
  registry intact.
- **A read-only view showed a half-finished transaction as settled.** `list`
  correctly does not recover, but printing `zaphod` alone hid that nothing had
  committed. It now warns, without taking a lock or recovering.

### Two bugs the run itself found

- **A failed `pkg_open` wrote no report**, so the one run that mattered most,
  the deliberately broken recovery, left no record. It was visible only as a
  missing file in a directory listing. Reporting now happens on every exit path.
- **The doctor report was not valid JSON.** `"kept": [, "README" ]`: the array
  separator counted the inventory index instead of the number of items emitted,
  so any kept file that was not the first entry produced a leading comma. In a
  report whose whole purpose is to be machine-read.

And one about the harness rather than the code: the first attempt failed at
every install with `not found`, because the script copied the archive into
`SYS:PkgTest/cache` before `pkg` had created that directory. The reports said
so in one line each. A screenshot would have taken far longer to read, which is
the argument for the evidence rule stated as a measurement rather than a
principle.

### Guarded publish window (2026-09-09)

The publish test was strengthened after review: reaching the window is not the
interesting part, what happens to content nobody registered is.

`--expect <status>` now gates every fault case, so a run passes only if it ends
where it should. That exists because the first version of F3 **produced a
report, with a status, in the right place, and had tested nothing**: it never
reached the window at all. And `pkg assert-hash <file> <sha256>` lets the
harness check a file it had no business touching is still byte-for-byte itself.

The state built deliberately: install, delete only the registry entry, then add
a file the registry has never heard of.

```
Echo "irreplaceable" >SYS:PkgTest/zaphod/MY-SAVEGAME
RAM:pkg --interrupt-at 7 ... install zaphod     -> interrupted on purpose (11)
RAM:pkg list                                     -> ok
RAM:pkg ... install zaphod                       -> unresolved transaction (3)
```

and what the recovery said:

```
SYS:PkgTest/zaphod still exists -- left in place, transaction kept
pkg: recovery stopped short
     1 transaction(s) could not be resolved. Nothing was cleaned up.
     Inspect db/transactions and run pkg doctor.

PASS assert-hash SYS:PkgTest/zaphod/MY-SAVEGAME
```

**The user's file survived**, and rollback removed only the 50 files it had
installed. Because one file it could not account for remained, the directory
did not go, the transaction was kept, and the next mutating command refused to
run rather than pressing on. That is the design's rule (absence of a registry
entry is not a licence to delete), measured rather than asserted.

Every `--expect` gate passed: f1 and f2 `i/o error`, f1x and f2x `not found`
after recovery, f3 `interrupted on purpose`, f3y `unresolved transaction`.

### The gap this exposes

Safe is not the same as usable. A leftover directory containing anything
unaccounted for now **wedges that package**: every mutating command refuses,
and the diagnostic tells the user to run `pkg doctor`, **which does not exist**.
The behaviour is right and the escape hatch is missing, which makes `pkg doctor`
the next thing worth building rather than a note in a design document.

## What the staging proof depends on

The rollback fix reads "the staged tree is still there, so the publish never
happened". Three things have to hold for that to be sound. The first was a
documented assumption and is now **enforced**; the other two remain assumptions
and are written down so a later change cannot break them quietly.

1. **The staging path is unique to the transaction: enforced 2026-09-10.**
   It was `tmp/<id>`, unique per *package*, which left the proof resting on the
   root lock to keep runs apart. A lock constrains concurrency; it does not
   establish whose leftovers these are after a crash. The transaction id is now
   minted before staging, the path is `tmp/<txn>/<id>`, it is recorded in the
   plan, and an install refuses outright rather than reusing a path that
   already exists. Measured: an interrupted install leaves one transaction-named
   directory under `tmp/`, and after `doctor --retry` `tmp/` is empty.
2. **Nothing recreates staging afterwards.** Recovery removes the transaction's
   whole staging root; nothing else may put a tree back at that path, or a later
   run would conclude a publish which *had* happened never did.
3. **Publishing is a rename of the whole tree, never a file-by-file copy.** The
   moment anything publishes incrementally, the staged tree's presence stops
   being evidence of anything.

## Still not done

- **Power loss is untested.** Every interruption here is a controlled return
  from a function; it checks the state left behind, but it is not a killed
  process and it is certainly not a machine losing power mid-write.
- `--interrupt-at` covers the six points the code defines; there are more places
  a real crash could land, in particular inside `u_publish` between the delete
  and the rename.
- No networking, no external files, no dependency resolution: unchanged.
- `zaphod/Zaphod.info` is the program's icon. The **drawer** icon,
  `<root>/zaphod.info`, is a separate rule and is not implemented.

Screenshots: `interrupt-points.png`, `stabilisation-reports.png`.

## Fault run (2026-09-09)

The stabilisation run only ever *stopped early*. These are the paths that
execute when a write actually **fails**, which stopping cannot reach, plus the
one window the design admits it cannot avoid.

### Provenance, properly this time

The source hash identifies the source, not the program: it says nothing about
the toolchain, the flags or what was linked. So the binary now hashes **itself**
at run time through `GetProgramName()`, and every report carries that alongside
the compiler, toolchain and SDK paths:

```
pkg 0.2
  binary  d7d8059bd668994941025b7405a818c865e2c3aeafab1e8214ec275461fb3b1f
  source  eff9139f31307ec9
  cc      10.5.0
  tc      /Volumes/arosmain/toolchain-mainline
  sdk     /Volumes/arosmain/build/bin/pc-x86_64/AROS/Developer
```

`shasum -a 256` of the file on the Mac gives the same 64 characters. The source
id is kept, but it is now labelled as what it is rather than standing in for the
binary.

### Results

| run | what was injected | result |
|---|---|---|
| f1 | `005-commit` refused during install | `i/o error`, transaction **kept** |
| f1x | next mutating run | recovery rolled back, then `not found` |
| f2 | doctor report refused during remove | `i/o error`, **registry still present** |
| f2x | next mutating run | recovery rolled **forward**, then `not found` |
| f3 | stopped inside `u_publish`, between the delete and the rename | `interrupted on purpose` |
| f3y | reinstall afterwards | `ok` |

f1 and f2 are the two cases the review asked for, and both behave as the design
says they must: a failure to record a decision does not report success, and a
failure to record what a removal left behind does not destroy the registry that
explains it.

### A test that was wrong before it was right

The first version of f3 reinstalled over a live package to reach the publish
window. It never got there: `install` refuses with `already installed` long
before publishing. The window only opens when the **destination exists and no
registry claims it**, which is precisely the leftover state a previous failure
produces. The test now creates that state deliberately by installing and then
deleting only the registry entry.

Worth recording because the first version *looked* like it passed: it produced a
report, with a status, in the right place. `already installed` is only visibly
wrong if you know what the test was for.

## What the staging proof depends on

The rollback fix reads "the staged tree is still there, so the publish never
happened". Three things have to hold for that to be sound. The first was a
documented assumption and is now **enforced**; the other two remain assumptions
and are written down so a later change cannot break them quietly.

1. **The staging path is unique to the transaction: enforced 2026-09-10.**
   It was `tmp/<id>`, unique per *package*, which left the proof resting on the
   root lock to keep runs apart. A lock constrains concurrency; it does not
   establish whose leftovers these are after a crash. The transaction id is now
   minted before staging, the path is `tmp/<txn>/<id>`, it is recorded in the
   plan, and an install refuses outright rather than reusing a path that
   already exists. Measured: an interrupted install leaves one transaction-named
   directory under `tmp/`, and after `doctor --retry` `tmp/` is empty.
2. **Nothing recreates staging afterwards.** Recovery removes the transaction's
   whole staging root; nothing else may put a tree back at that path, or a later
   run would conclude a publish which *had* happened never did.
3. **Publishing is a rename of the whole tree, never a file-by-file copy.** The
   moment anything publishes incrementally, the staged tree's presence stops
   being evidence of anything.

## Still not done

- **A real kill and a real reset are untested.** Everything here is a
  controlled return from a function. Issuing `system_reset` to QEMU mid-install
  was attempted and **blocked by the test environment's permissions**, so it
  remains owed rather than skipped. When it is run it must be reported as
  *"recovery after an abrupt guest reset"*: QEMU documents `system_reset` as
  resetting the guest, **not** as simulating the loss of unflushed data down
  the whole storage stack, so it is not a power-loss test and naming it one
  would overstate it. Power loss stays a separate, untested item. It should
  also run on a **separate test VM with its own disk copy**, not on a machine
  other tests share.
- Reboot behaviour is therefore also untested: the root is on `SYS:` now, so the
  state does survive, but nothing has yet been interrupted and then examined
  after a restart.
- Networking, external files, the drawer icon and dependency resolution are
  unchanged.

## Files

`src/libpkg/`: the library, `core.c` (context, lock, journal), `ops.c`
(install, remove, listing), `recover.c`, `zip.c`, `json.c`, `sha256.c`,
`util.c`. `src/pkg/main.c` is the CLI. `src/build.sh` builds for mainline
x86_64; the binary is 139 KB, because SHA-256 is in-tree rather than linked
from OpenSSL.

Screenshots: `install.png`, `tree-and-remove.png`, `recovery.png`. Per this
repo's evidence rule a screendump is not proof; these are illustrations, and
the claims above are the observable behaviour of code in `src/`.
