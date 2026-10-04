/* apkg -- the command line front end. Presentation only: every decision is in
 * libpkg, so that PkgManager can make the same ones without reimplementing
 * them. Errors are printed as libpkg wrote them rather than re-phrased here. */

#include "../libpkg/pkg.h"
#include "../libpkg/tls.h"
#include "../libpkg/sha256.h"
#include "../libpkg/verify.h"
#include "../libpkg/entries.h"
#include "present.h"
#include "hint.h"
#include "selfupdate.h"
#include "../about.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* APKG_TEST_VERSION lets a self-update test build call itself older than the
   release it updates to; a release never sets it. */
#ifdef APKG_TEST_VERSION
static const char *VERSION = "apkg " APKG_TEST_VERSION;
#else
static const char *VERSION = "apkg 0.3.2";
#endif

/* `show`: the library's account of one package, laid out for a person. The
 * facts come from pkg_details_get and the probes behind the window's panel;
 * nothing is decided here. */
/* A verdict's colour; the word itself is printed in every mode. */
static void verdict(const char *w)
{
    int c = -1;
    if (!strcmp(w, "native") || !strcmp(w, "satisfied")) c = PR_GREEN;
    else if (!strcmp(w, "undetermined")) c = PR_YELLOW;
    else if (!strcmp(w, "incompatible") || !strcmp(w, "missing")) c = PR_RED;
    if (c < 0) fputs(w, stdout); else pr_word(c, w);
}

static void print_notes(const pkg_details *d, int after_install);

static const char *hint_root;

static void show_rich(pkg_ctx *c, const char *index, const char *id, const pkg_details *d)
{
    char version[80], text[1200], why[240];
    rev_label(version, sizeof version, d->e.version, d->e.revision);
    snprintf(text, sizeof text, "%s %s", d->e.id, version);
    pr_heading(text);
    if (*d->e.summary) pr_text(d->e.summary, 2);
    putchar('\n');
    if (d->e.installed) {
        rev_label(version, sizeof version, d->e.installed_version, d->e.installed_revision);
        snprintf(text, sizeof text, "installed (%s)", version);
        pr_field("State", text, PR_GREEN);
        pr_field("Location", d->installed_dir, -1);
        open_hint(text, sizeof text, hint_root, d->e.id);
        pr_field("Open folder", text, -1);
        if (*d->installed_when) pr_field("Installed on", d->installed_when, -1);
        if (*d->installed_arch) {
            snprintf(text, sizeof text, "%s / %s", d->installed_arch,
                     *d->installed_abi ? d->installed_abi : "ABI not recorded");
            pr_field("Installed target", text, -1);
        }
    } else pr_field("State", "not installed", -1);
    if (d->index_missing) pr_field("Catalogue", "missing -- run apkg update", PR_YELLOW);
    else if (!d->in_index) pr_field("Catalogue", "no longer lists this package", PR_YELLOW);
    if (d->ambiguous) pr_field("Selection", "multiple matching builds; installation refused", PR_RED);
    if (*d->e.category) pr_field("Category", d->e.category, -1);
    /* Before the technical sections: "needs the original game data" is
       what decides whether to install at all. */
    print_notes(d, 0);

    putchar('\n'); pr_heading("Compatibility and requirements");
    if (d->in_index) {
        snprintf(text, sizeof text, "%s / %s", d->e.arch, d->e.abi);
        pr_field("Target", text, -1);
        if (d->nvariants > 1) pr_field("Other builds", d->variants, -1);
    }
    snprintf(text, sizeof text, "%s -- %s", pkg_compat_word(d->compat), d->compat_why);
    pr_field("Compatibility", text, d->compat == PKG_COMPAT_NATIVE ? PR_GREEN :
             d->compat == PKG_COMPAT_INCOMPATIBLE ? PR_RED : PR_YELLOW);
    if (d->in_index) {
        if (d->compat != PKG_COMPAT_NATIVE)
            pr_field("Requirements", "not checked for this target", PR_YELLOW);
        else if (!*d->e.requires) pr_field("Requirements", "none stated", -1);
        else {
            pkg_err err; memset(&err, 0, sizeof err);
            pr_field("Requirements", d->requirements, !strcmp(d->requirements, "satisfied") ? PR_GREEN :
                     !strcmp(d->requirements, "missing") ? PR_RED : PR_YELLOW);
            if (pkg_requirements(c, index, id, text, sizeof text, &err) == PKG_OK)
                pr_text(text, 4);
            else { pr_text(d->e.requires, 4); pr_text(err.summary, 4); }
        }
    }
    if (d->e.installed) {
        putchar('\n'); pr_heading("Installed package actions");
        if (d->in_index) {
            int yes = pkg_can_upgrade(&d->e, why, sizeof why);
            pr_field("Upgrade", yes ? "available" : why, yes ? PR_GREEN : -1);
        }
        {
            int yes = pkg_can_rollback(c, id, why, sizeof why);
            pr_field("Roll back", yes ? "available" : why, yes ? PR_GREEN : -1);
        }
    }
    putchar('\n'); pr_heading("Download and provenance");
    if (d->in_index) {
        if (d->e.size >= 0) {
            snprintf(text, sizeof text, "%ld KiB (%ld bytes)", d->e.size / 1024, d->e.size);
            pr_field("Download", text, -1);
        }
        pr_field("URL", d->url, -1);
        if (*d->sha256) pr_field("SHA-256", d->sha256, -1);
    }
    if (*d->source) pr_field("Source", d->source, -1);
    if (*d->license) pr_field("License", d->license, -1);
}

static pkg_status show_package(pkg_ctx *c, const char *index, const char *id, pkg_err *e)
{
    pkg_details d; pkg_status st; char a[80], b[80], why[240];
    st = pkg_details_get(c, index, id, &d, e);
    if (st != PKG_OK) return st;
    if (pr_rich()) { show_rich(c, index, id, &d); return PKG_OK; }
    rev_label(a, sizeof a, d.e.version, d.e.revision);
    printf("%s%s %s%s\n", pr_bold(), d.e.id, a, pr_off());
    if (d.e.summary[0]) printf("  %s\n", d.e.summary);
    if (d.index_missing) printf("  index:        none on this machine -- apkg update fetches it\n");
    else if (!d.in_index) printf("  index:        does not list it any more\n");
    if (d.ambiguous) printf("  ambiguous:    the index lists %d builds of it for this system; install refuses\n", d.ambiguous);
    if (d.e.category[0]) printf("  category:     %s\n", d.e.category);
    if (d.in_index) {
        printf("  target:       %s, ABI %s", d.e.arch[0] ? d.e.arch : "?", d.e.abi[0] ? d.e.abi : "?");
        if (d.nvariants > 1) printf(" (the index lists: %s)", d.variants);
        printf("\n");
    }
    printf("  compatibility: "); verdict(pkg_compat_word(d.compat)); printf(" -- %s\n", d.compat_why);
    if (d.in_index) {
        if (d.e.size >= 0) printf("  download:     %ld KB from %s\n", (d.e.size + 1023) / 1024, d.url);
        else printf("  download:     %s\n", d.url);
        if (d.sha256[0]) printf("  sha256:       %s\n", d.sha256);
    }
    if (d.source[0])  printf("  source:       %s\n", d.source);
    if (d.license[0]) printf("  license:      %s\n", d.license);
    if (d.in_index) {
        if (d.compat != PKG_COMPAT_NATIVE)
            printf("  requirements: not checked -- it is not compatible with this machine\n");
        else if (!d.e.requires[0])
            printf("  requirements: none stated\n");
        else {
            char reqs[1024]; pkg_err e2; memset(&e2, 0, sizeof e2);
            if (pkg_requirements(c, index, id, reqs, sizeof reqs, &e2) == PKG_OK) {
                char *line = reqs, *nl;
                printf("  requirements: "); verdict(d.requirements); printf(", probed now:\n");
                while (line && *line) {
                    char *colon, *sp;
                    nl = strchr(line, '\n'); if (nl) *nl = 0;
                    /* "crt.library: satisfied (...)": the verdict in colour */
                    colon = strstr(line, ": ");
                    sp = colon ? strchr(colon + 2, ' ') : NULL;
                    if (colon && sp) {
                        *sp = 0;
                        printf("    %.*s: ", (int)(colon - line), line); verdict(colon + 2); printf(" %s\n", sp + 1);
                    } else if (colon) {
                        printf("    %.*s: ", (int)(colon - line), line); verdict(colon + 2); printf("\n");
                    } else printf("    %s\n", line);
                    line = nl ? nl + 1 : NULL;
                }
            } else printf("  requirements: %s (%s)\n", d.e.requires, e2.summary);
        }
    }
    if (!d.e.installed) printf("  installed:    no\n");
    else {
        rev_label(b, sizeof b, d.e.installed_version, d.e.installed_revision);
        printf("  installed:    %s%s%s%s%s%s, in %s%s%s\n", pr_color(PR_GREEN), b, pr_off(),
               d.installed_arch[0] ? " (" : "", d.installed_arch, d.installed_arch[0] ? ")" : "",
               d.installed_dir, d.installed_when[0] ? ", since " : "", d.installed_when);
        if (d.in_index) {
            if (pkg_can_upgrade(&d.e, why, sizeof why)) printf("  upgrade:      available, %s -> %s\n", b, a);
            else printf("  upgrade:      no -- %s\n", why);
        }
        if (pkg_can_rollback(c, id, why, sizeof why)) printf("  roll back:    possible\n");
        else printf("  roll back:    no -- %s\n", why);
    }
    print_notes(&d, 0);
    return PKG_OK;
}

/* Rich search consumes the same rows as JSON. The renderer wraps all fields
 * and switches to blocks when the table cannot fit; no description is lost. */
static void search_table(const pkg_entries *es, int hidden)
{
    int i;
    char text[160];
    snprintf(text, sizeof text, "%d package%s", es->n, es->n == 1 ? "" : "s");
    pr_heading(text);
    if (!es->n) pr_text("Nothing matches this search.", 0);
    else pr_search_heading();
    for (i = 0; i < es->n; i++) {
        const pkg_entry *p = &es->v[i];
        char version[80], note[100];
        snprintf(note, sizeof note, "%s/%s", p->arch, p->abi);
        if (!p->ours) {
            const char *why = (*pkg_arch() && *p->arch && strcmp(p->arch, pkg_arch())) ? "other CPU" :
                              !pkg_abi_known(p->abi) ? "unknown ABI" : "other ABI";
            snprintf(note, sizeof note, "%s: %s/%s", why, p->arch, p->abi);
        }
        rev_label(version, sizeof version, p->version, p->revision);
        pr_search_row(p->id, version, p->installed ? "installed" : "not installed", p->summary, note);
    }
    if (hidden) {
        snprintf(text, sizeof text, "%d hidden: other CPU/ABI or unknown compatibility. Use --all-abi to list them.", hidden);
        pr_text(text, 0);
    }
    pr_text("Details: apkg show <id>", 0);
}

static void usage(void)
{
    printf("%s -- Find and install software for AROS\n\n", VERSION);
    printf("Usage: apkg [options] <command>\n\n"
           "Commands:\n"
           "  search [term]     Search available packages\n"
           "  show <id>         Show package details and requirements\n"
           "  install <id>      Install a package\n"
           "  list              List installed packages\n"
           "  update            Refresh the package catalogue\n"
           "  upgrade <id>      Install a newer revision of the same version\n"
           "  rollback <id>     Restore the previous package revision\n"
           "  remove <id>       Remove a package; keep locally modified files\n"
           "  verify <id>       Check installed files for changes\n"
           "  info <id>         Show the installed package record\n"
           "  doctor            Report installation and recovery problems\n"
           "  open <id>         Open an installed package's drawer on the desktop\n"
           "  self-update       Replace this apkg with the latest stable release\n"
           "  self-update --check  Say whether there is a newer one\n\n"
           "Options:\n"
           "  --root <dir>      Package directory (default: SYS:Packages)\n"
           "  --plain           Disable formatting and progress output\n"
           "  --color=<mode>    Colour output: auto, always or never\n"
           "  --json            Output search and list results as JSON\n"
           "  --verbose         Also show download addresses and file locations\n"
           "  --version         Show version and build information\n"
           "  --about           Show author, project and licence information\n"
           "  --help            Show this help\n"
           "  --help-all        Include advanced and testing options\n"
           "  --help-testing    Show testing options\n\n"
           "Example:\n"
           "  apkg update\n"
           "  apkg search soliton\n"
           "  apkg install soliton\n");
}

static void usage_testing(void)
{
    printf("Testing options (use a separate --root):\n"
           "  assert-hash <file> <sha>  Check a file's SHA-256\n"
           "  --verify-name <name>     Override the TLS certificate hostname\n"
           "  --abi <v0|v1|v11>        Override ABI selection for testing\n"
           "  --expect <status>        Require the specified exit status\n"
           "  --report <file>          Write a machine-readable test report\n"
           "  --run-id <id>            Set the test report identifier\n"
           "  --machine <name>         Record the test machine name\n"
           "  --slow <ms>              Delay download and extraction steps\n"
           "  --progress               Print progress callback events\n"
           "  --fail-at commit|doctor  Simulate a failed write\n"
           "  --break-download-at <n>  Simulate one connection reset after n bytes\n"
           "  --self-update-from <url> Release base URL for self-update (https, ends in /)\n"
           "  --self-update-sums <file> Use a local SHA256SUMS for self-update\n"
           "  --self-update-fail-at <swap|stop> Fail the self-update at the swap\n"
           "  --corrupt-download-at <n> Simulate one TLS data error after n bytes\n"
           "  --cancel-at <phase>      Request cancellation in a phase\n"
           "                          download, download-mid, verify, extract, publish\n"
           "  --break-recovery <n>     Interrupt recovery after n files\n"
           "  --interrupt-at <n>       Interrupt an operation at a checkpoint:\n"
           "    1 plan; 2 directory; 3 directory marker; 4 registry;\n"
           "    5 before commit; 6 remove; 7 publish; 8 drawer icon;\n"
           "    9 old tree moved; 10 new tree published\n"
           "Interrupted operations may require recovery on the next run.\n");
}

static void usage_all(void)
{
    usage();
    printf("\nAdvanced options:\n"
           "  requires <id>       Check system requirements for a package\n"
           "  unlock              Remove a stale operation lock\n"
           "  doctor --retry      Retry pending recovery\n"
           "  --index <file>      Read a local catalogue file\n"
           "  --index-url <url>   Set the catalogue download URL\n"
           "  --all-abi           Include packages for other CPUs and ABIs\n"
           "  --dry-run           Preview install/remove/upgrade/rollback\n"
           "  --fetch             Allow downloads during a dry run\n\n");
    usage_testing();
}

/* The progress contract, driven from the command line so a guest run can show
 * the callback fires and that cancelling does exactly what pkg.h promises. */
static const char *cancel_at = NULL;
static int show_progress = 0;
static void cli_progress(void *u, const pkg_progress_ev *ev, int *cancel)
{
    (void)u;
    if (show_progress)
        printf("progress %s %s %lu/%lu cancel=%s\n", ev->id, ev->phase, ev->done, ev->total,
               ev->can_cancel ? "yes" : "no");
    else pr_progress(ev->id, ev->phase, ev->done, ev->total);
    /* Set regardless of can_cancel on purpose: the test is that the LIBRARY
       ignores it where it said it would, not that this caller is polite. */
    if (cancel_at && strcmp(ev->phase, cancel_at) == 0) *cancel = 1;
    /* "download-mid": only once bytes are flowing, so the cancel lands inside
       the transport rather than at the check before it. */
    if (cancel_at && strcmp(cancel_at, "download-mid") == 0 &&
        strcmp(ev->phase, "download") == 0 && ev->done > 0) *cancel = 1;
}

/* Rows as a JSON array, one object per line. The strings are the index's and
 * the registry's, so the escapes needed are the two our own writer emits. */
static void jstr(const char *v)
{
    putchar('"');
    for (; *v; v++) {
        if (*v == '"' || *v == '\\') putchar('\\');
        putchar(*v);
    }
    putchar('"');
}

static void print_rows(const pkg_entries *es, int hidden)
{
    int i;
    printf("{ \"rows\": [");
    for (i = 0; i < es->n; i++) {
        const pkg_entry *p = &es->v[i];
        printf("%s\n  { \"id\": ", i ? "," : "");     jstr(p->id);
        printf(", \"version\": ");                    jstr(p->version);
        printf(", \"revision\": %ld, \"summary\": ", p->revision); jstr(p->summary);
        printf(", \"category\": ");                   jstr(p->category);
        printf(", \"arch\": ");                       jstr(p->arch);
        printf(", \"abi\": ");                        jstr(p->abi);
        printf(", \"kind\": ");                       jstr(p->kind);
        printf(", \"size\": %ld, \"ours\": %s, \"installed\": %s }",
               p->size, p->ours ? "true" : "false", p->installed ? "true" : "false");
    }
    printf("%s], \"hidden\": %d }\n", es->n ? "\n" : "", hidden);
}

/* One place, so every exit path records the run. */
static void write_report(const char *path, const char *run_id, const char *machine,
                         const char *cmd, const char *arg, const char *root,
                         pkg_stop stop, pkg_status st, const pkg_err *e)
{
    FILE *f;
    char rid[64];
    if (!path) return;
    f = fopen(path, "wb");
    if (!f) { printf("apkg: cannot write the report to %s\n", path); return; }
    if (run_id) snprintf(rid, sizeof rid, "%s", run_id);
    else        snprintf(rid, sizeof rid, "%lx", (unsigned long)time(NULL));
    {
        char self[65];
        pkg_self_sha256(self);      /* "unknown" if it could not be read */
        fprintf(f,
            "{ \"run_id\": \"%s\", \"machine\": \"%s\",\n"
            "  \"binary_sha256\": \"%s\",\n"
            "  \"source_id\": \"%s\", \"toolchain\": \"%s\",\n"
            "  \"sdk\": \"%s\", \"cc\": \"%s\",\n"
            "  \"command\": \"%s\", \"argument\": \"%s\", \"root\": \"%s\",\n"
            "  \"interrupt_at\": %d,\n"
            "  \"status\": %d, \"status_text\": \"%s\",\n"
            "  \"summary\": \"%s\", \"subject\": \"%s\" }\n",
            rid, machine, self, pkg_build_id(), pkg_toolchain(),
            pkg_sdk(), pkg_cc(), cmd, arg ? arg : "", root,
            (int)stop, (int)st, pkg_strstatus(st),
            st == PKG_OK ? "ok" : e->summary, st == PKG_OK ? "" : e->subject);
    }
    fclose(f);
    printf("report %s\n", path);
}

/* What the command was doing, for its error: "install soliton". Empty for
   commands whose errors read on their own. */
static char doing[96];
static const char *doing_id;       /* the package named in `doing` */
static const char *cancel_noun;    /* "Installation", for "Installation cancelled" */

/* The error as libpkg worded it -- the cause is never replaced by a
   generic one -- after what was being done, and the subject and detail on
   their own lines. A cancel is said as a cancel, not as a failure. */
static void show(const pkg_err *e)
{
    char head[400];
    pr_end_line();
    if (doing[0] && e->status == PKG_E_CANCELLED)
        /* libpkg's summary ("download cancelled") only repeats the word;
           anything it says beyond that is in the detail line. */
        snprintf(head, sizeof head, "%s cancelled: %s%s%s", cancel_noun ? cancel_noun : "Operation",
                 doing_id ? doing_id : doing,
                 strstr(e->summary, "cancel") ? "" : " -- ", strstr(e->summary, "cancel") ? "" : e->summary);
    else if (doing[0])
        snprintf(head, sizeof head, "Could not %s: %s", doing, e->summary);
    else
        snprintf(head, sizeof head, "apkg: %s", e->summary);
    fputs(pr_color(PR_RED), stdout);
    pr_text(head, 0);
    fputs(pr_off(), stdout);
    /* The subject is often the package itself, already in the first line. */
    if (e->subject[0] && !(doing_id && !strcmp(e->subject, doing_id))) pr_text(e->subject, 2);
    if (e->detail[0])  pr_text(e->detail, 2);
}

/* The package's notes as a section. After an install, the copy just
   recorded; in show, the catalogue's current text, or the recorded copy
   when the catalogue no longer has the package. */
static void print_notes(const pkg_details *d, int after_install)
{
    const char *n = after_install || !d->in_index ? d->notes_recorded : d->notes;
    if (!after_install && d->in_index && d->notes_bad) {
        pr_section("Notes from the package");
        pr_text("Not shown: they are not plain lines this apkg can display.", 2);
        return;
    }
    if (!*n) return;
    pr_section(after_install || d->in_index ? "Notes from the package" : "Notes recorded at installation");
    pr_text(n, 2);
}

/* After an install, upgrade or rollback: what is installed now, where, and
   how to get to it, from the registry entry just written. */
static void print_installed(pkg_ctx *c, const char *index, const char *id, const char *verb)
{
    pkg_details d; pkg_err e2; char v[80], line[2 * PKG_MAXPATH + PKG_MAXID + 32];
    memset(&e2, 0, sizeof e2);
    putchar('\n');
    if (pkg_details_get(c, index, id, &d, &e2) != PKG_OK || !d.e.installed) {
        pr_word(PR_GREEN, verb); printf(" %s\n", id);
        return;
    }
    rev_label(v, sizeof v, d.e.installed_version, d.e.installed_revision);
    pr_word(PR_GREEN, verb); printf(" %s %s\n", id, v);
    pr_field("Location", d.installed_dir, -1);
    open_hint(line, sizeof line, hint_root, id);
    pr_field("Open folder", line, -1);
    print_notes(&d, 1);
}

/* The version the catalogue offers, for an operation's first line. */
static void offered(pkg_ctx *c, const char *index, const char *id, char *v, size_t n, int installed)
{
    pkg_details d; pkg_err e2;
    v[0] = 0;
    memset(&e2, 0, sizeof e2);
    if (pkg_details_get(c, index, id, &d, &e2) != PKG_OK) return;
    if (installed && d.e.installed) rev_label(v, n, d.e.installed_version, d.e.installed_revision);
    else if (!installed && d.in_index) rev_label(v, n, d.e.version, d.e.revision);
}

static void heading2(const char *verb, const char *id, const char *v)
{
    char t[200];
    snprintf(t, sizeof t, "%s %s%s%s", verb, id, *v ? " " : "", v);
    pr_heading(t);
}

static int real_main(int argc, char **argv)
{
    const char *root = "SYS:Packages";
    const char *index = NULL;
    char indexbuf[512];
    pkg_stop stop = PKG_STOP_NONE;
    int i;
    const char *cmd = NULL, *arg = NULL;
    const char *report = NULL, *run_id = NULL, *machine = "unspecified";
    const char *index_url =
        "https://raw.githubusercontent.com/tomaszstaniak/arospkg-index/main/index.json";
    const char *arg2 = NULL;
    int expect = -1, retry = 0, json = 0, dry = 0, fetch = 0, plain = 0, color = PR_COLOR_AUTO, check = 0;
    pkg_ctx *c = NULL;
    pkg_err e;
    pkg_status st;

    memset(&e, 0, sizeof e);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root") && i + 1 < argc)        hint_root = root = argv[++i];
        else if (!strcmp(argv[i], "--index") && i + 1 < argc)  index = argv[++i];
        else if (!strcmp(argv[i], "--index-url") && i + 1 < argc) index_url = argv[++i];
        else if (!strcmp(argv[i], "--interrupt-at") && i + 1 < argc)
            stop = (pkg_stop)atoi(argv[++i]);
        else if (!strcmp(argv[i], "--fail-at") && i + 1 < argc) {
            const char *w = argv[++i];
            pkg_test_fault(!strcmp(w, "commit") ? PKG_FAIL_COMMIT :
                           !strcmp(w, "doctor") ? PKG_FAIL_DOCTOR : PKG_FAIL_NONE);
        }
        else if (!strcmp(argv[i], "--break-recovery") && i + 1 < argc)
            pkg_test_break_recovery(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--verify-name") && i + 1 < argc)
            pkg_set_verify_name(argv[++i]);
        else if (!strcmp(argv[i], "--all-abi")) pkg_set_abi_show_all(1);
        else if (!strcmp(argv[i], "--json")) json = 1;
        else if (!strcmp(argv[i], "--verbose")) pkg_set_verbose(1);
        else if (!strcmp(argv[i], "--plain")) plain = 1;
        else if (!strcmp(argv[i], "--color=auto")) color = PR_COLOR_AUTO;
        else if (!strcmp(argv[i], "--color=always")) color = PR_COLOR_ALWAYS;
        else if (!strcmp(argv[i], "--color=never")) color = PR_COLOR_NEVER;
        else if (!strcmp(argv[i], "--dry-run")) dry = 1;
        else if (!strcmp(argv[i], "--fetch")) fetch = 1;
        else if (!strcmp(argv[i], "--progress")) show_progress = 1;
        else if (!strcmp(argv[i], "--slow") && i + 1 < argc) pkg_test_slow(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--check")) check = 1;
        else if (!strcmp(argv[i], "--self-update-from") && i + 1 < argc) su_test_from(argv[++i]);
        else if (!strcmp(argv[i], "--self-update-sums") && i + 1 < argc) su_test_sums(argv[++i]);
        else if (!strcmp(argv[i], "--self-update-fail-at") && i + 1 < argc) su_test_fail(argv[++i]);
        else if (!strcmp(argv[i], "--break-download-at") && i + 1 < argc)
            pkg_test_break_download(atol(argv[++i]), 1);
        else if (!strcmp(argv[i], "--corrupt-download-at") && i + 1 < argc)
            pkg_test_break_download(atol(argv[++i]), 2);
        else if (!strcmp(argv[i], "--cancel-at") && i + 1 < argc) cancel_at = argv[++i];
        else if (!strcmp(argv[i], "--abi") && i + 1 < argc)
            pkg_set_abi_override(argv[++i]);
        else if (!strcmp(argv[i], "--retry")) retry = 1;
        else if (!strcmp(argv[i], "--expect") && i + 1 < argc)   expect = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--report") && i + 1 < argc)  report = argv[++i];
        else if (!strcmp(argv[i], "--run-id") && i + 1 < argc)   run_id = argv[++i];
        else if (!strcmp(argv[i], "--machine") && i + 1 < argc)  machine = argv[++i];
        else if (!strcmp(argv[i], "--version")) {
            {
                char self[65];
                pkg_self_sha256(self);
                printf("%s\n  abi     %s\n  binary  %s\n  source  %s\n"
                       "  cc      %s\n  tc      %s\n  sdk     %s\n  tls     %s\n",
                       VERSION, pkg_abi(), self, pkg_build_id(), pkg_cc(),
                       pkg_toolchain(), pkg_sdk(), tls_name());
            }
            return 0;
        }
        else if (!strcmp(argv[i], "--about")) { printf("%s\n\n%s\n", VERSION, AROSPKG_ABOUT); return 0; }
        else if (!strcmp(argv[i], "--help-all")) { usage_all(); return 0; }
        else if (!strcmp(argv[i], "--help-testing")) { usage_testing(); return 0; }
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!cmd) cmd = argv[i];
        else if (!arg) arg = argv[i];
        else if (!arg2) arg2 = argv[i];
    }
    if (!cmd) { usage(); return 5; }
    /* Once, before the first line this command prints. */
    pr_init(json, plain, color);

    if (!index) {
        snprintf(indexbuf, sizeof indexbuf, "%s/db/index.json", root);
        index = indexbuf;
    }

    if (!strcmp(cmd, "assert-hash")) {
        /* A test passes when the end state is what was expected, not when it
           produced a report. This is how the harness checks that a file it
           had no right to touch is still byte-for-byte what it was. */
        char got[65];
        if (!arg || !arg2) { printf("FAIL assert-hash: need <file> <sha256>\n"); return 5; }
        if (sha256_file(arg, got) != 0) {
            printf("FAIL assert-hash %s: cannot read it\n", arg);
            return 10;
        }
        if (strcmp(got, arg2) != 0) {
            printf("FAIL assert-hash %s\n  want %s\n  got  %s\n", arg, arg2, got);
            return 10;
        }
        printf("PASS assert-hash %s\n", arg);
        return 0;
    }

    /* It replaces this program's file and nothing in the package root, so
       it never opens the root: no recovery, no lock of its own. */
    if (!strcmp(cmd, "self-update")) return apkg_self_update(VERSION + 5, root, check);

    if (!strcmp(cmd, "unlock")) {
        st = pkg_unlock(root, &e);
        if (st != PKG_OK) { show(&e); return 10; }
        printf("lock cleared. Make sure no other apkg or PkgManager is running.\n");
        return 0;
    }

    {
        pkg_mode mode = PKG_OPEN_WRITE;
        if (!strcmp(cmd, "list") || !strcmp(cmd, "info") || !strcmp(cmd, "show") ||
            !strcmp(cmd, "search") || !strcmp(cmd, "requires") ||
            !strcmp(cmd, "verify") || !strcmp(cmd, "open") || dry) mode = PKG_OPEN_READ;   /* a dry run never recovers */
        else if (!strcmp(cmd, "doctor")) mode = retry ? PKG_OPEN_DOCTOR : PKG_OPEN_READ;
        st = pkg_open(root, mode, &c, &e);
    }
    if (st != PKG_OK) {
        show(&e);
        /* Report before leaving. A run that could not even open the root --
           a held lock, a stale lock, recovery that stopped short -- is
           precisely the one a test needs recorded; the first stabilisation
           run lost b1.json this way and the gap was only visible because a
           report was missing from a directory listing. */
        write_report(report, run_id, machine, cmd, arg, root, stop, st, &e);
        return 10;
    }

    if (show_progress || cancel_at || pr_wants_progress()) pkg_set_progress(c, cli_progress, NULL);

    {
        int r = 0, u = 0, pending;
        pkg_recovery_counts(c, &r, &u);
        if (r) printf("recovered %d interrupted transaction(s)\n", r);
        pending = pkg_pending_txns(c);
        if (pending)
            printf("WARNING: %d uncommitted transaction(s) exist. What follows is\n"
                   "         what is on disk, not a settled state. A mutating\n"
                   "         command will recover them.\n", pending);
    }

    if (!strcmp(cmd, "list") && json) {
        pkg_entries es;
        st = pkg_installed(c, index, &es, &e);
        if (st == PKG_OK) print_rows(&es, 0);
        pkg_entries_free(&es);
    } else if (!strcmp(cmd, "list")) {
        char *out = NULL;
        st = pkg_list(c, &out, &e);
        if (st == PKG_OK) { if (out && *out) fputs(out, stdout); else printf("nothing installed\n"); }
        free(out);
    } else if (!strcmp(cmd, "show")) {
        if (!arg) { printf("apkg show: which package?\n"); pkg_close(c); return 5; }
        st = show_package(c, index, arg, &e);
    } else if (!strcmp(cmd, "info")) {
        char *out = NULL;
        if (!arg) { printf("apkg info: which package?\n"); pkg_close(c); return 5; }
        st = pkg_info(c, arg, &out, &e);
        if (st == PKG_OK) fputs(out, stdout);
        free(out);
    } else if (!strcmp(cmd, "update")) {
        snprintf(doing, sizeof doing, "update the package catalogue");
        cancel_noun = "Catalogue update";
        if (!json) pr_heading("Updating the package catalogue");
        st = pkg_update(c, index_url, &e);
        if (st == PKG_OK && !json) {
            pkg_entries es; int hidden = 0; pkg_err e2; char line[120];
            memset(&e2, 0, sizeof e2);
            putchar('\n');
            pr_word(PR_GREEN, "Updated");
            if (pkg_query(c, index, NULL, &es, &hidden, &e2) == PKG_OK) {
                printf(" the package catalogue: %d packages for this system\n", es.n);
                if (hidden) {
                    snprintf(line, sizeof line, "%d more are for other systems (apkg --all-abi search)", hidden);
                    pr_text(line, 2);
                }
                pkg_entries_free(&es);
            } else printf(" the package catalogue\n");
        }
    } else if (!strcmp(cmd, "search") && json) {
        pkg_entries es; int hidden = 0;
        st = pkg_query(c, index, arg, &es, &hidden, &e);
        if (st == PKG_OK) print_rows(&es, hidden);
        pkg_entries_free(&es);
    } else if (!strcmp(cmd, "search") && pr_rich()) {
        pkg_entries es; int hidden = 0;
        st = pkg_query(c, index, arg, &es, &hidden, &e);
        if (st == PKG_OK) search_table(&es, hidden);
        pkg_entries_free(&es);
    } else if (!strcmp(cmd, "search")) {
        char *out = NULL;
        st = pkg_search(c, index, arg, &out, &e);
        if (st == PKG_OK) fputs(out, stdout);
        free(out);
    } else if (dry && (!strcmp(cmd, "install") || !strcmp(cmd, "remove") ||
                       !strcmp(cmd, "upgrade") || !strcmp(cmd, "rollback"))) {
        char *out = NULL;
        if (!arg) { printf("apkg %s: which package?\n", cmd); pkg_close(c); return 5; }
        st = pkg_dry_run(c, index, cmd, arg, fetch, &out, &e);
        if (st == PKG_OK) fputs(out, stdout);
        free(out);
    } else if (!strcmp(cmd, "verify")) {
        pkg_verify_result v; char *listing = NULL;
        if (!arg) { printf("apkg verify: which package?\n"); pkg_close(c); return 5; }
        st = pkg_verify(c, arg, &v, &listing, &e);
        if (st == PKG_OK) {
            if (json)
                printf("{ \"id\": \"%s\", \"ok\": %d, \"changed\": %d, \"missing\": %d, \"unknown\": %d, \"icon\": \"%s\" }\n",
                       arg, v.ok, v.changed, v.missing, v.unknown, v.icon);
            else {
                printf("%s: %d as installed, %d changed locally, %d missing, %d not the package's; drawer icon %s\n",
                       arg, v.ok, v.changed, v.missing, v.unknown, v.icon);
                if (listing && *listing) fputs(listing, stdout);
            }
        }
        free(listing);
    } else if (!strcmp(cmd, "requires")) {
        char out[2048];
        if (!arg) { printf("apkg requires: which package?\n"); pkg_close(c); return 5; }
        st = pkg_requirements(c, index, arg, out, sizeof out, &e);
        if (st == PKG_OK) printf("%s\n", out[0] ? out : "no system requirements");
    } else if (!strcmp(cmd, "doctor")) {
        char *out = NULL;
        st = pkg_doctor(c, &out, &e);
        if (st == PKG_OK) fputs(out, stdout);
        free(out);
    } else if (!strcmp(cmd, "install")) {
        char v[80];
        if (!arg) { printf("apkg install: which package?\n"); pkg_close(c); return 5; }
        snprintf(doing, sizeof doing, "install %s", arg);
        doing_id = arg;
        cancel_noun = "Installation";
        offered(c, index, arg, v, sizeof v, 0);
        heading2("Installing", arg, v);
        st = pkg_install(c, index, arg, stop, &e);
        pr_end_line();
        if (st == PKG_OK) print_installed(c, index, arg, "Installed");
    } else if (!strcmp(cmd, "remove")) {
        char v[80];
        pkg_removal rm;
        if (!arg) { printf("apkg remove: which package?\n"); pkg_close(c); return 5; }
        snprintf(doing, sizeof doing, "remove %s", arg);
        doing_id = arg;
        cancel_noun = "Removal";
        offered(c, index, arg, v, sizeof v, 1);
        heading2("Removing", arg, v);
        st = pkg_remove(c, arg, stop, &e);
        pr_end_line();
        if (st == PKG_OK) {
            int i;
            char line[PKG_MAXPATH + 120];
            pkg_last_removal(c, &rm);
            putchar('\n');
            pr_word(PR_GREEN, "Removed"); printf(" %s%s%s\n", arg, *v ? " " : "", v);
            /* What is still there is said as such: a user who reads only
               "Removed" would think it gone. */
            /* What is still there, file by file with the reason: what the
               user changed, and what apkg never installed. Only names apkg
               knows are listed; a longer list says how many more. */
            if (rm.kept || rm.others) {
                snprintf(line, sizeof line, "Kept files in %s:", rm.dir[0] ? rm.dir : "the drawer");
                pr_text(line, 2);
                for (i = 0; i < rm.named; i++) {
                    snprintf(line, sizeof line, "%s (you changed it)", rm.names[i]);
                    pr_text(line, 4);
                }
                if (rm.kept > rm.named) {
                    snprintf(line, sizeof line, "and %d more you changed, listed in %s", rm.kept - rm.named, rm.list_file);
                    pr_text(line, 4);
                }
                for (i = 0; i < rm.others_named; i++) {
                    snprintf(line, sizeof line, "%s (not installed by apkg)", rm.other_names[i]);
                    pr_text(line, 4);
                }
                if (rm.others > rm.others_named) {
                    snprintf(line, sizeof line, "and %d more not installed by apkg", rm.others - rm.others_named);
                    pr_text(line, 4);
                }
            } else if (rm.dir[0]) {
                snprintf(line, sizeof line, "The drawer %s was kept; it is not empty.", rm.dir);
                pr_text(line, 2);
            }
            if (rm.icon[0]) {
                snprintf(line, sizeof line, "Kept the drawer icon %s: it was changed after installation", rm.icon);
                pr_text(line, 2);
            }
        }
    } else if (!strcmp(cmd, "upgrade")) {
        if (!arg) { printf("apkg upgrade: which package?\n"); pkg_close(c); return 5; }
        snprintf(doing, sizeof doing, "upgrade %s", arg);
        doing_id = arg;
        cancel_noun = "Upgrade";
        st = pkg_upgrade(c, index, arg, stop, &e);
        pr_end_line();
        if (st == PKG_OK) print_installed(c, index, arg, "Upgraded");
    } else if (!strcmp(cmd, "rollback")) {
        if (!arg) { printf("apkg rollback: which package?\n"); pkg_close(c); return 5; }
        snprintf(doing, sizeof doing, "roll back %s", arg);
        doing_id = arg;
        cancel_noun = "Rollback";
        st = pkg_rollback(c, arg, stop, &e);
        pr_end_line();
        if (st == PKG_OK) print_installed(c, index, arg, "Rolled back");
    } else if (!strcmp(cmd, "open")) {
        char where[PKG_MAXPATH];
        if (!arg) { printf("apkg open: which package?\n"); pkg_close(c); return 5; }
        snprintf(doing, sizeof doing, "open %s", arg);
        doing_id = arg;
        cancel_noun = "Opening";
        st = pkg_open_folder(c, arg, where, sizeof where, &e);
        if (st == PKG_OK) printf("Opened %s\n", where);
    } else {
        printf("apkg: unknown command '%s'\n", cmd);
        pkg_close(c);
        return 5;
    }

    if (st != PKG_OK) show(&e);

    write_report(report, run_id, machine, cmd, arg, root, stop, st, &e);

    if (expect >= 0) {
        if ((int)st == expect) {
            printf("PASS %s: %s as expected\n", run_id ? run_id : cmd, pkg_strstatus(st));
        } else {
            printf("FAIL %s: expected %s, got %s\n", run_id ? run_id : cmd,
                   pkg_strstatus((pkg_status)expect), pkg_strstatus(st));
            pkg_close(c);
            return 20;
        }
    }

    pkg_close(c);
    return st == PKG_OK ? 0 : 10;
}

int main(int argc, char **argv)
{
    return pkg_run_with_stack(real_main, argc, argv);
}
