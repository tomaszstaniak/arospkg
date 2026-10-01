---
title: "Richer output for show, search and install"
status: implemented on presentation; revised layout awaits joint guest acceptance
created: 2026-10-01
---

# Richer output for show, search and install

The terminal side and the shared contract are in
`aros-xterm/docs/apkg-integration.md` (operation 13, `XTY_GET_PRESENTATION`).
Both sides implement the query. The initial joint guest run is recorded in
`aros-xterm/docs/apkg-joint-20261001.md`; it predates the responsive layout
below. The primary user journey is Limpet launching apkg inside AROS XTerm.
There is no check for a shell name. These changes are on `presentation`,
not part of the frozen TLS release candidate.

## Three modes, chosen once per command

| mode | when | what changes |
|---|---|---|
| **as today** | `--json`; or output not interactive (`>file`, a pipe) | nothing: byte for byte the current output, which the test suites grade |
| **plain interactive** | interactive output that does not answer the query (CON:, AUX:), or `--plain` | the current text; `install` adds a progress line every 10% (`--plain`: none) |
| **rich** | interactive output whose terminal answers operation 13 | bold headings, coloured verdicts, width from the terminal, one updating progress line |

The query is made on the handle `Output()` returns before anything is
printed; a failure means plain. During in-place progress geometry is refreshed
at most once per wall-clock second, on the next callback. This is not a resize
subscription: without callbacks there is no redraw. The synchronous DOS query
has no timeout guarantee if its handler hangs. `--color=never` removes colour
but keeps bold and the progress line; `--plain` removes all of it.
`--color=always` adds colour even on a plain interactive handle, never on a
non-interactive one (no escape sequence ever goes into a file).

Meaning never depends on colour: every verdict keeps its word (`native`,
`satisfied`, `missing`, `failed`).

## show

As today (and plain), unchanged:

```
isomaker 1.0
  Create an ISO CD from DOS
  category:     utility
  target:       x86_64, ABI v11
  compatibility: native -- built for this machine: x86_64, ABIv11, current distributions such as AROS One
  download:     44 KB from https://archives.arosworld.org/share/utility/isomaker.x86_64-aros-v11.zip
  sha256:       c9b16161ed6ccc737deac09004be2112dd635cfe2bdf841fd3cf80cda6aed47d
  requirements: satisfied, probed now:
    crt.library: satisfied (already open, version 5)
    stdlib.library: satisfied (already open, version 3)
  installed:    no
```

Rich: package/version and description first, then installation state and
location. Separate sections show compatibility and requirements, installed
package actions, and finally download/provenance. A compatible target is not
a promise that the application starts. Verdicts remain textual without colour.
Values, paths and identifiers wrap without losing content. Wrapping currently
counts bytes (the current catalogue is ASCII), not Unicode display cells.
Previously printed text is not reflowed on resize.

## search

As today (and plain), unchanged: four columns of fixed width (id 20,
version 10, arch 9, summary to the end of the line), as in
`docs/reports/2026-09-29-final-index/results/fi-search.txt`.

Rich:

- a package count, then package/version/state/description columns at 80 or
  more columns; descriptions wrap into the remaining width;
- below 80 columns, or for identifiers/versions too long for the table,
  package blocks with full version, textual installation state and description;
- no discarded description text; unknown width uses readable blocks;
- CPU/ABI on a separate line, with mismatch reasons in yellow when relevant;
- a final hint pointing to `apkg show`.

## install

As today, redirected: unchanged (`fetching`, `redirect ->`, `verified and
cached`, `installed`).

Plain interactive, adds between `fetching` and `verified`:

```
  download 10% (22 KB of 219 KB)
  download 20% (44 KB of 219 KB)
  ...
```

Rich: one line, rewritten in place with CR and erase-to-end-of-line, never
wider than the terminal:

```
Downloading [#########-----------]  45%  98 KiB / 219 KiB
```

The command first names the package and intended destination. At smaller
widths the bar disappears, then byte counters, retaining the download stage
and percentage where they fit. The bar never wraps into the next line.
Unknown-length downloads use complete byte-count lines, initially and at
each new MiB, because the transport has no final progress event in that case.

The existing verify callback reports completion only: `Archive verified`.
Extraction/publication show stage names, not invented percentages. The engine
does not emit per-file extraction progress. Completion clears the live line
before the ordinary result; rich errors wrap too. Existing engine messages
(fetching, redirects and cache verification) remain unchanged.

`--slow` uses the same milliseconds-to-50-Hz-ticks conversion for transfer and
extraction, rounded up. This is a testing aid, not a progress timer.

## Acceptance (from the shared contract)

On native v11, mainline, the PTY preview, CON:, redirected output, `--json`,
a narrow window and a terminal that refuses the query: each command in each
mode. A redirected run must be byte-identical to today's, and contain no
escape sequence. The existing suites must pass unchanged.

Host renderer checks: `python3 tests/test_presentation.py`, also included in
`tests/run-host-tests.sh`. They execute production `present.c`, substituting
only DOS handle/capability calls and time. Widths 12/24/40/80/120, a live
80-to-24 resize, plain/JSON/redirected silence, unknown capabilities, large
byte counters and delay conversion are exercised. They do not prove Limpet,
terminal rendering or guest scheduling; the new guest acceptance is pending.
