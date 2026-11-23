/* The PNG reader of the pictures of packages (src/libpkg/pngdec.c): a small
 * truecolour picture with alpha, written here byte by byte with stored
 * (uncompressed) data and every filter type, must come back as the pixels
 * it was made from; what the reader does not take must be refused. */
#include "../src/libpkg/pngdec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

static int fails;
#define CHECK(c, what) do { if (!(c)) { printf("FAIL: %s\n", what); fails++; } } while (0)

/* pngdec.c reads files through libpkg's helpers; the test decodes from
   memory only. */
long u_size(const char *p) { (void)p; return -1; }
char *u_read_all(const char *p, size_t *n) { (void)p; (void)n; return NULL; }

static size_t put_chunk(unsigned char *o, const char *type, const unsigned char *data, size_t n)
{
    unsigned long crc;
    o[0] = (unsigned char)(n >> 24); o[1] = (unsigned char)(n >> 16); o[2] = (unsigned char)(n >> 8); o[3] = (unsigned char)n;
    memcpy(o + 4, type, 4);
    if (n) memcpy(o + 8, data, n);
    crc = crc32(0L, o + 4, (uInt)(n + 4));
    o[8 + n] = (unsigned char)(crc >> 24); o[9 + n] = (unsigned char)(crc >> 16);
    o[10 + n] = (unsigned char)(crc >> 8); o[11 + n] = (unsigned char)crc;
    return n + 12;
}

#define W 5
#define H 5

/* One row filtered with `f`, from the true bytes of this row and the one
   above (NULL for the first). */
static void filter_row(int f, const unsigned char *row, const unsigned char *up, unsigned char *out, int bpp, int n)
{
    int x;
    for (x = 0; x < n; x++) {
        int a = x >= bpp ? row[x - bpp] : 0, b = up ? up[x] : 0, c = (up && x >= bpp) ? up[x - bpp] : 0, pr;
        int p = a + b - c, pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
        pr = pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
        out[x] = (unsigned char)(row[x] - (f == 1 ? a : f == 2 ? b : f == 3 ? (a + b) / 2 : f == 4 ? pr : 0));
    }
}

static size_t make_png(unsigned char *png, int colour, int depth, int interlace, const unsigned char *pix, int bpp)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    unsigned char ihdr[13] = { 0, 0, 0, W, 0, 0, 0, H, 0, 0, 0, 0, 0 }, raw[H * (W * 4 + 1)], z[512];
    uLongf zn = sizeof z;
    size_t k = 8;
    int y;
    ihdr[8] = (unsigned char)depth; ihdr[9] = (unsigned char)colour; ihdr[12] = (unsigned char)interlace;
    for (y = 0; y < H; y++) {
        raw[y * (W * bpp + 1)] = (unsigned char)(y % 5);
        filter_row(y % 5, pix + y * W * bpp, y ? pix + (y - 1) * W * bpp : NULL, raw + y * (W * bpp + 1) + 1, bpp, W * bpp);
    }
    compress2(z, &zn, raw, (uLong)(H * (W * bpp + 1)), 0);
    memcpy(png, sig, 8);
    k += put_chunk(png + k, "IHDR", ihdr, 13);
    k += put_chunk(png + k, "tEXt", (const unsigned char *)"k\0v", 3);
    /* In two pieces: the data of a picture may come in any number. */
    k += put_chunk(png + k, "IDAT", z, 7);
    k += put_chunk(png + k, "IDAT", z + 7, zn - 7);
    k += put_chunk(png + k, "IEND", NULL, 0);
    return k;
}

int main(void)
{
    unsigned char pix4[W * H * 4], pix3[W * H * 3], png[1024], *out;
    char why[120];
    size_t n;
    int i, w, h, same;
    for (i = 0; i < W * H * 4; i++) pix4[i] = (unsigned char)(i * 37 + 11);
    for (i = 0; i < W * H * 3; i++) pix3[i] = (unsigned char)(i * 53 + 7);

    n = make_png(png, 6, 8, 0, pix4, 4);
    CHECK(pkg_png_decode(png, n, 64, 64, &out, &w, &h, why, sizeof why) == 0 && w == W && h == H, "an RGBA picture is read");
    for (same = 1, i = 0; out && i < W * H; i++)
        if (out[i * 4] != pix4[i * 4 + 3] || out[i * 4 + 1] != pix4[i * 4] || out[i * 4 + 2] != pix4[i * 4 + 1] || out[i * 4 + 3] != pix4[i * 4 + 2]) same = 0;
    CHECK(out && same, "its pixels come back as A, R, G, B through every filter");
    free(out);

    n = make_png(png, 2, 8, 0, pix3, 3);
    CHECK(pkg_png_decode(png, n, 64, 64, &out, &w, &h, why, sizeof why) == 0, "an RGB picture is read");
    for (same = 1, i = 0; out && i < W * H; i++)
        if (out[i * 4] != 255 || out[i * 4 + 1] != pix3[i * 3] || out[i * 4 + 2] != pix3[i * 3 + 1] || out[i * 4 + 3] != pix3[i * 3 + 2]) same = 0;
    CHECK(out && same, "without alpha every pixel is opaque");
    free(out);

    n = make_png(png, 6, 8, 0, pix4, 4);
    CHECK(pkg_png_decode(png, n, 4, 64, &out, &w, &h, why, sizeof why) != 0 && !out, "a picture wider than its place is refused");
    n = make_png(png, 3, 8, 0, pix4, 4);
    CHECK(pkg_png_decode(png, n, 64, 64, &out, &w, &h, why, sizeof why) != 0 && !out, "a palette picture is refused");
    n = make_png(png, 6, 16, 0, pix4, 4);
    CHECK(pkg_png_decode(png, n, 64, 64, &out, &w, &h, why, sizeof why) != 0 && !out, "16 bits a channel are refused");
    n = make_png(png, 6, 8, 1, pix4, 4);
    CHECK(pkg_png_decode(png, n, 64, 64, &out, &w, &h, why, sizeof why) != 0 && !out, "an interlaced picture is refused");
    n = make_png(png, 6, 8, 0, pix4, 4);
    CHECK(pkg_png_decode(png, n - 40, 64, 64, &out, &w, &h, why, sizeof why) != 0 && !out, "a cut file is refused");
    CHECK(pkg_png_decode((const unsigned char *)"not a picture at all, just some words here", 42, 64, 64, &out, &w, &h, why, sizeof why) != 0, "other bytes are refused");

    if (fails) { printf("test_pngdec: %d FAILED\n", fails); return 1; }
    printf("test_pngdec: all passed\n");
    return 0;
}
