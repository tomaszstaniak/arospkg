#!/usr/bin/env python3
"""Make the pictures of one package for the media drawer of an AROS system
(docs/rfc/0003, section 5), from ordinary PNG or JPEG files.

    make-media.py ID [--icon FILE] [--screenshot FILE]... [--caption TEXT]...
                  [--no-full] OUT_DRAWER

Writes into OUT_DRAWER/ID/:

    icon.png          64 x 64     the icon, centred on a transparent picture
    icon-small.png    24 x 24
    1.png             fits in 640 x 400     the first --screenshot
    1-thumb.png       fits in 160 x 100
    1-full.png        fits in 1280 x 800    (left out with --no-full)
    captions.txt      line N is the caption of N.png

For a distribution's author: OUT_DRAWER is the media drawer of the system
image, SYS:Packages/media with the default package root. PkgManager shows
these files for a package the catalogue has no pictures for; when the
catalogue gets pictures of its own, those take their place.

Every file is a truecolour PNG, as the catalogue's own are. A picture is
scaled down to fit and never enlarged. Up to 6 screenshots; the first
--caption belongs to the first --screenshot. Files already in
OUT_DRAWER/ID that this run does not write are left alone, except
screenshots past the ones given, which would be shown after them.

Needs Pillow (pip install pillow).
"""
import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import catalogue as cat  # noqa: E402
import catalogue_media as cm  # noqa: E402


def source(path):
    """A decoded picture, or a refusal that names the file."""
    try:
        return cm.decoded(Path(path).read_bytes())
    except SystemExit:
        raise
    except Exception as exc:
        sys.exit(f"{path}: not a picture this tool reads ({exc})")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("id", help="the package id, as the catalogue and apkg name it")
    ap.add_argument("--icon", metavar="FILE", help="the icon, PNG (an AROS PNG icon, a .info file, works too)")
    ap.add_argument("--screenshot", action="append", default=[], metavar="FILE",
                    help="a screenshot, PNG or JPEG; repeat for up to 6")
    ap.add_argument("--caption", action="append", default=[], metavar="TEXT",
                    help="the caption of a screenshot, in the same order: plain ASCII, at most 120 characters")
    ap.add_argument("--no-full", action="store_true", help="do not write N-full.png")
    ap.add_argument("out", metavar="OUT_DRAWER", help="the media drawer; the files go into OUT_DRAWER/ID")
    a = ap.parse_args()

    problems = []
    if not cat.ID_RE.match(a.id):
        problems.append(f"{a.id!r} is not a package id (lowercase letters, digits, '.', '_' and '-')")
    if not a.icon and not a.screenshot:
        problems.append("nothing to make: give --icon, --screenshot or both")
    if len(a.screenshot) > cm.SHOTS_MAX:
        problems.append(f"{len(a.screenshot)} screenshots; a client shows at most {cm.SHOTS_MAX}")
    if len(a.caption) > len(a.screenshot):
        problems.append(f"{len(a.caption)} captions for {len(a.screenshot)} screenshot(s)")
    for n, text in enumerate(a.caption, 1):
        why = cm.check_caption(text.strip(), for_index=False)
        if why:
            problems.append(f"caption {n}: {why}")
    if problems:
        sys.exit("\n".join(problems))

    # Everything is made before anything is written: a refused picture
    # must not leave half a drawer.
    files = {}
    if a.icon:
        try:
            made = cm.pictures_of_icon(source(a.icon))
        except ValueError as exc:
            sys.exit(f"{a.icon}: {exc}")
        files["icon.png"], files["icon-small.png"] = made["icon"], made["icon-small"]
    for n, path in enumerate(a.screenshot, 1):
        data = Path(path).read_bytes()
        try:
            made = cm.shot_pictures(data if data[:8] == cm.PNG_SIG else None, source(path), full_box=cm.SHOT_MAX)
        except ValueError as exc:
            sys.exit(f"{path}: {exc}")
        files[f"{n}.png"], files[f"{n}-thumb.png"] = made["view"], made["thumb"]
        if not a.no_full:
            files[f"{n}-full.png"] = made["full"]
    captions = [c.strip() for c in a.caption]
    if any(captions):
        files[cm.CAPTIONS] = ("\n".join(captions) + "\n").encode("ascii")

    drawer = Path(a.out) / a.id
    drawer.mkdir(parents=True, exist_ok=True)
    if a.screenshot:
        stale = [f"{n}{kind}.png" for n in range(1, cm.SHOTS_MAX + 1) for kind in ("", "-thumb", "-full")]
        for name in stale + [cm.CAPTIONS]:
            if name not in files:
                (drawer / name).unlink(missing_ok=True)
    for name, data in files.items():
        (drawer / name).write_bytes(data)
        if name.endswith(".png"):
            w, h, _, _ = cm.png_info(data, whole=False)
            print(f"written {drawer / name}: {w} x {h}, {len(data)} bytes")
        else:
            print(f"written {drawer / name}")


if __name__ == "__main__":
    main()
