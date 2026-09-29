---
title: "Upgrade and rollback in the window"
date: 2026-09-25
machine: onetest, a private copy of the AROS One 1.3 disk, QEMU/TCG
binary: PkgManager beca593cf29e5258 (sha256 prefix), ABIv11; apkg 6dcf483579f3c0ab
driver: tests/gui-upgrade-accept.sh (host side), tests/gui-upgrade-launch.script (guest side)
---

# Upgrade and rollback in the window

The same operations the CLI got on 2026-09-25, in `PkgManager`, without
widening them: same upstream version and a higher revision only; a rollback
only when the previous revision and its archive are there, and a visible
reason when they are not; before either, the plan **as data from the
library** (`pkg_upgrade_preview`, `pkg_rollback_preview`, `pkg_plan`): from
and to, the counts, the conflicts, shown and confirmed; the same worker,
progress, `can_cancel` and close-protection as an install; afterwards a
refill that keeps the selection and the filters.

Evidence: the window's own event log, the Shell's `Status` listing after
each start of the program, `apkg --json list` and `apkg info` read from the
registry, and the process listing after the window closed. The screenshots
in `docs/spikes/gui3/` illustrate. The window ran with `--slow 300` and
`--yes` (a test switch: the confirmation requester answers itself and says
so in the log).

## What happened, from the log

```
install pressed for soliton                         done install status=0
upgrade pressed  -> plan upgrade soliton: 2.2-aros0 -> 2.2-aros2
                    add=1 replace=1 remove=0 kept=0 unchanged=32 conflicts=0
                    confirmed automatically (--yes)     done upgrade status=0
rollback pressed -> plan rollback soliton: 2.2-aros2 -> 2.2-aros0
                    add=0 replace=1 remove=1 kept=0 unchanged=32 conflicts=0
                    confirmed automatically (--yes)     done rollback status=0
upgrade pressed  -> plan … conflicts=1
                    why=a file you changed would be overwritten by the new revision: Soliton.guide
                    (no confirmation, no operation)
upgrade pressed  -> plan … add=2 replace=0 … conflicts=0   done upgrade status=0
detail soliton: installed 2.2-aros2, index 2.2-aros2, can_upgrade=0 can_rollback=0
                why=the previous revision's archive is not in the cache (SYS:PkgSol/cache/previous/soliton.zip)
worker task gone 0 ms after done
quit
```

| acceptance point | evidence |
|---|---|
| upgrade only for a supported revision change | `detail … can_upgrade=1` only once the index offered revision 2 over installed 0; at 2 over 2: `can_upgrade=0 why=… not newer`; the button follows |
| from → to, plan and conflicts shown before acting, from the library | the `plan` lines carry `pkg_plan` verbatim; `g3` shows "upgrade available: 2.2 revision 0 -> 2" in the panel |
| the program runs after the upgrade and after the rollback | `gui-run1..3.txt`: one `Loaded as command: Soliton` process after the install, after the upgrade, after the rollback (`run1..3.jpg`), and the upgrade succeeded with Soliton **running**, its drawer held as current directory |
| conflict refused, nothing changed, without a Shell | `conflicts=1 why=… Soliton.guide` then no `confirmed` line; the status line reads "upgrade not possible: …" (`g6`); the next upgrade after the local change was removed went through |
| rollback unavailable, with the reason visible | after the previous archive was deleted: `can_rollback=0 why=the previous revision's archive is not in the cache (…)`, "rollback unavailable: …" in the panel and the button greyed (`g8`) |
| same worker, progress, `can_cancel`, close-protection | the preview and the operation both report `verify can_cancel=1`, `extract`/`publish can_cancel=0`; `worker task gone 0 ms after done` before unloading |
| selection and filters kept | every `refill (done)` carries `keep=soliton` |

`gui-info.txt` after the last upgrade: `"revision":2`, `"previous": {
"registry": ".../db/previous/soliton.json", "archive":
".../cache/previous/soliton.zip" }`: the path is recorded; the file is the
one the scenario deleted, which is exactly what the panel reports.

## Five runs, three binaries

`superseded/` holds the four earlier logs. What each showed:

1. *attempt1* (`f488156c`): the driver, not the window. A Zune Cycle does
   not notify when the same entry is re-chosen, so "refresh" did nothing and
   the Upgrade button stayed disabled; and Soliton, once started, took the
   keyboard focus, so the Shell commands meant to record and stop it went to
   the game. The install itself and the clean quit were fine.
2. *attempt2* (`f488156c`): the whole scenario passed. Two things in the
   screenshots: the panel header showed the **index's** revision for an
   installed package ("2.2-aros2 installed" after a rollback to 0), and the
   "rollback unavailable" line was missing after the archive was deleted.
3. *attempt3* (`f488156c`): the same, now with `Status` listings (the Shell
   re-focused before typing): one Soliton process after each operation.
4. *attempt4* (`eb78207c`): the header shows the installed revision, and a
   `detail` log line records what the library told the buttons. It showed
   the "rollback unavailable" reason **was** computed correctly and **was**
   in the panel, as its seventh line, below the six the Floattext showed.
   Correctness had been there; visibility had not.
5. This run (`beca593c`): the availability lines sit under the header and the
   panel shows eight lines. `g3` and `g8` are the proof that the fix is
   visible, not only logged.

## The two safety points from the review

- **The worker wait no longer has a deadline.** `job_wait_gone` polls until
  the task is gone, reporting every 3 s through the status line and the log
  ("worker task still present N ms after done"); the window never unloads
  the code while the task exists. In every run here the task was gone at the
  first poll: `worker task gone 0 ms after done`.
- **An interrupted rollback no longer loses an archive.** The archive and the
  previous registry entry a rollback displaces are moved under the
  transaction's staging root, not deleted, and recovery puts them back. The
  CLI run of the same day (`docs/reports/2026-09-25-upgrade`, binary
  `6dcf4835`) checks it after an undone `--interrupt-at 10 rollback`:
  `current archive kept`, `previous archive kept`, `previous registry entry
  kept`.

And the wording: nothing here calls a file "config". A file is what the
hash says: unchanged, changed locally, or not the package's.
