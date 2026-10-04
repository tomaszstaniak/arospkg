# arospkg

Find, install and remove AROS software from a window or the Shell.
**PkgManager** is the graphical interface; **apkg** is the command-line tool.
Both use the same catalogue and installed packages.

**[Download 0.3.2 for AROS (x86_64, ABIv11)](https://github.com/tomaszstaniak/arospkg/releases/download/v0.3.2/arospkg-0.3.2.x86_64-aros-v11.zip)**
· [Release notes](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.3.2)
· [Other builds (0.3.1, experimental)](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.3.1)

0.3.2 fixes CLI progress during window resizing: complete progress lines
replace the live redraw bar. PkgManager is unchanged from 0.3.1.

## Install

1. Download the ZIP matching your CPU and ABI. The example below uses
   **x86_64 ABIv11**; for another build, substitute its archive and drawer
   names. Other builds are experimental; see the release notes.
2. Open a Shell and use `CD` to enter the drawer containing the ZIP.
3. Run these commands to unpack it, put `apkg` on the command path and
   fetch the software catalogue:

```text
UnZip arospkg-0.3.2.x86_64-aros-v11.zip
CD arospkg-0.3.2.x86_64-aros-v11
Copy apkg C:
apkg --version
apkg update
```

`apkg --version` should report **0.3.2** for this build (0.3.1 for the
experimental builds) and the ABI you selected.
No compiler, Installer script or separate TLS libraries are needed.

Keep the extracted drawer somewhere permanent, not `RAM:`, if using
PkgManager (included in the ABIv11 archive). Installing packages requires
a working network connection.

**Updating an older arospkg?** Close PkgManager and finish running apkg
operations before copying the new programs. Keep `SYS:Packages` and its
database — they contain your installed packages and their records.

## Use the graphical interface

From the extracted arospkg drawer, start:

```text
Run PkgManager
```

Click **Update index** to refresh the catalogue, select an application,
read its requirements and click **Install**. To uninstall it later, select
it and click **Remove**.

Programs are installed in `SYS:Packages/<id>/`. Open that drawer in
Wanderer and launch the program inside. For example, after installing
Soliton, open `SYS:Packages/soliton`.

![PkgManager: the catalogue, with GrafX2 selected](docs/images/pkgmanager.png)

## Use the Shell

For example, find and install the Soliton card game:

```text
apkg search soliton
apkg show soliton
apkg install soliton
```

`show` displays the description, compatibility and requirements before
you download anything. After installation apkg prints where the program
is and the command that opens its drawer in Wanderer:

```text
apkg open soliton
```

Four commands with similar names do different things:

```text
apkg update          refreshes the catalogue
apkg upgrade <id>    updates an installed package
apkg self-update     updates apkg itself
apkg open <id>       opens the drawer of an installed package
```

| Command | What it does |
|---|---|
| `apkg update` | Refresh the available-software catalogue; does not upgrade installed programs. |
| `apkg search` | List available packages. Add a word to search. |
| `apkg list` | List installed packages. |
| `apkg verify soliton` | Check installed files against the package record. |
| `apkg remove soliton` | Remove the package, keeping locally changed files and files it did not install. |
| `apkg upgrade soliton` | Install a newer revision of the same upstream version, when available. |
| `apkg rollback soliton` | Return to the previous revision, if its archive is still cached. |
| `apkg open soliton` | Open the package's drawer in Wanderer. |
| `apkg self-update` | Replace this apkg with the latest stable release; `--check` only reports. |

`self-update` first appears after 0.3.2, so the first release that has it is
installed by hand as above; later ones update with `apkg self-update`.
PkgManager is not updated by it: take it from the release archive.

Both interfaces use `SYS:Packages` by default. For another location, pass
the **same root to both**, every time:

```text
apkg --root Work:Packages update
apkg --root Work:Packages install soliton
Run PkgManager --root Work:Packages
```

This selects a separate package root; it does not move existing installs.

In the development version, use `apkg --help` for common commands,
`--help-all` for the complete list,
and `--help-testing` for test controls. `apkg --about` shows the author,
project page and licence; PkgManager has an **About** button. Detailed build
information remains available through `apkg --version`.

### If a download fails

Check that the system has a working network connection and that
`ENV:SYS/Certificates/ca-bundle.crt` exists. Certificate checks are
required; do not disable them.
If `show` reports a CPU or ABI mismatch, choose a build for your system.

## What it looks like

![apkg search and apkg show in an AROS Shell](docs/images/apkg-search-show.png)

Installing and removing a package from AROS Archives:

![apkg install, list and remove](docs/images/apkg-install-remove.png)

PkgManager does the same from a window, and shows the plan of an upgrade
or a rollback before it runs. The screenshots are from AROS One 1.3 under
QEMU, with the published catalogue.

## Documentation

Start at [`docs/README.md`](docs/README.md):

- [packaging guide](docs/guide/packaging.md): from a program's drawer to an
  approved index entry, tested on AROS;
- [metadata reference](docs/guide/metadata.md): every field, and who
  checks it;
- RFCs for an [embedded package manifest](docs/rfc/0001-package-format.md)
  and an [application folder format](docs/rfc/0002-application-folder.md),
  both proposals;
- [ARexx interface](docs/arexx.md);
- [release notes](https://github.com/tomaszstaniak/arospkg/releases)
  for changes and known limitations.

## Layout

- `src/`: `libpkg/` (all the logic), `pkg/` (`apkg`), `pkgmanager/`
  (`PkgManager`).
- `tools/`: host scripts that import the AROS Archives catalogue, check
  archives and generate the index.
- `index/`: candidates imported from the catalogue and not yet reviewed.
  Approved manifests and the index are in arospkg-index.
- `tests/`: host unit tests, the scripts run on AROS, and pinned fixtures.
- `examples/arexx/`: ARexx scripts for PkgManager.
- `release/`: the README and licences shipped in the release archive.

## Building

You need an x86_64 AROS cross toolchain for ABIv11 (GCC 10.5 was used) and
the matching SDK with `libz.static.a`. The default build uses vendored
Mbed TLS 3.6.7 and jitterentropy 3.7.0. `TLS=openssl` is an optional
alternative, not the backend shipped in 0.3.1.

```
TC=/path/to/toolchain SDK=/path/to/sdk sh src/build.sh       # apkg
TC=/path/to/toolchain SDK=/path/to/sdk sh src/build-gui.sh   # PkgManager
```

`src/build-mainline.sh`, `src/build-i386.sh` and `src/build-aarch64.sh`
build other targets with their matching SDKs and toolchains. Mainline
uses system `getentropy()`. Check the release notes for each target's
test coverage.

The published archives were made from the release commit with
`README=release/README sh tools/make-rc.sh 0.3.1`. Despite its name, this
script packages stable releases too; it requires all four toolchains.
`tools/make-release.sh` is the older packaging path, not the 0.3.1 recipe.

`tests/run-host-tests.sh` runs the unit tests on the host: ZIP, LHA,
SHA-256, the upgrade planner, the ARexx parser, the index generator and the
documentation examples. It needs a C compiler, Python 3 and `lha` (lhasa).

The tests on AROS (`tests/*.script`, `tests/arexx/`, `tests/show/`) run on
AROS One under QEMU. The shell scripts that drive them were written for our
own test machines and will need adapting elsewhere. Test coverage is
summarised in the release notes. Historical run records remain available
in earlier Git revisions; local reports and work notes are not part of
the current public tree.

## Licence

MIT, see [`LICENSE`](LICENSE). The 0.3.1 archives include the licences for
Mbed TLS, jitterentropy (except mainline), zlib and the AROS XTerm
terminal-control client.
