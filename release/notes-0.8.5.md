arospkg 0.8.5 adds **Upgrade all** to PkgManager and brings back the **Update index** button.

## What's new

- **Upgrade all.** When updates can be installed, PkgManager shows **Upgrade all...** above the list (it is also in the Catalogue menu). It lists the packages it will upgrade, with the installed and the new version, and those it leaves out, with the reason, and waits for your confirmation. Then it upgrades them one after the other, as `apkg upgrade --all` does in the Shell. Files you changed are kept. A package that cannot be upgraded, for example because a file you changed would be overwritten, stays as it was and the others continue; the status line says how many were upgraded and what went wrong. **Cancel** during a download stops there.
- **Update index is a button again.** It stands above the list, beside **Upgrade all...**, and is still in the Catalogue menu (Right Amiga+U).
- **ARexx:** `UPGRADEALL` starts the same operation from a script; the list is confirmed in the window.

## Updating

From 0.7.x or 0.8.x, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.
