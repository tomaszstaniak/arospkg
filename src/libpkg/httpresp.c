/* See httpresp.h. */
#include "httpresp.h"

#include <stdlib.h>
#include <string.h>

enum { P_HEAD = 0, P_IDENTITY, P_SIZE, P_DATA, P_AFTER, P_TRAILER };

void http_init(http_resp *r, http_sink sink, void *user)
{
    r->headlen = 0;
    r->head[0] = 0;
    r->phase = P_HEAD;
    r->code = 0;
    r->chunked = 0;
    r->declared = -1;
    r->body = 0;
    r->left = 0;
    r->linelen = 0;
    r->sink = sink;
    r->user = user;
}

int http_has_head(const http_resp *r) { return r->phase != P_HEAD; }

const char *http_header(const http_resp *r, const char *name)
{
    char key[64];
    size_t n = strlen(name);
    const char *at;
    if (r->phase == P_HEAD || n + 2 > sizeof key) return NULL;
    key[0] = '\n';
    memcpy(key + 1, name, n + 1);
    at = strstr(r->head, key);
    if (!at) {
        size_t i;
        for (i = 1; key[i]; i++)
            if (key[i] >= 'A' && key[i] <= 'Z') key[i] = (char)(key[i] - 'A' + 'a');
        at = strstr(r->head, key);
    }
    if (!at) return NULL;
    at += n + 1;
    while (*at == ' ') at++;
    return at;
}

static void head_done(http_resp *r)
{
    const char *sp = strchr(r->head, ' '), *v;
    r->phase = P_IDENTITY;
    r->code = sp ? atoi(sp + 1) : 0;
    if ((v = http_header(r, "Content-Length:")) != NULL) r->declared = atol(v);
    r->chunked = strstr(r->head, "\nTransfer-Encoding: chunked") ||
                 strstr(r->head, "\ntransfer-encoding: chunked");
    r->phase = r->chunked ? P_SIZE : P_IDENTITY;
}

static int pass(http_resp *r, const char *d, size_t n)
{
    if (n && r->sink && r->sink(r->user, d, n) != 0) return HTTP_E_SINK;
    r->body += (long)n;
    return HTTP_OK;
}

int http_feed(http_resp *r, const char *d, size_t n)
{
    size_t i = 0;
    int rc;

    while (i < n && r->phase == P_HEAD) {
        if (r->headlen + 1 >= sizeof r->head) return HTTP_E_HEAD;
        r->head[r->headlen++] = d[i++];
        if (r->headlen >= 4 && !memcmp(r->head + r->headlen - 4, "\r\n\r\n", 4)) {
            r->head[r->headlen - 4] = 0;
            head_done(r);
        }
    }
    while (i < n) {
        switch (r->phase) {
        case P_IDENTITY:
            if ((rc = pass(r, d + i, n - i)) != HTTP_OK) return rc;
            i = n;
            break;
        case P_SIZE:
            if (d[i] != '\n') {
                if (r->linelen + 1 < sizeof r->line) r->line[r->linelen++] = d[i];
                i++;
                break;
            }
            {
                char *e;
                r->line[r->linelen] = 0;
                r->left = strtoul(r->line, &e, 16);
                if (e == r->line) return HTTP_E_FORM;
                r->linelen = 0;
                r->phase = r->left ? P_DATA : P_TRAILER;
                i++;
            }
            break;
        case P_DATA:
            {
                size_t take = n - i;
                if (take > r->left) take = (size_t)r->left;
                if ((rc = pass(r, d + i, take)) != HTTP_OK) return rc;
                i += take;
                r->left -= take;
                if (!r->left) r->phase = P_AFTER;
            }
            break;
        case P_AFTER:
            if (d[i] == '\r' || d[i] == '\n') i++;
            else r->phase = P_SIZE;
            break;
        default:        /* P_TRAILER: nothing after the last chunk is body */
            i = n;
            break;
        }
    }
    return HTTP_OK;
}

int http_end(const http_resp *r)
{
    switch (r->phase) {
    case P_HEAD:     return HTTP_E_HEAD;
    case P_IDENTITY: return r->declared >= 0 && r->body < r->declared ? HTTP_E_SHORT : HTTP_OK;
    case P_DATA:     return HTTP_E_SHORT;
    case P_SIZE:     return r->linelen ? HTTP_E_SHORT : HTTP_OK;
    default:         return HTTP_OK;
    }
}
