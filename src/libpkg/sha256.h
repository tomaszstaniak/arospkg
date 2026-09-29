#ifndef PKG_SHA256_H
#define PKG_SHA256_H
#include <stddef.h>
typedef struct { unsigned int h[8]; unsigned long long len; unsigned char buf[64]; size_t n; } sha256_ctx;
void sha256_init(sha256_ctx *);
void sha256_update(sha256_ctx *, const void *, size_t);
void sha256_final(sha256_ctx *, char out_hex[65]);
/* Hash a whole file. Returns 0 on success. */
int  sha256_file(const char *path, char out_hex[65]);
#endif
