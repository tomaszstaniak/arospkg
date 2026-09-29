Mbed TLS 3.6.7 for AROS x86_64 (ABIv11), port revision aros1
============================================================

Static libraries of Mbed TLS 3.6.7 (the long-term support branch) for
programs built for AROS x86_64 with ABIv11: AROS One and the other current
distributions. Not for mainline AROS (ABIv1), i386 or aarch64: a library
built for one ABI does not work with another.

Upstream:  https://github.com/Mbed-TLS/mbedtls, release 3.6.7,
           mbedtls-3.6.7.tar.bz2, SHA-256
           a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6
           The library sources are unchanged.
Licence:   Apache-2.0 OR GPL-2.0-or-later (LICENSE). Copyright The Mbed TLS
           Contributors.
Build:     BUILD.txt names the compiler, the C library and the hash of each
           library file; src/make-packages.sh rebuilds both packages.


What is in it
-------------

  lib/libmbedcrypto.a   ciphers, hashes, public keys, random generators, PSA
  lib/libmbedx509.a     certificates
  lib/libmbedtls.a      the TLS protocol
  lib/libmbedtls_aros.a the AROS platform functions the configuration asks
                        for: mbedtls_ms_time() and mbedtls_platform_gmtime_r()
  include/              the upstream headers, and mbedtls_aros_config.h
  examples/https_get.c  a complete HTTPS client (see below)
  src/                  our platform file, the source lists and the build
                        script


The configuration is a TLS client profile
-----------------------------------------

The libraries were built with include/mbedtls_aros_config.h, and a program
MUST be compiled with the same file:

    -DMBEDTLS_CONFIG_FILE="<mbedtls_aros_config.h>"

It is the profile of a TLS client that verifies servers, nothing more:

  - TLS 1.2 and TLS 1.3, client side only; no server, no DTLS;
  - ECDHE key exchange (RSA and ECDSA certificates), curves P-256, P-384,
    P-521 and X25519;
  - AES-GCM and ChaCha20-Poly1305; SHA-1 (only for old certificates in a
    chain), SHA-224/256/384/512;
  - X.509 certificate parsing and verification, PEM and DER;
  - PSA Crypto (TLS 1.3 needs it);
  - mbedtls_strerror() for readable error messages;
  - no file system or socket layer of Mbed TLS's own (MBEDTLS_FS_IO and
    MBEDTLS_NET_C are off): read files with AmigaDOS and pass the data;
    connect with bsdsocket and pass send/recv (see the example).

If your program needs something else (a server, other ciphers, file
helpers), this profile is not for you; rebuild with your own configuration
using src/make-packages.sh as a starting point.


Entropy: you must connect a source
----------------------------------

Mbed TLS needs a source of unpredictable data to make keys. The C library's
rand() is NOT one. This configuration sets MBEDTLS_ENTROPY_HARDWARE_ALT and
MBEDTLS_NO_PLATFORM_ENTROPY, which means: your program must provide

    int mbedtls_hardware_poll(void *data, unsigned char *output,
                              size_t len, size_t *olen);

and the link fails without it. AROS One and the other ABIv11 systems have
no system entropy source today (no getentropy(), no entropy.resource). The
companion package JitterEntropy-3.7.0-aros1 provides this function from the
jitterentropy CPU-jitter source: link its libjitterentropy_mbedtls.a and
libjitterentropy.a.

On a system that has a source (mainline AROS: getentropy() in posixc,
answered by entropy.resource), use that instead; do not use both, and do
not fall back from one to the other.

Linking the libraries does not by itself make a program's TLS sound. Check
the return value of mbedtls_ctr_drbg_seed() and psa_crypto_init(): if the
source fails they fail, and the program must then not use TLS. The example
does exactly this.


Building a program
------------------

With MB and JE the directories of the two unpacked packages:

    x86_64-aros-gcc -O2 examples/https_get.c \
        -I$MB/include -I$JE/include \
        '-DMBEDTLS_CONFIG_FILE=<mbedtls_aros_config.h>' \
        -L$MB/lib -L$JE/lib \
        -lmbedtls -lmbedx509 -lmbedcrypto -lmbedtls_aros \
        -ljitterentropy_mbedtls -ljitterentropy \
        -o https_get

The order of the -l options matters: TLS, then X.509, then crypto, then the
AROS functions, then the entropy adapter and its library.


The example
-----------

    https_get <host> [path]

It checks the entropy source first and stops with exit code 20 if it is
unusable; loads the CA bundle from ENV:SYS/Certificates/ca-bundle.crt and
refuses to connect without it; requires the certificate to be valid for
<host>; and prints the negotiated protocol and the response's status line.
It needs a running TCP/IP stack.


Errors your program has to handle
---------------------------------

  - entropy: mbedtls_ctr_drbg_seed() or psa_crypto_init() failing;
  - trust: no CA bundle, or none of its certificates parsed;
  - the handshake: a certificate that does not verify, a wrong host name,
    an expired one; mbedtls_ssl_get_verify_result() says which;
  - the transport: your send/recv callbacks returning errors.

None of these should be retried with verification turned off.


Not covered by this package
---------------------------

It is a build and a working example, not an audit. It was tested on AROS
One 1.3 under QEMU; see the port's test reports. Tested: the example's
connections and the arospkg package manager, which uses the same libraries.
