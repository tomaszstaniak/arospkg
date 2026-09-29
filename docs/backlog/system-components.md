---
title: "System components and distribution upgrades: a placeholder"
status: stub
state: not started
created: 2026-09-28
updated: 2026-09-28
---

# System components and distribution upgrades

A placeholder, not a design. It exists so that the work has a place and a
boundary. Nothing here is being built in 0.3.

## What it is about

Today every package lives in a drawer of its own, and nothing is written
outside it except its icon. Two things will need more:

- **A system component**: one package that owns files outside its drawer, such
  as a library in `LIBS:` or a command in `C:`. The first real case decides
  the rules, and it has to cover ownership of each file, a conflict with a
  file already there, upgrade, rollback and recovery. EmuV0's own archive is
  one example: it writes to `SYS:C`, `ENVARC:SYS/Packages` and `SYS:Tools`
  (`docs/spikes/emuv0/README.md`).
- **A distribution upgrade**: moving a whole AROS installation from one
  release to the next. This is not planned. It is named here only so that
  the component work does not grow into it unnoticed.

## What comes first, when this starts

1. Package-to-package dependencies, with one real application that needs
   one real component.
2. That one component installed and removed with the guarantees the drawer
   packages have now, and no weaker ones unless they are said out loud.
3. A controlled mode for a package's own installer. `bde64` is the
   candidate: its archive carries an Installer script for `C:`. The guarantees
   are weaker there and have to be stated, not assumed.

## Not decided

- Whether system files are recorded in the same registry entry as the drawer
  or in one of their own.
- How to tell a library that other programs share from one only this
  package uses.
- What a rollback means for a library that is already loaded into memory.
