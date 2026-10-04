"""Open a pull request in arospkg-index for one submitted manifest.

Kept apart from apkg-pack so the way a submission reaches the catalogue can
change (another host, another review process) without touching how packages
are described, checked or packed. Uses git and the gh CLI with whatever
authentication they already have; it stores no credentials.
"""
import subprocess
import sys


def run(index_dir, *cmd, check=True):
    return subprocess.run(cmd, cwd=index_dir, capture_output=True, text=True, check=check)


def open_pr(index_dir, manifest, m):
    rev = m.get("revision", 0)
    branch = f"submit/{m['id']}-{m['arch']}-{m['version']}-r{rev}"
    # The same release submitted twice must not open a second pull request.
    existing = run(index_dir, "gh", "pr", "list", "--head", branch, "--state", "open",
                   "--json", "url", "--jq", ".[].url").stdout.strip()
    if existing:
        print(f"a pull request for this release is already open: {existing}")
        return
    base = run(index_dir, "git", "rev-parse", "--abbrev-ref", "HEAD").stdout.strip()
    run(index_dir, "git", "switch", "-c", branch)
    try:
        run(index_dir, "git", "add", str(manifest.relative_to(index_dir)))
        title = f"Submit {m['id']} {m['version']}" + (f" revision {rev}" if rev else "") + \
                f" for {m['arch']}/{m['abi']}"
        run(index_dir, "git", "commit", "-q", "-m", title + f"\n\nFrom {m['url']}\nsha256 {m['sha256']}, {m['size']} bytes.")
        run(index_dir, "git", "push", "-q", "-u", "origin", branch)
        url = run(index_dir, "gh", "pr", "create", "--title", title, "--body",
                  f"Archive: {m['url']}\n\nsha256 `{m['sha256']}`, {m['size']} bytes.\n\n"
                  "Prepared by apkg-pack submit from the archive's own manifest; "
                  "the checks run on this pull request download it again.",
                  "--head", branch).stdout.strip()
        print(f"opened {url}")
    except subprocess.CalledProcessError as exc:
        print(f"{' '.join(exc.cmd)}: {exc.stderr.strip()}", file=sys.stderr)
        sys.exit(1)
    finally:
        run(index_dir, "git", "switch", base, check=False)
