#!/bin/sh
# Build and package a release candidate for every target from the current
# commit, one archive per CPU/ABI, into dist/rc/. Nothing is uploaded.
#   tools/make-rc.sh 0.3.1-rc1
set -e
V=${1:?usage: make-rc.sh VERSION}
HERE=$(cd "$(dirname "$0")/.." && pwd)
OUT="$HERE/dist/rc/$V"; B="$HERE/build/rc/$V"
[ -z "$(git -C "$HERE" status --porcelain -- src third_party)" ] || { echo "src/ or third_party/ has uncommitted changes"; exit 1; }
COMMIT=$(git -C "$HERE" rev-parse HEAD)
rm -rf "$OUT" "$B"; mkdir -p "$OUT" "$B"
grep -q "\"apkg $V\"" "$HERE/src/pkg/main.c" || { echo "src/pkg/main.c does not say apkg $V"; exit 1; }

OUT_=$B/x86_64-aros-v11; mkdir -p $OUT_
OUT=$OUT_/apkg sh "$HERE/src/build.sh" >/dev/null 2>&1
OUT=$OUT_/PkgManager sh "$HERE/src/build-gui.sh" >/dev/null 2>&1
mkdir -p $B/x86_64-aros-v1;   OUT=$B/x86_64-aros-v1/apkg   sh "$HERE/src/build-mainline.sh" >/dev/null 2>&1
mkdir -p $B/i386-aros-v0;     OUT=$B/i386-aros-v0/apkg     sh "$HERE/src/build-i386.sh"     >/dev/null 2>&1
mkdir -p $B/aarch64-aros;     OUT=$B/aarch64-aros/apkg     sh "$HERE/src/build-aarch64.sh"  >/dev/null 2>&1

for t in x86_64-aros-v11 x86_64-aros-v1 i386-aros-v0 aarch64-aros; do
    D="$B/pkg/arospkg-$V.$t"; mkdir -p "$D/licenses"
    cp "$B/$t"/* "$D/"
    cp "$HERE/release/README-RC.txt" "$D/README.txt"
    cp "$HERE/release/LICENSE" "$D/"
    cp "$HERE/release/licenses/zlib-LICENSE" "$D/licenses/"
    cp "$HERE/third_party/mbedtls/LICENSE" "$D/licenses/MbedTLS-3.6.7-LICENSE"
    if [ "$t" != x86_64-aros-v1 ]; then
        cp "$HERE/third_party/jitterentropy/LICENSE" "$D/licenses/jitterentropy-3.7.0-LICENSE"
        cp "$HERE/third_party/jitterentropy/LICENSE.bsd" "$D/licenses/jitterentropy-3.7.0-LICENSE.bsd"
    fi
    [ "$t" = x86_64-aros-v11 ] && mkdir -p "$D/Examples/ARexx" && cp "$HERE"/examples/arexx/*.rexx "$D/Examples/ARexx/"
    { echo "arospkg $V, target $t"; echo "commit $COMMIT"; echo;
      (cd "$D" && for f in apkg PkgManager; do [ -f "$f" ] && shasum -a 256 "$f"; done; true); } > "$D/BUILD.txt"
    (cd "$B/pkg" && zip -qr "$OUT/arospkg-$V.$t.zip" "arospkg-$V.$t")
done
(cd "$OUT" && shasum -a 256 *.zip > SHA256SUMS && cat SHA256SUMS)
for t in x86_64-aros-v11 x86_64-aros-v1 i386-aros-v0 aarch64-aros; do
    (cd "$B/pkg/arospkg-$V.$t" && echo "$t:" && for f in apkg PkgManager; do [ -f "$f" ] && shasum -a 256 "$f"; done; true)
done
echo "commit $COMMIT"
