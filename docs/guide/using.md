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
apkg outdated
apkg upgrade zunecalc
apkg upgrade --all
apkg self-update --check
apkg self-update
```

`update` refreshes the catalogue and ends by saying how many installed
packages have updates, with the command that lists them:

```text
Updated the package catalogue: 110 packages for this system
2 updates available for installed packages. See them: apkg outdated
```

`outdated` lists them: the installed version, the version the catalogue
offers, and the state. Nothing is downloaded to make the list; it compares
your installed packages with the catalogue `update` last fetched, and says
when that was:

```text
Updates for installed packages
Catalogue updated 2026-10-11 10:50
  Package  Installed      Available      State
  upa      1.0-aros1      1.1-aros1      update available
  upe      1.0-aros1      1.1-aros1      update blocked
    this system does not provide what the newer version needs: foo.library
```

An *available* update can be started now. A *blocked* one exists but cannot
be installed on this machine: the newer build is for another CPU or ABI, a
library it requires is missing, or the catalogue has no checksum for it.
Neither is a promise that the upgrade will succeed: whether a file you
changed would be overwritten is found by the upgrade's own preview, which
needs the new archive (`apkg --dry-run --fetch upgrade <id>` shows it).

`outdated --all` lists every installed package and its state, including
those that cannot be compared: installed newer than the catalogue (apkg
never goes back to an older version), versions it cannot order, a package
the catalogue no longer lists, one it lists only for another target, or
several entries for the same target. Each comes with the reason.

`outdated --json` prints the same answer as one JSON object and nothing
else, for scripts and other programs. Its fields: `catalogue` (`state`
`ok`, `missing` or `unreadable`, `updated`, `stale`), the counts
`installed`, `newer` (updates found), `ready` (can be started now) and
`blocked`, and `packages`, one object per installed package with
`installed_version`, `available_version`, `state`, `newer`,
`requirements` and `reason`.

Exit status of `outdated`: 0 when the check ran, with or without updates;
10 when there is no catalogue or it cannot be read (run `apkg update`);
5 when it was given a package name. A catalogue older than two weeks is
reported as such, because an empty list then says little.

`upgrade` installs a newer version of a package, or a newer packaging
revision of the same version, and keeps files you changed. Versions are
compared as numbers (1.10 is newer than 1.9) only when they consist of
numbers and dots; apkg refuses an older version and any version it cannot
order, and says why. `rollback` returns to what the last upgrade replaced.

`upgrade --all` first prints the plan: every update it will install and
every one it skips, with the reason. Then it upgrades the packages one after
the other, each exactly as `apkg upgrade <id>` would, and reports each.
Each package is its own operation: one that fails, for example because a
file you changed would be overwritten, is left as it was and the others
continue. The group is not undone as a whole. `apkg --dry-run upgrade
--all` prints each package's dry run and changes nothing. Its exit status
is 0 when every planned upgrade succeeded (or there was nothing to do) and
10 when any failed or was stopped; the dry run exits with 10 when any
package's dry run failed.

### Ctrl-C, redirection and terminals

Ctrl-C stops an install or upgrade while that is still free: during the
download, or right after the archive has been verified. Once files are
being changed it is not acted on; the operation finishes, and apkg says
that the Ctrl-C came too late to stop it. `upgrade --all` then stops before
the next package, and exits with 10. Ctrl-C during `apkg update` stops the
download and keeps the catalogue you had. (On mainline AROS the TCP stack
itself breaks the transfer on Ctrl-C; apkg reports that as a cancel too.) A command that was stopped can
simply be run again.

Output redirected to a file (`apkg outdated >RAM:updates.txt`) is plain
text, as is output with `--plain`. In a terminal that tells apkg what it
can show, such as Limpet, apkg uses colour and wraps to the window; in the
standard Shell window it prints plain text. Commands apkg prints for you to
copy carry your `--root`, quoted when needed.

### apkg and PkgManager are updated separately

`self-update --check` checks for a newer stable apkg release. `self-update`
replaces the apkg executable you started and keeps the previous copy as
`apkg.old`. It does not update PkgManager: take PkgManager from the release
archive. In PkgManager, **About** > **Check for a newer version** says
whether a newer release exists. When the latest release has no archive for
your machine (as for Raspberry Pi since 0.4.1), `--check` says so and your
apkg stays as it is.

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
