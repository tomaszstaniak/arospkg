/* Minimal ZIP reader: stored and deflated entries, which is everything the
 * AROS Archives uploads use.
 *
 * Validation happens before anything is written, and the AROS-specific rule is
 * the one a Unix-shaped check misses: a ':' ANYWHERE in a member path makes it
 * an absolute path, because "LIBS:foo" names a volume. Section 2 requires that
 * rejection explicitly.
 */
#include "zip.h"
#include "util.h"
#include <zlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned rd32(const unsigned char *p)
{ return (unsigned)p[0] | (unsigned)p[1]<<8 | (unsigned)p[2]<<16 | (unsigned)p[3]<<24; }
static unsigned rd16(const unsigned char *p)
{ return (unsigned)p[0] | (unsigned)p[1]<<8; }

static int unsafe_name(const char *n)
{
    const char *p;
    if (!*n) return 1;
    if (*n == '/' || *n == '\\') return 1;      /* absolute, POSIX style   */
    if (strchr(n, ':')) return 1;               /* absolute, AROS style    */
    for (p = n; *p; p++) {
        if (p[0] == '.' && p[1] == '.' &&
            (p[2] == '/' || p[2] == 0) && (p == n || p[-1] == '/'))
            return 1;                           /* .. escaping the root    */
        if ((unsigned char)*p < 0x20) return 1;
    }
    return 0;
}

/* Make every parent directory of a relative path under `dest`. */
static void mk_parents(const char *dest, const char *rel)
{
    char buf[512], full[512];
    size_t i;
    strncpy(buf, rel, sizeof buf - 1); buf[sizeof buf - 1] = 0;
    for (i = 0; buf[i]; i++)
        if (buf[i] == '/') {
            buf[i] = 0;
            u_join(full, sizeof full, dest, buf);
            u_mkdir(full);
            buf[i] = '/';
        }
}

/* `only`, when set, extracts that single member -- matched by its full name
 * inside the archive -- to the file path `dest`, and fails if it is absent. The
 * drawer icon is the reason: it sits BESIDE the program's directory in the
 * archive, so the ordinary strip-to-subdir extraction never sees it. Every
 * member is still validated first, so an archive with one unsafe path is
 * rejected whichever member was asked for. */
static int extract(const char *archive, const char *dest, const char *strip,
                   const char *only, long max_file, long max_total,
                   zip_cb cb, void *user, char why[160]);

int zip_extract(const char *archive, const char *dest, const char *strip,
                long max_file, long max_total,
                zip_cb cb, void *user, char why[160])
{
    return extract(archive, dest, strip, NULL, max_file, max_total, cb, user, why);
}

int zip_extract_member(const char *archive, const char *member,
                       const char *dest_file, long max_file, char why[160])
{
    return extract(archive, dest_file, "", member, max_file, 0, NULL, NULL, why);
}

static int extract(const char *archive, const char *dest, const char *strip,
                   const char *only, long max_file, long max_total,
                   zip_cb cb, void *user, char why[160])
{
    int found = 0;
    FILE *f = fopen(archive, "rb");
    unsigned char *cd = NULL, eocd[22];
    long size, i, total = 0;
    unsigned n_ent, cd_size, cd_off, k;
    size_t striplen = strip ? strlen(strip) : 0;
    int rc = -1;

    why[0] = 0;
    if (!f) { snprintf(why, 160, "cannot open %s", archive); return -1; }
    fseek(f, 0, SEEK_END); size = ftell(f);

    /* End of central directory: no ZIP comment is assumed, then a short scan. */
    for (i = size - 22; i >= 0 && i > size - 22 - 65536; i--) {
        fseek(f, i, SEEK_SET);
        if (fread(eocd, 1, 22, f) != 22) continue;
        if (rd32(eocd) == 0x06054b50) break;
    }
    if (i < 0) { snprintf(why, 160, "not a zip file (no end record)"); goto out; }

    n_ent   = rd16(eocd + 10);
    cd_size = rd32(eocd + 12);
    cd_off  = rd32(eocd + 16);
    cd = (unsigned char *)malloc(cd_size);
    if (!cd) { snprintf(why, 160, "out of memory"); goto out; }
    fseek(f, (long)cd_off, SEEK_SET);
    if (fread(cd, 1, cd_size, f) != cd_size) { snprintf(why, 160, "short central directory"); goto out; }

    /* Pass 1: validate every member before a single byte is written. One bad
       entry rejects the whole archive, as section 2 requires. */
    {
        unsigned off = 0;
        for (k = 0; k < n_ent && off + 46 <= cd_size; k++) {
            unsigned nlen = rd16(cd + off + 28), elen = rd16(cd + off + 30),
                     clen = rd16(cd + off + 32), usz = rd32(cd + off + 24);
            char name[512];
            if (rd32(cd + off) != 0x02014b50) { snprintf(why, 160, "bad directory entry %u", k); goto out; }
            if (nlen >= sizeof name) { snprintf(why, 160, "member name too long"); goto out; }
            memcpy(name, cd + off + 46, nlen); name[nlen] = 0;
            if (unsafe_name(name)) { snprintf(why, 160, "unsafe member path: %s", name); goto out; }
            /* Every member's PATH is checked whatever is being extracted, but
               a size limit only means something for what is actually written.
               Applying the single-member limit to the whole archive refused
               every package with any file over it -- five of the first thirty
               candidates, on AROS One, 2026-09-19. */
            if (only && strcmp(name, only) != 0) { off += 46 + nlen + elen + clen; continue; }
            if (max_file > 0 && (long)usz > max_file) {
                snprintf(why, 160, "member over size limit: %s", name); goto out;
            }
            total += (long)usz;
            if (max_total > 0 && total > max_total) {
                snprintf(why, 160, "archive over total size limit"); goto out;
            }
            off += 46 + nlen + elen + clen;
        }
    }

    /* Pass 2: extract. */
    {
        unsigned off = 0;
        for (k = 0; k < n_ent && off + 46 <= cd_size; k++) {
            unsigned nlen = rd16(cd + off + 28), elen = rd16(cd + off + 30),
                     clen = rd16(cd + off + 32);
            unsigned method = rd16(cd + off + 10), csz = rd32(cd + off + 20),
                     usz = rd32(cd + off + 24), lho = rd32(cd + off + 42);
            char name[512], rel[512], full[512];
            unsigned char lh[30];
            const char *use;

            memcpy(name, cd + off + 46, nlen); name[nlen] = 0;
            off += 46 + nlen + elen + clen;

            if (only) {
                if (strcmp(name, only) != 0) continue;
                found = 1;
                strncpy(rel, name, sizeof rel - 1); rel[sizeof rel - 1] = 0;
                strncpy(full, dest, sizeof full - 1); full[sizeof full - 1] = 0;
                goto write_member;
            }
            use = name;
            if (striplen && strncmp(name, strip, striplen) == 0 && name[striplen] == '/')
                use = name + striplen + 1;
            else if (striplen)
                continue;                      /* outside the chosen subdir */
            if (!*use) continue;

            strncpy(rel, use, sizeof rel - 1); rel[sizeof rel - 1] = 0;
            u_join(full, sizeof full, dest, rel);

            if (rel[strlen(rel) - 1] == '/') { /* a directory entry */
                char d[512];
                strncpy(d, rel, sizeof d - 1); d[sizeof d - 1] = 0;
                d[strlen(d) - 1] = 0;
                mk_parents(dest, rel);
                u_join(full, sizeof full, dest, d);
                u_mkdir(full);
                continue;
            }
            mk_parents(dest, rel);
write_member:
            fseek(f, (long)lho, SEEK_SET);
            if (fread(lh, 1, 30, f) != 30 || rd32(lh) != 0x04034b50) {
                snprintf(why, 160, "bad local header for %s", name); goto out;
            }
            fseek(f, (long)(lho + 30 + rd16(lh + 26) + rd16(lh + 28)), SEEK_SET);

            {
                FILE *o = fopen(full, "wb");
                unsigned char *in, *outb;
                if (!o) { snprintf(why, 160, "cannot write %s", full); goto out; }
                in = (unsigned char *)malloc(csz ? csz : 1);
                outb = (unsigned char *)malloc(usz ? usz : 1);
                if (!in || !outb) { free(in); free(outb); fclose(o);
                                    snprintf(why, 160, "out of memory"); goto out; }
                if (fread(in, 1, csz, f) != csz) {
                    free(in); free(outb); fclose(o);
                    snprintf(why, 160, "short read in %s", name); goto out;
                }
                if (method == 0) {
                    if (fwrite(in, 1, csz, o) != csz) {
                        free(in); free(outb); fclose(o);
                        snprintf(why, 160, "write failed: %s", full); goto out;
                    }
                } else if (method == 8) {
                    z_stream z;
                    int zr;
                    memset(&z, 0, sizeof z);
                    z.next_in = in; z.avail_in = csz;
                    z.next_out = outb; z.avail_out = usz;
                    if (inflateInit2(&z, -15) != Z_OK) {
                        free(in); free(outb); fclose(o);
                        snprintf(why, 160, "inflateInit failed"); goto out;
                    }
                    zr = inflate(&z, Z_FINISH);
                    inflateEnd(&z);
                    if (zr != Z_STREAM_END || z.total_out != usz) {
                        free(in); free(outb); fclose(o);
                        snprintf(why, 160, "inflate failed for %s", name); goto out;
                    }
                    if (fwrite(outb, 1, usz, o) != usz) {
                        free(in); free(outb); fclose(o);
                        snprintf(why, 160, "write failed: %s", full); goto out;
                    }
                } else {
                    free(in); free(outb); fclose(o);
                    snprintf(why, 160, "unsupported compression %u in %s", method, name);
                    goto out;
                }
                free(in); free(outb);
                if (fclose(o) != 0) { snprintf(why, 160, "close failed: %s", full); goto out; }
            }
            if (cb && cb(rel, full, user) != 0) { snprintf(why, 160, "aborted at %s", rel); goto out; }
        }
    }
    if (only && !found) { snprintf(why, 160, "no member %s in the archive", only); goto out; }
    rc = 0;
out:
    free(cd);
    fclose(f);
    return rc;
}
