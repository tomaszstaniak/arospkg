# arospkg 0.5.0: tests, 2026-10-12

All on QEMU (TCG) on the shared pool; **nothing ran on real hardware**.
Every result file names its step; `.png` files are screenshots of the
guest at that moment.

| directory | system, slot | binaries | what |
|---|---|---|---|
| `proto/` | AROS One 1.3, v11-2 | tests/launch-proto/wbrun.c | the prototype: OpenWorkbenchObjectA starts Folio from a 40960-byte Shell with the icon's 8 MiB stack, opening a PDF passed as a Workbench argument, from a drawer path with a space. The first count of "tasks named Folio" (38) was a bug in the probe, not in the system |
| `run1b/` | AROS One 1.3, v11-2 | apkg of the working tree (bb5a54aa...) | `apkg run` suite (tests/run1.script): Folio and GrafX2 (Workbench, icon stack), grep (Shell, return codes 0 and 5), a tool with spaces in its paths and arguments with `"` and `*`, a Workbench tool with a tool type and a document; refusals: no description, missing program, `..`, description for i386, i386 program file, Workbench without icon, unknown key, not installed; a package installed without a launch table |
| `gui1/`, `gui2/` | AROS One 1.3, v11-2 | PkgManager of the working tree | Run, Verify, Installed, Demo store, Buy; upgrade with preview (a user-changed README kept), rollback, Verify showing the changes, Remove keeping them, Run of a Shell program, ARexx RUN. `gui1/07` shows a bug found here (an offer showing Folio's picture), fixed later |
| `final-v11-2/` | AROS One 1.3, v11-2 | apkg e7432440... (the release binary) | the run suite again (`f1-*`) and the 0.4.2 updates suite (`upd-f050-*`: 21 steps rc 0, 4 rc 10, 1 rc 5, 4 of 4 file hashes as expected, as in 0.4.2); Ctrl-C during connect (`ctrlc-connect-update.png`, "cancelled"), during a package download (cache empty) and during the first package of `upgrade --all` (finished, the second not started, rc 10) |
| `limpet/` | ABIv11 2026.09, v11-3 | apkg e7432440..., Limpet 0.3.0 | CON: against Limpet: colours and wrapping, redirection, JSON, Ctrl-C (one prompt), `apkg run grep`, Ctrl-C into it. See its README |
| `i386/` | AROS i386 ABIv0, v0-2 | apkg of the working tree (source = release) | network install of DirTree from AROS Archives with a launch table naming `C/DirTree`, `apkg run` in Shell and Workbench mode |
| `mainline-v1-2/` | mainline v1-20260923, v1-2 | apkg and PkgManager for mainline | PkgManager starts and lists; `apkg run` of a v1 tool and refusal of a v11 one; **no network on this slot**, so no install over the network (see below) |

The gallery (docs/screenshots/0.5.0/) is from the final archive
(PkgManager 28cc388e...).

## What these tests did not show

- **Mainline network install.** v1-2 has no network configured (AROSTCP
  could not be opened); the only mainline slot with a network, v1-1, was
  held by another session for the whole run. The controlled catalogue
  (one mainline package, arospkg 0.4.2 for v1, from its GitHub release) is
  in `mainline-v1-2/`; the install stopped at "no network". Mainline
  network installs were last shown with 0.4.2 (`apkg update`).
- **Raspberry Pi**: the Pi binaries were compiled and linked only; the
  pool's Pi slot has no working network or input medium.
- PkgManager on mainline beyond starting and listing; PkgManager on i386
  (none is built).
- Workbench starts on mainline (only a Shell tool was started there).
- A physical keyboard and mouse: all input was injected over QMP.
