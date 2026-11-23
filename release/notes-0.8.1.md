arospkg 0.8.1 keeps the button of each row inside the list.

## Fixed

- In PkgManager 0.8.0 the button at the end of a row (**Install**, **Run**, **Upgrade**) was cut off at the right edge when the list became narrower: after the window was made smaller, and after icons were added to the list. The list now measures its columns again each time it is filled.

## Updating

From 0.7.x or 0.8.0, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.

Everything else is as in [0.8.0](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.8.0), which brought the new PkgManager window and pictures of packages.
