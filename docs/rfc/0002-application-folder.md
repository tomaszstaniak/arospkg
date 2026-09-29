---
title: "RFC 0002: application folder format"
status: draft
schema: 0
created: 2026-09-27
updated: 2026-09-29
---

# RFC 0002: application folder format

| | |
|---|---|
| **Status** | Draft. **Not implemented**: nothing reads it, and no package ships one. |
| **Schema version** | `0`: draft, may change incompatibly. |
| **Companion** | [RFC 0001](0001-package-format.md), the package format |

Statuses are used as in RFC 0001: **proposed**, **deferred**, and **shared**,
which means the field is defined in RFC 0001 and used here with that exact
definition.

## 1. Goal

Make an installed application recognisable and startable as one thing, the
way macOS bundles and RISC OS application directories are, while it stays an
ordinary drawer that every AmigaOS and AROS file manager can open, browse and
start programs from, with no arospkg involved.

The package format answers *how does this get onto the disk*. This format
answers *what is this drawer, and how is it started*. It travels with the
drawer, so it also works for an application that was unpacked by hand.

## 2. Scope

In scope: how a reader finds the description, the application's identity and
targets, one entry point, and how a reader validates the description and
falls back when it is missing or wrong.

Out of scope for schema 0: document types, actions, presentation data,
optional features, per-target executables and data roles (section 7);
installation, which belongs to RFC 0001; library lookup or isolation.

## 3. The folder

```
Micropolis/                  an ordinary drawer; any name
├── application.toml         this description (proposed fixed name)
├── Micropolis               the program
├── Micropolis.info          its icon: tool types and stack stay here
└── …
Micropolis.info              the drawer icon, beside the drawer as usual
```

- A reader looks for `application.toml` directly inside the drawer: UTF-8
  TOML 1.0, at most 64 KiB (proposed).
- The drawer can have any name and can be moved. Paths in the description
  are relative to the drawer.
- Icons are unchanged. The program icon keeps its tool types and stack size,
  and the description does not copy them.
- A file manager that knows nothing of this format still shows a drawer, and
  the program inside still starts from its icon.

## 4. A complete example

Illustrative: written for the Micropolis drawer, which does not ship one.

<!-- proposal -->
```toml
schema   = 0
id       = "micropolis"
name     = "Micropolis"
version  = "0.1.0-rc3"
revision = 1

[[targets]]
os   = "aros"
arch = "x86_64"
abi  = "v11"

[launch]
executable        = "Micropolis"
mode              = "workbench"
working_directory = "."
```

## 5. Fields

| field | type | required | status | definition |
|---|---|---|---|---|
| `schema` | integer | yes | proposed | `0`. An unsupported value means the reader ignores the file (section 6). |
| `id`, `name`, `version`, `revision` | | as in RFC 0001 | shared | RFC 0001 section 5. |
| `[[targets]]` | | yes | shared | RFC 0001 section 5: what the program in this drawer runs on. |
| `[[requires_system]]` | | no | shared | RFC 0001 section 5: what the machine must provide to start it. |
| `[launch]` | table | yes | proposed | The one entry point. |
| `launch.executable` | string | yes | proposed | Path of the program, relative to the drawer. It must be a regular file. |
| `launch.mode` | string | yes | proposed | `"workbench"`: started as from its icon, using that icon's settings. `"shell"`: started as a Shell command. |
| `launch.working_directory` | string | no | proposed | Relative to the drawer. Default `"."`. The initial current directory. It does not change `PROGDIR:`. |

## 6. Reader behaviour

1. **Reading never runs code.** A reader parses the file and checks paths.
   It evaluates nothing as a command, and must not block the file manager
   while doing so.
2. **Validation.** Invalid TOML, a duplicate key, a missing required field, a
   wrong type or an unsupported `schema` make the description unusable. A
   path is invalid if it is absolute, contains `:`, `\`, a `..` component or
   a control character, or leads outside the drawer once links are
   resolved. The executable must exist, and `working_directory` must be a
   directory.
3. **Fallback.** Whatever is wrong with the description, the drawer still
   opens as a drawer. An attempt to launch through the description says what
   is wrong.
4. **Offer, then launch.** For a valid description, a reader may offer
   *Launch* (and may make it the double-click action) and must also offer
   *Show contents*, which opens the drawer as usual.
5. **Checks before launch.** A reader checks the executable, the targets and
   `requires_system` again. It refuses a known mismatch or a missing
   requirement. An undetermined result is reported as undetermined, never as
   satisfied.
6. **Launch as declared.** A reader that cannot launch in the declared
   `mode` says so, and must not switch to the other mode on its own. It
   installs nothing and changes no system settings.
7. **Nothing central is needed.** The description travels with the drawer.
   Launching needs no arospkg database, and losing arospkg's records must not
   stop the application from starting.

## 7. Deferred

Named for later revisions, not defined by schema 0. Each will be added to
this RFC as a proposal, with its syntax, before anything implements it.

- **Document types**: file types the application opens, and in what role
  (viewer, editor), so that *Open with…* can offer it. This complements a
  document icon's default tool and never overwrites it.
- **Actions**: named entry points such as *New document* or *Open terminal
  here*, with inputs passed as Workbench arguments or as a working directory,
  never assembled into a command line.
- **Presentation**: description, screenshots and homepage, for a storefront
  and for file-manager details. This must not become a second copy of
  `summary` and `category`.
- **Optional features**: requirements needed for one feature only, so their
  absence is explained instead of blocking the start.
- **Per-target executables**: one drawer holding builds for several targets.
- **Settings and data**: which paths hold configuration, caches and
  documents, for backup and reset. Naming a path does not claim it.

## 8. Relationship to the package format

A drawer installed from a package has two descriptions at different times:

| | package manifest (RFC 0001) | `application.toml` |
|---|---|---|
| where | `.arospkg/manifest.toml`, outside `subdir` | inside the drawer, so inside `subdir` |
| exists after installation | **no**, since it is not installed | **yes** |
| authority for | installation: `subdir`, `icon`, source, licence | launching: `[launch]` |

The shared fields (`id`, `name`, `version`, `revision`, `targets`,
`requires_system`) appear in both. Proposed rule: **they must be equal.** A
generator or packaging tool that finds both compares them and refuses the
package if they differ, as RFC 0001 section 8 does for the index manifest.
After installation `application.toml` is the only copy on the disk, and a
reader uses it as it stands.

Moving or renaming an installed drawer keeps it launchable. It does not
update the package manager's records, which then no longer match (Open:
A4).

## 9. Open decisions

- **A1. One source for the shared fields.** Keep them in both files and
  compare them (section 8)? Generate `application.toml` from the package
  manifest when the archive is built? Or make the package manifest point to
  it? Whatever is chosen, it must work when `subdir` leaves `.arospkg/`
  behind.
- **A2. Workbench launch.** How to start as Workbench does, using the icon's
  settings. What happens when the program icon is missing. Which wins when
  the icon's settings and the description disagree. Shell input and output
  for `mode = "shell"`.
- **A3. Reader limits.** Maximum size, the target vocabulary (shared with RFC
  0001 O7), unknown keys, caching, and checking paths again at launch.
- **A4. Moving and renaming.** Whether a move that understands bundles
  should take the drawer icon beside it along too. How a package manager
  takes back an installation that was moved.
- **A5. The file name.** `application.toml` is proposed and not yet settled.
