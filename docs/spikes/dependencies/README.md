---
title: "Before implementing dependencies: is there anything to resolve?"
status: complete
state: measured
created: 2026-09-12
updated: 2026-09-12
---

# Do package dependencies exist in this catalogue?

Increment 1 was to be dependency resolution, starting (as instructed) by
checking whether `sdllopan`'s dependency has a provider, and picking another
pair if not. The check went wider than one pair, and the answer changes what
increment 1 should be.

## The AROS One library set corrects an earlier finding

`deps.py` had been run against **mainline's** `Libs/` as its reference image,
which is the wrong baseline for ABIv11 packages. Mounting the AROS One 1.3 ISO
read-only gives the right one: **84 libraries**, and `SDL.library` is among
them.

So the earlier conclusion, "no provider found for `SDL.library` in this
catalogue", was true and beside the point. The provider is **the system**, not
a package. On AROS One `sdllopan` has everything it needs.

## No package dependency was found by this method

Thirty-two ABIv11 packages were downloaded and every library reference `strings`
could find in every binary was checked against AROS One's 84 libraries.

That is the claim, and it is narrower than "the catalogue has no package
dependencies". A `strings` scan is an aid, not a completeness proof: it cannot
see a library name assembled at run time, it says nothing about data files or
tools a program shells out to, and it covers 32 of 173 candidates. What follows
is therefore what the method found, not what exists.

**One unresolved reference in the whole sample**, and it is not a package
dependency either: `libmpg123`'s four command-line tools reference
`bsdsocket.library`, which AROSTCP provides at runtime.

The library *packages* in the catalogue (`libsdl2`, `libmpg123`, `libmad`,
`filesysbox`) turn out to be **developer SDK packages**: static `.a` archives
and headers, no runtime `.library` at all. They are build-time dependencies for
someone cross-compiling, which is the category the MVP already excluded.

So this method found nothing for a package-to-package resolver to resolve. That
is enough to stop building one now and not enough to conclude the catalogue has
none: the sample is 32 of 173 and the instrument sees only literal strings.

## But system requirements are real, and one package proves it

The two systems do not carry the same libraries. **AROS One has 21 that mainline
does not:**

```
bgui  crt  filesysbox  gtlayout  guigfx  lcms2  m  mesa3dgl  openal  pixman
ptplay  regina  render  SDL  stdlib  thread  ttengine  Warp3D  xadmaster
xprzmodem
```

Which gives a real, testable case with a real package:

| | |
|---|---|
| `sdllopan` needs | `SDL.library` |
| on **AROS One** | present → installable, should run |
| on **mainline** | **absent** → must be refused, and the reason is the machine's, not the index's |

*Corrected after implementation:* on mainline the **ABI gate refuses it first**,
because it is the earlier check: the package is ABIv11. So mainline cannot be
used to test the requirement check at all, and the negative test had to be a
missing library on an ABIv11 machine instead. Both refusals are real; only the
earlier one is ever seen.

That is exactly the distinction section 3 requires and the client does not yet
implement: *"no package in the index provides X"* and *"this machine does not
have X"* are different conditions and must produce different messages. The first
is ours to fix; the second is not.

## So increment 1 became system requirements

**Accepted 2026-09-12 and implemented; see
`docs/reports/2026-09-12-requires-system-onetest.md`.**

Not a change of direction, but the other half of the same feature, and the half
that has a test case. Concretely:

1. `requires_system` in the manifest, as section 3 already specifies: type, id,
   optional minimum version.
2. The client checks each requirement **before downloading**, the same place the
   ABI gate sits.
3. Three outcomes, as designed: satisfied proceeds silently; unsatisfied refuses
   and says the requirement is the machine's; **undetermined** proceeds, reports
   prominently, and is recorded in the registry, because on this platform most
   requirements are not decidable, and a manager that refuses whenever it cannot
   be sure is one nobody runs.
4. The measurable result: `sdllopan` installs on AROS One and runs. The
   negative half cannot be mainline, for the reason noted above; it is a
   library nothing provides, asked for on a machine of the right ABI.

Package-to-package resolution (the plan, the ordering, cycles, refcounted
removal) stays designed and unbuilt. It should be built when something needs
it, and the first thing that will is our own software, where we control the
manifests. Building a resolver now would mean testing it against a dependency
graph we invented.
