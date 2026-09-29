#include "json.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static int alloc_tok(js_tok *t, int max, int *n)
{
    if (*n >= max) return -1;
    t[*n].type = JS_UNDEF; t[*n].start = t[*n].end = -1;
    t[*n].size = 0; t[*n].parent = -1;
    return (*n)++;
}

int js_parse(const char *js, size_t len, js_tok *t, int max)
{
    int n = 0, super = -1;
    size_t i;
    for (i = 0; i < len; i++) {
        char c = js[i];
        int id;
        switch (c) {
        case '{': case '[':
            id = alloc_tok(t, max, &n); if (id < 0) return -1;
            if (super != -1) t[super].size++;
            t[id].type = (c == '{') ? JS_OBJ : JS_ARR;
            t[id].start = (int)i; t[id].parent = super;
            super = id;
            break;
        case '}': case ']': {
            js_type want = (c == '}') ? JS_OBJ : JS_ARR;
            int k;
            for (k = n - 1; k >= 0; k--)
                if (t[k].start != -1 && t[k].end == -1) {
                    if (t[k].type != want) return -1;
                    t[k].end = (int)i + 1;
                    super = t[k].parent;
                    break;
                }
            if (k < 0) return -1;
            break;
        }
        case '"': {
            size_t start = ++i;
            for (; i < len && js[i] != '"'; i++)
                if (js[i] == '\\' && i + 1 < len) i++;
            if (i >= len) return -1;
            id = alloc_tok(t, max, &n); if (id < 0) return -1;
            t[id].type = JS_STR; t[id].start = (int)start; t[id].end = (int)i;
            t[id].parent = super;
            if (super != -1) t[super].size++;
            break;
        }
        case '\t': case '\r': case '\n': case ' ': case ':': case ',':
            break;
        default: {
            size_t start = i;
            for (; i < len; i++) {
                char d = js[i];
                if (d == ',' || d == '}' || d == ']' || d == ' ' ||
                    d == '\t' || d == '\r' || d == '\n') break;
            }
            id = alloc_tok(t, max, &n); if (id < 0) return -1;
            t[id].type = JS_PRIM; t[id].start = (int)start; t[id].end = (int)i;
            t[id].parent = super;
            if (super != -1) t[super].size++;
            i--;
            break;
        }
        }
    }
    return n;
}

/* Skip over token i and everything nested inside it. */
static int skip(const js_tok *t, int ntok, int i)
{
    int end = t[i].end, j = i + 1;
    while (j < ntok && t[j].start < end) j++;
    return j;
}

int js_member(const char *js, const js_tok *t, int ntok, int obj, const char *name)
{
    int i, len = (int)strlen(name);
    if (obj < 0 || obj >= ntok || t[obj].type != JS_OBJ) return -1;
    i = obj + 1;
    while (i < ntok && t[i].start < t[obj].end) {
        if (t[i].parent == obj && t[i].type == JS_STR &&
            t[i].end - t[i].start == len &&
            strncmp(js + t[i].start, name, (size_t)len) == 0)
            return (i + 1 < ntok) ? i + 1 : -1;
        i = skip(t, ntok, i);
    }
    return -1;
}

int js_elem(const js_tok *t, int ntok, int arr, int i)
{
    int j, k = 0;
    if (arr < 0 || arr >= ntok || t[arr].type != JS_ARR) return -1;
    j = arr + 1;
    while (j < ntok && t[j].start < t[arr].end) {
        if (t[j].parent == arr) {
            if (k == i) return j;
            k++;
        }
        j = skip(t, ntok, j);
    }
    return -1;
}

int js_str(const char *js, const js_tok *t, int i, char *buf, size_t n)
{
    size_t len;
    if (i < 0) { if (n) buf[0] = 0; return -1; }
    len = (size_t)(t[i].end - t[i].start);
    if (len >= n) len = n - 1;
    memcpy(buf, js + t[i].start, len);
    buf[len] = 0;
    return 0;
}

long js_long(const char *js, const js_tok *t, int i, long dflt)
{
    char b[32];
    if (i < 0) return dflt;
    if (js_str(js, t, i, b, sizeof b) != 0) return dflt;
    return strtol(b, NULL, 10);
}

void js_out_init(js_out *o, char *buf, size_t cap)
{ o->buf = buf; o->len = 0; o->cap = cap; o->fail = 0; if (cap) buf[0] = 0; }

void js_raw(js_out *o, const char *s)
{
    size_t n = strlen(s);
    if (o->len + n + 1 > o->cap) { o->fail = 1; return; }
    memcpy(o->buf + o->len, s, n); o->len += n; o->buf[o->len] = 0;
}

void js_key(js_out *o, const char *k) { js_raw(o, "\""); js_raw(o, k); js_raw(o, "\":"); }

void js_vstr(js_out *o, const char *s)
{
    js_raw(o, "\"");
    for (; *s; s++) {
        char e[3];
        if (*s == '"' || *s == '\\') { e[0] = '\\'; e[1] = *s; e[2] = 0; js_raw(o, e); }
        else if (*s == '\n') js_raw(o, "\\n");
        else { e[0] = *s; e[1] = 0; js_raw(o, e); }
    }
    js_raw(o, "\"");
}

void js_vlong(js_out *o, long v) { char b[32]; sprintf(b, "%ld", v); js_raw(o, b); }
