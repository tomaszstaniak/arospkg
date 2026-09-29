# EmuV0 on AROS One: what was measured, and where it stopped

A bounded experiment, 2026-09-28. The question was whether arospkg can offer
32-bit ABIv0 packages on AROS One x86_64 (ABIv11) through the EmuV0 emulator.
The answer on the configuration we can test is **not yet**: in every case,
with both emulator versions, the emulator ended in a Software Failure. That
includes a plain Shell program as the control. The configuration is recorded
as unsupported for now, and arospkg work goes on without it.

Machine: pool slot `v11-2`, AROS One 1.3, x86_64, ABIv11, QEMU with TCG
(`-cpu qemu64`, 2 GB). Source read: `workbench/tools/AoA` in deadwood2's tree
(EmuV0 1.12, 2026-08-24).

## What is on the machine already (measured, EV1)

AROS One 1.3 ships EmuV0 installed and configured
(`runs/EV1-inventory.txt`):

- `C:EmuV0` (569680 bytes), reporting `EmuV0 1.10 (01/25/26)`. The archive
  on AROS Archives is 1.12 and names its binary `EmuV0-pc`.
- `SYS:Tools/EmuV0`, registered in `ENVARC:SYS/Packages/EmuV0`. At boot,
  `S:Startup-Sequence` changes into each registered drawer and executes its
  `S/Package-Startup`, which assigns `EmuV0:` to the drawer and `LIBSV0:` to
  `EmuV0:Libs` plus `EmuV0:Classes`.
- `LIBSV0:` holds 27 32-bit libraries, among them `muimaster`, `arosc`,
  `crt`, `stdlib`, `autoinit`, `asl`, `reqtools`, `gadtools`, `icon`,
  `datatypes` and `locale`. It also holds partial `dos`, `graphics`,
  `intuition`, `layers` and `cybergraphics`, plus Zune classes and datatypes.

The 1.12 archive does not fit arospkg as it is: it writes to `SYS:C`,
`ENVARC:SYS/Packages` and `SYS:Tools`, and arospkg installs into a package's
own drawer only (`docs/backlog/system-components.md`).

## How a program is started (read in AoA.c; measured where noted)

- **From a Shell only**: `EmuV0 <program> <arguments>`. With no argument it
  prints "EmuV0 needs at least one argument" (measured, EV2). `main` needs
  `argc > 1`, so there is no Workbench start. A Workbench icon would need a
  launcher that runs a Shell command, and that launcher would be a file the
  package owns: removed, upgraded and recovered with the rest.
- **The return code is always 0**, for a missing program as well (measured:
  "Program '...' not found.", rc 0). A zero from the emulator means only
  that the emulator was called. Whether the program worked needs a test of
  its own.
- **An ABIv0 binary run directly** fails with "file is not executable"
  (measured).
- **Arguments** are the raw text after the program's name. They are copied
  into the buffer of the process's `Input()`, as is `REFRESH` for the
  `AddDataTypes` the emulator always runs first.
- **`PROGDIR:` and the current directory** are set to the program's drawer.
  A relative file name given as an argument is resolved there, not where the
  command was typed.
- **Stack:** the emulated program runs on a fixed 64 KB stack in 31-bit
  memory.
- **Libraries:** the program sees what is in `LIBSV0:`, never the 64-bit
  `LIBS:`. A 64-bit library being present says nothing about an emulated
  program's requirement. A 32-bit one being present in `LIBSV0:` does not
  prove the emulator can load it either. Until that can be decided, the
  verdict for such a requirement is "undetermined", not "satisfied".
- **Not supported** (README): network, 3D, AppIcons, ARexx ports.

## The matrix (EV2, EV3, M-*)

Each case ran from a fresh boot, because a failing case leaves the machine
unable to take commands. The system's `C:EmuV0` (1.10) hashes to
`46d4d1d847e87d17c31efeb0374974a0fb93aebb1102a286d9f8518bc85e8446`; the 1.12
`EmuV0-pc` to `e57c4c86f23f3fcae386c3184ffcd7d1bf5f7c412ba12e3128c409582b9fe609`.
The 27 libraries in `LIBSV0:`, with sizes, are in `runs/libsv0.txt`. They
were copied off the machine in a later run that did not start the emulator.
The 1.12 cases used a faithful installation in RAM:
`SYS:Tools/EmuV0` copied, the 1.12 archive laid over the copy, and `EmuV0:`
and `LIBSV0:` assigned to it (`matrix/setup112.script`). The disk was not
changed. The staged programs and the 1.12 binary are hashed in
`runs/matrix-stage.sha256`.

| program | EmuV0 1.10 (`C:EmuV0`) | EmuV0 1.12 (`EmuV0-pc`) |
|---|---|---|
| asciitable, a Shell program that only prints (the control) | Software Failure | Software Failure |
| Sudoku (MUI) | Software Failure | Software Failure |
| ResCode (Intuition) | Software Failure | Software Failure |

In every case the requester's text was blank. The Shell then took no more
input: the command typed afterwards never appeared (`runs/M-*-after.png`).
The result files the cases wrote before starting the emulator never reached
the results disk either, which fits a machine that stopped before the FAT
handler flushed. The serial log was empty. Earlier, EV2 started ResCode with
`Run >NIL:` and EV3 gave it a console window of its own; both ended the same
way (`runs/EV2-rescode-failure.png`, `runs/EV3-rescode-failure.png`).

## What this does and does not show

It shows that on this configuration EmuV0 cannot be used, for any program
tried, including one that uses only `dos.library`. It does not show why. The
common part of every case is the emulator's own start, which runs
`AddDataTypes` before the program. The candidates for the cause are that
start on this emulated CPU, something in this installation, or both
versions sharing one defect. Not tried: another QEMU CPU model or KVM, the
`v11-3` base (ABIv11 2026.09), and real hardware.

## What it means for arospkg (independent of the cause)

- **A package's target and the way it runs are two facts.** An i386/ABIv0
  package stays i386/ABIv0 when EmuV0 runs it, and is never shown as
  native. `pkg_runs_on` and `apkg show` keep these apart; today an ABIv0
  entry reads "undetermined".
- **961 i386 archives on AROS Archives are candidates, not programs that
  run.** The `i386-aros` tag says nothing about the ABI; each package needs
  its ABI established and a run of its own.
- **Variants must be chosen explicitly**: native preferred, emulated shown as
  such, the installed variant recorded, and no upgrade that crosses between
  them on its own. `apkg show` already says which variant it describes when
  the index has two (`rescode` in the `show` test).
- **Requirements are checked where the program runs**: `LIBSV0:` for ABIv0,
  and "undetermined" whenever that cannot be settled.

Chosen for a pilot if the emulator starts working: Abacus (MUI, saves games
into `Gamesaves/` in its own drawer, so removal must keep them), Sudoku
(MUI), and ResCode for the choice between variants.
