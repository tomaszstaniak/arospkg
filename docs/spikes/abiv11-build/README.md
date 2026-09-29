---
title: "pkg for AROS One: built, and it installs software that runs"
status: complete
state: measured
created: 2026-09-11
updated: 2026-09-11
---

# pkg on AROS One

Requirement (c) of three set 2026-09-11: `pkg` must be runnable on AROS One.
Done, and it turned out to carry (a) and most of the project's scope question
with it.

## The whole chain, on AROS One, from nothing

```
RAM:pkg --root RAM:P list
  nothing installed

RAM:pkg --root RAM:P update
  fetching https://raw.githubusercontent.com/tomaszstaniak/arospkg-index/main/index.json
  index updated: RAM:P/db/index.json

RAM:pkg --root RAM:P search zaphod
  zaphod               1.3        x86_64    A simple binary file editor

RAM:pkg --root RAM:P install zaphod
  fetching https://archives.arosworld.org/share/development/edit/zaphod-v1.3-.x86_64-aros-v11.zip
    redirect -> https://arosarchives.os4depot.net/download.php?file=...
  verified and cached RAM:P/cache/zaphod.zip
  installed zaphod

Run RAM:P/zaphod/Zaphod
  [the editor opens: ZAPHOD 1.3, ARTSOFT logo, search and replace, hex pane]
```

**And that is the first time in this project that something arospkg installed
actually ran.** Not on the platform we chose, but on the one the package was
built for.

## What had to change, and what did not

One build script, `src/build-one.sh`, and one portability fix. Nothing in the
design, the journal, the registry or recovery.

- **OpenSSL 1.1.0h against 4.0.1.** AROS One ships the 2018 release. The
  hostname check was calling `SSL_set1_dnsname()`, the OpenSSL 4.0 name; 1.1.0h
  has only `SSL_set1_host()`. `net.c` now picks by `OPENSSL_VERSION_NUMBER`.
  This is not cosmetic: without the call, `SSL_VERIFY_PEER` validates the chain
  and accepts any valid certificate for any name.
- **`-L$SDK/lib`.** The mainline toolchain finds its SDK by default; this one
  does not.
- The C library is one `libcrt.a` rather than mainline's split
  `libposixc`/`libstdcio`/`libstdc`, which the link line already tolerated.

**3.5 MB**, against 8.0 MB for the mainline build, and the reason is now
measured rather than assumed, because the sentence it replaces was misread
exactly as written (it reads as though *mainline* has the older OpenSSL).

Mainline has the **newer** one: **4.0.1, 9 June 2026**. AROS One's SDK carries
**1.1.0h, 27 March 2018**. Newer is bigger here, and that is the whole of the
difference.

Both builds relinked with a stubbed `net.c` and no `-lssl -lcrypto`:

| | with OpenSSL | without | OpenSSL's share |
|---|---|---|---|
| mainline, OpenSSL 4.0.1 | 8,084,104 | 172,616 | 7,911,488 (**97.9%**) |
| ABIv11, OpenSSL 1.1.0h | 3,509,480 | 123,888 | 3,385,592 (**96.5%**) |

The two builds differ by 4,574,624 bytes, of which **4,525,896 (98.9%) is
OpenSSL**. Everything else that differs between the targets (the split
`libposixc`/`libstdcio`/`libstdc` against one `libcrt.a`, and the two compiler
versions) accounts for 48,728 bytes, under 1%.

Consistent with the static archives themselves: `libcrypto.a` plus `libssl.a`
is 11.5 MB in the mainline SDK and 5.05 MB in AROS One's, a ratio of 2.27
against the linked binaries' 2.34.

## The thing that was expected to block it, and did not

deadwood warned that on his tree the file-descriptor split between the C
library and the network stack is live, and that OpenSSL there needs the socket
patch this project wrote and withdrew. That was the reason to expect TLS not to
work on AROS One.

**It worked unmodified**, first try, through `socket()` and `SSL_set_fd()`.
Either that SDK's OpenSSL already carries the patch, or the split does not bite
this code path. **Not investigated, and not claimed either way**: the previous
time this project explained a working system with a plausible mechanism, the
mechanism was wrong twice over.

## What this settles

The scope question was framed as a choice: mainline, or ABIv11, or an
ABI-aware client serving both. It is now measured that **both binaries exist
and both work**:

| | mainline build | AROS One build |
|---|---|---|
| runs | mainline, official nightly | AROS One |
| size | 8.0 MB | 3.5 MB |
| OpenSSL | 4.0.1 | 1.1.0h |
| `update`, `search`, `install`, `list`, `remove` | yes | yes |
| installs a package that then **runs** | no: mainline has 4 packages and the ABIv11 ones crash | **yes** |

So it is not a choice between two ABIs. It is **two builds of one source**, and
the expensive part (the design, the journal, recovery, the index format) is
shared. The cost is a second build script and a version check.

## The ABI gate, and recovery on a persistent root

Requirement (a), done, plus the verification the review asked for, on
`SYS:PkgAbi`, not `RAM:`.

### Where the default comes from

`pkg --version` on AROS One reports `abi v11`; the mainline build reports `v1`.
The default needs no detection, and the reason is worth stating because it is
stronger than a probe: **a binary for one ABI does not start on the other, so a
running `pkg` is proof of the system's ABI.** `ArosInquire()` is a cross-check,
not the source, and it could not be the source anyway, since the probe that
asks is itself a binary that has to run first.

### What the gate does

- **`install` refuses a package for the other ABI**, before downloading
  anything: *"wrong ABI for this system … It would install correctly and then
  fail to start."*
- **An unknown or missing `abi` is refused too.** The filename convention is
  the only source for this field and this project misread it for two days;
  guessing that an unmarked entry is probably fine is exactly how that happened.
- **`search` hides packages for the other ABI** and says how many it hid.
  `--all-abi` shows them marked `[other ABI]`.
- **`--abi <v1|v11>` overrides**, printing a warning that says the package will
  install and not start. Diagnostic, not a user feature; both flags are listed
  as such in the help.

### Measured on AROS One, persistent root

```
RAM:pkg --version                        abi v11
RAM:pkg --root SYS:PkgAbi update         index updated: SYS:PkgAbi/db/index.json
RAM:pkg --root SYS:PkgAbi search         sdlpop 1.23, zaphod 1.3   (both v11: both shown)
RAM:pkg --root SYS:PkgAbi install zaphod installed zaphod
RAM:pkg ... --interrupt-at 4 install sdlpop
                                         stopped on purpose, PKG_STOP_AFTER_REGISTRY
RAM:pkg --root SYS:PkgAbi list
  WARNING: 1 uncommitted transaction(s) exist. What follows is
           what is on disk, not a settled state...
  sdlpop
  zaphod
RAM:pkg --root SYS:PkgAbi doctor --retry
  recovery: 1 transaction(s) to resolve
    6aa41396-sdlpop: install without commit -- rolling back
        1074 file(s) removed, 0 left because changed
  no unresolved transactions
RAM:pkg --root SYS:PkgAbi list           zaphod
```

**Recovery works on ABIv11, on a persistent volume, at 1074 files**, and it
left `zaphod` alone, which is the part that matters: the rollback touched only
the transaction's own package.

## The negative transport tests, on ABIv11

A successful download shows the *right* certificate was accepted. It says
nothing about whether a wrong one would be refused, and that is the half that
matters. `--verify-name` makes `pkg` require a certificate for a name the
server does not have; the handshake must then fail, and failing is the pass.

Run on AROS One against OpenSSL **1.1.0h**, all five gated with `--expect`:

| | injected | result |
|---|---|---|
| **N1** | certificate must match `example.invalid` | refused |
| **N2** | must match `www.google.com` (a real certificate, for someone else) | refused |
| **N3** | a plain `http://` index URL | *"not an https URL"*, no connection attempted |
| **N4** | the CA bundle renamed away | *"cannot load the CA bundle … refusing to download"* |
| **N5** | **control**: unmodified | **succeeded** |

The exact rejection, which is the line that was missing:

```
  (test) requiring the certificate to match www.google.com
pkg: the index could not be fetched
     certificate for raw.githubusercontent.com rejected: Hostname mismatch
```

So on the older OpenSSL the hostname check is real, the CA bundle is mandatory,
and a downgrade to plain HTTP is refused before any socket is opened. That was
the open question about this build and it is closed.

**N5 is why the suite is trustworthy.** On the first attempt against mainline it
failed, and that failure invalidated the run rather than the build: the live CD
had no network, so N1–N4 had "passed" for the wrong reason. A negative suite
without a positive control cannot tell refusal from inability.

### The mainline half is incomplete, and that is a harness limit

The same suite against mainline could not be completed today. The official
nightly boots as a live CD, and even with `startnet` run by hand it cannot
resolve a name (`cannot resolve raw.githubusercontent.com`), so N5 fails and
the run is void by its own control.

The equivalent check *was* made on mainline earlier, on 2026-09-03, with the
spike downloader `dl` against contrib OpenSSL 4.0.1: same transport code,
same `--verify-name` idea, `certificate rejected: hostname mismatch`. So
mainline is not unverified; it is verified with a different binary, and
repeating it with `pkg` needs a mainline guest with DNS rather than a live CD.

## What this does not yet show

- Only `--interrupt-at 4` was exercised on ABIv11. The other five points and the
  fault injections have been run on mainline only.
- Entropy on AROS One is the older libc `rand()`; what that means for TLS there
  is an open question this project raised earlier and has not answered. The
  negative tests above say the certificate checks work; they say nothing about
  the quality of the randomness underneath the handshake.
- OpenSSL 1.1.0h is a 2018 release. Its maintenance status is a question about
  the platform that no test here addresses.

## Still owed

- **Requirement (a), an ABI option with the default taken from the running
  system.** The binary knows what it was built for; what it cannot yet do is
  refuse an index entry marked for the other ABI. The index already carries
  `abi` per package, so this is a check, not a redesign. `ArosInquire()` reports
  `ABI major -1` on mainline and has not been read on AROS One, because the
  probe that asks is itself a mainline binary and crashes there; an ABIv11
  probe would answer it.
- **Requirement (b), building from source.** Untouched.
- Nothing here was tested beyond `zaphod`, and the root was `RAM:`.
