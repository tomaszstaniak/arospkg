/* apkg -- the command line front end. Presentation only: every decision is in
 * libpkg, so that PkgManager can make the same ones without reimplementing
 * them. Errors are printed as libpkg wrote them rather than re-phrased here. */

#include "../libpkg/pkg.h"
#include "../libpkg/tls.h"
#include "../libpkg/sha256.h"
#include "../libpkg/verify.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *VERSION = "apkg 0.3";

/* `show`: the library's account of one package, laid out for a person. The
 * facts come from pkg_details_get and the probes behind the window's panel;
 * nothing is decided here. */
static pkg_status show_package(pkg_ctx *c, const char *index, const char *id, pkg_err *e)
{
    pkg_details d; pkg_status st; char a[80], b[80], why[240];
    st = pkg_details_get(c, index, id, &d, e);
    if (st != PKG_OK) return st;
    rev_label(a, sizeof a, d.e.version, d.e.revision);
    printf("%s %s\n", d.e.id, a);
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
    printf("  compatibility: %s -- %s\n", pkg_compat_word(d.compat), d.compat_why);
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
                printf("  requirements: %s, probed now:\n", d.requirements);
                while (line && *line) {
                    nl = strchr(line, '\n'); if (nl) *nl = 0;
                    printf("    %s\n", line);
                    line = nl ? nl + 1 : NULL;
                }
            } else printf("  requirements: %s (%s)\n", d.e.requires, e2.summary);
        }
    }
    if (!d.e.installed) printf("  installed:    no\n");
    else {
        rev_label(b, sizeof b, d.e.installed_version, d.e.installed_revision);
        printf("  installed:    %s%s%s%s, in %s%s%s\n", b,
               d.installed_arch[0] ? " (" : "", d.installed_arch, d.installed_arch[0] ? ")" : "",
               d.installed_dir, d.installed_when[0] ? ", since " : "", d.installed_when);
        if (d.in_index) {
            if (pkg_can_upgrade(&d.e, why, sizeof why)) printf("  upgrade:      available, %s -> %s\n", b, a);
            else printf("  upgrade:      no -- %s\n", why);
        }
        if (pkg_can_rollback(c, id, why, sizeof why)) printf("  roll back:    possible\n");
        else printf("  roll back:    no -- %s\n", why);
    }
    return PKG_OK;
}

static void usage(void)
{
    printf("%s\n\n", VERSION);
    printf("usage: apkg [options] <command>\n\n");
    printf("  update                download the index\n");
    printf("  search [term]         what the index offers\n");
    printf("  list                  what is installed\n");
    printf("  show <id>             what a package is, whether it runs here, what it\n");
    printf("                        needs and what is installed; changes nothing\n");
    printf("  info <id>             the registry entry for one package\n");
    printf("  install <id>          install from the index, archive from cache/\n");
    printf("  remove <id>           uninstall, keeping anything modified\n");
    printf("  upgrade <id>          to the index's newer revision of the same\n");
    printf("                        version; prints the plan, refuses on conflict\n");
    printf("  rollback <id>         back to the revision the last upgrade replaced\n");
    printf("  unlock                clear a lock left by a dead run\n");
    printf("  doctor [--retry]      say what is unresolved and why; --retry\n");
    printf("                        re-runs recovery. There is no force.\n");
    printf("  assert-hash <f> <sha> PASS if the file hashes to sha, else FAIL\n\n");
    printf("  --root <dir>          package root (default SYS:Packages)\n");
    printf("  --index <file>        index.json (default <root>/db/index.json)\n");
    printf("  --index-url <url>     where update fetches it from\n");
    printf("  --verify-name <name>  require the certificate to match this\n");
    printf("                        instead of the host; must fail, for testing\n");
    printf("  --json                search and list print rows as JSON, for\n");
    printf("                        programs rather than people\n");
    printf("  --all-abi             also list packages for the other ABI\n");
    printf("  --abi <v0|v1|v11>     install one anyway. It will not start;\n");
    printf("                        this is for diagnosis, not for use\n");
    printf("  --expect <status>     PASS only if the run ends with this status;\n");
    printf("                        a test that merely produced a report has\n");
    printf("                        not shown it reached what it was testing\n");
    printf("  --report <file>       write a machine-readable run report\n");
    printf("  --run-id <s>          label that report; defaults to a timestamp\n");
    printf("  --machine <s>         which guest this ran on, recorded verbatim\n");
    printf("  --version             print the build id and exit\n");
    printf("  --fail-at commit|doctor  make that write fail, for testing\n");
    printf("  verify <id>           the installed files against the registry: as\n");
    printf("                        installed, changed locally, missing, not ours\n");
    printf("  --dry-run             with install/remove/upgrade/rollback: the plan\n");
    printf("                        only; nothing changed, nothing recovered\n");
    printf("  --fetch               with --dry-run: may download into the cache\n");
    printf("  requires <id>         each system requirement, probed here and now\n");
    printf("  --slow <ms>           slow the download and the extract, for testing\n");
    printf("  --progress            print each progress callback, for testing\n");
    printf("  --cancel-at <phase>   cancel from the callback in that phase, for\n");
    printf("                        testing: download, download-mid (once bytes\n");
    printf("                        flow), verify, extract, publish\n");
    printf("  --break-recovery <n>  abort recovery after n files, for testing\n");
    printf("  --interrupt-at <n>    stop mid-operation on purpose, for testing:\n");
    printf("                        1 after plan, 2 after dir, 3 after dir marker,\n");
    printf("                        4 after registry, 5 before commit,\n");
    printf("                        6 after remove deletes files,\n");
    printf("                        7 inside publish, between delete and rename,\n");
    printf("                        8 after the drawer icon is in place,\n");
    printf("                        9 upgrade: old tree moved aside,\n");
    printf("                        10 upgrade: new tree in place\n");
    printf("\nAn interrupted run leaves the machine in a state the next run must\n");
    printf("recover. That is the point of the flag.\n");
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

static void show(const pkg_err *e)
{
    printf("apkg: %s\n", e->summary);
    if (e->subject[0]) printf("     %s\n", e->subject);
    if (e->detail[0])  printf("     %s\n", e->detail);
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
    int expect = -1, retry = 0, json = 0, dry = 0, fetch = 0;
    pkg_ctx *c = NULL;
    pkg_err e;
    pkg_status st;

    memset(&e, 0, sizeof e);

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--root") && i + 1 < argc)        root = argv[++i];
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
        else if (!strcmp(argv[i], "--dry-run")) dry = 1;
        else if (!strcmp(argv[i], "--fetch")) fetch = 1;
        else if (!strcmp(argv[i], "--progress")) show_progress = 1;
        else if (!strcmp(argv[i], "--slow") && i + 1 < argc) pkg_test_slow(atoi(argv[++i]));
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
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) { usage(); return 0; }
        else if (!cmd) cmd = argv[i];
        else if (!arg) arg = argv[i];
        else if (!arg2) arg2 = argv[i];
    }
    if (!cmd) { usage(); return 5; }

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
            !strcmp(cmd, "verify") || dry) mode = PKG_OPEN_READ;   /* a dry run never recovers */
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

    if (show_progress || cancel_at) pkg_set_progress(c, cli_progress, NULL);

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
        st = pkg_update(c, index_url, &e);
    } else if (!strcmp(cmd, "search") && json) {
        pkg_entries es; int hidden = 0;
        st = pkg_query(c, index, arg, &es, &hidden, &e);
        if (st == PKG_OK) print_rows(&es, hidden);
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
        if (!arg) { printf("apkg install: which package?\n"); pkg_close(c); return 5; }
        st = pkg_install(c, index, arg, stop, &e);
        if (st == PKG_OK) printf("installed %s\n", arg);
    } else if (!strcmp(cmd, "remove")) {
        if (!arg) { printf("apkg remove: which package?\n"); pkg_close(c); return 5; }
        st = pkg_remove(c, arg, stop, &e);
        if (st == PKG_OK) printf("removed %s\n", arg);
    } else if (!strcmp(cmd, "upgrade")) {
        if (!arg) { printf("apkg upgrade: which package?\n"); pkg_close(c); return 5; }
        st = pkg_upgrade(c, index, arg, stop, &e);
        if (st == PKG_OK) printf("upgraded %s\n", arg);
    } else if (!strcmp(cmd, "rollback")) {
        if (!arg) { printf("apkg rollback: which package?\n"); pkg_close(c); return 5; }
        st = pkg_rollback(c, arg, stop, &e);
        if (st == PKG_OK) printf("rolled back %s\n", arg);
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
