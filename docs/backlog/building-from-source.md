---
title: "Building from source: not on the target, and what is left"
status: analysis
state: measured
created: 2026-09-12
updated: 2026-09-12
---

# Building from source

Requirement (b) of three, 2026-09-11: *"probably get an option to build from
source"*. The reasons for wanting it are good: mainline promises no backwards
compatibility, so published binaries rot, and deadwood's own advice is to keep
sources and republish. But the literal version is not implementable, and it is
better to say why than to design around a gap.

## No compiler was found for AROS x86_64

Checked in two places, and the claim is bounded by those two places. This is
not a proof that no such toolchain exists anywhere: someone may have one
unpublished, or in a distribution, or on a site nobody here searched.

**The built mainline system** has no compiler and no `make`. `Developer/bin`
holds thirteen tools and they are the bzip2 family; `C:` has `MakeDir` and
`MakeLink`, which are DOS commands, not build tools.

**The catalogue** has native compilers for AROS, and every one is **i386**:

| | |
|---|---|
| `gcc-3.3.1-v2-i386-aros.zip` | gcc/g++ 3.3.1 |
| `gcc-4.2.2.i386-aros.zip` | gcc/g++ 4.2.2 |
| `luxcc.i386-aros.lha` | a C compiler and toolchain |
| `make-3.8.1-1-i386.zip` | GNU make |
| `mmake.i386.tar.gz` | native mmake |
| `fpc-3.2.0a-i386-aros.lha` | FreePascal, ABIv0 |

For **x86_64 or aarch64 the search returns nothing usable**: three hits, all
false positives that matched on "make" in `isomaker` and `iconmake`.

Everything this project and this workspace produces is **cross-built from
macOS**. That is not a preference; on these architectures it is the only way.

## So `apkg build` has nothing to invoke

On the systems we can inspect, there is no compiler for `apkg` to call. That is
not the same as proving one could never exist, but it does mean a design in
which `apkg` compiles a package on the user's machine would be designing for
something we cannot point at today.

If a native 64-bit toolchain does exist somewhere, this whole section is
answerable by someone naming it, and that would be a better outcome than the
workaround below.

## What is left, and it is most of the value

The reason for wanting (b) was not compiling on the target. It was **not being
stranded when a binary stops working**. That is achievable without a compiler on
AROS, and it moves the work to where the toolchain actually is.

**Source provenance in the manifest.** A package records where its source is and
how it was built: repository and revision, or a source archive and its hash,
plus the SDK it was built against. That costs three fields and makes a rebuild a
task rather than an excavation. It also makes the ABI question answerable after
the fact: *what was this built with?*

**A rebuild workflow on the host.** When mainline moves and a binary stops
starting, the publisher rebuilds from the recorded source against the current
SDK and republishes. This is deadwood's advice with the roles made explicit: the
person with the toolchain does the rebuilding, and the person with the AROS
machine gets a fresh binary through `apkg update`.

**The SDK identity is provenance, not a compatibility test.** Recording which
SDK and which build date a binary came from says *what it was built against*.
It does not say whether it still runs, and nothing here should be read as an
expiry date: a binary built a year ago may work perfectly and one built last
week may not. What the field buys is the ability to answer "what was this made
with?" when something breaks, and to find every entry built before a known
change. Whether any of those entries actually stopped working is a question only
running them answers.

None of that is `apkg build`. All of it is what `apkg build` was wanted for.

## What this means for the two ABIs

It cuts differently on each, which is worth noticing.

**ABIv11 has a compatibility promise**, so a binary published against it should
keep working, and rebuilding is occasional. An overlay of URLs and hashes is
sound there.

**Mainline has no such promise**, so rebuilding is the normal case rather than
the exception, and an index of prebuilt binaries is a perishable good. On that
side the source provenance is not a nicety; it is what makes the index
maintainable at all.

Which is a second, independent reason why the two targets are not symmetric:
the first being that mainline's catalogue has four packages and ABIv11's has
171.

## Recommendation

Do not implement `apkg build`. Add source provenance to the package standard
proposal, where it belongs, and treat rebuilding as a publisher workflow rather
than a client feature. If a native x86_64 toolchain ever appears on AROS, this
can be revisited from a position where it would actually run.
