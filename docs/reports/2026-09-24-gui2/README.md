---
title: "PkgManager, second increment: browse a category, read the requirements, install"
date: 2026-09-24
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG
binary: PkgManager ece0a930988c8c2f (sha256 prefix), ABIv11, src/build-gui.sh; apkg c24b708f6d54c504
driver: tests/gui-accept2.sh (host side; clicks and drags by coordinate), tests/gui-launch.script (guest side)
---

# PkgManager, second increment

The scenario from the review: *browse a category → pick an application →
understand its requirements → install*. Plus the two executor tests it asked
for: closing the window during a download and during the non-cancellable
phase, and a concurrent CLI attempt, and a slowed transfer so none of them
depends on clicking within the right second.

`--slow 300` (a testing knob in libpkg, `pkg_test_slow`) delays each 16 KB by
about 0.3 s and pauses 6 s after extraction begins. The window was driven from
the host; **the evidence is the window's own event log**, `apkg`'s view of
the registry, the Shell's `Status` listing after each close, and the CLI's
refusal text. Screenshots in `docs/spikes/gui2/` illustrate.

## The scenario

```
refill (start): no index                       -> "no index yet -- press Update index"
update pressed / done update status=0
refill (done)   rows=23 shown=23 installed=0
filter category=game state=All                 -> refill shown=9
filter category=game state=Not installed       -> refill shown=9
refill (search) term="soliton" rows=1 shown=1
install pressed for soliton
  download 0..223945  can_cancel=1 cancel_button=enabled
  verify              can_cancel=1
  extract, publish    can_cancel=0 cancel_button=disabled
done install soliton status=0
refill (done) term="soliton" rows=1 shown=1 installed=1 keep=soliton
```

| review point | evidence |
|---|---|
| category filter and All / Installed / Not installed | `filter category=game` → 9 of 23 shown (`g2`); `state=Not installed` (`g2b`); the cycle's entries come from the index (development, game, office, utility) |
| detail panel: description, version, ABI, requirements, download size | `g3`: `soliton 2.2`, summary, `game/card  x86_64/v11  app  219 KB download`, then `crt.library: satisfied (already open, version 5)`, `stdlib.library: satisfied (already open, version 3)`, probed on the machine when the row is selected, not copied from the index |
| search, selection and filters survive install, remove and refresh | every `refill (done)` carries `keep=<id>` and the term; `g5` shows the row still selected, with `game` still chosen, after the install |
| empty index, no results and a read error told apart | `refill (start): no index` → "no index yet -- press Update index"; a term with no hits → `no packages match "…"`; an unreadable index → the libpkg error text; 0 packages in an index → "the index lists no packages" (the last two are code paths, not exercised here) |
| concurrent CLI attempt refused, window not hung | `gui-lock.txt`: `apkg: another instance is running / SYS:Packages/db/lock.owner / PkgManager worker`; the window's install went on to `status=0` |
| cancel with time to spare | `blips`, 1.4 MB: `cancel pressed` after 153 600 bytes → `status=14 download cancelled`; `g7` shows the outcome on one line; no `.part` in the cache |
| close during a download | `sdlpop`: `close requested during download can_cancel=1` → `status=14 download cancelled` → `quit`; `gui-status1.txt` lists no PkgManager process afterwards |
| close during the non-cancellable phase | second instance: `close requested during extract can_cancel=0` → install **completes**, `status=0` → `quit` (`g10` shows "closing when the current operation finishes…"); `gui-status2.txt` lists no PkgManager process; `gui-list.txt` has soliton installed |

## "Done" is not "gone": the worker's exit, measured

The review's reservation: the worker's last message says the operation
ended, not that the process has. So the window now measures the second fact
before it unloads the code the worker ran: after PM_DONE it polls
`FindTask("PkgManager worker")` under `Forbid()` every 20 ms with a 3 s
deadline and logs the result. Both closes in this run:

```
close requested during download can_cancel=1
done install sdlpop status=14 download cancelled
worker task gone 20 ms after done
quit
…
close requested during extract can_cancel=0
done install soliton status=0
worker task gone 20 ms after done
quit
```

20 ms is one poll: the task was already gone at the first look. Why that
is expected, from the ABIv11 DOS source (`rom/dos/createnewproc.c`):
`CreateNewProc` copies `NP_Name` into memory of its own (`AllocMem` +
`CopyMem`, `ln_Name = name`), so the process holds no pointer into this
program; and the worker's last instruction in this program's code is the
return from `worker_main`, after which the PC is in `DosEntry` in
dos.library, which closes the NIL: streams, frees the argument copy and
ends the task. The `Forbid()` before the final `PutMsg` guarantees the
window cannot run between the message and that return. The header calls
`NP_NotifyOnDeath` unimplemented; the source shows `DosEntry` signalling the
parent with `SIGF_CHILD` at its very end, not relied on here, since the
poll measures the same fact without depending on it.

The previous run of this scenario, on `a5deb1ad` before the measurement
existed, is in `superseded-a5deb1ad/`, identical in every other outcome.

## Four runs, four binaries

Each earlier run is in a `superseded-*` directory named by the binary it ran
on. Each found something a screenshot or the log showed and reading had not:

1. `2b17fc5e` (first): the scenario reused one package, so after `Remove` the
   archive was still cached and the "cancel" test had nothing to cancel;
   `Install` on an installed package is disabled, so the "close during
   download" click closed an idle window. The flow was wrong, not the program.
   Fixed by using a different package per test and deleting the cache before
   the last one.
2. `2b17fc5e` (second, corrected flow): everything below passed except the
   category filter, which the driver never changed: **a Zune Cycle selects on
   release**, its popup opens while the button is held, so a click in place
   is a no-op. Selecting is a drag. And the detail panel printed
   `soliton 2.20`: the revision 0 glued onto upstream's string.
3. `db09740c`: choosing a category put the window into a loop: `refill`
   rebuilt the cycle and set its active entry, which fired the same
   notification a user's choice fires, which called `refill`. Several hundred
   `filter` lines per second until the window closed; the later clicks of the
   flow were lost in it. `MUIA_NoNotify` on the window's own set.
4. `820532a8`: two more. A search with no hits rebuilt the categories from an
   empty list and forgot the chosen one; the cycle now lists the whole
   index's categories whatever the search. And the string gadget acknowledges
   on losing focus, not only on Return, so every click elsewhere refilled the
   list and overwrote the status line with a package count; now only a
   changed term is a search. The driver also failed to clear the search field
   (`solitonblips`), so the cancel and close tests never started.

## Measured, worth keeping

- **Zune Cycle: press opens the popup under the gadget, release selects.**
  `tests/tools/hold.py` presses, holds, and screenshots: that is how the
  entry positions (20 px apart) were read. `vmctl.py drag` selects.
- **A Zune String gadget fires `MUIA_String_Acknowledge` on focus loss**, not
  only on Return. A front end that refills on every acknowledge refills on
  every click.
- **A worker that exits under `Forbid()` after its last `PutMsg`** leaves no
  process behind when the window quits on that message: two closes, two
  `Status` listings, no `PkgManager` in either.
