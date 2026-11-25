arospkg 0.8.4 is 0.8.3 rebuilt for mainline AROS and Raspberry Pi nightly builds from 2026-11-24 on. ABIv11 systems get the same programs under the new version number.

## Changed

- The nightly build of 2026-11-24 changed a system structure that programs are built against. With arospkg 0.8.3 on that nightly, apkg on mainline x86_64 lost output sent to a file (`apkg list >file`), and apkg on the Raspberry Pi stopped with a hardware bus fault. The mainline x86_64 and Raspberry Pi builds of 0.8.4 are made against the SDK of that nightly.

## Mainline and Raspberry Pi: check your nightly first

The mainline x86_64 and Raspberry Pi builds of 0.8.4 need a nightly build of 2026-11-24 or later. On an earlier nightly they do not run: stay at [0.8.3](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.8.3) there and do not run `apkg self-update` until the system is updated. If you updated by mistake, the previous programs are beside the new ones as `apkg.old` and `PkgManager.old`; rename them back.

## Updating

From 0.7.x or 0.8.x, close PkgManager and run `apkg self-update`, or use **About > Check for a newer version > Update arospkg**. On a Raspberry Pi nightly of 2026-11-24 or later, 0.8.3 cannot update itself: unpack the 0.8.4 archive over the old drawer. From 0.4.x to 0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.
