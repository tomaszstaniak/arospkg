---
title: "Richer output for show, search and install"
status: design, waiting for the terminal's query (operation 13)
created: 2026-10-01
---

# Richer output for show, search and install

The terminal side and the shared contract are in
`aros-xterm/docs/apkg-integration.md` (operation 13, `XTY_GET_PRESENTATION`,
proposed and not yet implemented). This page is what apkg prints. Nothing
here is implemented, and no apkg code depends on operation 13 until the
terminal ships it. The new options are features, so this comes after 0.3.1.

## Three modes, chosen once per command

| mode | when | what changes |
|---|---|---|
| **as today** | `--json`; or output not interactive (`>file`, a pipe) | nothing: byte for byte the current output, which the test suites grade |
| **plain interactive** | interactive output that does not answer the query (CON:, AUX:), or `--plain` | the current text; `install` adds a progress line every 10% (`--plain`: none) |
| **rich** | interactive output whose terminal answers operation 13 | bold headings, coloured verdicts, width from the terminal, one updating progress line |

The query is made on the handle `Output()` returns, once, before anything is
printed; a failure of any kind means plain. `--color=never` removes colour
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

Rich: the same lines, in the same order, with

- the first line (`isomaker 1.0`) in bold;
- the verdict word in colour: `native` and `satisfied` green; `undetermined`
  yellow; `incompatible` and `missing` red; `installed: yes` green;
- long values wrapped at the terminal's width, continuation lines indented
  under the value instead of breaking the column.

## search

As today (and plain), unchanged: four columns of fixed width (id 20,
version 10, arch 9, summary to the end of the line), as in
`docs/reports/2026-09-29-final-index/results/fi-search.txt`.

Rich:

- a bold header line `package  version  target  summary`;
- the id column as wide as the longest id in the result, not a fixed 20;
- the summary cut at the terminal's width with `...`, so each package stays
  on one line; with a width below 40 columns, two lines per package (id and
  version, then the summary indented);
- the tags that exist today (`[other CPU]`, `[unknown ABI]`, `[other ABI]`)
  in yellow, still as words.

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
  soliton  downloading  [#########...........]  45%  98 KB of 219 KB
```

then replaced by `verified and cached ...` and `installed soliton` in
green, or by the failure text in red with its word (`failed: ...`).
Extraction gets the same line (`extracting 12 of 30 files`). On Ctrl-C the
line is ended before the `cancelled` message, so the next output starts on
a clean line.

## Acceptance (from the shared contract)

On native v11, mainline, the PTY preview, CON:, redirected output, `--json`,
a narrow window and a terminal that refuses the query: each command in each
mode. A redirected run must be byte-identical to today's, and contain no
escape sequence. The existing suites must pass unchanged.
