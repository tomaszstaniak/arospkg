/* The upgrade planner. Pure: inventories in, steps out. See upgrade.h. */
#include "upgrade.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *upgrade_actname(up_act a)
{
    switch (a) {
    case UP_ADD:          return "add";
    case UP_REPLACE:      return "replace";
    case UP_REMOVE:       return "remove";
    case UP_UNCHANGED:    return "unchanged";
    case UP_KEEP_USER:    return "keep (changed by the user)";
    case UP_KEEP_UNKNOWN: return "keep (not ours)";
    case UP_CONFLICT:     return "CONFLICT";
    }
    return "?";
}

static const inv_ent *find(const inv_ent *v, int n, const char *rel)
{
    int i;
    for (i = 0; i < n; i++) if (strcmp(v[i].rel, rel) == 0) return &v[i];
    return NULL;
}

static int cmp_step(const void *a, const void *b)
{ return strcmp(((const up_step *)a)->rel, ((const up_step *)b)->rel); }

static int same(const inv_ent *a, const inv_ent *b)
{ return a && b && strcmp(a->sha, b->sha) == 0; }

int upgrade_plan(const inv_ent *old, int nold,
                 const inv_ent *new_, int nnew,
                 const inv_ent *live, int nlive,
                 up_step *out, int max, int *conflicts)
{
    int n = 0, i, pass;
    *conflicts = 0;

    /* Three passes over the union, each path decided once: new first (adds,
       replacements, conflicts), then old-only (removals, kept user files),
       then live-only (unknown files). */
    for (pass = 0; pass < 3; pass++) {
        const inv_ent *src = pass == 0 ? new_ : pass == 1 ? old : live;
        int cnt = pass == 0 ? nnew : pass == 1 ? nold : nlive;
        for (i = 0; i < cnt; i++) {
            const char *rel = src[i].rel;
            const inv_ent *o = find(old, nold, rel), *nw = find(new_, nnew, rel),
                          *l = find(live, nlive, rel);
            up_act act;
            if (pass == 1 && nw) continue;          /* decided in pass 0 */
            if (pass == 2 && (nw || o)) continue;   /* decided earlier */
            if (nw && o) {
                if (same(nw, o))           act = l ? UP_UNCHANGED : UP_ADD;
                else if (!l)               act = UP_ADD;
                else if (same(l, o))       act = UP_REPLACE;
                else if (same(l, nw))      act = UP_UNCHANGED;
                else                       act = UP_CONFLICT;
            } else if (nw) {
                if (!l)                    act = UP_ADD;
                else if (same(l, nw))      act = UP_UNCHANGED;
                else                       act = UP_CONFLICT;
            } else if (o) {
                if (!l)                    continue;          /* already gone */
                else if (same(l, o))       act = UP_REMOVE;
                else                       act = UP_KEEP_USER;
            } else {
                act = UP_KEEP_UNKNOWN;
            }
            if (n >= max) return -1;
            snprintf(out[n].rel, sizeof out[n].rel, "%s", rel);
            out[n].act = act;
            if (act == UP_CONFLICT) (*conflicts)++;
            n++;
        }
    }
    if (n > 1) qsort(out, (size_t)n, sizeof *out, cmp_step);
    return n;
}

int upgrade_allowed(const char *from_version, long from_rev,
                    const char *to_version, long to_rev,
                    char *why, size_t n)
{
    if (strcmp(from_version, to_version) != 0) {
        snprintf(why, n, "installed is version %s, the index offers %s: no rule "
                 "orders one upstream version against another yet, so this is "
                 "refused rather than guessed", from_version, to_version);
        return -1;
    }
    if (to_rev <= from_rev) {
        snprintf(why, n, "installed is %s revision %ld and the index offers "
                 "revision %ld, which is not newer", from_version, from_rev, to_rev);
        return -1;
    }
    return 0;
}
