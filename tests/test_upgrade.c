/* The upgrade planner, every rule in upgrade.h, on the host. */
#include "../src/libpkg/upgrade.h"
#include <stdio.h>
#include <string.h>

static int bad = 0;
static void ok(int c, const char *l) { printf("%-4s %s\n", c ? "ok" : "FAIL", l); if (!c) bad++; }

static inv_ent E(const char *rel, const char *sha)
{
    inv_ent e; memset(&e, 0, sizeof e);
    snprintf(e.rel, sizeof e.rel, "%s", rel);
    snprintf(e.sha, sizeof e.sha, "%s", sha);
    e.size = 1;
    return e;
}

static up_act act_of(up_step *s, int n, const char *rel)
{
    int i;
    for (i = 0; i < n; i++) if (strcmp(s[i].rel, rel) == 0) return s[i].act;
    return (up_act)-1;
}

int main(void)
{
    /* old revision: Prog(a) Same(s) Gone(g) Cfg(c)
       new revision: Prog(b) Same(s) New(n)  Cfg(c)
       live:  Prog untouched, Same edited by the user, Gone untouched,
              Cfg edited by the user, Extra unknown */
    inv_ent old[] = { E("Prog","a"), E("Same","s"), E("Gone","g"), E("Cfg","c") };
    inv_ent nw[]  = { E("Prog","b"), E("Same","s"), E("New","n"),  E("Cfg","c") };
    inv_ent live[] = { E("Prog","a"), E("Same","s2"), E("Gone","g"), E("Cfg","c2"), E("Extra","x") };
    up_step steps[32]; int n, conflicts = -1;
    char why[240];

    n = upgrade_plan(old, 4, nw, 4, live, 5, steps, 32, &conflicts);
    ok(n == 6 && conflicts == 0, "six steps, no conflict for the clean case");
    ok(act_of(steps, n, "Prog") == UP_REPLACE,     "changed upstream, untouched here: replace");
    ok(act_of(steps, n, "Same") == UP_UNCHANGED,   "same upstream, edited here: the user's copy stays");
    ok(act_of(steps, n, "Cfg")  == UP_UNCHANGED,   "a config the package did not change stays as edited");
    ok(act_of(steps, n, "New")  == UP_ADD,         "new upstream, absent here: add");
    ok(act_of(steps, n, "Gone") == UP_REMOVE,      "dropped upstream, untouched here: remove");
    ok(act_of(steps, n, "Extra") == UP_KEEP_UNKNOWN, "a file we never installed is kept");
    ok(strcmp(steps[0].rel, "Cfg") == 0 && strcmp(steps[n-1].rel, "Same") == 0,
       "steps are sorted by path");

    /* the user edited a file the new revision changes: refuse */
    {
        inv_ent live2[] = { E("Prog","user"), E("Same","s"), E("Gone","g"), E("Cfg","c") };
        n = upgrade_plan(old, 4, nw, 4, live2, 4, steps, 32, &conflicts);
        ok(conflicts == 1 && act_of(steps, n, "Prog") == UP_CONFLICT,
           "changed upstream AND edited here: conflict");
    }
    /* the user already has the new content: nothing to do */
    {
        inv_ent live3[] = { E("Prog","b"), E("Same","s"), E("Gone","g"), E("Cfg","c") };
        n = upgrade_plan(old, 4, nw, 4, live3, 4, steps, 32, &conflicts);
        ok(conflicts == 0 && act_of(steps, n, "Prog") == UP_UNCHANGED,
           "live already equals the new content: unchanged");
    }
    /* an unknown file where the new revision wants one: conflict */
    {
        inv_ent live4[] = { E("Prog","a"), E("Same","s"), E("Gone","g"), E("Cfg","c"), E("New","theirs") };
        n = upgrade_plan(old, 4, nw, 4, live4, 5, steps, 32, &conflicts);
        ok(conflicts == 1 && act_of(steps, n, "New") == UP_CONFLICT,
           "something already at a path the new revision adds: conflict");
    }
    /* dropped upstream but edited here: kept */
    {
        inv_ent live5[] = { E("Prog","a"), E("Same","s"), E("Gone","g-edited"), E("Cfg","c") };
        n = upgrade_plan(old, 4, nw, 4, live5, 4, steps, 32, &conflicts);
        ok(conflicts == 0 && act_of(steps, n, "Gone") == UP_KEEP_USER,
           "dropped upstream, edited here: kept for the user");
    }
    /* the user deleted a package file: the upgrade delivers it complete */
    {
        inv_ent live6[] = { E("Same","s"), E("Gone","g"), E("Cfg","c") };
        n = upgrade_plan(old, 4, nw, 4, live6, 3, steps, 32, &conflicts);
        ok(conflicts == 0 && act_of(steps, n, "Prog") == UP_ADD,
           "a deleted package file comes back with the new revision");
    }
    /* rollback is the same planner with the roles swapped */
    {
        inv_ent after[] = { E("Prog","b"), E("Same","s2"), E("New","n"), E("Cfg","c2"), E("Extra","x") };
        n = upgrade_plan(nw, 4, old, 4, after, 5, steps, 32, &conflicts);
        ok(conflicts == 0 &&
           act_of(steps, n, "Prog") == UP_REPLACE && act_of(steps, n, "New") == UP_REMOVE &&
           act_of(steps, n, "Gone") == UP_ADD && act_of(steps, n, "Same") == UP_UNCHANGED &&
           act_of(steps, n, "Extra") == UP_KEEP_UNKNOWN,
           "rollback: new roles swapped; the user's edits survive the way back too");
        {
            inv_ent after2[] = { E("Prog","b"), E("Same","s2"), E("New","n-edited"), E("Cfg","c2") };
            n = upgrade_plan(nw, 4, old, 4, after2, 4, steps, 32, &conflicts);
            ok(conflicts == 0 && act_of(steps, n, "New") == UP_KEEP_USER,
               "rollback keeps a file the user edited after the upgrade");
        }
    }
    ok(upgrade_plan(old, 4, nw, 4, live, 5, steps, 3, &conflicts) == -1,
       "too small an output is refused, not truncated");

    ok(upgrade_allowed("2.9", 1, "2.9", 2, why, sizeof why) == 0, "same version, higher revision: allowed");
    ok(upgrade_allowed("2.9", 2, "2.9", 2, why, sizeof why) != 0 && strstr(why, "not newer"),
       "same revision: refused as not newer");
    ok(upgrade_allowed("2.9", 1, "3.0", 1, why, sizeof why) != 0 && strstr(why, "no rule"),
       "different upstream version: refused, no rule invented");
    ok(upgrade_allowed("2.10", 1, "2.9", 5, why, sizeof why) != 0,
       "and never compared lexically: 2.10 vs 2.9 is just 'different'");

    printf(bad ? "\nFAIL %d\n" : "\nPASS all checks\n", bad);
    return bad != 0;
}
