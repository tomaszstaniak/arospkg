#!/usr/bin/env python3
"""tools/mkindex.py: what it refuses, and what it does when it refuses.

Each case changes one field of a manifest that passes, and names the reason
that must appear. The publication cases run the tool itself and check its
exit status and that a previous index survives a failed run.
"""
import subprocess, sys, tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(HERE / "tools"))
import mkindex  # noqa: E402

GOOD = dict(status="approved", id="demo", version="1.0", arch="x86_64", abi="v11",
            url="https://example.invalid/demo.lha", size=10, sha256="a" * 64,
            kind="app", depends=[], depends_checked=True, subdir="Demo",
            icon="Demo.info", requires_system=[{"type": "library", "id": "crt.library"}])
fails = []


def expect(name, change, want):
    m = {**GOOD, **change}
    for k, v in change.items():
        if v is None: del m[k]
    got = mkindex.check(m)
    if want is None:
        if got: fails.append(f"{name}: expected to pass, got {got}")
    elif not any(want in p for p in got):
        fails.append(f"{name}: expected a reason with {want!r}, got {got}")


expect("the base passes", {}, None)
expect("abi missing", {"abi": None}, "abi None is not one the client knows")
expect("abi unknown", {"abi": "v0"}, "abi 'v0' is not one the client knows")
expect("abi v1", {"abi": "v1"}, None)
expect("plain http", {"url": "http://example.invalid/demo.lha"}, "url is not https://")
expect("revision 0", {"revision": 0}, "revision 0 is not an integer")
expect("revision negative", {"revision": -1}, "revision -1 is not an integer")
expect("revision text", {"revision": "2"}, "revision '2' is not an integer")
expect("revision bool", {"revision": True}, "revision True is not an integer")
expect("revision 4", {"revision": 4}, None)
expect("size text", {"size": "10"}, "size '10' is not a positive integer")
expect("quote in version", {"version": '1.0 "beta"'}, "version '1.0 \"beta\"' has a character")
expect("backslash in subdir", {"subdir": "Demo\\x"}, "subdir")
expect("non-ASCII summary", {"summary": "Café"}, "summary 'Café' has a character")
expect("long url", {"url": "https://e.invalid/" + "a" * 500}, "url is 518 characters")
expect("long id", {"id": "a" * 64}, "id is 64 characters")
expect("long licence is a note", {"license": "x" * 80}, None)
expect("requirement not ASCII", {"requires_system": [{"type": "library", "id": "é.library"}]},
       "requires_system[0] id")
expect("min_version bool", {"requires_system": [{"type": "library", "id": "a.library", "min_version": True}]},
       "min_version True")


def toml(m):
    lines = []
    for k, v in m.items():
        if k == "requires_system": continue
        if isinstance(v, bool): lines.append(f"{k} = {'true' if v else 'false'}")
        elif isinstance(v, (int, list)): lines.append(f"{k} = {v!r}".replace("'", '"'))
        else: lines.append(f'{k} = "{v}"')
    return "\n".join(lines) + "\n"


def run(manifests):
    with tempfile.TemporaryDirectory() as d:
        d = Path(d); (d / "m").mkdir()
        for name, text in manifests.items():
            (d / "m" / name).write_text(text)
        out = d / "index.json"
        out.write_text("PREVIOUS\n")
        r = subprocess.run([sys.executable, str(HERE / "tools/mkindex.py"), "--manifests", str(d / "m"),
                            "--out", str(out), "--cache", str(d / "nocache")], capture_output=True, text=True)
        return r.returncode, out.read_text(), r.stdout


skel = toml({**GOOD, "id": "sk", "status": "skeleton", "sha256": ""})
code, text, _ = run({"demo.x86_64.toml": toml(GOOD), "sk.x86_64.toml": skel})
if code != 0 or text == "PREVIOUS\n":
    fails.append(f"a skeleton beside a good manifest: exit {code}, index {'kept' if text == 'PREVIOUS\n' else 'written'}")

bad = toml({**GOOD, "id": "bad", "url": "http://example.invalid/b.lha"})
code, text, out = run({"demo.x86_64.toml": toml(GOOD), "bad.x86_64.toml": bad})
if code == 0 or text != "PREVIOUS\n" or "NOT WRITTEN" not in out:
    fails.append(f"an approved manifest refused: exit {code}, previous index {'kept' if text == 'PREVIOUS\n' else 'replaced'}")

dep = toml({**GOOD, "id": "dep", "depends": ["nothere"]})
code, text, _ = run({"demo.x86_64.toml": toml(GOOD), "dep.x86_64.toml": dep})
if code == 0 or text != "PREVIOUS\n":
    fails.append(f"an unmet dependency: exit {code}")

many = {f"p{i:03}.x86_64.toml": toml({**GOOD, "id": f"p{i:03}"}) for i in range(260)}
code, text, out = run(many)
if code == 0 or text != "PREVIOUS\n" or "larger than the 0.3 client can read" not in out:
    fails.append(f"an index past the client's capacity: exit {code}")

if fails:
    print("\n".join(fails)); print(f"MKINDEX: {len(fails)} failed"); sys.exit(1)
print("MKINDEX: all refusals and publication rules hold")
