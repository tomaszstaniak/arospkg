"""ci_check_index.py, the check an arospkg-index pull request gets, on a
scratch repository: it must refuse what submit refuses even when a pull
request is written by hand, fail on an archive it cannot read, and check a
change made only to an override. Host only."""
import hashlib, io, subprocess, sys, tempfile, zipfile
from pathlib import Path

HERE = Path(__file__).resolve().parents[1]
fails = []


def archive(readme=b"hello\n"):
    b = io.BytesIO()
    with zipfile.ZipFile(b, "w") as z:
        z.writestr("Demo/", b""); z.writestr("Demo/ReadMe", readme); z.writestr("Demo.info", b"icon")
    return b.getvalue()


def manifest(data, version="1.0", url="https://example.invalid/demo.zip", extra=""):
    return (f'status = "approved"\nid = "demo"\nversion = "{version}"\nsummary = "Demo"\n'
            f'arch = "x86_64"\nabi = "v11"\nkind = "app"\nurl = "{url}"\nsize = {len(data)}\n'
            f'sha256 = "{hashlib.sha256(data).hexdigest()}"\ndepends = []\ndepends_checked = true\n'
            f'subdir = "Demo"\nicon = "Demo.info"\n{extra}')


def git(repo, *args):
    subprocess.run(["git", "-c", "user.name=t", "-c", "user.email=t@t", *args], cwd=repo, check=True,
                   capture_output=True)


def check(repo, files):
    with tempfile.TemporaryDirectory() as local:
        for name, data in files.items():
            (Path(local) / name).write_bytes(data)
        r = subprocess.run([sys.executable, str(HERE / "tools/ci_check_index.py"), str(repo), "--base", "base",
                            "--use-local-copies", local, "--cache", str(Path(local) / "cache")],
                           capture_output=True, text=True)
    return r.returncode, r.stdout + r.stderr


def scenario(change):
    """A repository whose base has demo 1.0, then `change` applied on top."""
    d = Path(tempfile.mkdtemp())
    (d / "manifests").mkdir()
    a = archive()
    (d / "manifests/demo.x86_64.v11.toml").write_text(manifest(a))
    git(d, "init", "-q"); git(d, "add", "-A"); git(d, "commit", "-qm", "base"); git(d, "tag", "base")
    files = change(d, a)
    git(d, "add", "-A"); git(d, "commit", "-qm", "change")
    return check(d, files)


# Other bytes under the published version and revision, written by hand.
b = archive(b"changed\n")
code, out = scenario(lambda d, a: ((d / "manifests/demo.x86_64.v11.toml").write_text(manifest(b)), {"demo.zip": b})[1])
if code == 0 or "different bytes need a new revision" not in out:
    fails.append(f"same release, other bytes: {code} {out}")

# The same bytes as a new revision: accepted.
code, out = scenario(lambda d, a: ((d / "manifests/demo.x86_64.v11.toml").write_text(manifest(b, extra="revision = 1\n")), {"demo.zip": b})[1])
if code or "layout passes" not in out:
    fails.append(f"new revision: {code} {out}")

# An archive that cannot be read, with a matching hash: a failed check.
junk = b"this is not an archive" * 10
code, out = scenario(lambda d, a: ((d / "manifests/demo.x86_64.v11.toml").write_text(manifest(junk, "1.1")), {"demo.zip": junk})[1])
if code == 0 or "cannot list the archive" not in out:
    fails.append(f"unreadable archive: {code} {out}")

# Only an override changes: the entry is checked with it applied.
def override(d, a):
    (d / "overrides").mkdir()
    (d / "overrides/demo.x86_64.v11.toml").write_text('subdir = "Elsewhere"\n')
    return {"demo.zip": a}
code, out = scenario(override)
if code == 0 or "subdir: 'Elsewhere' is not in the archive" not in out:
    fails.append(f"override-only change: {code} {out}")

# Two uploads with the same file name do not stand in for each other.
c = archive(b"other\n")
def two(d, a):
    (d / "manifests/demo.x86_64.v11.toml").write_text(manifest(c, "2.0", "https://one.invalid/release.zip"))
    return {"release.zip": c}
code, out = scenario(two)
if code:
    fails.append(f"new version from another URL: {code} {out}")

if fails:
    print("\n".join(fails)); print(f"CI-CHECK: {len(fails)} failed"); sys.exit(1)
print("CI-CHECK: same-release rule, unreadable archives and overrides are enforced")
