#!/usr/bin/env python3
"""Pictures in the catalogue (docs/rfc/0003): the rules for gallery/ in
arospkg-index, and the files the publish workflow makes from it and from
the icons inside the package archives.

    gallery/<id>/1.png ... 6.png, captions.txt      what a packager adds
    <out>/<id>/<sha8>-icon.png, -icon-small.png     what publication makes,
    <out>/<id>/<sha8>-<n>-full.png, -view, -thumb   laid out as it is served
    <out>/icons-<sha8>.zip                          every icon, one download
    <out>/_sources/<key>.json                       which archive gave which icon

The rules (check_gallery, check_picture, check_caption) need only Python,
because apkg-pack and the check of a pull request apply them and neither
has more. Making pictures needs Pillow, imported when the first picture is
made.

Every generated picture is a truecolour PNG, 8 bits a channel, with or
without alpha: the AROS picture datatype does not read palette PNG files.
"""
import hashlib
import io
import json
import re
import zipfile
import zlib
from pathlib import Path

PNG_SIG = b"\x89PNG\r\n\x1a\n"
SHOTS_MAX = 6
SHOT_MAX = (1280, 800)
FILE_MAX = 512 * 1024
CAPTION_MAX = 120
ICON, ICON_SMALL = (64, 64), (24, 24)
VIEW, THUMB = (640, 400), (160, 100)
# An icon is decoded only below this: a .info file is the packager's, and a
# few bytes of PNG can name a picture of gigabytes.
ICON_SOURCE_MAX = (1024, 1024)
CAPTIONS = "captions.txt"
SOURCES = "_sources"
# Part of the key of a _sources record: raise it when icons are made
# differently, so every archive is read again instead of an old result
# being taken for the new one.
ICON_GENERATION = 2
# The order in which the builds of one application are asked for its icon.
BUILD_ORDER = (("x86_64", "v11"), ("x86_64", "v1"), ("aarch64", "v1"))
STAMP = (1980, 1, 1, 0, 0, 0)
SHOT_RE = re.compile(r"^([1-9][0-9]?)\.png$")
PRINTABLE = re.compile(r"^[\x20-\x7e]*$")
# What json.dumps writes without an escape: the client does not decode
# escapes (mkindex.PLAIN), and a caption goes into index-v3.json.
PLAIN = re.compile(r'^[\x20-\x21\x23-\x5b\x5d-\x7e]*$')
CHANNELS = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}
DEPTHS = {0: (1, 2, 4, 8, 16), 2: (8, 16), 3: (1, 2, 4, 8), 4: (8, 16), 6: (8, 16)}


# ------------------------------------------------------------------ rules

def png_info(data, whole=True):
    """(width, height, bit depth, colour type) of a PNG, or ValueError.

    The chunks are walked and their checksums compared, so a file cut short
    or damaged is refused here and not by a decoder later. whole: nothing
    may follow the picture, and its data must unpack to exactly the size
    the header states. An AROS icon is read with whole=False: it has a
    second picture after the first, and only the header is needed."""
    if data[:8] != PNG_SIG:
        raise ValueError("not a PNG file")
    i, head, idat, ended = 8, None, [], False
    while i + 12 <= len(data):
        n, tag = int.from_bytes(data[i:i + 4], "big"), data[i + 4:i + 8]
        body = data[i + 8:i + 8 + n]
        crc = data[i + 8 + n:i + 12 + n]
        if len(body) != n or len(crc) != 4:
            raise ValueError("cut short")
        if zlib.crc32(tag + body) != int.from_bytes(crc, "big"):
            raise ValueError(f"damaged (checksum of the {tag.decode('latin-1')} chunk)")
        i += 12 + n
        if head is None:
            if tag != b"IHDR" or n != 13:
                raise ValueError("no PNG header")
            w, h = int.from_bytes(body[:4], "big"), int.from_bytes(body[4:8], "big")
            depth, ctype = body[8], body[9]
            if not w or not h or depth not in DEPTHS.get(ctype, ()) or body[10] or body[11] or body[12] > 1:
                raise ValueError("a PNG header no reader accepts")
            head = (w, h, depth, ctype, body[12])
            if not whole:
                return head[:4]
        elif tag == b"IDAT":
            idat.append(body)
        elif tag == b"IEND":
            ended = True
            break
    if head is None or not ended:
        raise ValueError("cut short")
    if i != len(data):
        raise ValueError(f"{len(data) - i} bytes after the end of the picture")
    w, h, depth, ctype, interlace = head
    if w > 1 << 15 or h > 1 << 15:
        return head[:4]             # refused by its size; not worth unpacking
    line = (w * CHANNELS[ctype] * depth + 7) // 8
    size = h * (1 + line)
    # An interlaced picture is stored in seven passes, each line with its
    # own filter byte: never more than this.
    limit = size if not interlace else size + 8 * h + 64
    unpack = zlib.decompressobj()
    try:
        raw = unpack.decompress(b"".join(idat), limit + 1)
    except zlib.error as exc:
        raise ValueError(f"picture data does not unpack ({exc})")
    if not unpack.eof or len(raw) > limit or (not interlace and len(raw) != size):
        raise ValueError("picture data is not the size its header states")
    return head[:4]


def check_picture(data):
    """Why these bytes cannot be a screenshot of the catalogue; [] when they can."""
    if len(data) > FILE_MAX:
        return [f"{len(data)} bytes; at most {FILE_MAX} (512 KiB)"]
    try:
        w, h, _, _ = png_info(data)
    except ValueError as exc:
        return [f"{exc}; a screenshot is a PNG file"]
    if w > SHOT_MAX[0] or h > SHOT_MAX[1]:
        return [f"{w} x {h} pixels; at most {SHOT_MAX[0]} x {SHOT_MAX[1]}"]
    return []


def check_caption(text, for_index=True):
    """Why this caption cannot be used; None when it can. for_index: it goes
    into index-v3.json, where the client reads strings without escapes."""
    if len(text) > CAPTION_MAX:
        return f"{len(text)} characters; at most {CAPTION_MAX}"
    if not PRINTABLE.match(text):
        return "use plain ASCII (no accents, tabs or other control characters)"
    if for_index and not PLAIN.match(text):
        return "has '\"' or '\\', which the client would read differently; write it without"
    return None


def gallery_shots(drawer):
    """{number: file} of the N.png files in one gallery/<id> drawer."""
    out = {}
    for p in drawer.iterdir():
        hit = SHOT_RE.match(p.name)
        if hit and p.is_file():
            out[int(hit.group(1))] = p
    return out


def read_captions(drawer, where):
    """(captions by line, problems) of a drawer's captions.txt."""
    p = drawer / CAPTIONS
    if not p.is_file():
        return [], []
    try:
        lines = p.read_bytes().decode("ascii").splitlines()
    except UnicodeError:
        return [], [f"{where}/{CAPTIONS}: not plain ASCII"]
    problems = []
    for n, line in enumerate(lines, 1):
        why = check_caption(line.strip())
        if why:
            problems.append(f"{where}/{CAPTIONS} line {n}: {why}")
    return [ln.strip() for ln in lines], problems


def check_drawer(drawer, where):
    """Problems of one gallery/<id> drawer, one line each."""
    problems = []
    shots = gallery_shots(drawer)
    for p in sorted(drawer.iterdir()):
        if p.is_symlink():
            problems.append(f"{where}/{p.name}: a link; put the file itself here")
        elif p.name != CAPTIONS and not (SHOT_RE.match(p.name) and p.is_file()):
            problems.append(f"{where}/{p.name}: not a screenshot; the drawer holds 1.png to "
                            f"{SHOTS_MAX}.png and {CAPTIONS}")
    if not shots:
        problems.append(f"{where}: no screenshot; the first is 1.png")
    for n in sorted(shots):
        if n > SHOTS_MAX:
            problems.append(f"{where}/{n}.png: at most {SHOTS_MAX} screenshots")
        elif n > 1 and n - 1 not in shots:
            # Line N of captions.txt belongs to N.png, and a client stops at
            # the first number it does not find.
            problems.append(f"{where}/{n}.png: there is no {n - 1}.png; number them without a gap")
        if not shots[n].is_symlink():
            problems += [f"{where}/{n}.png: {why}" for why in check_picture(shots[n].read_bytes())]
    captions, more = read_captions(drawer, where)
    problems += more
    if len(captions) > max(shots, default=0):
        problems.append(f"{where}/{CAPTIONS}: {len(captions)} lines for {len(shots)} screenshot(s); "
                        "line N is the caption of N.png")
    return problems


def check_gallery(gallery, ids):
    """Problems of a catalogue's gallery/ drawer. ids: the applications the
    catalogue lists; pictures of anything else have nowhere to be shown."""
    gallery = Path(gallery)
    if not gallery.is_dir():
        return []
    problems = []
    for d in sorted(gallery.iterdir()):
        where = f"gallery/{d.name}"
        if not d.is_dir() or d.is_symlink():
            problems.append(f"{where}: not a drawer; screenshots go in gallery/<id>/")
        elif d.name not in ids:
            problems.append(f"{where}: no approved manifest has the id {d.name!r}")
        else:
            problems += check_drawer(d, where)
    return problems


# ------------------------------------------------------------------ making

def pillow():
    try:
        from PIL import Image
    except ImportError:
        raise SystemExit("making pictures needs Pillow: pip install pillow")
    return Image


def decoded(data):
    """A Pillow image in RGB or RGBA, whatever the file's own form."""
    Image = pillow()
    img = Image.open(io.BytesIO(data))
    img.load()
    if img.mode.startswith("I;16"):
        # Pillow would clip 16-bit grey to its low byte; keep the high one.
        img = img.point([v >> 8 for v in range(65536)], "L")
    alpha = img.mode in ("RGBA", "LA", "PA", "La", "RGBa") or "transparency" in img.info
    return img.convert("RGBA" if alpha else "RGB")


def encoded(img):
    out = io.BytesIO()
    img.save(out, "PNG", optimize=True)
    return out.getvalue()


def fitted(img, box):
    """img scaled down to fit in box, proportions kept. Never enlarged: a
    small picture made bigger only gets blurred."""
    Image = pillow()
    scale = min(box[0] / img.width, box[1] / img.height, 1.0)
    if scale == 1.0:
        return img
    size = (max(1, round(img.width * scale)), max(1, round(img.height * scale)))
    return img.resize(size, Image.LANCZOS)


def on_canvas(img, box):
    """img, fitted, in the middle of a transparent picture of exactly box."""
    Image = pillow()
    small = fitted(img.convert("RGBA"), box)
    canvas = Image.new("RGBA", box, (0, 0, 0, 0))
    canvas.paste(small, ((box[0] - small.width) // 2, (box[1] - small.height) // 2))
    return canvas


def icon_pictures(data):
    """{"icon": bytes, "icon-small": bytes} from a picture, or ValueError."""
    w, h, _, _ = png_info(data, whole=False)
    if w > ICON_SOURCE_MAX[0] or h > ICON_SOURCE_MAX[1]:
        raise ValueError(f"{w} x {h} pixels; an icon of at most {ICON_SOURCE_MAX[0]} x "
                         f"{ICON_SOURCE_MAX[1]} is read")
    return pictures_of_icon(decoded(data))


def _rle35(data, bits, entries):
    """The packed data of a colour icon: runs of `bits`-wide values behind
    count bytes, in one bit stream (the scheme of icon.library 44)."""
    out = bytearray()
    pos = buf = nbits = copy = 0
    while len(out) < entries and (pos < len(data) or (copy and nbits >= bits)):
        loop = 0
        if not copy:
            if nbits < 8:
                if pos >= len(data):
                    break
                buf = (buf << 8) | data[pos]; pos += 1; nbits += 8
            count = (buf >> (nbits - 8)) & 0xFF
            nbits -= 8
            if count <= 127:
                copy = count + 1
            elif count > 128:
                loop = 256 - count + 1
            else:
                continue
        if copy:
            loop += 1
        if nbits < bits:
            if pos >= len(data):
                break
            buf = (buf << 8) | data[pos]; pos += 1; nbits += 8
        value = (buf >> (nbits - bits)) & ((1 << bits) - 1)
        out.extend([value] * min(loop, entries - len(out)))
        if copy:
            copy -= 1
        nbits -= bits
        buf &= (1 << nbits) - 1
    if len(out) != entries:
        raise ValueError("the colour icon's data ends early")
    return bytes(out)


def classic_icon_image(data):
    """The picture of a classic icon file (a DiskObject) that carries a
    colour icon: the FORM ICON written by icon.library 44 and later, with an
    ARGB chunk (preferred) or a palette image (IMAG). A Pillow RGBA image, or
    None when the file has neither: a plain planar icon is not converted,
    because its colours belong to the screen it is shown on."""
    Image = pillow()
    at = data.find(b"FORM")
    while at >= 0 and data[at + 8:at + 12] != b"ICON":
        at = data.find(b"FORM", at + 4)
    if at < 0:
        return None
    end = min(len(data), at + 8 + int.from_bytes(data[at + 4:at + 8], "big"))
    pos, size, argb, imag = at + 12, None, None, None
    while pos + 8 <= end:
        cid, n = data[pos:pos + 4], int.from_bytes(data[pos + 4:pos + 8], "big")
        body = data[pos + 8:pos + 8 + n]
        if cid == b"FACE" and n >= 2:
            size = (body[0] + 1, body[1] + 1)
        elif cid == b"ARGB" and argb is None:
            argb = body
        elif cid == b"IMAG" and imag is None:
            imag = body
        pos += 8 + n + (n & 1)
    if not size or size[0] > ICON_SOURCE_MAX[0] or size[1] > ICON_SOURCE_MAX[1]:
        return None
    w, h = size
    if argb is not None and len(argb) > 10:
        # Ten bytes of header, then the pixels (A, R, G, B) through zlib.
        pixels = zlib.decompress(argb[10:])
        if len(pixels) >= w * h * 4:
            a, r, g, b = Image.frombytes("RGBA", size, pixels[:w * h * 4]).split()
            return Image.merge("RGBA", (r, g, b, a))
    if imag is not None and len(imag) >= 10:
        transparent, colours, flags, image_format, palette_format, depth = imag[:6]
        colours += 1
        image_size = int.from_bytes(imag[6:8], "big") + 1
        palette_size = int.from_bytes(imag[8:10], "big") + 1
        if not flags & 2:
            return None                 # no palette of its own: its colours are unknown
        image = imag[10:10 + image_size]
        palette = imag[10 + image_size:10 + image_size + palette_size]
        indexes = _rle35(image, depth, w * h) if image_format == 1 else image[:w * h]
        colour_bytes = _rle35(palette, 8, colours * 3) if palette_format == 1 else palette[:colours * 3]
        if len(indexes) < w * h or len(colour_bytes) < colours * 3:
            raise ValueError("the colour icon's data ends early")
        out = bytearray()
        for i in indexes:
            if i >= colours:
                out += b"\0\0\0\0"
            else:
                out += colour_bytes[i * 3:i * 3 + 3] + (b"\0" if (flags & 1) and i == transparent else b"\xff")
        return Image.frombytes("RGBA", size, bytes(out))
    return None


def pictures_of_icon(img):
    out = {"icon": encoded(on_canvas(img, ICON)), "icon-small": encoded(on_canvas(img, ICON_SMALL))}
    return within_limit(out)


def shot_pictures(data, img=None, full_box=None):
    """{"full": bytes, "view": bytes, "thumb": bytes} of one screenshot.

    data: the source file when it is a PNG, kept as `full` byte for byte if
    it is already truecolour. full_box: scale `full` down to fit (a
    distribution author's source may be any size; the catalogue's are
    checked before they get here)."""
    if img is None:
        img = decoded(data)
    keep = False
    if data is not None and (full_box is None or (img.width <= full_box[0] and img.height <= full_box[1])):
        try:
            _, _, depth, ctype = png_info(data)
            # Byte 28 is the interlace method. The client's own decoder
            # (src/libpkg/pngdec.c) reads 8-bit truecolour, not interlaced.
            keep = depth == 8 and ctype in (2, 6) and data[28] == 0
        except ValueError:
            pass
    full = data if keep else encoded(fitted(img, full_box) if full_box else img)
    return within_limit({"full": full, "view": encoded(fitted(img, VIEW)), "thumb": encoded(fitted(img, THUMB))})


def within_limit(files):
    for kind, data in files.items():
        if len(data) > FILE_MAX:
            raise ValueError(f"its {kind} picture is {len(data)} bytes; at most {FILE_MAX} (512 KiB)")
    return files


def bundle(icons):
    """The icon archive: {id: {"icon": bytes, "icon-small": bytes}} as one
    ZIP. The same icons give the same bytes: sorted names, one fixed time,
    and members stored, since a PNG does not get smaller and a stored
    member does not depend on the zlib that wrote it."""
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w", zipfile.ZIP_STORED) as z:
        for pid in sorted(icons):
            for kind in ("icon", "icon-small"):
                zi = zipfile.ZipInfo(f"{pid}/{kind}.png", STAMP)
                zi.create_system = 3
                zi.external_attr = 0o644 << 16
                z.writestr(zi, icons[pid][kind])
    return out.getvalue()


# ------------------------------------------------------------------ publication

def sha8(data):
    return hashlib.sha256(data).hexdigest()[:8]


def find_archive(cache, p):
    """The downloaded archive of entry p with exactly its bytes, or None.
    ci_check_index.py keeps archives by the hash of their URL; a local run
    of mkindex.py has them by file name."""
    name = p["url"].rsplit("/", 1)[-1]
    if not name or not cache:
        return None
    cache = Path(cache)
    places = [cache / hashlib.sha256(p["url"].encode()).hexdigest()[:24] / name]
    if cache.is_dir():
        places += sorted(cache.rglob(name))
    for f in places:
        # Two uploads can share a file name; only the entry's own bytes
        # may give its icon.
        if f.is_file() and f.stat().st_size == p["size"] \
                and hashlib.sha256(f.read_bytes()).hexdigest() == p["sha256"]:
            return f
    return None


def icon_members(p):
    """Where the icon of entry p may be in its archive, best first: the
    program's own icon, then the drawer's."""
    out = []
    launch = p.get("launch")
    if isinstance(launch, dict) and launch.get("executable"):
        out.append((p["subdir"] + "/" if p.get("subdir") else "") + launch["executable"] + ".info")
    if p.get("icon") and p["icon"] not in out:
        out.append(p["icon"])
    return out


class Publication:
    """The pictures of one catalogue, written under `out` as they are served
    from `base_url`. Nothing already there is changed or removed: a name
    holds the start of its file's SHA-256, so it never gets other bytes."""

    def __init__(self, out, base_url, cache=None, gallery=None, archive_names=None, archive_member=None):
        self.out = Path(out)
        self.base = base_url.rstrip("/") + "/"
        self.cache = cache
        self.gallery = Path(gallery) if gallery else None
        self.archive_names, self.archive_member = archive_names, archive_member
        self.notes = []
        # Applications whose archives were never read: one line for all of
        # them, since a run that downloads only what changed has many.
        self.unread = []

    def note(self, text):
        self.notes.append(text)

    def put(self, rel, data):
        f = self.out / rel
        if not f.is_file():
            f.parent.mkdir(parents=True, exist_ok=True)
            tmp = f.with_name(f.name + ".part")
            tmp.write_bytes(data)
            tmp.replace(f)
        return self.described(rel, data)

    def described(self, rel, data):
        w, h, _, _ = png_info(data, whole=False)
        return {"url": self.base + rel, "sha256": hashlib.sha256(data).hexdigest(), "size": len(data),
                "width": w, "height": h}

    # An archive is read once. What it gave is kept beside the pictures,
    # under a name made of everything the answer depends on, so a later run
    # needs neither the archive nor Pillow for it; the publish workflow
    # downloads only the archives of entries that changed.
    def source_file(self, p, members):
        key = "\n".join([str(ICON_GENERATION), p["id"], p["sha256"], *members])
        return self.out / SOURCES / (hashlib.sha256(key.encode()).hexdigest()[:32] + ".json")

    def icon_of_build(self, p):
        """(files or None, known): the icon pictures of one build. known is
        False when its archive is not at hand and was never read."""
        members = icon_members(p)
        if not members:
            return None, True
        rec_file = self.source_file(p, members)
        if rec_file.is_file():
            try:
                rec = json.loads(rec_file.read_text(encoding="utf-8"))
                if not rec.get("icon"):
                    return None, True
                files = {k: (self.out / p["id"] / rec[k]).read_bytes() for k in ("icon", "icon-small")}
                if all(sha8(files[k]) == rec[k][:8] for k in files):
                    return files, True
            except (OSError, ValueError, KeyError, TypeError):
                pass                # an unusable record: read the archive again
        archive = find_archive(self.cache, p)
        if archive is None:
            return None, False
        rec = {"id": p["id"], "archive": p["sha256"], "member": None, "icon": None, "icon-small": None}
        files = None
        try:
            names = self.archive_names(archive)
            for member in members:
                if member not in names:
                    continue
                data = self.archive_member(archive, member)
                try:
                    if data[:8] == PNG_SIG:
                        files = icon_pictures(data)
                    else:
                        # A classic icon (a DiskObject) has a picture to
                        # take only when it carries a colour icon.
                        img = classic_icon_image(data)
                        if img is None:
                            continue
                        files = pictures_of_icon(img)
                except Exception as exc:
                    self.note(f"{p['id']}: icon {member} in {archive.name}: {exc}; left out")
                    continue
                rec.update(member=member, **{k: f"{sha8(v)}-{k}.png" for k, v in files.items()})
                break
        except Exception as exc:
            self.note(f"{p['id']}: cannot read {archive.name} for its icon ({exc})")
            return None, False
        if files:
            for k, v in files.items():
                self.put(f"{p['id']}/{rec[k]}", v)
        rec_file.parent.mkdir(parents=True, exist_ok=True)
        rec_file.write_text(json.dumps(rec, indent=1, sort_keys=True) + "\n", encoding="utf-8")
        return files, True

    def icon_of(self, pid, builds):
        order = {b: n for n, b in enumerate(BUILD_ORDER)}
        unknown = []
        for p in sorted(builds, key=lambda p: (order.get((p["arch"], p["abi"]), len(order)), p["arch"], p["abi"])):
            files, known = self.icon_of_build(p)
            if files:
                return files
            if not known:
                unknown.append(f"{p['arch']}/{p['abi']}")
        if unknown:
            self.unread.append(pid)
        return None

    def gallery_of(self, pid):
        drawer = self.gallery / pid if self.gallery else None
        if not drawer or not drawer.is_dir():
            return []
        where = f"gallery/{pid}"
        captions, problems = read_captions(drawer, where)
        for why in problems:
            self.note(f"{why}; caption left out")
        out = []
        shots = gallery_shots(drawer)
        for n in sorted(shots):
            if n > SHOTS_MAX or (n > 1 and n - 1 not in shots):
                self.note(f"{where}/{n}.png: past the first {SHOTS_MAX} or after a gap; left out")
                continue
            data = shots[n].read_bytes()
            problems = check_picture(data)
            try:
                files = None if problems else shot_pictures(data)
            except Exception as exc:
                problems = [str(exc)]
            if problems:
                self.note(f"{where}/{n}.png: {problems[0]}; left out")
                continue
            entry = {"type": "screenshot"}
            caption = captions[n - 1] if n <= len(captions) else ""
            if caption and not check_caption(caption):
                entry["caption"] = caption
            for kind in ("full", "view", "thumb"):
                entry[kind] = self.put(f"{pid}/{sha8(files[kind])}-{n}-{kind}.png", files[kind])
            out.append(entry)
        return out

    def run(self, packages):
        """({id: media object}, media_bundle or None) for published entries."""
        builds = {}
        for p in packages:
            builds.setdefault(p["id"], []).append(p)
        media, icons = {}, {}
        for pid in sorted(builds):
            m = {}
            files = self.icon_of(pid, builds[pid])
            if files:
                icons[pid] = files
                m["icon"] = self.described(f"{pid}/{sha8(files['icon'])}-icon.png", files["icon"])
                m["icon_small"] = self.described(f"{pid}/{sha8(files['icon-small'])}-icon-small.png",
                                                 files["icon-small"])
            shots = self.gallery_of(pid)
            if shots:
                m["gallery"] = shots
            if m:
                media[pid] = m
        if self.gallery and self.gallery.is_dir():
            for d in sorted(self.gallery.iterdir()):
                if d.name not in builds:
                    self.note(f"gallery/{d.name}: no published package has this id; not used")
        if self.unread:
            few = len(self.unread) <= 8
            self.note(f"{len(self.unread)} application(s) whose archive is not at hand and was not read "
                      f"before: icon not looked for" + (f" ({', '.join(self.unread)})" if few else
                                                         "; a run that reads every archive finds them"))
        if not icons:
            return media, None
        data = bundle(icons)
        rel = f"icons-{sha8(data)}.zip"
        f = self.out / rel
        if not f.is_file():
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_bytes(data)
        return media, {"url": self.base + rel, "sha256": hashlib.sha256(data).hexdigest(), "size": len(data)}
