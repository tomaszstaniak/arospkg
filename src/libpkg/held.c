/* See held.h. Kept apart from ops.c so that it can be tested on the host. */
#include "held.h"
#include "util.h"
#include <stdio.h>

int pkg_move_held_aside(const char *tmpdir, const char *dest, const char *id,
                        char *to, size_t n)
{
    int k;
    if (!to || n == 0) return -1;
    u_mkdir(tmpdir);
    for (k = 0; k < 100; k++) {
        snprintf(to, n, "%s/held-%s-%d", tmpdir, id, k);
        if (!u_exists(to)) {
            if (u_rename(dest, to) == 0) return 0;
            break;
        }
    }
    to[0] = 0;
    return -1;
}
