---
title: "RFC 0003: pictures in the catalogue (icons and screenshots)"
status: implemented
created: 2026-10-12
revised: 2026-11-23
---

# RFC 0003: pictures in the catalogue

| | |
|---|---|
| **Status** | **Implemented in the client (arospkg 0.8.0) and in the catalogue tools. The public catalogue does not publish pictures yet.** Sections 1 to 8 describe what is implemented. Section 9 (offers) is a note for later and is not built. |
| **Replaces** | The demonstration of PkgManager 0.5.0 to 0.7.8 (`Demo/storefront.json`), which is removed. Section 7. |
| **Companion** | [RFC 0002](0002-application-folder.md), the application folder |

## 1. What this is for

A package in the catalogue can have an icon and screenshots. PkgManager
shows the icon in its list and both on the page of a package; `apkg`
in a terminal that can draw pictures (Limpet) can use the same files
later. Pictures are never required: a package without them is listed and
installed as before.

Three people can supply pictures, and none of them needs an account or a
server of their own:

- **Nobody**, for icons: the catalogue takes the icon from the package's
  archive.
- **The packager**, for screenshots: PNG files added to the catalogue
  repository in the same pull request as the manifest.
- **A distribution's author**, for anything the catalogue lacks: PNG files
  put into the media drawer of the system they ship. Section 5.

## 2. What the packager does

Screenshots sit in the catalogue repository, beside the manifests, and
belong to the application, not to one build of it:

```
gallery/<id>/1.png
gallery/<id>/2.png
gallery/<id>/captions.txt          # optional: line N is the caption of entry N
```

| rule | value |
|---|---|
| format | PNG, truecolour |
| count | at most 6 |
| size | at most 1280 x 800 pixels and 512 KiB each |
| caption | plain ASCII, at most 120 characters |
| content | a real screenshot of that program on AROS |

Screenshots are not put into the package's archive. A published archive
is never changed, so pictures in archives would need a new revision of
every package, and a user would have to download a program to see what it
looks like.

`apkg-pack submit` takes `--screenshot <file>` and `--caption <text>`,
both repeatable and in the same order, which copy the files into place in
the pull request. The screenshots given are the application's whole
gallery: they take the place of the ones the catalogue has.

Icons need nothing: see section 3.

## 3. What the catalogue does

The `check` workflow refuses a pull request whose screenshots break the
rules of section 2.

The `publish` workflow, which already downloads every archive it reviews:

1. takes the package's icon from the archive: the icon of the program
   `launch` names, else the drawer icon the manifest names (`icon`). When
   that file is a PNG icon it is written as `icon.png` (64 x 64) and
   `icon-small.png` (24 x 24), truecolour;
2. scales each screenshot to the sizes the clients show, keeping its
   proportions: `view` fits in 640 x 400, `thumb` in 160 x 100; the
   original is kept as `full`;
3. writes the files to the branch `media` of the catalogue repository,
   under names that contain the start of their SHA-256, so an address
   never serves different bytes;
4. packs every `icon.png` and `icon-small.png` into one archive,
   `icons-<start of its SHA-256>.zip`, with the paths `<id>/icon.png` and
   `<id>/icon-small.png`, so a client gets the icons of the whole
   catalogue in one download;
5. writes what it made into `index-v3.json` (section 4).

An application with builds for several systems has one set of pictures.
The icon comes from the first of its builds in a fixed order (x86_64
ABIv11, x86_64 mainline, AArch64).

## 4. Fields in `index-v3.json`

One `media` object per application id, repeated in each of its builds:

```json
"media": {
  "icon":       {"url": "https://…/media/folio/3fa9c1d2-icon.png",       "sha256": "…", "size": 4211, "width": 64, "height": 64},
  "icon_small": {"url": "https://…/media/folio/9b20e4aa-icon-small.png", "sha256": "…", "size": 1502, "width": 24, "height": 24},
  "gallery": [
    {"type": "screenshot",
     "caption": "Two-page spread with search results",
     "full":  {"url": "…", "sha256": "…", "size": 61234, "width": 1280, "height": 800},
     "view":  {"url": "…", "sha256": "…", "size": 20110, "width": 640,  "height": 400},
     "thumb": {"url": "…", "sha256": "…", "size": 3120,  "width": 160,  "height": 100}}
  ]
}
```

The catalogue itself (the top level of `index-v3.json`) names the icon
archive:

```json
"media_bundle": {"url": "https://…/media/icons-5c0e77a1.zip", "sha256": "…", "size": 1048576}
```

The gallery is a list of typed entries so that it can hold more than
screenshots later. Today the only type is `screenshot`. A client skips an
entry whose type it does not know, so a later type, such as a short video
with a still picture to show in its place, can be added without breaking
the clients of this RFC. Video is not designed here: it needs a decision
on format, size and what plays it on AROS.

`index-v2.json` and `index.json` never carry `media`. Clients before this
RFC read `index-v3.json` and ignore the object.

A client reads only the addresses and hashes, so it does not matter who
made the pictures. A catalogue built another way can fill the same object:
for example from the `Icon:` and `Screenshot:` fields of a signed
manifest, which name pictures inside the package.

## 5. The media drawer

Pictures live in one drawer of the package root, beside `cache`:

```
<root>/media/<id>/icon.png
<root>/media/<id>/icon-small.png
<root>/media/<id>/1.png            # `view`, the size a package's page shows
<root>/media/<id>/1-thumb.png
<root>/media/<id>/1-full.png       # optional
<root>/media/<id>/captions.txt     # optional
<root>/media/<id>/gallery.sha256   # written by the client: the hashes of the gallery it fetched
<root>/media/icons.sha256          # written by the client: the hash of the icon archive it unpacked
```

With the default root this is `SYS:Packages/media/`. `apkg` and
PkgManager create it with the package root, as they create `cache`.

How a client fills and reads it:

- The icon archive is fetched after the catalogue, when its hash is not
  the one unpacked last (`<root>/media/icons.sha256`), and unpacked into
  this drawer. Its icons do not replace the files of a package the
  catalogue has no entry for.
- The gallery of a package is fetched the first time its page is opened.
- Every download is made off the window's task, with the size and
  SHA-256 checks an archive gets. A file that fails a check is deleted
  and not shown.
- A picture already there with the hash the catalogue names is used
  without any network.
- For a package the catalogue has **no** pictures for, the files in the
  drawer are shown as they are, after the checks of the picture's header
  (PNG, the sizes above). This is how a distribution adds pictures.
- When the catalogue later gets pictures for that package, they replace
  the local ones.

**For a distribution's author.** Fill `<root>/media/` in the system
image. Either copy the drawer from a machine that has shown every
package (then no user of the distribution needs the network to see
pictures), or make pictures of your own with

```
tools/make-media.py ID [--icon FILE] [--screenshot FILE]... [--caption TEXT]... [--no-full] OUT_DRAWER
```

which writes every size and name above into `OUT_DRAWER/ID/` from
ordinary PNG or JPEG files. `--no-full` leaves out `N-full.png`. The tool
needs Python with Pillow.

## 6. Limits a client keeps

- Pictures never block a button and are never needed for an operation.
- No picture is decoded before its header was checked against the limits.
- At most 6 screenshots per package; more in a drawer are ignored.
- A package with no icon shows the text row it shows today.

## 7. The demonstration is removed

PkgManager 0.5.0 to 0.7.8 shipped `Demo/` (pictures for two programs and
one fictional paid application). It goes:

- the release archives no longer contain `Demo/`;
- `apkg` and PkgManager delete the `Demo` drawer beside them when they
  are started, when it is that demonstration (it holds
  `storefront.json`). An update is carried out by the version being
  replaced, so the new programs do this themselves. A drawer of the same
  name without that file is left;
- the *Demo store* filter, the *demo offer* rows, the purchase preview
  window and the `--demo` option are removed;
- `tools/make-demo-store.py` becomes `tools/make-media.py`.

## 8. The window

PkgManager shows pictures in two places:

- **The list**: `icon-small` (24 x 24) in the first column. A package
  without one has an empty place there, so the names stay in line.
- **The page of a package**, opened by **Details...** or by a double click
  on a row, in the same window, with a button back to the list: `icon`
  (64 x 64) in the header beside the name and summary, and under the
  header a strip of the screenshots that scrolls sideways, with
  **Previous** and **Next** and the caption of the picture in view. The
  strip is hidden when the package has no screenshots.

The line under the list, for the selected package, has no pictures.

The strip shows each picture at 352 x 220, 480 x 300 or 640 x 400,
depending on the height of the screen (below 700 lines, from 700, from
900), scaled down from `view`.

PkgManager decodes the PNG files itself (8-bit truecolour, not
interlaced) and does not use the picture datatype for them.

`thumb` is fetched with `view`, but this version of PkgManager does not
show thumbnails. `full` is not fetched or shown by this version; both are
kept for a later one and for other clients.

## 9. Later: offers

A catalogue that also lists applications for sale stays a direction, not
a plan. It needs a portal that knows sellers and buyers, which this
project does not run; it would be designed together with the portal's
maintainer. Nothing in the catalogue or the clients carries an offer, and
the fields of the earlier revision of this RFC (`[offer]`: price,
currency, seller, terms, purchase address) are kept here only as a
starting point for that design.
