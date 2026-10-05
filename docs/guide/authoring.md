# Publishing your program with apkg-pack

`apkg-pack` turns an AROS program you have already built into a checked
ZIP that apkg can install. It then prepares the catalogue entry from the
URL where you uploaded the ZIP.

- Your source code and repository can stay private. Only the finished ZIP
  is published, wherever you normally publish it.
- Making the package needs no GitHub account, no AI assistant and no
  network. GitHub is used only by `submit --pr`.
- `build` packs your program; it does not compile it.
- `check` confirms the description and the archive layout. It does not run
  your program and does not show that it works on any AROS.

## Install

You need Python 3.11 or later on macOS, Linux or Windows. `init`, `check`
and `build` need nothing else.

Download `apkg-pack.pyz` from the
[latest arospkg release](https://github.com/tomaszstaniak/arospkg/releases/latest)
and run it with Python:

```text
python3 apkg-pack.pyz --version
```

On Windows, run `py apkg-pack.pyz`. The examples below write `apkg-pack`
for whichever form you use. To build the file from an arospkg checkout
instead: `python3 tools/make-apkg-pack.py --output apkg-pack.pyz`.

On AROS itself, `--version`, `check` and `build` ran once on AROS One
(ABIv11) with the aros-cpython 0.1.0 interpreter, with two limits: that
Python starts only while a TCP/IP stack is running, and paths must be
written POSIX style (`/RAM/demo.zip`, not `RAM:demo.zip`). The same input
packed there gives the same files but a different ZIP checksum than on a
host, because the compressor differs. Other AROS systems are not tested.

Optional: `git` and the GitHub CLI `gh`, signed in with `gh auth login`,
for `submit --pr`; `lha` (lhasa) to check `.lha` archives.

## A complete example

### 1. The built drawer

Your build leaves the drawer users get, with its icon beside it:

```text
build/xRick/          the drawer users get
build/xRick.info      its icon
```

The computer you build on does not matter. A program built on an Apple
Silicon Mac for AROS i386 is an i386 package.

### 2. Describe it once

```text
apkg-pack init build/xRick
```

`init` reads the drawer name, the program's `$VER:` string, the CPU of its
executables and the icon, then asks for the rest. It never guesses the ABI.
Enter the one you built for: `v11` for AROS One and current distributions,
`v1` for mainline, `v0` for i386. A binary for one ABI does not run on
another, and the CPU does not tell them apart. Also enter the system
libraries and arospkg packages it needs, and its licence.

The result is `build/xRick.arospkg.toml`, beside the drawer. Keep it with
your project and edit it like any text file; `init` never overwrites it.
In a release script, give every field as an option and add
`--non-interactive` (see `apkg-pack init --help`).

Optional lines shown to the user after installation:

<!-- validate: fragment -->
```toml
post_install_notes = [
  "This port needs the data files of the original game.",
  "Copy them into the Data drawer, or choose their folder at first start.",
]
```

At most 8 lines of 159 characters, printable ASCII without `"` or `\`.
apkg already says where the program was installed and how to start it.

### 3. Check and build

```text
apkg-pack check build/xRick
apkg-pack build build/xRick --output xrick-1.0.2.x86_64-aros-v11.zip
apkg-pack check xrick-1.0.2.x86_64-aros-v11.zip
```

`check` names the field and the reason for every problem:

```text
xRick.arospkg.toml: post_install_notes[1]: 184 characters; maximum is 159
```

The ZIP holds the drawer (empty drawers included), its icon, and your
description as `.arospkg/manifest.toml`. Files such as `.DS_Store` are left
out; every other file is stored byte for byte. The same files, description
and timestamp give the same ZIP on the same computer. `SOURCE_DATE_EPOCH`
or `--epoch` sets the timestamp.

Test the program on its target system before you publish it; see
[Testing on AROS](packaging.md#5-testing-on-aros).

### 4. Upload

Upload the ZIP unchanged to [AROS Archives](https://archives.aros-exec.org)
or as a GitHub release asset, and copy its public HTTPS download URL. Use a
new URL for each release where you can.

Uploading does not change the arospkg catalogue. The next step does.

### 5. Submit

```text
apkg-pack submit <download-url> --pr
```

`submit` downloads the archive, reads the description inside it, measures
its size and SHA-256, and opens a pull request in
[arospkg-index](https://github.com/tomaszstaniak/arospkg-index) with the
entry. It uses your existing `gh` login and goes through your fork if you
cannot push. Submitting the same release again opens no second pull
request. You never type the size or checksum.

Without GitHub, preview the entry in a local copy of the catalogue and send
it to the maintainers another way, such as an issue or an e-mail:

```text
git clone https://github.com/tomaszstaniak/arospkg-index.git
apkg-pack submit <download-url> --index arospkg-index --dry-run
```

### 6. After you submit

An automatic check downloads the archive again, compares it with the entry
and applies the same rules. A maintainer reviews the entry and merges it.
The catalogue is then regenerated, and users see the package the next time
apkg or PkgManager refreshes its index. Nothing is published before the
merge.

### 7. The next release

Build as usual, then edit the description you kept:

- a new upstream release: set `version` to the upstream version and remove
  `revision`;
- a new AROS package of the same version, such as a packaging fix or a
  rebuild: raise `revision`.

`version` is the program's own version. `revision` numbers the AROS
packages of that version. apkg-pack changes neither. Then repeat steps 3
to 5. Changed bytes need a new version or revision: `submit` refuses a
different file under one that is already published.

To pack from a release script, run the same three commands after the
build. [Folio's release script](https://github.com/tomaszstaniak/aros-foliopdf/blob/main/scripts/make-release.sh)
is a working example. It keeps one description per target in
`packaging/catalogue/` and stops if its version does not match the release.

## Optional: a skill for AI coding assistants

If you use an AI coding assistant that reads agent skills (Claude Code and
others supported by [skills.sh](https://skills.sh)), the
`aros-package-author` skill walks it through the same steps. It runs
apkg-pack, asks only for what is missing, never guesses the ABI or
dependencies, does not read or send your sources, and uploads or submits
nothing unless you ask. Install it in your project with Node.js:

```text
npx skills add tomaszstaniak/arospkg --skill aros-package-author
```

Nothing above depends on it.

## Notes for maintainers

- Archives without `.arospkg/manifest.toml`, including those already on
  AROS Archives, keep their maintainer-written manifests in `manifests/`.
- A correction to an author's entry, or a test result such as `runs_on`,
  goes in `overrides/` under the manifest's file name: any field except
  id, arch, abi, url, size and sha256. It is applied each time the
  catalogue is made and survives the author's next submission.
- A variant is its id, CPU and ABI: `xrick.x86_64.v11.toml` and
  `xrick.x86_64.v1.toml` can both be published. Older files named
  `<id>.<arch>.toml` are still read.
- Do not edit `index.json` or `index-v2.json`; they are generated.

See the [packaging guide](packaging.md) for the archive layout and the
[metadata reference](metadata.md) for every field.
