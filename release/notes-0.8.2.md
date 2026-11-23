arospkg 0.8.2 fixes PkgManager on the Raspberry Pi and the buttons of a package's page.

## Fixed

- On the Raspberry Pi (AArch64), PkgManager 0.8.0 and 0.8.1 stopped with "Hardware bus fault/address error" when it started with icons in its list, that is from the second start after the catalogue's icons had arrived. The pictures in the list were created in a way that is only correct on x86_64. If PkgManager cannot be started to update itself, run `apkg self-update` in a Shell.
- On a package's page, **Open folder** and **Remove** could stand partly outside the window, for example right after an installation from that page.

## Updating

From 0.7.x or 0.8.x, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.

The new PkgManager window and the pictures of packages came with [0.8.0](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.8.0).
