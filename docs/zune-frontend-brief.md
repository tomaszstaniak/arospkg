---
title: "Brief for a Zune front end"
status: brief
created: 2026-09-23
audience: whoever builds PkgManager (Zune), and whoever reviews it
---

# A Zune front end for apkg

Everything a front end needs from `libpkg` exists as of 2026-09-23 and is
declared in `src/libpkg/pkg.h`. This file says what to code against, what is
already known about the target, and what not to do.

**Status 2026-09-25: three increments exist and passed their acceptances**
(the third adds Upgrade and Roll back over `pkg_upgrade_preview`,
`pkg_rollback_preview`, `pkg_can_upgrade` and `pkg_can_rollback`; run at
`docs/reports/2026-09-25-gui-upgrade/`):
`src/pkgmanager/`, built by `src/build-gui.sh`; runs at
`docs/reports/2026-09-23-gui/` and `docs/reports/2026-09-24-gui2/`. The brief
stays as the contract for whatever comes next in that window. Two Zune facts
learned the hard way are in the second report: a Cycle selects on release,
and a String gadget acknowledges on focus loss.

## The contract: rows and progress

```c
pkg_status pkg_query(pkg_ctx *, const char *index_path, const char *term,
                     pkg_entries *out, int *hidden, pkg_err *);
pkg_status pkg_installed(pkg_ctx *, const char *index_path,
                         pkg_entries *out, pkg_err *);
void       pkg_entries_free(pkg_entries *);
void       pkg_set_progress(pkg_ctx *, pkg_progress, void *user);
```

`pkg_entry` is a fixed-size struct (id, version, revision, summary, category,
arch, abi, kind, size, `ours`, `installed`), so a list can point at the array
without allocating. The Shell's `apkg search` is formatted **from these same
rows** (`pkg_search` calls `pkg_query`), so the window and the Shell cannot
show different indexes. `apkg --json search` / `--json list` print the rows
verbatim if you want to look at them without writing C.

Do not parse `pkg_list`/`pkg_search` text. That is the mistake the accessor
exists to prevent.

`pkg_progress` delivers a `pkg_progress_ev`: `phase` (`download`, `verify`,
`extract`, `publish`), the package `id`, `done`/`total` bytes where known,
and **`can_cancel`**. The Cancel button is enabled exactly when the last
event said `can_cancel` is 1 and disabled the moment one says 0: the window
never infers it from the phase name. That is 1 during the download and after
verification, 0 from the moment a transaction exists; the library also
enforces it, so a stray cancel after that point is ignored rather than
half-honoured. The reason is in `pkg.h`.

## The execution contract

"Run it on another task" is necessary and not sufficient. What the installer
needs, and what the window must not do:

- **`pkg_install` blocks for its whole duration** and does DOS I/O and
  network I/O. It must run on a task or process **with its own DOS process
  context**, a `CreateNewProc()` process, not a bare `CreateTask()`, because
  `Open()`, `Lock()`, `bsdsocket.library` and the current directory are
  per-process state. A task without them fails in ways that look like bugs in
  `libpkg`.
- **One `pkg_ctx` per operation, opened on the worker.** The root lock is
  held by whoever opened it; do not open in the window and install from the
  worker.
- **The callback runs on the worker.** It copies the event into a message and
  sends it to the window's port; the Zune loop picks it up and updates the
  gauge and the button. The callback never touches a MUI object.
- **Cancel goes the other way the same way**: the window sets a flag the
  callback reads (a shared int is enough; the callback is polled by the
  library, not signalled), and the button greys out on the next event that
  says `can_cancel` 0.
- **Errors come back as `pkg_err`**: `summary`, `detail`, `subject`,
  already user-facing. Show them as they are; do not re-phrase.

`apkg --progress install <id>` prints every event, one per line with
`cancel=yes|no`, which is the cheapest way to see the sequence before writing
the message plumbing.

## Acceptance for the first increment

On AROS One x86_64 / ABIv11, from a pool `v11` machine: a user searches for a
package, installs it and removes it without a Shell; the window stays
responsive throughout; cancelling a download leaves no installation and no
`.part` in the cache; the Cancel button is disabled from the extract phase
on. Evidence as everywhere in this repository: run reports with the binary's
hash, screenshots as illustration only.

## What is already known about the target

- **`muimaster.library` is on both systems.** Line 47 of both
  `docs/spikes/twenty/aros-one-1.3-libs.txt` and `mainline-libs.txt`. A Zune
  front end runs on AROS One and on mainline with no `requires_system` entry.
- **The v0.1 target is ABIv11 / AROS One x86_64.** Build with
  `src/build.sh`'s toolchain. A mainline build is an extra, not a gate.
- **The index has 23 packages, all verified installed/started/removed**, and
  a further 156 unreviewed skeletons that are *not* in `index.json`. The
  front end sees only what the index publishes.
- Every package carries a drawer icon (`icon` in the index); the client
  installs it beside the drawer. Icon **images** for a storefront tile are a
  separate question: nobody has counted how many archives include one, and
  `docs/backlog/storefront-proposal.md` says why that count must come before
  the design.

## What not to build into the front end

- **No logic.** Version comparison, ABI decisions, what to keep on removal:
  all of it is in `libpkg` and none of it belongs in the window. The MVP's
  words: the fronts are "thin fronts of a few hundred lines each and contain
  no logic of their own".
- **No second index reader.** Use the rows.
- **No `SYS:` installs, no `LIBS:`.** Deferred in v0.1 by decision; the
  proposal's `install.external` will carry it when it comes.
- **No "New releases" row.** Upload date is not a release date (measured:
  `protrekkr`'s newest upload is older software). "Recently added" is honest.

## Where to look

- `src/libpkg/pkg.h`: the contract, with the reasoning in comments.
- `src/libpkg/entries.c` + `tests/test_entries.c`: the rows, host-tested.
- `tests/json-progress.script`: the same contract exercised on AROS One,
  including all cancel outcomes; `docs/reports/2026-09-23-rows/` is the run.
- `docs/backlog/storefront-proposal.md`: what a browsing UI should and should
  not claim from the data it has.
- `docs/backlog/package-manager-mvp.md` §5: PkgManager's original scope.
