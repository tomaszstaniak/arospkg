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

All under QEMU (TCG); **nothing in this release was run on real hardware.** Evidence: [docs/evidence/2026-10-12-050](https://github.com/tomaszstaniak/arospkg/blob/main/docs/evidence/2026-10-12-050/README.md).

**x86_64 ABIv11, AROS One 1.3** (pool slot v11-2), with the archive's binaries (apkg `e7432440…`, PkgManager `28cc388e…`):

- `apkg run`: Folio (with a PDF) and GrafX2 started from their icons with their 8 MiB stack from a 40960-byte Shell; grep as a Shell command, its return code passed on (0 and 5); a program in a drawer with spaces, arguments with `"` and `*`; a Workbench tool with a tool type and a document; and the refusals: no description, missing program, a path with `..`, a description for i386, an i386 program file, a Workbench start without an icon, an unknown key, not installed. A package installed from a catalogue without a launch table still installs; Run says it does not say how to start it.
- The main path: install Folio, start it by `apkg run` and by PkgManager's Run, open a PDF, upgrade to a controlled test revision (the same program repackaged as revision 2, not published) with the plan shown first and a changed README kept, roll back (the start description is restored), Verify (a user's file reported as not the package's), Remove (changed and added files kept and listed). GrafX2 and grep through the same window.
- The 0.4.2 regression suite on the release apkg, with the same results as in 0.4.2: `outdated` (text, `--all`, `--plain`, `--json`, redirected), the selection of CPU and ABI, every update state, `upgrade --all`, its dry run and report, a conflicting user file, rollback and recovery, no catalogue and an unreadable one, a root with a space.
- Ctrl-C: during the catalogue's connection (now "cancelled", not a network error), during a package download (nothing left in the cache), and while the first package of `upgrade --all` was being installed (it finished, the second did not start, exit 10).
- PkgManager: Installed and Updates filters, the pictures panel with switching thumbnails, an application without pictures, the Demo store and the purchase preview, ARexx RUN.

**Limpet 0.3.0 on ABIv11 2026.09** (pool slot v11-3), with the release apkg, compared with the system console: colours and word-wrapping in narrow and wide windows, redirected output byte-identical to the console's, `--json` output without escape codes, Ctrl-C during the catalogue download with one prompt afterwards, `apkg run grep`, Ctrl-C into it. The double prompt reported with 0.4.2 came from Limpet 0.2.1 and is fixed in Limpet 0.3.0. One small fault: in a narrow window the summary line of `apkg update` is broken mid-word.

**i386 ABIv0** (pool slot v0-2), apkg built from the release source: an install over the network from AROS Archives (DirTree, whose program is in a sub-drawer), and `apkg run` in Shell and Workbench mode.

**x86_64 ABIv1, mainline nightly 2026-09-23** (pool slot v1-2), the archive's binaries: PkgManager starts and lists a catalogue; `apkg run` starts a mainline tool and refuses an ABIv11 one with the reason. **Not shown: an install over the network** — this test slot has no network configured, and the slot that has one was in use by another session for the whole run. 0.4.2 fetched the public catalogue on mainline.

**Raspberry Pi**: compiled and linked only. The self-update target of a Pi build was checked on the host against this release's `SHA256SUMS`: it selects `arospkg-0.5.0.aarch64-aros-raspi.zip`.

Known limits: a Shell program started from PkgManager keeps PkgManager's drawer in use until it ends (AROS's SystemTagList copies the caller's home directory); a Workbench program that ends within a fraction of a second is reported as not seen; Remove asks no confirmation (as before) and keeps your files.

Use the `SHA256SUMS` from this release to verify your download.
