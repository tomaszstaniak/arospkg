---
title: "Run report: requires_system on ABIv11"
run_id_prefix: req-
machine: onetest (private copy of AROS One 1.3, QEMU/TCG)
date: 2026-09-12
binary_sha256: 37b2774a16afcd950e82ef29dbcb0d2fb994125406b1db0d8071ca69d14a7598
source_id: 628e220d2861a26f
abi: v11
---

# requires_system, measured on AROS One

## What ran

`tests/requires-system.script`, executed from `PAYLOAD:REQTEST` on a private
copy of the AROS One 1.3 disk (`aros-onetest-hd.qcow2`). The binary was
`pkg-one` built by `src/build-one.sh`.

The binary hash below is the same value the host computed for the file it put
on the payload ISO **and** the value `pkg` computed for itself at run time by
hashing `GetProgramName()`. Two independent measurements of the same 64
characters, which is what makes the identification worth having:

```
host   shasum -a 256 payload/PKGONE
       37b2774a16afcd950e82ef29dbcb0d2fb994125406b1db0d8071ca69d14a7598
guest  "binary_sha256" in every report below
       37b2774a16afcd950e82ef29dbcb0d2fb994125406b1db0d8071ca69d14a7598
```

## Results

| run id | command | expect | status | verdict |
|---|---|---|---|---|
| `req-1-sdllopan` | `install sdllopan` | 0 | **0** ok | PASS |
| `req-2-missing` | `install req-test-missing` | 13 | **13** system requirement not met | PASS |
| `req-3-satisfied` | `install req-test-satisfied` | 0 | **0** ok | PASS |
| `req-4-undetermined` | `install req-test-undetermined` | 0 | **0** ok | PASS |
| `req-6-remove` | `remove sdllopan` | 0 | **0** ok | PASS |

Every run carried `--expect`, so a run that merely produced a report did not
pass; it had to end in the status the test was looking for.

## The three outcomes, as the machine reported them

**Satisfied**: `sdllopan`'s three requirements, from its registry entry:

```
{ "type":"library", "id":"SDL.library",    "min_version":0, "state":"satisfied",
  "found_version":1, "checked":"already open, version 1" },
{ "type":"library", "id":"crt.library",    "min_version":0, "state":"satisfied",
  "found_version":5, "checked":"already open, version 5" },
{ "type":"library", "id":"stdlib.library", "min_version":0, "state":"satisfied",
  "found_version":3, "checked":"already open, version 3" }
```

All three were already resident, so the library-list lookup answered and
`OpenLibrary()` was never reached. The versions found are recorded, not just
the verdict.

**Missing**: refused, before any download:

```
pkg: this system does not provide what the package needs
     req-test-missing
     arospkg-test-absent.library (not on this machine)
```

**Undetermined**: installed, said so, and wrote the doubt down:

```
WARNING: 1 of this package's 1 system requirement(s) could not be
         checked. It will be installed, and this is recorded with it.
         datatype png: requirement type "datatype" is not one this client can check
installed req-test-undetermined
```

and in the registry entry afterwards:

```
{ "type":"datatype", "id":"png", "min_version":0, "state":"undetermined",
  "found_version":-1, "checked":"requirement type \"datatype\" is not one this client can check" }
```

## It runs

`lopan` started from its installed directory and drew a full Mahjong board:
`docs/spikes/requires-system/lopan-runs.png`. This is a real application from
AROS Archives, installed by `pkg`, whose system requirements were checked
against the live machine, running.

**One finding worth carrying forward:** started from anywhere other than its own
directory it prints `Failed to load a tile set, code 1`. It looks for `data/`
relative to the current directory, not to the executable. Launched from an
icon, Workbench would set `PROGDIR:` and the drawer icon the `icon` field
describes would make that the normal way in. From a Shell, `CD` first. That is
a packaging concern, not a requirements one, and it belongs with the drawer
icon work.

## What this does not show

- **Nothing was run on mainline.** The negative case deliberately does not use
  mainline: an ABIv11 package is stopped there by the ABI gate, which is an
  earlier check, so a refusal on mainline would say nothing about this code.
  The controlled environment is instead the same ABIv11 machine asked for a
  library that was never anywhere.
- **No library was removed from any machine** to make the negative case.
- `sdllopan` was run once, to a drawn board and a working close gadget. That is
  not a claim the game is fully playable.
- The transcript below the reports was read off the screen. The **reports**
  are the evidence: the status and the binary hash in each, and the binary
  hash is self-verifying against the host's own measurement. Screenshots
  illustrate.

## Incidental, recorded because it was measured

All three published packages installed on the same root, in the order
`sdllopan`, `zaphod`, `sdlpop`. `pkg list` then prints:

```
sdlpop
zaphod
sdllopan
```

Reverse of the install order, and identical to what `List
SYS:Packages/db/installed` prints. `pkg list` walks the directory and does not
sort; it is reporting filesystem order, which here happens to be
newest-first. **Not a guarantee**; it is one observation on one filesystem, and
the missing sort is a known debt.

`sdlpop`'s registry entry is now **139,646 bytes**, 26 more than the 139,620
measured before this change. That is exactly the width of the empty
`"requires_system": [ ]` the emitter now writes, an accidental but welcome
confirmation that the field is emitted for a package that has none, rather than
being skipped.

## Host tests

`tests/run-host-tests.sh`: 6 SHA-256 vectors and 20 checks over all three
requirement outcomes, the two parse failures, and the registry fragment. All
pass. A host pass says nothing about AROS, which is why the above exists.
