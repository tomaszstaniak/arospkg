# Responsive apkg presentation: host checks and ABIv11 build

Date: 2026-10-01. Development work on `presentation`, based on `4eb782a`.
This is not release acceptance and does not change the frozen TLS candidate.

## Delivered

- Rich `show`: installation state/location before compatibility, requirements,
  actions and download/provenance; full values wrap at the reported width.
- Rich `search`: table at 80+ columns, blocks below it or for oversized names
  and versions; descriptions remain complete, installed state is explicit,
  CPU/ABI and mismatch reasons remain visible.
- Download: bar, percentage and byte counts at wider widths; stage/percentage
  at narrow widths. Geometry is refreshed at most once per wall-clock second
  when a progress callback arrives. Previously printed command output does
  not reflow; this is not a full-screen UI.
- Actual verification-complete event prints `Archive verified`. Extraction
  and publication show stage names, not fictitious fractions. Unknown-length
  transfers use complete lines to avoid colliding with engine messages.
- Errors use wrapped fields in rich mode. Plain/redirection/JSON continue on
  their existing formatting paths.
- Shared `--slow` conversion rounds milliseconds up to 50 Hz ticks for both
  transfer and extraction; 5000 ms is 250 ticks, not 5000 ticks.

## Verification performed

`python3 tests/test_presentation.py`: **10 tests passed**. The C driver runs
production `present.c`; test substitutes cover only DOS handle/capability
queries and time. Tests include 12/24/40/80/120 columns, long identifiers and
paths, preserving complete text, 80-to-24 resize, plain/JSON/redirected silence,
unknown capabilities, large counters, cleanup, unknown-length transfer followed
by an engine message, and millisecond conversion boundaries.

Red tests preceded fixes: lost percentage at 24 columns; widths overflowing;
old millisecond conversion; and unknown-length output colliding with the next
engine message. Review also caught an invented verification-start event in the
test; the fixture now uses the real completion event (1/1).

`bash tests/run-host-tests.sh`: **ALL HOST TESTS PASS**, including the renderer.
`git diff --check`: passed.

`OUT=/private/tmp/apkg-presentation-v11 sh src/build.sh`: exit 0. Default
x86_64 ABIv11 toolchain/SDK at `~/Work/AROS/toolchain` and `~/Work/AROS/sdk`;
Mbed TLS with jitterentropy, existing per-target static-library cache.
The build emits warnings in libpkg; no warning was reported in `src/pkg/`.

Built artifact: `/private/tmp/apkg-presentation-v11`

SHA-256: `0b4aeb1d0cfa84bf763529151a481a6e4b39b97f70665150bc2c1c73359d97d9`

## What these checks do NOT show

This new binary has not run on AROS. Earlier CON: and joint XTerm reports
cover the earlier binary, not this layout. Tests here do not cover the whole
`show_rich`/`search_table` integration, terminal font/colour rendering, DOS IPC,
Limpet, or the live timing of resize/cancellation. Wrapping counts bytes;
Unicode display-cell widths are not implemented. The synchronous capability
query cannot promise a deadline against an unresponsive handler.

No guest was acquired, no terminal product code changed, and nothing was
published. Follow the joint acceptance brief before declaring the UI accepted.
