# Updating apkg

```
apkg self-update --check
apkg self-update
```

`--check` shows the version you have, the file it was started from, and the
latest stable release for this machine. It changes nothing.

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
- The release's checksums come from the same place as the release; the
  release is not signed.
