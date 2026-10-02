# Joint acceptance: responsive apkg from Limpet

Use the existing VM pool instructions. Do not provision a machine for this
test. Primary target: AROS One x86_64/ABIv11, Limpet inside AROS XTerm with
operation 13. Do not replace the installed release: deploy the development
binary under a distinct name and choose a fresh test root.

Binary for this test (2026-10-02, with XTY_COLOR_PENS): built from commit
`57018a8` with `OUT=local/pens/apkg sh src/build.sh`, SHA-256
`6f9ee12c3811f43bba4cf13e7583a60aef042f153bcc26eb4b1b3a2064e37196`. It
supersedes the binary in `../reports/2026-10-01-responsive-presentation/README.md`.
Accept it together with the named terminal build that reports the pens
interpretation; a result with another terminal build does not carry over.
Rebuild with `OUT=<separate-file> sh src/build.sh` if the local artifact is gone;
record the new hash and do not reuse another binary's acceptance.

## User-visible acceptance

1. Update an empty root from the public index. Record the index hash as well
   as the binary, terminal and Limpet versions.
2. `show micropolis` at 80 and 40 columns: state/location before technical
   download fields; complete readable requirements, URL and hash; no lost text.
3. `search` at 120/80/40/24 columns: wide table versus narrow blocks; installed
   state, full version including revision, description and target visible.
4. On an empty cache, install a sufficiently large package (e.g. Micropolis)
   with `--slow 200`. Resize from 80 to 40 and back while downloading. Capture
   intermediate byte counts, not just the final `installed` line. Percentage
   must remain readable; no stale suffix after a shorter frame. `--slow` also
   delays extraction by 200 ms, not 200 ticks.
5. Repeat one download with cancellation. Result and next prompt must start
   on a clean line. Do not infer cancellation support from a new keyboard
   shortcut: use the application's existing mechanism/test option.
6. Inspect a failed command with a long subject/detail at 40 columns. Errors
   retain their text without colour. Check a light and a dark terminal theme.
7. Repeat `show`/`search` with `--plain`, `--color=never`, and JSON where supported.
   Redirected auto/plain output must match; compare redirected output with the
   earlier baseline after normalizing only the deliberately different root.
8. Colours with the pens interpretation: the verdict words green, yellow and
   red on a light and a dark theme, and the Limpet prompt after each command
   (success, failure, cancel) in its usual colours. First record that the
   terminal's reply on apkg's own output has KNOWN_INTERPRETATION and
   COLOR_PENS set, and the terminal build's hash. Do item 4 (live bar,
   resize) with the same install, not as a separate run. The variant without
   bold (SGR 39 ends the colour) needs a reply without BOLD_RESET; if the
   terminal does not give one in a normal session, it stays covered by the
   host test `test_colour_without_bold_resets_with_39` only, and the report
   says so.
9. Check ordinary CON: and an unsupported presentation query. A successful
   capability reply on the parent terminal does not authorize decoration of
   a redirected output handle.

Do not mark live resize accepted from host renderer tests. The renderer
refreshes geometry on callbacks (at most once per wall-clock second), not via
an asynchronous resize subscription. Existing library fetching/redirect/cache
messages are unchanged and may wrap naturally at the terminal boundary.

Mainline native/PTY remain the terminal agent's additional acceptance matrix.
No full TLS regression is requested solely for the presentation changes.
