# PkgManager's ARexx port on AROS One, 2026-09-27

Pool slot `v11-2` (AROS One 1.3, x86_64, ABIv11), QEMU TCG, reserved for this
work. The binary under test:

| binary | SHA-256 |
|---|---|
| PkgManager | `7fdb7c80f61eb012372dccbfab9bde6aa8c529c71fb4f9e0ed5d760430d7e000` |
| apkg (unchanged from v0.2) | `a513aea7d8b066911a88466ac83d166f9c30578fb80768b0961d14df426040fc` |

| run | what | result |
|---|---|---|
| AX7 | the acceptance run: 27 phases of `tests/arexx/rxtest.rexx`, driven by `tests/arexx/run-on-pool.sh` | 194 checks passed in the guest, 0 failed; 29 of 29 checks on the event logs and the final state hold ([grade-AX7.md](grade-AX7.md)) |
| EX7 | the five scripts in `examples/arexx/`, run the way a user runs them (`tests/arexx/run-examples.sh`) | all eight invocations print what they should ([grade-EX7.md](grade-EX7.md)) |
| MX2 | the `rapid` phase alone against a build with the old busy test (`mx2/mutant.diff`), `tests/arexx/run-phase.sh` | the test catches it: see [below](#jobs-back-to-back) |
| SP1 | the `spin` phase alone against the previous binary, whose worker ran one priority below the window (`tests/arexx/run-phase.sh`) | the test catches it: see [below](#a-script-that-polls-without-a-pause) |

Every result file names the PkgManager that answered: its first answer is
`VERSION`, which carries the running binary's own hash. The grader checks it
against the staged files (`stage-ax7.sha256`).

## What was tested, and where

The guest decides every check itself against libpkg's state and the files on
disk. The host adds what only it can see: the window's own event logs
(`ax7/AX7-pm1.log` to `pm6.log`), and that every phase and every parked
`WAIT` reported at all.

| requirement | phase, in `ax7/AX7-<phase>.txt` |
|---|---|
| valid and invalid arguments, spaces and special characters | `parse`: unknown command, no prefix match, missing and extra arguments, unknown field, unterminated quote, `"sol iton"`, `"a*"b;$x&y"`, lower case, `KEY=value`. The parser is also covered on the host by `tests/test_rexxcmd.c` |
| queries before and after installing | `queries0`, `installcheck`, `remove` |
| queries change nothing | `queries0`: a listing of the whole root before and after is identical |
| showing a package in the window | `show`, `select`; `shots/ax7-02-show.png` |
| install and remove from ARexx | `installstart`, `installcheck`, `remove`; installing again gives `EXISTS`, removing again `NOTFOUND` |
| jobs back to back | `rapid`: see below |
| a script polling without a pause | `spin`: see below |
| upgrade and rollback of a controlled package | `upgradestart` (declined by CANCEL, nothing changed; then left for the user), `upgradecheck` (confirmed in the window: revision 2, the new file present), `rollbackstart`, `rollbackcheck` (back to revision 0, the file gone); `shots/ax7-05-confirm.png` |
| window and ARexx at once | `guibusy`: an install started by pressing the button is visible to `JOB` with origin `window`, and ARexx gets `BUSY` naming it. While a plan waited for confirmation, clicking Update index did nothing (host check on `AX7-pm1.log`) |
| conflict with the CLI | `installstart`: `apkg` during an ARexx job is refused, "another instance is running", holder "PkgManager worker". `clilock`: an ARexx job while `apkg` holds the root fails with `LOCKED` and changes nothing |
| cancel where allowed, refused after | `cancel`: `CANCEL` during the download is accepted and the job ends `cancelled`, with no drawer, archive or `.part` left. `installstart`: refused with `NOTCANCELLABLE` during extraction. `installcheck`: `FINISHED` after the end |
| the ARexx client ending during an operation | `installstart` ends while its job runs; `installcheck`, a new client, waits for it by number |
| the window closing during an operation | `quitstart`/`gone` (QUIT) and `closestart`/`gone2` (close gadget): the window leaves only after the job has ended, a parked `WAIT` gets the final state, the port disappears, and `apkg` sees the package installed |
| no ARexx, and a port name already taken | `norexx` with `--rexxlib nosuch.library`: no port, the title says so, and the window still removes a package (`AX7-pm4.log`). `collide`: a second PkgManager has no port (`AX7-pm3.log`) while the first keeps answering |
| no memory | `nomem`, on a PkgManager started with `--test-nomem start --test-nomem result`: an answer that cannot be allocated fails with `NOMEM` instead of RC 0 without a result; a job whose final message cannot be allocated is refused before it starts (`CANNOTSTART`), nothing is installed, and the next attempt is refused the same way rather than hanging. `nomem2`, with `--test-nomem prepare`: when the short answer of `INSTALL` (the job number) cannot be allocated, the script gets `NOMEM` and no job exists, nothing is installed; `QUIT` answers `NOMEM` and the window stays |
| `UPDATE` to the end | `update`, on a fresh PkgManager: the public catalogue is fetched over the network, `LIST` shows it, `INFO grafx2` reads it, and an installed package stays installed |
| answers agree with the registry and the files | checked after every change: the drawer, `db/installed/<id>.json`, `apkg list` from the CLI, and `INFO` from ARexx |
| the RexxMast crash stays fixed | `stress`: 100 commands from inside a PROCEDURE, half of them failing, then `LASTERROR`'s semantics |

The grader requires the three `waitjob-*` files: the scripts started with
`Run` that held a `WAIT` open across a confirmation, a QUIT and a close
gadget. A `WAIT` that never came back writes nothing, so a missing file
fails the run.

## Fixes the reviews asked for, and the tests that show them

### Jobs back to back

The window used to count a job as running for as long as the worker's own
flag said so. The worker clears that flag before its final message has
been read. In that gap a new job could be accepted, and the old job's
outcome would then be read as the new job's. The fix has three parts:
- the window counts a job as running until it has recorded the outcome;
- every message from the worker carries its job's number, and one for
  another job is logged and ignored;
- the final message is allocated before the worker starts.

With `--slow`, the worker holds that gap open for 3 s. The `rapid` phase
installs, waits for the last phase (`publish`), then sends `REMOVE` once a
second until it is taken:

- **AX4, the first build with the fix** (`13b4c795`): in each of 5 rounds,
  `REMOVE` got `BUSY` three times during the gap. It was taken only after
  the install was `done`, and both jobs ended with their own outcome
  (`second-binary-13b4c795/ax4/AX4-rapid.txt`).
- **AX7** repeats that on the final binary (`ax7/AX7-rapid.txt`).
- **MX2, the old busy test** (`mx2/mutant.diff`, applied to the source of
  `13b4c795`, the build before the final one): in the first round `REMOVE`
  was taken while the install was still `running`. The install's final
  message was then ignored, logged as "message for job 1 ignored: job 2 is
  out". The install job never became final, and the phase stayed blocked in
  its `WAIT`: `mx2/MX2-rapid.txt` ends there, and `mx2/MX2-pm1.log` holds
  the log. Without the message numbers, that outcome would have been read as
  the remove's.

### A script that polls without a pause

The worker used to run one priority below the window. A script that polls
`JOB` with no pause keeps itself and the window ready all the time, so a
lower-priority worker never runs. The worker now runs at the window's
priority. The port also takes at most four commands per turn of the
window's loop and raises its own signal for the rest, so the window's
input and the worker's messages come in between.

- **SP1, the previous binary** (`13b4c795`, worker at -1): the script polled
  281296 times in 120 s and the install stayed `running` throughout. The
  worker did not send even its first event until the polling stopped
  (`sp1/SP1-spin.txt`; `sp1/SP1-pm1-excerpt.log` holds every line of the
  14 MB log that is not a poll, with the whole log's hash).
- **AX7, the final binary:** the script polled 89586 times without a pause
  and the install finished in 13.2 s (`ax7/AX7-spin.txt`). While the script
  polled, a search typed into the window and its clearing were both handled
  (host check on `AX7-pm1.log`).

### Answers allocated before a command acts

A command that changes something allocates its answer before it acts. That
covers `UPDATE`, `INSTALL`, `REMOVE`, `UPGRADE`, `ROLLBACK`, `CANCEL` and
`QUIT`; for the jobs, the answer is the job's number. If the allocation
fails, the command does nothing and answers `NOMEM`. Before this, the job
was started first and a failure to allocate its number reached the script
as `NOMEM` for a job that was in fact running. `nomem2` tests it.

## Runs that did not count, and why

- **The first binary, `8c248304...`**, passed runs AX1 and EX2
  (`first-binary-8c248304/`). A review of it found the gap above, a final
  message allocated only after the work, an unchecked `CreateArgstring`, a
  `LASTERROR` guarantee stronger than its implementation, and a grader that
  still passed with the three `waitjob` files deleted. All five are fixed;
  that directory is kept as the record of what the review read.
- **AX2**: the first `rapid` phase gave up after 5000 immediate retries,
  before the 10 s install had even finished. The later phases then failed
  one after another, because `cls` was left installed and the Update index
  click meant to be ignored during a confirmation fetched the public
  catalogue instead. The grader failed the run, as it should.
- **AX3**: `REMOVE` sent without any pause kept the worker from running
  under TCG. The worker runs one priority below the window, and a script and
  the window trading messages as fast as they can starve it. `rapid` now
  waits a second between tries, and `docs/arexx.md` tells scripts to do the
  same.
- **RF1, EX3**: two drivers ran on the machine at once, each stopping it
  under the other. `tests/arexx/pool-lib.sh` now takes a lock per machine.
  MX1, the mutant under the older `rapid` phase with no pause, showed the
  same acceptance in the gap as MX2; MX2 repeats it with the phase as it is
  now.
- **EX4, EX5**: in EX4 the click on Proceed was lost, and the next command
  waited behind the example forever. The drivers now check that the window
  has gone, and press again if not (`press_proceed`). In EX5 the window log
  was copied too late to reach the results disk.
- **AX4, EX6** passed on `13b4c795`. A second review then found the worker
  starvation above and the `NOMEM` after a started job. They are kept in
  `second-binary-13b4c795/`.
- **AX5**: the check for the confirmation window read its title bar, whose
  colour changes with which window is active, and aborted the run. It now
  reads the Cancel button.
- **AX6**: every guest check passed. The host check for a responsive window
  clicked a cycle gadget, which only opens its menu, so nothing could be
  logged. It now types a search.

## How this was run

```
src/build.sh && src/build-gui.sh
tests/run-host-tests.sh
tests/arexx/make-stage.sh STAGE
# with AROS_VM_OWNER and AROS_VM_RESERVATION exported for the pool machine
tests/arexx/run-on-pool.sh AX7 STAGE RESULTS-AX7 SHOTS-AX7
tests/arexx/run-examples.sh EX7 STAGE RESULTS-EX7 SHOTS-EX7
tests/arexx/grade.py AX7 RESULTS-AX7 STAGE.sha256
tests/arexx/grade.py --examples EX7 RESULTS-EX7 STAGE.sha256
```

The package root is `SYS:PkgRx`, with its own index:
- soliton revision 1 (the published archive), and a generated revision 2
  with one file changed and one added;
- cls;
- xrick, the one package fetched over the network, because only a download
  can be cancelled.

`--slow 500` does three things: it makes that download slow, holds each
install in extraction for 10 s, and holds the worker's final report back for
3 s. Every timed action then lands where it is meant to. Screenshots are
illustration only.

## Not covered

- Nothing was built or run on mainline AROS (ABIv1) or aarch64.
- `NOINDEX` and `TOOLONG` are implemented but were not provoked.
- The `LASTERROR` gap described in `docs/arexx.md` was not provoked. That
  is a new script landing on a finished script's port and task addresses.
