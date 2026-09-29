---
title: "Do the packages we install actually run?"
status: open
state: measured, one test missing
created: 2026-09-10
updated: 2026-09-10
---

# Do they run?

## Why this was run

`STATUS.md` had carried a line for days saying that nothing in this project had
shown an installed package does anything at all. Installing files is not
evidence that a program works. This is the first attempt to find out, and it is
the first item of the agreed next round.

## What happened

Both packages in the public index were installed from it (`pkg update`,
`pkg install`, no manual preparation) on **mainline AROS x86_64**, and then
started.

**Both crashed immediately.**

```
sdlpop:   Task  SYS:PkgRun/sdlpop/Prince
          Error 0x80000003 - Illegal address access
          Module lddemon.resource  Function Lddemon_0_OpenLibrary + 0x25

zaphod:   Task  SYS:PkgRun/zaphod/Zaphod
          Error 0x80000003 - Illegal address access
          Module Kickstart ELF Segment 1 .text
          Function Exec_49_FindTask + 0xB
```

The installs themselves were clean: 1.4 MB and 218 KB fetched over TLS, both
redirects followed, both hashes verified against the index, both trees unpacked
with their `.info` files. **The package manager did its job and the programs
still do not start.**

## Eight of eight (2026-09-10)

The sample was widened to eight packages, chosen small and spread across
categories, and **every one of them crashes on startup**:

| package | category | crash site |
|---|---|---|
| `zaphod` | development/edit | `Exec_49_FindTask` |
| `sdlpop` | game/platform | `Lddemon_0_OpenLibrary` |
| `untangle` | game/puzzle | `Lddemon_0_OpenLibrary` |
| `ZuneCalc` | office/misc | `Exec_49_FindTask` |
| `ScreenTest` | demo/misc | `Exec_49_FindTask` |
| `PlaySID-GUI` | audio | `Exec_49_FindTask` |
| `SMB2-GUI` | network/samba | `Exec_49_FindTask` |
| `FFmpegVideoTool` | video/convert | `Exec_49_FindTask` |

Eight uploads, eight categories, different authors, and **two different
compilers**: `ZuneCalc` was built with GCC 6.5.0 where the rest report 10.5.0,
and it fails identically. All are tagged `x86_64-aros-v11`. Every failure is
`0x80000003 Illegal address access` with the PC inside a Kickstart module.

Three is an anecdote. **Eight across eight categories is a survey**, and the
"these particular uploads are unlucky" explanation does not survive it.

### The caveat is now resolved: see [`nightly-control.md`](nightly-control.md)

The same binaries were run on the **official AROS nightly of 2026-09-10**, live,
on a machine we did not build. The archive binary crashes there identically; our
own binary runs there. Our build is not the outlier. What follows below is the
reasoning as it stood before that test.

### The caveat that mattered most

**Our mainline is a build we made ourselves**, from source on macOS. Every one
of these results is against that one build. If our build is the unusual thing
here, then the catalogue is fine and we are the outlier, and nothing measured
so far distinguishes those two, because a program compiled with *our* SDK runs
on *our* build, which is exactly what you would expect either way.

The two tests that would separate them, neither of which has been run:

1. the same binaries on an **official mainline nightly**, rather than ours;
2. the same binaries on **AROS One**, which is what they were presumably built
   against.

Until one of those happens, the honest statement is: **eight archive packages
do not run on this mainline build**, not "the archive does not run on
mainline".

## A third package, from a different category and author

`untangle` (game/puzzle, 60 KB, unrelated to the other two) was fetched and run
on mainline. **Same result**, and the same crash site as sdlpop:

```
untangle: Task  RAM:batch/Untangle
          Error 0x80000003 - Illegal address access
          Module lddemon.resource  Function Lddemon_0_OpenLibrary + 0x25
```

Three distinct packages, three different uploaders, two distinct crash sites
(`FindTask` and `OpenLibrary`), all `x86_64-aros-v11`. Three is still not a
survey, but it is no longer plausibly a property of one upload.

Five more were downloaded and staged and **have not been run**: the Shell wedged
on a suspended crashed task and the remaining attempts went into a blocked
window. That is a limitation of how this VM is driven, not of the question, and
the five are sitting in the cache ready for a cleaner pass.

## What the crash signature says

The workspace notes already described this exact failure, written down weeks
earlier:

> `proto/exec.h` inlines library calls, and how they pass `SysBase` differs:
> AROS One uses a register call through `%r12`, mainline passes it as an
> ordinary third argument in `%rdx`. Mixing them produces a crash *inside*
> `AllocMem` or `FindTask`, with a PC in a Kickstart module, which reads
> exactly like a broken startup and is not one.

A crash inside `FindTask` with the PC in Kickstart is that description
word for word. `OpenLibrary` is the same class: both are library calls made
through the inlined stubs where the base register convention matters.

## What was ruled out

**Stripping.** The parent repo records a scar where a fully stripped AROS
executable loads without its `.text` relocations applied and dies in its very
first `OpenLibrary()`, which is exactly what `sdlpop` did. So it was checked:
both binaries still have `.symtab` and `.strtab`, and `nm` lists their symbols.
**Neither is stripped**, so that explanation is out.

Host-side disassembly did not isolate the calling convention any further;
register-usage counts are not a discriminator, and no clearer static marker was
found in an afternoon.

## A fresh mainline build is unaffected

The third possibility -- that mainline, its toolchain or those libraries are
simply broken -- is ruled out. `abitest.c` was compiled with the **current
mainline SDK** and made exactly the calls the two archive binaries died in:
`FindTask`, then `OpenLibrary` of all nine libraries zaphod uses, then it opens
a window.

```
FindTask      000000004a2cf2d0  RAM:abitest
OpenLibrary   intuition.library      ok
OpenLibrary   graphics.library       ok
OpenLibrary   gadtools.library       ok
OpenLibrary   asl.library            ok
OpenLibrary   diskfont.library       ok
OpenLibrary   iffparse.library       ok
OpenLibrary   locale.library         ok
OpenLibrary   workbench.library      ok
OpenLibrary   icon.library           ok
OpenWindow    ok
abitest: finished without crashing
```

A GUI program built for mainline today works on the same call path where the
archive binary dies. So the platform is not the problem; something about how
those binaries were built is. That still does not say *which* difference kills
them.

## The source will not build against mainline either

ZapHod ships its own `src/`, so it was compiled with the mainline toolchain --
the second half of the comparison. It does not build, for two reasons that are
themselves evidence of a different SDK:

- It declares `struct Library *IntuitionBase` (and `GfxBase`, `LocaleBase`),
  which conflicts with mainline's `struct IntuitionBase *IntuitionBase` unless
  `__INTUITION_STDLIBBASE__` is defined. Mainline's headers offer both forms and
  also a `__aros_rellib_offset_` mechanism the program knows nothing about.
- With that resolved, two static initialisers fail with *"initializer element is
  not computable at load time"*: a pointer cast into an integer `Tag` field,
  which an older toolchain accepted and this one rejects.

Neither is the runtime crash, and neither should be presented as its cause. What
they establish is that this program was written and built against a materially
different SDK from the one we target.

## The test that would settle it, and why it has not been run

**Run the same binary on AROS One.** Same file, same hash, different AROS. If it
runs there and not on mainline, the ABI difference is proven and it is not a
broken package.

That test needs the `one` VM, which has been in use for another test since
11:15, with that work on screen. It was not touched. The test is owed,
not skipped.

## Why this matters more than a bug

The project's scope decision of 2026-09-03 was: **build against mainline, not a
distribution.** That decision was made for good reasons and this does not
overturn it, but it now has a consequence nobody had measured:

**If AROS Archives' `x86_64-aros-v11` binaries are built against a distribution
SDK, then arospkg on mainline can install the entire catalogue perfectly and
none of it will run.**

Two of two is not a survey. But two of two, both in the documented way, is
enough that the next thing worth doing is establishing how general this is,
before any more manifests are approved, and certainly before a page claims
anyone can install software with this.

Three possibilities, none of them yet evidenced over the other:

1. The archive is largely built for distributions, and a mainline-targeted
   package manager needs packages rebuilt for mainline to be useful at all.
2. ~~It is a property of these two uploads~~ -- weakened: three unrelated
   packages from three uploaders now fail the same way. Not disproven, but it
   would need most of the catalogue to be fine and these three to be unlucky.
3. Something else is wrong that looks like an ABI mismatch: the project has
   made that mistake before, with the OpenSSL socket patch, and the lesson
   recorded then was that a mechanism which explains the symptom is not
   thereby the mechanism.

## What this does not change

The installer works. Verification, unpacking, the registry, uninstall and
recovery were all exercised again in this run and behaved exactly as before.
What is in question is whether the software it installs is usable on the target
we chose, which is a question about the catalogue and the platform, not about
the code.

Screenshots: `../first-run/sdlpop-crash.png`, `../first-run/zaphod-crash.png`.
