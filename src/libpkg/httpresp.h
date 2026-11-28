#ifndef PKG_HTTPRESP_H
#define PKG_HTTPRESP_H
#include <stddef.h>

/* An HTTP/1.1 response taken in the pieces the connection delivers. The head
 * is kept; the body is passed on as it arrives, with chunked framing removed,
 * so the memory a response takes does not depend on its size. */
#define HTTP_HEAD_MAX 32768

/* Receives body bytes in order. Returns 0, or non-zero to stop the response. */
typedef int (*http_sink)(void *user, const char *data, size_t n);

enum { HTTP_OK = 0, HTTP_E_HEAD = -1, HTTP_E_FORM = -2, HTTP_E_SINK = -3, HTTP_E_SHORT = -4 };

typedef struct {
    char head[HTTP_HEAD_MAX];   /* status line and headers, 0-terminated once complete */
    size_t headlen;
    int phase;                  /* 0 while the head is incomplete */
    int code;                   /* status code; 0 when the status line has none */
    int chunked;
    long declared;              /* Content-Length, -1 when none was sent */
    long body;                  /* body bytes passed on so far */
    unsigned long left;         /* of the current chunk */
    char line[40];
    size_t linelen;
    http_sink sink;
    void *user;
} http_resp;

void http_init(http_resp *r, http_sink sink, void *user);
/* HTTP_OK, or: HTTP_E_HEAD the head is over HTTP_HEAD_MAX; HTTP_E_FORM a
 * chunk size cannot be read; HTTP_E_SINK the sink stopped it. */
int http_feed(http_resp *r, const char *data, size_t n);
/* At the end of the stream: HTTP_OK, HTTP_E_HEAD when the head never
 * completed, HTTP_E_SHORT when the body ended before its declared length or
 * inside a chunk. */
int http_end(const http_resp *r);
/* Whether the head is complete. */
int http_has_head(const http_resp *r);
/* The value of a header in a complete head, or NULL. `name` is given as
 * "Location:"; the capitalised and the lower-case spelling are found. The
 * value runs to the end of its line. */
const char *http_header(const http_resp *r, const char *name);
#endif
