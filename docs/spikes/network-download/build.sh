#!/bin/sh
# Build the spike downloader against the mainline AROS tree.
set -e

TC=${TC:-/Volumes/arosmain/toolchain-mainline}
SDK=${SDK:-/Volumes/arosmain/build/bin/pc-x86_64/AROS/Developer}
OUT=${OUT:-$(dirname "$0")/dl}

"$TC/x86_64-aros-gcc" -O2 -Wall -Wextra \
    -I"$SDK/include" \
    "$(dirname "$0")/dl.c" \
    -L"$SDK/lib" -lssl -lcrypto \
    -o "$OUT"

ls -l "$OUT"
