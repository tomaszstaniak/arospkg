#ifndef PKG_PNGDEC_H
#define PKG_PNGDEC_H
#include <stddef.h>

/* Decodes the PNG files the catalogue's tools write: 8 bits per channel,
 * truecolour with or without alpha, not interlaced. Anything else is
 * refused, which is the point: a front draws these bytes itself and does
 * not depend on a picture datatype being installed, or on how one treats a
 * palette. The result is width*height*4 bytes in the order A, R, G, B
 * (free() it). 0 on success; -1 with a reason. */
int pkg_png_decode(const unsigned char *data, size_t len, int max_w, int max_h,
                   unsigned char **argb, int *w, int *h, char *why, size_t whyn);

/* The same for a file of at most `max_bytes`. */
int pkg_png_load(const char *path, long max_bytes, int max_w, int max_h,
                 unsigned char **argb, int *w, int *h, char *why, size_t whyn);
#endif
