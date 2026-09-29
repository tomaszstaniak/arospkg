---
title: "Spike: what does an overlay entry actually cost?"
status: complete
state: measured
created: 2026-09-07
updated: 2026-09-07
---

# Spike: four packages through the whole metadata pipeline

## Why

The index generator produced 168 candidates and the temptation was to call that
168 packages. It is not. Before designing `libpkg`'s API around the overlay, we
took four representative uploads all the way through (download, hash, look
inside, approve a manifest by hand) to find out what an entry really costs and
what the catalogue cannot tell us.

Sample chosen for shape, not size: a plain application, a library, an
application expected to have a dependency, and one with a configuration file.

| | package | size |
|---|---|---|
| plain application | `zaphod` (binary file editor) | 218 KB |
| library | `libmad-0.15.1b` | 121 KB |
| expected dependency | `sdllopan` (MahJong) | 490 KB |
| has configuration | `sdlpop` (Prince of Persia) | 1.4 MB |

All four downloaded with `Content-Length` matching the catalogue exactly.

## Result: two of four could be approved

| package | outcome |
|---|---|
| `zaphod` | **approved** |
| `sdlpop` | **approved** |
| `libmad` | **cannot be expressed** by the current design |
| `sdllopan` | **dependency unavailable** in the archive |

That is the honest overlay cost: **50% of a deliberately ordinary sample**, and
neither failure is about effort. Both are structural, and neither was visible
from the catalogue.

## Finding 1: dependency discovery can be assisted, not automated

This question had been open since the first design discussion and was never
spiked. It has an answer, and the answer is narrower than it first looked:
the paragraph below originally claimed more than the measurement supports and
is corrected here.

AROS executables name every library they open as a literal string, because
`OpenLibrary("foo.library", 0)` needs the name at runtime. So a plain `strings`
recovers them, with no debug info and without running AROS:

```
sdllopan/lopan   5 refs   crt, dos, intuition, SDL, stdlib
SDLPoP/Prince   17 refs   commodities, crt, cybergraphics, dos, exec, gadtools,
                          gl, graphics, icon, iffparse, intuition, keymap,
                          lowlevel, m, muimaster, stdlib, workbench
ZapHod/Zaphod   13 refs   asl, crt, diskfont, dos, exec, gadtools, graphics,
                          icon, iffparse, intuition, locale, stdlib, workbench
```

Most references are to the base system, so the useful step is subtraction.
`tools/deps.py` reads the mainline build's `Libs/` directory (79 libraries) as
the base set rather than hardcoding one, adds a short curated set that lives in
ROM or comes from the toolchain (`exec`, `utility`, `aros`, `crt`, `stdlib`,
`m`), and reports what is left:

```
sdllopan-10   1 not in the base system:  sdl.library
SDLPoP        no dependencies outside the base system
ZapHod        no dependencies outside the base system
libmad        0 binaries scanned
```

**35 references found across three binaries, one of them outside the reference
image.** That is the claim the measurement supports. It is *not* "exactly one
real dependency", which is what an earlier version of this document said.

The error runs in both directions:

- **Misses.** A library name can be built at runtime, read from a config file,
  or supplied by a plugin. Dependencies on external commands, data files,
  datatypes and devices are not visible to this method at all.
- **Inventions.** A literal may sit in a code path that never runs, or behind an
  optional feature. The string does not prove the program fails without it.
- **Invisible by construction.** `SDLPoP` references no SDL whatsoever, because
  SDL2 is linked statically. A clean report means "nothing left to open at
  runtime", not "nothing was used".

Subtraction inherits the same limit. The base set is one build's `Libs/`
directory, so "in the base set" means *available in that build*, not a
guarantee for every mainline installation, and nothing at all about a
distribution.

What it is worth is still a great deal: it turns "write dependencies for 168
packages from nothing" into "review a short list per package". `depends_checked
= true` therefore asserts that a person made a judgement with this scan in front
of them, not that a scan was run.

`SDLPoP` also opens `cybergraphics.library`. The parent repo's note about that
library concerns Picasso96 and Amiga Forever on real Amiga hardware and **does
not transfer to mainline AROS**, where it is present in `Libs/`; and the literal
alone would not prove the program refuses to run without it. Recorded as
something to check on a real machine, not as a defect.

## Finding 2: the archive has no dependency closure

`sdllopan` references `SDL.library`. That is SDL **1**, not the `libsdl2` its
name suggests, and **no provider for it was found in this catalogue.** The only
SDL in `FULLINDEX.readme` is `libsdl2`. That is a statement about what we
searched, not proof that SDL 1 exists nowhere; it may live in another archive,
or be expected from the system.

So the dependency cannot be satisfied from the source we are building on.
`sdllopan` stays unpublished until either a provider is identified or the
requirement is established as one the system is expected to meet. The program
presumably works on a distribution that already ships SDL 1 in `LIBS:`, which is
the "works on my AROS One" situation the project decided not to target.

This is a property of the catalogue, not of our tooling, and it has a
consequence the design must state: **`pkg` will regularly meet dependencies it
cannot resolve.** Refusing to install is right, but two different conditions
must produce two different messages: "no package in the index provides
`sdl.library`" and "this machine does not have `sdl.library`" are not the same
problem, and only the first is ours to fix. That distinction is now a
requirement on section 3.

## Finding 3: a `.info` lives beside its drawer, not inside it

`SDLPoP.info` and `sdllopan-10.info` sit at the **archive root, as siblings of
the package directory**, because that is how Amiga drawer icons work.

Section 2's `subdir` field assumes the package is one directory inside the
archive. Taking `subdir = "SDLPoP"` installs the program and **silently drops
its Workbench icon**: the package works from a Shell and is invisible where a
user would look for it.

Not all packages do this: `ZapHod` has no root-level `.info` and keeps its icons
inside. So the layout cannot be assumed either way and `mkindex.py` cannot infer
it. The sample manifest uses `subdir = ""` to keep the pair together, at the
cost of a nested directory, and that workaround does **not** give the package's
own directory an icon in Workbench, it merely stops one being lost. Section 2
now has an explicit rule; see its drawer-icon amendment.

## Finding 4: SDK packages cannot be expressed at all

`libmad` contains exactly two useful things:

```
libmad-0.15.1b.x86_64-aros/include/mad.h
libmad-0.15.1b.x86_64-aros/lib/libmad.a
```

A header and a static library. It has no runtime component, installs nothing to
`LIBS:`, and is useful only at build time, where it belongs under the SDK's
`Developer/include` and `Developer/lib`.

Section 2 restricts external writes to **`LIBS:` and `Fonts:`**, deliberately.
So this package cannot be described by our manifest format: not because the
rule is wrong, but because "development package" is a category the design never
considered. `development/library` holds 12 of the 168 candidates, so it is not a
curiosity.

**Decided 2026-09-07: installing headers and static libraries into the SDK is
out of scope for the MVP.** The approved destination list is not widened.

The exclusion is about *destination*, not about development software: `zaphod`
is a developer tool from `development/edit` and is approved, because it installs
as an ordinary application. Such candidates keep an explicit exclusion reason in
their manifest so they are visibly deferred rather than quietly missing.

## Finding 5: ids from filenames need review, always

The importer suggested `zaphod-v1.3` from `zaphod-v1.3-.x86_64-aros-v11.zip`.
An id has to be stable across releases, so it is `zaphod` with the version in
its own field. The catalogue also carries `python-2.5.2` and `python-2.7.18` as
two unrelated programs, which they are not.

No amount of parsing fixes this: the filename genuinely does not distinguish
"version 2.5.2 of python" from "a program called python-2.5.2". Approval is
where it gets decided, which is why `status = "skeleton"` exists.

## What this changed

- `mkindex.py` was split. It had been generating the index straight from the
  catalogue with `depends: []` hardcoded, so a hand-written dependency could
  never reach the index: an importer wearing the name of a generator.
  `import_catalogue.py` produces candidates and skeletons; `mkindex.py`
  publishes approved manifests only.
- `depends_checked` distinguishes "investigated, none" from "nobody looked".
- The download cache keys on `<category>/<filename>`, writes to `.part`, and
  publishes only after the size matches; a mismatch now blocks the entry
  instead of warning.
- `tools/deps.py` exists.

## The index checked from inside AROS (2026-09-08)

The first thing in this project that connects the generated index to the
machine it is for. `dl` (the network spike's downloader) was run on **mainline
AROS x86_64 in QEMU** against the URL in `index/index.json`:

```
RAM:dl https://archives.arosworld.org/share/development/edit/zaphod-v1.3-.x86_64-aros-v11.zip
  GET https://archives.arosworld.org/share/development/edit/zaphod-v1.3-.x86_64-aros-v11.zip
  302 -> https://arosarchives.os4depot.net/download.php?file=development/edit/zaphod-v1.3-.x86_64-aros-v11.zip
  GET https://arosarchives.os4depot.net/download.php?file=development/edit/zaphod-v1.3-.x86_64-aros-v11.zip
  218782 bytes
  sha256 603e292457c3439f32709380bdd1d9c72b06536d013eebc01541a0bf0fa75448
```

`index/index.json` records `size 218782` and the same digest. **Exact match**,
so a native AROS binary fetched a real package from the real archive over TLS
and the bytes are the ones our index promises.

Screenshot: `verify-on-aros.png`. Per this repo's evidence rule a screendump is
not proof, but a 64-character digest is self-verifying, since a misread cannot
match. That is why the check was built around a hash rather than a status line.

### Two things this run discovered

**The archive redirects to another host.** `archives.arosworld.org/share/...`
answers **302** and the bytes come from `arosarchives.os4depot.net`. Every hash
we recorded on the Mac went through the same redirect without our noticing,
because `curl -L` and `urllib` follow it silently. It matters for three reasons:
the download host is not the catalogue host, so availability and TLS trust
depend on a second party; `libpkg` must follow cross-host redirects, which is
not optional; and the project's hosting rule (never mirror, always point at
AROS Archives) is satisfied in name, while the actual bytes are served by
os4depot's infrastructure. Recorded, not resolved.

**The redirect path had never been executed before.** The network spike listed
"redirect following: code written, never exercised" as an open item since
2026-09-03. It has now run, in the one place that counts, and worked on the
first attempt.

The network stack was up without `startnet`, so the `AROSTCP/AutoRun=True`
an earlier test set on this image is in effect.

### What it still does not show

Nothing was installed and nothing was run. `zaphod` was downloaded and hashed,
not unpacked. There is no installer yet, so "the index is correct about these
bytes" is the whole claim.

## Where it stands

```
catalogue entries      1895
candidates (id,arch)   168
published packages       2
```

Two, not 168, and the distance between those numbers is this document.

Two published manifests are also not two working programs: nothing here has
been installed or run on mainline. And four samples show concrete metadata
gaps without supporting any estimate of what the whole catalogue would cost.
