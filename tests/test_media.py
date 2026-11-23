#!/usr/bin/env python3
"""Pictures in the catalogue (docs/rfc/0003): the rules for gallery/ and
what breaks them, apkg-pack submit --screenshot, and the files that
tools/mkindex.py --media-out, tools/make-media.py and the publish step make.

The rules need only Python and are always tested. Making pictures needs
Pillow; without it that half is skipped, and says so. The pictures used
are written here, a few hundred bytes each; nothing is downloaded.
Host only."""
import hashlib, io, json, os, shutil, struct, subprocess, sys, tempfile, zipfile, zlib
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(HERE / "tools"))
sys.path.insert(0, str(HERE / "tests"))
import catalogue_media as cm  # noqa: E402
import mkindex  # noqa: E402

fails = []
TMP = Path(tempfile.mkdtemp(prefix="media-test-"))
BASE = "https://media.example.invalid/m/"


def chunk(tag, body):
    return struct.pack(">I", len(body)) + tag + body + struct.pack(">I", zlib.crc32(tag + body))


def png(w, h, ctype=2, depth=8, interlace=0, extra=b"", seed=1):
    """A PNG written by hand: truecolour (2), with alpha (6), or palette (3)."""
    n = {2: 3, 6: 4, 3: 1}[ctype]
    rows = b"".join(b"\0" + bytes((x * 7 + y * 3 + c * 40 + seed) % (4 if ctype == 3 else 256)
                                  for x in range(w) for c in range(n)) for y in range(h))
    out = cm.PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, interlace))
    if ctype == 3:
        out += chunk(b"PLTE", bytes([200, 30, 30, 30, 200, 30, 30, 30, 200, 240, 240, 240]))
    return out + extra + chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")


def ihdr(data):
    """(width, height, bit depth, colour type, interlace) of written PNG bytes."""
    return struct.unpack(">IIBB", data[16:26]) + (data[28],)


def want(name, got, expected):
    if got != expected:
        fails.append(f"{name}: got {got!r}, expected {expected!r}")


def want_problem(name, problems, text):
    if not any(text in p for p in problems):
        fails.append(f"{name}: expected a problem with {text!r}, got {problems}")


# ------------------------------------------------------------------ the rules

SHOT = png(320, 200)
want("a truecolour PNG is read", cm.png_info(SHOT), (320, 200, 8, 2))
want("a palette PNG is read", cm.png_info(png(32, 20, ctype=3)), (32, 20, 8, 3))
want("a sound screenshot passes", cm.check_picture(SHOT), [])
want("a palette screenshot passes (publication writes it truecolour)", cm.check_picture(png(32, 20, ctype=3)), [])
want_problem("not a PNG", cm.check_picture(b"\xff\xd8\xff\xe0 a JPEG"), "not a PNG file")
want_problem("cut short", cm.check_picture(SHOT[:-20]), "cut short")
broken = bytearray(SHOT); broken[60] ^= 0xFF
want_problem("damaged", cm.check_picture(bytes(broken)), "damaged")
want_problem("bytes after the picture", cm.check_picture(SHOT + b"PK\x03\x04payload"), "after the end of the picture")
want_problem("too wide", cm.check_picture(png(1281, 2)), "1281 x 2 pixels; at most 1280 x 800")
want_problem("too tall", cm.check_picture(png(2, 801)), "2 x 801 pixels")
want("the largest size passes", cm.check_picture(png(1280, 800, ctype=3)), [])
want_problem("too many bytes", cm.check_picture(SHOT + b"\0" * cm.FILE_MAX), "at most 524288")
# A header that states less than the data holds: a decoder would be handed
# more than it was promised.
lying = cm.PNG_SIG + chunk(b"IHDR", struct.pack(">IIBBBBB", 4, 4, 8, 2, 0, 0, 0)) \
    + chunk(b"IDAT", zlib.compress(b"\0" * 5000)) + chunk(b"IEND", b"")
want_problem("data larger than the header states", cm.check_picture(lying), "not the size its header states")

want("a caption passes", cm.check_caption("Two-page spread with search results"), None)
want("a caption of the limit passes", cm.check_caption("a" * 120), None)
if "121 characters" not in (cm.check_caption("a" * 121) or ""):
    fails.append("a caption of 121 characters was accepted")
if "plain ASCII" not in (cm.check_caption("Café") or ""):
    fails.append("a caption with an accent was accepted")
if not cm.check_caption('The "main" window'):
    fails.append("a caption with a quote was accepted for the index")
want("a quote is fine in a local captions.txt", cm.check_caption('The "main" window', for_index=False), None)


def gallery(files, ids=("demo",)):
    """Problems of a gallery/ holding files {relative name: bytes}."""
    d = Path(tempfile.mkdtemp(dir=TMP)) / "gallery"
    d.mkdir()
    for name, data in files.items():
        (d / name).parent.mkdir(parents=True, exist_ok=True)
        (d / name).write_bytes(data)
    return cm.check_gallery(d, set(ids))


want("a sound gallery", gallery({"demo/1.png": SHOT, "demo/2.png": SHOT,
                                 "demo/captions.txt": b"First\nSecond\n"}), [])
want("no gallery at all", cm.check_gallery(TMP / "nowhere", {"demo"}), [])
want_problem("an id the catalogue does not list", gallery({"ghost/1.png": SHOT}), "no approved manifest has the id 'ghost'")
want_problem("seven screenshots", gallery({f"demo/{n}.png": SHOT for n in range(1, 8)}), "demo/7.png: at most 6")
want_problem("a gap", gallery({"demo/1.png": SHOT, "demo/3.png": SHOT}), "there is no 2.png")
want_problem("a JPEG named .png", gallery({"demo/1.png": b"\xff\xd8\xff\xe0"}), "gallery/demo/1.png: not a PNG file")
want_problem("an oversized picture", gallery({"demo/1.png": png(1300, 10)}), "1300 x 10 pixels")
want_problem("another file", gallery({"demo/1.png": SHOT, "demo/notes.txt": b"x"}), "demo/notes.txt: not a screenshot")
want_problem("a JPEG", gallery({"demo/1.png": SHOT, "demo/2.jpg": b"x"}), "demo/2.jpg: not a screenshot")
want_problem("a file in gallery/", gallery({"readme.txt": b"x", "demo/1.png": SHOT}), "gallery/readme.txt: not a drawer")
want_problem("a drawer without a screenshot", gallery({"demo/captions.txt": b"x\n"}), "no screenshot")
want_problem("a long caption", gallery({"demo/1.png": SHOT, "demo/captions.txt": b"a" * 121 + b"\n"}),
             "captions.txt line 1: 121 characters")
want_problem("a caption that is not ASCII", gallery({"demo/1.png": SHOT, "demo/captions.txt": "Café\n".encode()}),
             "captions.txt: not plain ASCII")
want_problem("more captions than screenshots", gallery({"demo/1.png": SHOT, "demo/captions.txt": b"a\nb\n"}),
             "2 lines for 1 screenshot")
got = gallery({"demo/1.png": png(1300, 10), "demo/3.png": SHOT, "ghost/1.png": SHOT})
if len(got) != 3:
    fails.append(f"one line per problem: {got}")


# ------------------------------------------------------------------ the check of a pull request

def git(repo, *args):
    r = subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args], cwd=repo,
                       capture_output=True, text=True)
    if r.returncode:
        raise RuntimeError(f"git {args}: {r.stderr}")
    return r.stdout.strip()


def archive(name="Demo", members=None, info=b"icon"):
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w") as z:
        for n, d in members or ((f"{name}/", b""), (f"{name}/ReadMe", b"hello\n"), (f"{name}.info", info)):
            z.writestr(n, d)
    return b.getvalue()


def manifest(data, pid="demo", arch="x86_64", abi="v11", extra="", url=None):
    name = pid.capitalize()
    return (f'status = "approved"\nid = "{pid}"\nversion = "1.0"\nsummary = "{name}"\n'
            f'arch = "{arch}"\nabi = "{abi}"\nkind = "app"\n'
            f'url = "{url or f"https://example.invalid/{pid}-{arch}-{abi}.zip"}"\nsize = {len(data)}\n'
            f'sha256 = "{hashlib.sha256(data).hexdigest()}"\ndepends = []\ndepends_checked = true\n'
            f'subdir = "{name}"\nicon = "{name}.info"\n{extra}')


def pull_request(files):
    """ci_check_index.py on a repository whose pull request adds `files` to gallery/."""
    d = Path(tempfile.mkdtemp(dir=TMP))
    (d / "manifests").mkdir()
    a = archive()
    (d / "manifests/demo.x86_64.v11.toml").write_text(manifest(a))
    git(d, "init", "-q"); git(d, "add", "-A"); git(d, "commit", "-qm", "base"); git(d, "tag", "base")
    for name, data in files.items():
        (d / "gallery" / name).parent.mkdir(parents=True, exist_ok=True)
        (d / "gallery" / name).write_bytes(data)
    git(d, "add", "-A"); git(d, "commit", "-qm", "screenshots")
    r = subprocess.run([sys.executable, str(HERE / "tools/ci_check_index.py"), str(d), "--base", "base",
                        "--cache", str(d.parent / (d.name + "-cache"))], capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


code, out = pull_request({"demo/1.png": SHOT, "demo/captions.txt": b"The window\n"})
if code or "all checks pass" not in out:
    fails.append(f"a pull request with sound screenshots: {code} {out}")
code, out = pull_request({"demo/1.png": png(1300, 10), "ghost/1.png": SHOT, "demo/captions.txt": b"a" * 130 + b"\n"})
if code == 0 or "1300 x 10 pixels" not in out or "no approved manifest has the id 'ghost'" not in out \
        or "130 characters" not in out or "3 problem(s)" not in out:
    fails.append(f"a pull request whose gallery breaks three rules: {code} {out}")


# ------------------------------------------------------------------ apkg-pack submit --screenshot

def elf():
    h = bytearray(64); h[:4] = b"\x7fELF"; h[4] = 2; h[5] = 1; h[18:20] = (62).to_bytes(2, "little")
    return bytes(h) + b"\0" * 64 + b"$VER: Tool 1.0 (01.01.2026)\0"


def pack(*args):
    r = subprocess.run([sys.executable, str(HERE / "tools/apkg-pack")] + [str(a) for a in args],
                       capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


d = Path(tempfile.mkdtemp(dir=TMP))
(d / "Tool").mkdir()
(d / "Tool/Tool").write_bytes(elf())
(d / "Tool.arospkg.toml").write_text(
    'id = "tool"\nversion = "1.0"\nsummary = "A tool"\nkind = "app"\narch = "x86_64"\nabi = "v11"\n'
    'subdir = "Tool"\ndepends = []\ndepends_checked = true\n')
code, out = pack("build", d / "Tool", "--output", d / "tool.zip")
if code:
    fails.append(f"build for the submit cases: {out}")
idx = d / "index"; (idx / "manifests").mkdir(parents=True)
one, two, pal, wide = d / "one.png", d / "two.png", d / "pal.png", d / "wide.png"
one.write_bytes(png(320, 200)); two.write_bytes(png(64, 40, seed=9)); pal.write_bytes(png(32, 20, ctype=3))
wide.write_bytes(png(1300, 10))
url = "https://example.invalid/tool.zip"
submit = ("submit", url, "--index", idx, "--use-local-copy", d / "tool.zip")
shots = idx / "gallery/tool"

code, out = pack(*submit, "--screenshot", one, "--screenshot", wide)
if code == 0 or "wide.png: 1300 x 10 pixels" not in out or (idx / "manifests/tool.x86_64.v11.toml").exists():
    fails.append(f"submit with an oversized screenshot must write nothing: {code} {out}")
code, out = pack(*submit, "--screenshot", one, "--caption", "a", "--caption", "b")
if code == 0 or "2 captions for 1 screenshot" not in out:
    fails.append(f"submit with more captions than screenshots: {code} {out}")
code, out = pack(*submit, "--screenshot", one, "--caption", "Café")
if code == 0 or "caption 1" not in out:
    fails.append(f"submit with a caption that is not ASCII: {code} {out}")
code, out = pack(*submit, *[x for _ in range(7) for x in ("--screenshot", one)])
if code == 0 or "7 screenshots" not in out:
    fails.append(f"submit with seven screenshots: {code} {out}")
code, out = pack(*submit, "--screenshot", one, "--dry-run")
if code or "gallery/tool/1.png" not in out or shots.exists():
    fails.append(f"submit --dry-run with a screenshot: {code} {out}")
code, out = pack(*submit, "--screenshot", one, "--screenshot", two, "--screenshot", pal,
                 "--caption", "The main window", "--caption", "", "--caption", "Preferences")
names = sorted(p.name for p in shots.iterdir()) if shots.is_dir() else []
if code or names != ["1.png", "2.png", "3.png", "captions.txt"] or (shots / "2.png").read_bytes() != two.read_bytes() \
        or (shots / "captions.txt").read_bytes() != b"The main window\n\nPreferences\n":
    fails.append(f"submit with three screenshots: {code} {out} {names}")
if cm.check_gallery(idx / "gallery", {"tool"}):
    fails.append(f"what submit placed breaks the gallery rules: {cm.check_gallery(idx / 'gallery', {'tool'})}")
code, out = pack(*submit, "--screenshot", one, "--screenshot", two, "--screenshot", pal,
                 "--caption", "The main window", "--caption", "", "--caption", "Preferences")
if code or "already up to date" not in out:
    fails.append(f"the same submission again: {code} {out}")
# The screenshots given are the whole gallery: the others go.
code, out = pack(*submit, "--screenshot", two)
names = sorted(p.name for p in shots.iterdir())
if code or names != ["1.png"] or (shots / "1.png").read_bytes() != two.read_bytes():
    fails.append(f"submit with one screenshot after three: {code} {out} {names}")
# Without the options, the gallery is not touched.
code, out = pack(*submit)
if code or "already up to date" not in out or not (shots / "1.png").is_file():
    fails.append(f"submit without --screenshot must leave the gallery: {code} {out}")


# ------------------------------------------------------------------ making pictures

try:
    import PIL  # noqa: F401
    HAVE_PILLOW = True
except ImportError:
    HAVE_PILLOW = False


def crc16(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


def lha_of(members):
    """Level-1 -lh0- headers with a real CRC, so lhasa extracts them."""
    out = b""
    for name, data in members:
        name = name.encode()
        body = (b"-lh0-" + struct.pack("<II", len(data), len(data)) + struct.pack("<I", 0x5A210000) +
                bytes([0x20, 1, len(name)]) + name + struct.pack("<H", crc16(data)) + b"A" + struct.pack("<H", 0))
        out += bytes([len(body), sum(body) & 0xFF]) + body + data
    return out + b"\x00"


# An AROS PNG icon: a PNG with an icOn chunk, and the picture for the
# selected state after it.
ICON_PROGRAM = png(48, 48, ctype=6, extra=chunk(b"icOn", struct.pack(">II", 0x8000100f, 3)), seed=5) + png(48, 48, ctype=6)
ICON_DRAWER = png(40, 20, ctype=3, seed=3)
CLASSIC = b"\xe3\x10" + b"\0" * 76
LAUNCH = '\n[launch]\nexecutable = "Demo"\nmode = "workbench"\n'


def catalogue(builds, shots=None):
    """A scratch catalogue: builds is a list of (manifest file, text, archive
    file, bytes); shots is {relative name in gallery/: bytes}."""
    d = Path(tempfile.mkdtemp(dir=TMP))
    (d / "manifests").mkdir(); (d / "cache").mkdir()
    for mname, text, aname, data in builds:
        (d / "manifests" / mname).write_text(text)
        (d / "cache" / aname).write_bytes(data)
    for name, data in (shots or {}).items():
        (d / "gallery" / name).parent.mkdir(parents=True, exist_ok=True)
        (d / "gallery" / name).write_bytes(data)
    return d


def generate(d, out="out", media="media", options=True, cache="cache"):
    """mkindex.py on a scratch catalogue: (exit code, output, index-v3 as data)."""
    (d / out).mkdir(exist_ok=True)
    cmd = [sys.executable, str(HERE / "tools/mkindex.py"), "--manifests", str(d / "manifests"),
           "--out-dir", str(d / out), "--no-archive-check", "--cache", str(d / cache)]
    if options:
        cmd += ["--media-out", str(d / media), "--media-base-url", BASE, "--gallery", str(d / "gallery")]
    r = subprocess.run(cmd, capture_output=True, text=True)
    v3 = d / out / "index-v3.json"
    return r.returncode, r.stdout + r.stderr, json.loads(v3.read_text()) if v3.exists() else {}


def build(pid="demo", arch="x86_64", abi="v11", members=None, info=b"icon", extra="", lha=False):
    name = pid.capitalize()
    members = members or [(f"{name}/", b""), (f"{name}/ReadMe", b"hello\n"), (f"{name}.info", info)]
    data = lha_of([m for m in members if not m[0].endswith("/")]) if lha else archive(members=members)
    aname = f"{pid}-{arch}-{abi}." + ("lha" if lha else "zip")
    return (f"{pid}.{arch}.{abi}.toml",
            manifest(data, pid, arch, abi, extra, url=f"https://example.invalid/{aname}"), aname, data)


def served(d, obj, media="media"):
    """The bytes a media object of the index names, checked against what it says."""
    if not obj["url"].startswith(BASE):
        fails.append(f"{obj['url']}: not under the base address")
        return b""
    rel = obj["url"][len(BASE):]
    f = d / media / rel
    if not f.is_file():
        fails.append(f"{rel}: named by the index, not written")
        return b""
    data = f.read_bytes()
    if hashlib.sha256(data).hexdigest() != obj["sha256"] or len(data) != obj["size"]:
        fails.append(f"{rel}: size or sha256 is not the file's")
    if not rel.split("/")[-1].startswith(obj["sha256"][:8] + "-") and not rel.startswith("icons-" + obj["sha256"][:8]):
        fails.append(f"{rel}: the name does not start with the file's sha256")
    if len(data) > cm.FILE_MAX and not rel.endswith(".zip"):
        fails.append(f"{rel}: larger than 512 KiB")
    return data


def truecolour(name, data, size=None, inside=None):
    w, h, depth, ctype, interlace = ihdr(data)
    if depth != 8 or ctype not in (2, 6) or interlace:
        fails.append(f"{name}: depth {depth}, colour type {ctype}, interlace {interlace}; expected 8-bit truecolour")
    if size and (w, h) != size:
        fails.append(f"{name}: {w} x {h}, expected {size[0]} x {size[1]}")
    if inside and (w > inside[0] or h > inside[1]):
        fails.append(f"{name}: {w} x {h} does not fit in {inside[0]} x {inside[1]}")
    return w, h


def with_pillow():
    from PIL import Image

    # The program's icon is preferred to the drawer's; both are PNG icons.
    both = [("Demo/", b""), ("Demo/Demo", b"program"), ("Demo/Demo.info", ICON_PROGRAM), ("Demo.info", ICON_DRAWER)]
    d = catalogue([build(members=both, extra=LAUNCH)],
                  {"demo/1.png": png(1280, 800, ctype=3), "demo/2.png": png(300, 400, ctype=6),
                   "demo/3.png": png(100, 50), "demo/captions.txt": b"The main window\n\nA small one\n"})
    code, out, v3 = generate(d)
    p = (v3.get("packages") or [{}])[0]
    m = p.get("media") or {}
    if code or set(m) != {"icon", "icon_small", "gallery"}:
        fails.append(f"an application with an icon and screenshots: {code} {out} {sorted(m)}")
        return
    icon, small = served(d, m["icon"]), served(d, m["icon_small"])
    truecolour("icon", icon, size=(64, 64)); truecolour("icon-small", small, size=(24, 24))
    want("icon: colour type (with alpha)", ihdr(icon)[3], 6)
    want("icon: the size the index states", (m["icon"]["width"], m["icon"]["height"]), (64, 64))
    want("icon-small: the size the index states", (m["icon_small"]["width"], m["icon_small"]["height"]), (24, 24))
    want("icon address", m["icon"]["url"], f"{BASE}demo/{m['icon']['sha256'][:8]}-icon.png")
    want("icon-small address", m["icon_small"]["url"], f"{BASE}demo/{m['icon_small']['sha256'][:8]}-icon-small.png")
    # 48 x 48 is not enlarged: it sits in the middle of a transparent 64 x 64.
    img = Image.open(io.BytesIO(icon))
    want("icon: transparent corner", img.getpixel((0, 0))[3], 0)
    want("icon: the program's picture in the middle", img.getpixel((8, 8)), Image.open(io.BytesIO(ICON_PROGRAM)).getpixel((0, 0)))
    want("icon-small: scaled to fill 24 x 24", Image.open(io.BytesIO(small)).getbbox(), (0, 0, 24, 24))

    g = m["gallery"]
    want("gallery: three entries, typed", [e["type"] for e in g], ["screenshot"] * 3)
    want("gallery: captions by line", [e.get("caption") for e in g], ["The main window", None, "A small one"])
    for n, e in enumerate(g, 1):
        for kind, box in (("full", cm.SHOT_MAX), ("view", cm.VIEW), ("thumb", cm.THUMB)):
            data = served(d, e[kind])
            size = truecolour(f"{n}-{kind}", data, inside=box)
            want(f"{n}-{kind}: the size the index states", (e[kind]["width"], e[kind]["height"]), size)
            want(f"{n}-{kind} address", e[kind]["url"], f"{BASE}demo/{e[kind]['sha256'][:8]}-{n}-{kind}.png")
    want("1: view of 1280 x 800", (g[0]["view"]["width"], g[0]["view"]["height"]), (640, 400))
    want("1: thumb of 1280 x 800", (g[0]["thumb"]["width"], g[0]["thumb"]["height"]), (160, 100))
    want("2: view of 300 x 400 keeps its proportions", (g[1]["view"]["width"], g[1]["view"]["height"]), (300, 400))
    want("2: thumb of 300 x 400 keeps its proportions", (g[1]["thumb"]["width"], g[1]["thumb"]["height"]), (75, 100))
    want("3: a small screenshot is not enlarged", (g[2]["view"]["width"], g[2]["view"]["height"]), (100, 50))
    # A palette screenshot is written again as truecolour, the same picture;
    # a truecolour one is kept byte for byte.
    full1 = served(d, g[0]["full"])
    want("1-full: a palette PNG is re-encoded", ihdr(full1)[2:4], (8, 2))
    want("1-full: the same pixels", Image.open(io.BytesIO(full1)).getpixel((5, 7)),
         Image.open(io.BytesIO(png(1280, 800, ctype=3))).convert("RGB").getpixel((5, 7)))
    want("2-full: a truecolour PNG is kept as it is", served(d, g[1]["full"]), png(300, 400, ctype=6))

    # The icon archive, and what index-v2.json and index.json do not get.
    b = v3.get("media_bundle") or {}
    want("media_bundle keys", sorted(b), ["sha256", "size", "url"])
    data = served(d, b) if b else b""
    want("media_bundle address", b.get("url"), f"{BASE}icons-{hashlib.sha256(data).hexdigest()[:8]}.zip")
    z = zipfile.ZipFile(io.BytesIO(data))
    want("icon archive members", z.namelist(), ["demo/icon.png", "demo/icon-small.png"])
    want("icon archive: icon.png", z.read("demo/icon.png"), icon)
    want("icon archive: icon-small.png", z.read("demo/icon-small.png"), small)
    for name in ("index-v2.json", "index.json"):
        text = (d / "out" / name).read_text()
        if '"media' in text or "icons-" in text:
            fails.append(f"{name} carries pictures")
    v2 = json.loads((d / "out/index-v2.json").read_text())["packages"][0]
    want("index-v3.json entry is the index-v2.json entry plus media", {k: v for k, v in p.items() if k != "media"}, v2)

    # The same input twice, into another place: the same bytes and names.
    code, out, again = generate(d, out="out2", media="media2")
    want("two runs: the same media_bundle", again.get("media_bundle"), b)
    want("two runs: the same index-v3.json", (d / "out2/index-v3.json").read_bytes(), (d / "out/index-v3.json").read_bytes())
    want("two runs: the same files", sorted(p.relative_to(d / "media2").as_posix() for p in (d / "media2").rglob("*")),
         sorted(p.relative_to(d / "media").as_posix() for p in (d / "media").rglob("*")))
    # A later run without the archive (the publish run downloads only what
    # changed) keeps the icon the earlier one read.
    code, out, later = generate(d, out="out3", cache="nocache")
    want("a run without the archive keeps the icon", (later.get("packages") or [{}])[0].get("media"), m)
    # ...and into an empty place it has no icon, and says why.
    code, out, none = generate(d, out="out4", media="media4", cache="nocache")
    m4 = (none.get("packages") or [{}])[0].get("media") or {}
    if code or "icon" in m4 or "gallery" not in m4 or "media_bundle" in none or "icon not looked for" not in out:
        fails.append(f"no archive and no earlier run: {code} {out} {sorted(m4)}")

    # Without the options nothing changes: index-v3.json has no media, is
    # the text the generator wrote before, and no picture is made.
    code, out, plain = generate(d, out="plain", options=False)
    text = (d / "plain/index-v3.json").read_text()
    if code or "media" in text or "with pictures" in out:
        fails.append(f"no media options: {code} {out}")
    want("no media options: index-v3.json as before", text,
         json.dumps({"schema": 1, "source": plain.get("source"), "packages": plain.get("packages")}, indent=2) + "\n")
    want("no media options: index-v3.json is index-v2.json here", text, (d / "plain/index-v2.json").read_text())
    want("no media options: the other files are the same with them",
         [(d / "plain" / n).read_bytes() for n in ("index-v2.json", "index.json")],
         [(d / "out" / n).read_bytes() for n in ("index-v2.json", "index.json")])
    for bad in (["--gallery", str(d / "gallery")], ["--media-out", str(d / "m9")],
                ["--media-out", str(d / "m9"), "--media-base-url", "http://example.invalid/"]):
        r = subprocess.run([sys.executable, str(HERE / "tools/mkindex.py"), "--manifests", str(d / "manifests"),
                            "--out-dir", str(d / "bad"), *bad], capture_output=True, text=True)
        if r.returncode == 0 or (d / "bad").exists():
            fails.append(f"mkindex {' '.join(bad[::2])} alone must refuse")

    def media_of(builds, shots=None, pid="demo"):
        d = catalogue(builds, shots)
        code, out, v3 = generate(d)
        got = [p.get("media") for p in v3.get("packages", []) if p["id"] == pid]
        return d, out, v3, got

    # Without [launch], the drawer icon the manifest names.
    d, out, v3, got = media_of([build(info=ICON_DRAWER)])
    if not got or not got[0] or set(got[0]) != {"icon", "icon_small"}:
        fails.append(f"the drawer icon when there is no program icon: {out} {got}")
    else:
        img = Image.open(io.BytesIO(served(d, got[0]["icon"])))
        truecolour("drawer icon (a palette PNG)", served(d, got[0]["icon"]), size=(64, 64))
        # 40 x 20 keeps its proportions: rows above and below stay clear.
        want("drawer icon: its proportions kept", img.getbbox(), (12, 22, 52, 42))
    # A program icon that is not a PNG: the drawer's PNG icon is taken.
    d, out, v3, got = media_of([build(members=[("Demo/", b""), ("Demo/Demo", b"p"), ("Demo/Demo.info", CLASSIC),
                                               ("Demo.info", ICON_DRAWER)], extra=LAUNCH)])
    if not got or not got[0] or "icon" not in got[0]:
        fails.append(f"a classic program icon beside a PNG drawer icon: {out} {got}")
    # No PNG icon at all: no icon, no media key, no icon archive.
    d, out, v3, got = media_of([build(info=CLASSIC)])
    if got != [None] or "media_bundle" in v3 or any((d / "media").glob("icons-*")):
        fails.append(f"a classic icon must give no icon: {out} {got}")
    if not list((d / "media" / cm.SOURCES).glob("*.json")):
        fails.append("an archive without a PNG icon is not recorded as read")
    # ...and screenshots alone still give a gallery.
    d, out, v3, got = media_of([build(info=CLASSIC)], {"demo/1.png": SHOT})
    if not got or not got[0] or set(got[0]) != {"gallery"} or "media_bundle" in v3:
        fails.append(f"screenshots without an icon: {out} {got}")
    # A file that only starts like a PNG is left out, and named.
    d, out, v3, got = media_of([build(info=cm.PNG_SIG + b"not really")])
    if got != [None] or "media: demo: icon Demo.info" not in out:
        fails.append(f"a broken PNG icon: {out} {got}")

    # One set per application: the icon of the first build in the order
    # x86_64/v11, x86_64/v1, aarch64, the same object in each build.
    a11, a1, arm = png(48, 48, seed=11), png(48, 48, seed=22), png(48, 48, seed=33)
    d, out, v3, got = media_of([build(arch="aarch64", abi="v1", info=arm), build(abi="v1", info=a1),
                                build(abi="v11", info=a11), build(pid="other", info=CLASSIC)])
    if len(got) != 3 or not got[0] or got[0] != got[1] or got[1] != got[2]:
        fails.append(f"three builds, one media object: {out} {got}")
    else:
        img = Image.open(io.BytesIO(served(d, got[0]["icon"]))).convert("RGB")
        want("icon of the x86_64/v11 build", img.getpixel((8, 8)), Image.open(io.BytesIO(a11)).getpixel((0, 0)))
    if [p for p in v3.get("packages", []) if p["id"] == "other" and "media" in p]:
        fails.append("an application without pictures has a media key")
    # The first build has no PNG icon: the next one's is used.
    d, out, v3, got = media_of([build(abi="v11", info=CLASSIC), build(abi="v1", info=a1)])
    if len(got) != 2 or not got[0] or got[0] != got[1]:
        fails.append(f"icon from the second build: {out} {got}")
    else:
        img = Image.open(io.BytesIO(served(d, got[0]["icon"]))).convert("RGB")
        want("icon of the x86_64/v1 build", img.getpixel((8, 8)), Image.open(io.BytesIO(a1)).getpixel((0, 0)))
    # Another upload with the same file name must not give its icon.
    d = catalogue([build(info=a11)])
    (d / "cache/demo-x86_64-v11.zip").write_bytes(archive(info=a1))
    code, out, v3 = generate(d)
    if code or "media" in v3["packages"][0] or "icon not looked for" not in out:
        fails.append(f"an archive with other bytes under the same name: {code} {out}")

    # An LHA archive, read through lhasa as the generator reads them.
    if shutil.which("lha"):
        d, out, v3, got = media_of([build(members=[("Demo/Demo", b"p"), ("Demo/Demo.info", ICON_PROGRAM),
                                                   ("Demo.info", CLASSIC)], extra=LAUNCH, lha=True)])
        if not got or not got[0] or "icon" not in got[0]:
            fails.append(f"an icon from an LHA archive: {out} {got}")
        else:
            truecolour("icon from an LHA archive", served(d, got[0]["icon"]), size=(64, 64))
    else:
        print("MEDIA: lha not installed; the LHA icon case was not run")

    # Publication leaves a bad picture out and keeps the rest.
    d, out, v3, got = media_of([build(info=CLASSIC)],
                               {"demo/1.png": SHOT, "demo/2.png": png(1300, 10), "demo/3.png": SHOT,
                                "demo/captions.txt": b"ok\n\n" + b"a" * 130 + b"\n", "ghost/1.png": SHOT})
    g = (got[0] or {}).get("gallery", []) if got else []
    if len(g) != 2 or g[0].get("caption") != "ok" or "caption" in g[1] or "-3-full.png" not in g[1]["full"]["url"] \
            or "gallery/demo/2.png: 1300 x 10 pixels" not in out or "gallery/ghost" not in out:
        fails.append(f"a bad picture among good ones: {out} {g}")
    # A generated file over 512 KiB is a problem of that picture.
    noise = Image.frombytes("RGB", (1280, 800), os.urandom(1280 * 800 * 3)).quantize(256)
    buf = io.BytesIO(); noise.save(buf, "PNG")
    try:
        cm.shot_pictures(buf.getvalue())
        fails.append("a screenshot whose truecolour form is over 512 KiB was accepted")
    except ValueError as exc:
        if "at most 524288" not in str(exc):
            fails.append(f"oversized generated picture: {exc}")

    # ------------------------------------------------------------- make-media.py
    d = Path(tempfile.mkdtemp(dir=TMP))
    (d / "icon.png").write_bytes(png(128, 128, ctype=6))
    (d / "shot.png").write_bytes(png(800, 600))
    Image.new("RGB", (2000, 1000), (40, 90, 160)).save(d / "photo.jpg", "JPEG")
    Image.new("P", (320, 200), 3).save(d / "pal.png", "PNG")
    tool = [sys.executable, str(HERE / "tools/make-media.py")]
    r = subprocess.run(tool + ["demo", "--icon", str(d / "icon.png"), "--screenshot", str(d / "shot.png"),
                               "--screenshot", str(d / "photo.jpg"), "--screenshot", str(d / "pal.png"),
                               "--caption", 'The "main" window', "--caption", "", "--caption", "Preferences",
                               str(d / "media")], capture_output=True, text=True)
    out = d / "media/demo"
    names = sorted(p.name for p in out.iterdir()) if out.is_dir() else []
    if r.returncode or names != sorted(["icon.png", "icon-small.png", "captions.txt"] + [
            f"{n}{k}.png" for n in (1, 2, 3) for k in ("", "-thumb", "-full")]):
        fails.append(f"make-media.py: {r.returncode} {r.stdout} {r.stderr} {names}")
    else:
        truecolour("make-media icon.png", (out / "icon.png").read_bytes(), size=(64, 64))
        truecolour("make-media icon-small.png", (out / "icon-small.png").read_bytes(), size=(24, 24))
        want("make-media 1.png", truecolour("1.png", (out / "1.png").read_bytes()), (533, 400))
        want("make-media 1-thumb.png", truecolour("1-thumb.png", (out / "1-thumb.png").read_bytes()), (133, 100))
        want("make-media 1-full.png is the source", (out / "1-full.png").read_bytes(), (d / "shot.png").read_bytes())
        want("make-media 2.png (JPEG)", truecolour("2.png", (out / "2.png").read_bytes()), (640, 320))
        want("make-media 2-full.png (JPEG, scaled to fit)", truecolour("2-full.png", (out / "2-full.png").read_bytes()), (1280, 640))
        want("make-media 3-full.png (palette)", truecolour("3-full.png", (out / "3-full.png").read_bytes()), (320, 200))
        want("make-media captions.txt", (out / "captions.txt").read_bytes(), b'The "main" window\n\nPreferences\n')
        for p in out.glob("*.png"):
            if p.stat().st_size > cm.FILE_MAX:
                fails.append(f"make-media {p.name}: larger than 512 KiB")
    # Fewer screenshots later: the old ones past them go, the icon stays.
    r = subprocess.run(tool + ["demo", "--screenshot", str(d / "shot.png"), "--no-full", str(d / "media")],
                       capture_output=True, text=True)
    want("make-media: a second run with one screenshot", sorted(p.name for p in out.iterdir()),
         ["1-thumb.png", "1.png", "icon-small.png", "icon.png"])
    for args, text in ((["Demo", "--icon", str(d / "icon.png")], "not a package id"),
                       (["demo"], "nothing to make"),
                       (["demo", "--screenshot", str(d / "shot.png"), "--caption", "a", "--caption", "b"], "2 captions"),
                       (["demo", "--icon", str(d / "captions-missing.png")], "")):
        r = subprocess.run(tool + args + [str(d / "refused")], capture_output=True, text=True)
        if r.returncode == 0 or text not in r.stderr or (d / "refused").exists():
            fails.append(f"make-media.py {args[1:2]}: must refuse with {text!r}: {r.returncode} {r.stderr}")

    # ------------------------------------------------------------- the publish step
    import test_publish_index as t
    t.archive = lambda name, readme=b"hello\n": archive(name, info=png(48, 48, seed=len(name) + len(readme)))
    os.environ["MEDIA_BASE_URL"] = BASE
    try:
        w = t.World()
        (w.dev / "gallery/demo").mkdir(parents=True)
        (w.dev / "gallery/demo/1.png").write_bytes(SHOT)
        w.merge("screenshots of demo")
        code, out = w.publish()
        v3 = json.loads(w.show("index-v3.json") or "{}")
        m = (v3.get("packages") or [{}])[0].get("media") or {}
        first = w.sha("media")
        if code or not first or set(m) != {"icon", "icon_small", "gallery"} or "media_bundle" not in v3 \
                or "media pushed" not in out or w.sha("published") != w.sha("main"):
            fails.append(f"publication with pictures: {code} {out}")
        else:
            listed = set(git(w.origin, "ls-tree", "-r", "--name-only", "media").splitlines())
            named = [m["icon"], m["icon_small"], v3["media_bundle"]] + [e[k] for e in m["gallery"] for k in ("full", "view", "thumb")]
            for obj in named:
                rel = obj["url"][len(BASE):]
                blob = subprocess.run(["git", "show", f"media:{rel}"], cwd=w.origin, capture_output=True).stdout
                if rel not in listed or hashlib.sha256(blob).hexdigest() != obj["sha256"]:
                    fails.append(f"{rel}: named by the index on main, not on the media branch with those bytes")
            want("the media branch starts without a parent", git(w.origin, "rev-list", "--count", "media"), "1")
            if set(git(w.origin, "ls-tree", "-r", "--name-only", "main").splitlines()) & listed \
                    or any(n.startswith("manifests/") or n.startswith("index") for n in listed):
                fails.append("catalogue files and pictures are mixed between main and media")
            if '"media' in (w.show("index-v2.json") or "") or '"media' in (w.show("index.json") or ""):
                fails.append("published index-v2.json or index.json carries pictures")
        # Nothing new: nothing pushed, the branch stays.
        code, out = w.publish(start=w.sha("main"))
        if code or "media unchanged" not in out or w.sha("media") != first:
            fails.append(f"a second run with nothing new: {code} {out}")
        # Another package: the branch gains a commit on top, keeps every
        # file, and demo keeps its icon though its archive is not read again.
        w.package("other", "1.0"); w.merge("other 1.0")
        code, out = w.publish(start=w.sha("main"))
        after = set(git(w.origin, "ls-tree", "-r", "--name-only", "media").splitlines())
        v3 = json.loads(w.show("index-v3.json") or "{}")
        by_id = {p["id"]: p.get("media") or {} for p in v3.get("packages", [])}
        if code or git(w.origin, "rev-parse", "media~1") != first or not listed <= after \
                or by_id.get("demo", {}).get("icon") != m.get("icon") or "icon" not in by_id.get("other", {}):
            fails.append(f"a second publication with pictures: {code} {out}")
        # The pictures are served before the catalogue that names them: when
        # main moves during the run, the media push has happened and the
        # catalogue's has not.
        w2 = t.World(); w2.publish()
        media0, main0 = w2.sha("media"), w2.sha("main")
        w2.package("third", "1.0"); a = w2.merge("third 1.0")
        code, out = w2.publish(start=a, attempts=1, before_push=t.merge_in_other_clone(w2, "fourth", "1.0", "PR 2"))
        if code == 0 or "push refused" not in out or w2.sha("media") == media0 \
                or git(w2.origin, "rev-parse", "media~1") != media0 \
                or "third" in (w2.show("index-v3.json") or ""):
            fails.append(f"a refused catalogue push after the media push: {code} {out}")
        code, out = w2.publish(start=w2.sha("main"))
        # (The package merged meanwhile comes from another process, with the
        # classic icon of test_publish_index.py: it has no pictures.)
        ids = {p["id"] for p in json.loads(w2.show("index-v3.json") or "{}").get("packages", []) if "media" in p}
        if code or ids != {"demo", "third"}:
            fails.append(f"the run after a refused push: {code} {out} {ids}")
    finally:
        del os.environ["MEDIA_BASE_URL"]


if HAVE_PILLOW:
    with_pillow()
shutil.rmtree(TMP, ignore_errors=True)
if fails:
    print("\n".join(fails)); print(f"MEDIA: {len(fails)} failed"); sys.exit(1)
if HAVE_PILLOW:
    print("MEDIA: gallery rules, submit --screenshot, icons, screenshots, the icon archive, "
          "make-media.py and publication with pictures hold")
else:
    print("MEDIA: gallery rules and submit --screenshot hold; MAKING PICTURES WAS NOT TESTED "
          "(Pillow is not installed: pip install pillow)")
