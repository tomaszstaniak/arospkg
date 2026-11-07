---
title: "arospkg metadata reference"
status: reference
describes: arospkg 0.7.3 and apkg-pack 0.2.0, checked 2026-11-03
audience: index maintainers, application authors, tool writers
---

# arospkg metadata reference

Supported fields for released arospkg 0.7.3. Start with the [publishing guide](authoring.md) for new packages or [archive layout and testing](packaging.md) for existing uploads.

## Three files, three writers

| | **manifest** | **index** | **registry** |
|---|---|---|---|
| file | `manifests/<id>.<arch>.<abi>.toml` in arospkg-index (older ones `<id>.<arch>.toml`), corrections in `overrides/` under the same name | `index-v3.json` (and `index-v2.json`, `index.json` for older clients) in arospkg-index | `<root>/db/installed/<id>.json` |
| format | TOML | JSON, `schema: 1` | JSON, `schema: 1` |
| written by | `apkg-pack submit`, or an index maintainer for an existing archive | `tools/mkindex.py`, never by hand | the client, when it installs or upgrades |
| read by | `mkindex.py` | `apkg`, `PkgManager` | `apkg`, `PkgManager` |
| describes | one package build, as reviewed | every published build | one installation on one machine |

```
manifests ──mkindex.py──▶ index-v3.json ──apkg update──▶ <root>/db/index.json ──install──▶ <root>/db/installed/<id>.json
 (reviewed)               (derived)                    (the machine's copy)                (what happened here)
```

- **Nobody writes the index by hand.** Change the manifest and regenerate.
- **The registry is not something an author writes.** It records what the
  client did on one machine: when, from which URL, which files with which
  hashes, and what the requirement probe found. A package cannot supply
  one, and it is never published.
- The embedded `.arospkg/manifest.toml` is written by `apkg-pack build` and read by `apkg-pack submit`. It describes the archive without its own download URL, size or hash. Submission adds those fields. The client installs from the generated catalogue, not from this embedded file.

## The manifest

All keys are top-level unless shown inside a table. Keys `mkindex.py` does not know are ignored without a warning.

**Every string must be printable ASCII, and must contain neither `"` nor `\`.** The client does not decode JSON escapes. It would act on `1.0 \"beta\"` instead of `1.0 "beta"`, and on `\u00e9` instead of `é`. `mkindex.py` refuses any string it would have to escape. Strings the client acts on (`id`, `version`, `arch`, `abi`, `url`, `subdir`, `icon`, and the `id` and `type` of each requirement) are also refused when they are longer than the client holds. Strings it only shows (`summary`, `category`, `kind`, `license`, `source`) get a note instead, and the client shortens them.

**Required** means that `mkindex.py` rejects the manifest without it. **Index** says which index key the value is copied to. **Client** says what the client does with it.

### Review state

| field | type | required | default | meaning and constraints |
|---|---|---|---|---|
| `status` | string | yes | none | `"approved"` is published (the entry is **Listed**); anything else is rejected. Generated candidates carry `"skeleton"`, which means nobody has looked at them. There is no other status. It says nothing about whether the program was started: see [Listed, Reviewed and runtime tests](#listed-reviewed-and-runtime-tests). |
| `excluded` | string | no | none | A reason. If present, the manifest is rejected as a deliberate exclusion, whatever else it says. |
| `depends_checked` | boolean | yes, and it must be `true` | `false` | Someone has checked what the package needs from other packages. An empty `depends` with `depends_checked = false` means nobody has looked yet. It does not mean the package needs nothing. Not copied to the index. |
| `upload_date`, `selected_by` | string | no | none | Where a generated candidate came from in the catalogue. Not copied to the index. |

### Identity

| field | type | required | default | index | client | meaning and constraints |
|---|---|---|---|---|---|---|
| `id` | string | yes | none | `id` | registry key; installed drawer is `<root>/<id>`, icon `<root>/<id>.info` | `^[a-z][a-z0-9._-]{0,63}$`, and at most 63 characters (refused otherwise: the client keeps 63). Stable: a different `id` is a different package. Unique per (`id`, `arch`, `abi`) in the current index. |
| `version` | string | yes | none | `version` | compared for upgrades | Upstream's version string; ordered when it follows the [supported version scheme](using.md#keep-things-up-to-date). Example `"0.1.0-rc3"`. At most 63 characters. |
| `revision` | integer | no | absent | `revision` | absent reads as 0; shown as `<version>-aros<revision>`; for an unchanged `version`, upgrade requires a higher `revision`; a newer version can have any revision | This port's release count for that `version`: an integer from 1 to 2147483647. Anything else is refused. |
| `summary` | string | no | `""` | `summary` | shown; searched | One line, no trailing full stop. Client keeps 159 characters. |
| `category` | string | no | `""` | `category` | shown; searched | AROS Archives style, `"game/strategy"`. Client keeps 63 characters. |
| `kind` | string | yes | none | `kind` | shown in ARexx `INFO`; no effect on installing | Package kind, for example `"app"` or `"tool"`. Client keeps 15 characters. |
| `license` | string | no | absent | `license` | shown by `show` and in the window | Free text. Nothing checks it. Client keeps 63 characters. |

### Target

| field | type | required | default | index | client | meaning and constraints |
|---|---|---|---|---|---|---|
| `arch` | string | yes | none | `arch` | must equal the client's CPU, or install is refused before download | `"x86_64"`, `"aarch64"` or `"i386"`. |
| `abi` | string | yes | none | `abi` | must equal the client's, or install is refused before download | `"v11"`: ABIv11, including AROS One. `"v1"`: mainline. `"v0"` for the i386 ABIv0 line. Missing or unknown ABI is not native compatibility. |

**Choosing between builds.** The client collects the entries with the requested `id`, and keeps those whose `arch` and `abi` both match its own (compatibility `native`):

- exactly one: that entry is used;
- none: `install` is refused (wrong CPU, wrong ABI or unknown ABI), and
  `show` describes the first entry in (`arch`, `abi`, `url`) order;
- more than one: `install` and `upgrade` refuse rather than pick.

The order of entries in the file never changes the choice. An upgrade stays on the `arch` and `abi` recorded in the registry. `mkindex.py` allows one manifest per (`id`, `arch`, `abi`), so v1 and v11 builds of the same program can coexist. The legacy `index.json` retains one build per (`id`, `arch`).

**Compatibility is not "runs".** `native` means the CPU and the ABI match, nothing more. Requirements are reported separately, and only a test on a real system says it runs.

### Archive and integrity

| field | type | required | default | index | client | meaning and constraints |
|---|---|---|---|---|---|---|
| `url` | string | yes | none | `url` | fetched over HTTPS only, redirects followed | The exact file. Must start with `https://`, and be at most 511 characters. Must not change content. |
| `size` | integer | yes | none | `size` | download must have exactly this size, or it is discarded | Bytes, a positive integer. Not compared with the file. |
| `sha256` | string | yes | none | `sha256` | download must hash to it, or it is discarded; install refused if it is not 64 characters | Lower-case hex, 64 characters, of the archive at `url`. `mkindex.py` checks the form, not that it matches the file. |
| `subdir` | string | no | `""` | `subdir` | the part of the archive that is installed, as `<root>/<id>/` | <a id="subdir"></a>A directory at the archive's top level, no trailing `/`, at most 127 characters. `""` installs the whole top level, **including `.arospkg/` if present**. Only the members under `subdir/` are installed. Checked against the archive by `mkindex.py` when the archive is in `--cache`, together with the length of every path the client will store (at most 255 bytes inside the package). |
| `icon` | string | no | `""` | `icon` | extracted to `<root>/<id>.info`; install refused if the archive lacks it; an existing icon there is left alone and not claimed | The drawer icon, at most 127 characters. `mkindex.py` checks that the file exists in the archive, not what it is: whether it is the drawer icon and not the program icon is for the reviewer to decide. |

### Requirements and dependencies

| field | type | required | default | index | client | meaning and constraints |
|---|---|---|---|---|---|---|
| `requires_system` | array of tables | no | `[]` | copied as is | probed before download; *missing* refuses the install; *undetermined* installs with a warning and is recorded | What the machine must already provide. At most 16. |
| `requires_system[].type` | string | yes | none | | only `"library"` is probed | Any other value is accepted: `mkindex.py` prints a note and the client reports *undetermined*. |
| `requires_system[].id` | string | yes | none | | the library name | `"crt.library"`. At most 63 characters. |
| `requires_system[].min_version` | integer ≥ 0 | no | `0` | | `OpenLibrary(id, min_version)` | Only when you know it. No other keys are allowed in the table. |
| `depends` | array of strings | no | `[]` | `depends` | 0.6: installed first; not read before 0.6 | `"id"` or `"id >= 1.2"`: other packages of the same catalogue, same CPU and ABI. See [Relations](#relations). |
| `recommends` | array of tables | no | none | `recommends` | 0.6: shown, never installed or required | `[[recommends]]` with `id`, `reason`, optional `min_version`. See [Relations](#relations). |
| `conflicts` | array of strings | no | none | `conflicts` | 0.6: refuses to install them together | Package ids. See [Relations](#relations). |
| `replaces` | array of strings | no | none | `replaces` | 0.6: removes the old package after installing this one | Package ids. See [Relations](#relations). |
| `install` | table | no | `{}` | `install`, copied as is | not read | Reserved. No manifest uses it. |

### Relations

Four relations between packages, each with one meaning in the manifest, the catalogue, the local registry, `apkg` and PkgManager.

**What apkg can deliver.** apkg installs every package into its own drawer, `<root>/<id>`, with its drawer icon beside it. It puts nothing into `LIBS:`, `C:`, `Fonts:` or `DEVS:`, and makes no assigns. So a package can depend only on another package that the program finds in the same root: beside its own drawer (`<root>/<id>`), or through the registry. A library or a command that the system must provide is a `requires_system`, which apkg checks on the machine and never installs. Installing a library's archive would not make the requirement true.

| relation | written as | what apkg does |
|---|---|---|
| `depends` | `depends = ["limpet", "foo >= 1.2"]` | Without it the program does not work. `apkg install` installs it first from the same catalogue, the build for this CPU and ABI, at least the version given (only `>=`, dotted numbers). Its own `depends` too, deepest first. It is recorded as installed as a dependency. A package that another installed package depends on is not removed. |
| `recommends` | `[[recommends]]` with `id`, `reason`, optional `min_version` | Optional. `show` and PkgManager say what it adds (`reason`), whether the catalogue has it for this system, and whether it is installed. It is never installed automatically, never blocks an install or a start, and does not change how the program starts. |
| `conflicts` | `conflicts = ["other"]` | The two are never installed together. Checked in both directions: the new package's list, and the list of every installed package. Nothing is removed to resolve it: the user removes one first. |
| `replaces` | `replaces = ["old"]` | This package takes over an installed one. `apkg install` installs it, then removes the old one as `apkg remove` does: files the user changed or added stay in the old drawer. It implies a conflict. It is **not** a provides: what depends on the old id is not satisfied by the new one, so the replacement is refused while another installed package depends on the old id. |

A rename is a `replaces` plus the program's own migration of its settings: apkg moves no configuration between packages.

Metadata validation checks: ids are package ids; a minimum version is dotted numbers; no relation names the package itself; nothing is both needed and refused or replaced; at most 16 of each; a recommendation says what it adds, in plain ASCII of at most 159 characters; Catalogue generation also checks that every `depends` exists for the same CPU and ABI at the stated version, with no dependency cycles.

**Old clients.** apkg 0.4 and 0.5 read `index-v2.json` and skip keys they do not know: they would install a package without its dependencies, beside a conflicting one or without removing what it replaces, and report success. An entry with `depends`, `conflicts` or `replaces` therefore carries `"client_features": ["relations"]` and is published only in `index-v3.json`, the file apkg 0.6 and later fetch; a client that meets a feature it does not know refuses the package. `recommends` alone needs no new client.

**Large members.** apkg before 0.6 refuses an archive with a member that unpacks to more than 64 MiB ("member over size limit"); 0.6 accepts up to 128 MiB in a ZIP. The review record (`tools/review.py`, suite 3) notes the largest member, and an entry whose largest member is over 64 MiB carries `"client_features": ["large-members"]` and is published only in `index-v3.json`. Entries reviewed before suite 3 carry no size record and are listed as before.

**States.** For each relation `show` keeps apart: the id is in the catalogue (for any system); the catalogue has a build for this system, new enough; the registry has it in this root; and the relation is satisfied (installed, for this system, new enough). An author's declaration never makes an entry Reviewed or runtime tested.

A library requirement is decided on the machine, in this order:

1. the library is already open, at `min_version` or later: **satisfied**;
2. `OpenLibrary` succeeds: **satisfied**. This runs the library's
   initialisation, as the program would;
3. otherwise **missing**, and the message says whether a file of that name is
   in `LIBS:`.

A machine short of memory can report *missing* for a library that is there.

### Claims from testing

| field | type | required | default | index | client | meaning |
|---|---|---|---|---|---|---|
| `installs_on` | array of strings | no | `[]` | copied | not read | Systems where the package was installed. |
| `runs_on` | array of strings | no | `[]` | copied | not read | Systems where it was started. |
| `does_not_run_on` | array of strings | no | `[]` | copied | not read | Systems where it is known not to run. |

These are free-form claims, kept as they were. They are not bound to an archive and they do not make an entry Reviewed or runtime tested; a test that names the bytes it was made with goes in `runtime-tests/`.

### Listed, Reviewed and runtime tests

Three facts about one variant (`id`, `arch`, `abi`) and one archive, kept apart:

| fact | written by | where | in the index |
|---|---|---|---|
| **Listed** | the maintainer, by merging an approved manifest | `manifests/` | every entry; `review.status` is `"listed"` unless the next row applies |
| **Reviewed** | the publish workflow, from `tools/review.py` | `review/<id>.<arch>.<abi>.json` | `review.status` `"reviewed"`, with `suite`, `date`, `checked`, `unchecked` |
| **Runtime tested** | the maintainer, after installing and starting it | `runtime-tests/<id>.<arch>.<abi>.toml` | `runtime_tests`, only the tests whose `sha256` is the entry's |

**The review** checks, without installing or starting anything:

- `metadata`: every rule of this reference (`mkindex.py`'s check);
- `download`: the file at `url` has the entry's `size` and `sha256`;
- `archive`: it is a ZIP or LHA that can be listed;
- `paths`: no member name the client refuses (absolute, a volume, `..`,
  control characters, too long), and no two members that would land on one
  another: the same name twice, names that differ only in case (AROS file
  systems compare names without case), or a file with the name of a drawer;
- `layout`: `subdir`, `icon`, `launch.executable`, its icon for a Workbench
  start, `launch.working_directory`, the file count, and an
  `application.json` in the archive that agrees with the entry;
- `cpu`: the ELF `e_machine` of the program named by `[launch]`, and of
  the archive's programs, is the entry's `arch`; at least one ELF program is
  for it. A file for another CPU beside it is reported, not refused. An
  archive without ELF programs records `cpu` as unchecked;
- `abi`: always unchecked. ELF headers do not record the AROS ABI, so
  whether an x86_64 program is for ABIv1 or ABIv11 is not decided from the
  files, and not guessed.

The record (`review/<id>.<arch>.<abi>.json`, `schema` 1) holds the `suite`, the `date`, the `sha256` and `size` of the archive reviewed, the `manifest_digest` (SHA-256 of the entry with its override, as parsed: a comment or a reformatted line is no change, a changed value is), the `result`, each check with its `detail`, and what was `unchecked` and why. `mkindex.py` writes `"reviewed"` only when a passed record's `sha256`, `size` and `manifest_digest` are the entry's. New bytes or changed metadata make the entry Listed until it is reviewed again.

A pull request that changes an entry's data has the changed variants reviewed; one that only changes comments has none. A pull request that edits `review/` or `history.json` fails. After a merge the publish workflow reviews every entry changed since the last published catalogue and commits the records. That catalogue's commit is the branch `published`, which only the workflow moves, in the same push as `main`; a run whose push is refused because `main` moved leaves it in place and tries again on the new `main`, so two merges in quick succession are both reviewed. Its *review_all* run reviews every entry; an entry that no longer passes stays Listed, and the run goes on.

<a id="runtime-tests"></a>**Runtime tests**, one file per variant:

<!-- validate: none -->
```toml
[[test]]
sha256  = "671d89c9b45c9358cd02faf379510ef8a49bf746119fa45f7b90c466575c3e98"
date    = "2026-09-29"
system  = "AROS One 1.3"
arch    = "x86_64"
abi     = "v11"
machine = "QEMU (KVM)"
result  = "passed"                      # or "failed"
checked = ["install", "start", "remove"] # install, start, run, remove, upgrade, rollback, verify
note    = "window open after 15 s"       # optional, plain ASCII, at most 159 characters
```

`arch` and `abi` must be the file's; `system`, `machine` and `note` follow the string rules above. A malformed file stops publication. A test whose `sha256` is not the entry's stays in the file and is not published: it was made with other bytes.

**First publication dates.** `history.json` holds, for every variant and every application id, the UTC day the catalogue first published it and the commit. The publish workflow wrote it first from the git history of `index.json` and `index-v2.json` (`tools/catalogue_history.py bootstrap`; the repository's first commit, 2026-09-10, is the start of the catalogue); since then it adds each new variant on the day it first publishes it. A new version, a new revision, a new review, or a removal and return keep the first date. The index carries them as `first_added` (the variant) and `app_first_added` (the application, any variant), both absent when unknown. `mkindex.py` dates nothing without a history file.

### Notes for the user

| field | type | required | default | index | client | meaning and constraints |
|---|---|---|---|---|---|---|
| `post_install_notes` | array of strings | no | absent | `post_install_notes` | shown after a successful install, by `show` and in PkgManager's details; copied into the registry entry at installation | Short plain text a user needs after installing, one string per line: game data to supply separately, a folder to choose at first start, where the manual is. At most 8 lines of at most 159 characters, printable ASCII without `"` or `\`. `mkindex.py` refuses anything else, and does not shorten it. |

The notes are text, nothing more: no markup, no variables, nothing is run, and no terminal sequence can be in them. The package's author can write them for their own package, or the index maintainer for an existing upload, without repacking the archive. PkgManager makes the `http://` and `https://` addresses in them links; one that ends in `.`, `,`, `:`, `;`, `!`, `?` or `'` must be written as `<address>`, or that last character is taken for punctuation. They do not replace structured fields: a library the program needs belongs in `requires_system`, where the client checks it. Do not repeat where the package is installed or how to open it: the client prints both after every install, with the right path for the user's package root.

These limits are what the current client can read safely, not the intended shape of the format; Unicode and longer text need a client change first.

A client that does not know the key skips it. The registry's copy is a record of what the user was told at installation, read when the catalogue no longer has the package; the catalogue's text is the one maintained.

<!-- validate: fragment -->
```toml
post_install_notes = [
  "This port needs the data files of the original game.",
  "Copy them into the Data drawer, or choose their folder at first start.",
]
```

### `[launch]`: how it is started

Optional, for an application: which file is the program, and whether it is a Workbench or a Shell program. With it, `apkg run <id>`, PkgManager's **Run** and ARexx `RUN` can start it; without it they say so and Open folder is what remains. The full design is [RFC 0002](../rfc/0002-application-folder.md).

| field | type | required | meaning and constraints |
|---|---|---|---|
| `executable` | string | yes | The program, relative to the package's drawer (`subdir`): `"Folio"`, `"C/DirTree"`. Not absolute, no `:`, `\`, `..`, `.` or empty component, printable ASCII without `"`. Not an icon, not the drawer. |
| `mode` | string | yes | `"workbench"`: started as a double-click on its icon starts it, with the icon's stack and tool types; the archive must have `<executable>.info` beside the program, a tool icon (classic or PNG). `"shell"`: started as a Shell command with the arguments the user gives. |
| `working_directory` | string | no | Shell mode: where it starts, relative to the drawer. Default: the drawer holding the program. A Workbench program always starts in its own drawer, so in workbench mode only that default is accepted. |

No other key is accepted, by `mkindex.py` or by the client. Stack size and tool types are not fields: they stay in the program's icon.

<!-- validate: fragment -->
```toml
[launch]
executable = "Micropolis"
mode       = "workbench"
```

Where it goes: `index-v3.json` and `index-v2.json` carry it as `launch` (with `working_directory` written out); `index.json` never does. `apkg-pack build` writes `<drawer>/application.json` from it into the archive; for an archive without that file (an upload you did not build), apkg writes it into the drawer at installation and records the table in the registry entry, so a rollback writes it again. An `application.json` already in an archive must say the same as the manifest: `mkindex.py` and `apkg-pack check` refuse it otherwise.

### `[source]`

Optional, recommended for anything built from source. It records where the source is. Installing never needs it, never fetches it and never checks it, and a missing `[source]` produces no warning.

| field | type | index | meaning |
|---|---|---|---|
| `repository` | string | `source` (this value only) | Repository URL. The 0.7.3 client holds 2047 characters. The generator still warns above the older 255-character display limit; it does not truncate the index value. |
| `revision` | string | none | A commit or tag, so the link names a fixed point. |
| `archive` | string | none | File name of a source archive published beside the package. |
| `sha256` | string | none | That archive's hash. |
| `built_with` | string | none | Toolchain or SDK profile. |
| `built_on` | string | none | Build date of that SDK, not of the package. |

A pinned repository and a hashed source archive are equally valid, alone or together. Nothing in `[source]` proves that the sources produced the binary.

### A worked example

The manifest in [guide section 4](../guide/packaging.md#4-the-manifest) is complete, and is checked by `tests/test_doc_examples.py`.

## The index

`mkindex.py` writes one JSON object:

| key | meaning |
|---|---|
| `schema` | `1`. The client does not check it. |
| `source` | The AROS Archives catalogue the candidates were imported from. **Not** the per-package `source`. |
| `packages` | Array of entries, sorted by `id`, then `arch`. |

This is the legacy-compatible entry generated from the historical Micropolis manifest in the layout guide. The test checks that it matches `mkindex.py`'s output exactly:

<!-- validate: index-entry -->
```json
{
  "id": "micropolis",
  "version": "0.1.0-rc3",
  "summary": "Classic city-building simulation for AROS",
  "arch": "x86_64",
  "abi": "v11",
  "category": "game/strategy",
  "url": "https://github.com/tomaszstaniak/aros-micropolis/releases/download/v0.1.0-rc3/micropolis.x86_64-aros-v11.lha",
  "size": 2593505,
  "sha256": "c46bbc14274af53fb4452f59081fbcf28f1ce5fffb534c06d169b0b1911cd124",
  "depends": [],
  "requires_system": [
    { "type": "library", "id": "crt.library" },
    { "type": "library", "id": "m.library" },
    { "type": "library", "id": "stdlib.library" }
  ],
  "installs_on": ["aros-one"],
  "runs_on": ["aros-one"],
  "does_not_run_on": ["mainline-x86_64"],
  "kind": "app",
  "subdir": "Micropolis",
  "icon": "Micropolis.info",
  "install": {},
  "revision": 1,
  "source": "https://github.com/tomaszstaniak/aros-micropolis",
  "license": "GPL-3.0-or-later with EA Section 7 terms; Micropolis Public Name License",
  "review": { "status": "listed" },
  "runtime_tests": []
}
```

`revision`, `source` and `license` appear only when the manifest gives them. `review` and `runtime_tests` are always present (clients before 0.6 skip them); `first_added` and `app_first_added` appear when `history.json` dates the entry. A reviewed entry reads, for example, `"review": { "status": "reviewed", "suite": 1, "date": "2026-10-14", "checked": ["metadata", "download", "archive", "paths", "layout", "cpu"], "unchecked": ["abi"] }`. `index.json`, for clients up to 0.3.2, carries none of these. `abi` is `null` when the manifest has none.

### Catalogue versions and client limits

- `index-v3.json`: current catalogue, fetched by apkg 0.6 and later. Includes packages needing relations or large ZIP members.
- `index-v2.json`: for 0.4 and 0.5 clients; omits entries requiring those features.
- `index.json`: a restricted catalogue for clients up to 0.3.2. It retains one ABI per (`id`, `arch`) and fits the old parser limits.

All three use JSON schema 1. They differ in which entries and fields older clients can handle. A current client refuses unknown `client_features`.

The client skips unknown keys and does not decode JSON string escapes; the generator therefore requires printable ASCII without double quotes or backslashes. Current catalogue limits are 16 MiB and 1,048,576 JSON values. Older clients were limited to 4 MiB and 8192 values. The generator refuses publication when the corresponding file exceeds its limits.

## The local registry

Written by the client, one file per installed package, as `<root>/db/installed/<id>.json`. The default root is `SYS:Packages`. `apkg info <id>` prints it. This historical example was recorded with 0.3 on 29 September 2026. Its contents list is shortened to one file. It illustrates the base registry fields; later clients also record launch metadata, relations and package notes when present:

<!-- validate: registry -->
```json
{
  "schema":1,
  "name":"micropolis",
  "version":"0.1.0-rc3",
  "revision":1,
  "arch":"x86_64",
  "abi":"v11",
  "subdir":"Micropolis",
  "installed":"2026-09-29T09:38:48Z",
  "origin":"https://github.com/tomaszstaniak/aros-micropolis/releases/download/v0.1.0-rc3/micropolis.x86_64-aros-v11.lha",
  "archive_sha256":"c46bbc14274af53fb4452f59081fbcf28f1ce5fffb534c06d169b0b1911cd124",
  "archive_size":2593505,
  "reason":"explicit",
  "dir":"micropolis",
  "requires_system": [
    { "type":"library", "id":"crt.library", "min_version":0, "state":"satisfied", "found_version":5, "checked":"already open, version 5" },
    { "type":"library", "id":"m.library", "min_version":0, "state":"satisfied", "found_version":1, "checked":"already open, version 1" },
    { "type":"library", "id":"stdlib.library", "min_version":0, "state":"satisfied", "found_version":3, "checked":"already open, version 3" }
  ],
  "icon": { "state":"installed", "sha256":"8d131cb407523acbcce1cd7000370aa391a9aa34bd31889400267433372a68db", "size":1822, "member":"Micropolis.info" },
  "previous": { "registry":"", "archive":"" },
  "contents": [
    { "path":"tiles.bmp", "sha256":"b6e0dd77ca5e59baba6a473aa50a6e41b53471beaa2613a04eb404251dfe1399", "size":246838 }
  ]
}
```

| key | from | meaning |
|---|---|---|
| `schema` | client | `1` |
| `name` | index `id` | |
| `version`, `revision` | index | `revision` is `0` when the index gave none |
| `arch`, `abi` | index | The build installed; an upgrade stays on it. `abi` is missing in entries written before 0.3, and readers then assume the client's own. |
| `subdir` | index | How the archive was cut, so a rollback cuts the same way. |
| `installed` | client | UTC time of the install or upgrade. |
| `origin`, `archive_sha256`, `archive_size` | index, verified | The archive that was installed. |
| `reason` | client | `"explicit"` when installed by name, or `"dependency"` when installed for another package. |
| `dir` | client | The drawer under the root: always the `id`. |
| `requires_system` | index + probe | Each requirement with its `state` (`satisfied`, `missing`, `undetermined`), `found_version` (`-1` if none) and the probe's words in `checked`. It records what this machine said at install time. |
| `icon` | client | `state`: `installed` (the icon belongs to the package), `kept-existing` (an icon was already there and was not touched) or `none`. When the icon was installed, `sha256` and `size` let removal tell whether Workbench has since rewritten it. `member` is its name in the archive. |
| `previous` | client | After an upgrade: the registry entry and archive it replaced, for `rollback`. Empty otherwise. |
| `contents` | client | Every installed file, with `path` relative to the drawer, `sha256` and `size`. Removal deletes a file only if it still matches. Directories are not listed. |

Other things under the root, all the client's:

- `db/index.json`, the index `apkg update` fetched;
- `db/transactions/`, the journal of work in progress;
- `db/previous/<id>.json` and `cache/previous/<id>.<ext>`, kept for rollback;
- `db/doctor/<id>-<n>.json`, what a removal left behind and why;
- `cache/<id>.<ext>`, the verified archive, kept after removal;
- `tmp/`.

## What is checked where

| check | `mkindex.py` | client | a person |
|---|---|---|---|
| status is approved, `depends_checked` is true | ✔ | | decides it |
| required keys present | ✔ | | |
| `id` pattern | ✔ | | chooses a stable name |
| `arch` is x86_64, i386 or aarch64 | ✔ | equals its own CPU | |
| `abi` value | ✔ `v0`, `v1` or `v11` | equal to its own | that the binary really is that ABI |
| one entry per (`id`, `arch`, `abi`) | ✔ | refuses when several match | |
| `sha256` form | ✔ | 64 characters | |
| `size` and `sha256` match the file at `url` | review ✔ | ✔ on every download | measures them |
| member names distinct, also without case; CPU of the programs | review ✔ | names ✔ before writing | |
| `url` is HTTPS, reachable and stable | HTTPS ✔ | refuses anything but HTTPS; fetches it | chooses it |
| `subdir` and `icon` exist in the archive | ✔ if the archive is cached | `icon` ✔; `subdir` is not checked, and members outside it are skipped | that `icon` is the drawer icon |
| archive member paths and sizes | review ✔ | ✔ before writing anything | |
| `requires_system` shape | ✔ | ✔ | |
| `requires_system` is complete and correct | none | probes what is listed | derives it from the binary |
| `depends` exist for the same CPU and ABI | ✔ | installs missing dependencies; checks versions | |
| strings readable by the client, operational lengths | ✔ | reads them as they stand | |
| index.json fits clients up to 0.3.2 (8192 values, 4 MiB); index-v2.json and index-v3.json fit later ones | ✔ all three files | 0.3.2 reports invalid JSON past it; later clients read up to 1048576 values, 16 MiB | |
| `version`, `revision`, `summary`, `license`, `[source]` | types, string limits and revision range | version ordering for upgrades; other fields displayed | chooses values |
| `installs_on`, `runs_on`, `does_not_run_on` | none | none | only claims that were tested |
| the program starts and keeps user data | none | none | ✔, recorded in `runtime-tests/` |

**Neither tool authenticates anything.** A SHA-256 in the index proves that a download is the file the index names. It does not prove who wrote the index, and nothing is signed.

## Running `mkindex.py`

From an arospkg checkout:

```text
python3 tools/mkindex.py --manifests ../arospkg-index/manifests --out-dir ../arospkg-index
```

`--out-dir` receives `index-v3.json`, `index-v2.json` and `index.json`. The old `--out` option is no longer supported. Use the catalogue's publish workflow for production updates; it supplies review records, overrides, runtime tests and publication history as well.

Skeleton and excluded manifests are omitted with a reason. An invalid approved manifest, duplicate variant or unresolved dependency stops publication with exit code 1. Missing cached archives produce warnings; metadata validation alone does not verify the download. Use the review workflow to check the remote archive.

## Current limitations

- Strings must follow the ASCII restrictions above. Display fields can be shortened by the client.
- With an empty `subdir`, metadata at the archive root is installed too. Prefer a named application drawer.
- The drawer icon field is checked for existence; a maintainer must confirm it is the intended icon.
- SHA-256 checks identify the expected bytes. The catalogue is unsigned, and archive review does not establish runtime compatibility or application safety.
- Cached archives remain after removal. There is no automatic cache pruning in 0.7.3; rollback depends on the saved previous archive and registry.
