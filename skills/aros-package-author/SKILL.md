---
name: aros-package-author
description: Guide the author of an AROS program through packing an already built application drawer with apkg-pack (init, check, build), and, only when asked, submitting the uploaded ZIP to the arospkg catalogue. Use when someone wants to publish, package or submit an AROS program for apkg or PkgManager.
---

# Packing an AROS program with apkg-pack

You help an author turn a program they have already built into an arospkg
package. `apkg-pack` does the work and holds every rule; you run it, read its
output and ask the author for what it needs. The manual equivalent of every
step is in https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/authoring.md.

## Rules

- Run `apkg-pack`. Never write the ZIP, the manifest or a checksum yourself,
  and never restate or second-guess its validation rules.
- Never guess the ABI (`v11`, `v1`, `v0`), dependencies, system libraries,
  licence or test results. The host computer's CPU says nothing about the
  target. If the author does not know, say so and stop that field.
- Ask only for what `apkg-pack` reports as missing or what `init` cannot read.
- Read only the built drawer, its icon and its `.arospkg.toml`. Do not read,
  upload or send source code or other project files. The author's
  repository may be private; do not ask to make it public.
- Uploading and `submit` happen only after the author explicitly asks for
  them in this conversation. `submit --dry-run` is a preview and is fine.
- When `apkg-pack` fails, fix the cause (a manifest field, a stray file in
  the drawer) and run the same step again. Do not work around a refusal by
  hand, by editing the archive or by skipping the step.
- `post_install_notes` say only what this program needs after installation
  (data to supply, a first-start choice). apkg already says where it was
  installed and how to start it.
- Passing `check` means the package is well formed, not that the program
  runs. Say that when you report the result.

## Steps

1. Find the tool: `apkg-pack --version`, or `python3 apkg-pack.pyz --version`
   (`py` on Windows). If neither exists, point the author to the Install
   section of the guide; it needs Python 3.11 or later.
2. Find the built drawer and its icon beside it. Ask if there are several
   candidates. `build` packs; it does not compile. If there is no build yet,
   the author builds first.
3. If `<Drawer>.arospkg.toml` does not exist beside the drawer, run
   `apkg-pack init <drawer>` and answer its questions with the author's
   answers. For a script, pass the answers as options with
   `--non-interactive`.
4. `apkg-pack check <drawer>`; fix and repeat until it prints `ok:`.
5. `apkg-pack build <drawer> --output <id>-<version>.<arch>-aros-<abi>.zip`,
   then `apkg-pack check` on that ZIP. Report the path, size and SHA-256 it
   printed.
6. Stop here unless asked to continue. Tell the author to test the program
   on its target AROS and to upload the ZIP unchanged where they normally
   publish (AROS Archives or a GitHub release). Uploading alone does not
   change the catalogue.
7. When the author gives the public HTTPS URL and asks to submit:
   `apkg-pack submit <url> --pr` (needs `gh auth login`). Without GitHub:
   `apkg-pack submit <url> --index <arospkg-index clone> --dry-run` and the
   author sends the entry to the maintainers.

For the next release: set `version` to the new upstream version and remove
`revision`, or raise `revision` for a new package of the same version. Then
repeat from step 4.
