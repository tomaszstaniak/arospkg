#!/bin/sh
# The tests that do not need AROS. Pure logic only -- everything platform-
# dependent is verified on a guest and reported with a run id and a binary
# hash, because a host pass says nothing about AROS.
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$HERE/../src/libpkg
OUT=${OUT:-/tmp/arospkg-host-tests}
CC=${CC:-cc}
mkdir -p "$OUT"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_sha256" \
    "$HERE/test_sha256.c" "$SRC/sha256.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_req" \
    "$HERE/test_req.c" "$SRC/req.c" "$SRC/json.c" "$SRC/err.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_icon" \
    "$HERE/test_icon.c" "$SRC/icon.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_listing" \
    "$HERE/test_listing.c" "$SRC/listing.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_entries" \
    "$HERE/test_entries.c" "$SRC/entries.c" "$SRC/json.c" "$SRC/listing.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_upgrade" \
    "$HERE/test_upgrade.c" "$SRC/upgrade.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_verify" \
    "$HERE/test_verify.c" "$SRC/verify.c"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_zip" \
    "$HERE/test_zip.c" "$SRC/zip.c" "$HERE/host_util.c" -lz

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_rexxcmd" \
    "$HERE/test_rexxcmd.c" "$HERE/../src/pkgmanager/rexxcmd.c"

# test_zip reads archives made here, shaped like the ones that failed on AROS
python3 - <<'PY2'
import zipfile, os
with zipfile.ZipFile('/tmp/arospkg-test.zip', 'w', zipfile.ZIP_DEFLATED) as z:
    z.writestr('Game/Big', os.urandom(2 * 1024 * 1024))
    z.writestr('Game/Game', b'x' * 100)
    z.writestr('Game.info', b'I' * 64)
with zipfile.ZipFile('/tmp/arospkg-unsafe.zip', 'w') as z:
    z.writestr('Game.info', b'I' * 64)
    z.writestr('LIBS:evil.library', b'e')
PY2

python3 "$HERE/make-lha-fixtures.py"

$CC -O1 -Wall -Wextra -std=gnu99 -o "$OUT/test_lha" \
    "$HERE/test_lha.c" "$SRC/arc.c" "$SRC/lha.c" "$SRC/zip.c" "$HERE/host_util.c" -lz

fail=0
for t in test_sha256 test_req test_icon test_listing test_entries test_upgrade test_verify test_zip test_lha test_rexxcmd; do
    echo "== $t"
    "$OUT/$t" || fail=1
done
echo "== test_mkindex"
python3 "$HERE/test_mkindex.py" || fail=1
echo "== test_doc_examples"
python3 "$HERE/test_doc_examples.py" || fail=1
[ $fail -eq 0 ] && echo "ALL HOST TESTS PASS" || { echo "HOST TESTS FAILED"; exit 1; }
