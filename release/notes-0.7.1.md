arospkg 0.7.1 fixes **Update arospkg** in PkgManager for installations that still have an apkg from 0.6.0 or earlier beside them.

## Fixed

- To find the apkg that finishes an update, PkgManager 0.7.0 asked each candidate in a way that apkg 0.6.0 and earlier take as a command to update themselves. When the apkg in PkgManager's drawer, its `apkg.old` or the `C:apkg` it tried was such an older version, that apkg could replace itself from the network, and the update then stopped with "a file changed since it was checked; nothing was changed". PkgManager now asks only `apkg --version` until it knows the apkg is 0.7.0 or later.

Everything new in 0.7 is in the [0.7.0 release notes](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.7.0): apkg and PkgManager now update together, from the Shell or from PkgManager.

## Updating

From 0.7.0:

```text
apkg self-update
```

or **About** > **Check for a newer version** > **Update arospkg** in PkgManager. If PkgManager 0.7.0 reports "a file changed since it was checked", close it and run `apkg self-update` in the Shell from its drawer.

### From 0.4.x to 0.6.0

These versions can only replace apkg itself. Run the update twice:

```text
apkg self-update
apkg self-update
```

The first run installs the new apkg and leaves PkgManager as it was; the second, now with the new apkg, updates PkgManager. If you run the copy in `C:`, tell the second run which drawer it belongs to:

```text
apkg self-update --drawer "Work:arospkg-0.6.0.x86_64-aros-v11"
```

Close PkgManager before you start.

From 0.3.x: unpack the archive and copy `apkg` to `C:`. Keep your existing `SYS:Packages`.

## Downloads

| Build | Included | Status |
|---|---|---|
| [x86_64 ABIv11 / AROS One](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.1/arospkg-0.7.1.x86_64-aros-v11.zip) | apkg + PkgManager | Stable |
| [x86_64 ABIv1 / mainline](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.1/arospkg-0.7.1.x86_64-aros-v1.zip) | apkg + PkgManager | Experimental |
| [Raspberry Pi (native AROS, aarch64)](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.1/arospkg-0.7.1.aarch64-aros-raspi.zip) | apkg + PkgManager | Experimental |
| [i386 ABIv0](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.1/arospkg-0.7.1.i386-aros-v0.zip) | apkg | Experimental |

Pick the archive for your CPU and system: an ABIv11 build does not run on mainline, and the reverse. Self-update takes only the archive for the system it runs on; the Raspberry Pi build takes only the Raspberry Pi archive.

On the Raspberry Pi, as elsewhere, self-update needs a working network connection; without one, install the archive by hand. A checksum failure reported for one download on a Pi is still unexplained; `--keep-rejected` keeps such a download for a report.

[Updating arospkg](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md) · [User guide](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/using.md)

Checksums are in the attached `SHA256SUMS`.
