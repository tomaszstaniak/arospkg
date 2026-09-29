#ifndef PKG_UPGRADE_H
#define PKG_UPGRADE_H
#include "internal.h"

/* The plan of an upgrade (or of a rollback, which is an upgrade with the
 * roles swapped): what to add, replace, remove and keep, decided from three
 * inventories and nothing else, so it can be tested on the host and printed
 * before a byte moves.
 *
 *   old   what the registry says the installed revision contained
 *   new   what the staged tree of the target revision contains
 *   live  what is in the drawer now
 *
 * The rules, per path:
 *   in new and old, same content upstream  -> UNCHANGED: the live file stays
 *                                             whatever the user did to it;
 *                                             absent live -> ADD (the package
 *                                             is delivered complete)
 *   in new and old, changed upstream        -> live absent: ADD
 *                                             live == old: REPLACE
 *                                             live == new: UNCHANGED (already)
 *                                             otherwise:   CONFLICT
 *   in new only                             -> live absent: ADD
 *                                             live == new: UNCHANGED
 *                                             otherwise:   CONFLICT
 *   in old only                             -> live absent: nothing
 *                                             live == old: REMOVE
 *                                             otherwise:   KEEP_USER
 *   in live only                            -> KEEP_UNKNOWN
 *
 * A CONFLICT is a file the user changed (or put there) that the target
 * revision wants to overwrite. One conflict stops the whole operation before
 * anything is touched; there is no "take theirs" in this increment, because
 * the answer is the user's to give and nothing here can give it for them. */

typedef enum {
    UP_ADD, UP_REPLACE, UP_REMOVE, UP_UNCHANGED,
    UP_KEEP_USER, UP_KEEP_UNKNOWN, UP_CONFLICT
} up_act;

typedef struct { char rel[256]; up_act act; } up_step;

/* Returns the number of steps, or -1 if `max` is too small. *conflicts is the
 * count of UP_CONFLICT steps. Steps come out sorted by path. */
int upgrade_plan(const inv_ent *old, int nold,
                 const inv_ent *new_, int nnew,
                 const inv_ent *live, int nlive,
                 up_step *out, int max, int *conflicts);

const char *upgrade_actname(up_act);

/* The revision rule, in one place. Returns 0 if `to` may replace `from`, or
 * -1 with the reason: the versions differ (no ordering rule exists across
 * upstream versions and none is invented here), or the revision does not
 * increase. */
int upgrade_allowed(const char *from_version, long from_rev,
                    const char *to_version, long to_rev,
                    char *why, size_t n);
#endif
