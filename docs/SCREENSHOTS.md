---
title: "Screenshots: what each one shows"
status: index
updated: 2026-09-12
---

# Screenshots

All taken on real AROS under QEMU, 800×600 or 1024×768, terminal type. Grouped
by what they are evidence *of*, because most of them are investigation rather
than the manager working.

## The manager working

| file | what it shows |
|---|---|
| **`spikes/requires-system/lopan-runs.png`** | `sdllopan` installed by `pkg` on AROS One, its three system requirements checked against the live machine, and the Mahjong board drawn. The first package installed here whose requirements were **verified** rather than assumed |
| `spikes/requires-system/registry-verdicts.png` | the registry entries, searched for `state`: `satisfied` with the version found for SDL, crt and stdlib; `undetermined` for a `datatype` requirement the client cannot decide |
| `spikes/requires-system/undetermined-installs.png` | an undecidable requirement installing anyway, with the warning it prints |
| **`spikes/abiv11-build/runs-on-one.png`** | **the best single image**: `pkg` installed ZapHod on AROS One and the editor is open: ARTSOFT logo, search and replace fields, hex pane. The first time in this project that something arospkg installed actually ran |
| `spikes/abiv11-build/install-on-one.png` | the whole chain on AROS One from nothing: `update`, `search`, `install` with the cross-host redirect, `installed zaphod` |
| `spikes/do-they-run/bare-and-abitest.png` | a plain run, no flags, `pkg` from `C:`: five commands and every line they printed |
| `spikes/do-they-run/typed-at-prompt.png` | `pkg install zaphod` typed at the Shell rather than scripted, cold cache, all four lines |
| `spikes/first-run/sdlpop-cold-install.png` | a 1.4 MB package fetched, verified and installed (1074 files) |
| `spikes/first-run/tree-and-remove.png` | the installed tree listed, then `removed zaphod` |
| `spikes/first-run/acceptance.png` | the acceptance run: every step gated with `--expect`, all passing |
| `spikes/abiv11-build/abi-identity.png` | `pkg --version` on AROS One: `abi v11`, binary hash, toolchain, SDK |

## The manager refusing, which is the point of it

| file | what it shows |
|---|---|
| **`spikes/requires-system/refused-missing-library.png`** | *"this system does not provide what the package needs: arospkg-test-absent.library (not on this machine)"*. Refused before the download, on a machine of the right ABI, so the ABI gate is not what refused it |
| **`spikes/first-run/unregistered-file-survives.png`** | an install interrupted with a file present that nobody registered. The rollback leaves it alone, stops, and says *"1 transaction(s) could not be resolved. Nothing was cleaned up."* |
| `spikes/first-run/recovery.png` | an install interrupted after the registry write, then rolled back (50 files removed, 0 left because changed) |
| `spikes/abiv11-build/recovery-persistent-one.png` | the same on AROS One, on a persistent disk, at 1074 files, with the other package untouched |
| `spikes/do-they-run/corrupt-cache-refused.png` | a byte appended to a cached archive; `pkg` refuses and prints the hash it computed |
| `spikes/first-run/http-vs-content.png` | a 404 reported as a network error and a 200-that-is-not-an-index reported as bad content: two failures kept distinct |
| `spikes/first-run/fault-injection.png` | writes made to fail on purpose: the commit marker and the leftovers report |

## Evidence and provenance

| file | what it shows |
|---|---|
| `spikes/overlay-sample/verify-on-aros.png` | `dl` fetching from the archives and printing a SHA-256 that matches the index |
| `spikes/first-run/provenance.png` | the build identity: binary hash, source id, compiler, toolchain, SDK |
| `spikes/first-run/stabilisation-reports.png` | 25 machine-readable run reports written to disk |
| `spikes/first-run/interrupt-points.png` | every interruption point, each reporting `interrupted on purpose` |

## The ABI investigation: not the manager

These show crashes and are evidence about the platform, not about `pkg`:
`spikes/do-they-run/batch-w1..w5.png` (five archive packages crashing on
mainline), `sdlpop-crash.png`, `zaphod-crash.png`, `untangle-crash.png`,
`nightly-zaphod-crash.png`, `rust-crashes-on-mainline.png`,
`abi-probe-one-crash.png`, against `one-zaphod-runs.png`,
`one-prince-runs.png`, `rust-runs-on-one.png` and `nightly-abitest-runs.png`,
which show the same binaries running where they belong.

## Two caveats about using these

**They illustrate; they do not evidence.** This repository's rule is that an
automated check writes a report carrying a run id and the binary's own SHA-256,
because a screendump cannot say whose output it is: two test runs' commands
landed in each other's Shells on 2026-09-05 and that is why the rule exists. So
a screenshot shows *what a run looked like*; the report file is what says the
run happened and which binary produced it. An earlier version of this page
called them "evidence that the text is real", which overstates them against our
own rule.

**Legibility will not come from resolution.** The Shell's font is a fixed pixel
size, so a larger guest screen shows more characters, not bigger ones. What
decides readability is the font and the scaling at which the image is
presented, or better, not using an image at all: the transcripts sit in the
spike
documents beside each file, and text set in a monospace face reads better than a
photograph of text.

Console styling is deliberately **not** being changed to make better
screenshots. That belongs with our own terminal when it runs, not with `pkg`'s
output now.
