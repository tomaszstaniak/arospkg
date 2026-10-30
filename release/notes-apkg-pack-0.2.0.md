**apkg-pack 0.2.0: build packages for arospkg**

`apkg-pack` checks your application folder, creates a ZIP with its manifest, and submits published packages to the arospkg catalogue.

## What's new

- **How the program starts.** A `[launch]` table says how `apkg run` and PkgManager's **Run** start the program (from its icon, or as a Shell command). `check` reads the icon and reports the stack it sets, and `build` puts the same description into the archive as `application.json`.
- **Relations.** A package can depend on other packages (with a minimum version), recommend them, conflict with them and replace them. Entries that use relations are published for apkg 0.6 and later only.
- **Larger files.** One file in a package may be up to 128 MiB unpacked, as apkg accepts since 0.6.0. Entries with a file over 64 MiB are published for apkg 0.6 and later only.
- **Programs stay executable.** AROS programs (ELF files) keep their execute bit in the archive, so they start after an unpack by hand too.
- The checks are the ones the catalogue now uses for its automatic review.

## Use

Download `apkg-pack.pyz`, install Python 3.11 or newer, and follow the [authoring guide](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/authoring.md).

```text
python3 apkg-pack.pyz init build/MyApp
python3 apkg-pack.pyz check build/MyApp
python3 apkg-pack.pyz build build/MyApp --output myapp-1.0.x86_64-aros-v11.zip
python3 apkg-pack.pyz submit <download-url> --pr
```

Packing needs no GitHub account or network. `build` packs, it does not compile, and `check` does not test the program on AROS.

Known limit: `submit --pr` is not tested on Windows yet.

This release contains only the author tool. The apkg client and PkgManager are in [arospkg 0.7.0](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.7.0).

Checksums are in the attached `SHA256SUMS`.
