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

`update` refreshes the catalogue. `upgrade` installs a newer revision of
the same upstream version of a package.

`self-update --check` checks for a newer stable apkg release. `self-update`
replaces the apkg executable you started and keeps the previous copy as
`apkg.old`. Update PkgManager separately from the release archive.

## Remove

```text
apkg remove zunecalc
```

Files you changed, and files apkg did not install, are kept; apkg lists them.
