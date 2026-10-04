#!/usr/bin/env python3
"""Check an arospkg-index checkout the way its pull requests are checked.

    ci_check_index.py INDEX_DIR [--base REF] [--out-dir DIR] [--cache DIR]

For every manifest changed since REF (all of them without --base): download
the archive at its url, within the client's limits, and check that its size
and SHA-256 are the manifest's; then generate both catalogue files with
mkindex.py into OUT_DIR, which also checks subdir and icon against the
downloaded archives. Exit 1 on any problem.

Nothing from the checkout is executed: manifests are read as TOML data,
archives are only listed, never unpacked or run. That is what lets this run
on an untrusted pull request without secrets.
"""
import argparse
import hashlib
import subprocess
import sys
import tomllib
import urllib.request
from pathlib import Path

HERE = Path(__file__).resolve().parent
DOWNLOAD_MAX = 64 * 1024 * 1024
TIMEOUT = 300


def changed(index_dir, base):
    out = subprocess.run(["git", "diff", "--name-only", "--diff-filter=AMR", f"{base}...HEAD", "--",
                          "manifests/", "overrides/"],
                         cwd=index_dir, capture_output=True, text=True, check=True).stdout
    names = set()
    for line in out.splitlines():
        names.add(Path(line).name)
    return sorted(index_dir / "manifests" / n for n in names if (index_dir / "manifests" / n).exists())


def download(url, dest):
    req = urllib.request.Request(url, headers={"User-Agent": "arospkg-index-check"})
    with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
        data = r.read(DOWNLOAD_MAX + 1)
    if len(data) > DOWNLOAD_MAX:
        raise ValueError(f"larger than {DOWNLOAD_MAX} bytes")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_bytes(data)
    return data


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("index_dir")
    ap.add_argument("--base", help="check only manifests changed since this git ref")
    ap.add_argument("--out-dir", default=None, help="where the trial catalogue goes (default: a temporary directory)")
    ap.add_argument("--cache", default=".cache/ci-archives")
    ap.add_argument("--use-local-copies", metavar="DIR", help=argparse.SUPPRESS)
    a = ap.parse_args()
    index_dir = Path(a.index_dir).resolve()
    base_ok = a.base and subprocess.run(["git", "rev-parse", "--verify", "--quiet", a.base + "^{commit}"],
                                        cwd=index_dir, capture_output=True).returncode == 0
    if a.base and not base_ok:
        print(f"{a.base}: not a commit here (a first push?); checking every manifest")
    files = changed(index_dir, a.base) if base_ok else sorted((index_dir / "manifests").glob("*.toml"))
    cache = Path(a.cache).resolve()
    problems = 0
    for f in files:
        m = tomllib.loads(f.read_text(encoding="utf-8"))
        if m.get("status") != "approved" or m.get("excluded"):
            continue
        url = m.get("url", "")
        if not url.startswith("https://"):
            print(f"{f.name}: url is not https://"); problems += 1; continue
        try:
            name = url.rsplit("/", 1)[-1]
            if a.use_local_copies:          # tests: the bytes "at" the url
                data = (Path(a.use_local_copies) / name).read_bytes()
                (cache / name).parent.mkdir(parents=True, exist_ok=True)
                (cache / name).write_bytes(data)
            else:
                data = download(url, cache / name)
        except Exception as exc:
            print(f"{f.name}: cannot download {url}: {exc}"); problems += 1; continue
        sha = hashlib.sha256(data).hexdigest()
        if sha != m.get("sha256") or len(data) != m.get("size"):
            print(f"{f.name}: the file at its url is {len(data)} bytes, sha256 {sha}; "
                  f"the manifest says {m.get('size')} bytes, sha256 {m.get('sha256')}")
            problems += 1
            continue
        print(f"{f.name}: archive matches ({len(data)} bytes)", flush=True)
    sys.stdout.flush()
    import tempfile
    with tempfile.TemporaryDirectory() as tmp:
        out = Path(a.out_dir) if a.out_dir else Path(tmp)
        out.mkdir(parents=True, exist_ok=True)
        # The trial starts from the published index.json, so its rule (keep
        # the packages it lists) is checked against the real file.
        prev = index_dir / "index.json"
        if prev.exists() and out.resolve() != index_dir:
            (out / "index.json").write_bytes(prev.read_bytes())
        r = subprocess.run([sys.executable, str(HERE / "mkindex.py"), "--manifests", str(index_dir / "manifests"),
                            "--out-dir", str(out), "--cache", str(cache)])
        if r.returncode:
            problems += 1
    if problems:
        print(f"\n{problems} problem(s)")
        sys.exit(1)
    print("\nall checks pass")


if __name__ == "__main__":
    main()
