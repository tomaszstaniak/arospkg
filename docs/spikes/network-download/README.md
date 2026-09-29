---
title: "Spike: HTTPS download on AROS"
status: answered 2026-09-03: verified HTTPS works; see Results
created: 2026-09-03
updated: 2026-09-03
blocks: ../../backlog/package-manager-mvp.md (section 3, libpkg API)
---

# Spike: HTTPS download on AROS

## The question

Can an AROS x86_64 binary fetch `index.json` from raw.githubusercontent.com
and one archive from archives.arosworld.org over TLS, following redirects,
validating the certificate, and confirming a SHA-256, and at what cost?

## Findings: most of this was already answered on 2026-09-03

This spike was written without checking our own logs. It should have been.
Work done earlier the same day in the parent workspace already settles the
transport question, so the two "candidate routes" it proposed are both wrong.

**HTTPS from native AROS works and has been run.** Verified that morning:
the AROSTCP stack (`System/Network/AROSTCP/`, which creates
`bsdsocket.library`) gives working DNS, TCP and HTTP; contrib's **OpenSSL
4.0.1** builds for AROS with `getentropy()` verified; and a native AROS
program built on it talks to a public HTTPS API over **TLS 1.3** and gets
answers back.

That kills both proposed routes:

- **Route A, the SDK's static OpenSSL 1.1.0h (2018), is obsolete.** It comes
  from the *forked* toolchain used with AROS One. Mainline contrib carries
  OpenSSL **4.0.1** (`contrib/development/libs/openssl/`, with
  `openssl-4.0.1-aros.diff`). There is no reason to link an eight-year-old
  TLS stack.
- **Route B, AmiSSL, is moot.** AmiSSL is the AmigaOS/MorphOS answer to not
  having OpenSSL. AROS mainline has OpenSSL. The extended step 0 survey of
  `AmiSSL:` assigns, `AmiSSL:Libs`, `AmiSSL:Certs` and library ABI is
  therefore not worth running: whatever it found would not change the choice.

**Retracted 2026-09-04.** This document said we had found and fixed an
AROS-specific OpenSSL bug: that `include/internal/sockets.h` collided with the
bsdsocket descriptor namespace, and that routing socket I/O through
`SocketBase` was the fix ready for upstream. That patch is now **superseded and
must not be submitted.** It worked by masking two other bugs, both merged
upstream on 2026-09-04: `fd.library` was in the tree but never built
(PR #1122), and `posixc/ioctl.c` sign-extended the ioctl request so `FIONBIO`
(`0x8008667e`) reached the socket hook as `0xffffffff8008667e`, matched no case
and returned `EINVAL` (PR #1124, 64-bit only, so aarch64 is affected exactly
as x86_64 is). With both applied, **unmodified** contrib OpenSSL 4.0.1
completes a TLS 1.3 handshake over plain `read()`/`write()`/`ioctl()`.

Consequence for `libpkg`: link stock OpenSSL, use POSIX sockets, carry no
vendored patch. The `SocketBase` plumbing in `dl.c` below is therefore
scaffolding, not a requirement.

Consequence for this spike's results: everything below was measured **before**
those two fixes, against the patched OpenSSL. The byte-exact fetches and the
negative TLS test still stand as evidence that our verification logic is
correct, but they have **not** been re-run against a stock build. Re-running
`dl` unmodified on a current mainline is the cheapest way to close this, and it
has not been done.

The full account, including why the first explanation was believed for a week
and what measurement finally settled it, is in `~/Work/AROS/patches/README.md`.

**Entropy, which was the real risk, is resolved on mainline.** AROS One 1.3
has weak RNG (libc `rand()`); mainline has the
`entropy.resource` / RDRAND / `getentropy()` stack, verified 2026-09-03
(250/512 bits on the first probe).

## What is still genuinely open

The transport works; what is missing is everything that makes it *safe*, plus
the download mechanics we actually need.

1. **Hostname verification is absent.** `hardcode.c` sets SNI
   (`SSL_set_tlsext_host_name`) and, when the CA bundle loads,
   `SSL_VERIFY_PEER`, but it never calls `SSL_set1_host()` /
   `X509_check_host()`. Chain validation without hostname checking accepts any
   valid certificate for any name. The negative test stands: a connection to a
   host whose certificate does not match the requested name **must fail**.
2. **The CA bundle is trusted blindly if absent.** `hardcode.c` hardcodes
   `ENV:SYS/Certificates/ca-bundle.crt` and, when it cannot be loaded, prints
   `warning: CA bundle not loaded, certificate unchecked` **and continues**.
   That is defensible in a demo and unacceptable in a package manager, where
   an unverified download is the whole attack. `libpkg` must refuse to proceed.
   A `ca-bundle.crt` does exist somewhere on the mainline image (found by
   string search, alongside curl's `mk-ca-bundle.pl`); its real path, owner
   package and freshness are **not** established.
3. **Redirect following: done 2026-09-08.** Exercised for the first time on
   mainline, and by accident: `archives.arosworld.org/share/...` answers **302**
   and redirects to `arosarchives.os4depot.net`, a different host. `dl` followed
   it and the download hashed correctly. See
   `../overlay-sample/README.md`.
4. **SHA-256 of a downloaded file**, compared against a host-computed value.
5. **Cost:** binary size with OpenSSL 4.0.1 linked, and whether stripping is
   safe (the parent repo's `x86_64-aros-strip` scar).

## Steps

0. ~~AmiSSL survey~~, dropped, see Findings.
1. ~~Find the CA bundle's real path, its provider and its age.~~ **Done
   2026-09-03, see "Step 1 result" below.**
2. ~~Minimal downloader over verified TLS with hard failure.~~ **Done:** `dl.c`.
3. ~~Download from both hosts.~~ **Done.**
4. ~~SHA-256 against the host.~~ **Done, byte-exact.**
5. ~~Negative test.~~ **Done, fails as required.**
6. Redirect following: written, **not yet exercised**.

## The platform question: resolved 2026-09-03

Mainline. Our programs are not scoped to a distribution: AROS One 1.3 is a
convenient VM, not a target. Development tracks mainline AROS and
distributions pull from mainline on their own schedule, so where the two
differ (entropy, OpenSSL version, toolchain), mainline is what we build
against. AROS One's weak RNG and 2018 TLS stack are therefore not our problem
to solve.

`pkg` itself must build for **x86_64 and aarch64**.

## Step 1 result: the trust store (2026-09-03)

**Path and provider.** The bundle is part of **AROS mainline itself**, not a
port: `workbench/prefs/env-archive/SYS/Certificates/ca-bundle.crt`, installed
through `ENVARC:` and therefore readable at
`ENV:SYS/Certificates/ca-bundle.crt`, exactly the path `hardcode.c` hardcodes.
Nothing needs installing, and no distribution decides this for us.

**Age: 2018.** The file's own header reads *"Certificate data from Mozilla as
of: Sun Apr 22 22:00:11 2018 GMT"*. The last commit touching it is
2018-06-09, *"store the certificates under ENVARC:SYS/Certificates on AROS"*.
It holds **133 roots** and has not been refreshed in over eight years.

**It still works, today.** Verified by validating each host we care about
against this exact file:

| host | result against the 2018 bundle |
|---|---|
| `raw.githubusercontent.com` | verifies (301 to the repo path) |
| `archives.arosworld.org` | verifies (200) |

The roots these hosts chain to predate 2018 and are in the file.

**Caveat on this test.** It was run with the *host* Mac's TLS client against
the bundle's contents. It proves the bundle is sufficient, not that AROS's
OpenSSL 4.0.1 reaches the same verdict. Only step 2 shows that.

**The real risk is rot, not breakage.** Roots rotate. The day GitHub or the
archives chain to a root added after April 2018, verification fails, and
because `libpkg` must refuse an unverified download, the correct behaviour
looks to the user like the package manager breaking. So `libpkg` needs a
stated policy, not silence:

- use `ENV:SYS/Certificates/ca-bundle.crt` by default;
- allow an explicit override path;
- report the bundle's date in `pkg` diagnostics, so "your trust store is from
  2018" is one command away rather than a mystery.

Whether we ship our own refreshed bundle is a **decision, not a detail**:
distributing trust material is a different kind of responsibility from
distributing an index, and it is not taken here.

**Upstream opportunity.** Mainline pairs OpenSSL 4.0.1 in contrib with a 2018
trust store in the core. Refreshing `ca-bundle.crt` is a small, obviously
useful patch, and we already have one OpenSSL patch queued for submission.

## OpenSSL version status (checked 2026-09-03)

`contrib/development/libs/openssl/mmakefile.src` builds **OpenSSL 4.0.1**
(`OPENSSL_RELEASE=4.0`, released 2026-06-09). Upstream's current release is
**4.0.2**, published 2026-08-25, so AROS contrib is one security patch
behind by nine days, which is unusually current for this ecosystem.

4.0.2 is a security release, most severe CVE rated Moderate. Nearly all of it
is server-side (QUIC server, CMP server, RPK server, DTLS epoch buffering)
and irrelevant to an HTTPS client. Two items do touch client code:
CVE-2026-54876, a client-side memory leak in OCSP response checking, and an
AEAD forgery with empty ciphertext via `EVP_Cipher()`. Hygiene, not urgency.

Bumping it is mechanical but not one line: `OPENSSL_VERSION` derives
`ARCHBASE`, and `patches_specs=$(ARCHBASE)-aros.diff`, so the patch file must
be renamed `openssl-4.0.2-aros.diff` and re-confirmed to apply. The diff is
11 KB and mostly `Configurations` entries.

**Good news for our aarch64 target:** that diff defines `aros-aarch64` and
`aros-aarch64-buildsys` explicitly, alongside arm, i386, m68k, ppc, riscv,
riscv64 and x86_64. The mmakefile's `findstring arm` test does not match
`aarch64` (no substring), so an aarch64 build correctly selects
`aros-aarch64-buildsys`, which exists. OpenSSL is ready for our second
architecture.

Build options in use: `no-threads no-ssl3 no-asm no-shared no-tests`
(plus `no-sse2` off x86_64). `no-asm` means no assembly fast paths.

## Results (2026-09-03): verified in QEMU on mainline AROS

`dl.c` (287 lines) plus `build.sh`, built with the mainline toolchain against
`/Volumes/arosmain/build/bin/pc-x86_64/AROS/Developer`. Run on the mainline
install after `Execute SYS:System/Network/AROSTCP/S/startnet`.

**Fetch from the archives.**

```
GET https://archives.arosworld.org/share/RECENT.readme
  1742 bytes
  sha256 040dec2bfb0d48742e472dba477cfa5919d16870ba20ac762868f379a6a5a1b5
```

**Fetch from raw.githubusercontent.com**, the index's future home:

```
GET https://raw.githubusercontent.com/aros-development-team/contrib/master/Networking/Apps/aaedt/README
  1125 bytes
  sha256 f306ec9ab5c137a00e733bb4a4372d1aea371c2b1b3738c9f94e9dd3658f54e8
```

Both hashes and both byte counts match the same URLs fetched on the Mac
exactly. The bytes that arrive on AROS are the bytes the server sent.

**The negative test fails, as it must.** Asking for a certificate name the
server cannot present:

```
RAM:dl --verify-name example.com https://archives.arosworld.org/share/RECENT.readme
  dl: TLS handshake with archives.arosworld.org failed
  dl: certificate rejected: hostname mismatch
  error:0A000086:SSL routines:tls_post_process_server_certificate:
    certificate verify failed:ssl/statem/statem_clnt.c:2497
```

So hostname verification is genuinely on, not merely requested. This is the
gap `aros-hardcode` has and `libpkg` must not.

### What this settles

- Verified HTTPS from a native AROS binary works, on both hosts we need.
- The 2018 CA bundle at `ENV:SYS/Certificates/ca-bundle.crt` (215544 bytes on
  disk, matching mainline git exactly) is sufficient **today** for both.
- `SSL_set1_host()` is deprecated in OpenSSL 4.0; the current name is
  **`SSL_set1_dnsname()`**. Ours uses the new one.
- Cost: **7,905,448 bytes** statically linked. That is the price of carrying
  OpenSSL, and it is the single biggest design input for `libpkg`: one
  library shared by two fronts, not two binaries each carrying this.

### Still not verified

- **Redirect following.** The code handles 3xx and re-resolves, but no test
  exercised it: our two hosts do not redirect within https, and `dl` refuses
  plain http by design so the archives' 301 cannot be used.
- Strip safety on this binary (the parent repo's `x86_64-aros-strip` scar).
- Timing on larger downloads; both test files are under 2 KB.

## Explicitly out of scope

Resumed downloads, parallel downloads, progress reporting, proxies, and any
`libpkg` API design.
