# Publishing your program with apkg-pack

For an author who wants a program in the catalogue. You describe it once;
the tool checks it with the catalogue's own rules, packs it, and prepares
the catalogue entry from the file you uploaded. It runs on a computer with
Python 3.11 or later (macOS, Linux); it does not run on AROS, and it does not
run your program.

The [packaging guide](packaging.md) explains the rules behind each step, and
the [metadata reference](metadata.md) every field.

## 1. Describe the drawer

Your program is a drawer, with its icon beside it:

```text
build/xRick/          the drawer users get
build/xRick.info      its icon
```

```text
tools/apkg-pack init build/xRick
```

It reads what it can: the id from the drawer name, the version from the
program's `$VER:` string, the CPU from its executables, the icon. It asks for
the rest. It never guesses the ABI (`v11` for AROS One and other current
distributions, `v1` for mainline) or what the program needs, because a wrong
answer installs something that crashes. For a release script, give every
answer as an option and add `--non-interactive`; `--help` lists them.

The result is `build/xRick.arospkg.toml`, beside the drawer. It is never
overwritten; edit it like any text file.

## 2. Notes for the user (optional)

<!-- validate: fragment -->
```toml
post_install_notes = [
  "This port needs the data files of the original game.",
  "Copy them into the Data drawer, or choose their folder at first start.",
]
```

Write what this program in particular needs after installation: data to
supply, a first-start choice, a setting. Do not write where it is installed
or how to open it: apkg prints both, correctly for each user. The current
client reads at most 8 lines of 159 characters, printable ASCII without `"`
or `\`. Anything else is refused with the line and the reason; nothing is
shortened.

## 3. Check

```text
tools/apkg-pack check build/xRick
```

```text
xRick.arospkg.toml: post_install_notes[1]: 184 characters; maximum is 159
```

Each problem names the field and what to change. A clean check means the
catalogue will accept the description. It does not mean the program works:
test it on AROS ([packaging guide, section 5](packaging.md#5-testing-on-aros)).

## 4. Pack

```text
tools/apkg-pack build build/xRick --output xrick.x86_64-aros-v11.zip
```

The ZIP holds the drawer (empty drawers included), its icon, and the
description as `.arospkg/manifest.toml`. Nothing is reorganised and junk
such as `.DS_Store` is left out. The same drawer always packs to the same
bytes (`SOURCE_DATE_EPOCH` sets the timestamp). The version and revision
are yours to raise; the tool never changes them.

## 5. Upload

Put the ZIP at a URL that will not change: a GitHub release asset or an AROS
Archives upload. New bytes need a new revision or version, and normally a new
URL: under the same version and revision, other bytes are refused.

## 6. Submit

```text
tools/apkg-pack submit https://github.com/you/xrick/releases/download/v1.0.2/xrick.x86_64-aros-v11.zip --dry-run
tools/apkg-pack submit <url> --pr
```

`submit` downloads the file, checks the description inside it, measures its
size and SHA-256, and writes the catalogue entry,
`manifests/<id>.<arch>.<abi>.toml`. `--dry-run` shows the change against a
checkout of [arospkg-index](https://github.com/tomaszstaniak/arospkg-index)
(`--index`) and writes nothing. `--pr` works in a fresh clone of the
catalogue repository, pushes a branch to your fork of it (made if you have
none) and opens a pull request, with the `gh` command's existing login.
Submitting the same release again opens no second one. You never edit
`index.json` or copy fields out of your ZIP.

The pull request is checked automatically: the archive is downloaded again
and must match, be readable and pass the same rules. A maintainer reviews
the source and metadata before merging; the catalogue is then regenerated
by the repository's own workflow.

## For maintainers

- Packages whose archive has no `.arospkg/manifest.toml` keep their
  maintainer-written manifests in `manifests/`, as before.
- A deliberate correction to an author's entry goes in `overrides/`, under
  the same file name as the manifest: any field except id, arch, abi, url,
  size and sha256. The generator applies it whenever the catalogue is made,
  so it takes effect without a new submission and survives the next one.
- A variant is its id, CPU and ABI: `xrick.x86_64.v11.toml` and
  `xrick.x86_64.v1.toml` can both be published. Older files named
  `<id>.<arch>.toml` are still read.
