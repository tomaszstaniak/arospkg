# PkgManager 0.3.1-rc1: GUI worker, network and TLS acceptance (2026-10-01)

The run described by `docs/testing/rc1-gui-tls-linux.md`: the ARexx suite,
the ARexx examples and the network acceptance, each from a fresh boot, on
the published rc1 binaries.

## What was tested

- Release `v0.3.1-rc1`, archive `arospkg-0.3.1-rc1.x86_64-aros-v11.zip`,
  SHA-256 `01d5bdd904fe52a8e24c1abc17fa0cb591998bc6e672be68c7e4314c8d0f325f`,
  fetched and checked by `tests/rc1net/fetch-rc1.sh`.
- `apkg` `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae`,
  `PkgManager` `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`.
- Fault-injection build `tests/rc1net/bin/PkgManager-entropyfail`
  `f81b0e9c1ea3b622d3c1b5cef3fbe9448755b027f446c0d72c39bfcac9a4bba5`, used
  only for `entfail`, `quitent` and `goneent`. Not a release binary.
- Tests at commit `2a715e5b54720c5ebe366d8a5537321726807df7` (branch
  `mbedtls`). No binary was rebuilt and no test file was changed.
- Every staged file and its hash: `RUN.txt`.

## Where it ran

Not on the author's pool. A Fedora 44 x86_64 host, QEMU 10.2.2 with KVM, one
guest at a time: AROS One 1.3 x86_64 (ABIv11), 2 GB, 2 CPUs, e1000 user
networking, vmware VGA at 1024x768, usb-tablet. Each boot used a fresh
copy-on-write overlay of one installed AROS One disk (qcow2 SHA-256
`c766559f856b39bde0c83e7af2119424e448e2260d3007a30170dc00a7e5acaf`), with AROSTCP and
`ENV:SYS/Certificates/ca-bundle.crt` present.

The scripts drove it through `tests/arexx/pool-lib.sh` unchanged. In place
of the pool's `vm.sh` and `tools/vmctl.py`, a local implementation of the
same commands was used: the staged files as `payload.zip` on an ISO labelled
POOLINPUT, a FAT32 disk labelled RESULTS that AROS One mounts as
`RESULTS:`, keys, clicks and screenshots over QMP. Two properties of this
setup matter for reading the results:

- AROS One maps the usb-tablet's range onto the screen with a margin; the
  local `vmctl.py` corrects for it, so a click at (50,300) lands at (50,300).
- FAT writes to `RESULTS:` are lazy; the machine is powered off 10 s after
  the last command so the last file is not lost.

## Runs

| run id | pass | result |
|---|---|---|
| rc1g10010101 | (none) | stopped while staging, before any guest: the ARexx fixtures need the Soliton and CLS archives in `.cache/archives`, which was empty. Both were fetched and checked against `tests/release-index.json` |
| rc1g10010102a | ARexx suite | 25 of 27 phases pass; `closestart` and `gone2` fail, and with them the `waitjob` check for the install that never started (below). Guest checks 191 pass, 3 fail; host checks 26 of 29 hold |
| rc1g10010102e | ARexx examples | did not finish: the host shell running the driver hit its time limit; not counted |
| rc1g10010102e2 | ARexx examples | pass: every example holds (`grade-examples.md`) |
| rc1g10010102n | network acceptance | pass: every phase (`grade-net.md`) |
| rc1g10010102a2 | ARexx suite, again | same result as `a`, same counts (`grade-arexx-2.md`) |
| rc1d1001A, B, C, E | ARexx suite, diagnostic | the close click of `closestart` timed differently; not acceptance runs (below) |

## Network acceptance (rc1g10010102n)

Release `PkgManager`:

| phase | result | what it shows |
|---|---|---|
| update | PASS | Update from the window's worker: the real catalogue over HTTPS |
| archives | PASS | gmore from AROS Archives on an empty cache, verified |
| github | PASS | micropolis from GitHub Releases on an empty cache, verified |
| quitnet, gonenet | PASS | window closed; window and lock gone |
| tls | PASS | archive on `expired.badssl.com` refused with the certificate error; nothing installed, no partial file, lock released (`04-tls-refused.png`) |
| recover | PASS | the next install after the refusal succeeds (`05-recovered.png`) |
| quittls, gonetls | PASS | window closed; window and lock gone |

Fault-injection build, reported apart:

| phase | result | what it shows |
|---|---|---|
| entfail | PASS | forced entropy failure: Update refused, nothing written, window still answers (`06-entropy-failed.png`) |
| quitent, goneent | PASS | window closed; window and lock gone |

## ARexx suite: closestart and gone2

Both runs of the suite fail the same three checks, and pass everything else
(including `cancel`, `quitstart`/`gone`, `collide`, `norexx`, `nomem`,
`update`):

- `closestart`: `INSTALL cls` answered `NOPORT PKGMANAGER is not there`;
- `gone2`: no final state for the parked `WAIT`, and soliton is reported as
  finished despite the quit; `waitjob-` in `a2` follows from the missing job
  id.

What the logs show: after `collide`, the driver closes the second window
(`arexx-09-two-windows.png`: the front window is "PkgManager (no ARexx port)").
That window's log ends with `quit`. The first window then logs
`close requested during start can_cancel=0` while running the `REMOVE cls`
that `closestart` begins with, finishes that removal (`status=0`) and quits,
so the `INSTALL` that the phase expects to be closed during finds no port.
PkgManager's handling of the close is the documented one (a non-cancellable
job finishes, then the window quits); what differs from the earlier runs on
the pool (`docs/reports/2026-09-29-v03/arexx/`, AX9: `closestart` and
`gone2` pass on 0.3) is when the close request reaches the first window.

## Diagnosis of closestart

The committed driver (`tests/arexx/run-on-pool.sh`, `rx closestart 4`)
clicks the close gadget a fixed 4 seconds after typing the `closestart`
command. The phase (`rxtest.rexx`, `p_closestart`) first sends `REMOVE cls`
and waits for it, and only then sends the `INSTALL` that the click is meant
to interrupt. On this host the removal takes about 3 seconds after the
script starts, and the script itself starts some time after the command is
typed, so the click arrives during the removal.

Four diagnostic runs (`rc1d1001-DIAG.txt` lists the two changes, both made
outside the repository: guest timestamps around each step of the phase, and
a variable delay before the click) on the same stage and binaries:

| run | click after | result |
|---|---|---|
| rc1d1001A | 4 s, as committed | same failure: `close requested during start` while the `REMOVE` runs, then `NOPORT`; 191 pass, 3 fail |
| rc1d1001B | 20 s | every guest check passes (194), but the click came after the install had finished, so the window was idle; the host check "the close gadget during an install" does not hold (28 of 29) |
| rc1d1001C | 6 s | everything passes: 194 guest checks, 29 of 29 host checks; `close requested during extract can_cancel=0`, the install finishes, `WAIT` gets the final state, then the window quits |
| rc1d1001E | 8 s | same as C |

So the failure in `a` and `a2` comes from the driver's fixed delay being too
short for this host, not from PkgManager: with the click inside the
`INSTALL`, PkgManager handles it as documented. The guest and host clocks
are not synchronised, so the timestamps are compared only within each side.

As `docs/testing/rc1-gui-tls-linux.md` asks, `tests/arexx/` was not changed. A fix belongs in the
driver: click only once the `INSTALL` has been answered, not after a fixed
time chosen on a faster machine.

## Verdict

The network, TLS and entropy acceptance and the ARexx examples pass on the
tested ABIv11 configuration. The ARexx suite as committed does not pass on
this host: `closestart` and `gone2` fail in both runs. The diagnostic runs
show the cause is the driver's timing; with the click timed into the install
the whole suite passes (C and E). rc1 acceptance is not closed by this run:
the ARexx suite still has to pass with a driver that waits for the install.

## Files

`grade-*.md` (grades), `rc1g10010102a*`, `rc1g10010102a2*`,
`rc1g10010102e2*`, `rc1g10010102n*` (what the guest wrote, by run id),
`rc1d1001*` (the diagnostic runs: only the files of `closestart`, `gone2`,
the `waitjob` checks, the window's log and the grade),
`RUN.txt`, the screenshots named above; `MANIFEST.sha256` lists them all.
