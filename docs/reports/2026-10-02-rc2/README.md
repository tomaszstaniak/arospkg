# 0.3.1-rc2 acceptance (2026-10-02)

rc2 changes one thing from rc1: a download whose connection breaks is
reported as such ("the connection to HOST broke after N of M bytes:
connection reset", or the TLS error) instead of "the download is the wrong
size". Nothing reaches the cache, as before. Commit
`420e5442b1369a58127600d0cf56c29eaba75e1c`, built with `tools/make-rc.sh`.

## Binaries

| archive | archive SHA-256 | binary SHA-256 |
|---|---|---|
| x86_64-aros-v11 | `e0ab91a3...857d` | apkg `977f52aded5f2139adaab5cde1f330d6426b4dc9c329b961825056a36a7e31fc`, PkgManager `a97d039005fb99d6de5c3d930d71b5471c3e26236c39429e22a441c50e4eedcf` |
| x86_64-aros-v1 | `28532025...8c0` | apkg `0c2f93d9663531c41d749a7d4dd1327ece193e5278913e53f572820db3c45ef1` |
| i386-aros-v0 | `f74c1572...532` | apkg `1d28053c7d8008388ea08e90b39ad40b639da54abfde008194238dcea93ed987` |
| aarch64-aros | `8302214b...e96` | apkg `dae89bf35eb6c73aa3853ba413b10e3a77c64a03cadd28ea1b3efbb0803a944a` |

Full archive hashes: `archives.sha256`.

## Results

| target | system | test | result |
|---|---|---|---|
| x86_64 ABIv11 | AROS One 1.3, pool `v11-2`, QEMU TCG | full regression (`grades.txt`, `driver.txt`): UPGRADE, SOLUP, VERIFY, JSONTEST, REL1, REL2, REL1G, LHATEST, trials A to D, ACCEPT | **pass**: every graded suite meets its expectations; ACCEPT checked the rc2 archive's and apkg's hashes first; trial B as in earlier records (15 of 17 windowed programs alive, isomaker and isotool end by themselves) |
| x86_64 ABIv11 | the same | window flow (`gui-*`) | **pass**: same event sequence as rc1, including the deliberate conflict refusal |
| x86_64 ABIv11 | the same | broken download, `tests/netbreak` (`nbrc2-*`) | **pass**: CLI 19 of 19, window 10 of 10, close 2 of 2. A reset reported as "connection reset" with bytes of total, corrupted data as a TLS error; nothing cached, no registry entry, lock released; an interrupted update leaves the index byte for byte; the next install in the same PkgManager succeeds |
| i386 ABIv0 | ABIv0 20250313, pool `v0-2` | smoke (`rc2i386*`): version, update, search, show, install, start, remove with a test index, expired certificate, broken index download | **pass** on those; not a full regression |
| x86_64 mainline | pool `v1-2` | smoke (`rc2bx8664*`) | **starts; network NOT RUN**: version and binary hash correct, but the machine had no working DNS in two boots ("cannot open bsdsocket.library", then "cannot resolve"). An environment fault, not an apkg result |
| aarch64 | - | - | **built only**: experimental, not runtime-tested |

The power cycle before trial A happened between suites, after LHATEST had
written its reports (the Micropolis window it leaves cannot be closed with
Ctrl-C), as in the rc1 run; no suite was interrupted.

Development runs for the fix are in `../2026-10-02-netbreak-dev/`; the
reproduction of the original report in `../2026-10-02-slow-download-reset/`
(on branch `presentation`).
