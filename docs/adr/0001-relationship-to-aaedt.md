---
title: "Relationship to AAEDT"
status: accepted
created: 2026-09-03
---

# ADR 0001: Relationship to AAEDT

## Context

The prior art was reported to be "AADT in AROS contrib". Nothing by that name
exists: not in the AROS mainline tree (25029 paths), contrib (19767 paths),
`ports`, `external-sources` or `documentation`. The real tool is **AAEDT**,
spelled with an E, at `contrib/Networking/Apps/aaedt/`. The name traces to a
2026-09-03 forum post by deadwood: *"there is AADT tool in contrib package.
It's a front end for downloading from AROS Archives. It's not a package
manager in sense of linux etc."* (the misspelling is his).

Verified from primary sources:

- **AAEDT = "Archives AROS Exec Download Tool"**, stated in its README.
- It is an **AmiLua script** (`AAEDT.alua`), not a binary. It shells out to
  `c:wget` to fetch an index, parses it into a list gadget, shows an entry's
  description, can fetch and display its README with MultiView, and downloads
  the chosen archive to `ram:`.
- It does **not** unpack, install, record, resolve dependencies or update.
- Author Matthias Rustler, 2007; last functional change 2013 (deadwood, Lua
  5.2 fixes); everything since is build-system churn. Public Domain.
- It ships on our target: `AAEDT.alua` and `AAEDT.info` are both present in
  our own AROS One 64-bit v1.3 ISO and hard disk image.
- Its AROS URLs are **dead**. It hard-codes `http://archives.aros-exec.org/`,
  which now redirects to a domain-parking page (`www.webhuset.no/parkering`,
  HTTP 200 with parking HTML) because the archives moved to
  `archives.arosworld.org` in March 2025. The os4depot.net half still works.
  Out of the box the AROS side fetches a parking page and parses garbage.

## Decision

AAEDT is not the thing we are building and does not displace this project. It
is a downloader: the "just a downloader" outcome that section 1 of the MVP
document already names as the failure mode to avoid. We continue.

We take two things from it: confirmation that a plain index file is the right
interface to the archives, and the index URLs themselves (see ADR 0002 /
the section 1 amendment).

We do not fork it, adopt its code, or make our client compatible with it.
Fixing its dead URL is a four-line edit that upstream may want; that is a
courtesy patch, unrelated to this project, and not scheduled here.

## Consequences

The MVP's value proposition sharpens: on AROS today there is exactly one
software-management tool, it is an unmaintained 2007 Lua script, and its
download source has been broken since March 2025. Install, uninstall,
dependencies and a registry are all still unclaimed ground.
