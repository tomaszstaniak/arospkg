/* apkg self-update: replace the running apkg with the latest stable release
 * for exactly this build's CPU and ABI. docs/self-update.md is the user's
 * side of it.
 *
 * Where the release comes from: GitHub's releases/latest/download/, which
 * names the latest STABLE release (prereleases never), and in it
 * SHA256SUMS and arospkg-<version>.<target>.zip, the names
 * tools/make-rc.sh gives them.
 *
 * Measured on AROS One 1.3 (tests/selfupdate/selfren.c): a running program
 * is loaded whole and holds no lock on its file, so it can rename its own
 * file, put a new one in its place and delete the old one, on SYS:, RAM:
 * and C:. So the swap is done here, in this process, with no helper:
 * <self>.new is written and checked first, <self> becomes <self>.old,
 * <self>.new becomes <self>, and if that last step fails the first is
 * undone. */
#include "selfupdate.h"
#include "present.h"
#include "../libpkg/pkg.h"
#include "../libpkg/net.h"
#include "../libpkg/zip.h"
#include "../libpkg/sha256.h"
#include "../libpkg/util.h"
#include <proto/dos.h>
#include <dos/dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LATEST "https://github.com/tomaszstaniak/arospkg/releases/latest/download/"

static const char *from_s, *sums_s, *fail_s;
void su_test_from(const char *u) { from_s = u; }
void su_test_sums(const char *f) { sums_s = f; }
void su_test_fail(const char *w) { fail_s = w; }

/* The archive name's target part for this build, or NULL. Kept beside the
   checks the index uses: this build's own CPU and ABI, nothing else. */
static const char *target(void)
{
    const char *a = pkg_arch(), *b = pkg_abi();
    if (!strcmp(a, "x86_64") && !strcmp(b, "v11")) return "x86_64-aros-v11";
    if (!strcmp(a, "x86_64") && !strcmp(b, "v1"))  return "x86_64-aros-v1";
    if (!strcmp(a, "i386")   && !strcmp(b, "v0"))  return "i386-aros-v0";
    if (!strcmp(a, "aarch64"))                     return "aarch64-aros";
    return NULL;
}

/* The file this process was loaded from: its PROGDIR: and its name. Never
   another copy found on the command path. */
static int self_path(char *out, size_t n)
{
    char name[256];
    if (!GetProgramName((STRPTR)name, sizeof name) || !name[0]) return -1;
    if (!GetProgramDir() || !NameFromLock(GetProgramDir(), (STRPTR)out, (LONG)n)) return -1;
    if (!AddPart((STRPTR)out, FilePart((STRPTR)name), (LONG)n)) return -1;
    return u_exists(out) ? 0 : -1;
}

/* 1.2.3[-x]: numbers first; a release outranks its own pre-releases. */
static int vercmp(const char *a, const char *b)
{
    while (*a || *b) {
        long x = strtol(a, (char **)&a, 10), y = strtol(b, (char **)&b, 10);
        if (x != y) return x < y ? -1 : 1;
        if (*a == '.' && *b == '.') { a++; b++; continue; }
        if (*a == *b) return 0;                 /* both ended */
        if (!*a) return 1;                      /* a is the release, b a pre-release */
        if (!*b) return -1;
        return strcmp(a, b) < 0 ? -1 : 1;
    }
    return 0;
}

/* The temporary directory; T: when the system has it. */
static void tmpname(char *out, size_t n, const char *leaf)
{
    snprintf(out, n, "%s%s", u_exists("T:") ? "T:" : "RAM:", leaf);
}

/* The CPU of an ELF file, and whether it carries this build's ABI tag. */
static int same_target(const char *file, char why[240])
{
    size_t len, i, tl;
    char tag[40];
    unsigned char *d = (unsigned char *)u_read_all(file, &len);
    unsigned m, want = !strcmp(pkg_arch(), "x86_64") ? 62 : !strcmp(pkg_arch(), "i386") ? 3 : 183;
    int found = 0;
    if (!d || len < 64 || memcmp(d, "\177ELF", 4)) {
        free(d); snprintf(why, 240, "the new apkg is not an AROS program"); return -1;
    }
    m = d[18] | (d[19] << 8);
    snprintf(tag, sizeof tag, "arospkg-abi=%s", pkg_abi());
    tl = strlen(tag) + 1;                       /* with its terminating zero */
    for (i = 0; i + tl <= len && !found; i++) if (!memcmp(d + i, tag, tl)) found = 1;
    free(d);
    if (m != want) { snprintf(why, 240, "the new apkg is for another CPU"); return -1; }
    if (!found) { snprintf(why, 240, "the new apkg is not built for ABI %s", pkg_abi()); return -1; }
    return 0;
}

/* net_fetch leaves its file as <dest>.part; take it only when complete. */
static int fetch(const char *url, const char *dest, long max, char why[240])
{
    char part[PKG_MAXPATH];
    snprintf(part, sizeof part, "%s.part", dest);
    u_delete(dest);
    if (net_fetch(url, dest, max, why) != 0) { u_delete(part); return -1; }
    if (!Rename((CONST_STRPTR)part, (CONST_STRPTR)dest)) {
        u_delete(part); snprintf(why, 240, "cannot write %s", dest); return -1;
    }
    return 0;
}

static int checking;                     /* --check: the wording of a failure */

static int fail(const char *what, const char *detail)
{
    char head[300];
    snprintf(head, sizeof head, "Could not %s: %s", checking ? "check for a newer apkg" : "update apkg", what);
    fputs(pr_color(PR_RED), stdout);
    pr_text(head, 0);
    fputs(pr_off(), stdout);
    if (detail && *detail) pr_text(detail, 2);
    return 10;
}

int apkg_self_update(const char *current, const char *root, int check_only)
{
    char self[512], fresh[520], prev[520], lockf[512];
    char sums[PKG_MAXPATH], zip[PKG_MAXPATH], build[PKG_MAXPATH];
    char url[1024], why[240], zwhy[160], name[160], member[200], ver[40], suffix[40];
    char want_zip[65] = "", want_bin[65] = "", got[65];
    const char *t = target(), *base = from_s ? from_s : LATEST;
    char *list = NULL, *line, *b = NULL;
    int rc = 10, c, found = 0;
    struct FileInfoBlock *fib;
    LONG prot = 0;

    fresh[0] = 0;
    checking = check_only;
    if (!t) return fail("this build's CPU and ABI have no release archive",
                        "Update it by hand from https://github.com/tomaszstaniak/arospkg/releases");
    if (self_path(self, sizeof self) != 0)
        return fail("cannot find the file this apkg was started from",
                    "A resident apkg is not updated; start it from its file and try again.");

    tmpname(sums, sizeof sums, "apkg-self-update.sums");
    tmpname(zip, sizeof zip, "apkg-self-update.zip");
    tmpname(build, sizeof build, "apkg-self-update.build");

    if (net_open(why) != 0) return fail("no network", why);
    if (sums_s) {                               /* testing only */
        list = u_read_all(sums_s, NULL);
        if (!list) { rc = fail("cannot read the test checksum file", sums_s); goto out; }
    } else {
        snprintf(url, sizeof url, "%sSHA256SUMS", base);
        if (fetch(url, sums, 65536, why) != 0) { rc = fail("cannot fetch the release's checksums", why); goto out; }
        list = u_read_all(sums, NULL);
    }
    /* "<sha256>  arospkg-<version>.<target>.zip", one archive per target */
    snprintf(suffix, sizeof suffix, ".%s.zip", t);
    for (line = list; line && *line; line = b ? b + 1 : NULL) {
        char sha[80], fn[200];
        size_t fl, sl = strlen(suffix);
        b = strchr(line, '\n');
        if (sscanf(line, "%79s %199s", sha, fn) == 2 && strlen(sha) == 64 &&
            !strncmp(fn, "arospkg-", 8) && (fl = strlen(fn)) > 8 + sl &&
            !strcmp(fn + fl - sl, suffix) && fl - sl - 8 < sizeof ver) {
            found++;
            snprintf(want_zip, sizeof want_zip, "%s", sha);
            snprintf(name, sizeof name, "%s", fn);
            snprintf(ver, sizeof ver, "%.*s", (int)(fl - sl - 8), fn + 8);
        }
        if (!b) break;
    }
    if (found != 1) {
        rc = fail(found ? "the release lists more than one archive for this target"
                        : "the latest stable release has no archive for this CPU and ABI", t);
        goto out;
    }

    c = vercmp(current, ver);
    {
        char line[600];
        snprintf(line, sizeof line, "apkg %s", current);
        pr_heading(line);
        pr_field("Location", self, -1);
        snprintf(line, sizeof line, "%s (%s)", ver, t);
        pr_field("Latest stable release", line, -1);
        putchar('\n');
    }
    if (c >= 0) {
        if (c == 0) { pr_word(PR_GREEN, "Up to date"); putchar('\n'); }
        else printf("This apkg is newer than the latest stable release.\n");
        rc = 0; goto out;
    }
    if (check_only) { printf("A newer release is available. Install it with: apkg self-update\n"); rc = 0; goto out; }

    /* A package operation in progress may start apkg again from this file:
       leave the file alone until it has finished. */
    snprintf(lockf, sizeof lockf, "%s/db/lock.owner", root);
    if (u_exists(lockf)) {
        rc = fail("an apkg or PkgManager operation is running", "Try again when it has finished.");
        goto out;
    }

    {
        char line[120];
        snprintf(line, sizeof line, "Updating apkg %s to %s", current, ver);
        pr_heading(line);
    }
    snprintf(url, sizeof url, "%s%s", base, name);
    printf("Downloading %s\n", name);
    if (pkg_verbose()) printf("  from %s\n", url);
    if (fetch(url, zip, 16L * 1024 * 1024, why) != 0) { rc = fail("cannot fetch the release archive", why); goto out; }
    printf("Verifying archive\n");
    if (sha256_file(zip, got) != 0 || strcmp(got, want_zip) != 0) {
        rc = fail("the archive does not match the release's checksum", got); goto out;
    }
    /* The program's own checksum is in the archive's BUILD.txt. */
    snprintf(member, sizeof member, "arospkg-%s.%s/BUILD.txt", ver, t);
    if (zip_extract_member(zip, member, build, 65536, zwhy) != 0) { rc = fail("the archive has no BUILD.txt", zwhy); goto out; }
    {
        char *bt = u_read_all(build, NULL), *l;
        for (l = bt; l && *l; ) {
            char sha[80], fn[64];
            if (sscanf(l, "%79s %63s", sha, fn) == 2 && strlen(sha) == 64 && !strcmp(fn, "apkg"))
                snprintf(want_bin, sizeof want_bin, "%s", sha);
            l = strchr(l, '\n'); if (l) l++;
        }
        free(bt);
    }
    if (!want_bin[0]) { rc = fail("BUILD.txt in the archive does not name apkg's checksum", ""); goto out; }

    snprintf(fresh, sizeof fresh, "%s.new", self);
    snprintf(prev, sizeof prev, "%s.old", self);
    snprintf(member, sizeof member, "arospkg-%s.%s/apkg", ver, t);
    u_delete(fresh);
    if (zip_extract_member(zip, member, fresh, 16L * 1024 * 1024, zwhy) != 0) {
        u_delete(fresh); rc = fail("cannot unpack apkg from the archive", zwhy); goto out;
    }
    if (sha256_file(fresh, got) != 0 || strcmp(got, want_bin) != 0) {
        u_delete(fresh); rc = fail("the unpacked apkg does not match BUILD.txt", got); goto out;
    }
    if (same_target(fresh, why) != 0) { u_delete(fresh); rc = fail(why, fresh); goto out; }

    printf("Installing apkg\n");
    /* The new file gets the old one's protection bits, so it stays runnable. */
    if ((fib = AllocDosObject(DOS_FIB, NULL))) {
        BPTR l = Lock((CONST_STRPTR)self, SHARED_LOCK);
        if (l && Examine(l, fib)) prot = fib->fib_Protection;
        if (l) UnLock(l);
        FreeDosObject(DOS_FIB, fib);
    }
    SetProtection((CONST_STRPTR)fresh, prot);

    u_delete(prev);
    if (!Rename((CONST_STRPTR)self, (CONST_STRPTR)prev)) {
        u_delete(fresh); rc = fail("cannot move the current apkg aside; nothing changed", self); goto out;
    }
    if (fail_s && !strcmp(fail_s, "stop")) {    /* testing only: as if the machine stopped here */
        printf("(test) stopped after moving %s to %s\n", self, prev);
        rc = 20; goto keep;
    }
    if ((fail_s && !strcmp(fail_s, "swap")) || !Rename((CONST_STRPTR)fresh, (CONST_STRPTR)self)) {
        LONG back = Rename((CONST_STRPTR)prev, (CONST_STRPTR)self);
        u_delete(fresh);
        rc = fail(back ? "cannot put the new apkg in place; the previous one is back, nothing changed"
                       : "cannot put the new apkg in place, nor the previous one back",
                  back ? self : "Rename the .old file to the original name to get it back.");
        goto out;
    }
    if (sha256_file(self, got) != 0 || strcmp(got, want_bin) != 0) {
        rc = fail("the installed file does not match after the update", self); goto out;
    }
    putchar('\n');
    pr_word(PR_GREEN, "Updated"); printf(" apkg to %s\n", ver);
    pr_field("Location", self, -1);
    pr_field("Previous version", prev, -1);
    /* This process is still the old code; only the file has changed. */
    pr_text("The new version runs from the next apkg command.", 2);
    pr_text("PkgManager is updated separately, from the release archive.", 2);
    rc = 0;
out:
    if (fresh[0]) u_delete(fresh);         /* gone already after a swap */
keep:
    free(list);
    u_delete(sums); u_delete(zip); u_delete(build);
    net_close();
    return rc;
}
