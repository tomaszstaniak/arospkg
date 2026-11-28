/* Pictures of packages (media.h, docs/rfc/0003). The drawer is read with
 * plain file calls and the catalogue with the JSON reader; the two network
 * calls go through fetch_verified, so a picture is checked like an archive
 * before anything shows it. */
#include "media.h"
#include "internal.h"
#include "json.h"
#include "util.h"
#include "zip.h"
#include "limits.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* The catalogue's hashes for a gallery, as one string: what media.json in a
   package's drawer records, so "is the gallery here" is one comparison. */
#define SIG_MAX (PKG_MEDIA_SHOTS * 2 * 65 + 1)

int pkg_png_check(const unsigned char *h, size_t n, long file_size,
                  int max_w, int max_h, int *w, int *hh, char *why, size_t whyn)
{
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
    char none[8];
    if (!why) { why = none; whyn = sizeof none; }
    *w = *hh = 0;
    if (file_size < 0) { snprintf(why, whyn, "the picture file is missing"); return -1; }
    if (file_size > PKG_MEDIA_MAXBYTES) { snprintf(why, whyn, "the picture is larger than %ld KB", PKG_MEDIA_MAXBYTES / 1024); return -1; }
    if (n < 24 || memcmp(h, sig, 8) != 0 || memcmp(h + 12, "IHDR", 4) != 0) {
        snprintf(why, whyn, "the picture is not a PNG file"); return -1;
    }
    *w = (int)((unsigned long)h[16] << 24 | (unsigned long)h[17] << 16 | (unsigned long)h[18] << 8 | h[19]);
    *hh = (int)((unsigned long)h[20] << 24 | (unsigned long)h[21] << 16 | (unsigned long)h[22] << 8 | h[23]);
    /* Truecolour only (PNG colour type 2 or 6). A palette PNG did not load
       through the picture datatype on AROS One 1.3 (2026-10-12), and the
       picture object then kept the previous picture on screen, so one
       program's picture showed as another's. Refused here, it shows as
       nothing. */
    if (n < 26 || (h[25] != 2 && h[25] != 6)) {
        snprintf(why, whyn, "the picture is not a truecolour PNG"); return -1;
    }
    if (*w <= 0 || *hh <= 0 || *w > max_w || *hh > max_h) {
        snprintf(why, whyn, "the picture is %dx%d; at most %dx%d is shown here", *w, *hh, max_w, max_h);
        return -1;
    }
    return 0;
}

int pkg_png_file(const char *path, int max_w, int max_h, char *why, size_t whyn)
{
    unsigned char head[26];
    size_t got = 0;
    long size = -1;
    int w, h;
    FILE *f = path && path[0] ? fopen(path, "rb") : NULL;
    if (f) {
        got = fread(head, 1, sizeof head, f);
        fclose(f);
        size = u_size(path);
    }
    return pkg_png_check(head, got, size, max_w, max_h, &w, &h, why, whyn);
}

void pkg_media_dir(const char *root, const char *id, char *out, size_t n)
{
    snprintf(out, n, "%s/media%s%s", root, id && id[0] ? "/" : "", id ? id : "");
}

int pkg_media_icon_small(const char *root, const char *id, char *out, size_t n)
{
    snprintf(out, n, "%s/media/%s/icon-small.png", root, id);
    if (pkg_png_file(out, PKG_ICON_SMALL_W, PKG_ICON_SMALL_H, NULL, 0) == 0) return 1;
    out[0] = 0;
    return 0;
}

/* --------------------------------------------------------- the catalogue */

typedef struct { char *json; js_tok *t; int ntok; } idx;

static int idx_open(idx *x, const char *index_path)
{
    size_t len = 0;
    memset(x, 0, sizeof *x);
    if (!index_path || !index_path[0]) return -1;
    x->json = u_read_all(index_path, &len);
    if (!x->json) return -1;
    x->ntok = js_parse_alloc(x->json, len, &x->t, PKG_JSON_MAXTOK);
    if (x->ntok < 0) { free(x->json); memset(x, 0, sizeof *x); return -1; }
    return 0;
}

static void idx_close(idx *x)
{
    free(x->t); free(x->json);
    memset(x, 0, sizeof *x);
}

/* The `media` object of the first build of `id` that has one: every build
   of an application carries the same. -1 when there is none. */
static int idx_media(const idx *x, const char *id)
{
    int arr = js_member(x->json, x->t, x->ntok, 0, "packages"), i, el;
    for (i = 0; arr >= 0 && (el = js_elem(x->t, x->ntok, arr, i)) >= 0; i++) {
        char got[PKG_MAXID];
        int m;
        if (js_str(x->json, x->t, js_member(x->json, x->t, x->ntok, el, "id"), got, sizeof got) != 0) continue;
        if (strcmp(got, id) != 0) continue;
        m = js_member(x->json, x->t, x->ntok, el, "media");
        if (m >= 0 && x->t[m].type == JS_OBJ) return m;
    }
    return -1;
}

typedef struct { char url[512]; char sha[65]; long size; } mfile;

/* One file the catalogue names. 0 when it is complete and within the
   limits; a picture without a hash or a size is not fetched at all. */
static int idx_file(const idx *x, int obj, const char *name, mfile *f)
{
    int o = obj >= 0 ? js_member(x->json, x->t, x->ntok, obj, name) : -1;
    memset(f, 0, sizeof *f);
    if (o < 0 || x->t[o].type != JS_OBJ) return -1;
    js_str(x->json, x->t, js_member(x->json, x->t, x->ntok, o, "url"), f->url, sizeof f->url);
    js_str(x->json, x->t, js_member(x->json, x->t, x->ntok, o, "sha256"), f->sha, sizeof f->sha);
    f->size = js_long(x->json, x->t, js_member(x->json, x->t, x->ntok, o, "size"), -1);
    if (!f->url[0] || strlen(f->sha) != 64 || f->size <= 0) return -1;
    return 0;
}

typedef struct { mfile view, thumb; char caption[124]; } mshot;

/* The screenshots of a gallery, in order; entries of a type this version
   does not know are skipped, as the RFC promises later types. */
static int idx_gallery(const idx *x, int media, mshot *out, int max)
{
    int arr = media >= 0 ? js_member(x->json, x->t, x->ntok, media, "gallery") : -1, i, el, n = 0;
    for (i = 0; arr >= 0 && n < max && (el = js_elem(x->t, x->ntok, arr, i)) >= 0; i++) {
        char type[24] = "";
        size_t k;
        js_str(x->json, x->t, js_member(x->json, x->t, x->ntok, el, "type"), type, sizeof type);
        if (strcmp(type, "screenshot") != 0) continue;
        if (idx_file(x, el, "view", &out[n].view) != 0 || out[n].view.size > PKG_MEDIA_MAXBYTES) continue;
        if (idx_file(x, el, "thumb", &out[n].thumb) != 0 || out[n].thumb.size > PKG_MEDIA_MAXBYTES)
            memset(&out[n].thumb, 0, sizeof out[n].thumb);
        out[n].caption[0] = 0;
        js_str(x->json, x->t, js_member(x->json, x->t, x->ntok, el, "caption"), out[n].caption, sizeof out[n].caption);
        /* One line of printable ASCII, whatever the file says: it is shown
           in a text view that reads escape codes. */
        for (k = 0; out[n].caption[k]; k++)
            if ((unsigned char)out[n].caption[k] < 32 || (unsigned char)out[n].caption[k] > 126) out[n].caption[k] = ' ';
        n++;
    }
    return n;
}

static void gallery_sig(const mshot *s, int n, char *out, size_t max)
{
    size_t k = 0;
    int i;
    out[0] = 0;
    for (i = 0; i < n && k + 132 < max; i++)
        k += (size_t)snprintf(out + k, max - k, "%s %s\n", s[i].view.sha, s[i].thumb.sha[0] ? s[i].thumb.sha : "-");
}

static void sig_path(const char *root, const char *id, char *out, size_t n)
{
    snprintf(out, n, "%s/media/%s/gallery.sha256", root, id);
}

/* ------------------------------------------------------------ the drawer */

static void read_captions(const char *dir, pkg_media *m)
{
    char path[PKG_MAXPATH], *text, *p;
    int line = 0;
    snprintf(path, sizeof path, "%s/captions.txt", dir);
    if (u_size(path) <= 0 || u_size(path) > 4096) return;
    text = u_read_all(path, NULL);
    for (p = text; p && *p && line < PKG_MEDIA_SHOTS; line++) {
        size_t n = strcspn(p, "\r\n"), k, c = n < sizeof m->shots[0].caption - 1 ? n : sizeof m->shots[0].caption - 1;
        for (k = 0; k < c; k++)
            m->shots[line].caption[k] = ((unsigned char)p[k] < 32 || (unsigned char)p[k] > 126) ? ' ' : p[k];
        m->shots[line].caption[c] = 0;
        p += n;
        if (*p == '\r') p++;
        if (*p == '\n') p++;
    }
    free(text);
}

void pkg_media_get(const char *root, const char *index_path, const char *id, pkg_media *m)
{
    char dir[PKG_MAXPATH], path[PKG_MAXPATH];
    pkg_media all;
    idx x;
    int i;
    memset(m, 0, sizeof *m);
    memset(&all, 0, sizeof all);
    pkg_media_dir(root, id, dir, sizeof dir);
    snprintf(path, sizeof path, "%s/icon.png", dir);
    if (pkg_png_file(path, PKG_ICON_W, PKG_ICON_H, NULL, 0) == 0) snprintf(m->icon, sizeof m->icon, "%s", path);
    pkg_media_icon_small(root, id, m->icon_small, sizeof m->icon_small);
    /* Captions are read by file number, then the pictures that can be shown
       are kept in order: a missing 2.png does not move the caption of 3. */
    read_captions(dir, &all);
    for (i = 0; i < PKG_MEDIA_SHOTS; i++) {
        pkg_media_shot *s = &m->shots[m->nshots];
        snprintf(path, sizeof path, "%s/%d.png", dir, i + 1);
        if (pkg_png_file(path, PKG_SHOT_W, PKG_SHOT_H, NULL, 0) != 0) continue;
        snprintf(s->file, sizeof s->file, "%s", path);
        snprintf(path, sizeof path, "%s/%d-thumb.png", dir, i + 1);
        if (pkg_png_file(path, PKG_THUMB_W, PKG_THUMB_H, NULL, 0) == 0) snprintf(s->thumb, sizeof s->thumb, "%s", path);
        memcpy(s->caption, all.shots[i].caption, sizeof s->caption);
        m->nshots++;
    }
    if (idx_open(&x, index_path) == 0) {
        mshot *g = (mshot *)calloc(PKG_MEDIA_SHOTS, sizeof *g);
        if (g) {
            m->listed = idx_gallery(&x, idx_media(&x, id), g, PKG_MEDIA_SHOTS);
            if (m->listed) {
                char want[SIG_MAX], *have;
                gallery_sig(g, m->listed, want, sizeof want);
                sig_path(root, id, path, sizeof path);
                have = u_size(path) > 0 && u_size(path) < (long)sizeof want ? u_read_all(path, NULL) : NULL;
                m->due = !have || strcmp(have, want) != 0;
                free(have);
            }
            free(g);
        }
        idx_close(&x);
    }
}

/* ------------------------------------------------------------ the network */

pkg_status pkg_media_fetch(pkg_ctx *c, const char *index_path, const char *id, pkg_err *e)
{
    char dir[PKG_MAXPATH], path[PKG_MAXPATH], want[SIG_MAX], caps[PKG_MEDIA_SHOTS * 126 + 1];
    pkg_status st = PKG_OK;
    mshot *g;
    idx x;
    int n, i;
    size_t k = 0;
    if (idx_open(&x, index_path) != 0)
        return pkg_fail(e, PKG_E_NOT_FOUND, "cannot read the catalogue", "", index_path);
    g = (mshot *)calloc(PKG_MEDIA_SHOTS, sizeof *g);
    if (!g) { idx_close(&x); return pkg_fail(e, PKG_E_NOMEM, "out of memory", "", ""); }
    n = idx_gallery(&x, idx_media(&x, id), g, PKG_MEDIA_SHOTS);
    idx_close(&x);
    if (!n) { free(g); return PKG_OK; }

    pkg_media_dir(c->root, NULL, dir, sizeof dir); u_mkdir(dir);
    pkg_media_dir(c->root, id, dir, sizeof dir);   u_mkdir(dir);
    /* The record goes first: a gallery half replaced must read as "not
       here", never as the old one or the new one. */
    sig_path(c->root, id, path, sizeof path);
    u_delete(path);
    caps[0] = 0;
    for (i = 0; i < n && st == PKG_OK; i++) {
        snprintf(path, sizeof path, "%s/%d.png", dir, i + 1);
        st = fetch_verified(c, id, g[i].view.url, g[i].view.sha, g[i].view.size, path, e);
        if (st == PKG_OK && pkg_png_file(path, PKG_SHOT_W, PKG_SHOT_H, NULL, 0) != 0) {
            u_delete(path);
            st = pkg_fail(e, PKG_E_ARCHIVE, "a picture of the catalogue is not one this version shows", "", g[i].view.url);
        }
        snprintf(path, sizeof path, "%s/%d-thumb.png", dir, i + 1);
        if (st == PKG_OK && g[i].thumb.url[0]) {
            st = fetch_verified(c, id, g[i].thumb.url, g[i].thumb.sha, g[i].thumb.size, path, e);
            if (st == PKG_OK && pkg_png_file(path, PKG_THUMB_W, PKG_THUMB_H, NULL, 0) != 0) u_delete(path);
        } else if (st == PKG_OK) u_delete(path);
        k += (size_t)snprintf(caps + k, sizeof caps - k, "%s\n", g[i].caption);
    }
    /* Pictures past the catalogue's count belong to an older gallery. */
    for (i = n; st == PKG_OK && i < PKG_MEDIA_SHOTS; i++) {
        snprintf(path, sizeof path, "%s/%d.png", dir, i + 1);       u_delete(path);
        snprintf(path, sizeof path, "%s/%d-thumb.png", dir, i + 1); u_delete(path);
    }
    if (st == PKG_OK) {
        snprintf(path, sizeof path, "%s/captions.txt", dir);
        u_write_atomicish(path, caps);
        gallery_sig(g, n, want, sizeof want);
        sig_path(c->root, id, path, sizeof path);
        if (u_write_atomicish(path, want) != 0)
            st = pkg_fail(e, PKG_E_IO, "cannot record the pictures", "", path);
    }
    free(g);
    return st;
}

static int bundle_of(const char *index_path, mfile *f)
{
    idx x;
    int rc;
    if (idx_open(&x, index_path) != 0) return -1;
    rc = idx_file(&x, 0, "media_bundle", f);
    idx_close(&x);
    return rc;
}

static void bundle_mark(const char *root, char *out, size_t n)
{
    snprintf(out, n, "%s/media/icons.sha256", root);
}

int pkg_media_icons_due(const char *root, const char *index_path)
{
    char path[PKG_MAXPATH], *have;
    mfile f;
    int due;
    if (bundle_of(index_path, &f) != 0) return 0;
    bundle_mark(root, path, sizeof path);
    have = u_size(path) == 64 ? u_read_all(path, NULL) : NULL;
    due = !have || strcmp(have, f.sha) != 0;
    free(have);
    return due;
}

pkg_status pkg_media_icons(pkg_ctx *c, const char *index_path, pkg_err *e)
{
    char dir[PKG_MAXPATH], arc[PKG_MAXPATH], mark[PKG_MAXPATH], why[160];
    pkg_status st;
    mfile f;
    if (bundle_of(index_path, &f) != 0 || !pkg_media_icons_due(c->root, index_path)) return PKG_OK;
    if (f.size > PKG_MEDIA_MAXBUNDLE)
        return pkg_fail(e, PKG_E_ARCHIVE, "the catalogue's archive of icons is too large", "", f.url);
    pkg_media_dir(c->root, NULL, dir, sizeof dir); u_mkdir(dir);
    snprintf(arc, sizeof arc, "%s/icons.zip", c->cache);
    st = fetch_verified(c, "icons", f.url, f.sha, f.size, arc, e);
    if (st != PKG_OK) return st;
    prog(c, "extract", 0, 1, 0);
    if (zip_extract(arc, dir, "", PKG_MEDIA_MAXBYTES, PKG_MEDIA_MAXBUNDLE, NULL, NULL, why) != 0) {
        u_delete(arc);
        return pkg_fail(e, PKG_E_ARCHIVE, "the catalogue's archive of icons was rejected", why, f.url);
    }
    prog(c, "extract", 1, 1, 0);
    u_delete(arc);
    bundle_mark(c->root, mark, sizeof mark);
    if (u_write_atomicish(mark, f.sha) != 0)
        return pkg_fail(e, PKG_E_IO, "cannot record the icons", "", mark);
    return PKG_OK;
}

int pkg_media_clean(const char *root, const char *index_path)
{
    char dir[PKG_MAXPATH], path[PKG_MAXPATH], **names = NULL;
    int count = 0, i, k, cleaned = 0;
    idx x;
    int have_idx = idx_open(&x, index_path) == 0;
    pkg_media_dir(root, NULL, dir, sizeof dir);
    if (u_listdir(dir, &names, &count) != 0) { if (have_idx) idx_close(&x); return 0; }
    for (i = 0; i < count; i++) {
        int fetched_gallery, listed;
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        if (!u_isdir(path)) continue;
        sig_path(root, names[i], path, sizeof path);
        fetched_gallery = u_exists(path);
        /* Icons come in one archive for the whole catalogue, so an icon is
           the catalogue's when the catalogue names one for this package. */
        listed = have_idx && js_member(x.json, x.t, x.ntok, idx_media(&x, names[i]), "icon") >= 0;
        if (!fetched_gallery && !listed) continue;
        if (fetched_gallery) {
            for (k = 1; k <= PKG_MEDIA_SHOTS; k++) {
                snprintf(path, sizeof path, "%s/%s/%d.png", dir, names[i], k);       u_delete(path);
                snprintf(path, sizeof path, "%s/%s/%d-thumb.png", dir, names[i], k); u_delete(path);
            }
            snprintf(path, sizeof path, "%s/%s/captions.txt", dir, names[i]); u_delete(path);
            sig_path(root, names[i], path, sizeof path); u_delete(path);
        }
        if (listed) {
            snprintf(path, sizeof path, "%s/%s/icon.png", dir, names[i]);       u_delete(path);
            snprintf(path, sizeof path, "%s/%s/icon-small.png", dir, names[i]); u_delete(path);
        }
        snprintf(path, sizeof path, "%s/%s", dir, names[i]);
        u_delete(path);                    /* only when nothing of the user's is left in it */
        cleaned++;
    }
    u_freelist(names, count);
    if (have_idx) idx_close(&x);
    bundle_mark(root, path, sizeof path);
    u_delete(path);
    return cleaned;
}
