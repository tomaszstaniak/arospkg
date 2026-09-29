#!/bin/sh
# Build the entropy probe for ABIv11 x86_64, against the same Mbed TLS as apkg.
set -e
TC=${TC:-$HOME/Work/AROS/toolchain}; SDK=${SDK:-$HOME/Work/AROS/sdk}
HERE=$(cd "$(dirname "$0")/../../src" && pwd)
CC="$TC/x86_64-aros-gcc"; AR="$TC/x86_64-aros-ar"; TAG=${TAG:-abiv11-x86_64}
. "$HERE/tls/tls.sh"
$CC -O2 -std=gnu99 -Wall -I"$SDK/include" $TLS_CFLAGS \
    "$HERE/../tests/entropy/probe.c" "$HERE/tls/entropy_aros.c" $TLS_LIBS -L"$SDK/lib" \
    -o "${OUT:-$HERE/../build/entropy-probe}"
