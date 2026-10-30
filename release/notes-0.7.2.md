arospkg 0.7.2 makes web addresses in PkgManager clickable and copyable.

## New

- Every `http://` and `https://` address PkgManager shows is a link: the home page and the release page in **About**, a package's source and catalogue addresses, addresses in its notes and picture credits, the confirmation window and messages such as a failed download. Click one to pass it to the browser chosen in Prefs/OpenURL. **Copy** beside it puts the whole address on the clipboard.
- A long address is shortened on screen only; its bubble help, the browser and the clipboard get it whole. With more than four addresses, **Previous** and **Next** turn to the others.
- Addresses from the catalogue's fields are used exactly as written. In package notes an address ends before a final `.`, `,`, `:`, `;`, `!`, `?` or `'`; package authors write `<address>` when one really ends with such a character.

## Limitations

- An address longer than 480 characters can only be copied: AROS cannot hand a longer one to a browser. The same goes for an address with `'`, `*`, `;` or `$`, which would act as Shell or ARexx syntax when the browser is started.
- Without openurl.library, or with no browser enabled in Prefs/OpenURL, a click says that the address could not be opened, and **Copy** still works. On mainline AROS, enable a browser in Prefs/OpenURL first.
- `ftp://`, `mailto:` and other addresses stay plain text.

apkg `show` now prints a source address longer than 255 characters whole; otherwise apkg is unchanged.

## Updating

From 0.7.0 or 0.7.1:

```text
apkg self-update
```

or **About** > **Check for a newer version** > **Update arospkg** in PkgManager.

### From 0.4.x to 0.6.0

These versions can only replace apkg itself. Run the update twice:

```text
apkg self-update
apkg self-update
```

The first run installs the new apkg and leaves PkgManager as it was; the second, now with the new apkg, updates PkgManager. If you run the copy in `C:`, tell the second run which drawer it belongs to:

```text
apkg self-update --drawer "Work:arospkg-0.6.0.x86_64-aros-v11"
```

Close PkgManager before you start.

From 0.3.x: unpack the archive and copy `apkg` to `C:`. Keep your existing `SYS:Packages`.

## Downloads

| Build | Included | Status |
|---|---|---|
| [x86_64 ABIv11 / AROS One](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.2/arospkg-0.7.2.x86_64-aros-v11.zip) | apkg + PkgManager | Stable |
| [x86_64 ABIv1 / mainline](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.2/arospkg-0.7.2.x86_64-aros-v1.zip) | apkg + PkgManager | Experimental |
| [Raspberry Pi (native AROS, aarch64)](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.2/arospkg-0.7.2.aarch64-aros-raspi.zip) | apkg + PkgManager | Experimental |
| [i386 ABIv0](https://github.com/tomaszstaniak/arospkg/releases/download/v0.7.2/arospkg-0.7.2.i386-aros-v0.zip) | apkg | Experimental |

Pick the archive for your CPU and system: an ABIv11 build does not run on mainline, and the reverse. Self-update takes only the archive for the system it runs on; the Raspberry Pi build takes only the Raspberry Pi archive.

On the Raspberry Pi, as elsewhere, self-update needs a working network connection; without one, install the archive by hand. A checksum failure reported for one download on a Pi is still unexplained; `--keep-rejected` keeps such a download for a report.

[Updating arospkg](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md) · [User guide](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/using.md)

Checksums are in the attached `SHA256SUMS`.
