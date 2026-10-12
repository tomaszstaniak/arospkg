Package management for AROS, with the apkg command-line tool and PkgManager desktop application. 0.5.0 closes the user's path: find an application, install it, **start it**, check for updates, update it, and roll it back or remove it.

## Downloads

| Target | Included | Status |
|---|---|---|
| [x86_64 ABIv11 (AROS One)](https://github.com/tomaszstaniak/arospkg/releases/download/v0.5.0/arospkg-0.5.0.x86_64-aros-v11.zip) | apkg and PkgManager | **stable** |
| [i386 ABIv0](https://github.com/tomaszstaniak/arospkg/releases/download/v0.5.0/arospkg-0.5.0.i386-aros-v0.zip) | apkg | tested under QEMU, see below |
| [x86_64 ABIv1, mainline](https://github.com/tomaszstaniak/arospkg/releases/download/v0.5.0/arospkg-0.5.0.x86_64-aros-v1.zip) | apkg and PkgManager | experimental |
| [Raspberry Pi, aarch64 ABIv1](https://github.com/tomaszstaniak/arospkg/releases/download/v0.5.0/arospkg-0.5.0.aarch64-aros-raspi.zip) | apkg and PkgManager | experimental: compiled and linked, **not run on a Pi** |
| [Screenshots](https://github.com/tomaszstaniak/arospkg/releases/download/v0.5.0/arospkg-0.5.0-v11-screenshots.zip) | the gallery below, as files | |

**Raspberry Pi users: please test.** Both Pi binaries compile and link with Clang 20.1.0 and the native Pi SDK; neither was run on a Pi, emulated or physical (the test machine's Pi emulation has no working network or input medium). `apkg self-update` on a Pi now offers this build; it updates only apkg. Please report what works and what does not at https://github.com/tomaszstaniak/arospkg/issues. The Folio checksum failure reported on a Pi is not explained; `apkg --keep-rejected install folio` keeps such a download for a report.

[0.5.0 screenshots on AROS One](https://github.com/tomaszstaniak/arospkg/blob/main/docs/screenshots/0.5.0/README.md).

## What's new in 0.5.0

- **Start installed applications.** `apkg run <id> [files...]` and PkgManager's **Run** start an application the way its package describes:
  - a **Workbench** program as a double-click on its icon starts it: with the icon's stack and tool types, in its own drawer, so Folio and GrafX2 (which need 8 MiB of stack) start without a `Stack` command. Files after the name are passed as Workbench arguments: `apkg run folio Work:manual.pdf` opens the PDF.
  - a **Shell** program in your Shell window, in the directory the package names, with your arguments; apkg returns its return code. PkgManager gives it a console window of its own.
  - `apkg run --check <id>` (and `--json`) says whether and how it would start, and why not.
  - Before starting, apkg checks the description, the paths (nothing outside the application's drawer), the program file's CPU, the icon a Workbench start needs and the libraries the package names. A start is reported only when it was seen: workbench.library loads programs on its own and reports no failure, so apkg waits for the program's process.
  - The description is `application.json` in the program's drawer ([RFC 0002](https://github.com/tomaszstaniak/arospkg/blob/main/docs/rfc/0002-application-folder.md)). It travels with the drawer, so `apkg run "Work:Apps/Folio"` also starts an application you unpacked or moved yourself. A package without one is installed and opened as before: Run says so, and Open folder remains.
  - In the catalogue, Folio, GrafX2, grep and arospkg itself describe how they start.
- **PkgManager**: Run, **Verify** (the result at the top of the details), the files a removal kept, the start information in a header that stays in view, installed packages listed even without a readable catalogue, and versions written as everywhere else (`0.4.1-aros2`). ARexx: `RUN` and `VERIFY`.
- **Demonstration only** — these show a possible direction and are marked as such in the window:
  - pictures beside the details: an application's icon, a screenshot and thumbnails, with a line saying where they come from. In 0.5.0 they are a few **real screenshots shipped in PkgManager's Demo drawer**, not pictures from the catalogue;
  - a **Demo store** with one fictional paid application. **"Demo -- purchasing is not available yet"**: Buy shows how a purchase could work, nothing can be bought or installed from it, and no data is asked for or sent. The proposed metadata is in [RFC 0003](https://github.com/tomaszstaniak/arospkg/blob/main/docs/rfc/0003-storefront-metadata.md).
- **For authors**: `apkg-pack` checks the new `[launch]` table against the drawer (the program, its CPU, a tool icon for a Workbench start, classic and PNG icons, the stack the icon sets) and writes `application.json` into the archive.
- **Catalogue for old clients**: `index.json`, which clients up to 0.3.2 read, now carries only the fields they use, always lists arospkg itself (their way to a current version), and is kept under 80% of what they can hold.
- A Ctrl-C while connecting or during the TLS handshake is reported as cancelled, not as a network error.

## Install or upgrade

On 0.4.x:

```text
apkg self-update
```

This replaces only apkg. **Take PkgManager from the archive**: unpack it and start PkgManager from the unpacked drawer; About > Check for a newer version tells you when there is a newer one.

From 0.3.x, or by hand: download and unpack the archive, close PkgManager, copy `apkg` to `C:`, then run `apkg --version` and `apkg update`. Keep your existing `SYS:Packages`.

## Test coverage

TEST-COVERAGE

Use the `SHA256SUMS` from this release to verify your download.
