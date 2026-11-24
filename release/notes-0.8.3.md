arospkg 0.8.3 brings back a filter that the new PkgManager window had lost.

## Changed

- PkgManager has an **Installed only** box beside the name of the list. It narrows whatever is chosen on the left to what is installed, so a category can again be combined with the installed state, for example to list the installed games. It also works together with the search.

## Updating

From 0.7.x or 0.8.x, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.

The new PkgManager window and the pictures of packages came with [0.8.0](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.8.0).
