arospkg 0.7.0 updates apkg and PkgManager together.

## What's new

- **One update for the whole installation.** `apkg self-update` in the Shell, or **About** > **Check for a newer version** > **Update arospkg** in PkgManager, now updates apkg and PkgManager to the same release. This works for a drawer you unpacked, for arospkg installed as a package (updated as `apkg upgrade` would, keeping icons you changed), and for a copy such as `C:apkg` once you say which installation it belongs to. apkg never searches your disks for copies.
- **Checked before anything changes.** The release archive is downloaded and checked against `SHA256SUMS` and its `BUILD.txt`, and every target is checked for space and write access, before PkgManager closes or a file is replaced. The previous files stay beside the new ones as `.old`; for the installed package, `apkg rollback arospkg` goes back.
- **Interrupted updates are settled.** If the machine stops during an update, the next `apkg self-update` (or `apkg self-update --recover`) finishes it or puts the previous files back, and PkgManager offers **Repair now** when it starts. Only one update runs at a time, and an update refuses while a package operation is running.
- **PkgManager restarts itself.** After **Proceed**, PkgManager closes, its installation's apkg replaces the files, and PkgManager starts again with the same package root and says what happened.
- **Raspberry Pi.** The Pi archive is back, with apkg and PkgManager.
- **For package authors:** apkg-pack accepts files up to 128 MiB, as the client has since 0.6.0, and keeps the execute bit of programs in the archive.

## Updating from 0.4.x to 0.6.0

These versions can only replace apkg itself. Run the update twice:

```text
apkg self-update
apkg self-update
```

The first run installs the new apkg and leaves PkgManager as it was; the second, now with the new apkg, updates PkgManager. If you run the copy in `C:`, tell the second run which drawer it belongs to:

```text
apkg self-update --drawer "Work:arospkg-0.6.0.x86_64-aros-v11"
```

Close PkgManager before you start. From 0.7.0 on, one run, or **Update arospkg** in PkgManager, is enough.

From 0.3.x: unpack the archive and copy `apkg` to `C:`. Keep your existing `SYS:Packages`.

## Downloads

| Build | Included | Status |
|---|---|---|
| [x86_64 ABIv11 / AROS One](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.0/arospkg-0.7.0.x86_64-aros-v11.zip) | apkg + PkgManager | Stable |
| [x86_64 ABIv1 / mainline](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.0/arospkg-0.7.0.x86_64-aros-v1.zip) | apkg + PkgManager | Experimental |
| [Raspberry Pi (native AROS, aarch64)](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.0/arospkg-0.7.0.aarch64-aros-raspi.zip) | apkg + PkgManager | Experimental |
| [i386 ABIv0](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.0/arospkg-0.7.0.i386-aros-v0.zip) | apkg | Experimental |

Pick the archive for your CPU and system: an ABIv11 build does not run on mainline, and the reverse. Self-update takes only the archive for the system it runs on; the Raspberry Pi build takes only the Raspberry Pi archive.

On the Raspberry Pi, as elsewhere, self-update needs a working network connection; without one, install the archive by hand. A checksum failure reported for one download on a Pi is still unexplained; `--keep-rejected` keeps such a download for a report.

[Updating arospkg](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md) · [User guide](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/using.md)

Checksums are in the attached `SHA256SUMS`.
