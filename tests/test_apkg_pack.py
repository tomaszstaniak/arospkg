"""apkg-pack on a made-up drawer: init, check, build twice to the same bytes,
check of the archive, and submit into a scratch arospkg-index, with the
refusals an author and a maintainer meet. Host only: nothing here runs on
AROS, and nothing reaches the published catalogue."""
import hashlib, subprocess, sys, tempfile, tomllib
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
PACK = [sys.executable, str(HERE / "tools/apkg-pack")]
fails = []


def run(*args, inp=None):
    r = subprocess.run(PACK + [str(a) for a in args], capture_output=True, text=True, input=inp)
    return r.returncode, r.stdout + r.stderr


def elf(machine=62, ver=b"$VER: xRick 1.0.2 (01.01.2020)"):
    h = bytearray(64); h[:4] = b"\x7fELF"; h[4] = 2; h[5] = 1
    h[18:20] = machine.to_bytes(2, "little")
    return bytes(h) + b"\0" * 64 + ver + b"\0"


with tempfile.TemporaryDirectory() as d:
    d = Path(d)
    dr = d / "build" / "xRick"
    (dr / "data" / "saves").mkdir(parents=True)        # an empty drawer must survive packing
    (dr / "xRick").write_bytes(elf())
    (dr / "ReadMe").write_text("xRick\n")
    (dr / ".DS_Store").write_bytes(b"junk")
    (d / "build" / "xRick.info").write_bytes(b"\xe3\x10icon")

    # init: what it can read, it reads; ABI and requirements it is told.
    code, out = run("init", dr, "--non-interactive", "--summary", "Rick Dangerous clone")
    if code == 0 or "abi" not in out or "requires_system" not in out:
        fails.append(f"init without abi/requirements must refuse and name them: {out}")
    code, out = run("init", dr, "--non-interactive", "--summary", "Rick Dangerous clone",
                    "--abi", "v11", "--kind", "app", "--requires", "SDL2.library", "--no-depends",
                    "--category", "game/platform",
                    "--note", "The game needs no extra data.")
    mf = d / "build" / "xRick.arospkg.toml"
    m = tomllib.loads(mf.read_text()) if mf.exists() else {}
    if code or m.get("id") != "xrick" or m.get("version") != "1.0.2" or m.get("arch") != "x86_64" \
            or m.get("icon") != "xRick.info" or "url" in m:
        fails.append(f"init: {code} {out} {m}")
    code, out = run("init", dr, "--non-interactive", "--abi", "v11")
    if code == 0 or "does not overwrite" not in out:
        fails.append("init must not overwrite an existing description")

    code, out = run("check", dr)
    if code or not out.startswith("ok: xrick 1.0.2 for x86_64/v11: 3 files"):
        fails.append(f"check of the drawer: {out}")

    # The error names the field and what to do.
    good = mf.read_text()
    mf.write_text(good.replace('"The game needs no extra data."', '"' + "x" * 184 + '"'))
    code, out = run("check", dr)
    if code == 0 or "post_install_notes[0]: 184 characters; maximum is 159" not in out:
        fails.append(f"long note: {out}")
    mf.write_text('url = "https://example.invalid/x.zip"\n' + good)
    code, out = run("check", dr)
    if code == 0 or "url: set when the archive is submitted" not in out:
        fails.append(f"url in an author manifest: {out}")
    mf.write_text(good)

    z1, z2 = d / "xrick-1.zip", d / "xrick-2.zip"
    for z in (z1, z2):
        code, out = run("build", dr, "--output", z)
        if code:
            fails.append(f"build: {out}")
    if z1.read_bytes() != z2.read_bytes():
        fails.append("two builds of the same drawer differ")
    import zipfile
    names = zipfile.ZipFile(z1).namelist()
    if "xRick/data/saves/" not in names or ".arospkg/manifest.toml" not in names \
            or "xRick.info" not in names or any(".DS_Store" in n for n in names):
        fails.append(f"archive members: {names}")
    code, out = run("build", dr, "--output", dr / "self.zip")
    if code == 0:
        fails.append("build must refuse an output inside the drawer")
    code, out = run("check", z1)
    if code or "ok: xrick 1.0.2" not in out:
        fails.append(f"check of the archive: {out}")

    # submit into a scratch index
    idx = d / "arospkg-index"; (idx / "manifests").mkdir(parents=True)
    url = "https://example.invalid/releases/xrick.x86_64-aros-v11.zip"
    code, out = run("submit", url, "--index", idx, "--use-local-copy", z1, "--dry-run")
    if code or "+sha256" not in out.replace(" ", "") or (idx / "manifests/xrick.x86_64.toml").exists():
        fails.append(f"submit --dry-run: {out}")
    code, out = run("submit", url, "--index", idx, "--use-local-copy", z1)
    written = idx / "manifests/xrick.x86_64.toml"
    if code or not written.exists():
        fails.append(f"submit: {out}")
    else:
        w = tomllib.loads(written.read_text())
        if w["sha256"] != hashlib.sha256(z1.read_bytes()).hexdigest() or w["size"] != z1.stat().st_size \
                or w["url"] != url or w["status"] != "approved" or w["post_install_notes"] != ["The game needs no extra data."]:
            fails.append(f"submitted manifest: {w}")
    code, out = run("submit", url, "--index", idx, "--use-local-copy", z1)
    if code or "already up to date" not in out:
        fails.append(f"the same release again: {out}")
    # A maintainer's correction survives the next submission.
    (idx / "overrides").mkdir()
    (idx / "overrides/xrick.x86_64.toml").write_text('category = "game/action"\n')
    code, out = run("submit", url, "--index", idx, "--use-local-copy", z1)
    if code or tomllib.loads(written.read_text()).get("category") != "game/action":
        fails.append(f"override: {out}")
    # Other bytes under the same version and revision: refused, not renumbered.
    (dr / "ReadMe").write_text("xRick, changed\n")
    run("build", dr, "--output", z2)
    code, out = run("submit", url, "--index", idx, "--use-local-copy", z2)
    if code == 0 or "does not renumber" not in out:
        fails.append(f"changed bytes under the same release: {out}")
    # And the generator accepts what submit wrote.
    r = subprocess.run([sys.executable, str(HERE / "tools/mkindex.py"), "--manifests", str(idx / "manifests"),
                        "--out-dir", str(idx), "--cache", str(d / "nocache")], capture_output=True, text=True)
    if r.returncode or '"id": "xrick"' not in (idx / "index-v2.json").read_text():
        fails.append(f"mkindex on the submitted manifest: {r.stdout}")

if fails:
    print("\n".join(fails)); print(f"APKG-PACK: {len(fails)} failed"); sys.exit(1)
print("APKG-PACK: init, check, build, submit and their refusals hold")
