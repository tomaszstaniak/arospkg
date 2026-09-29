---
title: "Storefront: browsing, not just installing"
status: proposal
state: direction
created: 2026-09-10
updated: 2026-09-10
related:
  - package-manager-mvp.md
  - package-standard-proposal.md
  - ../spikes/overlay-sample/README.md
---

# Storefront

## Document contract

A direction for `PkgManager`, section 5 of
[`package-manager-mvp.md`](package-manager-mvp.md). It does not change the MVP,
does not block `libpkg`, and adds nothing to the CLI. It exists because the
question "how does someone *find* something to install" has a different answer
from "how does something install safely", and the second is nearly done while
the first has never been asked.

## The idea

Not a list of package names. Something closer to what a person expects from an
app store: categories to browse, search that works on descriptions, a row of
what is new, some sense of what is worth trying, and eventually a way for the
community to say so.

## What the data already supports, at no cost

Measured from `index/candidates.json`, 2026-09-10, over the 168 **candidates**.
Three distinctions matter and are easy to lose:

- **168 candidates is not 168 installable applications.** Two are approved and
  in the index; the rest are unreviewed skeletons, and the sample in
  [`../spikes/overlay-sample/README.md`](../spikes/overlay-sample/README.md)
  found half of an ordinary four could not be approved at all. A storefront
  that shows 168 tiles is advertising things that do not install.
- **Upload date is not a new version.** It is when a file was put on the
  server. A 2026 upload can be a re-upload of an old release, and `protrekkr`'s
  newest x86_64 upload is a prerelease that is *older* software than the 2.8.2
  beside it. A "Recently added" row is honest; a "New releases" row is not.
- **Icons in three sampled archives is not icons in every package.** All three
  applications we opened had one, which is encouraging and nothing more.
  Coverage has to be counted before anything is designed around it.

| | |
|---|---|
| top-level categories | **12** |
| sub-categories | **50** |
| packages with a usable one-line description | **168 of 168** |
| upload dates spanning | 2008-11-13 to 2026-09-04 |

```
game 59   utility 42   audio 12   development 12   demo 11   emulation 9
network 9   graphics 7   video 3   office 2   library 1   driver 1
```

So four of the five things asked for need no new metadata whatsoever:

- **Browse by category.** The catalogue is already a two-level tree:
  `game/platform` (17), `utility/archive` (10), `game/puzzle` (9). It is not a
  taxonomy we invented and have to maintain; it is the one uploaders already
  use.
- **Search.** Every entry has a summary. Substring search over id, summary and
  category covers the realistic case on a set this size, with no index server
  and no ranking to argue about.
- **New.** Upload date is in the catalogue, so a "recently added" row is
  truthful and free: the newest five today are `telegramamiga`, `ledblur`,
  `ominoarcade`, `pkg-7mezzoplus`, `libyaml`.
- **Installed state.** The registry already knows.

That is a genuine storefront's worth of navigation from data we parse today.

## What is missing, and what each thing actually costs

### Images: the real gap

A store without pictures is a list with a nicer font. And the catalogue has
**no images at all**: not one standalone `.png`, `.jpg`, `.iff` or `.gif` in
1895 entries. What exists is inside the archives, but that is the wrong side
of the download, since browsing happens before you fetch anything.

There is, however, something Amiga-shaped already there: **every package ships
its own `.info` icon**, which is exactly the picture a user associates with a
program on this platform. `SDLPoP.info`, `Zaphod.info`, `sdllopan-10.info`:
they were in all three sampled applications.

Two routes, and they are not exclusive:

1. **Extract the `.info` at index-generation time** and publish it beside the
   index. Coverage is unmeasured (three of three sampled applications had one,
   which is not a statistic), and measuring it means downloading every archive,
   so that cost comes first. Small, present for nearly every package, native to the platform, and
   requires nothing from authors. **But it means we host files**, a departure
   from "never mirror, always point at AROS Archives". That rule was written
   about binaries; a few kilobytes of icon is a different thing, but it is the
   user's call and not a detail to slip past. Note also that a `.info` must be
   rendered to be shown off-platform, which the GUI on AROS does not need but a
   web view would.
2. **The package standard adds an optional screenshot**, so new packages can
   supply a real one. Costs nothing for existing uploads and nothing to
   maintain: the author provides it or does not.

Route 1 gives coverage now; route 2 gives quality later. Neither should block
the other.

### Featured: editorial, and it must say so

"Featured" cannot be computed from anything we have. There are no downloads
counts, no ratings, no telemetry, and inventing a score from upload date and
category would be a ranking that looks objective and is not.

So if there is a featured row it is **hand-picked, by us, and labelled as
hand-picked**. That is honest and cheap: a list of ids in a file in this repo,
reviewed when someone feels like it. What it must never be is an algorithm
nobody can explain, or a silent bias in what a user sees first.

The maintenance cost is the familiar one: the client is written once, a
curated list is maintained forever. A stale featured row is worse than none.

### Community likes and badges: the expensive one

This is the only item that changes the shape of the project rather than adding
to it.

Everything so far is **static**: `mkindex.py` generates a file, the client
downloads it, and there is no server anywhere. Likes need writes, and writes
need a service, identity, rate limiting, moderation and a plan for abuse. On a
platform this small the first person to write a script decides every ranking.

Two things must be true if it is ever built, and they are worth writing down
now because they constrain the design:

- **A social signal must never become a trust signal.** A "like" must not make
  an unverified package installable, must not substitute for a SHA-256, and
  must not move a package past any check in section 2. The store may sort by
  popularity; the installer must not care that popularity exists.
- **It must degrade to nothing.** If the service is down, unreachable, or the
  user is offline (which on AROS is the common case), the storefront shows the
  catalogue without likes and everything still installs. No feature here may
  become a dependency of installing software.

My recommendation is to defer it, and to build the display side first. If
`PkgManager` browses well without likes, the argument for adding a backend can
be made on evidence rather than on the analogy with app stores that have
millions of users and a moderation team.

## Suggested order

1. Categories, search, new, installed state. No new data, no new decisions.
2. Icons from `.info`, if hosting a few kilobytes per package is acceptable.
3. Screenshots in the package standard, for new packages.
4. A hand-picked, clearly-labelled featured list.
5. Likes, only if 1–4 are being used and someone still wants them.

## Scale, and being honest about it

168 packages is not an app store. It is a shelf. Every design choice above
should be measured against that: a two-level category tree is generous for 168
items, search does not need an engine, and paging is not required. The
architecture should not be built for a catalogue we do not have, but the
*catalogue* will grow, and the aarch64 side is 2 packages today, so the client
must handle a shelf gracefully and a shop eventually, in that order.
