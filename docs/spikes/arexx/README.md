# ARexx on AROS One: what a host can rely on

Measured 2026-09-27 before and while PkgManager got its ARexx port
(`docs/arexx.md`). Machine: pool slot `v11-2`, AROS One 1.3 x86_64 (ABIv11),
QEMU TCG. The interpreter is `REXX-Regina_3.5(MT) 5.00 31 Dec 2009`, run
through `AROS:Rexxc/RX` and `C:RexxMast`, which the system starts at boot.
`rexxsyslib.library` is version 44.

`rxprobe.c` is a host port that logs every message and answers with chosen
RC, Result2 and variable settings. `probe.rexx` and `probe2.rexx` drive it,
`probe.script` and `probe2.script` run them on the guest. Built with
`x86_64-aros-gcc -O2 -Wall -I$SDK/include rxprobe.c -L$SDK/lib -lamiga -o rxprobe`,
toolchain and SDK from `~/Work/AROS`.
`crash.rexx` and `crash2.rexx` narrow down the crash described below.

## Findings

1. **RC is wrong for every command sent to a port.** After a successful
   command, RC holds the RESULT text (`echo rc=[hello]`, for a reply of
   "hello world"). After a failed one, RC is empty. `SIGNAL ON ERROR` still
   fires, and Regina's own trace prints the correct code (`+++ RC=10 +++`).
   The cause is in `contrib/regina/envir.c`: RESULT's string was passed as
   RC's value. It was fixed upstream in AROS contrib `0258b5de4`
   (2026-09-18), which AROS One 1.3 and the current ABIv11 bases predate.
   **A script that tests `IF RC = 0` misreads every answer on these systems.**
2. **What does hold:** with `OPTIONS RESULTS`, RESULT is set after a success
   (to "" for an empty answer) and dropped after a failure. Regina always
   sends `RXFF_RESULT`, even without `OPTIONS RESULTS`.
3. **Results are limited to 65535 bytes.** A 70000-byte result arrived as
   4464 bytes, 70000 mod 65536: an argstring's length is 16 bits. A host must
   refuse a longer answer, not send it.
4. **`SetRexxVar` works, and then takes RexxMast down.** Setting a variable
   in the calling script (RC2, a stem) works from the top level of a script:
   50 calls in a row, no problem. PkgManager called amiga.lib's
   `CheckRexxMsg` and then `SetRexxVar`; both talk to RexxMast, and which of
   the two corrupts memory was not separated. Neither is used now. From inside a REXX `PROCEDURE`, the way
   anyone wraps a command, RexxMast dies after 30 to 40 calls, with a
   privilege violation in `tlsf_freevec`. It happens with or without
   `DROP rc2` first (runs RXA2, RXC2 below). Regina handles RXSETVAR on a
   separate task, through the global `subtask_tsd`
   (`contrib/regina/amifuncs.c`, `ReginaHandleMessages`). The exact mechanism
   was not pursued. **So PkgManager sets no variable in the caller.** The
   reason for a failure is kept per client and read with `LASTERROR`. Run
   AX7's stress phase sends 100 commands from a PROCEDURE, half of them
   failing, with no crash.
5. **Special characters pass through unchanged** in both directions: spaces,
   a tab, `"`, `'`, `;`, `$`, `&` in the command, and a newline in a result
   (`hex=6F6E650A74776F`).
6. **An unknown port** is a FAILURE condition in the script (`ADDRESS
   NOSUCHPORT` traced `RC=30`). Regina looks the port up by name for every
   command, so a script talking to a PkgManager that has quit gets an error,
   not a message sent into freed memory.

Read in the source rather than measured: Zune's own ARexx dispatcher
(`workbench/libs/muimaster/classes/application.c`, used when an application
sets `MUIA_Application_Commands`) does the following:
- it leaves RC 0 for an unknown command;
- it matches command names by prefix (`IN` would run `INSTALL`);
- it calls the command's hook even when ReadArgs refused the arguments;
- it keeps one command's `MUIA_Application_RexxString` for the next.

PkgManager therefore serves a port of its own from its event loop.

## Runs

Result files and screenshots are not kept in the repository. They were
collected from the machine's results disk.

| run | what | outcome |
|---|---|---|
| P1 | `probe.rexx` against `rxprobe` | findings 1 to 6; the run id picked up a trailing blank (`PARSE ARG run` keeps it; use `PARSE ARG run .`) |
| P2 | `probe2.rexx`: RC and RESULT right after each command | RC = RESULT text on success, empty on failure; RC2 set by `SetRexxVar` at top level |
| RXA2 | first acceptance attempt, PkgManager setting RC2 with `SetRexxVar` | RexxMast died at the 11th failing command from `ask: PROCEDURE` |
| RXC1 | `crash.rexx`: 50 failures at top level, each special character alone and together | no crash |
| RXC2 | `crash2.rexx`: the harness's `PROCEDURE` pattern, 40 times | RexxMast died after 30 calls, without `DROP` as well |
