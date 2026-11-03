# Publish your program with apkg-pack

`apkg-pack` turns a built AROS application drawer into a ZIP and prepares its catalogue entry. This guide covers **apkg-pack 0.2.0** and **arospkg 0.7.3**.

Your source repository can stay private. Building a package needs no account or network connection. The tool checks metadata and archive contents; you still need to test the program on AROS.

## Install

Download [apkg-pack.pyz 0.2.0](https://github.com/tomaszstaniak/arospkg/releases/download/apkg-pack-v0.2.0/apkg-pack.pyz) ([checksums](https://github.com/tomaszstaniak/arospkg/releases/download/apkg-pack-v0.2.0/SHA256SUMS), [release notes](https://github.com/tomaszstaniak/arospkg/releases/tag/apkg-pack-v0.2.0)). Use Python 3.11 or later on macOS, Linux or Windows:

```text
python3 apkg-pack.pyz --version
```

On Windows, use `py` instead of `python3`. The examples below use `python3` throughout. Only `submit --pr` also needs Git and the GitHub CLI, signed in with `gh auth login`. Checking existing LHA archives needs `lha` (lhasa).

AROS support is experimental: `check` and `build` were tested with aros-cpython 0.1.0 on AROS One ABIv11. That interpreter needs a running TCP/IP stack at startup, even for offline work. Use POSIX paths such as `/RAM/myapp.zip`, not `RAM:myapp.zip`.

## 1. Prepare the drawer

Build the application first. Put everything the user needs in one drawer, with its drawer icon beside it:

```text
build/MyApp/
build/MyApp.info
```

The [layout guide](packaging.md#1-the-archive) explains icons, paths and archive limits. The target is the system you built for, regardless of which computer you used to build it.

## 2. Describe the package

```text
python3 apkg-pack.pyz init build/MyApp
```

`init` reads the drawer name, the program's `$VER:` string, executable CPU and icon, then asks for the remaining fields. It never guesses the ABI. Enter the ABI from your build configuration, the licence and required system libraries or packages.

It writes `build/MyApp.arospkg.toml` beside the drawer. Keep this file with your project and edit it for later releases. `init` will not overwrite it. For release scripts, supply the fields as options and use `--non-interactive`; see `init --help`.

### Say how it starts

For an application, add a launch table. Paths are relative to its drawer:

<!-- validate: fragment -->
```toml
[launch]
executable = "MyApp"
mode = "workbench"
```

A Workbench launch needs `MyApp.info` beside the executable, with the stack size and tool types the program requires. For a command-line program, use `mode = "shell"`. You can also set a relative `working_directory` in Shell mode; by default it starts in the executable's drawer.

`build` generates `application.json` from this table. That lets `apkg run` and PkgManager's **Run** start the application, including a drawer unpacked by hand. Omit `[launch]` for a package with no program to start. See the [launch fields](metadata.md#launch-how-it-is-started).

### Requirements and notes

Use `requires_system` for a library already provided by the system. Use `depends` for another package your program can find under the same package root. arospkg does not install into shared system drawers such as `LIBS:`.

The other relations are `recommends` (optional), `conflicts` (cannot be installed together) and `replaces` (removes the old package after installing the new one). See [relations](metadata.md#relations) for constraints and version syntax. Packages using `depends`, `conflicts` or `replaces` are published only for clients from 0.6 onward.

Write top-level fields before any `[table]` or `[[table]]`. For example, notes about game data belong near the top of the file:

<!-- validate: fragment -->
```toml
post_install_notes = [
  "This port needs the data files of the original game.",
  "Copy them into the Data drawer before starting.",
]
```

Notes can contain up to 8 lines of 159 characters, using printable ASCII without double quotes or backslashes inside the text. Keep them to instructions specific to your program. apkg already reports where it installed the package.

## 3. Check, build and test

```text
python3 apkg-pack.pyz check build/MyApp
python3 apkg-pack.pyz build build/MyApp --output myapp.zip
python3 apkg-pack.pyz check myapp.zip
```

The ZIP contains the drawer, its icon and `.arospkg/manifest.toml`. Empty drawers are preserved; files such as `.DS_Store` are excluded. `check` reports invalid metadata, unsafe paths, missing launch files and CPU mismatches. It cannot determine the ABI from an ELF header or tell whether the application works.

[Test the resulting archive on AROS](packaging.md#5-testing-on-aros) before uploading. A ZIP built on another host may have a different checksum, even with the same contents. Publish the exact file you tested. For repeatable builds on the same host, use `SOURCE_DATE_EPOCH` or `--epoch` to set the timestamp.

<a id="4-upload"></a>
<a id="5-submit"></a>
## 4. Upload and submit

Upload the ZIP unchanged to [AROS Archives](https://archives.arosworld.org), a GitHub release or another HTTPS host. Use a new download URL for each release where possible. Then submit that URL:

```text
python3 apkg-pack.pyz submit <download-url> --pr
```

`submit` downloads the ZIP, reads its embedded manifest and calculates its size and SHA-256. It opens a pull request in [arospkg-index](https://github.com/tomaszstaniak/arospkg-index), using your `gh` login and a fork when needed. Repeating the same submission does not open another pull request. This workflow has been tested on macOS and Linux; `submit --pr` has not been tested on Windows.

To preview an entry without opening a pull request:

```text
git clone https://github.com/tomaszstaniak/arospkg-index.git
python3 apkg-pack.pyz submit <download-url> --index arospkg-index --dry-run
```

You can send the resulting entry to a maintainer through another channel.

<a id="6-after-you-submit"></a>
## 5. Review and publication

The catalogue workflow checks metadata, download size and hash, archive paths, layout and executable CPU. A maintainer reviews the submission. After merge, the catalogue is regenerated; users see the package when they refresh their index.

**Listed** means the entry is published. **Reviewed** means the automatic checks passed for that archive and metadata. **Runtime tests** separately record what someone ran, on which system and when. Publication does not require a recorded runtime test, and an automatic review is not one. See the [review reference](metadata.md#listed-reviewed-and-runtime-tests).

<a id="7-the-next-release"></a>
## 6. The next release

Update the description kept beside your drawer:

- For a new program version, change `version` and remove `revision` or start it again at 1.
- For another build or packaging fix of the same version, increase `revision`.

Repeat the check, build, test and submit steps. Changed archive bytes require a new version or revision; `submit` refuses a different archive under an already published identity. apkg-pack does not change the version for you.

## Notes for maintainers

- For existing uploads without an embedded manifest, maintain an entry in `manifests/`; see the [complete example](packaging.md#4-the-manifest).
- A variant is (`id`, `arch`, `abi`). Use `<id>.<arch>.<abi>.toml`; legacy `<id>.<arch>.toml` filenames are still read.
- Put corrections in `overrides/` under the manifest's filename. Overrides may change any field except `id`, `arch`, `abi`, `url`, `size` and `sha256`, and survive the author's next submission.
- Record tests in `runtime-tests/<id>.<arch>.<abi>.toml`, with the tested archive's SHA-256. Only evidence matching the current archive is published. See the [runtime-test fields](metadata.md#runtime-tests).
- The workflow owns `review/`, `history.json`, `index.json`, `index-v2.json` and `index-v3.json`. Do not edit them by hand. Use the publish workflow's `review_all` option when the review rules or remote files need rechecking.
- If a file changes under a published URL, installs fail their integrity check. Publish the replacement under a new version or revision after checking it. If the entry cannot be repaired, set `excluded` to the reason.

For field types and limits, see the [metadata reference](metadata.md).
