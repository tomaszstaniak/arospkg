#!/bin/sh
# Assemble what run-on-pool.sh stages: the two binaries as built, the
# fixtures, the guest scripts. Prints the SHA-256 of every file; the run
# report repeats the binaries' hashes.
#
#   tests/arexx/make-stage.sh STAGEDIR
set -e
HERE=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$1
[ -n "$OUT" ] || { echo "usage: $0 STAGEDIR"; exit 2; }
for b in apkg PkgManager; do [ -f "$HERE/$b" ] || { echo "build $b first"; exit 1; }; done
rm -rf "$OUT"; mkdir -p "$OUT"
python3 "$HERE/tests/arexx/make-arexx-fixtures.py" "$HERE" "$OUT" >/dev/null
cp "$HERE/apkg" "$HERE/PkgManager" "$HERE/tests/arexx/rxtest.rexx" "$HERE/tests/arexx/setup.script" "$OUT/"
cp "$HERE"/examples/arexx/*.rexx "$OUT/"
( cd "$OUT" && shasum -a 256 * ) > "$OUT.sha256"
cat "$OUT.sha256"
