# PkgManager 0.3.1-rc1: ARexx suite with the synchronised close click (2026-10-01)

The ARexx suite of `docs/testing/rc1-gui-tls-linux.md`, run again after the
driver fix in commit `157477b`. The first acceptance run on this host
(`docs/reports/2026-10-01-rc1-gui-tls/`) failed `closestart` and `gone2`
because the close click came a fixed 4 seconds after the phase started,
during the removal that comes before the install. The driver now waits until
the window shows the install running, and `grade.py` checks in the window's
log that the close request came during the install.

## What was tested

- The same staged files as `rc1g10010102a` (`stage-arexx.sha256`): release
  `v0.3.1-rc1`, `apkg`
  `e4583d6aeb20af0de62b96827cc5c9c43984f4abf5519dbcc7eb12283f2b5aae`,
  `PkgManager` `732ecd9b356c200e3436e5f3bf49240c30b5698050e9aea3c6464eed360070ff`.
  No binary was rebuilt.
- Tests at commit `157477b` (branch `mbedtls`): `tests/arexx/run-on-pool.sh`
  and `tests/arexx/grade.py` as changed there, everything else as at
  `2a715e5`.

## Where it ran

The same host and guest setup as the first run (see its README): Fedora 44,
QEMU 10.2.2 with KVM, AROS One 1.3 x86_64 (ABIv11), 2 GB, 2 CPUs, a fresh
copy-on-write overlay per boot, the local implementation of the pool
commands. The two runs below ran at the same time on two guests.

## Runs

| run id | result |
|---|---|
| rc1g10011144a | every phase passes: guest checks 194 passed, 0 failed; host checks 30 of 30 hold (`grade-a.md`) |
| rc1g10011144a2 | the same (`grade-a2.md`) |

In both runs the click waited for the install
(`rc1g10011144a*-host-closestart.txt`: synchronised `yes`), and the window's
log (`rc1g10011144a*-pm2.log`) shows the order the phase expects:

```
job 2 install cls from arexx
arexx wait for job 2 (running)
close requested during extract can_cancel=0
done install cls status=0
job 2 install cls done: installed cls
quit
```

The install could not be cancelled at that point, so it finished, the
parked `WAIT` received its final state (`gone2`: `wait-got-final-before-quit`,
`job-finished-despite-quit`), and only then did the window quit.
`rc1g10011144a*-10-closing.png` is the screen 2 seconds after the click.

## Verdict

The ARexx suite passes in full, twice, on the tested ABIv11 configuration.
With the network acceptance and the ARexx examples of the first run, every
pass of `docs/testing/rc1-gui-tls-linux.md` has now passed there. Other
architectures and ABIv1 were not part of this run.

## Files

`rc1g10011144a*` (what the guest wrote, by run id, plus the host's record of
the close click and the driver's times), `grade-*.md`, `stage-arexx.sha256`;
`MANIFEST.sha256` lists them all.
