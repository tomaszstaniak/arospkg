---
title: "Is there a place for this, next to Rust, Go and Python?"
status: analysis
state: measured where it can be
created: 2026-09-11
updated: 2026-09-11
---

# Where we fit

The question, asked 2026-09-11: languages like Rust, Go and Python bring their
own package managers. Is there a real place for arospkg beside them?

## What the catalogue actually contains

Counted from `FULLINDEX.readme`, whole archive, all architectures:

| language | in AROS Archives |
|---|---|
| **Rust** | **nothing at all** |
| **Go** | **nothing at all** |
| Python | 2.5.2 and 2.7.18 (ABIv11 x86_64); 2.5.2, 3.3.7 (i386). **Nothing for mainline x86_64** |
| Perl | 5.7.2, a release from 2001 |
| Ruby | 1.8.4 / 1.8.7, i386 only |
| FreePascal | i386 and arm, ABIv0 |
| Lua | present, mostly as Zune GUI front ends |

So for the three languages in the question, on the platform we target, the
existing catalogue offers **nothing**.

## Why cargo, pip and go modules do not cover it here

The instinct is right on Linux and wrong here. `cargo`, `pip` and `go mod` are
**build-time tools that run on the target machine**. On AROS x86_64:

- there is no native Rust toolchain; `rust-aros` cross-builds from a Mac or PC;
- there is no native Go toolchain; `go-aros` is the same shape;
- Python has no working 3.x at all on mainline, so `pip` has nothing to run in.

Every one of these produces a **cross-built binary that arrives from outside**.
Nothing on the AROS side fetches, verifies or installs it. That gap is exactly
what a binary package manager is for, and it is not one those ecosystems fill on
this platform: they fill it on Linux because the toolchain is there.

**Python is the partial exception worth naming.** If CPython 3 runs on AROS,
`pip` genuinely does take over part of the job for pure-Python packages. Native
extension modules still need a compiler on the target, which does not exist. So
Python is the one case where the language ecosystem might legitimately do some
of what we do.

## The place, stated plainly

Not the one the MVP designed for.

The MVP is an **overlay over an existing catalogue**. Filtered for mainline that
catalogue is four x86_64 packages, and for these languages it is empty. As an
overlay for mainline, there is almost nothing to overlay.

But this workspace is already producing software for mainline that has **no
distribution channel whatsoever**:

| | targets |
|---|---|
| `rust-aros` | mainline x86_64 |
| `go-aros` | mainline x86_64 |
| `aros-python3` | mainline, started 2026-09-11 |
| `aros-xterm` | mainline x86_64 |
| `aros-hardcode` | native AROS |
| `dosboxer`, `burntime-rs`, `aros-tls` | mainline |

None of it is in any archive. Anyone who wants it gets a zip from a person. That
is the gap, and it is a real one.

So the honest answer is: **there is a place, and it is not overlay, it is
distribution.** Not "index someone else's old binaries" but "be the way software
built for mainline reaches a machine". Which is a different product from the one
in section 1, though it shares almost all of its code.

## What that would change

- The **package standard proposal** stops being a nice-to-have. If we publish
  our own software we control its manifests, ship the standard format from day
  one, and the overlay-maintenance argument that has haunted this project
  disappears: there is nothing to maintain an overlay *of*.
- **Source or binary** becomes answerable. Mainline promises no backwards
  compatibility, so published binaries rot. But if the publisher is also the
  builder, republishing on an ABI break is a build job, not an archaeology job.
- The **success criterion changes**. "Twenty packages install without error" is
  unreachable on mainline. "Every program this workspace produces installs with
  one command" is reachable, and worth more.

## The risk, which is not small

A package manager whose catalogue is *the software its own authors wrote* is
small and self-referential. It becomes a real thing only if other people build
for mainline, and the four-package catalogue is evidence that today they
mostly do not.

That is the chicken-and-egg the forum named: mainline has little software partly
because it is a moving target, and it stays a moving target partly because
little software pressures it to stop moving. Being the distribution channel is
a bet that the first half can be improved.

**Nothing here is decided.** It is an argument that the project's real
opportunity is somewhere other than where it started, offered so the choice is
made deliberately.

## The follow-up: would AROS One ever have this, and should we pin our own base?

Asked 2026-09-11: everything that accelerated our work is in mainline. Will a
distribution that promises backwards compatibility ever carry it, and if not,
should there be something between mainline and AROS One?

### What we actually needed, and where it was

Not a preference. Each of these was load-bearing for work done in this
workspace, and each is mainline:

| what | why it mattered |
|---|---|
| `fd.library` actually built | without it, sockets and posixc files share a number space and TLS reads the console. Our PR #1122 |
| posixc `ioctl` sign-extension fix | without it, **every** TLS connection fails before the handshake. Our PR #1124 |
| contrib OpenSSL **4.0.1** | AROS One ships 1.1.0h, from 2018 |
| `entropy.resource` / `getentropy()` | AROS One has libc `rand()`; TLS on it is on sand |
| mbedTLS 3.6.7 | not on AROS One |
| thread-local storage (`aros-tls`) | Go needs it; a kernel change |

Two of those are fixes **we made**, and one of them is a 64-bit sign-extension
bug that had to be fixed in the OS before a program could download a file.

### Why a backwards-compatible line will structurally lag

This is not a criticism of AROS One, it is the arithmetic of its own promise.
`deadwood2/AROS` states that **every commit to Core must preserve backwards
compatibility**. A change that alters a calling convention, a structure layout
or a library's behaviour cannot land under that rule without a deliberate break.

So the changes most useful to us (the ones that fix how the system talks to
programs) are exactly the changes such a line adopts last, or through a
coordinated break. Expecting fresh mainline internals *and* a compatibility
guarantee is asking for two things that trade against each other.

### The idea, and what it actually is

A line between the two: takes from mainline, steadier than mainline, fresher
than AROS One.

**A distribution is, functionally, a pinned ABI.** That is the whole mechanism
behind AROS One's promise: not magic, a fixed point. Mainline declares
`ABI major -1` and promises nothing because it is *not* pinned. Pin a snapshot
and binaries built against it keep working on it, without mainline having to
promise anything at all.

Which means the useful part is **much cheaper than a distribution**:

| version | what it costs | what it buys |
|---|---|---|
| **A named base**: pin a dated mainline snapshot; every package records which snapshot it was built against; `apkg` warns or refuses on mismatch | one field in the manifest, one check in the client | the ABI-drift problem solved *for that base*, which is the only place it bites |
| **Publish the base**: we already build mainline from source; publish that ISO as "the base arospkg targets" | a file and a hash | users get a known-good pairing instead of guessing |
| **A distribution**: installer, branding, release cadence, updates, support, tracking mainline merges | a project larger than this one | a third line in a small community |

The first two give most of the benefit. The third is a different commitment and
should not be entered by drifting into it.

### The honest objection

A third line fragments a small ecosystem further, and the community has two
already. Before proposing one it is worth asking on the forum whether a *pinned
mainline base* is something others want, because if two or three people build
against the same snapshot, it is a base; if one does, it is a private build with
a grand name.

### Who actually works on mainline (measured 2026-09-11)

From the GitHub API, `aros-development-team/AROS`, the twelve months to
2026-09-11. Our local clone is shallow and says 30 commits; that is an artifact
of how it was fetched, not a fact about the project.

```
2918 commits    35 distinct authors    15 with 10+ commits    5 with 50+
```

| | commits | share |
|---|---|---|
| **Kalamatee** (= Nick Andrews, one person, same address) | **1746** | **60%** |
| **deadwood** | 378 | 13% |
| Bo Ståle Kopperud | 307 | 11% |
| Nicolas Ramz | 72 | 2% |
| Bernie Innocenti | 70 | 2% |
| everyone else (30 people) | 345 | 12% |

`aros-development-team/contrib`, same period: **264 commits**, of which
Kalamatee 218.

Three things follow that matter here.

**Mainline is alive and fast.** 2918 commits a year is not a dormant project.
That is why it has the features we needed, and equally why it is a moving
target.

**It is one person and a small circle.** 60% from a single contributor, 83%
from three. That is not a criticism; it is load-bearing for any plan that
assumes the platform will be there in a particular shape.

**deadwood is the second-largest mainline contributor** while maintaining the
ABIv11 fork. The two lines are not rival camps with a wall between them:
the person maintaining the stable line is doing an eighth of the work on the
fast one. Any "third line" framing should start from that rather than from an
assumed split.

### So would a pinned base have takers?

Honestly: probably very few, at first.

The 15 people with 10+ commits are working **on** AROS, not writing
applications **for** it. The population that would use a package manager is the
one uploading to AROS Archives, and the measurement there is stark: **171
x86_64 packages for ABIv11, 4 for mainline.** Application authors are on the
distribution side, overwhelmingly.

Which means a pinned mainline base would initially serve this workspace and
perhaps a handful of others. That is not nothing: this workspace is producing
Rust, Go, Python 3, a terminal and more, none of which has anywhere to go, but
it should be said plainly rather than dressed up as community infrastructure.

## What actually binds us to mainline: the network

The sharpest version of the question, 2026-09-11: we would need Rust and Python
for ABIv11 too. Can we?

### One correction first

We did **not** patch OpenSSL. We wrote such a patch, and it was withdrawn: it
worked by masking two platform bugs, which were then fixed at their source and
merged into mainline: `fd.library` never being built (PR #1122) and the posixc
`ioctl()` sign-extension (PR #1124). **Stock contrib OpenSSL 4.0.1 needs no
patch on mainline**, because of those two merges.

That distinction is exactly what decides the ABIv11 question, because deadwood
said on the same forum thread that on **his** tree the socket patch *is* needed:

> If (1) [`deadwood2/AROS`] then yes, you are correct and such change is needed
> because of split of file descriptors between C library and network stack.

So the bug our withdrawn patch worked around is still live on the ABIv11 side.

### The dependency map

| what we build | what binds it | portable to ABIv11? |
|---|---|---|
| **Go** (`go-aros`) | `aros-tls`: **38 lines of kernel patch** | **No.** Needs the patch in deadwood's kernel, or we ship a modified ABIv11, which is no longer AROS One |
| **Python 3 + `ssl`** | contrib OpenSSL 4.0.1, sockets as file descriptors | Hard. AROS One ships OpenSSL **1.1.0h from 2018**; a 4.x build for ABIv11 would also need the socket patch deadwood says his tree requires |
| **arospkg itself** | the same TLS stack: its whole job is verified download | Same problem as Python |
| **aros-hardcode** | TLS to a real API | Same |
| **Rust** (`rust-aros`) | nothing network-shaped in the `no_std` path | **Already ABIv11**, measured today: runs on AROS One, crashes on mainline |

`aros-python3`'s own README states the dependency plainly: *"sockets as file
descriptors: yes, since PRs #1122/#1124"* and *"OpenSSL 4.0.1 in the SDK"*.

### The pattern, and the irony

**Everything that touches the network is bound to mainline**, because mainline
is where the TLS stack works, and it works there partly because we fixed it.
That covers `apkg`, Python 3's `ssl`, and `hardcode`: the three most interesting
things this workspace produces.

Rust is the exception and it cuts the other way: the `no_std` path needs none of
that, which is why `rust-aros` could sit on ABIv11 without anyone noticing.

So the irony is exact: **a package manager cannot work without the network, the
network works on mainline, and mainline has four packages.** That is not an
argument for abandoning mainline. It is the reason the answer to "where do we
fit" was *distribution, not overlay*: the thing we are best placed to
distribute is software that only exists because the platform got fixed
underneath it.

### aros-xterm: measured, and narrower than feared

Not our project; the facts below are a compile test we ran plus checks the
author made on the AROS One ISO itself.

**Compilation.** Against the AROS One SDK, **40 of 41 source files compile**.
One does not:

```
aros_break.c  ->  resources/task.h: No such file or directory
```

That file uses `AddTaskNotifyHook` from `task.resource`, which mainline's SDK
declares and AROS One's does not. Worth noting because it is a trap: 
`docs/shell-interrupt.md` describes runtime capability detection, which reads
like graceful degradation on any target, but the declarations exist only in
mainline's SDK, so against AROS One the file does not compile at all. The
runtime check guards an old mainline guest, not a different line.

**FreeType is present.** Checked on the AROS One 1.3 64-bit ISO directly, not
in the SDK: `Libs/freetype2.library` version **6.9 (15.10.2025)**, together with
`VeraMono.ttf` and `OpenSans-Regular.ttf`. So the rendering dependency this
project worried about is satisfied.

**What is still unknown, stated as the author put it.** Presence of the library
does not prove every call is compatible or that the renderer works, and
compilation says nothing about Shell behaviour, DOS packets, the clipboard or
session teardown. A short run is what would settle it, and nobody has done one.

**`task.resource` is not deliberately mainline-only.** The code needs
task-removal notifications so that a signal is never sent through a stale
pointer. A backport to AROS One's base is potentially possible but means
checking integration with task creation and removal, not copying a library.

**And the loss is narrower than "Ctrl-C".** Without the mechanism what goes is
*safe local signal forwarding*. Raw mode's byte `0x03`, which is what SSH uses,
is handled separately and is unaffected.

### What would change the answer

If the two merged fixes, or their equivalents, reached the ABIv11 line, most of
this map changes at once. That is a conversation with deadwood rather than a
build task, and he is the second-largest mainline contributor, so it is not a
far-fetched one. Go would still need its kernel patch.
