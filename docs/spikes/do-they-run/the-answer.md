---
title: "The answer: ABIv1 versus ABIv11"
status: complete
state: answered by an AROS developer
created: 2026-09-11
updated: 2026-09-11
---

# The answer

Two days of measurement established that eight archive packages install
correctly on mainline and crash on startup, that the same binaries run on AROS
One, and that our build was not at fault. The mechanism stayed unidentified
through two failed attempts, and "the ABI" was deliberately refused as a name
for an unfinished diagnosis.

It was answered on the AROS forum by **deadwood**, an AROS developer, on
2026-09-11:

> The role of the `x86_64-aros-v11` suffix is exactly that: to distinguish
> between the ABIv1 and ABIv11 binaries and let them live together on the
> archives. The `-v11` marks binaries usable with current distributions like
> AROS One. Archives without such marker are intended for ABIv1.
>
> Please keep in mind: ABIv1 does not yet maintain backwards compatibility. If
> you are building third party software (one that is not re-build with every
> nightly) it may make sense to keep sources at hand and publish refreshed
> binaries whenever required (same goes for aarch64 of course).

So the suffix we treated as an architecture tag is an **ABI** tag, and it means
the opposite of what our tooling assumed.

| filename tag | ABI | runs on |
|---|---|---|
| `x86_64-aros-v11` | ABIv11 | AROS One and other current distributions |
| `x86_64-aros`, untagged | ABIv1 | **mainline** |
| `aarch64-aros` | ABIv1 | mainline |

Every crash we measured is explained: we build against mainline, which is
ABIv1, and we selected exactly the 168 packages marked for the other ABI.

## What this does to the catalogue

`tools/import_catalogue.py` now filters on ABI as well as architecture. The
result is not a small correction:

```
right arch, wrong ABI  171   (ABIv11: distributions, not mainline)
offered (ABIv1)          7
```

**Seven.** The entire AROS Archives catalogue, filtered for what mainline can
actually run on the architectures we target:

| | arch | what it is |
|---|---|---|
| `ghostscript-10.0.0` | x86_64 | the only end-user application |
| `libiffanim` | x86_64 | library |
| `libmikmod` | x86_64 | library |
| `lzo-2.03` | x86_64 | library |
| `commander_keen_4` | aarch64 | game |
| `super_trevor_land` | aarch64 | game |
| `pintp` | aarch64 | NTP client |

For **mainline x86_64** that is one application and three libraries. There are
another dozen `x86_64` entries in the catalogue that are cross-compilers
running on macOS, Cygwin or MinGW: host tools, not AROS programs.

## What it does to this project

The MVP's success criterion is "twenty packages install without error". On
mainline x86_64 the catalogue contains four things, of which one is a program.
**The criterion cannot be met on the target we chose**, and no amount of work
on the client changes that.

This is the chicken-and-egg the forum named plainly. From **terminills**, the
same thread:

> ABIv1 is lacking in software so it has less users however ABIv1 is also the
> mainline. So you do what you feel is best for your goals.

Three options, and this is a scope decision rather than an engineering one:

1. **Stay on mainline (ABIv1).** Honest, matches the 2026-09-03 decision, and
   the package manager has almost nothing to manage. It would be a tool waiting
   for a catalogue.
2. **Target ABIv11 / distributions.** 171 x86_64 packages and the users who
   actually run AROS today. It reverses a scope decision that was made
   deliberately and for good reasons.
3. **Be ABI-aware and serve both.** The client already carries `arch` and could
   carry `abi`; the index becomes two sets. More work, and it is the only option
   that does not throw something away.

Nothing here is decided. It is the user's call and it is a real one.

## A second warning worth more than it looks

> ABIv1 does not yet maintain backwards compatibility.

That undermines the overlay premise on the mainline side specifically. An index
that points at prebuilt ABIv1 binaries is pointing at files that may stop
working when mainline moves, without anything in the index changing. deadwood's
own advice is to keep sources and republish binaries as required.

A package manager for mainline may therefore need to be a **source** package
manager, or to track rebuild dates against ABI breaks, in a way that an overlay
of URLs and hashes cannot express. That is not a small addition to section 1.

## What we got right, and what we did not

The measurements were sound: every crash was real, the AROS One contrast was
real, and refusing to name "the ABI" without evidence was correct; the name
turned out right, but we did not have the evidence, and a correct guess is
still a guess.

What we got wrong was reading `-v11` as an architecture variant and never
asking. The information was public, on the archive's own site, and one forum
question answered in a day what two days of disassembly did not.
