#!/usr/bin/env python3
"""Survey candidate archives for v0.1: layout, what the docs say, and which
names the binaries reference that AROS One 1.3 does not ship.

An aid for a human decision, with the same limits as deps.py, plus one the
user named explicitly: the archive's LAYOUT proves nothing about what the
program needs in LIBS: or Fonts:. A drawer-only archive can still tell you, in
its readme, to copy a font somewhere -- so the docs are read too, and the
program is run before anything is approved.

    survey.py <zip>...        prints one block per archive
"""
import pathlib, re, sys, zipfile

HERE = pathlib.Path(__file__).resolve().parent.parent
BASE = HERE / "docs/spikes/twenty"
def load(name):
    return {l.strip() for l in (BASE / name).read_text().splitlines() if l.strip()}
HAVE = (load("aros-one-1.3-libs.txt") | load("aros-one-1.3-rom.txt") |
        load("aros-one-1.3-classes.txt") | load("aros-one-1.3-devices.txt"))
# Provided at run time rather than by a file in Libs/ or Devs/.
RUNTIME = {"bsdsocket.library": "AROSTCP", "console.device": "ROM console handler",
           "ramdrive.device": "ROM", "trackdisk.device": "absent on PC; harmless if unused"}
FONTS = load("aros-one-1.3-fonts.txt")

REF = re.compile(rb"[A-Za-z0-9_.+-]{2,40}\.(?:library|device|datatype|mcc|mcp|gadget|image|class|font)\b")
DOCHINT = re.compile(r"(?i)(libs:|fonts:|devs:|classes:|c:|s:|copy\s+\S+\s+to|install|require|needs?\b|mui|\.mcc|\.font|\.library|ixemul|arexx)")

def is_doc(n):
    l = n.lower()
    return l.endswith((".txt", ".readme", ".guide", ".doc", ".md", "readme", "readme.txt", "liesmich")) \
        or "/readme" in l or l.startswith("readme")

def survey(path):
    z = zipfile.ZipFile(path)
    names = [n for n in z.namelist() if not n.endswith("/")]
    tops = sorted({n.split("/")[0] for n in names if "/" in n})
    loose = [n for n in names if "/" not in n]
    icon = next((l for l in loose if l.lower().endswith(".info") and l[:-5] in tops), "")
    refs, execs = {}, []
    for n in names:
        data = z.read(n)
        if data[:4] == b"\x7fELF":
            execs.append(n)
            for m in set(REF.findall(data)):
                refs.setdefault(m.decode().lower(), set()).add(n.split("/")[-1])
    missing = {r: w for r, w in refs.items()
               if r not in HAVE and r not in RUNTIME and not r.endswith(".font")}
    fonts = {r: w for r, w in refs.items() if r.endswith(".font") and r not in FONTS}
    docs = []
    for n in names:
        if is_doc(n):
            txt = z.read(n).decode("latin-1", "replace")
            hits = sorted({m.group(0).lower() for m in DOCHINT.finditer(txt)})
            docs.append((n, hits))
    print(f"== {path.name}")
    print(f"   drawer {tops}  icon {icon or '-'}  files {len(names)}  executables {len(execs)}")
    print(f"   references not on AROS One: {', '.join(sorted(missing)) or 'none'}")
    if fonts: print(f"   fonts not on AROS One: {', '.join(sorted(fonts))}")
    rt = sorted(r for r in refs if r in RUNTIME)
    if rt: print(f"   runtime-provided: {', '.join(rt)}")
    for n, hits in docs:
        print(f"   doc {n}: {', '.join(hits) if hits else '(no install hints)'}")
    if not docs: print("   doc: NONE in the archive")

for a in sys.argv[1:]:
    survey(pathlib.Path(a))
