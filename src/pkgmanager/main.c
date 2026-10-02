/* PkgManager -- the Zune front end. Presentation only, as pkg.h says of every
 * front: the rows come from pkg_query/pkg_installed, the work is done by a
 * worker process through libpkg, and the only decisions here are which
 * button is enabled. Pattern after ~/Work/AROS/src/hello2.c. */
#include "job.h"
#include "rexx.h"
#include "rexxcmd.h"
#include "../libpkg/verify.h"
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <libraries/mui.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#define PM_VERSION "0.3.1-rc1"

enum { ID_SEARCH = 1, ID_INSTALL, ID_REMOVE, ID_UPDATE, ID_CANCEL, ID_SELECT, ID_FILTER,
       ID_UPGRADE, ID_ROLLBACK, ID_PROCEED, ID_DECLINE };

static Object *app, *win, *str_search, *cyc_cat, *cyc_state, *lst, *txt_detail,
              *txt_status, *gauge, *btn_install, *btn_remove, *btn_update, *btn_cancel,
              *btn_upgrade, *btn_rollback, *win_confirm, *txt_confirm, *btn_proceed, *btn_decline;
static int auto_yes;              /* --yes: confirm requesters without asking; for tests */
static pkg_entries rows;          /* the whole query; the list shows a filtered view */
static job cur;
static struct MsgPort *port;      /* the worker's messages arrive here */
static char root[PKG_MAXPATH] = "SYS:Packages";
static char index_path[PKG_MAXPATH];
static const char *index_url =
    "https://raw.githubusercontent.com/tomaszstaniak/arospkg-index/main/index.json";
static int quit_pending;          /* close asked for while a job ran */
static char last_phase[16]; static int last_can_cancel;
static int confirming;            /* a plan is shown and the window waits for the user */
static int rexx_quit;             /* QUIT arrived with nothing running */

/* Every operation gets a record and a number, whether the window or a
 * script started it, so a script can follow either. `active` is the one the
 * worker or the confirmation belongs to; the rest are finished. */
#define MAXJOBS 32
static rx_job jobs[MAXJOBS];
static long next_job = 1;
static rx_job *active;

/* WAIT holds the script's message until the job is final. */
#define MAXWAITS 16
static struct { rexx_msg *m; long job; char field[24]; } waits[MAXWAITS];

/* The window's own "a job is out": set when the worker starts, cleared only
 * when its PM_DONE has been handled. Not cur.running, which the worker
 * clears before that message is even sent: in between, a new job could be
 * started and the old outcome would be read as the new job's. */
static int in_flight;
static int busy_now(void) { return in_flight || confirming; }

/* --log <file>: every event the window acted on, one line each, for a run
 * that has to prove what the window did rather than show a screenshot of
 * it. A test aid in the spirit of apkg's --progress; off unless asked. */
static FILE *logf;
static void logline(const char *fmt, ...)
{
    va_list ap;
    if (!logf) return;
    va_start(ap, fmt); vfprintf(logf, fmt, ap); va_end(ap);
    fputc('\n', logf); fflush(logf);
}

/* ------------------------------------------------------------ list display */

AROS_UFH3S(void, display_func,
    AROS_UFHA(struct Hook *, hook,  A0),
    AROS_UFHA(char **,       array, A2),
    AROS_UFHA(pkg_entry *,   e,     A1))
{
    AROS_USERFUNC_INIT
    (void)hook;
    if (!e) {
        array[0] = (char *)"\33bPackage"; array[1] = (char *)"\33bVersion";
        array[2] = (char *)"\33bState";   array[3] = (char *)"\33bSummary";
    } else {
        array[0] = e->id; array[1] = e->version;
        array[2] = (char *)(e->installed ? "installed" : (e->ours ? "" : "other ABI"));
        array[3] = e->summary;
    }
    AROS_USERFUNC_EXIT
}
static struct Hook display_hook;

static void status(const char *text)
{
    SetAttrs(txt_status, MUIA_Text_Contents, (IPTR)text, TAG_DONE);
}

static void show_err(const char *what, pkg_status st, const pkg_err *e)
{
    static char buf[640];
    /* As libpkg wrote it; the front does not re-phrase. On one line, because
       the status is a Text object of one line's height. */
    if (st == PKG_OK) snprintf(buf, sizeof buf, "%s", what);
    else snprintf(buf, sizeof buf, "%s: %s%s%s%s%s%s", what, e->summary,
                  e->subject[0] ? " (" : "", e->subject, e->subject[0] ? ")" : "",
                  e->detail[0] ? " -- " : "", e->detail);
    status(buf);
}

/* ---------------------------------------------------------------- filters */

/* The category cycle is built from what the index holds -- the first path
 * component of each category -- so it never offers a choice with nothing in
 * it. Rebuilt on every refill; the chosen name is kept across rebuilds. */
#define MAXCAT 32
static char  cat_names[MAXCAT][32];
static STRPTR cat_entries[MAXCAT + 2];
static char  cat_chosen[32];
static STRPTR state_entries[] = { (STRPTR)"All", (STRPTR)"Installed", (STRPTR)"Not installed", NULL };

static void top_category(const char *cat, char *out, size_t n)
{
    size_t k = strcspn(cat, "/");
    if (k >= n) k = n - 1;
    memcpy(out, cat, k); out[k] = 0;
    if (!out[0]) snprintf(out, n, "%s", "(none)");
}

static void rebuild_categories(const pkg_entries *all)
{
    int i, n = 0, active = 0;
    for (i = 0; i < all->n && n < MAXCAT; i++) {
        char top[32]; int j, seen = 0;
        top_category(all->v[i].category, top, sizeof top);
        for (j = 0; j < n; j++) if (strcmp(cat_names[j], top) == 0) { seen = 1; break; }
        if (!seen) snprintf(cat_names[n++], sizeof cat_names[0], "%s", top);
    }
    /* Alphabetical, so the cycle reads the same whatever the index order. */
    for (i = 1; i < n; i++) {
        char tmp[32]; int j = i;
        snprintf(tmp, sizeof tmp, "%s", cat_names[i]);
        while (j > 0 && strcmp(cat_names[j - 1], tmp) > 0) { memcpy(cat_names[j], cat_names[j - 1], sizeof tmp); j--; }
        memcpy(cat_names[j], tmp, sizeof tmp);
    }
    cat_entries[0] = (STRPTR)"All categories";
    for (i = 0; i < n; i++) {
        cat_entries[i + 1] = (STRPTR)cat_names[i];
        if (strcmp(cat_names[i], cat_chosen) == 0) active = i + 1;
    }
    cat_entries[n + 1] = NULL;
    /* MUIA_NoNotify, because setting the active entry here would otherwise
       fire the same notification a user's choice does, which calls refill,
       which comes back here: measured as a few hundred "filter" events per
       second until the window was closed. The user's choice is the only
       one that should refill. */
    SetAttrs(cyc_cat, MUIA_NoNotify, TRUE, MUIA_Cycle_Entries, (IPTR)cat_entries,
             MUIA_Cycle_Active, active, TAG_DONE);
    if (!active) cat_chosen[0] = 0;
}

static int passes_filters(const pkg_entry *e)
{
    IPTR st = 0;
    if (cat_chosen[0]) {
        char top[32];
        top_category(e->category, top, sizeof top);
        if (strcmp(top, cat_chosen) != 0) return 0;
    }
    GetAttr(MUIA_Cycle_Active, cyc_state, &st);
    if (st == 1 && !e->installed) return 0;
    if (st == 2 &&  e->installed) return 0;
    return 1;
}

/* ------------------------------------------------------------------ rows */

static void show_detail(void);

/* Re-read the index and the registry, rebuild the category cycle, and show
 * the rows that pass the filters. The old rows stay alive until the list
 * has let go of them. Four outcomes are told apart on purpose: no index yet,
 * an index that cannot be read, an index with nothing in it, and filters
 * that leave nothing -- a window that says "0 packages" for all four sends
 * the user to the wrong fix. */
static void refill(const char *term, const char *keep_id, const char *why)
{
    pkg_ctx *c = NULL; pkg_err e; pkg_status st;
    pkg_entries fresh, all = {0}; int hidden = 0, i, again = -1, shown = 0, inst = 0;
    char msg[200];

    memset(&e, 0, sizeof e);
    st = pkg_open(root, PKG_OPEN_READ, &c, &e);
    if (st != PKG_OK) { show_err("cannot open the package root", st, &e); return; }
    st = pkg_query(c, index_path, term, &fresh, &hidden, &e);
    /* The category cycle lists what the whole index holds, not what the
       current search left: a search with no hits used to empty the cycle
       and forget the chosen category with it. */
    if (st == PKG_OK && term && *term) {
        pkg_err e2; memset(&e2, 0, sizeof e2);
        if (pkg_query(c, index_path, NULL, &all, NULL, &e2) != PKG_OK) all.n = 0;
    }
    pkg_close(c);

    SetAttrs(lst, MUIA_List_Quiet, TRUE, TAG_DONE);
    DoMethod(lst, MUIM_List_Clear);
    pkg_entries_free(&rows);
    if (st == PKG_OK) {
        rows = fresh;
        rebuild_categories(all.n ? &all : &rows);
        pkg_entries_free(&all);
        for (i = 0; i < rows.n; i++) {
            if (rows.v[i].installed) inst++;
            if (!passes_filters(&rows.v[i])) continue;
            DoMethod(lst, MUIM_List_InsertSingle, (IPTR)&rows.v[i], MUIV_List_Insert_Bottom);
            if (keep_id && strcmp(rows.v[i].id, keep_id) == 0) again = shown;
            shown++;
        }
        logline("refill (%s) term=\"%s\" rows=%d shown=%d installed=%d keep=%s",
                why, term ? term : "", rows.n, shown, inst, keep_id ? keep_id : "-");
        if (rows.n == 0 && hidden == 0 && !(term && *term))
            snprintf(msg, sizeof msg, "the index lists no packages");
        else if (shown == 0)
            snprintf(msg, sizeof msg, "no packages match%s%s%s",
                     term && *term ? " \"" : "", term && *term ? term : "", term && *term ? "\"" : "");
        else
            snprintf(msg, sizeof msg, "%d package%s%s%s%s", shown, shown == 1 ? "" : "s",
                     shown < rows.n ? " shown" : "",
                     term && *term ? " matching " : "", term && *term ? term : "");
        if (hidden) {
            size_t n = strlen(msg);
            snprintf(msg + n, sizeof msg - n, "; %d hidden (not for this machine)", hidden);
        }
        status(msg);
    } else if (st == PKG_E_NOT_FOUND) {
        logline("refill (%s): no index", why);
        status("no index yet -- press Update index");
    } else {
        logline("refill (%s): index unreadable: %s", why, e.summary);
        show_err("the index cannot be read", st, &e);
    }
    SetAttrs(lst, MUIA_List_Quiet, FALSE, TAG_DONE);
    /* The list forgets its selection when cleared; the user did not. */
    if (again >= 0) SetAttrs(lst, MUIA_List_Active, again, TAG_DONE);
    show_detail();
}

static pkg_entry *selected(void)
{
    IPTR act = (IPTR)MUIV_List_Active_Off; pkg_entry *e = NULL;
    GetAttr(MUIA_List_Active, lst, &act);
    if ((LONG)act == MUIV_List_Active_Off) return NULL;
    DoMethod(lst, MUIM_List_GetEntry, act, (IPTR)&e);
    return e;
}

/* Refill with the current search term, keeping the current selection. */
static void refill_keep(const char *why)
{
    IPTR t = 0; pkg_entry *e = selected(); char keep[PKG_MAXID] = "";
    if (e) snprintf(keep, sizeof keep, "%s", e->id);
    GetAttr(MUIA_String_Contents, str_search, &t);
    refill((const char *)t, keep[0] ? keep : NULL, why);
}

/* The string gadget acknowledges on Return and, in Zune, when it loses
 * focus to a click elsewhere. Only a changed term is a search; the rest
 * would refill for nothing and overwrite whatever the status line said. */
static char last_term[256];
static void search_acknowledged(void)
{
    IPTR t = 0;
    GetAttr(MUIA_String_Contents, str_search, &t);
    if (strcmp((const char *)t, last_term) == 0) { logline("search acknowledged, unchanged"); return; }
    snprintf(last_term, sizeof last_term, "%s", (const char *)t);
    refill_keep("search");
}

/* The panel a person reads before pressing Install: what it is, what it
 * needs from this machine (probed now, not copied from the index), and how
 * much comes down the wire. */
static void show_detail(void)
{
    static char buf[2048];
    char reqs[1024] = "";
    pkg_entry *e = selected();
    pkg_details det;
    size_t k;

    if (!e) {
        SetAttrs(txt_detail, MUIA_Floattext_Text, (IPTR)"", TAG_DONE);
        SetAttrs(btn_install,  MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_remove,   MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_upgrade,  MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_rollback, MUIA_Disabled, TRUE, TAG_DONE);
        return;
    }
    memset(&det, 0, sizeof det);
    {
        /* Whether it runs here comes from the same call `apkg show` and
           ARexx INFO make, so the three cannot disagree. */
        pkg_ctx *c = NULL; pkg_err err; memset(&err, 0, sizeof err);
        if (pkg_open(root, PKG_OPEN_READ, &c, &err) == PKG_OK) {
            if (pkg_details_get(c, index_path, e->id, &det, &err) != PKG_OK) {
                det.compat = PKG_COMPAT_UNDETERMINED;
                snprintf(det.compat_why, sizeof det.compat_why, "%s", err.summary);
            }
            if (e->requires[0] && pkg_requirements(c, index_path, e->id, reqs, sizeof reqs, &err) != PKG_OK)
                snprintf(reqs, sizeof reqs, "%s", err.summary);
            pkg_close(c);
        }
    }
    {
        /* The revision is shown only when the index states one: "2.2" is
           upstream's string and must appear exactly as upstream wrote it. */
        /* The header names what is ON THIS MACHINE when the package is
           installed -- the registry's version and revision -- and the
           index's otherwise. Showing the index's revision for an installed
           package read as "2.2-aros2 installed" while revision 0 was what
           had been rolled back to. */
        char ver[80];
        rev_label(ver, sizeof ver, e->installed ? e->installed_version : e->version,
                  e->installed ? e->installed_revision : e->revision);
        k = (size_t)snprintf(buf, sizeof buf, "\33b%s\33n %s   %s\n",
                             e->id, ver,
                             e->installed ? "\33binstalled\33n" : (e->ours ? "" : "\33bother ABI\33n"));
    }
    {
        /* The two questions the buttons need, answered by the library. */
        char why[240]; int up = 0, back = 0;
        if (e->installed) {
            up = pkg_can_upgrade(e, why, sizeof why);
            if (up) {
                size_t n2 = strlen(buf);
                char a[80], b[80];
                rev_label(a, sizeof a, e->installed_version, e->installed_revision);
                rev_label(b, sizeof b, e->version, e->revision);
                snprintf(buf + n2, sizeof buf - n2, "\33bupgrade available:\33n %s -> %s\n", a, b);
            }
            {
                pkg_ctx *c = NULL; pkg_err err; memset(&err, 0, sizeof err);
                if (pkg_open(root, PKG_OPEN_READ, &c, &err) == PKG_OK) {
                    back = pkg_can_rollback(c, e->id, why, sizeof why);
                    pkg_close(c);
                }
                /* An upgrade happened but the way back is gone: say so where
                   the greyed button is, rather than leave it unexplained. */
                if (!back && strstr(why, "archive")) {
                    size_t n2 = strlen(buf);
                    snprintf(buf + n2, sizeof buf - n2, "\33brollback unavailable:\33n %s\n", why);
                }
            }
            {
                char a[80], b[80];
                rev_label(a, sizeof a, e->installed_version, e->installed_revision);
                rev_label(b, sizeof b, e->version, e->revision);
                logline("detail %s: installed %s, index %s, can_upgrade=%d can_rollback=%d%s%s",
                        e->id, a, b, up, back, (!up || !back) ? " why=" : "", (!up || !back) ? why : "");
            }
        }
        SetAttrs(btn_upgrade,  MUIA_Disabled, busy_now() || !up,   TAG_DONE);
        SetAttrs(btn_rollback, MUIA_Disabled, busy_now() || !back, TAG_DONE);
    }
    k = strlen(buf);
    k += (size_t)snprintf(buf + k, sizeof buf - k, "%s\n%s   %s/%s   %s   %ld KB download\n",
                          e->summary, e->category, e->arch, e->abi[0] ? e->abi : "?", e->kind,
                          e->size > 0 ? (e->size + 1023) / 1024 : 0L);
    if (det.compat == PKG_COMPAT_NATIVE)
        k += (size_t)snprintf(buf + k, sizeof buf - k, "compatibility: native\n");
    else
        k += (size_t)snprintf(buf + k, sizeof buf - k, "\33bcompatibility: %s\33n -- %s\n", pkg_compat_word(det.compat), det.compat_why);
    if (det.compat != PKG_COMPAT_NATIVE)
        snprintf(buf + k, sizeof buf - k, "requirements: not checked");
    else if (e->requires[0])
        snprintf(buf + k, sizeof buf - k, "requirements: %s, probed now:\n%s", det.requirements, reqs[0] ? reqs : e->requires);
    else
        snprintf(buf + k, sizeof buf - k, "requirements: none stated");
    SetAttrs(txt_detail, MUIA_Floattext_Text, (IPTR)buf, TAG_DONE);
    SetAttrs(btn_install, MUIA_Disabled, busy_now() || e->installed || !e->ours, TAG_DONE);
    SetAttrs(btn_remove,  MUIA_Disabled, busy_now() || !e->installed, TAG_DONE);
}

/* ------------------------------------------------------------------ jobs */

static void busy(int on)
{
    SetAttrs(btn_update,  MUIA_Disabled, on, TAG_DONE);
    SetAttrs(btn_cancel,  MUIA_Disabled, TRUE, TAG_DONE);   /* until an event says otherwise */
    SetAttrs(gauge, MUIA_Gauge_Current, 0, MUIA_Gauge_InfoText, (IPTR)(on ? "starting" : ""), TAG_DONE);
    if (on) {
        SetAttrs(btn_install,  MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_remove,   MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_upgrade,  MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(btn_rollback, MUIA_Disabled, TRUE, TAG_DONE);
    } else show_detail();
}

static const char *opname(job_kind k)
{
    switch (k) {
    case JOB_INSTALL: return "install";
    case JOB_REMOVE:  return "remove";
    case JOB_UPDATE:  return "update";
    case JOB_PREVIEW_UPGRADE: case JOB_UPGRADE: return "upgrade";
    case JOB_PREVIEW_ROLLBACK: case JOB_ROLLBACK: return "rollback";
    default: return "none";
    }
}

/* 0 means the latest. Only the last MAXJOBS are kept. */
static rx_job *find_job(long id)
{
    rx_job *j;
    if (id <= 0) id = next_job - 1;
    if (id <= 0 || id >= next_job || id < next_job - MAXJOBS) return NULL;
    j = &jobs[(id - 1) % MAXJOBS];
    return j->id == id ? j : NULL;
}

static void err_text(char *out, size_t n, const pkg_err *e)
{
    snprintf(out, n, "%s%s%s%s%s%s", e->summary,
             e->subject[0] ? " (" : "", e->subject, e->subject[0] ? ")" : "",
             e->detail[0] ? " -- " : "", e->detail);
}

static void reply_job(rexx_msg *m, const rx_job *j, const char *field);

/* A job becomes final exactly once, here; every script waiting on it is
 * answered with the outcome. */
static void finish(rx_job *j, rx_jobstate st, const char *error, const char *message)
{
    int i;
    j->state = st;
    j->can_cancel = 0;
    snprintf(j->error, sizeof j->error, "%s", error ? error : "");
    snprintf(j->message, sizeof j->message, "%s", message ? message : "");
    logline("job %ld %s %s %s%s%s%s%s", j->id, j->op, j->package[0] ? j->package : "-",
            rx_statename(st), j->error[0] ? " " : "", j->error, j->message[0] ? ": " : "", j->message);
    for (i = 0; i < MAXWAITS; i++)
        if (waits[i].m && waits[i].job == j->id) {
            rexx_msg *m = waits[i].m;
            waits[i].m = NULL;
            reply_job(m, j, waits[i].field);
        }
    if (j == active) active = NULL;
}

static int start(job_kind kind, const char *id)
{
    if (in_flight) return -1;
    memset(&cur, 0, sizeof cur);
    cur.kind = kind;
    cur.seq = active ? active->id : 0;
    snprintf(cur.id, sizeof cur.id, "%s", id ? id : "");
    snprintf(cur.root, sizeof cur.root, "%s", root);
    snprintf(cur.index, sizeof cur.index, "%s", index_path);
    snprintf(cur.index_url, sizeof cur.index_url, "%s", index_url);
    cur.reply = port;
    last_phase[0] = 0; last_can_cancel = 0;
    busy(1);
    status(kind == JOB_INSTALL ? "installing..." : kind == JOB_REMOVE ? "removing..." :
           kind == JOB_UPDATE ? "updating the index..." :
           kind == JOB_UPGRADE ? "upgrading..." : kind == JOB_ROLLBACK ? "rolling back..." :
           kind == JOB_PREVIEW_UPGRADE ? "working out what the upgrade would do..." :
           "working out what the rollback would do...");
    if (job_start(&cur) != 0) { busy(0); status("could not start the worker process"); return -1; }
    in_flight = 1;
    return 0;
}

/* Every operation starts here, from a button or from a script. */
static rx_job *begin(job_kind kind, const char *pkg, const char *origin)
{
    rx_job *j;
    if (busy_now()) return NULL;
    j = &jobs[(next_job - 1) % MAXJOBS];
    memset(j, 0, sizeof *j);
    j->id = next_job++;
    snprintf(j->op, sizeof j->op, "%s", opname(kind));
    snprintf(j->package, sizeof j->package, "%s", pkg ? pkg : "");
    snprintf(j->origin, sizeof j->origin, "%s", origin);
    j->state = (kind == JOB_PREVIEW_UPGRADE || kind == JOB_PREVIEW_ROLLBACK) ? RXJ_PREVIEW : RXJ_RUNNING;
    active = j;
    logline("job %ld %s %s from %s", j->id, j->op, j->package[0] ? j->package : "-", origin);
    if (start(kind, pkg) != 0) finish(j, RXJ_FAILED, "CANNOTSTART", "could not start the worker process");
    return j;
}

/* The plan is shown in a window of its own rather than a requester: a
 * requester runs its own loop until it is answered, and the ARexx port --
 * served from this loop -- would stop answering for as long as the user
 * reads. */
static void confirm_close(void)
{
    SetAttrs(win_confirm, MUIA_Window_Open, FALSE, TAG_DONE);
    confirming = 0;
}

static void proceed(const char *how)
{
    rx_job *j = active;
    if (!confirming || !j) return;
    confirm_close();
    logline("%s %s %s", j->op, j->package, how);
    j->state = RXJ_RUNNING;
    j->phase[0] = 0; j->done = j->total = 0; j->can_cancel = 0;
    if (start(!strcmp(j->op, "upgrade") ? JOB_UPGRADE : JOB_ROLLBACK, j->package) != 0)
        finish(j, RXJ_FAILED, "CANNOTSTART", "could not start the worker process");
}

static void decline(const char *how)
{
    rx_job *j = active;
    if (!confirming || !j) return;
    confirm_close();
    logline("%s %s %s", j->op, j->package, how);
    finish(j, RXJ_DECLINED, "", "nothing changed");
    busy(0);
    status("nothing changed");
}

/* Returns 1 when the window should now quit. */
static int on_message(pm_msg *m)
{
    char info[80];
    /* Every message names its job. One for any other job than the one out
       cannot happen while in_flight holds the next one back; if it ever
       does, it is logged and not read as this job's. */
    if (!in_flight || m->seq != cur.seq) {
        logline("message for job %ld ignored: job %ld is out%s", m->seq, cur.seq, in_flight ? "" : " (none)");
        return 0;
    }
    if (m->kind == PM_PROGRESS) {
        unsigned long pct = m->total ? (m->done * 100UL) / m->total : (m->done ? 100 : 0);
        if (m->total) snprintf(info, sizeof info, "%s %lu%%", m->phase, pct);
        else snprintf(info, sizeof info, "%s", m->phase);
        SetAttrs(gauge, MUIA_Gauge_Current, pct, MUIA_Gauge_InfoText, (IPTR)info, TAG_DONE);
        /* Stated by the event, not inferred from the phase. */
        SetAttrs(btn_cancel, MUIA_Disabled, !m->can_cancel || quit_pending, TAG_DONE);
        snprintf(last_phase, sizeof last_phase, "%s", m->phase); last_can_cancel = m->can_cancel;
        if (active) {
            snprintf(active->phase, sizeof active->phase, "%s", m->phase);
            active->done = m->done; active->total = m->total; active->can_cancel = m->can_cancel;
        }
        logline("event %s %s %lu/%lu can_cancel=%d cancel_button=%s", m->id, m->phase,
                m->done, m->total, m->can_cancel, m->can_cancel && !quit_pending ? "enabled" : "disabled");
        return 0;
    }
    if (m->kind == PM_DONE) {
        static const char *verb[] = { "none", "install", "remove", "update",
                                      "preview upgrade", "upgrade", "preview rollback", "rollback" };
        static const char *past[] = { "", "installed", "removed", "index updated",
                                      "", "upgraded", "", "rolled back" };
        job_kind kind = cur.kind;
        rx_job *j = active;
        char line[160], why[640];
        snprintf(line, sizeof line, "%s %s", past[kind], cur.id);
        logline("done %s %s status=%d%s%s", verb[kind], cur.id, (int)m->status,
                m->status == PKG_OK ? "" : " ", m->status == PKG_OK ? "" : m->err.summary);
        in_flight = 0;
        err_text(why, sizeof why, &m->err);
        if (kind == JOB_PREVIEW_UPGRADE || kind == JOB_PREVIEW_ROLLBACK) {
            /* The plan came back as data. Show it, and ask -- or say why not. */
            const pkg_plan *p = &m->plan;
            const char *what = kind == JOB_PREVIEW_UPGRADE ? "upgrade" : "rollback";
            char text[640], a[80], b[80], keep[PKG_MAXID];
            snprintf(keep, sizeof keep, "%s", cur.id);
            if (m->status != PKG_OK) {
                if (j) finish(j, m->status == PKG_E_CANCELLED ? RXJ_CANCELLED : RXJ_FAILED,
                              rx_statusword(m->status), why);
                if (quit_pending) return 1;
                busy(0);
                show_err(what, m->status, &m->err);
                return 0;
            }
            rev_label(a, sizeof a, p->from_version, p->from_revision);
            rev_label(b, sizeof b, p->to_version, p->to_revision);
            logline("plan %s %s: %s -> %s allowed=%d add=%d replace=%d remove=%d kept=%d unchanged=%d conflicts=%d%s%s",
                    what, keep, a, b, p->allowed, p->add, p->replace, p->remove, p->kept, p->unchanged,
                    p->conflicts, p->why[0] ? " why=" : "", p->why);
            if (!p->allowed || p->conflicts) {
                snprintf(text, sizeof text, "%s not possible: %s", what, p->why);
                if (j) finish(j, RXJ_FAILED, p->conflicts ? "CONFLICT" : "NOTALLOWED", p->why);
                if (quit_pending) return 1;
                busy(0);
                status(text);
                return 0;
            }
            if (quit_pending) {
                if (j) finish(j, RXJ_DECLINED, "", "the window was closed before the plan was confirmed");
                return 1;
            }
            if (j) {
                j->state = RXJ_CONFIRM;
                j->phase[0] = 0; j->can_cancel = 0;
                snprintf(j->message, sizeof j->message,
                         "from %s to %s: %d added, %d replaced, %d removed, %d kept as changed, %d unchanged",
                         a, b, p->add, p->replace, p->remove, p->kept, p->unchanged);
            }
            /* Files a running program may read or write are not promised
               anything: the drawer is swapped underneath it. Say so. */
            snprintf(text, sizeof text,
                     "%s %s\n\nfrom %s to %s\n\n"
                     "%d file%s added, %d replaced, %d removed\n%d kept as you changed %s, %d unchanged\n\n"
                     "Files you changed are kept.\nIf %s is running, close it first.\n\nProceed?",
                     kind == JOB_PREVIEW_UPGRADE ? "Upgrade" : "Roll back", keep, a, b,
                     p->add, p->add == 1 ? "" : "s", p->replace, p->remove,
                     p->kept, p->kept == 1 ? "it" : "them", p->unchanged, keep);
            confirming = 1;
            SetAttrs(gauge, MUIA_Gauge_Current, 0, MUIA_Gauge_InfoText, (IPTR)"waiting for your decision", TAG_DONE);
            status("waiting for your decision");
            if (auto_yes) { proceed("confirmed automatically (--yes)"); return 0; }
            /* Open first: Floattext lays its text out only when it has a
               window, and text set before that stays blank. */
            SetAttrs(win_confirm, MUIA_Window_Open, TRUE, TAG_DONE);
            SetAttrs(txt_confirm, MUIA_Floattext_Text, (IPTR)text, TAG_DONE);
            return 0;
        }
        if (j) {
            if (m->status == PKG_OK) finish(j, RXJ_DONE, "", line);
            else finish(j, m->status == PKG_E_CANCELLED ? RXJ_CANCELLED : RXJ_FAILED,
                        rx_statusword(m->status), why);
        }
        if (quit_pending) return 1;       /* the worker is gone; see worker.c */
        busy(0);
        {
            IPTR t = 0; char keep[PKG_MAXID];
            snprintf(keep, sizeof keep, "%s", cur.id);
            GetAttr(MUIA_String_Contents, str_search, &t);
            refill((const char *)t, keep[0] ? keep : NULL, "done");
        }
        /* After the refill, which writes its own count: the outcome is what
           the user is waiting to read. */
        if (m->status == PKG_OK && kind != JOB_UPDATE) status(line);
        else if (m->status == PKG_OK) status(past[kind]);
        else show_err(verb[kind], m->status, &m->err);
    }
    return 0;
}

/* A close while a job runs: cancel it where cancelling is free, and leave
 * when it reports done -- never before, because the worker process is
 * still running this program's code until then. */
static void close_requested(void)
{
    if (!in_flight) return;
    quit_pending = 1;
    cur.cancel = 1;
    if (active) active->cancel_asked = 1;
    logline("close requested during %s can_cancel=%d", last_phase[0] ? last_phase : "start", last_can_cancel);
    status(last_can_cancel ? "closing: cancelling the download..."
                           : "closing when the current operation finishes...");
    SetAttrs(btn_cancel, MUIA_Disabled, TRUE, TAG_DONE);
}

/* The worker outliving its last message is not expected; if it happens, the
 * window says so and keeps waiting, because unloading the code it runs is
 * the one thing that must not happen. */
static void still_waiting(long ms)
{
    char line[96];
    snprintf(line, sizeof line, "waiting for the worker to end (%ld s)...", ms / 1000);
    status(line);
    logline("worker task still present %ld ms after done", ms);
}

/* ----------------------------------------------------------------- ARexx */

/* What a command does is decided here, on this task, between two Zune
 * events: queries read through libpkg exactly as the window does, and
 * operations go through begin() like the buttons, so a script is held to
 * the same rules -- one operation at a time, the same worker, the same
 * confirmation. */

static char rxout[RX_MAXRESULT + 1];

/* A script polling JOB in a loop sends the same line thousands of times;
 * the log keeps the first and then says how many followed, so a test's
 * event log stays readable. `quiet` silences the reply of a repeat. */
static char last_cmd[128];
static long repeats;
static int quiet;

static void log_command(const char *line)
{
    if (strlen(line) < sizeof last_cmd && !strcmp(line, last_cmd)) { repeats++; quiet = 1; return; }
    if (repeats) logline("arexx> (the line above %ld more times)", repeats);
    repeats = 0; quiet = 0;
    snprintf(last_cmd, sizeof last_cmd, "%s", line);
    logline("arexx> %s", line);
}

/* The binary's own hash, as apkg --version reports it: a run report names
 * the binary that answered, not the source it was built from. Once. */
static const char *self_sha(void)
{
    static char sha[65];
    if (!sha[0] && pkg_self_sha256(sha) != 0) snprintf(sha, sizeof sha, "unknown");
    return sha;
}

static void rx_ok(rexx_msg *m, const char *result)
{
    char head[72]; size_t i;
    snprintf(head, sizeof head, "%s", result);
    for (i = 0; head[i]; i++) if (head[i] == '\n') head[i] = '|';
    if (rexx_reply(m, RX_RC_OK, result, NULL) != RX_RC_OK)
        logline("arexx< rc=%d NOMEM no memory for the answer (len=%lu)", RX_RC_ERROR, (unsigned long)strlen(result));
    else if (!quiet)
        logline("arexx< rc=0 len=%lu %s%s", (unsigned long)strlen(result), head,
                strlen(result) >= sizeof head ? "..." : "");
}

static void rx_fail(rexx_msg *m, int rc, const char *fmt, ...)
{
    char error[600];
    va_list ap;
    va_start(ap, fmt); vsnprintf(error, sizeof error, fmt, ap); va_end(ap);
    if (!quiet) logline("arexx< rc=%d %s", rc, error);
    rexx_reply(m, rc, NULL, error);
}

static void reply_job(rexx_msg *m, const rx_job *j, const char *field)
{
    static char out[2048];
    if (rx_format_job(j, field, out, sizeof out) != 0)
        rx_fail(m, RX_RC_FAIL, "BADARGS unknown field \"%s\"; fields: %s", field, rx_job_fields());
    else rx_ok(m, out);
}

static pkg_ctx *rx_open(rexx_msg *m)
{
    pkg_ctx *c = NULL; pkg_err e; pkg_status st;
    memset(&e, 0, sizeof e);
    st = pkg_open(root, PKG_OPEN_READ, &c, &e);
    if (st != PKG_OK) {
        char t[600]; err_text(t, sizeof t, &e);
        rx_fail(m, RX_RC_ERROR, "%s %s", rx_statusword(st), t);
        return NULL;
    }
    return c;
}

/* The row for `id`, as `apkg show` finds it: the catalogue's entry for this
 * machine (or the one show would describe), else what the registry records.
 * 1 = found. `compat` and `requirements` (may be NULL) get the verdicts. */
static int lookup(pkg_ctx *c, const char *id, pkg_entry *out, int *in_index, pkg_compat *compat,
                  char *requirements, size_t rn)
{
    pkg_details det; pkg_err e;
    memset(&e, 0, sizeof e);
    *in_index = 0;
    if (pkg_details_get(c, index_path, id, &det, &e) != PKG_OK) return 0;
    *out = det.e;
    *in_index = det.in_index;
    if (compat) *compat = det.compat;
    if (requirements) snprintf(requirements, rn, "%s", det.requirements);
    return 1;
}

static void rx_list(rexx_msg *m, const rx_req *r)
{
    pkg_ctx *c = rx_open(m); pkg_entries rows = { 0 }; pkg_err e; pkg_status st;
    size_t k = 0; int i, over = 0;
    if (!c) return;
    memset(&e, 0, sizeof e);
    if (r->installed && !r->match[0]) {
        st = pkg_installed(c, index_path, &rows, &e);
        if (st != PKG_OK) { pkg_entries_free(&rows); memset(&rows, 0, sizeof rows); st = pkg_installed(c, NULL, &rows, &e); }
    } else st = pkg_query(c, index_path, r->match[0] ? r->match : NULL, &rows, NULL, &e);
    pkg_close(c);
    if (st == PKG_E_NOT_FOUND && !r->installed) { rx_fail(m, RX_RC_ERROR, "NOINDEX there is no catalogue yet; UPDATE fetches it"); pkg_entries_free(&rows); return; }
    if (st != PKG_OK) { char t[600]; err_text(t, sizeof t, &e); rx_fail(m, RX_RC_ERROR, "%s %s", rx_statusword(st), t); pkg_entries_free(&rows); return; }
    rxout[0] = 0;
    for (i = 0; i < rows.n && !over; i++) {
        size_t n = strlen(rows.v[i].id);
        if (r->installed && !rows.v[i].installed) continue;
        if (k + n + 2 > sizeof rxout) { over = 1; break; }
        if (k) rxout[k++] = ' ';
        memcpy(rxout + k, rows.v[i].id, n + 1); k += n;
    }
    pkg_entries_free(&rows);
    if (over) rx_fail(m, RX_RC_ERROR, "TOOLONG the list does not fit in one ARexx result; narrow it with MATCH");
    else rx_ok(m, rxout);
}

static void rx_info(rexx_msg *m, const rx_req *r)
{
    pkg_ctx *c = rx_open(m); pkg_entry e; int in_index, up = 0, back = 0, f;
    char why[240]; pkg_compat rv = PKG_COMPAT_UNDETERMINED; char reqs[16] = "";
    if (!c) return;
    if (!lookup(c, r->package, &e, &in_index, &rv, reqs, sizeof reqs)) {
        pkg_close(c);
        rx_fail(m, RX_RC_ERROR, "NOTFOUND %s is neither in the catalogue nor installed", r->package);
        return;
    }
    if (e.installed) {
        up = pkg_can_upgrade(&e, why, sizeof why);
        back = pkg_can_rollback(c, e.id, why, sizeof why);
    }
    pkg_close(c);
    f = rx_format_entry(&e, up, back, pkg_compat_word(rv), reqs, r->field, rxout, sizeof rxout);
    if (f == -1) rx_fail(m, RX_RC_FAIL, "BADARGS unknown field \"%s\"; fields: %s", r->field, rx_entry_fields());
    else rx_ok(m, rxout);
}

static void rx_requirements(rexx_msg *m, const rx_req *r)
{
    pkg_ctx *c = rx_open(m); pkg_entry e; pkg_err err; pkg_status st; int in_index; size_t n;
    if (!c) return;
    if (!lookup(c, r->package, &e, &in_index, NULL, NULL, 0) || !in_index) {
        pkg_close(c);
        rx_fail(m, RX_RC_ERROR, "NOTFOUND %s is not in the catalogue", r->package);
        return;
    }
    memset(&err, 0, sizeof err);
    rxout[0] = 0;
    st = pkg_requirements(c, index_path, r->package, rxout, sizeof rxout, &err);
    pkg_close(c);
    if (st != PKG_OK) { char t[600]; err_text(t, sizeof t, &err); rx_fail(m, RX_RC_ERROR, "%s %s", rx_statusword(st), t); return; }
    n = strlen(rxout);
    while (n && rxout[n - 1] == '\n') rxout[--n] = 0;
    rx_ok(m, rxout);
}

static void rx_show(rexx_msg *m, const rx_req *r)
{
    pkg_ctx *c = rx_open(m); pkg_entry e; pkg_entry *sel; int in_index, known;
    if (!c) return;
    known = lookup(c, r->package, &e, &in_index, NULL, NULL, 0) && in_index;
    pkg_close(c);
    if (!known) { rx_fail(m, RX_RC_ERROR, "NOTFOUND %s is not in the catalogue", r->package); return; }
    /* Clear the search and the filters, or the row could be filtered out of
       the very list it is to be shown in. */
    SetAttrs(str_search, MUIA_NoNotify, TRUE, MUIA_String_Contents, (IPTR)"", TAG_DONE);
    last_term[0] = 0; cat_chosen[0] = 0;
    SetAttrs(cyc_cat,   MUIA_NoNotify, TRUE, MUIA_Cycle_Active, 0, TAG_DONE);
    SetAttrs(cyc_state, MUIA_NoNotify, TRUE, MUIA_Cycle_Active, 0, TAG_DONE);
    refill("", r->package, "arexx show");
    sel = selected();
    if (!sel || strcmp(sel->id, r->package)) { rx_fail(m, RX_RC_ERROR, "NOTFOUND %s could not be selected in the list", r->package); return; }
    DoMethod(lst, MUIM_List_Jump, MUIV_List_Jump_Active);
    SetAttrs(app, MUIA_Application_Iconified, FALSE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    DoMethod(win, MUIM_Window_ToFront);
    SetAttrs(win, MUIA_Window_Activate, TRUE, TAG_DONE);
    rx_ok(m, "");
}

static void rx_start(rexx_msg *m, const rx_req *r)
{
    job_kind kind = r->cmd == RXC_UPDATE ? JOB_UPDATE : r->cmd == RXC_INSTALL ? JOB_INSTALL :
                    r->cmd == RXC_REMOVE ? JOB_REMOVE : r->cmd == RXC_UPGRADE ? JOB_PREVIEW_UPGRADE :
                    JOB_PREVIEW_ROLLBACK;
    rx_job *j;
    if (quit_pending || rexx_quit) { rx_fail(m, RX_RC_ERROR, "CLOSING PkgManager is closing"); return; }
    if (busy_now()) {
        rx_job *a = active;
        if (a) rx_fail(m, RX_RC_WARN, "BUSY job %ld (%s%s%s) is %s", a->id, a->op,
                       a->package[0] ? " " : "", a->package, rx_statename(a->state));
        else rx_fail(m, RX_RC_WARN, "BUSY another operation is running");
        return;
    }
    if (kind != JOB_UPDATE) {
        pkg_ctx *c = rx_open(m); pkg_entry e; int in_index, known;
        if (!c) return;
        known = lookup(c, r->package, &e, &in_index, NULL, NULL, 0);
        pkg_close(c);
        if (!known || (kind == JOB_INSTALL && !in_index)) {
            rx_fail(m, RX_RC_ERROR, "NOTFOUND %s %s", r->package,
                    kind == JOB_INSTALL ? "is not in the catalogue" : "is neither in the catalogue nor installed");
            return;
        }
    }
    /* The answer is the job's number, and it is allocated before the job
       exists: if that fails the script hears NOMEM and nothing was started. */
    snprintf(rxout, sizeof rxout, "%ld", next_job);
    if (!rexx_prepare(m, rxout)) { rx_fail(m, RX_RC_ERROR, "NOMEM no memory for the answer; nothing was started"); return; }
    j = begin(kind, r->package, "arexx");
    if (!j || j->state == RXJ_FAILED) { rx_fail(m, RX_RC_ERROR, "CANNOTSTART could not start the worker process"); return; }
    rx_ok(m, rxout);
}

static void rx_jobcmd(rexx_msg *m, const rx_req *r)
{
    rx_job *j = find_job(r->job);
    int i;
    if (!j) {
        if (r->job) rx_fail(m, RX_RC_ERROR, "NOJOB there is no job %ld", r->job);
        else rx_fail(m, RX_RC_ERROR, "NOJOB no job has been started");
        return;
    }
    if (rx_format_job(j, r->field, rxout, sizeof rxout) == -1) {
        rx_fail(m, RX_RC_FAIL, "BADARGS unknown field \"%s\"; fields: %s", r->field, rx_job_fields());
        return;
    }
    if (r->cmd == RXC_WAIT && !rx_final(j->state)) {
        for (i = 0; i < MAXWAITS; i++) if (!waits[i].m) break;
        if (i == MAXWAITS) { rx_fail(m, RX_RC_WARN, "BUSY %d scripts are already waiting", MAXWAITS); return; }
        waits[i].m = m; waits[i].job = j->id;
        snprintf(waits[i].field, sizeof waits[i].field, "%s", r->field);
        logline("arexx wait for job %ld (%s)", j->id, rx_statename(j->state));
        return;
    }
    rx_ok(m, rxout);
}

static void rx_cancel(rexx_msg *m, const rx_req *r)
{
    rx_job *j = find_job(r->job);
    if (!j) { rx_fail(m, RX_RC_ERROR, "NOJOB there is no job %ld", r->job); return; }
    if (rx_final(j->state)) { rx_fail(m, RX_RC_WARN, "FINISHED job %ld is already %s", j->id, rx_statename(j->state)); return; }
    if (j->state == RXJ_CONFIRM) {
        if (!rexx_prepare(m, "declined")) { rx_fail(m, RX_RC_ERROR, "NOMEM no memory for the answer; nothing was done"); return; }
        decline("declined (ARexx CANCEL)"); rx_ok(m, "declined"); return;
    }
    /* Only while the library's last event said it would still take it; the
       library decides, and JOB then says what came of it. */
    if (j != active || !in_flight || !j->can_cancel) {
        rx_fail(m, RX_RC_WARN, "NOTCANCELLABLE job %ld is in phase %s, which cannot be cancelled",
                j->id, j->phase[0] ? j->phase : "start");
        return;
    }
    if (!rexx_prepare(m, "requested")) { rx_fail(m, RX_RC_ERROR, "NOMEM no memory for the answer; nothing was done"); return; }
    cur.cancel = 1; j->cancel_asked = 1;
    status("cancelling...");
    logline("cancel asked by ARexx for job %ld in %s", j->id, j->phase);
    rx_ok(m, "requested");
}

static void rexx_dispatch(rexx_msg *m, const char *line)
{
    rx_req r; char err[300];
    log_command(line);
    if (rx_parse(line, &r, err, sizeof err) != RX_RC_OK) { rx_fail(m, RX_RC_FAIL, "%s", err); return; }
    switch (r.cmd) {
    case RXC_VERSION:
        snprintf(rxout, sizeof rxout, "PkgManager %s ARexx %d sha256 %s", PM_VERSION, RX_INTERFACE, self_sha());
        rx_ok(m, rxout); break;
    case RXC_HELP:         rx_ok(m, rx_help()); break;
    case RXC_LIST:         rx_list(m, &r); break;
    case RXC_INFO:         rx_info(m, &r); break;
    case RXC_REQUIREMENTS: rx_requirements(m, &r); break;
    case RXC_SHOW:         rx_show(m, &r); break;
    case RXC_UPDATE: case RXC_INSTALL: case RXC_REMOVE: case RXC_UPGRADE: case RXC_ROLLBACK:
        rx_start(m, &r); break;
    case RXC_JOB: case RXC_WAIT: rx_jobcmd(m, &r); break;
    case RXC_CANCEL:       rx_cancel(m, &r); break;
    case RXC_QUIT:
        /* The same as the close gadget, and as safe. */
        if (!rexx_prepare(m, "closing")) { rx_fail(m, RX_RC_ERROR, "NOMEM no memory for the answer; nothing was done"); break; }
        if (confirming) decline("declined (ARexx QUIT)");
        if (in_flight) close_requested(); else rexx_quit = 1;
        rx_ok(m, "closing");
        break;
    case RXC_LASTERROR:
        {
            char text[600];
            snprintf(text, sizeof text, "%s", rexx_lasterror(m));
            logline("arexx< rc=%d lasterror %s", rexx_reply(m, RX_RC_OK, text, REXX_KEEP), text);
        }
        break;
    default: rx_fail(m, RX_RC_FAIL, "BADCOMMAND"); break;
    }
}

/* Quiet lasts for the one command: a WAIT answered later, when its job
   ends, is always logged. */
static void rexx_command(rexx_msg *m, const char *line)
{
    rexx_dispatch(m, line);
    quiet = 0;
}

/* ------------------------------------------------------------------ main */

static int real_main(int argc, char **argv)
{
    ULONG sigs = 0, portsig, rxsig;
    IPTR ret;
    int i, quit = 0;
    const char *rexxlib = NULL;
    static char title[64];

    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--log") && i + 1 < argc) logf = fopen(argv[++i], "w");
        else if (!strcmp(argv[i], "--root") && i + 1 < argc) snprintf(root, sizeof root, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--slow") && i + 1 < argc) job_set_slow(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--break-download-at") && i + 1 < argc)   /* tests */
            pkg_test_break_download(atol(argv[++i]), 1);
        else if (!strcmp(argv[i], "--corrupt-download-at") && i + 1 < argc) /* tests */
            pkg_test_break_download(atol(argv[++i]), 2);
        else if (!strcmp(argv[i], "--yes")) auto_yes = 1;        /* tests: no requester to click */
        else if (!strcmp(argv[i], "--rexxlib") && i + 1 < argc) rexxlib = argv[++i];  /* tests: a machine without ARexx */
        else if (!strcmp(argv[i], "--test-nomem") && i + 1 < argc) {                   /* tests: allocation failures */
            const char *w = argv[++i];
            if (!strcmp(w, "start")) job_test_nomem();
            else if (!strcmp(w, "result")) rexx_test_nomem();
            else if (!strcmp(w, "prepare")) rexx_test_nomem_prepare();
        }

    snprintf(index_path, sizeof index_path, "%s/db/index.json", root);
    display_hook.h_Entry = (HOOKFUNC)AROS_ASMSYMNAME(display_func);
    cat_entries[0] = (STRPTR)"All categories"; cat_entries[1] = NULL;

    port = CreateMsgPort();
    if (!port) return RETURN_FAIL;
    portsig = 1UL << port->mp_SigBit;

    rexx_open(rexxlib);
    rxsig = rexx_sigmask();
    if (logf) logline("start PkgManager %s sha256 %s root %s: %s", PM_VERSION, self_sha(), root, rexx_state());
    snprintf(title, sizeof title, "%s", rexx_active() ? "PkgManager" : "PkgManager (no ARexx port)");

    app = ApplicationObject,
        MUIA_Application_Title,       (IPTR)"PkgManager",
        MUIA_Application_Version,     (IPTR)"$VER: PkgManager 0.3.1-rc1 (29.9.2026)",
        MUIA_Application_Description, (IPTR)"Install software from AROS Archives",
        MUIA_Application_Base,        (IPTR)"PKGMANAGER",
        SubWindow, (win = WindowObject,
            MUIA_Window_Title, (IPTR)title,
            MUIA_Window_ID,    MAKE_ID('P','K','G','M'),
            /* A fixed first placement: the list needs the width, and the
               acceptance run clicks by coordinates read off a screenshot. */
            MUIA_Window_LeftEdge, 100, MUIA_Window_TopEdge, 40,
            MUIA_Window_Width, 800,   MUIA_Window_Height, 600,
            WindowContents, (VGroup,
                Child, (HGroup,
                    Child, (str_search = StringObject,
                        MUIA_Frame, MUIV_Frame_String,
                        MUIA_CycleChain, 1,
                    End),
                    Child, (cyc_cat = CycleObject,
                        MUIA_Cycle_Entries, (IPTR)cat_entries,
                    End),
                    Child, (cyc_state = CycleObject,
                        MUIA_Cycle_Entries, (IPTR)state_entries,
                    End),
                    Child, (btn_update = SimpleButton("Update index")),
                End),
                Child, (lst = ListviewObject,
                    MUIA_Listview_List, (ListObject,
                        MUIA_Frame,           MUIV_Frame_InputList,
                        MUIA_List_Format,     (IPTR)"BAR,BAR,BAR,",
                        MUIA_List_Title,      TRUE,
                        MUIA_List_DisplayHook, (IPTR)&display_hook,
                    End),
                End),
                Child, (txt_detail = FloattextObject,
                    MUIA_Frame, MUIV_Frame_ReadList,
                    MUIA_FixHeightTxt, (IPTR)"\n\n\n\n\n\n\n\n",
                    MUIA_Floattext_Text, (IPTR)"",
                End),
                Child, (HGroup,
                    Child, (btn_install  = SimpleButton("Install")),
                    Child, (btn_upgrade  = SimpleButton("Upgrade")),
                    Child, (btn_rollback = SimpleButton("Roll back")),
                    Child, (btn_remove   = SimpleButton("Remove")),
                    Child, (btn_cancel   = SimpleButton("Cancel")),
                End),
                Child, (gauge = GaugeObject,
                    MUIA_Frame, MUIV_Frame_Gauge,
                    MUIA_Gauge_Horiz, TRUE,
                    MUIA_Gauge_Max, 100,
                    MUIA_Gauge_Current, 0,
                    MUIA_Gauge_InfoText, (IPTR)"",
                End),
                Child, (txt_status = TextObject,
                    MUIA_Frame, MUIV_Frame_Text,
                    MUIA_Text_Contents, (IPTR)"",
                End),
            End),
        End),
        SubWindow, (win_confirm = WindowObject,
            MUIA_Window_Title, (IPTR)"PkgManager: confirm",
            MUIA_Window_ID,    MAKE_ID('P','K','G','C'),
            /* Fixed, like the main window: the acceptance run presses
               Proceed by coordinates. */
            MUIA_Window_LeftEdge, 260, MUIA_Window_TopEdge, 180,
            MUIA_Window_Width, 440,
            WindowContents, (VGroup,
                Child, (txt_confirm = FloattextObject,
                    MUIA_Frame, MUIV_Frame_ReadList,
                    MUIA_FixHeightTxt, (IPTR)"\n\n\n\n\n\n\n\n\n\n\n",
                    MUIA_Floattext_Text, (IPTR)"",
                End),
                Child, (HGroup,
                    Child, (btn_proceed = SimpleButton("Proceed")),
                    Child, (btn_decline = SimpleButton("Cancel")),
                End),
            End),
        End),
    End;

    if (!app) { rexx_close("CLOSING PkgManager could not open its window"); DeleteMsgPort(port); return RETURN_FAIL; }

    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(str_search, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_SEARCH);
    DoMethod(cyc_cat,   MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_FILTER);
    DoMethod(cyc_state, MUIM_Notify, MUIA_Cycle_Active, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_FILTER);
    DoMethod(btn_update,  MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_UPDATE);
    DoMethod(btn_install, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_INSTALL);
    DoMethod(btn_remove,  MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE);
    DoMethod(btn_cancel,  MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(btn_upgrade, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_UPGRADE);
    DoMethod(btn_rollback,MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ROLLBACK);
    DoMethod(btn_proceed, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_PROCEED);
    DoMethod(btn_decline, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_DECLINE);
    DoMethod(win_confirm, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_DECLINE);
    DoMethod(lst, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_SELECT);

    SetAttrs(btn_install,  MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(btn_remove,   MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(btn_cancel,   MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(btn_upgrade,  MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(btn_rollback, MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    refill("", NULL, "start");

    while (!quit) {
        ret = DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
        if (ret == MUIV_Application_ReturnID_Quit) {
            if (confirming) { decline("declined (window closed)"); break; }
            if (in_flight) close_requested(); else break;
        }
        switch (ret) {
        case ID_SEARCH: search_acknowledged(); break;
        case ID_FILTER: {
            IPTR a = 0, st = 0;
            GetAttr(MUIA_Cycle_Active, cyc_cat, &a);
            GetAttr(MUIA_Cycle_Active, cyc_state, &st);
            snprintf(cat_chosen, sizeof cat_chosen, "%s", a ? cat_names[a - 1] : "");
            logline("filter category=%s state=%s", cat_chosen[0] ? cat_chosen : "all", state_entries[st]);
            refill_keep("filter");
        } break;
        case ID_SELECT:  show_detail(); break;
        case ID_UPDATE:  logline("update pressed"); begin(JOB_UPDATE, NULL, "window"); break;
        case ID_INSTALL: { pkg_entry *e = selected(); if (e) { logline("install pressed for %s", e->id); begin(JOB_INSTALL, e->id, "window"); } } break;
        case ID_REMOVE:  { pkg_entry *e = selected(); if (e) { logline("remove pressed for %s", e->id); begin(JOB_REMOVE, e->id, "window"); } } break;
        case ID_UPGRADE: { pkg_entry *e = selected(); if (e) { logline("upgrade pressed for %s", e->id); begin(JOB_PREVIEW_UPGRADE, e->id, "window"); } } break;
        case ID_ROLLBACK:{ pkg_entry *e = selected(); if (e) { logline("rollback pressed for %s", e->id); begin(JOB_PREVIEW_ROLLBACK, e->id, "window"); } } break;
        case ID_CANCEL:  if (in_flight) { cur.cancel = 1; if (active) active->cancel_asked = 1; status("cancelling..."); logline("cancel pressed for %s", cur.id); } break;
        case ID_PROCEED: proceed("confirmed"); break;
        case ID_DECLINE: decline("declined"); break;
        default: break;
        }
        if (sigs) {
            sigs = Wait(sigs | portsig | rxsig | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) {
                if (confirming) { decline("declined (break)"); break; }
                if (in_flight) close_requested(); else break;
            }
            if (sigs & portsig) {
                pm_msg *m;
                while ((m = (pm_msg *)GetMsg(port))) { if (on_message(m)) quit = 1; FreeVec(m); }
            }
            if (sigs & rxsig) rexx_poll(rexx_command);
        }
        if (rexx_quit && !busy_now()) break;
    }

    /* The worker's last message says the operation ended; whether the task
       has left the system is a separate fact, measured here before the code
       it ran is unloaded. */
    logline("worker task gone %ld ms after done", job_wait_gone(3000, still_waiting));
    for (i = 0; i < MAXWAITS; i++)
        if (waits[i].m) { rexx_reply(waits[i].m, RX_RC_ERROR, NULL, "CLOSING PkgManager has quit"); waits[i].m = NULL; }
    rexx_close("CLOSING PkgManager has quit");
    MUI_DisposeObject(app);
    pkg_entries_free(&rows);
    DeleteMsgPort(port);
    logline("quit");
    if (logf) fclose(logf);
    return RETURN_OK;
}

int main(int argc, char **argv)
{
    return pkg_run_with_stack(real_main, argc, argv);
}
