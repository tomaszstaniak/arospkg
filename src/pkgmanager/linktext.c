/* Finding and shortening web addresses in PkgManager's texts; see
 * linktext.h. Plain C, so the host tests run it. */
#include "linktext.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>

/* Where an address ends: what can never be part of one in running text. */
static int ends(unsigned char c)
{
    return c <= ' ' || c >= 0x7f || c == '"' || c == '<' || c == '>' || c == '\\'
        || c == '^' || c == '{' || c == '}' || c == '|' || c == '`';
}

int lk_unsafe(const char *u)
{
    return strpbrk(u, "'*;$") != NULL;
}

static int starts(const char *p)
{
    return !strncasecmp(p, "http://", 7) || !strncasecmp(p, "https://", 8);
}

size_t lk_next_url(const char *p, const char **at)
{
    for (; *p; p++) {
        size_t n;
        const char *s;
        if (!starts(p) || (p > *at && !ends((unsigned char)p[-1]) && p[-1] != '(' && p[-1] != '['
                           && p[-1] != '\'' && p[-1] != ':' && p[-1] != '='))
            continue;
        for (n = 0; p[n] && !ends((unsigned char)p[n]); n++) ;
        /* Sentence punctuation after an address is not part of it. */
        while (n > 0 && strchr(".,:;!?'", p[n - 1])) n--;
        if (n > 0 && p[n - 1] == ')') {
            int open = 0; size_t i;
            for (i = 0; i < n; i++) open += p[i] == '(' ? 1 : p[i] == ')' ? -1 : 0;
            if (open < 0) n--;
        }
        if (n > 0 && p[n - 1] == ']' && !memchr(p, '[', n)) n--;
        s = strstr(p, "://") + 3;
        if ((size_t)(s - p) >= n) { p += n ? n - 1 : 0; continue; }   /* nothing after the scheme */
        *at = p;
        return n;
    }
    return 0;
}

void lk_label(const char *url, char *out, size_t n, int max)
{
    size_t len = strlen(url), tail;
    if ((int)len <= max || max < 12) { snprintf(out, n, "%s", url); return; }
    tail = (size_t)max / 3;
    snprintf(out, n, "%.*s...%s", (int)((size_t)max - 3 - tail), url, url + len - tail);
}

void lk_shorten(const char *in, char *out, size_t n, int max)
{
    size_t k = 0;
    const char *p = in, *at;
    size_t len;
    if (!n) return;
    for (;;) {
        at = p;
        len = lk_next_url(p, &at);
        if (!len) break;
        while (p < at && k + 1 < n) out[k++] = *p++;
        if ((int)len > max && len < LK_URLMAX) {
            char u[LK_URLMAX], l[96];
            memcpy(u, at, len); u[len] = 0;
            lk_label(u, l, sizeof l, max);
            k += (size_t)snprintf(out + k, n - k, "%s", l);
            if (k >= n) k = n - 1;
        } else {
            size_t i;
            for (i = 0; i < len && k + 1 < n; i++) out[k++] = at[i];
        }
        p = at + len;
    }
    while (*p && k + 1 < n) out[k++] = *p++;
    out[k] = 0;
}
