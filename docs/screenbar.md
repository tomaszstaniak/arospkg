# PkgUpdates: an update counter for ScreenBar

PkgUpdates puts an item on ScreenBar's strip that says how many installed
packages have updates, and opens PkgManager on them. It is a demonstration
of ScreenBar's executable-provider SDK, which is experimental (protocol 1,
revision 0.x of ScreenBar's module contract): both may change.

arospkg and PkgManager do not need it, and nothing installs it: start it
yourself, and it stops with Ctrl-C (or `Break`).

```text
Run >NIL: PkgUpdates
Run >NIL: PkgUpdates --root Work:Packages --pkgmanager Work:arospkg/PkgManager
```

| Option | Meaning |
|---|---|
| `--root <dir>` | the package root, as for apkg (default `SYS:Packages`) |
| `--pkgmanager <path>` | where PkgManager is (default: found on the command path) |
| `--recheck <minutes>` | recount at least this often (default 15, minimum 1) |
| `--label <text>` | the item's label (default `Updates`) |
| `--log <file>` | write what it does to a file |

## What it shows

The text comes from the same model as `apkg outdated` and PkgManager's
Updates list: `2 available`, `3 available (1 blocked)`, `1 blocked`,
`up to date`, `none installed`, `no catalogue`, `catalogue unreadable`, with
`(old catalogue)` after a catalogue more than two weeks old. It never shows
a bare 0 for a catalogue it does not have.

It downloads nothing. It recounts when the catalogue or the record of
installed packages changes (after `apkg update`, an install, an upgrade, a
rollback or a removal, by apkg or PkgManager; checked every ten seconds),
and otherwise every `--recheck` minutes. It reads without the package
root's lock, so it never waits for, or blocks, apkg.

The item's one action, **Open PkgManager**, asks a running PkgManager
(through its ARexx port) to come forward on the Updates list; with no
PkgManager running it starts `PkgManager --updates` with the same root.

If ScreenBar is not running, or is restarted, PkgUpdates keeps going and
registers again within about five seconds.

## Building

ScreenBar's SDK is not copied into arospkg. The build takes its header and
`provider.c` from a ScreenBar checkout, by default the public one beside
this repository:

```text
sh src/build-screenbar.sh                         # x86_64 ABIv11
TARGET=raspi sh src/build-screenbar.sh            # native Raspberry Pi
SCREENBAR_DIR=/path/to/aros-screenbar sh src/build-screenbar.sh
```

Build PkgUpdates for the same CPU and ABI as the ScreenBar it talks to.

## Limits

- Protocol 1 offers a label, a text and one action; no icon.
- What was tested, and on which system, is in
  `docs/evidence/2026-10-11-updates/README.md`.
