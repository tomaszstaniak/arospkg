arospkg 0.8.6 installs larger packages and no longer holds a download in memory.

## What's new

- **Larger packages.** An archive may now be 1 GiB (before: 64 MiB). One file in a ZIP archive may unpack to 1 GiB (before: 128 MiB) and all files of a package to 2 GiB (before: 256 MiB). An LHA archive and each of its files may be 256 MiB.
- **Downloads go straight to the disk.** Until now apkg and PkgManager kept a whole download in memory before writing it. It is now written as it arrives, so a download takes the same memory whatever its size.
- **Catalogue.** A package over a limit of 0.8.5 is offered only to 0.8.6 and later. Earlier versions do not install it and say that a newer apkg is needed.

## Updating

From 0.7.x or 0.8.x, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.
