arospkg 0.8.0 has a new PkgManager window and can show pictures of packages.

## What's new

- **A new window.** The left column has a **Search** field and a list of views: **All applications**, **Installed**, **Updates**, **Recently added** and **Last refresh**, followed by the categories of the catalogue. One row is chosen at a time. When there are updates, their number stands beside **Updates**.
- **One button per package.** Each row of the list has the package's icon, its name, its summary and a button that says what a click does: **Install**, **Run** for an installed package, or **Upgrade** when a newer version or revision can be installed. Under the list, a line shows the selected package's version, state and size, and anything that needs attention, with **Open folder** and **Details...**.
- **A page for each package.** **Details...**, or a double click on a row, opens the package's page in the same window: icon, name, summary, the main button, **Open folder** and **Remove**, a strip of screenshots with **Previous** and **Next**, and two tabs. **About** has the package's notes, what it needs, recommends, conflicts with and replaces, its source and when it was added. **Details** has its location, how it starts, compatibility, the requirements probed on this machine, the catalogue's review status and runtime tests, and the last **Verify** result. **Back** returns to the list.
- **A menu.** Project (**About...**, **Quit**), Catalogue (**Update index**), Package (**Details...**, **Install**, **Run**, **Upgrade**, **Roll back**, **Verify**, **Open folder**, **Remove**).
- **The catalogue is fetched for you.** When PkgManager starts and there is no catalogue, or it is more than two weeks old, PkgManager fetches it.
- **Pictures of packages.** A package can have an icon, shown in the list and on its page, and up to six screenshots with captions, shown on its page. Pictures are kept in the `media` drawer of the package root (`SYS:Packages/media/<id>/`). The catalogue can name an archive of icons, fetched with the catalogue, and screenshots per package, fetched the first time the package's page is opened; every file is checked against the size and SHA-256 the catalogue states. PkgManager reads the PNG files itself and does not need the picture datatype for them.
- **Pictures from a distribution.** Files put into the media drawer are shown for packages the catalogue has no pictures for. `tools/make-media.py` in the source repository makes them from ordinary PNG or JPEG files (it needs Python with Pillow). See [Pictures](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/using.md#pictures).
- **For package authors:** `apkg-pack submit --screenshot FILE --caption TEXT` (repeatable) puts screenshots into the catalogue pull request. See the [authoring guide](https://github.com/tomaszstaniak/arospkg/blob/main/docs/guide/authoring.md#screenshots).

## Changed

- **Verify** and **Roll back** are in the Package menu only. **Remove** is on the package's page and in the menu.
- **Update index** is in the Catalogue menu (Right Amiga+U); the button is gone from the window.
- The two selectors above the list (categories and state) are replaced by the list on the left. There is no "Not installed" view.
- The Version and State columns are gone from the list; version and state are in the line under the list and on the package's page. In **Recently added** and **Last refresh**, the summary starts with the kind of news: added, new build, updated, new or back.
- The window has one status line at the bottom: the catalogue's date, whether updates are available, and what the list shows. The progress bar with **Cancel** is shown only while an operation runs.

## Removed

- The demonstration of 0.5.0 to 0.7.8: the archives have no `Demo` drawer, and the **Demo store** view, the **Buy...** window and the `--demo` option are gone. After an update, `apkg` and PkgManager delete the `Demo` drawer beside them the first time they are started.

## Updating

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Mainline x86_64 and Raspberry Pi builds remain experimental. Checksums are in `SHA256SUMS`.

## Known limitations

- The public catalogue does not publish pictures yet. Icons and screenshots appear in PkgManager once it carries them; until then only pictures a distribution put into the media drawer are shown.
- The window installs or upgrades one package at a time. `apkg upgrade --all` in the Shell upgrades every package that can be, one after the other.
