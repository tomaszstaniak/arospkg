# Platform change review for arospkg (CLI, Zune GUI, Exec-port widget)

Review date: 2026-10-11. Scope: upstream changes since 2026-09-20 (AGENTS.md section 10).
Method: read-only `gh api` listings of commit subjects, plus message/file lists of selected commits.
Nothing was built, mounted, run or modified. Findings below are "upstream commit exists";
presence in our SDKs/VMs is judged only from dates, not from inspecting those systems.

## Sources checked

| source | revision at review | notes |
|---|---|---|
| mainline `aros-development-team/AROS` master | pushed 2026-10-10T21:23Z; newest seen `a7102948cd` (2026-10-10) | 548 commits since 2026-09-20 listed by subject; selected ones read in full. No GitHub releases published. |
| ABIv11 `deadwood2/AROS` master | pushed 2026-10-04; 30 commits ahead of tag `ABIv11_2026.09` (`fbc242c104`, 2026-09-04; release published 2026-09-14) | 11 commits since 2026-09-20 listed. |
| local `aros-v11-src` | `e33ca77d6c` (2026-09-15), origin deadwood2/AROS | stale vs remote master; not used for conclusions. |
| local mainline `/Volumes/arosmain/AROS` | HEAD `74a2d80b7f` (2026-09-04) | stale (predates 20 Sep); not used, not modified. |

Not checked: mainline nightly download site and its changelog; Raspberry Pi nightly/release notes
(no release notes reachable via GitHub; only commit history seen); real hardware; our SDK/ISO contents.

## Our systems vs upstream (by date, not verified in the images)

- v1-1/v1-2 (mainline nightly v1-20260923): contain upstream up to about 2026-09-23. Everything
  dated 2026-09-24 or later below is NOT in them (CLAUDE.md already notes `da9bbd01b5` is after
  our ISO).
- v11-3 (tag `ABIv11_2026.09`) and v11-1/2 (AROS One 1.3): deadwood2 commits of 2026-09-25 and
  2026-10-01 are after both; none is in a release yet.
- rpi-1 (native Pi 20260822): everything below is newer.

## Findings, mainline (ABIv1)

1. stdc stdio / redirected stdout. `da9bbd01b5` (2026-09-24) "stdc: __close_stdio() restores only
   the buffering mode bits": the shell's `Output()` no longer loses `FHF_WRITE`. This is the fix
   for the CLAUDE.md "unflushed stdout vanishes" scar. Impact: CLI plain/redirected output on
   mainline is correct only on builds after 2026-09-24; our v1 pool base and the current
   mainline SDK/runtime (if older) still need the `FPuts(h,"")` workaround. Keep the workaround
   until a newer base is staged and a `>file` run is repeated. No later stdc/stdcio change found
   (path listing for `compiler/stdc/stdcio` since 09-20 returned only this class of change via subjects).
2. dos FRead/FWrite. `11b8928bd9`, `ba40ca031a` (both 2026-10-02, dos.library 50.82): large
   FRead/FWrite bypass the file buffer; short direct writes are retried; UnGetC pushback preserved.
   Impact: possible performance/behaviour change in downloads written via FWrite; not verified
   to affect us. Needs one runtime check of a large redirected write after the base update.
3. dos Execute. `03a6bc9b30` (2026-09-29): interactive `Execute` shells stay open after an
   initial command. Impact: only if the GUI starts shells through Execute; none expected.
   `a473d5fc64` (2026-10-01): RunCommand stack clamp to platform default (PROC_STACKSIZE);
   relevant to the "stack 40960" scar only on m68k, no x86_64 effect read.
4. Shell. `c913c27d17` (2026-09-21): Shell gains error-output (stderr) redirection. Impact: scripts
   that redirect stderr may behave differently; our CLI should keep writing errors to Error()
   and not depend on it.
5. console.handler. `e2c97e7c70` (09-28 control-key line editing), `b0f0838f5f` (10-01 history),
   `c7f312f1be` (10-03 filename completion), `b01e5dbafe` (10-06 wrapped input), plus
   console iconify `e736a64e31`. Impact: interactive prompts only. No change to Ctrl-C /
   SIGBREAKF delivery found in subjects or file lists seen.
6. Ctrl-C. No commit found for the mainline Type restart-after-first-Ctrl-C scar. `efda587bce`
   (09-28) touches only Type's output-open failure status. Conclusion: the scar presumably still
   holds; this was not tested.
7. posixc. 2026-10-09 batch (`2019d01939` fd allocation under table lock, `5de5491d25`
   per-task path buffer, `6490ab5288` pread, `2a8b082ab8` realpath) and `dd49869e85`, `703c89a9b9`.
   Impact: thread-safety of fd allocation matters if the GUI downloads in threads; fixes
   exist upstream, absent from v1-20260923. No change found for the 167,552 B per-open retention.
8. AROSTCP/bsdsocket (mainline only). Large 2026-10-06..10-10 series: IP-fragment crash fix on
   64-bit `4803a70fac` (10-06), socket-base/fd race fixes `063b1cb4d0`, `94e6932402`,
   `d22743df60`, `bcb772e974`, `ObtainSocket` unique id, `SOCK_NONBLOCK` and `SO_NOSIGPIPE`
   `f3373c65fa`, IPv6 DNS `4e59976285`, services framework, stack stop via Break `fba7f5e342`
   (10-09), miami `sockatmark` etc. Impact: all newer than v1-20260923; behaviour of our HTTP/TLS
   downloads on v1 should be re-measured on a new base. Nothing seen about TLS/CA bundle in the subjects.
9. e1000 (mainline). No e1000 commit since 09-20 in the subject list (mainline already at 1.2
   per CLAUDE.md; allocation-in-interrupt not fixed there as far as the subjects show; source not re-read).
10. Zune/Intuition/exec. Zune: `fb922a5e53` minimum width preserved across application sleep,
    `38ba006700` frame values unsigned long, `c22f4d0c1c` PrefsWindow, many comment/TODO-only
    commits. Intuition: ShowWindow/HideWindow semantics `1a8cdb58be`, `0e9bd872fd`,
    `bb1d25a1ee` (no activation of hidden windows), `e7e9dd8505` focus on hide. No change to
    List/Cycle/Floattext/Listview in muimaster path (commit listing for the path showed none such;
    only the first 10 subjects were read). NList: comment/TODO only. Exec: `995911ef6b`
    (ReleaseSemaphore alert order), `b46e7c4c9d` affinity, no MsgPort/PutMsg/WaitPort change seen
    in the first 10 subjects of the `rom/exec` path. Impact on the Exec-port widget: none found.
    Window-hide/focus changes could alter behaviour of an iconify/hide feature if added; none today.
11. Raspberry Pi (raspi-aarch64). 2026-09-28..10-09 commits are BCM2712 (Pi 5) display/HDMI
    work, Pi 400/CM4 Bluetooth firmware, clock/mailbox, battclock, I2C, debug console routing,
    `fd3ad20c3a` usb2otg (affects QEMU DWC2 only), `949d6834f8` hub.class split transfers for
    root-hub devices, `9a984adb20` USB ethernet classes exclusive-open flags cleared on last close.
    No GitHub release or release notes found. Impact: the only item that may touch rpi-1 network
    behaviour is USB ethernet; rpi-1 (20260822) predates it and still has no network, so nothing
    changes for the Pi 0.4.0 decision. Not checked: a newer Pi image.

## Findings, ABIv11 (deadwood2)

1. e1000. `2cfd102f91` (2026-10-01) "keep Tx frame buffers with the ring, no AllocMem/FreeMem from
   interrupt context" and `ffa6f25333` (10-02, version bump) = the upstream fix for deadwood2/AROS#274
   noted in CLAUDE.md. Present on deadwood2 master only: not in tag `ABIv11_2026.09`, hence not in v11-1/2/3.
   Impact: heavy transmit (large uploads, many requests) on v11 pool slots can still damage the heap;
   keep network-heavy tests limited and treat unexplained heap crashes as suspect until a base
   with e1000 >= the fix is staged. No release contains it yet (no tag after `ABIv11_2026.09`).
2. Wanderer `7cb1299732`, `22a1ba9645` (2026-09-25): DisplayIOError uses the calling process's own
   application object because message ports are bound to a process. Same principle applies to our
   Zune app/Exec-port widget: create and use the MUI app (and its ports) in one process. No change
   to our code is implied; it confirms the existing rule.
3. Graphics `5b5fd4cfd4` (10-02): makevport workaround for a custom-screen issue; no relevance seen.
4. The dos/stdc/console/Shell/posixc/AROSTCP changes listed above for mainline are NOT among the
   11 deadwood2 commits. No fix for the ABIv11 `snprintf` 16-slot limit, the `fh_Type`
   overwrite, or the NP_HomeDir/SystemTags scar was seen in those 11 subjects. ABIv11 stdout/redirect
   behaviour is therefore unchanged from AROS One 1.3 / 2026.09 as far as this review shows.

## Impact summary for arospkg

- CLI plain/redirected output: mainline fix exists upstream (da9bbd01b5) but is absent from the
  v1 pool base; keep the empty `FPuts` workaround and retest redirects on a base dated after 2026-09-24.
  ABIv11: no change.
- Ctrl-C: no upstream change found on either ABI; keep existing break-handling and do not claim a fix.
- GUI (Zune): no relevant list/cycle/floattext change; window show/hide/focus semantics changed on
  mainline only.
- Network: e1000 heap-damage fix is upstream on ABIv11 master, not in any release; large AROSTCP/
  posixc concurrency fixes landed in mainline in October, not in v1-20260923.
- Exec ports: no change found.
- Rebuild/recheck needed only after a new mainline base is staged (retest redirect, large
  download, TLS) or an ABIv11 release carrying 2cfd102f91 appears.

## Limits

Subjects were scanned with keyword filters, so a relevant commit with an unrelated-looking subject
could be missed. Only selected commits were read in full. Nightly/changelog pages and our
installed images were not inspected. Local trees are stale and unused.
