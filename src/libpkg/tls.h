#ifndef PKG_TLS_H
#define PKG_TLS_H
#include <stddef.h>

/* A TLS client connection over an already connected bsdsocket socket.
 *
 * Two implementations exist, chosen by the build script: src/tls/mbedtls.c
 * (the default) and src/tls/openssl.c (the SDK's OpenSSL, kept to compare
 * against). Both verify the chain against the CA bundle and require the
 * certificate to name `verify_name`; neither has an "insecure" mode. */
typedef struct tls tls;

/* Returns a connection, or NULL with the reason in why. */
tls *tls_start(int sock, const char *host, const char *verify_name,
               const char *cafile, char why[240]);
/* Both return bytes transferred, 0 at the end of the stream, <0 on error. */
int  tls_write(tls *t, const void *buf, size_t n);
int  tls_read(tls *t, void *buf, size_t n);
void tls_end(tls *t);
/* Which library, for `apkg --version`. */
const char *tls_name(void);
#endif
