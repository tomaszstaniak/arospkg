# Sourced by the build scripts. In: CC, AR, SDK, TAG (a name for the cache,
# per ABI and CPU), HERE (src/). TLS=mbedtls (default) or openssl.
# Out: TLS_SRC (sources to compile with the program), TLS_CFLAGS, TLS_LIBS.
# Mbed TLS is compiled once into build/<TAG>/libmbedtls.a and reused.
TLS=${TLS:-mbedtls}
case "$TLS" in
openssl)
    TLS_SRC="$HERE/tls/openssl.c"
    TLS_CFLAGS=""
    TLS_LIBS="-lssl -lcrypto"
    ;;
mbedtls)
    MB="$HERE/../third_party/mbedtls"
    CACHE="$HERE/../build/$TAG"
    TLS_CFLAGS="-DMBEDTLS_CONFIG_FILE=<mbedtls_config.h> -I$HERE/tls -I$MB/include"
    if [ ! -f "$CACHE/libmbedtls.a" ] || [ "$HERE/tls/mbedtls_config.h" -nt "$CACHE/libmbedtls.a" ]; then
        rm -rf "$CACHE/mbedtls"; mkdir -p "$CACHE/mbedtls"
        for c in "$MB"/library/*.c; do
            # -DMBEDTLS_TEST_SW_INET_PTON: the AROS C library has no inet_pton.
            $CC -O2 -std=gnu99 -w $TLS_CFLAGS -DMBEDTLS_TEST_SW_INET_PTON -I"$MB/library" -I"$SDK/include" \
                -c "$c" -o "$CACHE/mbedtls/$(basename "$c" .c).o" || exit 1
        done
        rm -f "$CACHE/libmbedtls.a"
        "$AR" rcs "$CACHE/libmbedtls.a" "$CACHE"/mbedtls/*.o
    fi
    TLS_SRC="$HERE/tls/mbedtls.c $HERE/tls/entropy_aros.c"
    # A build for a system with getentropy() adds -DPKG_HAVE_GETENTROPY.
    TLS_LIBS="$CACHE/libmbedtls.a"
    ;;
*) echo "TLS must be mbedtls or openssl"; exit 2 ;;
esac
