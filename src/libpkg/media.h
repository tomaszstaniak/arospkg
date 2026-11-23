#ifndef PKG_MEDIA_H
#define PKG_MEDIA_H
/* Pictures of packages: an icon and a gallery of screenshots, kept in one
 * drawer of the package root, <root>/media/<id>/ (docs/rfc/0003). The
 * catalogue names pictures with an address and a SHA-256; a distribution may
 * put files of its own there for a package the catalogue has none for.
 * Pictures are never needed for an operation: every call here may fail and
 * the fronts then show text. */
#include "pkg.h"
#include <stddef.h>

#define PKG_MEDIA_SHOTS     6
#define PKG_MEDIA_MAXBYTES  (512L * 1024)
#define PKG_ICON_W          64
#define PKG_ICON_H          64
#define PKG_ICON_SMALL_W    24
#define PKG_ICON_SMALL_H    24
#define PKG_SHOT_W          640
#define PKG_SHOT_H          400
#define PKG_THUMB_W         160
#define PKG_THUMB_H         100

typedef struct {
    char file[PKG_MAXPATH];      /* the size a package's page shows */
    char thumb[PKG_MAXPATH];     /* "" when the drawer has no usable one */
    char caption[124];
} pkg_media_shot;

typedef struct {
    char icon[PKG_MAXPATH];          /* "" when the drawer has none that can be shown */
    char icon_small[PKG_MAXPATH];
    pkg_media_shot shots[PKG_MEDIA_SHOTS];
    int  nshots;                     /* in the drawer now, each checked */
    int  listed;                     /* gallery pictures the catalogue names */
    int  due;                        /* the catalogue's gallery is not the one in the drawer */
} pkg_media;

/* A picture's size from its PNG header, without decoding it. 0 and the size
   on success; -1 with the reason when it is not a PNG the fronts show (not
   PNG, not truecolour, larger than max_w x max_h or PKG_MEDIA_MAXBYTES). */
int  pkg_png_check(const unsigned char *head, size_t n, long file_size,
                   int max_w, int max_h, int *w, int *h, char *why, size_t whyn);
/* The same for a file. `why` may be NULL. */
int  pkg_png_file(const char *path, int max_w, int max_h, char *why, size_t whyn);

void pkg_media_dir(const char *root, const char *id, char *out, size_t n);
/* The small icon of a package, for a list: 1 and its path when the drawer
   has one that can be shown. Reads one file header; no catalogue. */
int  pkg_media_icon_small(const char *root, const char *id, char *out, size_t n);

/* What the drawer holds for `id`, and whether the catalogue has a gallery
   that is not there yet. No network. `index_path` may be NULL or unreadable:
   then only the drawer is described. */
void pkg_media_get(const char *root, const char *index_path, const char *id, pkg_media *m);

/* Fetch the catalogue's gallery of `id` into the drawer, each file checked
   against the size and SHA-256 the catalogue states. PKG_OK also when the
   catalogue has none. */
pkg_status pkg_media_fetch(pkg_ctx *c, const char *index_path, const char *id, pkg_err *e);

/* The catalogue's archive of icons: 1 when it names one that is not the one
   unpacked last. */
int  pkg_media_icons_due(const char *root, const char *index_path);
/* Fetch and unpack it. PKG_OK also when there is nothing to do. */
pkg_status pkg_media_icons(pkg_ctx *c, const char *index_path, pkg_err *e);

/* Remove what was fetched; files of packages the catalogue does not name
   stay. Returns the number of package drawers emptied. */
int  pkg_media_clean(const char *root, const char *index_path);
#endif
