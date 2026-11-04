#ifndef PKG_HELD_H
#define PKG_HELD_H
#include <stddef.h>

/* Move an empty package drawer that another program holds into
 * `tmpdir/held-<id>-<n>`, the first such name that does not exist yet.
 * Returns 0 and the new path in `to` when the drawer was moved. On any
 * failure returns -1 with `to` empty, so a caller cannot report a move
 * that did not happen. Only util.h's u_mkdir, u_exists and u_rename are
 * used, which lets the host tests drive both outcomes. */
int pkg_move_held_aside(const char *tmpdir, const char *dest, const char *id,
                        char *to, size_t n);

#endif
