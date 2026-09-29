# arospkg

A package manager for AROS x86_64. `apkg` is the Shell client, `PkgManager`
a Zune window, and both are built on one static library, `libpkg`. They
install programs from AROS Archives and from their authors' release pages,
checking every download against a small public index:
[arospkg-index](https://github.com/tomaszstaniak/arospkg-index).

Version 0.3 is built for **AROS One x86_64 (ABIv11)**. It installs,
removes, upgrades and rolls back packages, keeps files you have changed,
verifies what is installed, and can show what an operation would do before
doing it. `apkg show` describes a package: whether it is built for this
machine, what it needs, what is installed. PkgManager has an ARexx port.

![PkgManager: the catalogue, with GrafX2 selected](docs/images/pkgmanager.png)

## Using it

Download `arospkg.x86_64-aros-v11.zip` from the
[releases](https://github.com/tomaszstaniak/arospkg/releases). The README
inside it lists what the machine needs and the commands. In short:

```
apkg update
apkg search
apkg install zaphod
apkg remove zaphod
```

`apkg search` lists what the catalogue offers, and `apkg show` says whether
a package is built for this machine and whether the machine has what it
needs, before anything is downloaded:

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
- [metadata reference](docs/reference/metadata.md): every field, and who
  checks it;
- RFCs for an [embedded package manifest](docs/rfc/0001-package-format.md)
  and an [application folder format](docs/rfc/0002-application-folder.md),
  both proposals;
- [ARexx interface](docs/arexx.md);
- [status and history](docs/STATUS.md), and the test evidence for each
  release under [`docs/reports/`](docs/reports/).

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
the matching AROS One SDK, which provides OpenSSL 1.1.0h and zlib.

```
TC=/path/to/toolchain SDK=/path/to/sdk sh src/build.sh       # apkg
TC=/path/to/toolchain SDK=/path/to/sdk sh src/build-gui.sh   # PkgManager
sh tools/make-release.sh                                     # dist/*.zip
```

`src/build-mainline.sh` builds `apkg` for mainline AROS (ABIv1). That build
is maintained but not released or tested for 0.3.

`tests/run-host-tests.sh` runs the unit tests on the host: ZIP, LHA,
SHA-256, the upgrade planner, the ARexx parser, the index generator and the
documentation examples. It needs a C compiler, Python 3 and `lha` (lhasa).

The tests on AROS (`tests/*.script`, `tests/arexx/`, `tests/show/`) run on
AROS One under QEMU. The shell scripts that drive them were written for our
own test machines and will need adapting elsewhere. What each release was
tested with, and the results, are in `docs/reports/`.

## Licence

MIT, see [`LICENSE`](LICENSE). Release archives also contain OpenSSL and
zlib, under the licences in `release/licenses/`.
