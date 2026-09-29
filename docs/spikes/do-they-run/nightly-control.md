---
title: "The control: an AROS we did not build"
status: complete
state: measured
created: 2026-09-11
updated: 2026-09-11
---

# The control

Every crash had been measured against **our own** mainline, built from source on
macOS. That left two explanations standing and nothing to separate them: either
the archive binaries are wrong, or our build is. A program compiled with our SDK
running on our build proves neither, because it is what you would see either
way.

So: the same binaries on an AROS we did not build.

## What was used

| | |
|---|---|
| Image | `AROS-20260910-pc-x86_64-boot-iso.zip`, official nightly, `nightly2/20260910` |
| Published MD5 | `8fa96620701bc303bbc4a3bc54915b06`: **matched** |
| ZIP SHA-256 | `4e3d2d9ee8cba91eff80db3126fb82d1327c5a1100e14b87273bac35974146ec` |
| ISO SHA-256 | `3679e3ceb2135f81dd1ce44c0d57e9bc3443f5e2c5cb3177529b91cf7a39003f` |
| Booted | live, no installation, its own QEMU with its own sockets, no shared machine touched |
| Our ISO, for contrast | `cc0ecd5d6abd622547c58ac8001baf2b08d4d5b009c204f35e279f7b86653eb4` |

Binaries, byte-identical to the ones that crashed on our build:

| | SHA-256 |
|---|---|
| `Zaphod` (archive) | `316b065a6de3bd92cd43ae6defd8c42a867d7950f357672ef1c021d033a6c4d8` |
| `Prince` (archive) | `bdb2ba8f21f390614a8409a01e030c9ad2b598924fb074490aa90e888ce029a6` |
| `abitest` (ours) | built with our mainline SDK, GCC 10.5.0 |

## The result

**The archive binary crashes on the official nightly, identically.**

```
Task : RAM:Zaphod
Error: 0x80000003 - Illegal address access
Module Kickstart ELF Segment 1 .text
Function Exec_49_FindTask + 0xD
```

Same error, same function, on a build nobody here made.

**Our binary runs on the official nightly.**

```
FindTask      0000000003870620  RAM:abitest
OpenLibrary   intuition.library      ok
   ... all nine libraries ok
OpenWindow    ok
abitest: finished without crashing
```

Which gives the whole picture:

| | our build | official nightly |
|---|---|---|
| **our binary**, our SDK | runs | **runs** |
| **archive binary** | crashes | **crashes** |

## And then AROS One: they run

Same binaries, same hashes, a **private copy** of the AROS One disk so the
shared machine was not touched, payload on the same plain-ISO9660 disc.

**Both run.**

`Zaphod` opens its full interface: window titled *ZAPHOD 1.3 (09.11.2010),
Screen name: "Workbench"*, the ARTSOFT logo, search and replace fields, the hex
pane. `Prince` opens its own window, initialises SDL, and stops with *"Can't
load sprites from resource 700. The last opened data file is: PRINCE.DAT"*,
which is the program working and missing its data directory, because only the
executable was copied. Not a crash.

So the whole matrix:

| | our mainline | official nightly | AROS One |
|---|---|---|---|
| **our binary**, our SDK | runs | runs | not tested |
| **`Zaphod`** (archive) | crashes | crashes | **runs** |
| **`Prince`** (archive) | crashes | not tested | **runs** |

**That is the boundary.** These binaries are built for AROS One, or for
something compatible with it, and mainline cannot run them. It is not our
build, it is not the nightly, and it is not a broken upload.

### A second thing, noticed in passing

`Prince`'s own window says **v1.24 RC**. The catalogue says 1.23, and so does
our index, because the manifest took the version from the catalogue. The
catalogue's version field is the uploader's description, not the program's
version, and the two can disagree. Worth knowing for any overlay that treats
`version` as meaningful, though section 1 already refuses to order it, which
now looks like the right call for a second reason.

## What this settles, and what it does not

**Our build is not the outlier.** Our SDK and the official nightly agree with
each other; the archive binaries disagree with both. The explanation "something
about how we built AROS" is dead, and with it the idea that this is a local
configuration or source-revision problem.

**What the archive binaries were built against is now known well enough to act
on**: AROS One runs them, mainline does not. The *mechanism* is still not
identified: the crash signature remains a symptom, not a cause, and naming it
"the ABI" would still be a guess dressed as a finding. But the compatibility
boundary is measured, and it runs between the distribution and mainline rather
than through anything we did.

For arospkg the conclusion does not depend on the remaining question: **the
client must be able to tell whether a package will run before installing it,
and `x86_64-aros-v11` in a filename does not tell it.** No static difference has
been found yet: same compiler version, near-identical ELF sections, no marker
in the symbol table, so this is an open problem, not a task.

## Two things learned about the harness

- **A Joliet ISO hangs AROS's CD handler.** `hdiutil makehybrid -iso -joliet`
  produced a disc where `Copy` never returned and wedged the machine twice.
  The same files as plain ISO9660, with `-default-volume-name`, copied fine.
  The parent repo's scar about a stuck CD read is the same family.
- **A live CD does not mount the vvfat drive**, so the usual way of handing
  binaries to a guest does not work there; `info` shows only the CD and RAM.
  A second CD is the way in.
