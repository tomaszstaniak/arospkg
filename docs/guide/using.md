# Using arospkg

The short version. The [README](../../README.md) has the details.

## Install apkg

Unpack the release archive for your CPU and ABI, copy `apkg` to `C:` and
fetch the catalogue:

```text
UnZip arospkg-0.4.0.x86_64-aros-v11.zip
CD arospkg-0.4.0.x86_64-aros-v11
Copy apkg C:
apkg update
```

0.3.2 and earlier cannot update themselves: install 0.4.0 this way once;
later releases come with `apkg self-update`. PkgManager is in the same archive; keep its
drawer somewhere permanent.

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
apkg update              refreshes the catalogue
apkg upgrade zunecalc    updates an installed package
apkg self-update         updates apkg itself (--check only reports)
```

`self-update` replaces only the apkg you started and keeps the previous one
beside it as `apkg.old`. PkgManager is updated from the release archive.

## Remove

```text
apkg remove zunecalc
```

Files you changed, and files apkg did not install, are kept; apkg lists them.
