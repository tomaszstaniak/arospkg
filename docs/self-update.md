# Updating apkg

```
apkg self-update --check
apkg self-update
```

## The first time

arospkg 0.3.2 and earlier do not have this command. Install the first
release that has it by hand, as before: unpack its archive and copy `apkg`
over the old one (to `C:`, or wherever you keep it). From then on,
`apkg self-update` updates it.

## Using it

`--check` shows the version you have, the file it was started from, and the
latest stable release for this machine. It needs the network to ask, and it
changes nothing: no file is written beside apkg.

`self-update` replaces the apkg you started, and only that file: if you run
`SYS:Tools/apkg self-update`, that copy is updated and `C:apkg` is not, and
the other way round. It shows the file and the version first. It:

1. reads the latest **stable** release from
   https://github.com/tomaszstaniak/arospkg/releases (release candidates are
   never used);
2. takes the archive for exactly this apkg's CPU and ABI. If the release has
   none, it says so and changes nothing: it never takes another target's
   build;
3. downloads it completely and checks it: the archive against the release's
   `SHA256SUMS`, apkg against the `BUILD.txt` in the archive, and that the
   new program is for this CPU and ABI;
4. writes the new program beside the old one as `apkg.new`, renames the old
   one to `apkg.old`, and the new one to `apkg`. If the last step fails, the
   old one is put back.

The apkg that runs `self-update` finishes as the old version; the new one
runs from the next command. What the checks prove: the archive is the one
the release's `SHA256SUMS` names, and the program is the one its `BUILD.txt`
names, for this CPU and ABI. Both files come from the same release as the
archive, so they guard against a damaged or wrong download, not against a
release that was itself replaced; the release is not signed.

The previous version stays as `apkg.old` beside the new one (one previous
version; an older `apkg.old` is replaced). Your packages, the catalogue and
`SYS:Packages` are not touched. While an install or another operation is
running, `self-update` refuses; try again when it has finished.

PkgManager is not updated by this. Take it from the release archive.

`update` and `upgrade` keep their meaning: `apkg update` refreshes the
package catalogue, `apkg upgrade <id>` updates an installed package.

## Going back to the previous version

```
Delete C:apkg
Rename C:apkg.old C:apkg
```

(with the path of the copy you updated instead of `C:apkg`).

## If an update was interrupted

An interruption before the swap (a broken download, a failed check) leaves
apkg as it was. If the machine stopped during the swap itself, you may find
`apkg.old` and `apkg.new` but no `apkg`. Either one is a complete, checked
program:

```
Rename C:apkg.old C:apkg       ; back to the version you had
Rename C:apkg.new C:apkg       ; or finish the update
```

## Limits

- A resident apkg (`Resident`) is not updated; start it from its file.
- It needs the network and the certificate bundle, as downloads do.
