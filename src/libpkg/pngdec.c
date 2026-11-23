/* A PNG reader for the pictures of packages (pngdec.h). */
#include "pngdec.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static unsigned long be32(const unsigned char *p)
{
    return (unsigned long)p[0] << 24 | (unsigned long)p[1] << 16 | (unsigned long)p[2] << 8 | p[3];
}

static int paeth(int a, int b, int c)
{
    int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}

int pkg_png_decode(const unsigned char *d, size_t len, int max_w, int max_h,
                   unsigned char **argb, int *w, int *h, char *why, size_t whyn)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    unsigned char *idat = NULL, *raw = NULL, *out = NULL;
    size_t pos = 8, nidat = 0, stride, rawlen;
    int bpp, y, x, rc = -1, ended = 0;
    uLongf got;
    char none[8];
    if (!why) { why = none; whyn = sizeof none; }
    *argb = NULL; *w = *h = 0;
    if (len < 33 || memcmp(d, sig, 8) != 0 || be32(d + 8) != 13 || memcmp(d + 12, "IHDR", 4) != 0) {
        snprintf(why, whyn, "not a PNG file"); return -1;
    }
    *w = (int)be32(d + 16); *h = (int)be32(d + 20);
    if (*w <= 0 || *h <= 0 || *w > max_w || *h > max_h) {
        snprintf(why, whyn, "the picture is %dx%d; at most %dx%d is shown here", *w, *h, max_w, max_h); return -1;
    }
    if (d[24] != 8 || (d[25] != 2 && d[25] != 6) || d[26] != 0 || d[27] != 0 || d[28] != 0) {
        snprintf(why, whyn, "not a plain truecolour PNG"); return -1;
    }
    bpp = d[25] == 6 ? 4 : 3;
    /* Every IDAT chunk in order is one zlib stream; they cannot hold more
       than the file does, so the file's length bounds the copy. */
    idat = (unsigned char *)malloc(len);
    if (!idat) { snprintf(why, whyn, "out of memory"); return -1; }
    while (pos + 12 <= len && !ended) {
        unsigned long n = be32(d + pos);
        if (n > len - pos - 12) break;
        if (!memcmp(d + pos + 4, "IDAT", 4)) { memcpy(idat + nidat, d + pos + 8, n); nidat += n; }
        else if (!memcmp(d + pos + 4, "IEND", 4)) ended = 1;
        pos += n + 12;
    }
    stride = (size_t)*w * (size_t)bpp;
    rawlen = (stride + 1) * (size_t)*h;
    raw = (unsigned char *)malloc(rawlen);
    out = (unsigned char *)malloc((size_t)*w * (size_t)*h * 4);
    if (!raw || !out) { snprintf(why, whyn, "out of memory"); goto done; }
    got = (uLongf)rawlen;
    if (!nidat || uncompress(raw, &got, idat, (uLong)nidat) != Z_OK || got != rawlen) {
        snprintf(why, whyn, "the picture's data is damaged"); goto done;
    }
    for (y = 0; y < *h; y++) {
        unsigned char *row = raw + (stride + 1) * (size_t)y + 1, *up = y ? row - stride - 1 : NULL;
        int f = row[-1];
        if (f > 4) { snprintf(why, whyn, "the picture's data is damaged"); goto done; }
        for (x = 0; x < (int)stride; x++) {
            int a = x >= bpp ? row[x - bpp] : 0, b = up ? up[x] : 0, c = (up && x >= bpp) ? up[x - bpp] : 0;
            int add = f == 1 ? a : f == 2 ? b : f == 3 ? (a + b) / 2 : f == 4 ? paeth(a, b, c) : 0;
            row[x] = (unsigned char)(row[x] + add);
        }
        for (x = 0; x < *w; x++) {
            unsigned char *o = out + ((size_t)y * (size_t)*w + (size_t)x) * 4, *s = row + (size_t)x * (size_t)bpp;
            o[0] = bpp == 4 ? s[3] : 255; o[1] = s[0]; o[2] = s[1]; o[3] = s[2];
        }
    }
    rc = 0;
done:
    free(idat); free(raw);
    if (rc != 0) { free(out); out = NULL; }
    *argb = out;
    return rc;
}

int pkg_png_load(const char *path, long max_bytes, int max_w, int max_h,
                 unsigned char **argb, int *w, int *h, char *why, size_t whyn)
{
    size_t len = 0;
    unsigned char *data;
    long size = path && path[0] ? u_size(path) : -1;
    int rc;
    char none[8];
    if (!why) { why = none; whyn = sizeof none; }
    *argb = NULL; *w = *h = 0;
    if (size <= 0) { snprintf(why, whyn, "the picture file is missing"); return -1; }
    if (size > max_bytes) { snprintf(why, whyn, "the picture is larger than %ld KB", max_bytes / 1024); return -1; }
    data = (unsigned char *)u_read_all(path, &len);
    if (!data) { snprintf(why, whyn, "the picture file cannot be read"); return -1; }
    rc = pkg_png_decode(data, len, max_w, max_h, argb, w, h, why, whyn);
    free(data);
    return rc;
}
