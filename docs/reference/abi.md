---
title: "ABI: what it means here, and what it means for our binaries"
status: reference
state: measured
created: 2026-09-11
updated: 2026-09-11
---

# ABI on AROS x86_64

Written because this cost the project two days and then rewrote its scope, and
because it determines how we build and release **our own** binaries, not just
which packages we index.

## What an ABI is, in one paragraph

An **API** is how a programmer writes a call: `OpenLibrary("dos.library", 0)`.
An **ABI** is how the compiled binary performs it: which register holds the
library base, where arguments go, how structures are laid out in memory. Two
systems can share an API, an architecture and a compiler and still be unable to
run each other's binaries, because the compiled-in agreement differs. That is
why "it's x86_64 and so am I" is not enough.

## The two lines on AROS

From **deadwood** (AROS developer), AROS forum, 2026-09-11:

> The role of the `x86_64-aros-v11` suffix is exactly that: to distinguish
> between the ABIv1 and ABIv11 binaries and let them live together on the
> archives. The `-v11` marks binaries usable with current distributions like
> AROS One. Archives without such marker are intended for ABIv1.

| | ABIv1 | ABIv11 |
|---|---|---|
| Used by | **mainline** AROS | current distributions, e.g. AROS One |
| Archive filename | `x86_64-aros`, or no marker | `x86_64-aros-v11` |
| Our toolchain builds | this | not this |

**The numbers are not a version ladder.** "11" is not "newer than 1" in the
sense of an upgrade path; they are two lines. Why they are named this way is not
something this document knows, and nothing is invented here to explain it.

## Is it like a Linux distribution's testing / stable?

Partly, and the part where it breaks is the part that decides this project.

**Where the analogy holds.** The two lines really do differ in cadence and
intent. Publicly stated in the AROS community and on deadwood2's repository
(reported, not measured by us):

- **mainline** (`aros-development-team/AROS`) is aimed at *"cutting edge,
  experimentation and newest features"*.
- **ABIv11** (`deadwood2/AROS`) is aimed at *"stability and repeatability"*, and
  its master branch is described as *"Stable and always backwards compatible"*,
  with the rule that **every commit to Core must preserve backwards
  compatibility**.
- The two repositories **synchronise source every couple of months**.

So yes: one moves fast, one is a stable line, and code flows between them. That
is the shape of testing-versus-stable.

**Where it breaks, and it matters.** In a Linux distribution, testing and stable
share an ABI (glibc holds still), so a binary built for stable generally runs
on testing too. The channels differ in *which versions* of packages they carry,
not in whether the binaries are compatible at all.

Here **the ABI itself is what differs**. A binary does not cross in either
direction, which we measured in both: eight archive packages crash on mainline,
and our own probe crashes on AROS One. These are not two channels of one
distribution; they are two lines whose binary contract is deliberately
different, sharing source through periodic merges.

A closer Linux analogy is **glibc versus musl**: Debian and Alpine. Same
upstream source, both current, both legitimate, and every binary has to be built
for one or the other.

### The consequence for a package manager

This is not a matter of taste. An overlay index of URLs and hashes, which is
what section 1 specifies, is only sound if the binaries it points at keep
working. That property has a name, and exactly one of the two lines offers it:

| | mainline (ABIv1) | ABIv11 |
|---|---|---|
| Backwards compatibility | **none promised**; reports `ABI major -1` | **promised by policy** |
| Packages in AROS Archives for x86_64 | 4 (one application) | 171 |
| Where users are | development | AROS One and other distributions |
| Suits an index of prebuilt binaries | **no** | **yes** |

The 2026-09-03 decision to target mainline was made for good reasons and with
the information then available. What was not known is that mainline is the one
line that explicitly does not promise its binaries will keep running, which is
the single property a prebuilt-binary package manager depends on.

That does not decide anything by itself: developing on mainline is still where
the platform moves, and a manager could target mainline by building from source
instead of distributing binaries. But the trade is now visible, and it is a
scope decision rather than an engineering one.

## Measured, not inferred

A probe calling `ArosInquire()` was built with our mainline SDK and run on both
systems.

**On the official AROS nightly of 2026-09-10:**

```
ABI major     -1
release       1.12
architecture  pc-x86_64
build date    Sep 10 2026
```

**On AROS One:** it crashes before printing anything:
`Illegal address access` in `GetTaskEntry`, the same shape as every archive
binary crashing on mainline. **A binary cannot ask the other system what it is,
because it cannot run there.** The boundary is symmetric, and this is the
clearest demonstration of it we have: our own 26 KB probe, doing nothing but
asking a question.

### `ABI major -1` is the important number

Mainline reports **-1**, not 1. The source says why:

```c
#define AROS_ABI_VERSION_MAJOR  -1   /* Change only value, name is used in external script */
```

Mainline **does not declare a stable ABI version**. That is consistent with
deadwood's second warning:

> ABIv1 does not yet maintain backwards compatibility. If you are building
> third party software (one that is not re-build with every nightly) it may make
> sense to keep sources at hand and publish refreshed binaries whenever
> required (same goes for aarch64 of course).

So on mainline the useful identity is not an ABI number: it is **release plus
build date**. A binary built against a nightly from some months ago may not run
on today's, and nothing in the filename or the system will say so in advance.

## What this means for the packages we index

`tools/import_catalogue.py` filters on ABI as well as architecture. Targeting
mainline, the offered set is 7 packages, not 168; the other 171 x86_64 entries
are ABIv11 and belong to distributions. See
[`../spikes/do-they-run/the-answer.md`](../spikes/do-they-run/the-answer.md).

## What this means for **our** binaries: the part that is easy to miss

`pkg` is built with the mainline SDK, so:

1. **It runs on mainline only.** It will not start on AROS One, for the same
   reason ZapHod will not start on mainline. Anyone told "here is a package
   manager for AROS" and handed our binary on a distribution gets an
   `Illegal address access` and no explanation.
2. **It has no long shelf life.** ABIv1 offers no backwards-compatibility
   guarantee, so a `pkg` binary published today may stop working against a
   mainline from some months hence. A release is a snapshot against a moving
   target.
3. **So a release must say what it was built against**, not merely "AROS
   x86_64". At minimum: ABI line, architecture, and the SDK or nightly date.
   `pkg --version` already prints the toolchain, SDK path and a source id;
   what it does not yet print is the **ABI and the SDK's build date**, which are
   the two facts that actually determine whether it will run. That is a gap.
4. **Two binaries, or none.** Serving distribution users means building a second
   binary with a distribution SDK. That is a real cost and a real decision, not
   a recompile flag.
5. **Keep the sources and rebuild**: deadwood's advice applies to us exactly as
   it applies to any third-party author. Our source is in git; what we do not
   have is a policy for rebuilding and republishing when mainline moves.

## Practical rules

- Never describe a build as "for AROS x86_64". Say **ABIv1/mainline** or
  **ABIv11/distribution**, and give the SDK date.
- A binary that runs here proves nothing about the other line. Test on the
  system you are claiming, every time.
- `ArosInquire()` is the way to ask at runtime, and `pkg` should use it before
  installing anything, but it can only ever describe the system it is already
  running on.
