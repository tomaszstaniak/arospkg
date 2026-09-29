#ifndef PKG_JSON_H
#define PKG_JSON_H
#include <stddef.h>

/* A tokenising JSON reader, sized for what libpkg reads: the generated index
 * and its own registry files. Not a general parser -- it does not unescape
 * beyond the few escapes our writer emits, because the only producers are
 * tools/mkindex.py and this library. */

typedef enum { JS_UNDEF = 0, JS_OBJ, JS_ARR, JS_STR, JS_PRIM } js_type;

typedef struct { js_type type; int start, end, size, parent; } js_tok;

/* Returns token count, or -1. */
int  js_parse(const char *json, size_t len, js_tok *toks, int max);

/* Index of the value of member `name` in object token `obj`, or -1. */
int  js_member(const char *json, const js_tok *t, int ntok, int obj, const char *name);
/* Index of element `i` of array token `arr`, or -1. */
int  js_elem(const js_tok *t, int ntok, int arr, int i);
/* Copy a string/primitive token into buf. Returns 0 on success. */
int  js_str(const char *json, const js_tok *t, int i, char *buf, size_t n);
long js_long(const char *json, const js_tok *t, int i, long dflt);

/* Writer: just enough to emit our registry and plan files. */
typedef struct { char *buf; size_t len, cap; int fail; } js_out;
void js_out_init(js_out *, char *buf, size_t cap);
void js_raw (js_out *, const char *);
void js_key (js_out *, const char *);
void js_vstr(js_out *, const char *);
void js_vlong(js_out *, long);
#endif
