#!/bin/sh
# Stage everything needed to verify the published index from inside AROS.
#
# What the demonstration proves, and what it does not:
#   PROVES  that a native AROS binary can fetch a real package from the real
#           AROS Archives over TLS, and that the bytes hash to exactly the
#           sha256 recorded in arospkg-index/index.json.
#   DOES NOT prove that the package installs, or runs. There is no installer.
#
# The SHA-256 is self-verifying evidence: a misread screenshot cannot produce
# 64 matching hex characters. That matters because guest-to-host file transfer
# on this platform is unreliable, so the result is read off the screen.
set -e
DEST=${1:-$HOME/Work/AROS/shared-main}
HERE=$(cd "$(dirname "$0")/.." && pwd)

[ -f "$HERE/docs/spikes/network-download/dl" ] || { echo "build dl first: docs/spikes/network-download/build.sh"; exit 1; }
mkdir -p "$DEST"
cp "$HERE/docs/spikes/network-download/dl" "$DEST/"

python3 - "$HERE" "$DEST" <<'PY'
import json, sys, hashlib, pathlib
here, dest = pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2])
idx = json.loads((here.parent / "arospkg-index/index.json").read_text())
dl  = (here / "docs/spikes/network-download/dl").read_bytes()
lines = [
    "; Expected results, generated from arospkg-index/index.json.",
    "; Compare what dl prints on the guest against these.",
    f"; dl sha256 {hashlib.sha256(dl).hexdigest()}",
    "",
]
cmds = ["Execute SYS:System/Network/AROSTCP/S/startnet"]
for p in idx["packages"]:
    lines += [f"{p['id']}  size {p['size']}", f"  {p['sha256']}", ""]
    cmds.append(f"RAM:dl {p['url']}")
(dest / "EXPECTED.txt").write_text("\n".join(lines))
(dest / "COMMANDS.txt").write_text("\n".join(cmds) + "\n")
print(f"staged {len(idx['packages'])} package(s) to {dest}")
for c in cmds:
    print("  " + c)
PY
