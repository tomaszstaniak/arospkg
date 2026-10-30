arospkg 0.7.3 fixes the black background left behind links after hovering over them in PkgManager on systems with older Urltext.mcc.

The fix is built into PkgManager. You do not need to replace system libraries or wait for an AROS update. Link opening and copying work as before.

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
