---
title: "Proposed package standard"
status: proposal
state: direction
created: 2026-09-06
updated: 2026-09-12
related:
  - ../reports/2026-09-12-requires-system-onetest.md
  - package-manager-mvp.md
  - building-from-source.md
  - ../spikes/overlay-sample/README.md
  - ../adr/0001-relationship-to-aaedt.md
---

# Proposed package standard

> **The definition has moved to [`../rfc/0001-package-format.md`](../rfc/0001-package-format.md).**
> This document keeps the reasoning and history behind it. Where the two
> differ, the RFC is the current proposal. For what the client does today,
> see [`../reference/metadata.md`](../reference/metadata.md).

## Document contract

This is a **direction for later iterations**, not a change to the MVP. Nothing
here is scheduled, nothing here blocks the `libpkg` API, and nothing here alters
what section 2 of [`package-manager-mvp.md`](package-manager-mvp.md) says. It
exists so the shape is written down while the reasoning is fresh, and so
section 3's API is designed with it in view rather than retrofitted around it.

It is complete when a handful of our own programs install from metadata carried
**inside the package**, with no hand-written overlay entry (see *First test*).

## Why propose a standard at all

The MVP treats AROS Archives as a catalogue it does not control. Every package
needs an overlay manifest written by us: name, version, architecture, URL,
hash, dependencies. That works, and it is the only thing that *can* work for
1894 existing uploads whose authors are not going to revisit them.

But it puts the cost in exactly the wrong place. **The client is written once;
the overlay is maintained forever.** Every new upload, every version bump, every
dependency that changes is our work, indefinitely, for software we did not
write. That is what killed Grunch, and it is the risk this project has carried
since the first section.

An author who wants their program to install cleanly currently has no way to say
so. A package standard gives them one: put a manifest in the archive, and the
overlay entry becomes unnecessary. The maintenance cost moves to the person who
already knows the answers and who is already making the archive.

Neither replaces the other. The overlay is how the existing catalogue works; the
standard is how new work can avoid needing one.

**Measured 2026-09-07**, on four ordinary uploads taken through the whole
pipeline ([`../spikes/overlay-sample/README.md`](../spikes/overlay-sample/README.md)):
two of the four could not be approved at all. One needs a dependency the archive
does not contain; one is a developer SDK the manifest format cannot describe. A
third would have lost its Workbench icon to a plausible-looking `subdir`. Every
one of those is something the author knew and the catalogue does not record,
which is the argument for this document, stated in numbers rather than
principle.

## What this is not

- **Not a mirror.** AROS Archives stays the source of binaries. We do not copy
  or self-host them, under a standard or without one.
- **Not a new archive format.** A standard package is an ordinary ZIP or LHA
  with one extra file; and since 2026-09-20 the client reads both, so that
  sentence is a description rather than an aspiration. It stays readable by
  hand, by `UnArc`, and by anyone who
  has never heard of `apkg`.
- **Not an installer runtime.** See *No scripts, first version*.
- **Not a compatibility claim about other archives.** Aminet and OS4Depot are
  plausible future *sources*; supporting a source says nothing about whether the
  programs in it run on AROS. Most Aminet software is 68k AmigaOS and most of
  OS4Depot is PPC OS4. Reading their catalogues would be a metadata feature, and
  would need architecture filtering that is honest about what it cannot promise.

## Minimum scope

A manifest at a fixed path inside the container: proposed `.arospkg/manifest.toml`,
TOML for consistency with the overlay manifests we already write.

### Identity, version, revision

```toml
schema   = 0                 # this format is a draft; 0 means proposed, not settled
id       = "grafx2"          # lowercase, ^[a-z][a-z0-9._-]{0,63}$, the registry key
name     = "GrafX2"          # what a human sees
version  = "2.9"             # upstream's own version string, opaque
revision = 3                 # optional; the third AROS release of that upstream version
```

**`id` establishes identity.** `version` does not. Two entries with the same
`id` are the same program however their version strings differ; that is what
makes the rest of this section a question of ordering rather than of naming.

`version` carries what upstream calls its release, and stays **opaque, compared
only for equality**: the MVP's rule, adopted because `1.10` loses to `1.9`
lexically and no ordering we invent will be right for every upstream. Where the
program is its own upstream, because nobody else numbers it, it carries our
release number.

`revision` counts **this port's releases of that upstream version**: port code
fixes and packaging fixes alike. It is scoped to the port, not to the platform:
it does not count every AROS release of the program ever made by anybody, and
two independent ports of GrafX2 2.9 each start at 1.

One counter, not three: every published correction raises it, whether what
changed was the program, the icon or a line of the manifest. It is an integer
and it *is* ordered, because we assign it.

```
first public release of the port       →  version = "2.9",  revision = 1
upstream still 2.9, a later release    →  version = "2.9",  revision = 2
upstream moves to 3.0                  →  version = "3.0",  revision = 1
```

Revisions compare **only within the same `version`**. Displayed as `2.9-aros2`,
where the suffix is derived for display and not stored.

**`revision` is optional and its absence is normal**, meaning no separate AROS
release: a program that is its own upstream usually needs only `version`. It is
not reserved for ports either: a native project republishing the same version
can use it.

Two earlier statements in this document were wrong and are withdrawn. It called
`revision` "our packaging revision", which is too narrow: `aros1` can be a
genuine port release with code changes in it, not a manifest fix. And it argued
that a suffix in `version` would make `2.9-aros1` "a different program" from
`2.9`: identity is `id`, and both may well be releases of the same application.
The real objection to the suffix is narrower: it merges two facts into one
opaque string and throws away the counter that can be ordered.

What none of this settles is ordering **across** upstream versions: whether
`3.0` supersedes `2.9` is the upgrade mechanism's problem, and deciding it is
not a prerequisite for the current release.

### Platform, architecture, ABI

```toml
[[targets]]
os   = "aros"
arch = "x86_64"      # x86_64 | aarch64
abi  = "v11"
```

Architecture is load-bearing, not decorative: the client must skip what it
cannot run. **ABI belongs here because it has already bitten us**: an AROS One
binary passes `SysBase` in `%r12` and a mainline binary in `%rdx`, so mixing
them crashes inside `AllocMem` with a backtrace that reads like a broken
startup. A package that cannot state which ABI it was built against cannot be
checked, and the failure it produces is one of the least legible on the
platform.

Multiple `[[targets]]` blocks mean one archive serves several; a client picks
the matching one or refuses.

### Dependencies, system requirements and conflicts

```toml
[[depends]]
id  = "sdl2"
min_revision = 3          # optional

# What the MACHINE must already provide. Implemented 2026-09-12; this is the
# one field in this document the client actually reads.
[[requires_system]]
type = "library"
id   = "SDL.library"

[[conflicts]]
id     = "dosbox-svn"
reason = "installs the same LIBS:sdl2.library with different content"
```

Dependencies are simple and required, as in the MVP.

**`requires_system` is not a second spelling of `depends`.** "No package in the
index provides X" is fixable by publishing a package; "this machine does not
have X" is not, and telling someone to wait for the second as though it were
the first is a lie a package manager should not tell. The separation was forced
by a real package: `sdllopan` needs `SDL.library`, `crt.library` and
`stdlib.library`, none of which is or can be a package: they are parts of a
distribution. It stayed unpublishable for five days because the format could
only express the wrong one of the two.

The client's three outcomes are specified in
[`package-manager-mvp.md`](package-manager-mvp.md#system-requirements-are-not-package-dependencies)
and measured in
[`../reports/2026-09-12-requires-system-onetest.md`](../reports/2026-09-12-requires-system-onetest.md).
The one worth repeating here is **undetermined**: a requirement the client
cannot decide installs anyway, warns, and is recorded against the install. A
standard that only admitted yes and no would force every implementation to
guess, and they would not all guess the same way.

**Conflicts are new**, and they exist because section 2 already discovered the
situation they describe: two packages providing the same external path with
different content is a hard error that aborts an install. Today that is found at
install time; a declared conflict lets it be found at resolve time, with a
sentence explaining it.

### Declarative installation

```toml
[install]
subdir = "DOSBox"                      # what inside the archive is the package
icon   = "DOSBox.info"                 # the drawer icon, BESIDE subdir in the archive

[[install.external]]
source      = "libs/sdl2.library"
destination = "LIBS:sdl2.library"
```

`icon` was missing from this document until 2026-09-20, and its absence had a
consequence: the first package built against this proposal (Micropolis, ours)
set `subdir = ""` to keep the whole archive root, because that was the only way
to stop the drawer icon beside the drawer from being discarded. The cost is
that everything else at the root, including `.arospkg/` itself, becomes part of
the installed package. With `icon` stated separately, `subdir` can name just the
drawer. The client has installed a drawer icon this way since 2026-09-19.

`destination` is restricted to the approved list: `LIBS:` and `Fonts:` in the
MVP. Everything else goes into the package's own directory. This is the same
model section 2 installs today; the manifest only lets the author state it
instead of us inferring it.

### Program files, configuration, user data

The distinction section 2 currently has to *infer* by hashing:

```toml
[[files]]
path = "dosbox.conf"
kind = "config"     # program | config | userdata
```

- **`program`**: ours; replaced on upgrade, deleted on uninstall when unchanged.
  The default, so most packages declare nothing.
- **`config`**: installed if absent, **never overwritten** on upgrade, kept on
  uninstall unless the user asks for a purge.
- **`userdata`**: saves, screenshots, downloaded levels. Never installed, never
  touched, never deleted.

This is the single largest practical gain. Section 2 protects user files by
comparing hashes and warning when something changed, which is correct but blunt:
it cannot tell an edited config from a corrupted binary, so it keeps both and
explains neither. An author saying `kind = "config"` once turns a warning into
correct behaviour.

### Where the source came from

```toml
[source]                        # optional; recommended for anything we build
repository = "https://github.com/someone/thing"
revision   = "a1b2c3d"          # a commit or tag, so the link is pinned
archive    = "thing-2.9-source.zip"   # equally valid on its own, or alongside
sha256     = "…"                      # pins the content rather than the location
built_with = "mainline-x86_64"  # which SDK profile
built_on   = "2026-09-10"       # that SDK's build date
```

Added 2026-09-12, after establishing that building on the target is not
available: nothing in the built mainline system compiles, and every native
compiler in the catalogue is i386 (see `building-from-source.md`). Software for
these architectures is **cross-built from a host**, and that is not likely to
change soon. So this table is provenance: it never feeds a build on the user's
machine, because there is nothing there to build with.

**The consumer is whoever maintains the index, not whoever installs.** When a
mainline change breaks thirty published binaries, somebody has to rebuild them,
and "find the source, guess the commit, guess the SDK" is an excavation. A
pinned reference turns it into a task. deadwood's advice is the reason:

> ABIv1 does not yet maintain backwards compatibility. If you are building third
> party software (one that is not re-build with every nightly) it may make sense
> to keep sources at hand and publish refreshed binaries whenever required.

A repository pinned to a commit and a source archive pinned by hash are **two
equal forms**, and they may appear together. Neither is a fallback for the
other: a complete source archive is a release artefact in its own right and does
not depend on the repository existing, being public, or keeping its history.

#### What this format does not do

**The manager is not the publisher.** arospkg is a client and a metadata layer
over AROS Archives; it does not host or repackage anybody else's binaries. The
obligations that come with publishing software (supplying corresponding source
where a licence requires it, complete and with its build scripts) belong to
whoever publishes the binary, discharged in that project's release process. When
that publisher happens to be us, for one of our own programs, it is still that
project's job and not a function of this format.

The same split is standard practice elsewhere and worth naming, because it is
the question this section kept getting wrong. Installing a binary package with
APT or DNF does not check that corresponding sources were published, and does
not make installation depend on it. Fetching source is a separate operation
against separate indices: `apt-get source` over `deb-src`, `dnf download
--source` for the source RPM. (Documented behaviour of both tools, from the
a reading of their documentation rather than a test run here.)

So, concretely:

- `[source]` is optional, and recommended rather than required;
- a client may display it; it does not check it;
- its absence never blocks an install and never produces a licence warning;
- installing never downloads sources.

#### What can and cannot be checked

Not "nothing", which an earlier draft claimed, and not correspondence either.
Checkable: that a reference resolves, that an archive matches its `sha256`, that
a revision is pinned rather than a moving branch, and, by review or by
building from it, that the sources are complete. What cannot be established
automatically is that those sources produced *this* binary; that would take
reproducible builds. The metadata is useful well short of that, and should not
be presented in the client or the tooling as proof that sources exist or
correspond.

#### Our own releases

For packages we publish ourselves, source archives go **beside** the package,
not inside it. Micropolis ships 5.7 MB of sources against a 2.4 MB package, and
bundling them would more than triple every install to deliver something the
installing machine cannot compile.

That is a rule for preparing our releases, not a validation rule: somebody
else's archive may already carry its sources, and that is no reason to refuse
it.

#### Provenance is not a compatibility test

These fields record what a binary was made with. They do not say whether it
still runs, and nothing here is an expiry date: a binary built a year ago may
work perfectly and one built last week may not.

The two lines need this differently. **ABIv11 promises backwards
compatibility**, so a published binary should keep working and rebuilding is
occasional. **Mainline promises nothing** and reports `ABI major -1`, so
rebuilding is the normal case and an index of prebuilt binaries is a perishable
good. On that side these fields are not a nicety; they are what makes an index
maintainable at all.

## No scripts, first version

**No install scripts, no pre/post hooks, no shell-outs.** A manifest declares
state; it does not run code.

The reason is section 2. Every guarantee there (that recovery can reconstruct
what happened, that unknown content is never deleted, that a rollback restores
the previous state) depends on the set of mutations being *known in advance and
enumerable in the plan*. An arbitrary script is a mutation of unbounded scope.
The moment one runs, the plan is incomplete, and recovery is reduced to hoping.

If a real case appears that declarative rules cannot express, it is a candidate
for a **new declarative rule**, not for an escape hatch.

## What this borrows, and from where

Neither compatibility nor imitation. Two systems solved parts of this before and
it would be silly to rediscover their vocabulary badly.

**Homebrew** ([formula cookbook](https://docs.brew.sh/Formula-Cookbook)):
package definitions as readable files in a Git repository, sources identified by
URL and checksum, and a test block that checks the installed thing works rather
than that files arrived. What we take: manifests in Git as the source of truth
with the index *derived* from them; checksums that are mandatory rather than
advisory; and the habit of treating "installs" and "runs" as two claims. The
third one is already load-bearing here: `installs_on`, `runs_on` and
`does_not_run_on` exist because two packages install perfectly on mainline and
crash on startup.

**RPM and DNF** ([RPM manual](https://rpm-software-management.github.io/rpm/manual/),
[DNF docs](https://dnf.readthedocs.io/)): worth naming separately, because they
are different things. **RPM** is the package format plus the low-level tool and
database that installs one. **DNF** is the layer above: it resolves
dependencies, manages repositories and decides what to fetch. What we take: the
explicit vocabulary (requires, provides, conflicts) and the format/manager
split itself, which is the same division as `libpkg` holding every decision
while `apkg` is only a front end.

An honest difference: in RPM's terms our `requires_system` entries are
requirements that nothing `Provides`, because on AROS the *distribution* is the
provider and no package expresses it. That is precisely why the two fields had
to be separate rather than one list with a flag.

What we are not copying: scriptlets and triggers (see "No scripts, first
version"), weak dependencies, epochs, and multiple installed versions of one
package. Each exists for a real problem none of which we have.

## Three layers, deliberately separate

Conflated in conversation, distinct on disk, and each can change without the
others:

| layer | what it is | who writes it | where it lives |
|---|---|---|---|
| **Package manifest** | what this package is and how it installs | the author, or us for an overlay package | inside the archive |
| **Repository index** | what exists, where to download it, which hash | generated by `tools/mkindex.py` | on the server |
| **Local registry** | what is installed on *this* machine and what it touched | `libpkg` | `<root>/db/installed/` |

The index is derived, never authoritative: a client must be able to rebuild its
view from manifests alone. The registry is machine-local and never shipped. The
manifest never contains a URL, because where a package is downloaded from is a
property of the repository, not of the package.

## A consequence worth naming: independent repositories

Not a proposal to build a distribution system, and explicitly not a commitment
to operate one. It is what the three-layer split above already implies, said out
loud so nobody has to infer it.

If a package's description travels **inside** the package, then publishing a
repository stops requiring us. An author, a community or a distribution could
keep a metadata repository of their own (plain static files on any web host, no
application server) and a client could read more than one, taking the entries
that match its ABI and architecture. Our own public index is already separate
from where the binaries live, so this extends a split that exists rather than
replacing anything. **AROS Archives remains the important source of existing
AROS software**, and the policy does not change: binaries are downloaded from
there and we neither mirror nor host them.

What that would need, none of it built, and the limits stated before the
features:

- **A SHA-256 in an index proves a download matches that index. It does not
  authenticate whoever wrote the index.** These are different guarantees and the
  first is routinely mistaken for the second.
- **A signature would not make a program good or safe** either. It would say who
  published the metadata, which is worth having and is not a quality judgement.
- Identifier collisions between repositories, and which source wins, with no
  silent substitution of one publisher's package by another's.
- Metadata freshness, and keeping a stale index from being served in place of a
  current one: a design problem, not a detail.
- Adding and removing a repository explicitly, by the user, with each package's
  origin visible afterwards.
- Optional stable/testing channels, which are orthogonal to ABI and must not be
  confused with it.

Any command syntax for this is **proposed, not working**. Nothing here is
scheduled, and the single-repository client is what exists.

## Relationship to the MVP

The MVP does not change. `arospkg` keeps installing existing AROS Archives
uploads through overlay manifests, and continues to for as long as the archive
holds software nobody is going to repackage.

The requirement this places on section 3 is recorded in
[`package-manager-mvp.md`](package-manager-mvp.md#a-requirement-carried-into-section-3):
**both manifest kinds parse into one internal representation, and everything
after the parser is identical**: the same resolution, install, registry and
recovery. A standard package that took a different path would need every
section 2 guarantee re-argued and re-tested for it, and the two paths would
drift apart the first time one was fixed.

So the standard is a **second front door to the same machine**, and that is the
whole of its cost.

## First test

*One field of this is no longer a proposal.* `requires_system` was implemented
on 2026-09-12 because a real package needed it, and it is described here with
the rest so the document stays one shape, but the client reads it today and
`sdllopan` is published on the strength of it. Everything else below is still
untried.

Not "the standard is finished" but: **several of our own programs install from
metadata carried in the package, with no hand-written overlay entry.** We
control those, they target mainline x86_64 and aarch64, and they will exercise
identity, target matching, an external file, a dependency and a `config` file
between them.

If that works, the standard is worth proposing to the wider AROS community. If
it does not, we have learned it cheaply on software we can change.

## Open questions

- **Multiple repositories**, per the section above: collision rules, source
  selection, metadata freshness and signing are each open, and each is a
  document of its own rather than a paragraph in this one.
- **Signing.** Deliberately out of scope here. The MVP verifies SHA-256 from the
  index, which protects against corruption and a tampered mirror but not against
  a compromised archive. Authorship signing is a separate document.
- **Where the ABI tag comes from.** `v11` is what we say today; whether mainline
  will keep a stable tag a package can rely on is not something this document
  can decide alone, and it is worth asking upstream before the format hardens.
- **Ordering across upstream versions.** `revision` orders our releases within
  one `version`; nothing orders `2.9` against `3.0`, and the upgrade mechanism
  will have to decide that. `min_revision` in `[[depends]]` inherits the gap:
  a revision floor means little without saying which version it counts within.
- **`revision` ownership** if a package is adopted by someone else, or if
  upstream starts shipping its own manifest with a different revision line.
- **Aminet and OS4Depot** as sources: reading their catalogues is tractable;
  deciding what to *claim* about the software in them is the hard part, and
  nothing should be added there until architecture filtering can be honest.
