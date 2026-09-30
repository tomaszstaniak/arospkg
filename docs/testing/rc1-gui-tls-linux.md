# PkgManager 0.3.1-rc1: GUI worker, network and TLS acceptance

What is left to accept for PkgManager rc1 on ABIv11. The window flow already
passed (`docs/reports/2026-09-30-rc1-regression/`); it used a local index and
a cached archive, so the window's worker never used the network. This run
covers that, the TLS and entropy failures as the window meets them, and the
ARexx suite on the rc1 binary.

It uses the existing pool harness (`tests/arexx/pool-lib.sh`: `vm.sh`
reservation, staging, `vmctl.py` typing and screenshots, collection). Nothing
here manages virtual machines.

## What is tested

The published binaries, taken from the release archive and checked before
anything is staged (`tests/rc1net/fetch-rc1.sh`):

| file | SHA-256 |
|---|---|
| `arospkg-0.3.1-rc1.x86_64-aros-v11.zip` ([release v0.3.1-rc1](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.3.1-rc1)) | `01d5bdd904fe52a8e24c1abc17fa0cb591998bc6e672be68c7e4314c8d0f325f` |
| `apkg` in it | `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae` |
| `PkgManager` in it | `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff` |
| `tests/rc1net/bin/PkgManager-entropyfail` (test build, see its `.BUILD.txt`) | `f81b0e9c1ea3b622d3c1b5cef3fbe9448755b027f446c0d72c39bfcac9a4bba5` |

The fault-injection build is the rc1 source compiled with
`-DAROS_JENT_FORCE_FAIL` and nothing else; the same command without the flag
reproduces the published `PkgManager` bit for bit. Its results are reported
separately and are not the release binary's. It is not part of any release.

The binaries are not rebuilt for this run, with GCC 16 or anything else.

| requirement | where |
|---|---|
| 1. Update from the window | net `update`: the window's worker fetches the real catalogue over HTTPS; ARexx suite `update` |
| 2. cold cache, Archives and GitHub | net `archives` (gmore, archives.arosworld.org) and `github` (micropolis, GitHub Releases): empty cache, download, verified install, `apkg verify` of every file |
| 3. cancel during the transfer | ARexx suite `cancel`: `--slow`, CANCEL in the download phase, no `.part`, not installed |
| 4. TLS refusal | net `tls`: a package whose archive is on `expired.badssl.com`; job fails with the certificate error, nothing installed, no partial file, lock released; verification is not relaxed anywhere |
| 5. working again after an error or a cancel | net `recover` (install after the TLS refusal); ARexx suite `quitstart` (install after the cancel) |
| 6. closing the window | net `quit*`/`gone*` after each window; ARexx suite `quitstart`/`gone`/`closestart`/`gone2` |
| forced entropy failure | net `entfail` on `PkgManager-entropyfail`: Update fails with "no good randomness", nothing written, lock released, a second Update is taken and fails the same way, the window closes |
| ARexx on rc1 | the existing suite and examples, unchanged, on the release `PkgManager` |

The TLS case uses a local index (`tests/rc1net/make-tls-index.py`), made
from the published catalogue at commit `fbe3d83a8986` (hash checked) with one
extra entry. The published index is not changed. The test depends on
`expired.badssl.com` still serving an expired certificate; if it does not,
`tls` fails and says so.

## Requirements the repository does not provide

- The pool on the Linux host with `vm.sh` and `tools/vmctl.py` in one
  directory, given as `AROS=<that directory>` (default `~/Work/AROS`), and
  `nc` with Unix sockets, `python3`, `curl`, `unzip`, `sha256sum` or `shasum`.
- One reservable **ABIv11 machine with AROS One 1.3 x86_64**, set up as the
  pool's v11 slots: `POOLINPUT:` from the staged ISO, a FAT `RESULTS:` disk,
  `C:UnZip`, AROSTCP starting at boot with network to the internet,
  `ENV:SYS/Certificates/ca-bundle.crt` present, a 1024x768 Workbench. The
  image is not in Git.
- A reservation for this run only. The machine must not be one the index
  build or the GCC 16 tests use, and nothing else may drive it while this
  runs (the harness refuses a second driver on the same machine).

## Running it

```sh
git clone https://github.com/tomaszstaniak/arospkg   # or fetch
cd arospkg
git checkout <the mbedtls commit named in the report>

cd "$AROS"
AROS_VM_OWNER=<owner> ./vm.sh acquire --abi v11 --project arospkg-rc1-gui
export AROS_VM_OWNER=<owner> AROS_VM_RESERVATION=<from acquire> AROS_VM_NAME=<machine>

cd <arospkg>
AROS="$AROS" sh tests/rc1net/run-all.sh ~/rc1-gui-$(date +%Y%m%d)

cd "$AROS" && ./vm.sh release "$AROS_VM_NAME"
```

`run-all.sh` fetches and checks the rc1 archive, stages, and runs three
passes, each from a fresh boot: the ARexx suite, the ARexx examples, the
network acceptance. It takes about 1.5 to 2 hours under TCG. The work
directory then holds `RUN.txt` (run id, machine, test commit, hashes of
everything staged, one line per pass), `grade-arexx.md`, `grade-examples.md`,
`grade-net.md`, the collected results (`res-*/`, with
`COLLECT-MANIFEST.sha256`) and the screenshots (`shots-*/`). Exit 0 only
when all three pass.

## Grading

Every phase is **PASS**, **FAIL** or **NOT RUN** (no result file, or a file
without its `SUMMARY`: the script never reached it or stopped). Every answer
must come from the binary meant for it, by the SHA-256 that `VERSION`
reports: the release `PkgManager` for everything except `entfail`, `quitent`
and `goneent`. The windows' logs must name the same binaries.

To grade again from the collected files:

```sh
python3 tests/arexx/grade.py <run>a <work>/res-arexx <work>/stage-arexx.sha256
python3 tests/arexx/grade.py --examples <run>e <work>/res-examples <work>/stage-arexx.sha256
python3 tests/rc1net/grade.py <run>n <work>/res-net <work>/stage-net.sha256
```

## A pass that did not finish

Each pass is independent and starts from a fresh boot. Run the failed one
again on its own with a **new run id**. A FAT results disk must not have a
file overwritten, and the run id keeps names unique. For example, for the
network pass:

```sh
sh tests/rc1net/run-on-pool.sh rc1gRETRY1n <work>/stage-net <work>/res-net-2 <work>/shots-net-2
python3 tests/rc1net/grade.py rc1gRETRY1n <work>/res-net-2 <work>/stage-net.sha256
```

## The report

Only this run's files go into a report, by run id, with a manifest:

```sh
sh tools/report-add.sh <run>n <work>/res-net docs/reports/<date>-rc1-gui-tls
python3 tools/check-reports.py
```

The report names the host, the machine and its base image, the test commit,
the binaries' hashes, which phases passed, failed or did not run and why, and
keeps the release binary's results apart from the fault-injection build's.
After all passes, the accurate statement is "rc1 acceptance closed on the
tested ABIv11 configuration", not an unqualified "fully tested".
