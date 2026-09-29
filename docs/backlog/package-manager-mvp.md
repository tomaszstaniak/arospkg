---
title: "Package Manager MVP"
status: draft
state: design
created: 2026-08-29
updated: 2026-09-10
related:
  - ../adr/0001-relationship-to-aaedt.md
  - ../spikes/network-download/README.md
  - ../spikes/dos-lock/README.md
  - package-standard-proposal.md
  - ../spikes/overlay-sample/README.md
  - ../spikes/first-run/README.md
  - storefront-proposal.md
---

# Package Manager MVP

## Document contract

This document fixes the scope and architecture of the MVP and collects the
design sections as they are accepted. It is complete when every section below
is accepted and the success criterion has been met in the QEMU VM; after that
set `status: accepted`, `state: complete`. New features go in new documents.

## Diagnosis

No real package manager exists for AROS. AROS Archives
(archives.arosworld.org) is an lha/zip catalogue with readmes: no dependency
metadata, no machine-readable versions, no checksums. Installation is
manual, or an `Installer` script that leaves no record. OS4 has AmiUpdate
(update only), MorphOS nothing. Earlier attempts (Grunch, ca. 2007–2010) died
with their maintainer because they were the only source of metadata.

Consequences: the index must be an *overlay* on what already exists, cheap to
generate, kept in git so anyone can contribute; the client must respect the
Amiga install model (program = directory; libraries go to LIBS:) rather than
imitate `/usr`.

## Decisions (2026-08-29)

| Question | Decision |
|---|---|
| Audience | Ourselves first; ~20 packages we actually use |
| Source of truth | Overlay of manifests over AROS Archives |
| Networking | Native bsdsocket + HTTP |
| Install layout | `Packages/<name>/` + registry of files placed elsewhere |
| Dependencies | Yes; simple, required, newest in index |
| UI | CLI + minimal Zune GUI |
| Platform | AROS x86_64 **and aarch64**, against mainline; plain Amiga API |
| Hosting | Archives only; we never host or mirror binaries |
| Success | 20 packages from the index install without error |

## Architecture A: one library, two fronts

`libpkg` (static C) owns everything: index parsing, HTTP download, archive
extraction, registry, dependency resolution. `apkg` (CLI) and `PkgManager`
(Zune) are thin fronts of a few hundred lines each and contain no logic of
their own. The index is generated on the host (Python, like `mkicon.py` in
`../AROS`) and committed; the client downloads one file.

Rejected: **B** (GUI runs the CLI via `SystemTags()`): the GUI would be a
mock-up with no structured progress or errors, and A is needed anyway later.
**C** (host resolves the plan, client only executes): the client is useless
without the host; a deploy script, not a package manager.

## Design sections

Each section is proposed, discussed and marked accepted here before code.

1. Index and manifest format: **accepted 2026-09-01**, amendment proposed 2026-09-03
2. On-disk layout and registry: **accepted 2026-09-06** (design; verification owed by section 6)
3. `libpkg` API: proposed 2026-09-07; **a subset implemented and running** 2026-09-08
4. `apkg` CLI: **first working version** 2026-09-08 (`list`, `info`, `install`, `remove`, `unlock`)
5. `PkgManager` GUI: *pending*; direction in [`storefront-proposal.md`](storefront-proposal.md)
6. Testing in QEMU: *pending*; owns section 2's verification obligations.
   One interruption point of six has been exercised: see
   [`../spikes/first-run/README.md`](../spikes/first-run/README.md)

### A requirement carried into section 3

`libpkg` will have to consume **two** kinds of metadata: the overlay manifests
we write for existing AROS Archives uploads, and, if the direction in
[`package-standard-proposal.md`](package-standard-proposal.md) is taken, a
manifest shipped inside the package by its author.

Section 3 must also carry **system requirements alongside package
dependencies** (see the drawer-icon amendment's third part), since resolution
has to fail differently for "the index has no provider" and "this machine lacks
it".

**Both must be parsed into one internal representation, and everything after
that point must be identical**: the same dependency resolution, the same
install, the same registry records, the same recovery. The difference between
them belongs entirely in the parser. If a standard-format package took a
different install path, every guarantee in section 2 would have to be re-argued
and re-tested for it, and the two paths would drift.

This constrains section 3's API and is recorded here rather than there so it
cannot be discovered late. The proposal itself does not extend the MVP and does
not block the API.


## Section 1: Index and manifest format (accepted 2026-09-01)

### Hosting: AROS Archives, always

`url` always points at AROS Archives. We do not mirror archives, we do not
host binaries in a GitHub release, and there is no `mirror` field to fall back
on. If we ever build a package ourselves it gets uploaded to Archives like
anyone else's and the index points there: the index stays pure metadata and
never becomes a distribution channel of its own.

The cost is accepted knowingly: a moved or renamed file upstream breaks that
entry until someone fixes the manifest, and `sha256` must be re-checked when
upstream replaces an archive in place. That is a manifest fix, not a mirror.

### Manifest: one TOML file per package, in git

`index/manifests/<name>.toml`, written by hand. TOML rather than JSON because
humans edit these: comments and no trailing-comma traps.

```toml
name    = "dosbox"
version = "0.74-3"        # upstream's string; opaque, compared only for equality
summary = "DOS emulator"
arch    = "x86_64"
url     = "https://archives.arosworld.org/share/emulation/computer/dosbox.x86_64-aros-v11.zip"
sha256  = "..."           # computed by tools/mkindex.py when the entry is added
depends = ["sdl2", "libpng"]
kind    = "app"           # app | lib
subdir  = "DOSBox"        # directory inside the archive that is the package; "" = archive root

[install]
libs = ["libs/sdl2.library"]   # paths inside the archive copied to LIBS: and recorded in the registry
```

### Index: one generated JSON file

`tools/mkindex.py` reads every manifest and writes `index/index.json`, which is
committed and served raw from GitHub. The client fetches only this file. Flat
JSON of a known shape, so the C parser is a few hundred lines with no
dependency: the TOML manifests never reach AROS.

```json
{"format": 1, "generated": "2026-09-01", "packages": [ { ...the manifest fields... } ]}
```

### Deliberately out of scope for the MVP

Version ranges in `depends`, conflicts and `provides`, more than one
architecture, more than one source. Each gets its own backlog document when it
is actually needed.


## Section 1 amendment: the archives have a machine-readable index (proposed 2026-09-03)

Section 1 assumed every manifest would be written by hand because the archives
carry no structured metadata. That was wrong, and chasing the AAEDT prior art
(ADR 0001) turned up what it actually reads:

```
https://archives.arosworld.org/share/FULLINDEX.readme
https://archives.arosworld.org/share/RECENT.readme
```

A plain text catalogue of the whole archive, one line per entry:

```
; category filename size date version :description
audio ffmpegaudioextractor.x86_64-aros-v11.zip 69548 20260812-13.27 1.0 :Convert audio or extract audio to Video
```

Fetched and measured 2026-09-03: **1894 entries, none malformed** (every line
has all five fields and the `:` separator). By architecture tag in the
filename: **163 `x86_64-aros-v11`**, 950 `i386-aros`, 8 `source`, 2
`os3-68k-aros`, 771 untagged. Top categories: game 599, utility 302,
development 263, graphics 235, audio 118, emulation 71, network 71.

163 entries for our exact target is comfortably more than the twenty the
success criterion asks for, and the file gives us category, filename, size,
date, version and description for free.

### What changes

`tools/mkindex.py` fetches `FULLINDEX.readme` and pre-fills a manifest
skeleton (name, version, category, url, summary) so a human only supplies
what the archive genuinely cannot know: `depends`, `kind`, `subdir`,
`install.libs`, and the `sha256` (computed by downloading the archive). The
hand-written overlay stays, because dependencies remain the whole point; it
just stops being hand-written from scratch.

Nothing else in section 1 moves. Hosting stays Archives-only, `url` is still
built from `https://archives.arosworld.org/share/<category>/<filename>`, and
the client still consumes only our generated `index.json`.

### Two verified gotchas for the parser

- **The index is Latin-1, not UTF-8.** It contains byte `0xf3`, so a naive
  `open(path)` in Python raises `UnicodeDecodeError` and macOS `grep` in a
  UTF-8 locale treats the whole file as binary and silently matches nothing.
  Decode as `latin-1` explicitly, on the host and in the C parser both.
- **The old host is gone.** `archives.aros-exec.org` redirects to a
  domain-parking page that returns HTTP 200 with HTML, so a fetch "succeeds"
  and yields garbage. Only `archives.arosworld.org` is live. Any fetch must
  check that the payload starts with the `;` comment banner before parsing it.



## Section 1 correction: the download URL (2026-09-06)

Every example in section 1 used `download.php?path=...`. **That is not the
download URL.** Measured 2026-09-06: it answers HTTP 200 with
`Content-Type: directory` and a few hundred bytes of HTML, not the archive. The
real form is

    https://archives.arosworld.org/share/<category>/<filename>

verified against four entries whose `Content-Length` matched the catalogue's
recorded size to the byte, across both architectures. `tools/mkindex.py`
generates this form and the examples above are corrected.

This was found by writing the generator, not by reviewing the document: the
URL had survived every round of review because nothing had ever fetched one.

## Section 1: corrections (2026-09-03)

- **Version comparison struck.** "compared lexically" was wrong: `1.10` loses
  to `1.9` lexically. The MVP index carries exactly one current version per
  package, so no ordering is needed: `version` is an **opaque string**,
  compared only for equality to answer "is the installed one the indexed one".
  Ordered comparison is a separate document if it is ever needed.
- **HTTPS is mandatory, and section 1's transport is therefore incomplete.**
  Verified 2026-09-03: `http://archives.arosworld.org/share/FULLINDEX.readme`
  answers 301 to HTTPS, and raw.githubusercontent.com is HTTPS only.
  `bsdsocket` alone cannot fetch either. The TLS layer is named and proven in
  `../spikes/network-download/`, which now blocks section 3.


## Section 1 second amendment: two architectures (2026-09-03)

The MVP is no longer scoped to a distribution. `apkg` builds for **AROS x86_64
and aarch64**, against **mainline**; distributions pull from mainline on their
own schedule. Scoping to AROS One 1.3 was a mistake and is corrected here and
in the parent repo's working notes.

### What that does to the index

`arch` in the manifest stops being a formality and becomes load-bearing: the
client must skip entries it cannot run, and `mkindex.py` must read the
architecture out of the archive filename.

The measured reality of the archives is lopsided. The table below was counted
by hand on 2026-09-03; **`tools/mkindex.py --stats` is now the authority** and
reports slightly different totals, both because the archive keeps growing and
because the tool's tag matching is stricter than the ad-hoc count was. As of
2026-09-06 it reports 1895 catalogue entries, 168 offered for x86_64 and
aarch64 combined, and 168 packages in the generated index.

| tag in filename | entries |
|---|---|
| `i386-aros` | 949 |
| untagged | 645 |
| `x86_64-aros-v11` | 163 |
| `i386` (older naming) | 82 |
| `ppc` | 25 |
| `x86_64` (older naming) | 12 |
| `68k` / `os3-68k` | 9 |
| `source` | 7 |
| **`aarch64`** | **2** |

The two aarch64 entries are `commander_keen_4.aarch64-aros.zip` and
`super_trevor_land.aarch64-aros.zip`, both from August 2026.

So the client is bi-arch from the start, but the *catalogue* is effectively
x86_64-only today. Consequences, stated plainly rather than discovered later:

- The success criterion (twenty packages installing cleanly) can only be
  met on **x86_64**. On aarch64 there are two candidates in the whole archive.
- That is an argument for the client being bi-arch anyway: the aarch64
  catalogue will fill in, and a manager that cannot express architecture will
  have to be retrofitted when it does.
- Untagged entries (645) need a rule. Some are architecture-neutral data
  (fonts, icons, documents), some are old uploads predating the convention.
  `mkindex.py` cannot guess; untagged entries stay out of the generated
  skeleton and are added by hand with an explicit `arch` if anyone wants one.


## Section 2: On-disk layout and registry (accepted 2026-09-06)

**Accepted as a design on 2026-09-06.** Acceptance covers the layout, the
ownership rules, the upgrade policy, manual unlocking and the recovery
semantics. It does **not** assert that any of it has been verified: crash
injection, power-loss behaviour and post-commit cleanup remain verification
obligations, listed at the end of this section and owed by section 6. A design
can be accepted and still be untested; conflating the two is how the earlier
drafts got their false confidence.

This section supersedes the earlier v1–v4 drafts and their amendments in full.
Those versions contained deliberately conflicting algorithms as the design was
argued out; implementing from them would be dangerous, so only this text is
normative. What changed and why is summarised in *Section 2: how it got here*
at the end of the document.

Facts below marked **measured** were run on mainline AROS x86_64 in QEMU; see
`../spikes/dos-lock/README.md`. Everything else is design and is owed a test.

### Where the root lives

`apkg` reads its root from `ENV:apkg/root`, written with
`SetVar(..., GVF_GLOBAL_ONLY | GVF_SAVE_VAR)` so `ENV:` and `ENVARC:` are
updated together, defaulting to `SYS:Packages`. We never edit `S:User-Startup`.

**Exactly one active root per system.** `LIBS:` and `Fonts:` are global, so two
roots would each treat the other's files as pre-existing and manage them
independently. Changing the root is allowed only when the current one has an
empty registry, no transaction directories and no external records. `--root` on
a mutating operation is rejected unless it names the active root; on `list`,
`info` and `search` it is unrestricted. Migrating a populated root is out of
scope for the MVP.

### Layout

```
<root>/
  <name>/                     unpacked package; an app brings its own .info
  db/                         no .info, so Workbench does not show it
    index.json                last downloaded index
    installed/<name>.json     one registry file per installed package
    external/<slug>.json      one record per pre-existing external file
    backup/
      _pre/<slug>             content that predates arospkg entirely
      upgrade-<name>-<ts>/    user files displaced by an upgrade
    transactions/<id>/        one directory per in-flight operation
    doctor/<name>-<ts>.json   what a removal deliberately left behind
    lock.owner                the lock, when held
  cache/                      verified archives, so a reinstall needs no network
  tmp/                        staging; cleaned only AFTER recovery
```

`db`, `cache` and `tmp` are reserved. Package names must match
`^[a-z][a-z0-9._-]{0,63}$`, so nothing under `backup/` can collide with one.

`<slug>` is the lowercase hex SHA-256 of the **case-folded `resolved_path`**
(below). AROS filesystems are case-insensitive, so folding before hashing is
what makes the identifier single-valued.

### An assign is not a file's identity

`LIBS:foo` and `SYS:Libs/foo` can be the same object, and an assign can be
repointed between install and uninstall. At plan time every external destination
is therefore resolved to a physical path: `Lock()` the containing directory,
`NameFromLock()` it, append the file name. **Measured:** this is how the spike
printed `ENV: resolves to RAM Disk:ENV`.

```json
{ "destination": "LIBS:sdl2.library",
  "resolved_path": "System:Libs/sdl2.library",
  "sha256": "b4d2..." }
```

Ownership, backup keys and uninstall all use `resolved_path`. `destination` is
kept to show the user what the manifest asked for and to detect that an assign
has moved: if it later resolves elsewhere, `apkg` acts on `resolved_path` (the
file it actually created) and says so. If `resolved_path` no longer exists it
warns and removes nothing rather than guessing.

A multi-directory assign such as `Fonts:` resolves to **one** physical directory,
chosen at plan time as the first writable member and recorded. The install fails
cleanly if none is writable.

### Registry file: `db/installed/<name>.json`

```json
{
  "schema": 1,
  "name": "dosbox",
  "version": "0.74-3",
  "arch": "x86_64",
  "installed": "2026-09-05T09:12:03Z",
  "origin": "https://archives.arosworld.org/share/emulation/computer/dosbox.x86_64-aros-v11.zip",
  "archive_sha256": "3f2a...",
  "reason": "explicit",
  "depends": ["sdl2", "libpng"],
  "dir": "dosbox",
  "contents": [
    { "path": "DOSBox",      "sha256": "9c1f...", "size": 4711232 },
    { "path": "DOSBox.info", "sha256": "a70e...", "size": 3140    }
  ],
  "external": [
    { "destination": "LIBS:sdl2.library",
      "resolved_path": "System:Libs/sdl2.library",
      "sha256": "b4d2..." }
  ]
}
```

`contents` paths are relative to `<root>/<name>`. Unknown keys are ignored so a
newer client can add fields. There is no `shared` flag: sharing is derivable by
scanning, and a stored flag would go stale the moment a second user arrives or
leaves.

### Ownership of files outside the package directory

**A package never edits another package's registry.** Before writing an external
file at `resolved_path` *P*, exactly one of these holds:

| state of *P* | action |
|---|---|
| no package claims *P*, and *P* is absent | write it; record it |
| no package claims *P*, but *P* exists | it predates us: copy to `db/backup/_pre/<slug>`, add `db/external/<slug>.json` with its original hash (**owned by no package**), then write ours. If a record already exists, reuse it; never back up twice |
| a package claims *P*, and its recorded hash equals what we would install | shared: record *P* in our `external` and **touch nothing else** |
| a package claims *P* with different content | **conflict: the install aborts**, naming the owning package, changing nothing |

The only exception is a controlled upgrade of the package that is *P*'s sole
provider.

Refcounting is a scan: at twenty packages it beats a counter that can drift.

### Uninstall

**The package directory is inventoried, not deleted.** For each entry in
`contents`, hash the file on disk:

- matches what we installed → delete it;
- differs → **leave it, warn**;
- present in the directory but absent from `contents` → the user put it there →
  **leave it, warn**.

Then remove now-empty subdirectories, and remove `<root>/<name>` itself **only
if it ends up empty**. Uninstall reports three counts (removed, kept because
modified, kept because unknown) and a directory left behind is a normal
outcome, not a failure.

**External files are hashed on disk before removal**, never removed on the
strength of the registry alone:

| on-disk state | action |
|---|---|
| hash matches what we installed | delete it; restore the `_pre` backup if one exists |
| hash differs | **leave it, warn.** Do **not** restore the backup over it |
| still claimed by another package | leave it, comparing on-disk content and not merely the two registries' recorded hashes |

A `_pre` backup left unrestored because the live file was modified is **kept and
flagged as needing a human decision**: the job of a future `apkg doctor`.

### The transaction journal

`db/transactions/<id>/`, `<id>` being `<timestamp>-<name>`, is **append-only**.
Every step creates a new name; nothing is ever rewritten, because with no atomic
replace available a rewritten field is exactly the information a crash can
destroy.

```
db/transactions/<id>/
  000-plan.json
  001-dir-done
  002-external-0-done
  003-external-1-done
  004-registry-written
  005-commit
```

Markers are zero-length: their existence is the signal, so there is no torn
content to interpret.

**A missing marker does not mean the mutation did not happen.** A crash between
performing a write and creating its marker is not only possible, it is the most
likely crash window there is, because the two are adjacent. Recovery therefore
**never** uses markers to decide what was done. They are an audit trail and a
fast path, nothing more. The one exception is `005-commit`, which records a
decision rather than an action; see below.

#### The plan

`000-plan.json` enumerates **every mutation the operation intends**, each with
its expected state before and after:

```json
{ "op": "install", "package": "dosbox",
  "mutations": [
    { "kind": "dir",      "path": "<root>/dosbox",
      "before": "absent",  "after": "present" },
    { "kind": "external", "destination": "LIBS:sdl2.library",
      "resolved_path": "System:Libs/sdl2.library",
      "before": { "state": "pre-existing", "sha256": "11aa...",
                  "snapshot": "old-external/9c3f..." },
      "after":  { "state": "present",      "sha256": "b4d2..." } },
    { "kind": "registry", "path": "db/installed/dosbox.json",
      "before": "absent",  "after_sha256": "77de..." }
  ] }
```

It is written, closed, reopened, read back and parsed before the first mutation.
**That proves the plan is readable at that moment; it does not prove that an
unreadable plan found later means nothing was changed.** Those are different
failures: a killed process cannot corrupt a closed file, but a power loss can
leave a file the filesystem never fully committed. So the two cases are treated
differently.

- Re-read fails *now*, before any mutation: delete the transaction directory and
  abort having touched nothing.
- Plan unreadable *at recovery time*, after a restart: **stop.** Do not guess,
  do not clean up, do not delete `tmp/`. Preserve the transaction directory and
  every snapshot in it, refuse mutating operations, and tell the user to run
  `apkg doctor`. An unreadable plan is the one state this design cannot resolve
  by itself, and pretending otherwise would destroy the evidence needed to
  resolve it by hand.

#### Recovery is plan-driven, content-driven and idempotent

`005-commit` decides only the **direction**:

| operation | commit marker absent |
|---|---|
| install, upgrade, set-reason | roll **back** |
| remove | roll **forward**: a deleted file cannot be restored |

The *work* is then driven by the plan, not by markers. Recovery walks **every**
mutation the plan lists, whether or not it has a marker, and establishes what
actually happened by hashing what is on disk:

| on-disk content matches | rolling back | rolling forward |
|---|---|---|
| `after` | undo it: restore `before` from its snapshot, or delete if `before` was `absent` | already done; skip |
| `before` | nothing to undo; skip | perform it |
| `absent`, where `before` was not `absent` | restore `before` from its snapshot | perform it |
| **anything else** | **leave it, warn, and leave the transaction unresolved** | same |

`absent` is a **recognised** state, not an unknown one, and separating the two
is what makes a crash mid-replacement recoverable rather than merely reported.

**No managed file is ever partially written under its real name.** Every
external file is written to a temporary name in the same directory and then put
in place; since `Rename()` cannot overwrite, replacing an existing file is
snapshot → delete → rename. The window that leaves is the file being *absent*,
which the table above resolves from the snapshot. Without this rule a crash
mid-copy would leave a truncated library that matches neither expected hash and
would therefore be classified as somebody else's file and left in place: a
broken library, preserved on purpose. The temp-and-rename discipline is what
keeps the "unknown content" row meaning what it says.

So the last row is reached only by content that is neither of our two expected
states nor absent: something outside this transaction changed it. **Unknown
content is never removed automatically**: destroying it to tidy up a rollback
would be the worst thing this design could do.

#### An unresolved transaction blocks

A transaction that recovery could not complete (because a file held unknown
content, or because its plan was unreadable) is **left in place and marked
unresolved**. While any unresolved transaction exists:

- mutating operations refuse to run and name the transaction;
- `tmp/` and every snapshot are preserved, not cleaned;
- `apkg list`, `apkg info` and `apkg doctor` still work.

This is the same rule the unreadable plan already had, stated once for every way
recovery can stop short. Recovery that cannot finish must not silently hand the
machine back as though it had.

Every step is written to be **idempotent**, because recovery itself can crash
and be re-run. Restoring a snapshot over a file that already matches `before` is
a no-op; deleting an absent file succeeds; renaming a directory that is already
in place is recognised and skipped. Recovery re-run from the beginning must
reach the same state as recovery run once.

On startup, in this order, and the order is the requirement:

1. take the lock;
2. recover every transaction directory found;
3. **only then** delete `tmp/`.

Cleaning `tmp/` first would discard the staged tree a roll-forward needs.

### Operation protocols

**install**: `plan → dir → external-N → registry-written → commit`.
Rollback walks every mutation in the plan, not the markers, and applies the
reconciliation table above: the package directory is removed if it is ours and
matches what we unpacked, each external write is undone by restoring its
snapshot or deleting the file when `before` was `absent`, and the registry file
is deleted if its content matches the one we wrote.

**remove**: `plan → report → external-N → contents → registry-deleted → commit`.
Roll-forward only. Recovery re-attempts every planned deletion, skipping those
already done, so the plan must be complete enough to finish without the registry,
which is what the last step destroys. A file whose content matches neither the
recorded hash nor absence is left alone with a warning, exactly as in an ordinary
uninstall.

Because the registry is destroyed, anything deliberately left behind would
otherwise become unattributable. So `remove` writes `db/doctor/<name>-<ts>.json`
listing every path it kept and why **before** deleting the registry entry. That
file is what `apkg doctor` reads, and it is the reason `report` appears in the
protocol above rather than being an afterthought.

**upgrade**: the new tree is assembled **completely in `tmp/<name>/`** and
verified before anything live is touched; it is never unpacked into the live
directory, where a crash mid-unpack would leave a tree nothing can classify.
Publication is two renames, with the journal covering the gap:

```
plan
  -> snapshot each external file to transactions/<id>/old-external/<slug>
  -> Rename <root>/<name>  ->  transactions/<id>/old/
  -> Rename tmp/<name>     ->  <root>/<name>
  -> migrate user files    (below)
  -> external-N
  -> registry-written
  -> commit                (only now are old/ and old-external/ deleted)
```

A crash between the two renames leaves no package directory, which the journal
recognises and repairs from `old/`. Preserving the real directory, rather than
planning to re-unpack from `cache/`, which is neither guaranteed present nor
able to restore user files, is what makes rollback possible without the
network.

**Migration** classifies every file in `old/` against the old registry:

| file in `old/` | new version provides that path | action |
|---|---|---|
| unchanged | either | nothing; the new version owns it |
| modified, or absent from the old registry | **no** | copy into the new tree; it is the user's and nothing conflicts |
| modified, or absent from the old registry | **yes** | the new file stays; the user's copy is preserved to `db/backup/upgrade-<name>-<ts>/<relpath>`, listed in the summary and flagged for `apkg doctor` |

**Upgrade refuses to touch a modified external file.** Before modifying *P*:

| current on-disk hash | action |
|---|---|
| equals the old registry's hash | snapshot to `old-external/<slug>`, verify the copy by hash, proceed |
| the file is absent | record `pre: absent`, proceed with a warning |
| anything else | **conflict: abort the upgrade, change nothing** |

This is why deleting `old-external/` after commit is safe: we only ever snapshot
content we installed ourselves. A user's hand-placed library is never
overwritten and therefore never needs preserving. A `--force` mode with a
permanent backup can be added later; it is deliberately absent from the MVP.

**set-reason**: `plan → registry-written → commit`. Trivial, but still a
non-atomic replacement of the file everything else depends on, so it still gets
a transaction.

### The lock

Acquisition uses `Rename()` as atomic create-if-absent. **Measured:** `Rename()`
refuses to overwrite; two processes over 150 attempts each produced **zero**
mutual-exclusion violations; the record left behind is an ordinary file that
another process can read and delete.

```
acquire:  write db/lock.<nonce>.new, close it
          Rename(db/lock.<nonce>.new -> db/lock.owner)
            success -> held
            failure -> delete our file, read db/lock.owner, report or wait
release:  delete db/lock.owner only if it still carries our nonce
```

The record is `{nonce, boot_id, process, task, started}`. `boot_id` is a random
token in `ENV:apkg/boot-id`, established by the same primitive: write
`ENV:apkg/boot-id.<nonce>` and rename it into place, loser deletes its file and
reads the winner's. The nonce file lives in `ENV:apkg/` so the rename stays
intra-volume. `SetVar()` is bypassed deliberately: an env var is a file, and we
need create-if-absent, which `SetVar` does not offer. **Measured:** `ENV:`
resolves to `RAM Disk:ENV`, so the token cannot outlive the boot and no
`GVF_SAVE_VAR` is involved.

Only mutating operations lock. `apkg list` and `apkg search` never do.

**A stale lock is not reclaimed automatically. This is a deliberate narrowing
and the one place this section chooses less than was asked for.** If
`db/lock.owner` exists and its `boot_id` differs from the current token, or its
process is not alive, `apkg` refuses to run and prints the record together with
the exact command to clear it. `apkg unlock` shows who held it, when, and
requires confirmation.

**Confirmation is not exclusion.** `apkg unlock` removes the only thing keeping
two instances apart, so it states as an operating condition that every other
`apkg` and `PkgManager` instance must be stopped first, and says so in the prompt
rather than only in the manual. The user agreeing that the lock looks stale is
not evidence that no second instance is running; nothing in the MVP can check
that for them, and the design should not imply it can.

The reason is that automatic reclamation is not a small addition. It needs a
reaper role, an identity comparison to avoid destroying a lock that was
re-acquired between the judgement and the removal, a restore path when that
comparison fails, and its own recovery for a reaper that dies mid-reap, with
each of those states reachable by a crash. That is a four-state distributed
protocol inside a single-user package manager, and its failure mode is
destroying a live lock, which is worse than the failure it prevents. Against
that: the boot token makes a post-reboot stale lock trivially recognisable, a
crash is rare, and the user is already at a Shell. One extra command in a rare
case buys the removal of the most dangerous machinery in the design.

If automatic reclamation is wanted later it is an additive change (the record
already carries the nonce that such a protocol needs) and it should arrive with
the crash-injection test that proves it, not before.

### Validation before anything is unpacked

Package names must match `^[a-z][a-z0-9._-]{0,63}$`, must not be `db`, `cache`
or `tmp`, and must be unique **case-insensitively**. `tools/mkindex.py` enforces
this across the index; the client re-checks, because the index arrived over a
network.

Every archive member is checked before extraction and a single failure rejects
the whole archive:

- no absolute path, and on AROS that means **a `:` anywhere in the path**, not
  just a leading separator. `LIBS:foo` inside a zip is an absolute path and is
  the escape a Unix-shaped check misses;
- no `..` component, and the resolved path must remain inside staging;
- no links of any kind;
- no two members colliding after case-folding, no duplicate members, and no
  member that is a file where another member requires a directory;
- no member over `max_file_size` and no total over `max_total_size`.

External writes are restricted to an approved list, for the MVP **`LIBS:` and
`Fonts:` only**. A manifest asking for anything else is rejected by
`mkindex.py` and again by the client. Widening the list is a design change, not
a manifest change; that is the reason it is a list.

`max_file_size` and `max_total_size` are configuration, defaulting to 64 MB and
256 MB. **Those defaults are a guess and are labelled as one**: they are owed a
measurement across the twenty packages chosen for the success criterion, and the
criterion cannot be declared met until that measurement has confirmed or moved
them.

### What this section is owed

- **Recovery has no test.** This is the largest gap in the design. Crash
  injection must cover, for all four protocols:
  - **between a mutation and its marker**: the window that made the
    marker-driven rollback of the earlier drafts wrong, and the most likely one
    in practice because the two operations are adjacent;
  - during a rollback and during a roll-forward, since recovery must be
    idempotent and can itself be interrupted;
  - during post-commit cleanup, which deletes snapshots and `old/`;
  - and separately for process kill versus power loss, because only the second
    can produce the unreadable-plan state that halts recovery.

  Marker boundaries alone are not sufficient and were the assumption that hid
  the defect.
- The within-boot liveness test (process pointer plus task name) is designed,
  not measured.
- `ERROR_OBJECT_EXISTS` is the presumed `IoErr` for a contended acquire but is
  **not** measured: the probe overwrote it with its own cleanup. Fixed in
  `renlock.c`, not re-run.
- The size-limit defaults are unmeasured.


## Section 2 amendment: drawer icons and SDK packages (2026-09-07)

Two of the three items below are now decided rather than open. Section 2 is not
reopened; these are additive.

### The drawer icon is a managed file

A package's Workbench icon is `<root>/<id>.info`, a **sibling** of the package
directory, because that is what Amiga expects. A manifest may name where it
comes from inside the archive:

```toml
icon = "SDLPoP.info"     # optional; path inside the archive
```

`libpkg` installs it as `<root>/<id>.info`, and **it is a managed file like any
other**: it appears in the plan with its `before`/`after` hashes, it is recorded
in the registry, and recovery reconciles it by the same table as everything
else. It is removed on uninstall only if unchanged.

This is the part the `subdir = ""` workaround does not do. Keeping the pair
inside the package directory stops the archive's icon being lost, but it gives
the *installed* directory no icon at all: the package is still invisible where
a user looks. Only an explicit mapping fixes that, and only if the file is
managed, or an uninstall would leave an icon pointing at nothing.

`icon` is optional. A package without one installs as it does today.

### Installing into the SDK is out of scope for the MVP

`libmad` is a header and a static library, belonging under `Developer/include`
and `Developer/lib`. **The approved destination list stays `LIBS:` and `Fonts:`
and is not widened.**

What is excluded is **installation into the SDK**, not development software.
A compiler, an editor or a debugger that installs as an ordinary application is
squarely in scope: the accepted `zaphod` is exactly that, and it sits in
`development/edit`. The line is where the files go, not what the program is
for.

Candidates excluded on this ground keep an explicit reason so they are visibly
deferred rather than quietly absent. Revisiting it is a design change with its
own document.

### System requirements are not package dependencies

*Implemented 2026-09-12. See `src/libpkg/req.c` and
`docs/reports/2026-09-12-requires-system-onetest.md`.*

A manifest needs `requires_system` alongside `depends`, because **"no package
in the index provides X" and "this machine does not have X" are different
conditions** and must produce different messages. Only the first is ours to
fix. `mkindex.py` closes `depends` over the index and deliberately does not
close `requires_system`.

**Three outcomes, not two.** The one that matters is the third:

| outcome | what happens |
|---|---|
| satisfied | proceeds silently |
| missing | refuses, **before the download**, saying the lack is the machine's |
| undetermined | proceeds, warns prominently, and the verdict is written into the registry entry |

`undetermined` is not a step on the way to `satisfied`. Most things a package
can require on this platform are not decidable from outside the program, and a
manager that refuses whenever it cannot be sure is one nobody runs, while one
that rounds doubt up to "fine" is lying. So the doubt is recorded against the
install and `apkg info` still shows it a month later.

The check sits beside the ABI gate and for the same reason: both are properties
of this machine, both are decidable before a byte is fetched, and a package
failing either would install perfectly and then not start.

**What is checked, and how.** Only `type = "library"` has a real verdict today.
The probe tries the resident library list first (no code loaded, and the
version is in the node), then `OpenLibrary()` (which does run the library's
init, deliberately, because that is the question actually being asked and what
the program will do a moment later), then looks for the file in `LIBS:` so that
"nothing by this name" and "it is here and will not open" are told apart. Every
other type answers `undetermined` and says so. What the probe cannot tell
apart: `OpenLibrary()` failing for want of memory looks exactly like absence.

**No provider is refused, not guessed.** A `requires_system` entry with no `id`
or no `type` refuses the install. Skipping it would make a malformed
requirement indistinguishable from a package with no requirements, which is the
exact confusion the field exists to end. `mkindex.py` runs the same check on
the host so the failure lands on us rather than on a user.

**Not implemented:** no override flag. There is deliberately no
`--ignore-requires` to sit next to `--abi`; the ABI override exists for
diagnosing the ABI question itself, and nothing here needs its equivalent yet.


## Open against section 2, from the first real packages (2026-09-07)

Four catalogue uploads were taken through the whole metadata pipeline
([`../spikes/overlay-sample/README.md`](../spikes/overlay-sample/README.md)).
**Two of the four could not be approved**, and neither failure was about effort.
Both are recorded here because they are gaps in this section, not in the tools.

1. **A `.info` lives beside its drawer, not inside it**: *decided*, see the
   drawer-icon amendment above.

2. **Development packages cannot be expressed**: *decided*, out of MVP scope,
   destination list not widened.

3. **No provider found for a needed library**: *closed 2026-09-12, and the
   answer was that the question was wrong.* `sdllopan` references
   `SDL.library`, `crt.library` and `stdlib.library`. None of the three is a
   package in this catalogue and none can be: they are parts of a distribution.
   AROS One 1.3 ships all three, mainline ships none. So the requirement was
   never `depends` waiting for a provider: it was `requires_system` all along,
   and "no provider found in this catalogue" was true and beside the point.
   `sdllopan` is now published, installed and run.

None of these invalidates the accepted design. They are the first evidence of
what it does not yet cover, which is what a sample is for.

**Two published manifests are not two working programs.** Nothing in the index
has been installed or run on mainline; publication asserts that the metadata is
complete and internally consistent, nothing more.


## Section 3: the `libpkg` API (proposed 2026-09-07)

Architecture A puts every decision in `libpkg` and leaves `apkg` and
`PkgManager` as presentation. That only holds if the API never requires a front
end to *decide* anything: only to display, and to answer questions the library
poses explicitly.

Three requirements were carried into this section and are addressed below:
cycle and self-dependency handling in the resolver, a typed and tri-state model
for system requirements, and one install plan shared by directory files, the
drawer icon and external files.

### Shape: C, no globals, two handles

Plain C89-compatible C, no allocator tricks, no global state: the CLI and the
GUI can both be linked into one process later, and a static library with globals
would make that a rewrite. Everything is reached through two opaque handles.

```c
typedef struct pkg_ctx  pkg_ctx;   /* a root: its db, cache, lock */
typedef struct pkg_plan pkg_plan;  /* a proposed change, inspectable */
```

Every entry point returns a status and fills an error record; nothing returns a
bare pointer that might be an error in disguise.

```c
typedef enum {
    PKG_OK = 0,
    PKG_E_LOCKED,          /* another instance holds the lock          */
    PKG_E_STALE_LOCK,      /* lock looks dead; apkg unlock is needed    */
    PKG_E_UNRESOLVED_TXN,  /* recovery stopped short; doctor is needed */
    PKG_E_NOT_FOUND,
    PKG_E_CYCLE,
    PKG_E_CONFLICT,        /* external path owned with other content   */
    PKG_E_REQUIREMENT,     /* a system requirement is unsatisfied      */
    PKG_E_VERIFY,          /* hash or size mismatch                    */
    PKG_E_NETWORK,
    PKG_E_IO
} pkg_status;

typedef struct {
    pkg_status status;
    char       summary[128];   /* one line, already user-facing        */
    char       detail[512];    /* what to do about it, may be empty    */
    char       subject[128];   /* the package, path or id it concerns  */
} pkg_err;
```

`summary` and `detail` are written by `libpkg`, not by the front ends. Two
fronts phrasing the same failure differently is how a project ends up with two
behaviours; and a GUI has nowhere sensible to compose an error from a code.

### One internal representation, as section 3's standing requirement demands

```c
typedef enum { PKG_SRC_OVERLAY, PKG_SRC_EMBEDDED } pkg_source;

typedef struct {
    char        id[64], version[64], summary[160];
    char        arch[16], abi[16];
    pkg_source  source;        /* which parser produced this, for diagnostics */
    char        url[512];
    char        sha256[65];
    unsigned    size;
    char        subdir[128];   /* "" = archive root                           */
    char        icon[128];     /* "" = none                                   */
    pkg_dep    *depends;       size_t n_depends;
    pkg_req    *requires;      size_t n_requires;
    pkg_extern *external;      size_t n_external;
} pkg_manifest;
```

An overlay manifest and a manifest shipped inside an archive both parse into
this and are indistinguishable afterwards except for `source`, which exists only
so a diagnostic can say where a fact came from. **Nothing downstream of the
parser may branch on `source`.** If it ever needs to, the two formats have
diverged and that is a design failure, not a feature.

### System requirements: typed, and tri-state

A requirement is not a dependency: it is something the machine must already
provide, and the index cannot supply it.

```c
typedef enum { PKG_REQ_LIBRARY, PKG_REQ_DEVICE, PKG_REQ_DATATYPE,
               PKG_REQ_ABI,     PKG_REQ_ARCH } pkg_req_type;

typedef struct {
    pkg_req_type type;
    char         id[64];       /* "cybergraphics.library", "v11", ...  */
    unsigned     min_version;  /* 0 = unversioned                      */
} pkg_req;

typedef enum {
    PKG_REQ_SATISFIED,
    PKG_REQ_UNSATISFIED,
    PKG_REQ_UNDETERMINED       /* could not be established either way  */
} pkg_req_result;
```

**`PKG_REQ_UNDETERMINED` is a first-class outcome, not an error.** The spike
made the reason concrete: a file existing in `Libs/` proves a name is present in
*that build*, not that its version or behaviour matches, and a version a library
does not report cannot be checked at all. A design with only satisfied and
unsatisfied would have to lie in one direction or the other.

The policy follows from that:

| result | effect |
|---|---|
| satisfied | proceed silently |
| unsatisfied | **refuse**, naming the requirement and that it is the *machine's*, not the index's |
| undetermined | proceed, **report prominently**, and record it in the registry |

Undetermined does not block, because on this platform most requirements are not
decidable and a manager that refuses whenever it cannot be sure is a manager
nobody runs. It is recorded so that if the program later misbehaves, the
registry can say the check was never conclusive rather than implying it passed.

This also fixes the message split the sample exposed. "No package in the index
provides `sdl.library`" is `PKG_E_NOT_FOUND` against a dependency and is our
problem to fix; "this machine does not have `sdl.library`" is
`PKG_E_REQUIREMENT` and is the user's. They are different code paths, not one
string with two meanings.

### Resolution, including the cases closure does not catch

```c
pkg_status pkg_resolve(pkg_ctx *, const char *id, pkg_plan **out, pkg_err *);
```

`mkindex.py` closes `depends` over the index, which guarantees every named
package exists. It says nothing about cycles: `a → b → a` is perfectly closed.
So the resolver owns three cases the generator cannot see.

- **Self-dependency**: rejected. It is always a manifest error.
- **Cycles**: **rejected in the MVP**, with the full path named in `subject`
  (`a -> b -> c -> a`). Cyclic dependencies are resolvable in principle, but
  only by a scheme that installs a group atomically, and section 2's transaction
  model is per package. Accepting cycles would mean either a lie about
  atomicity or a second transaction model. The MVP does neither, and says so.
- **Diamonds**: accepted and deduplicated; the same package reached twice is
  one install.

Resolution is over `(id, arch)`, never `id`. Everything the sample taught about
the catalogue applies here: one program can exist for both architectures and the
pair is the identity.

A plan is inspectable before anything happens, which is what lets a GUI show
what it is about to do:

```c
size_t          pkg_plan_count(const pkg_plan *);
const pkg_step *pkg_plan_step(const pkg_plan *, size_t i);
const pkg_req_check *pkg_plan_requirements(const pkg_plan *, size_t *n);
void            pkg_plan_free(pkg_plan *);
```

### One install plan for three kinds of file

The sample produced three kinds of managed file and the design's own history
says what happens when they get separate rules: they drift, and section 2's
guarantees have to be re-argued for each. So there is one type.

```c
typedef enum {
    PKG_FILE_CONTENT,   /* inside <root>/<id>/                          */
    PKG_FILE_ICON,      /* <root>/<id>.info, sibling of the directory   */
    PKG_FILE_EXTERNAL   /* LIBS: or Fonts:, resolved to a physical path */
} pkg_file_kind;

typedef struct {
    pkg_file_kind kind;
    char          destination[256];   /* as written, may be an assign    */
    char          resolved_path[256]; /* physical; ownership key         */
    char          sha256[65];
    unsigned      size;
} pkg_file;
```

**Identical treatment, and this is the load-bearing sentence of the section:**
every `pkg_file` regardless of kind is written to a temporary name and renamed
into place; recorded in the plan with its `before` and `after`; entered in the
registry; reconciled by section 2's recovery table; hashed on disk before
removal; and left alone with a warning if its content is unknown. The drawer
icon is not a special case with its own rules: it is a `pkg_file` whose `kind`
happens to be `PKG_FILE_ICON`.

Only two things vary by kind: where the path comes from (inside the package
directory, `<root>/<id>.info`, or an assign resolved at plan time), and that
`PKG_FILE_EXTERNAL` is subject to the ownership table and the approved
destination list. Ownership, recovery and user-change preservation do not vary
at all.

### Lifecycle, and where the lock and recovery live

```c
typedef enum { PKG_OPEN_READ, PKG_OPEN_WRITE } pkg_mode;

pkg_status pkg_open (const char *root, pkg_mode, pkg_ctx **, pkg_err *);
pkg_status pkg_apply(pkg_ctx *, pkg_plan *, pkg_progress_fn, void *, pkg_err *);
void       pkg_close(pkg_ctx *);
```

`pkg_open` with `PKG_OPEN_WRITE` takes the lock, runs recovery on every
transaction directory, and only then cleans `tmp/`: section 2's required
order, enforced in the one place both front ends must pass through rather than
trusted to each of them. It fails with `PKG_E_STALE_LOCK` or
`PKG_E_UNRESOLVED_TXN` rather than repairing anything a human should see first.

`PKG_OPEN_READ` takes no lock and never mutates, so `apkg list` and a GUI's
browsing view work while an install is running.

Progress is a callback, not a return value, because the GUI must stay responsive
during a download and the CLI wants a line per step:

```c
typedef void (*pkg_progress_fn)(const pkg_step *, unsigned pct, void *user);
```

There is deliberately **no cancellation in the MVP**. A cancel would have to
unwind mid-transaction, which is the rollback path: the least tested part of
the design. Adding it before that path has crash-injection coverage would be
adding a second way to reach untested code, on purpose.

### What section 3 does not settle

- Download resumption. `dl` fetches whole files; a partial download is discarded
  and refetched, as `fetch.py` already does on the host.
- Any notion of ordering between versions. `version` remains opaque and is
  compared only for equality, per section 1.
- Cancellation, as above.
- Whether `PKG_REQ_UNDETERMINED` should block under a strict mode. It is
  recorded so the question can be answered from data later.


## Section 2: how it got here

The algorithms of v1–v4 and their amendments are **removed**, not archived: they
contradicted each other by design, and a document that offers an implementer
five incompatible locking schemes is a hazard. What is worth keeping is why each
turn was taken.

| round | what was returned, and what it changed |
|---|---|
| v1 → v2 | No transaction journal; ownership could be rewritten between packages; uninstall deleted the package tree wholesale, destroying saves and configuration. |
| v2 → v3 | `Rename()` was assumed to be an atomic replace. It is not: it returns `ERROR_OBJECT_EXISTS` (`rom/filesys/fat/ops.c:574`). Replacement is delete-then-rename, so the journal, not the rename, is what makes the registry safe. |
| v3 → v4 | v3 guarded the lock with `Lock(EXCLUSIVE_LOCK)` held briefly. Measurement showed a DOS lock survives its owner and cannot be deleted, so a crash inside even a brief window wedges the machine until reboot. v3's text claimed the opposite of a result already in the spike. |
| v4 → amendments | Reaping a stale lock had an ABA race: a process could delete a lock re-acquired between its judgement and its removal. Boot-id creation needed the same atomic primitive. Upgrade snapshotted the package directory but not external files. |
| amendments → canonical | The reaping protocol still had no recovery for a reaper that died mid-reap, and its remaining transitions kept multiplying. **Automatic reclamation was removed from the MVP** in favour of `apkg unlock` with confirmation. Upgrade now aborts on a modified external file rather than snapshotting it. |
| canonical → this text | Rollback undid only the writes carrying a `-done` marker, so a crash between a write and its marker left the change outside the registry: the very failure the journal exists to prevent, reintroduced by the journal's own bookkeeping. Recovery is now driven by the plan and by the content on disk, never by markers, and is idempotent. An unreadable plan found after a restart halts recovery instead of being read as "nothing happened". |

Two lessons are worth more than the algorithms:

**A mechanism that explains the symptom is not thereby the mechanism.** This bit
the project twice: once on the OpenSSL socket patch, which turned out to mask
two unrelated bugs, and once on `Rename()`, whose assumed semantics survived two
drafts before anyone read the handler.

**A measurement already on the page can still be contradicted by the text next
to it.** v3 did exactly that. Writing a result down is not the same as letting
it change the design.

**Absence of a record is not evidence of absence of an action.** The journal was
introduced to stop a crash leaving a change nobody knew about, and then its
rollback was specified to trust its own markers, so a crash in the one-instruction
gap between doing the work and recording it produced exactly the state the
journal existed to prevent. Bookkeeping can only ever describe the world; when
they disagree, the world is right. Recovery must therefore look at the disk, not
at what it wrote down about the disk.
