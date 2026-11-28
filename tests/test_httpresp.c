/* The HTTP response reader: the same body whatever the sizes of the pieces
 * the connection delivers, chunked or not, and a short body is told apart
 * from a complete one. */
#include "../src/libpkg/httpresp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int bad = 0;
static void ok(int c, const char *l) { printf("%-4s %s\n", c ? "ok" : "FAIL", l); if (!c) bad++; }

typedef struct { char *p; size_t n, stop_at; } got;
static int sink(void *u, const char *d, size_t n)
{
    got *g = (got *)u;
    if (g->stop_at && g->n + n > g->stop_at) return 1;
    g->p = (char *)realloc(g->p, g->n + n + 1);
    memcpy(g->p + g->n, d, n);
    g->n += n;
    return 0;
}

/* Feeds `resp` in pieces of `piece` bytes; returns the first non-OK result of
   http_feed, or that of http_end. */
static int feed(http_resp *r, got *g, const char *resp, size_t len, size_t piece)
{
    size_t at;
    int rc;
    g->p = NULL; g->n = 0;
    http_init(r, sink, g);
    for (at = 0; at < len; at += piece)
        if ((rc = http_feed(r, resp + at, len - at < piece ? len - at : piece)) != HTTP_OK) return rc;
    return http_end(r);
}

int main(void)
{
    static http_resp r;
    got g = {0};
    static const size_t pieces[] = { 1, 2, 3, 7, 64, 100000 };
    char *body = (char *)malloc(300000), *resp = (char *)malloc(700000);
    size_t i, len, blen = 300000;
    int same;

    for (i = 0; i < blen; i++) body[i] = (char)(i * 31 + (i >> 8));

    /* Content-Length */
    len = (size_t)sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Type: x\r\nContent-Length: %lu\r\n\r\n", (unsigned long)blen);
    memcpy(resp + len, body, blen); len += blen;
    for (same = 1, i = 0; i < sizeof pieces / sizeof *pieces; i++) {
        int rc = feed(&r, &g, resp, len, pieces[i]);
        if (rc != HTTP_OK || g.n != blen || memcmp(g.p, body, blen) || r.code != 200 || r.declared != (long)blen || r.chunked)
            same = 0;
        free(g.p);
    }
    ok(same, "a body with a Content-Length arrives whole in pieces of 1 byte to 100000");
    ok(feed(&r, &g, resp, len - 5, 4096) == HTTP_E_SHORT && r.body == (long)blen - 5,
       "a body that ends before its Content-Length is short");
    free(g.p);

    /* chunked, chunks of varying size, lower-case header, a chunk extension and a trailer */
    len = (size_t)sprintf(resp, "HTTP/1.1 200 OK\r\ntransfer-encoding: chunked\r\n\r\n");
    {
        size_t at = 0, n = 1;
        while (at < blen) {
            if (n > blen - at) n = blen - at;
            len += (size_t)sprintf(resp + len, at ? "%lx\r\n" : "%lX;ext=1\r\n", (unsigned long)n);
            memcpy(resp + len, body + at, n); len += n;
            memcpy(resp + len, "\r\n", 2); len += 2;
            at += n; n = n * 3 + 1;
        }
        len += (size_t)sprintf(resp + len, "0\r\nX-Trailer: 1\r\n\r\n");
    }
    for (same = 1, i = 0; i < sizeof pieces / sizeof *pieces; i++) {
        int rc = feed(&r, &g, resp, len, pieces[i]);
        if (rc != HTTP_OK || g.n != blen || memcmp(g.p, body, blen) || !r.chunked || r.declared != -1) same = 0;
        free(g.p);
    }
    ok(same, "a chunked body arrives whole, without its framing, in pieces of 1 byte to 100000");
    ok(feed(&r, &g, resp, len - 100000, 4096) == HTTP_E_SHORT, "a chunked body that ends inside a chunk is short");
    free(g.p);

    len = (size_t)sprintf(resp, "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\nabc");
    ok(feed(&r, &g, resp, len, 5) == HTTP_E_FORM, "a chunk size that is not a number is refused");
    free(g.p);

    /* redirect */
    len = (size_t)sprintf(resp, "HTTP/1.1 302 Found\r\nlocation:  https://example.org/a?b=c\r\nContent-Length: 3\r\n\r\nbye");
    ok(feed(&r, &g, resp, len, 1) == HTTP_OK && r.code == 302 && http_header(&r, "Location:") &&
       !strncmp(http_header(&r, "Location:"), "https://example.org/a?b=c\r\n", 27) && !http_header(&r, "Etag:"),
       "a redirect has its code and its Location, in either spelling");
    free(g.p);

    /* incomplete and oversized heads */
    len = (size_t)sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Le");
    ok(feed(&r, &g, resp, len, 3) == HTTP_E_HEAD && !http_has_head(&r), "a stream that ends inside the head has no head");
    free(g.p);
    memset(resp, 'a', HTTP_HEAD_MAX + 10);
    ok(feed(&r, &g, resp, HTTP_HEAD_MAX + 10, 4096) == HTTP_E_HEAD, "a head over the limit is refused");
    free(g.p);
    len = (size_t)sprintf(resp, "garbage\r\n\r\n");
    ok(feed(&r, &g, resp, len, 4) == HTTP_OK && r.code == 0, "a head without a status code has code 0");
    free(g.p);

    /* the sink stops it */
    len = (size_t)sprintf(resp, "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n");
    memcpy(resp + len, body, 1000); len += 1000;
    g.stop_at = 500;
    ok(feed(&r, &g, resp, len, 64) == HTTP_E_SINK && g.n <= 500, "a sink that refuses stops the response");
    free(g.p);

    printf(bad ? "\nFAIL %d\n" : "\nPASS all checks\n", bad);
    return bad != 0;
}
