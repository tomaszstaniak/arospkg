---
title: "Spike: is Lock(EXCLUSIVE_LOCK) the process mutex for pkg?"
status: complete
state: measured
created: 2026-09-05
updated: 2026-09-05
---

# Spike: DOS exclusive locks as pkg's process mutex

## Why

Section 2 v2 proposed a lock file with a task name and a boot id derived from
wall clock minus uptime. The review called it fragile (task names are not
unique, the clock can be corrected) and pointed at `Lock(path,
EXCLUSIVE_LOCK)` instead, whose autodoc says there may be only one exclusive
lock and that a locked object cannot be deleted. If AROS releases the lock when
its owner dies, the whole stale-lock apparatus disappears.

So: measure it before designing around it.

## What was run

`locktest.c`, built with `build.sh` against the mainline SDK, run on **mainline
AROS x86_64 in QEMU** (`vm.sh start`, machine `mainline`), 2026-09-05. Nine
checks in one process, because an AROS Shell is awkward to drive one command at
a time. `IoErr` 202 is `ERROR_OBJECT_IN_USE`.

```
== locktest suite on T:locktest.tmp ==
1 first exclusive       OK   (lock=0000000002042340)
2 second exclusive      FAIL IoErr=202  (want FAIL)
3 shared while excl     FAIL IoErr=202  (want FAIL)
4 delete while locked   FAIL IoErr=202  (want FAIL)
5 open while excl       FAIL IoErr=202
6 relock after UnLock   OK   (lock=0000000002042340)
7 child 'leak' rc=0  (0 = it really held the lock)
8 lock after child died FAIL IoErr=202
   => lock SURVIVES its owner. A crash wedges pkg until reboot.
== unexpected results: 0 ==
```

## Findings

**Exclusion is real and enforced by the handler.** A second exclusive lock, a
shared lock, a `DeleteFile` and an `Open(MODE_OLDFILE)` all fail with
`ERROR_OBJECT_IN_USE` while one exclusive lock is held. This is a genuine
mutex, not an advisory convention, and it needs no timestamp, no task name and
no clock.

**But the lock outlives the process that took it.** Check 7 runs a child that
takes the exclusive lock and returns *without* calling `UnLock`; the child
exits with rc=0, and check 8 then fails with 202. Nothing reclaims it. If a
plain successful exit leaks the lock, a crash certainly does.

**And it cannot be broken.** Check 4 is the reason: a locked file cannot be
deleted, so there is no way for a later run to clear a lock left by a dead
process. The machine stays wedged until reboot.

That combination is the worst possible shape for a package manager: the state a
crash leaves behind is exactly the state in which the next run must be able to
start, because a crash is when journal recovery is owed.

**Check 5 also matters for the design:** an exclusive lock blocks readers.
Whatever object is locked cannot double as a file describing who holds it.

## Consequence for the design

Section 2 v3 proposed a hybrid: an exclusive lock held only for the moment of
acquiring an ordinary owner record. **That was wrong, and the review caught
it.** A crash *inside* that short window still leaves the sentinel locked
forever, and "the window is small" is not an argument available to a mechanism
whose entire purpose is surviving crashes. The measurement above was already on
the page and the v3 text contradicted it.

The second experiment below replaces the whole approach: `Rename()` does the
job with no kernel lock at all.

## A wrong result, and how it was caught

The first run reported the opposite: "OS RECLAIMS the lock. No stale-lock
logic needed." It was wrong: the child had been told to lock `T:lk` while the
suite worked on `T:locktest.tmp`, so it locked nothing, failed, and the parent
then took a lock no one was holding. The child's `rc=10` was visible in the
output and contradicted the conclusion printed two lines below it.

The probe now refuses to draw the conclusion at all unless the child returns 0,
and says so in the output. A check whose failure still lets the suite print a
verdict is not a check.

## Second experiment: Rename() as the lock (2026-09-05)

The review proposed using the `Rename()` semantics discovered while writing
Section 2 v2 (refusal to overwrite) as an atomic create-if-absent, so that no
kernel lock is taken at all and nothing can be left behind that a later run
cannot clear. `renlock.c` measures it. Same machine, same build.

```
1 acquire on free lock  OK (want OK)
2 acquire when held     FAIL (want FAIL)
3 holder is readable    'a' (want 'a')
4 stale lock deletable  OK (want OK)
5 acquire after delete  OK (want OK)
ENV: resolves to RAM Disk:ENV
```

Contention, two processes, 150 attempts each, each stamping its id into a
shared file inside the critical section and reading it back:

```
a wins=50 contended=100 violations=0
b wins=1  contended=149 violations=0
```

**Zero mutual-exclusion violations.** Every acquire that succeeded held the
critical section alone. And unlike the DOS lock, the object left behind is an
ordinary file: check 3 shows a later process can read who holds it, and check 4
that it can delete a stale one: the two things `Lock(EXCLUSIVE_LOCK)` makes
impossible.

`ENV:` resolves to `RAM Disk:ENV`, so it is volatile across a reboot by
construction. A random boot token written there without `GVF_SAVE_VAR` needs no
`wall clock - uptime` arithmetic, which removes the part of the v2/v3 design
that depended on the clock being sane.

### Two caveats, both mine

- **The win distribution is not a clean measurement.** `a` took the lock 50
  times and `b` once. Neither child backs off, so this says little on its own,
  and the machine was not quiet: another test was using it, and typed into
  the same Shell during the run. The safety result
  (`violations=0`) is a property of the programs and survives that; the
  fairness numbers do not, and no design decision here rests on them. A caller
  must retry with backoff and must not assume fairness.
- **`IoErr` was reported as 0 for the failed acquire** because the probe called
  `DeleteFile` to clean up its nonce before reading the error. The failure
  itself is real and repeatable; the *code* was clobbered by the probe. Fixed
  in `renlock.c` (the error is captured before cleanup) but **not re-run**, so
  the specific value (presumably `ERROR_OBJECT_EXISTS`) is still unmeasured
  and is not claimed anywhere.

## Conclusion

`Rename()` is the lock. The DOS exclusive lock is not usable for anything a
crash must be recoverable from, and with `Rename()` available it is not needed
for anything else either.

**What the design did with this (2026-09-05).** Acquisition and release use
`Rename()` as measured here. *Reclaiming* a stale lock automatically was
designed, reviewed, and then dropped from the MVP: it needs a reaper role, an
identity check so a lock re-acquired between judgement and removal is not
destroyed, a restore path when that check fails, and recovery for a reaper that
dies mid-reap: four states, each reachable by a crash, whose worst outcome is
destroying a live lock. `pkg` instead refuses to run on a stale lock and asks
the user to run `pkg unlock`. The boot token measured above is what makes a
stale lock trivially recognisable after a reboot, which is the common case.

## Files

- `locktest.c`: the probe; `suite` runs everything, other subcommands exist
  for driving pieces by hand.
- `build.sh`: mainline SDK, no libraries beyond dos/exec.
