#!/usr/bin/env python3
"""The automatic review of one catalogue entry: what "Reviewed" means.

    review.py MANIFEST ARCHIVE [--override FILE]   print the record as JSON

A variant is Reviewed when this suite passed for its archive bytes and its
metadata; nobody declares it. The record says which suite ran, on which
bytes (sha256, size) and on which metadata (manifest_digest: the parsed
manifest with its override, so a comment or a reformatted line is not a
change and a changed value is), what each check found, and what the suite
could not decide from the files alone.

What it does not do: install, unpack onto a system, or start anything.
Whether a program runs is a runtime test, recorded apart, per system.

Nothing from the archive is executed. ZIP members are read with zipfile;
LHA members are listed with lhasa and, only after every name passed the
path checks, printed to a pipe one at a time to read their first bytes;
nothing is unpacked to disk.
"""
import argparse
import hashlib
import json
import subprocess
import sys
import tempfile
import zipfile
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mkindex  # noqa: E402

# Raised when a check is added, removed or made stricter. A record of an
# older suite still counts as what it was: the catalogue shows its number.
SUITE = 1

# ELF e_machine of the CPUs the catalogue carries.
ELF_MACHINE = {"x86_64": 62, "i386": 3, "aarch64": 183}
MACHINE_NAME = {62: "x86_64", 3: "i386", 183: "aarch64", 20: "ppc", 40: "arm", 4: "m68k"}
HUNK = b"\x00\x00\x03\xf3"          # AmigaOS 68k executable


def manifest_digest(m):
    """The metadata a review covered. Parsed values only, keys sorted."""
    return hashlib.sha256(json.dumps(m, sort_keys=True, ensure_ascii=True, default=str).encode()).hexdigest()


def collisions(names):
    """Names that would land on one another on an AROS file system, which
    compares names without regard to case: the same name twice, two names
    equal but for case, and a file with the name of a drawer."""
    problems, seen, files, dirs = [], {}, set(), set()
    for n in names:
        key = n.rstrip("/").lower()
        if key in seen:
            what = "twice" if seen[key] == n else f"and {seen[key]!r}, which differ only in case"
            problems.append(f"member {n!r} {what}: one would overwrite the other when installed")
        else:
            seen[key] = n
        parts = key.split("/")
        for i in range(1, len(parts)):
            dirs.add("/".join(parts[:i]))
        if n.endswith("/"):
            dirs.add(key)
        else:
            files.add(key)
    for f in sorted(files & dirs):
        problems.append(f"member {f!r} is both a file and a drawer")
    return problems


def elf_machine(head):
    """e_machine of an ELF header, or None when it is not ELF."""
    if len(head) < 20 or head[:4] != b"\x7fELF":
        return None
    order = "little" if head[5] == 1 else "big"
    return int.from_bytes(head[18:20], order)


def member_heads(path, names):
    """{member: first 64 bytes} of every file member. Nothing is unpacked to
    disk: ZIP members are read with zipfile, LHA members are printed to a
    pipe by lhasa one at a time and only their first bytes are read. A member
    lhasa cannot print (a name in an 8-bit charset it lists differently) is
    left out, and check_cpu reports it as unread."""
    files = [n for n in names if not n.endswith("/")]
    out = {}
    if path.open("rb").read(4) == b"PK\x03\x04":
        with zipfile.ZipFile(path) as z:
            for n in files:
                with z.open(n) as f:
                    out[n] = f.read(64)
        return out
    for n in files:
        proc = subprocess.Popen(["lha", "pq", str(path.resolve()), n], stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL)
        head = proc.stdout.read(64)
        proc.stdout.close()
        proc.kill()
        proc.wait()
        if head:
            out[n] = head
    return out


def check_cpu(m, heads, nfiles=None):
    """Every program in the archive is for the entry's CPU, the declared
    program above all. Returns (result, detail): result "passed", "failed"
    or "not-applicable" (no ELF program in what could be read)."""
    unread = (nfiles - len(heads)) if nfiles is not None else 0
    result, detail = _check_cpu(m, heads)
    if unread:
        launch = m.get("launch")
        exe = ((m.get("subdir") + "/") if m.get("subdir") else "") + launch.get("executable", "") \
            if isinstance(launch, dict) else None
        if exe and exe not in heads:
            # The program the entry starts is the one that matters most; a
            # review that could not read it has not checked the CPU.
            return "failed", f"the program {exe} could not be read ({unread} member(s) unread)"
        detail += f"; {unread} member(s) could not be read here"
    return result, detail


def _check_cpu(m, heads):
    want = ELF_MACHINE[m["arch"]]
    elf = {n: elf_machine(h) for n, h in heads.items()}
    elf = {n: k for n, k in elf.items() if k is not None}
    hunks = sorted(n for n, h in heads.items() if h[:4] == HUNK)
    other = sorted(n for n, k in elf.items() if k != want)
    ours = sorted(n for n, k in elf.items() if k == want)
    launch = m.get("launch")
    if isinstance(launch, dict):
        exe = ((m.get("subdir") + "/") if m.get("subdir") else "") + launch.get("executable", "")
        if exe in elf and elf[exe] != want:
            return "failed", (f"the program {exe} is {MACHINE_NAME.get(elf[exe], elf[exe])}, "
                              f"the entry says {m['arch']}")
        if exe in hunks:
            return "failed", f"the program {exe} is a 68k (hunk) executable, the entry says {m['arch']}"
    if not elf:
        if hunks:
            return "failed", f"only 68k (hunk) executables, the entry says {m['arch']}"
        return "not-applicable", "no ELF program in the archive"
    if not ours:
        kinds = sorted({MACHINE_NAME.get(k, str(k)) for k in elf.values()})
        return "failed", f"no program for {m['arch']}; its ELF files are for {', '.join(kinds)}"
    detail = f"{len(ours)} ELF file(s) for {m['arch']}"
    if other or hunks:
        detail += f"; also {len(other) + len(hunks)} for another CPU ({', '.join((other + hunks)[:3])})"
    return "passed", detail


def review(m, archive, data=None, today=None):
    """The record for manifest dict `m` (override applied) and the archive
    file. `data`: its bytes when already read."""
    archive = Path(archive)
    data = archive.read_bytes() if data is None else data
    sha = hashlib.sha256(data).hexdigest()
    checks, unchecked = [], []

    def add(name, result, detail=""):
        checks.append({"name": name, "result": result, "detail": detail})

    meta = mkindex.check(m)
    add("metadata", "failed" if meta else "passed", "; ".join(meta))
    ok_dl = sha == m.get("sha256") and len(data) == m.get("size")
    add("download", "passed" if ok_dl else "failed",
        "" if ok_dl else f"{len(data)} bytes, sha256 {sha}; the entry says "
                         f"{m.get('size')} bytes, sha256 {m.get('sha256')}")
    names = None
    try:
        names = mkindex.archive_list(archive)
        add("archive", "passed", f"{len(names)} members")
    except Exception as exc:
        add("archive", "failed", f"cannot be read: {exc}")
    if names is not None:
        unsafe = [p for p in mkindex.check_members({**m, "icon": "", "launch": None, "subdir": ""}, set(names))
                  if p.startswith("member ")]
        clash = collisions(names) + [f"member {n!r} is a symbolic link; the client installs none"
                                     for n in mkindex.archive_links(archive)]
        add("paths", "failed" if unsafe or clash else "passed", "; ".join(unsafe + clash))
        layout = [p for p in mkindex.check_members(m, set(names)) if not p.startswith("member ")]
        app = (m["subdir"] + "/" if m.get("subdir") else "") + mkindex.APP_FILE
        if app in names and not unsafe:
            layout += mkindex.app_description_problems(m, mkindex.archive_member(archive, app), app)
        add("layout", "failed" if layout else "passed", "; ".join(layout))
        if m.get("arch") not in ELF_MACHINE:
            unchecked.append({"name": "cpu", "why": f"no ELF machine known for {m.get('arch')!r}"})
        elif unsafe or clash:
            unchecked.append({"name": "cpu", "why": "member names failed the path checks; nothing was read"})
        else:
            result, detail = check_cpu(m, member_heads(archive, names),
                                       len({n for n in names if not n.endswith("/")}))
            if result == "not-applicable":
                unchecked.append({"name": "cpu", "why": detail})
            else:
                add("cpu", result, detail)
    # The ELF header names the CPU, not the ABI: ABIv1 and ABIv11 programs
    # for x86_64 look alike there, and saying otherwise would be a guess.
    unchecked.append({"name": "abi", "why": "ELF headers do not record the AROS ABI"})
    passed = all(c["result"] == "passed" for c in checks)
    return {
        "schema": 1,
        "suite": SUITE,
        "id": m.get("id"), "arch": m.get("arch"), "abi": m.get("abi"),
        "date": today or utc_today(),
        "sha256": sha, "size": len(data),
        "manifest_digest": manifest_digest(m),
        "result": "passed" if passed else "failed",
        "checks": checks,
        "unchecked": unchecked,
    }


def utc_today():
    return datetime.now(timezone.utc).strftime("%Y-%m-%d")


def record_applies(rec, m):
    """Whether a stored record still describes this entry: the same bytes
    and the same metadata, and it passed."""
    return (isinstance(rec, dict) and rec.get("result") == "passed"
            and rec.get("sha256") == m.get("sha256") and rec.get("size") == m.get("size")
            and rec.get("manifest_digest") == manifest_digest(m)
            and isinstance(rec.get("suite"), int))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("manifest")
    ap.add_argument("archive")
    a = ap.parse_args()
    import tomllib
    f = Path(a.manifest)
    m = tomllib.loads(f.read_text(encoding="utf-8"))
    ov = mkindex.overrides_for(f)
    if isinstance(ov, str):
        sys.exit(ov)
    m = {**m, **(ov or {})}
    rec = review(m, a.archive)
    print(json.dumps(rec, indent=2))
    sys.exit(0 if rec["result"] == "passed" else 1)


if __name__ == "__main__":
    main()
