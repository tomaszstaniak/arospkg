arospkg 0.7.5 shows web addresses where they occur in PkgManager's text.

Links now stand inside a package's details, About, confirmation windows and messages, in place of the separate rows of addresses with their Copy buttons and Previous/Next.

- Click a link to open it in the browser chosen in Prefs/OpenURL. The right mouse button offers **Open link** and **Copy link address**.
- Dragging over text selects it without opening anything; Right Amiga+C copies the selection.
- With the keyboard: Tab to the text, Left and Right move from link to link, Return opens, Right Amiga+C copies the link, Tab leaves.
- A long address is shortened on screen. Its bubble help, opening and copying use the whole address. An address longer than 480 characters, or one containing `'`, `*`, `;` or `$`, can only be copied.

The text view is built into PkgManager, so no Zune class needs to be installed or updated. apkg is unchanged apart from its version. This release also brings back the i386 archive that 0.7.4 lacked.

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
