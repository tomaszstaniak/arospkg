# Combined presentation acceptance on AROS One

2026-10-02. User-visible acceptance of the existing combined build; no
rebuild and no repeat of the completed rc2 TLS regression.

| Artifact | SHA-256 |
| --- | --- |
| apkg, `local/pres-rc2/apkg` | `b170ca11463f1abd79b5268a59a15d607c0e5b1fe21081033a1d82e89ccf6e4c` |
| Limpet rc5, `dist/rc5/stage-abiv11/Limpet/Limpet` | `11fc46670995ea05d42837972fcc3b0a5c05c1be6bc0e5115ab7441406287f92` |
| Public index downloaded in this run | `92297eda9eac315d732b87e52a1499f5a3bf231b6d038378a693a87fdc81f94e` |
| Two-target yellow-verdict fixture | `ae017307c673412a38af228be3644388c146bda98c32d8a2805df657a7075cbc` |

Machine: pool v11-1, AROS One 1.3, QEMU TCG, 1024x768, Cocoa display.
The old stopped reservation was released at the user's explicit request,
under the pool locks after checking processes and sockets. All binaries,
installed packages and cache for this run were under RAM:Joint/JointRoot.
Existing system installations were not changed. Results use the unique
`joint-20261002-` prefix on RESULTS:. The machine was stopped, results
collected with the pool's hash verification, then released.

## Observed results

- The guest's `--version` reports the exact apkg hash above and Mbed TLS
  3.6.7. Public index update succeeds in ordinary CON:.
- Limpet's probe reports known=31, colors=3, styles=1, lines=3,
  rows=24, columns=80, interpretation=1. Green compatibility/requirements,
  yellow other-CPU verdict and red errors/cancellation are readable in
  dark and light themes. The next prompt retains its theme colours.
- `show micropolis` wraps the requirements, URL and full SHA at the narrow
  width. `search` switches from the wide table to blocks with version,
  installed state, description and target as the window narrows. The
  screenshots include the wide, intermediate and smallest tested windows;
  only the initial 80-column geometry was separately probed.
- `install xrick --slow 50` from Archives succeeds. During that same
  download the window narrows and widens: the narrow frame shows 6%,
  120 KiB / 1995 KiB without a bar; the widened frame shows the bar,
  36%, 720 KiB / 1995 KiB. The final green installed line and prompt
  occupy clean lines. This is actual guest resize, not a renderer mock.
- `--cancel-at download-mid install gmore` cancels on both themes, with
  the cancellation reason and clean next prompt. The later normal gmore
  install succeeds on the light theme. No cache-hit install was used as
  evidence of network progress.
- Interactive `--plain` and `--color=never` work. Redirected show and
  search in auto/plain modes are byte-identical (`cmp`, both exit 0).
  JSON output parses on the host. Captured outputs are in `evidence/`.
- The host presentation suite passed again: 16 tests, including the
  no-bold SGR 39 case, unknown interpretation and suppression modes.

## Limits and incidents

- There is no packet trace of apkg's own operation-13 response. The probe
  is a separate process in the same window; apkg's interpretation is
  evidenced indirectly by its output and the tested selection code.
  Do not describe the probe as direct instrumentation of apkg.
- No-bold is host-test coverage only. Mainline/PTY are not covered by
  this ABIv11 acceptance. Live resize was exercised on the dark theme;
  the light-theme install/cancel/colour checks did not repeat live resize.
- Slowed Micropolis from GitHub ended in a connection reset. apkg now
  correctly reports the transport error and body byte count, not wrong
  size. The successful resize case therefore used xrick from Archives.
  No claim is made about which network peer originated that reset.
- Initial mouse calibration was taken during changing download output;
  its affine mapping was wrong and the attempted drags did not resize.
  The cache was preserved, recalibrated on a static screen, and the real
  resize then succeeded. The unsuccessful drags are not acceptance evidence.
- The first light-theme launch had arguments in the wrong order and
  printed usage in CON:. It was repeated as `Limpet --theme FILE --shell`;
  only the actual light-window captures are counted.
- Existing printed lines are clipped, not reflowed, when the window
  shrinks. Newly printed apkg output uses the new width.

## Conclusion

The core combined presentation workflow is accepted on this AROS One
pair: show/search/install, visible live progress and resize, both themes,
error/cancel and next prompt, with plain/redirected output preserved.
The limits above are not a claim of universal terminal coverage.

Presentation follow-up, not a release blocker: hide the long signed
redirect URL behind a verbose option. It currently dominates the window
before progress begins. No product code was changed in this acceptance.
