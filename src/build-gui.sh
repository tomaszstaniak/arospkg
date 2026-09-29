#!/bin/sh
# Build PkgManager, the Zune front end, for ABIv11 -- AROS One x86_64.
# Same toolchain, SDK and libraries as build.sh; libpkg is compiled in, so the
# window and the Shell client are the same code with a different front.
set -e
TC=${TC:-$HOME/Work/AROS/toolchain}
SDK=${SDK:-$HOME/Work/AROS/sdk}
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$HERE/../PkgManager}

BUILD_ID=$(cat "$HERE"/libpkg/*.c "$HERE"/libpkg/*.h "$HERE"/pkgmanager/*.c "$HERE"/pkgmanager/*.h \
           | shasum -a 256 | cut -c1-16)
echo "build id $BUILD_ID  (PkgManager, ABIv11 / AROS One)"

"$TC/x86_64-aros-gcc" -O2 -Wall -Wextra -std=gnu99 \
    -DPKG_BUILD_ID='"'"$BUILD_ID"'"' \
    -DPKG_TOOLCHAIN='"'"$TC"'"' \
    -DPKG_SDK='"'"$SDK"'"' \
    -DPKG_ABI='"v11"' \
    -DPKG_CC="\"$("$TC/x86_64-aros-gcc" -dumpversion)\"" \
    -I"$SDK/include" \
    "$HERE"/libpkg/*.c "$HERE"/pkgmanager/*.c \
    "$SDK/lib/libz.static.a" -L"$SDK/lib" -lssl -lcrypto \
    -o "$OUT"
ls -l "$OUT"
