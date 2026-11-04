arospkg 0.7.4 handles a package drawer that another program still holds.

On AROS, the first program after a restart that opens gl.library keeps its drawer in use until the next restart. Removing such a package used to keep the empty drawer and call it "not empty", and installing the package again failed with "cannot move the package into place" until a restart. apkg now moves the held drawer into the root's `tmp` drawer, where a later apkg run deletes it once nothing holds it, and the reinstall goes ahead. When the drawer cannot be moved, apkg says that another program holds it before it downloads anything ([#1](https://github.com/tomaszstaniak/arospkg/issues/1), [#2](https://github.com/tomaszstaniak/arospkg/pull/2)).

The message about a lock left by a run from a previous boot now names `apkg unlock`, the command that clears it ([#3](https://github.com/tomaszstaniak/arospkg/issues/3)).

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
