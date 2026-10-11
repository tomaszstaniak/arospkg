# Using arospkg

Install programs with apkg in the Shell or PkgManager on the desktop.
[Choose the archive for your CPU and ABI](../../README.md#downloads).
0.4.0 is available for x86_64 ABIv11, i386 ABIv0 and native Raspberry Pi
aarch64 ABIv1. The older experimental builds are also linked there.

## Install apkg

Unpack the archive for your target. Close PkgManager and wait for any
apkg operation to finish. Enter the unpacked drawer in the Shell, then run:

```text
Copy apkg C:
apkg --version
apkg update
```

Check that the version and ABI match the archive you downloaded.
Keep your existing `SYS:Packages`. Versions 0.3.x require this manual
upgrade to 0.4.

PkgManager is included only in the x86_64 ABIv11 archive.
Keep its unpacked folder on disk. To use the graphical interface, run
`Run PkgManager` from that folder. Click **Update index**, select a package
and click **Install**.

## Find and install a program

```text
apkg search calculator
apkg show zunecalc
apkg install zunecalc
```

`show` tells you what it needs before anything is downloaded. After an
install, apkg prints where the program is, the command that opens its
drawer, and any notes its author left:

```text
Installed zunecalc 1.0
  Location: SYS:Packages/zunecalc
  Open folder: apkg open zunecalc
```

`apkg open zunecalc` opens that drawer in Wanderer. In PkgManager, select
the package and press **Open folder**. With `--root`, the printed command
carries the same root, so it can be copied as it is.

## Keep things up to date

```text
apkg update
apkg upgrade zunecalc
apkg self-update --check
apkg self-update
```

`update` refreshes the catalogue. `upgrade` installs a newer version of
a package, or a newer packaging revision of the same version, and keeps
files you changed. Versions are compared as numbers (1.10 is newer than
1.9) only when they consist of numbers and dots; apkg refuses an older
version and any version it cannot order, and says why. `rollback` returns
to what the last upgrade replaced.

`self-update --check` checks for a newer stable apkg release. `self-update`
replaces the apkg executable you started and keeps the previous copy as
`apkg.old`. Update PkgManager separately from the release archive.

## Remove

```text
apkg remove zunecalc
```

Files you changed, and files apkg did not install, are kept; apkg lists them.

## When a download does not match the catalogue

apkg downloads into `SYS:Packages/cache/<id>.<ext>.part`, checks its size
and SHA-256 against the catalogue, and only then renames it into the cache.
A download that fails either check is deleted at once, so after
`the download does not match the index` there is no file left to look at.

To keep it for diagnosis, repeat the install with `--keep-rejected`:

```text
apkg --keep-rejected install folio
```

The rejected file is then moved to `SYS:Packages/cache/<id>.<ext>.rejected`
(a later rejection replaces it) and apkg prints its path, its size, the
expected SHA-256, and three received ones: the body as it was in memory
before being written, the file as verification read it, and a second read
of the same file. The file is still refused: nothing is installed from it,
and the next install downloads again rather than using it.
