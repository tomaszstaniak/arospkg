/* Rows behind search and list. Pure, so the host tests cover it. */
#include "entries.h"
#include "json.h"
#include "listing.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The same ceiling ops.c uses for the index (MAXTOK in internal.h); repeated
 * here rather than included, because internal.h drags the context in and this
 * file is meant to build with nothing but json.c and listing.c beside it. */
#define ENTRIES_MAXTOK 8192

int pkg_abi_known(const char *a)
{
    return a && (strcmp(a, "v1") == 0 || strcmp(a, "v11") == 0);
}

const char *pkg_compat_word(pkg_compat r)
{
    switch (r) {
    case PKG_COMPAT_NATIVE: return "native";
    case PKG_COMPAT_INCOMPATIBLE: return "incompatible";
    default:              return "undetermined";
    }
}

static const char *abi_name(const char *abi)
{
    if (!strcmp(abi, "v1"))  return "ABIv1, mainline AROS";
    if (!strcmp(abi, "v11")) return "ABIv11, current distributions such as AROS One";
    return abi;
}

pkg_compat pkg_compat_of(const char *arch, const char *abi, const char *my_arch,
                     const char *my_abi, char *why, size_t n)
{
    if (!pkg_abi_known(abi)) {
        snprintf(why, n, "the catalogue gives no ABI this client knows (\"%s\")", abi ? abi : "");
        return PKG_COMPAT_UNDETERMINED;
    }
    /* A build that recorded no ABI of its own cannot tell. */
    if (!pkg_abi_known(my_abi)) {
        snprintf(why, n, "this build does not know its own ABI");
        return PKG_COMPAT_UNDETERMINED;
    }
    if (my_arch && *my_arch && arch && *arch && strcmp(arch, my_arch)) {
        snprintf(why, n, "built for %s; this machine is %s", arch, my_arch);
        return PKG_COMPAT_INCOMPATIBLE;
    }
    if (strcmp(abi, my_abi)) {
        snprintf(why, n, "built for %s; this machine runs %s", abi_name(abi), abi_name(my_abi));
        return PKG_COMPAT_INCOMPATIBLE;
    }
    snprintf(why, n, "built for this machine: %s, %s", my_arch && *my_arch ? my_arch : (arch ? arch : "?"),
             abi_name(abi));
    return PKG_COMPAT_NATIVE;
}

int entries_push(pkg_entries *es, const pkg_entry *e)
{
    if (es->n == es->cap) {
        int cap = es->cap ? es->cap * 2 : 32;
        pkg_entry *v = (pkg_entry *)realloc(es->v, sizeof *v * (size_t)cap);
        if (!v) return -1;
        es->v = v; es->cap = cap;
    }
    es->v[es->n++] = *e;
    return 0;
}

void pkg_entries_free(pkg_entries *es)
{
    free(es->v);
    es->v = NULL; es->n = es->cap = 0;
}

static int lc(int ch) { return (ch >= 'A' && ch <= 'Z') ? ch - 'A' + 'a' : ch; }

static int cmp_id(const void *a, const void *b)
{
    const char *x = ((const pkg_entry *)a)->id, *y = ((const pkg_entry *)b)->id;
    for (; *x && *y; x++, y++)
        if (lc((unsigned char)*x) != lc((unsigned char)*y))
            return lc((unsigned char)*x) - lc((unsigned char)*y);
    return lc((unsigned char)*x) - lc((unsigned char)*y);
}

void entries_sort(pkg_entries *es)
{
    if (es->n > 1) qsort(es->v, (size_t)es->n, sizeof *es->v, cmp_id);
}

static void member_str(const char *json, const js_tok *t, int ntok, int obj,
                       const char *name, char *buf, size_t n)
{
    int m = js_member(json, t, ntok, obj, name);
    buf[0] = 0;
    if (m >= 0) js_str(json, t, m, buf, n);
}

/* One index element as a row. search and show both read entries through
 * this, so a detail view cannot describe a package differently from the
 * list it was picked in. */
static void row_from_element(const char *json, const js_tok *t, int ntok, int el, pkg_entry *e)
{
    memset(e, 0, sizeof *e);
    member_str(json, t, ntok, el, "id",       e->id,       sizeof e->id);
    member_str(json, t, ntok, el, "version",  e->version,  sizeof e->version);
    member_str(json, t, ntok, el, "summary",  e->summary,  sizeof e->summary);
    member_str(json, t, ntok, el, "category", e->category, sizeof e->category);
    member_str(json, t, ntok, el, "arch",     e->arch,     sizeof e->arch);
    member_str(json, t, ntok, el, "abi",      e->abi,      sizeof e->abi);
    member_str(json, t, ntok, el, "kind",     e->kind,     sizeof e->kind);
    e->revision = js_long(json, t, js_member(json, t, ntok, el, "revision"), 0);
    {
        /* The ids only: what a detail panel lists. The probe that says
           whether this machine has them is pkg_requirements. */
        int ra = js_member(json, t, ntok, el, "requires_system"), k;
        size_t used = 0;
        for (k = 0; ra >= 0; k++) {
            int rel = js_elem(t, ntok, ra, k);
            char rid[64];
            if (rel < 0) break;
            member_str(json, t, ntok, rel, "id", rid, sizeof rid);
            if (!rid[0] || used + strlen(rid) + 3 > sizeof e->requires) continue;
            used += (size_t)snprintf(e->requires + used, sizeof e->requires - used,
                                     "%s%s", used ? ", " : "", rid);
        }
    }
    e->size = js_long(json, t, js_member(json, t, ntok, el, "size"), -1);
}

int entries_from_index(const char *json, size_t len, const char *term,
                       const char *my_abi, int show_all,
                       pkg_entries *out, int *hidden)
{
    return entries_from_index_on(json, len, term, "", my_abi, show_all, out, hidden);
}

int entries_from_index_on(const char *json, size_t len, const char *term,
                          const char *my_arch, const char *my_abi, int show_all,
                          pkg_entries *out, int *hidden)
{
    js_tok *t = (js_tok *)malloc(sizeof(js_tok) * ENTRIES_MAXTOK);
    int ntok, arr, i, rc = 0;

    if (hidden) *hidden = 0;
    if (!t) return -1;
    ntok = js_parse(json, len, t, ENTRIES_MAXTOK);
    arr = (ntok > 0) ? js_member(json, t, ntok, 0, "packages") : -1;
    if (arr < 0) { free(t); return -1; }

    for (i = 0; ; i++) {
        int el = js_elem(t, ntok, arr, i);
        pkg_entry e;
        if (el < 0) break;
        row_from_element(json, t, ntok, el, &e);
        /* A build that recorded no ABI cannot tell, so it hides nothing;
           that is the rule abi_check() applies before an install, too. */
        if (!pkg_abi_known(my_abi)) e.ours = 1;
        else {
            char why[200];
            e.ours = pkg_compat_of(e.arch, e.abi, my_arch, my_abi, why, sizeof why) == PKG_COMPAT_NATIVE;
        }

        if (!pkg_match(e.id, e.summary, e.category, term)) continue;
        if (!e.ours && !show_all) { if (hidden) (*hidden)++; continue; }
        if (entries_push(out, &e) != 0) { rc = -1; break; }
    }
    free(t);
    /* Sorted here, not trusted from the file: the index a machine holds is
       whatever `update` last fetched, and one staged for a test run came out
       in a different order from the published one. A list that changes order
       with its source is not a list anyone can find things in. */
    entries_sort(out);
    return rc;
}

/* The order of choice among entries of one id: by arch, then abi, then url. */
static int variant_key_less(const char *json, const js_tok *t, int ntok, int a, int b)
{
    static const char *const keys[] = { "arch", "abi", "url" };
    int k;
    for (k = 0; k < 3; k++) {
        char x[512], y[512]; int c;
        member_str(json, t, ntok, a, keys[k], x, sizeof x);
        member_str(json, t, ntok, b, keys[k], y, sizeof y);
        c = strcmp(x, y);
        if (c) return c < 0;
    }
    return 0;
}

int index_select_variant(const char *json, const js_tok *t, int ntok, int arr,
                         const char *id, const char *my_arch, const char *my_abi,
                         int *how, int *n_same)
{
    int i, best_any = -1, best_mine = -1, mine = 0;
    for (i = 0; arr >= 0; i++) {
        int el = js_elem(t, ntok, arr, i);
        char pid[PKG_MAXID], arch[16], abi[8], why[200];
        if (el < 0) break;
        member_str(json, t, ntok, el, "id", pid, sizeof pid);
        if (strcmp(pid, id)) continue;
        if (best_any < 0 || variant_key_less(json, t, ntok, el, best_any)) best_any = el;
        member_str(json, t, ntok, el, "arch", arch, sizeof arch);
        member_str(json, t, ntok, el, "abi",  abi,  sizeof abi);
        if (pkg_compat_of(arch, abi, my_arch, my_abi, why, sizeof why) == PKG_COMPAT_NATIVE) {
            mine++;
            if (best_mine < 0 || variant_key_less(json, t, ntok, el, best_mine)) best_mine = el;
        }
    }
    if (n_same) *n_same = mine;
    if (mine == 1) { *how = VARIANT_ONE; return best_mine; }
    if (mine > 1)  { *how = VARIANT_SEVERAL; return best_mine; }
    *how = VARIANT_NONE;
    return best_any;
}

int details_from_index(const char *json, size_t len, const char *id,
                       const char *my_arch, const char *my_abi, pkg_details *d)
{
    js_tok *t = (js_tok *)malloc(sizeof(js_tok) * ENTRIES_MAXTOK);
    int ntok, arr, i, chosen;
    size_t used = 0;
    char names[8][40];

    memset(d, 0, sizeof *d);
    if (!t) return -1;
    ntok = js_parse(json, len, t, ENTRIES_MAXTOK);
    arr = (ntok > 0) ? js_member(json, t, ntok, 0, "packages") : -1;
    if (arr < 0) { free(t); return -1; }
    for (i = 0; ; i++) {
        int el = js_elem(t, ntok, arr, i);
        char pid[PKG_MAXID], arch[16], abi[8];
        if (el < 0) break;
        member_str(json, t, ntok, el, "id", pid, sizeof pid);
        if (strcmp(pid, id)) continue;
        member_str(json, t, ntok, el, "arch", arch, sizeof arch);
        member_str(json, t, ntok, el, "abi",  abi,  sizeof abi);
        if (d->nvariants < 8)
            snprintf(names[d->nvariants], sizeof names[0], "%s/%s", arch[0] ? arch : "?", abi[0] ? abi : "?");
        d->nvariants++;
    }
    {
        /* Sorted, so the line reads the same whatever order the index has. */
        int a, b, n = d->nvariants < 8 ? d->nvariants : 8;
        for (a = 1; a < n; a++)
            for (b = a; b > 0 && strcmp(names[b - 1], names[b]) > 0; b--) {
                char tmp[40]; memcpy(tmp, names[b], sizeof tmp);
                memcpy(names[b], names[b - 1], sizeof tmp); memcpy(names[b - 1], tmp, sizeof tmp);
            }
        for (a = 0; a < n; a++)
            if (used + strlen(names[a]) + 3 < sizeof d->variants)
                used += (size_t)snprintf(d->variants + used, sizeof d->variants - used, "%s%s",
                                         used ? ", " : "", names[a]);
    }
    {
        int how, same;
        chosen = index_select_variant(json, t, ntok, arr, id, my_arch, my_abi, &how, &same);
        if (chosen < 0) { free(t); return 1; }
        d->ambiguous = how == VARIANT_SEVERAL ? same : 0;
    }
    {
        pkg_entry *e = &d->e; int el = chosen;
        row_from_element(json, t, ntok, el, e);
        member_str(json, t, ntok, el, "url",     d->url,     sizeof d->url);
        member_str(json, t, ntok, el, "sha256",  d->sha256,  sizeof d->sha256);
        member_str(json, t, ntok, el, "source",  d->source,  sizeof d->source);
        member_str(json, t, ntok, el, "license", d->license, sizeof d->license);
        d->compat = pkg_compat_of(e->arch, e->abi, my_arch, my_abi, d->compat_why, sizeof d->compat_why);
        e->ours = d->compat == PKG_COMPAT_NATIVE;
        d->in_index = 1;
    }
    free(t);
    return 0;
}

int entry_from_registry(const char *json, size_t len, pkg_entry *e)
{
    js_tok *t = (js_tok *)malloc(sizeof(js_tok) * ENTRIES_MAXTOK);
    int ntok;
    if (!t) return -1;
    memset(e, 0, sizeof *e);
    ntok = js_parse(json, len, t, ENTRIES_MAXTOK);
    if (ntok <= 0 || t[0].type != JS_OBJ) { free(t); return -1; }
    member_str(json, t, ntok, 0, "name",    e->id,      sizeof e->id);
    member_str(json, t, ntok, 0, "version", e->version, sizeof e->version);
    member_str(json, t, ntok, 0, "arch",    e->arch,    sizeof e->arch);
    e->size = js_long(json, t, js_member(json, t, ntok, 0, "archive_size"), -1);
    e->installed = 1;
    snprintf(e->installed_version, sizeof e->installed_version, "%s", e->version);
    e->installed_revision = js_long(json, t, js_member(json, t, ntok, 0, "revision"), 0);
    e->revision = e->installed_revision;
    free(t);
    return e->id[0] ? 0 : -1;
}
