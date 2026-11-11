arospkg 0.7.6 keeps a program's own drawers across an upgrade.

An upgrade or rollback replaces the package drawer with a new tree. That tree was built from files only, so a drawer the installed files did not fill, for example an empty one a program had made for itself, was gone afterwards. Dockbar keeps such a drawer for its widget requests, and after `apkg upgrade dockbar` while Dockbar was running every widget action said "Could not launch provider" until Dockbar was started again. The new tree now has those drawers too.

When a program is still running from the drawer being upgraded, apkg says so before the swap: the program keeps running from the old files, and what it saves until it is started again is not kept. Quit the program before you upgrade it.

An archive member whose path would not fit is refused as "member path too long" instead of being written under a cut-off name, and a `--root` longer than 255 characters is refused.

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
