# Publishing your program with apkg-pack

Use `apkg-pack` to create a package and submit it to the arospkg catalogue.
It checks the manifest, builds a ZIP and prepares a pull request from your
uploaded archive. It does not run the application.

## Before you start

Use macOS or Linux with Python 3.11 or later and Git. For LHA archives,
install `lha` (lhasa). To submit a pull request, install the GitHub CLI
(`gh`) and sign in with `gh auth login`.

Get the tools and run the commands below from this checkout:

```text
git clone https://github.com/tomaszstaniak/arospkg.git
cd arospkg
tools/apkg-pack --help
```

The tool runs on the host, not on AROS. See the
[packaging guide](packaging.md) for archive layout and the
[metadata reference](metadata.md) for field definitions.

## 1. Describe the drawer

Your program is a drawer, with its icon beside it:

```text
build/xRick/          the drawer users get
build/xRick.info      its icon
```

```text
tools/apkg-pack init build/xRick
```

The tool reads the drawer name, `$VER:` string, executable CPU and icon
where available, then asks for missing fields. Enter the ABI your program
was built for and its dependencies. For release scripts, supply the fields
as options and add `--non-interactive`. Run `tools/apkg-pack init --help`
for the options.

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

Fix reported errors before building. Passing this check validates the
package description, not application behaviour or acceptance into the
catalogue. Test your program on its target system; see
[Testing on AROS](packaging.md#5-testing-on-aros).

## 4. Pack

```text
tools/apkg-pack build build/xRick --output xrick.x86_64-aros-v11.zip
```

The ZIP holds the drawer (empty drawers included), its icon, and the
description as `.arospkg/manifest.toml`. Files such as `.DS_Store` are
excluded. The same input files, manifest and timestamp produce the same
ZIP; `SOURCE_DATE_EPOCH` sets the timestamp.

Keep the upstream version in `version`. Increase `revision` for a new
AROS release of that version. The tool does not change either field.

## 5. Upload

Upload the ZIP to AROS Archives or a GitHub release and copy its public
HTTPS download URL. Prefer a separate URL for each release. Archives may
reuse a URL; changed contents still require a new version or revision and
an updated catalogue entry.

## 6. Submit

```text
tools/apkg-pack submit <download-url> --pr
```

Replace `<download-url>` with the archive URL. The tool reads the embedded
manifest, calculates the size and SHA-256, and prepares
`manifests/<id>.<arch>.<abi>.toml`. It uses a fresh clone and your existing
`gh` login. If you lack write access, it creates or reuses your fork.
Re-submitting the same release does not open a duplicate PR.

To preview a change locally without opening a PR:

```text
git clone https://github.com/tomaszstaniak/arospkg-index.git ../arospkg-index
tools/apkg-pack submit <download-url> --index ../arospkg-index --dry-run
```

Do not edit `index.json` or `index-v2.json`; they are generated.

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
