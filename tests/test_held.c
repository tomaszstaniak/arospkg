/* pkg_move_held_aside: a held drawer moved aside, and every way it cannot be
 * (a failed rename, no free name) leaves `to` empty and returns -1, so apkg
 * cannot report a move that did not happen (issue #3 follow-up). */
#include "../src/libpkg/held.h"
#include <stdio.h>
#include <string.h>

static int fail_rename, taken, renames, mkdirs;
static int atoi_local(const char *s) { int v = 0; while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0'); return v; }
static char renamed_to[512];

int u_mkdir(const char *path) { (void)path; mkdirs++; return 0; }
int u_exists(const char *path)
{
    /* the first `taken` names held-<id>-0 .. held-<id>-(taken-1) exist */
    const char *dash = strrchr(path, '-');
    int k = dash ? atoi_local(dash + 1) : 0;
    return k < taken;
}
int u_rename(const char *from, const char *to)
{
    (void)from; renames++;
    if (fail_rename) return -1;
    snprintf(renamed_to, sizeof renamed_to, "%s", to);
    return 0;
}

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void reset(void) { fail_rename = taken = renames = mkdirs = 0; renamed_to[0] = 0; }

int main(void)
{
    char to[512];

    reset();
    CHECK(pkg_move_held_aside("R/tmp", "R/mini", "mini", to, sizeof to) == 0);
    CHECK(!strcmp(to, "R/tmp/held-mini-0"));
    CHECK(!strcmp(renamed_to, "R/tmp/held-mini-0"));
    CHECK(renames == 1 && mkdirs == 1);

    reset(); taken = 2;
    CHECK(pkg_move_held_aside("R/tmp", "R/mini", "mini", to, sizeof to) == 0);
    CHECK(!strcmp(to, "R/tmp/held-mini-2"));

    reset(); fail_rename = 1;
    strcpy(to, "stale");
    CHECK(pkg_move_held_aside("R/tmp", "R/mini", "mini", to, sizeof to) == -1);
    CHECK(to[0] == 0);
    CHECK(renames == 1);

    reset(); taken = 100;
    strcpy(to, "stale");
    CHECK(pkg_move_held_aside("R/tmp", "R/mini", "mini", to, sizeof to) == -1);
    CHECK(to[0] == 0);
    CHECK(renames == 0);

    reset();
    CHECK(pkg_move_held_aside("R/tmp", "R/mini", "mini", to, 0) == -1);

    if (failures) return 1;
    printf("test_held: ok\n");
    return 0;
}
