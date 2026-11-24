/* PkgManager -- the Zune front end. Presentation only, as pkg.h says of every
 * front: the rows come from pkg_query/pkg_installed, the work is done by a
 * worker process through libpkg, and the only decisions here are which
 * button is enabled. Pattern after ~/Work/AROS/src/hello2.c. */
#include "job.h"
#include "rexx.h"
#include "rexxcmd.h"
#include "../about.h"
#include "../libpkg/verify.h"
#include "../libpkg/media.h"
#include "pic.h"
#include "links.h"
#include "../libpkg/util.h"
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/graphics.h>
#include <proto/diskfont.h>
#include <graphics/text.h>
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <strings.h>

/* A text that does not fit its place ends in "..." where Zune can do that
   (MUIA_Text_Shorten, as in MUI 5). A Zune without the attribute ignores it
   and clips the text, as it always did, so it is set wherever a text may be
   wider than the window. Defined here for SDKs whose headers do not have
   it yet. */
#ifndef MUIA_Text_Shorten
#define MUIA_Text_Shorten            0x80428bbd
#define MUIV_Text_Shorten_ElideRight 5
#endif

/* PM_TEST_VERSION names a clearly labelled test build, so a self-update
   test can start from a version older than the test release; a release
   never sets it. */
#ifdef PM_TEST_VERSION
#define PM_VERSION PM_TEST_VERSION
#else
#define PM_VERSION "0.8.3"
#endif

enum { ID_SEARCH = 1, ID_INSTALL, ID_REMOVE, ID_UPDATE, ID_CANCEL, ID_SELECT, ID_FILTER,
       ID_UPGRADE, ID_ROLLBACK, ID_PROCEED, ID_DECLINE, ID_ABOUT, ID_ABOUT_CLOSE, ID_OPEN, ID_RUN, ID_VERIFY,
       ID_CHECK, ID_SELFUPDATE, ID_MAIN, ID_DETAILS, ID_BACK, ID_PREV, ID_NEXT, ID_QUIT, ID_ROWCLICK, ID_ROWDOUBLE, ID_ONLYINST };

/* The pictures on a package's page stand side by side in a strip that
   scrolls sideways, each in a place of this size (a larger one is scaled
   down), so the next one shows at the edge. */
#define CAR_GAP 8
/* Chosen at the start from the screen's height, so that the page fits a
   small screen and uses a large one. */
static int car_w = 480, car_h = 300;

static Object *app, *win, *menus, *str_search, *lst_side, *lst_sidelist, *txt_view, *lst, *lst_list,
              *gauge, *grp_progress, *btn_cancel,
              *btn_open, *btn_details,
              *btn_main2, *btn_open2, *btn_remove2, *btn_back, *grp_head,
              *win_confirm, *btn_proceed, *btn_decline,
              *win_about, *btn_about_close, *btn_check, *btn_selfupdate,
              *pg_main, *reg_page, *grp_car, *virt_car, *grp_carnav, *txt_caption, *btn_prev, *btn_next,
              *txt_name, *txt_name2, *txt_facts, *txt_sum2, *txt_facts2, *pic_icon2, *pic_car[PKG_MEDIA_SHOTS];
static int auto_yes;              /* --yes: confirm requesters without asking; for tests */
static int no_auto;               /* --no-auto: nothing is fetched at the start; for tests */
static int no_view;               /* --no-text-view: the plain fallback; for tests */
static pkg_entries rows;          /* the whole query; the list shows a filtered view */
static job cur;
static struct MsgPort *port;      /* the worker's messages arrive here */
static char root[PKG_MAXPATH] = "SYS:Packages";
static char index_path[PKG_MAXPATH];
static const char *index_url = PKG_INDEX_URL;
static int quit_pending;          /* close asked for while a job ran */
static char last_phase[16]; static int last_can_cancel;
static int confirming;            /* a plan is shown and the window waits for the user */
static int rexx_quit;             /* QUIT arrived with nothing running */
/* The shared updates model (pkg_updates in pkg.h), recomputed by every
   refill: after start, every finished job and every search or filter. It
   reads the catalogue and the registry only; no network. */
static pkg_updates upd;
static int start_updates;         /* --updates: open with the Updates filter */

/* Updating arospkg itself (libpkg/selfup.h). The worker finds and stages;
   the window asks and decides; the installation's apkg replaces the files
   after this window has gone, and starts it again. */
static su_job sujob;
static int su_offer;              /* the release check found a newer release */
static int su_confirming;         /* the confirmation shows the update */
static int su_handed_off;         /* apkg has the update: quit now */
static int wb_start;              /* started from the icon: start again from it */
static char log_path[PKG_MAXPATH];
static char self_drawer[PKG_MAXPATH];
static char announce_nonce[24];

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

/* The state selector's entries, in order (state_entries below). */
enum { ST_ALL, ST_INSTALLED, ST_UPDATES, ST_RECENT, ST_LAST };

/* The news behind the two views, recomputed by every refill that shows one. */
static pkg_news news;
static int news_view;             /* ST_RECENT or ST_LAST while news holds that view */

/* The news row for a list row: the same id and the same build. */
static const pkg_news_row *news_row(const pkg_entry *e)
{
    int i;
    for (i = 0; news_view && i < news.n; i++)
        if (!strcmp(news.rows[i].id, e->id) && !strcmp(news.rows[i].arch, e->arch) && !strcmp(news.rows[i].abi, e->abi))
            return &news.rows[i];
    return NULL;
}

/* ------------------------------------------------------------ list display */

/* The pictures of each row, as the list's own images (MUIM_List_CreateImage),
   one set per entry of `rows`, by its index there: the package's small icon,
   and a mark for its state (a tick when it is installed, an arrow when a
   newer revision waits). NULL where a row has none. Made by refill after
   the rows, freed before the rows are. */
static APTR   *row_img, *row_mark;
static Object **row_obj, **row_markobj;
static int     row_n;

/* The column of the list that holds each row's action. */
#define ACTION_COLUMN 3
static const char *const glyph_back[12] = {
    "............",
    ".....#......",
    "....##......",
    "...###......",
    "..##########",
    ".###########",
    ".###########",
    "..##########",
    "...###......",
    "....##......",
    ".....#......",
    "............",
};

static const char *shown_name(const pkg_entry *e)
{
    return e->name[0] ? e->name : e->id;
}

/* What the other columns of the list take, in pixels, worked out when the
   rows are made: the summary may use what is left of the list's width. */
static int row_other_width;
#define ROW_WIDEST_ACTION "Upgrade"
/* The width the summary column has, in pixels: what the list's width leaves
   after the other columns. The column is held to exactly this by its
   heading (pic_heading) and its texts are cut to it, so the button after it
   stands at the list's right edge whatever the summaries are. */
static int row_room, row_list_width;
static APTR row_head;
static Object *row_headobj;

/* How wide the rows of the package list are. A Zune list is a group of two:
   the area that shows the rows and its own scroller beside it, so the
   list's own width is not the rows' (measured on mainline: 576 against
   553). The area is the group's first member. 0 before the window is
   open. */
static int list_rows_width(void)
{
    struct List *kids = NULL;
    Object *state, *kid;
    if (!lst_list) return 0;
    GetAttr(MUIA_Group_ChildList, lst_list, (IPTR *)&kids);
    if (!kids) return 0;
    state = (Object *)kids->lh_Head;
    kid = NextObject(&state);
    return kid && _mwidth(kid) > 0 ? _mwidth(kid) : 0;
}

static void summary_fit(char *text)
{
    struct RastPort rp;
    struct TextExtent te;
    int room, len = (int)strlen(text), fit;
    if (!lst_list || !_font(lst_list)) return;
    room = row_room;
    if (room < 40) return;
    InitRastPort(&rp);
    SetFont(&rp, _font(lst_list));
    if (TextLength(&rp, (CONST_STRPTR)text, len) <= room) return;
    room -= TextLength(&rp, (CONST_STRPTR)"...", 3);
    fit = (int)TextFit(&rp, (CONST_STRPTR)text, len, &te, NULL, 1, room > 0 ? room : 1, 32767);
    if (fit < 0) fit = 0;
    if (fit + 4 < len) snprintf(text + fit, 4, "...");
}

AROS_UFH3S(void, display_func,
    AROS_UFHA(struct Hook *, hook,  A0),
    AROS_UFHA(char **,       array, A2),
    AROS_UFHA(pkg_entry *,   e,     A1))
{
    AROS_USERFUNC_INIT
    (void)hook;
    if (!e) {
        array[0] = (char *)""; array[1] = (char *)"\33bName";
        static char headimg[24];
        array[2] = (char *)"\33bSummary"; array[3] = (char *)"";
        if (row_head) { snprintf(headimg, sizeof headimg, "\33O[%08lx]", (unsigned long)(IPTR)row_head); array[2] = headimg; }
    } else {
        /* The hook is called for one row at a time and MUI draws it before
           the next, so one buffer of each kind serves every row. */
        static char img[24], mark[24], sum[260];
        const pkg_news_row *r = news_row(e);
        long at = (long)(e - rows.v);
        img[0] = mark[0] = 0;
        if (at >= 0 && at < row_n && row_img && row_img[at]) snprintf(img, sizeof img, "\33O[%08lx]", (unsigned long)(IPTR)row_img[at]);
        if (at >= 0 && at < row_n && row_mark && row_mark[at]) snprintf(mark, sizeof mark, "\33O[%08lx]", (unsigned long)(IPTR)row_mark[at]);
        array[0] = img;
        array[1] = (char *)shown_name(e);
        array[2] = e->summary;
        array[3] = mark;
        if (r) {
            /* In the two news views the summary starts with what kind of
               news it is. The date is when the catalogue first published
               it; an update has no date of its own, so it shows none. */
            const char *d = r->kind == PKG_NEWS_BUILD ? r->first_added :
                            r->kind == PKG_NEWS_UPDATED ? "" : r->app_first_added;
            snprintf(sum, sizeof sum, "\33b%s%s%s\33n  %s", r->kind == PKG_NEWS_BUILD ? "new build" :
                     r->kind == PKG_NEWS_UPDATED ? "updated" :
                     r->kind == PKG_NEWS_RECENT || news_view == ST_RECENT ? "added" :
                     r->returned ? "back" : "new",
                     d[0] ? " " : "", d, e->summary);
            array[2] = sum;
        } else if (!e->ours && !e->installed) {
            snprintf(sum, sizeof sum, "\33bother ABI\33n  %s", e->summary);
            array[2] = sum;
        }
        /* A list's columns are as wide as their widest text, so a long
           summary would push the button after it out of sight: the summary
           is cut to the room the list has for it now. */
        if (array[2] != sum) { snprintf(sum, sizeof sum, "%s", e->summary); array[2] = sum; }
        summary_fit(sum);
    }
    AROS_USERFUNC_EXIT
}
static struct Hook display_hook;

static void row_images_free(void)
{
    int i;
    if (row_head) DoMethod(lst_list, MUIM_List_DeleteImage, (IPTR)row_head);
    if (row_headobj) MUI_DisposeObject(row_headobj);
    row_head = NULL; row_headobj = NULL;
    for (i = 0; i < row_n; i++) {
        if (row_img && row_img[i]) DoMethod(lst_list, MUIM_List_DeleteImage, (IPTR)row_img[i]);
        if (row_obj && row_obj[i]) MUI_DisposeObject(row_obj[i]);
        if (row_mark && row_mark[i]) DoMethod(lst_list, MUIM_List_DeleteImage, (IPTR)row_mark[i]);
        if (row_markobj && row_markobj[i]) MUI_DisposeObject(row_markobj[i]);
    }
    free(row_img); free(row_obj); free(row_mark); free(row_markobj);
    row_img = row_mark = NULL; row_obj = row_markobj = NULL; row_n = 0;
}

static int passes_filters(const pkg_entry *e);

/* After `rows` is filled and before its entries are inserted. A row whose
   picture cannot be made simply has none. */
static void row_images_make(void)
{
    int i;
    if (rows.n <= 0) return;
    row_img = (APTR *)calloc((size_t)rows.n, sizeof *row_img);
    row_obj = (Object **)calloc((size_t)rows.n, sizeof *row_obj);
    row_mark = (APTR *)calloc((size_t)rows.n, sizeof *row_mark);
    row_markobj = (Object **)calloc((size_t)rows.n, sizeof *row_markobj);
    if (!row_img || !row_obj || !row_mark || !row_markobj) {
        free(row_img); free(row_obj); free(row_mark); free(row_markobj);
        row_img = row_mark = NULL; row_obj = row_markobj = NULL;
        return;
    }
    row_n = rows.n;
    for (i = 0; i < rows.n; i++) {
        char path[PKG_MAXPATH];
        Object *o;
        /* At the end of each row, what its main button would do, as a
           small button with that word. A click there does it
           (ID_ROWCLICK). */
        if (rows.v[i].installed || rows.v[i].ours) {
            const pkg_update_row *u = rows.v[i].installed ? pkg_updates_find(&upd, rows.v[i].id) : NULL;
            o = pic_label_button(!rows.v[i].installed ? "Install" : u && u->state == PKG_UPD_AVAILABLE ? "Upgrade" : "Run",
                                 ROW_WIDEST_ACTION);
        } else o = NULL;
        if (o) {
            row_mark[i] = (APTR)DoMethod(lst_list, MUIM_List_CreateImage, (IPTR)o, 0);
            if (row_mark[i]) row_markobj[i] = o; else MUI_DisposeObject(o);
        }
        /* One picture per application: a second build of the same id shows
           the same file, which is read again (a list has few of those). */
        if (!pkg_media_icon_small(root, rows.v[i].id, path, sizeof path)) continue;
        if (!(o = pic_new(PKG_ICON_SMALL_W, PKG_ICON_SMALL_H, 0, 1, 0))) continue;
        if (pic_set(o, path, NULL, 0) != 0) { MUI_DisposeObject(o); continue; }
        row_img[i] = (APTR)DoMethod(lst_list, MUIM_List_CreateImage, (IPTR)o, 0);
        if (row_img[i]) row_obj[i] = o; else MUI_DisposeObject(o);
    }
    row_room = 0;
    row_list_width = list_rows_width();
    if (_font(lst_list) && row_list_width > 0) {
        /* What the other columns take: the icon when any row has one, the
           widest name (or the word "Name"), the button in its frame, and
           the gaps and the bar between four columns. */
        /* From the rows the list will show: refill has the list measure
           its columns again once they are in (MUIM_List_Redraw), so each
           column is as wide as the widest cell now in it. */
        struct RastPort rp;
        int namew = 0, icons = 0, w;
        InitRastPort(&rp);
        SetFont(&rp, _font(lst_list));
        SetSoftStyle(&rp, FSF_BOLD, AskSoftStyle(&rp));
        w = TextLength(&rp, (CONST_STRPTR)"Name", 4);
        if (w > namew) namew = w;
        SetSoftStyle(&rp, FS_NORMAL, AskSoftStyle(&rp));
        for (i = 0; i < rows.n; i++) {
            if (!passes_filters(&rows.v[i])) continue;
            w = TextLength(&rp, (CONST_STRPTR)shown_name(&rows.v[i]), strlen(shown_name(&rows.v[i])));
            if (w > namew) namew = w;
            if (row_img[i]) icons = 1;
        }
        row_other_width = (icons ? PKG_ICON_SMALL_W : 0) + namew
                          + TextLength(&rp, (CONST_STRPTR)ROW_WIDEST_ACTION, strlen(ROW_WIDEST_ACTION)) + 10 + 40;
        row_room = row_list_width - row_other_width;
logline("list columns: rows %d wide, icons %d, name %d, other %d, summary %d", row_list_width, icons, namew, row_other_width, row_room);
        if (row_room >= 40 && (row_headobj = pic_heading("Summary", row_room)) != NULL) {
            row_head = (APTR)DoMethod(lst_list, MUIM_List_CreateImage, (IPTR)row_headobj, 0);
            if (!row_head) { MUI_DisposeObject(row_headobj); row_headobj = NULL; }
        }
    }
}

/* The main button of a package: the same frame as the others, with a bold
   label, the theme's highlight colour behind it and more of the row's
   width, so the eye finds what the package's state calls for. */
static Object *main_button(void)
{
    return TextObject,
        MUIA_Frame, MUIV_Frame_Button,
        MUIA_Background, MUII_FILL,
        MUIA_Font, MUIV_Font_Button,
        MUIA_Text_Contents, (IPTR)"Install",
        MUIA_Text_PreParse, (IPTR)"\33c\33b",
        MUIA_InputMode, MUIV_InputMode_RelVerify,
        MUIA_CycleChain, 1,
        MUIA_HorizWeight, 160,
    End;
}

/* A button with a small drawing on it and a word beside it. */
static Object *glyph_button(const char *const *rows_, int size, const char *label, const char *help)
{
    Object *glyph = pic_new(size, size, 0, 0, 0);
    pic_set_glyph(glyph, rows_, size, size);
    return HGroup,
        MUIA_Frame, MUIV_Frame_Button,
        MUIA_Background, MUII_ButtonBack,
        MUIA_InputMode, MUIV_InputMode_RelVerify,
        MUIA_CycleChain, 1,
        MUIA_HorizWeight, 0,
        MUIA_ShortHelp, (IPTR)help,
        Child, (VGroup, Child, VSpace(0), Child, glyph, Child, VSpace(0), End),
        label ? Child : TAG_IGNORE, label ? (IPTR)(TextObject,
            MUIA_Font, MUIV_Font_Button,
            MUIA_Text_Contents, (IPTR)label,
        End) : 0,
    End;
}

/* The package's name stands apart from the text under it: the screen's
   font, half as large again. Zune's own "big" font is whatever the user
   set, and on a system as installed that is the normal font, so the size
   is asked for here. Without it (a font that has no such size and cannot
   be scaled) the name is bold in the big font of the settings. */
static struct TextFont *title_font;

static void title_font_open(void)
{
    struct Screen *scr = LockPubScreen(NULL);
    if (scr) {
        if (scr->Height < 700)       { car_w = 352; car_h = 220; }
        else if (scr->Height >= 900) { car_w = PKG_SHOT_W; car_h = PKG_SHOT_H; }
    }
    if (scr && scr->Font && scr->Font->ta_Name) {
        struct TextAttr ta = *scr->Font;
        ta.ta_YSize = (UWORD)(ta.ta_YSize * 3 / 2);
        title_font = OpenDiskFont(&ta);
    }
    if (scr) UnlockPubScreen(NULL, scr);
}

static Object *name_text(void)
{
    return TextObject,
        MUIA_Font, title_font ? (IPTR)title_font : (IPTR)MUIV_Font_Big,
        MUIA_Text_Contents, (IPTR)"",
        MUIA_Text_PreParse, (IPTR)"\33b",
        MUIA_Text_SetMin, FALSE,
        MUIA_Text_Shorten, MUIV_Text_Shorten_ElideRight,
    End;
}

/* Under the name: what it is, in one line of the normal font. */
static Object *summary_text(void)
{
    return TextObject,
        MUIA_Text_Contents, (IPTR)"",
        MUIA_Text_SetMin, FALSE,
        MUIA_Text_Shorten, MUIV_Text_Shorten_ElideRight,
    End;
}

/* The status line as given: for what the links themselves report. */
static void status_plain(const char *text)
{
    lk_begin(LK_STATUS);
    lk_add_plain(LK_STATUS, text);
    lk_end(LK_STATUS);
}

/* A message with an address has its link in the sentence. */
static void status(const char *text)
{
    lk_show1(LK_STATUS, text);
}

static void confirm_text(const char *text)
{
    lk_show1(LK_CONFIRM, text);
}

static void show_err(const char *what, pkg_status st, const pkg_err *e)
{
    static char buf[640];
    lk_field f;
    /* As libpkg wrote it; the front does not re-phrase. The subject is a
       field: when it is an address (a download's), that address exactly,
       not what a reading of the sentence makes of it. */
    f.label = NULL;
    f.url = e->subject;
    if (st == PKG_OK) snprintf(buf, sizeof buf, "%s", what);
    else if (e->subject[0] && lk_exact(e->subject))
        snprintf(buf, sizeof buf, "%s: %s (\1)%s%s", what, e->summary, e->detail[0] ? " -- " : "", e->detail);
    else
        snprintf(buf, sizeof buf, "%s: %s%s%s%s%s%s", what, e->summary,
                 e->subject[0] ? " (" : "", e->subject, e->subject[0] ? ")" : "",
                 e->detail[0] ? " -- " : "", e->detail);
    lk_begin(LK_STATUS);
    lk_add(LK_STATUS, buf, &f, 1);
    lk_end(LK_STATUS);
}

/* ---------------------------------------------------------------- filters */

/* The category cycle is built from what the index holds -- the first path
 * component of each category -- so it never offers a choice with nothing in
 * it. Rebuilt on every refill; the chosen name is kept across rebuilds. */
#define MAXCAT 32
static char  cat_names[MAXCAT][32];
static int   cat_count;
static char  cat_chosen[32];
/* The third entry is relabelled with the count on every refill. */
static char upd_label[32] = "Updates";
/* The views at the top of the side list. "Recently added" and "Last
   refresh" list the shared news model (pkg_news in pkg.h), the same answers
   as apkg recent: the first newest first, the second by kind (new, new
   builds, updated), as apkg lists it. Under them, after an empty row, the
   categories. One row is chosen at a time: a view of everything, or one
   category. "Installed only" beside the list's name narrows either. */
static STRPTR state_entries[] = { (STRPTR)"All applications", (STRPTR)"Installed", (STRPTR)upd_label,
                                  (STRPTR)"Recently added", (STRPTR)"Last refresh", NULL };
#define SIDE_VIEWS 5
#define SIDE_FIRST_CAT (SIDE_VIEWS + 1)
static int view_state;            /* ST_*: the view chosen; ST_ALL while a category is */
/* "Installed only", beside the list's name: it narrows whatever the side
   list has chosen, so a category can be seen as what is installed of it. */
static int only_installed;
static Object *chk_inst;
static APTR side_badge;           /* the list image of the update count, or NULL */
static Object *side_badgeobj;
/* How many of the newest additions "Recently added" lists. */
#define RECENT_ROWS 25

static IPTR state_now(void)
{
    return (IPTR)view_state;
}

static void top_category(const char *cat, char *out, size_t n)
{
    size_t k = strcspn(cat, "/");
    if (k >= n) k = n - 1;
    memcpy(out, cat, k); out[k] = 0;
    if (!out[0]) snprintf(out, n, "%s", "(none)");
}

static void rebuild_categories(const pkg_entries *all)
{
    static char title[48];
    int i, n = 0, active = view_state;
    for (i = 0; i < all->n && n < MAXCAT; i++) {
        char top[32]; int j, seen = 0;
        top_category(all->v[i].category, top, sizeof top);
        for (j = 0; j < n; j++) if (strcmp(cat_names[j], top) == 0) { seen = 1; break; }
        if (!seen) snprintf(cat_names[n++], sizeof cat_names[0], "%s", top);
    }
    /* Alphabetical, so the list reads the same whatever the index order. */
    for (i = 1; i < n; i++) {
        char tmp[32]; int j = i;
        snprintf(tmp, sizeof tmp, "%s", cat_names[i]);
        while (j > 0 && strcmp(cat_names[j - 1], tmp) > 0) { memcpy(cat_names[j], cat_names[j - 1], sizeof tmp); j--; }
        memcpy(cat_names[j], tmp, sizeof tmp);
    }
    cat_count = n;
    SetAttrs(lst_side, MUIA_List_Quiet, TRUE, TAG_DONE);
    DoMethod(lst_side, MUIM_List_Clear);
    /* The count of updates stands beside "Updates" as a badge, when there
       are any: made anew with every list, like the rows' icons. */
    if (side_badge) DoMethod(lst_sidelist, MUIM_List_DeleteImage, (IPTR)side_badge);
    if (side_badgeobj) MUI_DisposeObject(side_badgeobj);
    side_badge = NULL; side_badgeobj = NULL;
    snprintf(upd_label, sizeof upd_label, "Updates");
    if (upd.catalogue == PKG_CAT_OK && upd.newer) {
        char num[12];
        snprintf(num, sizeof num, "%d", upd.newer);
        if ((side_badgeobj = pic_badge(num)) != NULL)
            side_badge = (APTR)DoMethod(lst_sidelist, MUIM_List_CreateImage, (IPTR)side_badgeobj, 0);
        if (side_badge) snprintf(upd_label, sizeof upd_label, "Updates  \33O[%08lx]", (unsigned long)(IPTR)side_badge);
        else {
            if (side_badgeobj) MUI_DisposeObject(side_badgeobj);
            side_badgeobj = NULL;
            snprintf(upd_label, sizeof upd_label, "Updates (%d)", upd.newer);
        }
    }
    for (i = 0; i < SIDE_VIEWS; i++) DoMethod(lst_side, MUIM_List_InsertSingle, (IPTR)state_entries[i], MUIV_List_Insert_Bottom);
    DoMethod(lst_side, MUIM_List_InsertSingle, (IPTR)"", MUIV_List_Insert_Bottom);
    for (i = 0; i < n; i++) {
        DoMethod(lst_side, MUIM_List_InsertSingle, (IPTR)cat_names[i], MUIV_List_Insert_Bottom);
        if (cat_chosen[0] && strcmp(cat_names[i], cat_chosen) == 0) active = SIDE_FIRST_CAT + i;
    }
    if (cat_chosen[0] && active < SIDE_FIRST_CAT) cat_chosen[0] = 0;
    SetAttrs(lst_side, MUIA_List_Quiet, FALSE, TAG_DONE);
    /* MUIA_NoNotify, because setting the active entry here would otherwise
       fire the same notification a user's choice does, which calls refill,
       which comes back here: measured as a few hundred "filter" events per
       second until the window was closed. The user's choice is the only
       one that should refill. */
    SetAttrs(lst_side, MUIA_NoNotify, TRUE, MUIA_List_Active, active, TAG_DONE);
    /* "Updates" by name: its row's text also carries the badge. */
    snprintf(title, sizeof title, "%s", cat_chosen[0] ? cat_chosen :
             view_state == ST_UPDATES ? "Updates" : (const char *)state_entries[view_state]);
    if (cat_chosen[0] && title[0] >= 'a' && title[0] <= 'z') title[0] = (char)(title[0] - 'a' + 'A');
    SetAttrs(txt_view, MUIA_Text_Contents, (IPTR)title, TAG_DONE);
}

static int passes_filters(const pkg_entry *e)
{
    IPTR st = state_now();
    if (only_installed && !e->installed) return 0;
    if (cat_chosen[0]) {
        char top[32];
        top_category(e->category, top, sizeof top);
        if (strcmp(top, cat_chosen) != 0) return 0;
    }
    if (st == ST_RECENT || st == ST_LAST) return news_row(e) != NULL;
    if (st == ST_INSTALLED && !e->installed) return 0;
    if (st == ST_UPDATES) {
        const pkg_update_row *u = e->installed ? pkg_updates_find(&upd, e->id) : NULL;
        if (!u || !u->newer) return 0;
    }
    return 1;
}

static int passes_filters_is_updates(void)
{
    return state_now() == ST_UPDATES;
}

/* ------------------------------------------------------------------ rows */

/* How old the catalogue is, and the update count, or the reason there is
 * no count: the first part of the status line under the list. A missing or
 * unreadable catalogue is said as such, never shown as "0 updates". */
static char cat_line[300];
static void show_catalogue(void)
{
    IPTR st = 0;
    char *line = cat_line;
    size_t n = sizeof cat_line;
    if (upd.catalogue == PKG_CAT_MISSING)
        snprintf(line, n, "\33bNo catalogue yet\33n -- choose Update index in the Catalogue menu.");
    else if (upd.catalogue == PKG_CAT_UNREADABLE)
        snprintf(line, n, "\33bThe catalogue cannot be read\33n (%s) -- choose Update index in the Catalogue menu.", upd.catalogue_error);
    else {
        size_t k = (size_t)snprintf(line, n, "Catalogue of %s%s. ",
                                    upd.catalogue_when[0] ? upd.catalogue_when : "(date unknown)",
                                    upd.catalogue_stale ? " \33b(more than two weeks ago)\33n" : "");
        if (!upd.installed) snprintf(line + k, n - k, "Nothing installed.");
        else if (!upd.newer) snprintf(line + k, n - k, "Installed packages are up to date.");
        else if (!upd.ready)
            snprintf(line + k, n - k, "\33b%d update%s\33n, none installable now (see the package's Details).",
                     upd.newer, upd.newer == 1 ? "" : "s");
        else if (upd.blocked)
            snprintf(line + k, n - k, "\33b%d update%s available\33n (%d of them blocked).",
                     upd.newer, upd.newer == 1 ? "" : "s", upd.blocked);
        else
            snprintf(line + k, n - k, "\33b%d update%s available\33n: choose Updates above.", upd.newer, upd.newer == 1 ? "" : "s");
    }
    (void)st;
}

/* The one status line: the catalogue's state, then what the list shows. */
static void status_list(const char *msg)
{
    static char both[560];
    snprintf(both, sizeof both, "%s  %s", cat_line, msg);
    status(both);
}

static void show_detail(void);
static int no_catalogue;          /* the rows are the registry's alone: why (a pkg_status) */
static pkg_err nocat_err;

/* The news for the view in front, from the same calls as apkg recent and
   apkg recent --since-last-update. A last refresh that does not describe
   the catalogue in use lists nothing: its rows belong to another one. */
static pkg_status news_st;
static pkg_err news_err;
static void news_fill(pkg_ctx *c, int have_catalogue)
{
    IPTR stv = state_now();
    pkg_news_free(&news);
    memset(&news, 0, sizeof news);
    news_view = 0; news_st = PKG_OK;
    if (!have_catalogue || (stv != ST_RECENT && stv != ST_LAST)) return;
    memset(&news_err, 0, sizeof news_err);
    if (stv == ST_RECENT) {
        pkg_recent_opts o;
        memset(&o, 0, sizeof o);
        o.limit = RECENT_ROWS; o.builds = 1;
        news_st = pkg_recent(c, index_path, &o, &news, &news_err);
    } else news_st = pkg_news_last(c, index_path, &news, &news_err);
    if (news_st == PKG_OK && (stv == ST_RECENT || (news.recorded && news.current))) news_view = (int)stv;
}

/* The line under the list for the two news views. Missing dates or a
   missing record are said as such, never as "nothing new". */
static void news_status(char *msg, size_t n, int shown)
{
    if (news_st != PKG_OK) { snprintf(msg, n, "%s", news_err.summary); return; }
    if (state_now() == ST_LAST) {
        if (!news.recorded) snprintf(msg, n, "no catalogue update recorded on this machine yet -- choose Update index in the Catalogue menu");
        else if (!news.current) snprintf(msg, n, "the catalogue was replaced since the last recorded update -- choose Update index in the Catalogue menu");
        else if (news.first) snprintf(msg, n, "update of %s: %s Recently added lists the newest.", news.when, news.first_why);
        else if (!news.n) snprintf(msg, n, "update of %s: nothing new for this machine", news.when);
        else snprintf(msg, n, "update of %s: %d new, %d new build%s, %d updated%s", news.when, news.apps,
                      news.builds, news.builds == 1 ? "" : "s", news.updated, shown < news.n ? " (some filtered out)" : "");
        return;
    }
    if (!news.n && news.unknown_dates)
        snprintf(msg, n, "the catalogue gives no dates for its applications -- choose Update index in the Catalogue menu");
    else if (!news.n) snprintf(msg, n, "no dated applications for this machine");
    else {
        size_t k = (size_t)snprintf(msg, n, "the %d most recently added for this machine, newest first", shown);
        if (news.unknown_dates && k < n)
            snprintf(msg + k, n - k, "; %d without a date not listed", news.unknown_dates);
    }
}

/* Re-read the index and the registry, rebuild the category cycle, and show
 * the rows that pass the filters. The old rows stay alive until the list
 * has let go of them. Four outcomes are told apart on purpose: no index yet,
 * an index that cannot be read, an index with nothing in it, and filters
 * that leave nothing -- a window that says "0 packages" for all four sends
 * the user to the wrong fix. */
static void refill(const char *term, const char *keep_id, const char *why)
{
    pkg_ctx *c = NULL; pkg_err e; pkg_status st;
    pkg_entries fresh, all = {0}; int hidden = 0, i, k, again = -1, shown = 0, inst = 0;
    char msg[200];

    memset(&e, 0, sizeof e);
    st = pkg_open(root, PKG_OPEN_READ, &c, &e);
    if (st != PKG_OK) { show_err("cannot open the package root", st, &e); return; }
    st = pkg_query(c, index_path, term, &fresh, &hidden, &e);
    {
        pkg_err e3; memset(&e3, 0, sizeof e3);
        pkg_updates_free(&upd);
        if (pkg_updates_get(c, index_path, 1, &upd, &e3) != PKG_OK) {
            /* Zeroed it would read as an OK catalogue with nothing installed. */
            memset(&upd, 0, sizeof upd);
            upd.catalogue = PKG_CAT_UNREADABLE;
            snprintf(upd.catalogue_error, sizeof upd.catalogue_error, "%s", e3.summary);
        }
    }
    /* The category cycle lists what the whole index holds, not what the
       current search left: a search with no hits used to empty the cycle
       and forget the chosen category with it. */
    if (st == PKG_OK && term && *term) {
        pkg_err e2; memset(&e2, 0, sizeof e2);
        if (pkg_query(c, index_path, NULL, &all, NULL, &e2) != PKG_OK) all.n = 0;
    }
    /* Without a readable catalogue the machine still has what is installed,
       and Run, Open folder, Verify, Remove and Roll back need no catalogue:
       the rows come from the registry alone. */
    no_catalogue = 0;
    if (st != PKG_OK) {
        pkg_err e2; memset(&e2, 0, sizeof e2);
        if (pkg_installed(c, NULL, &fresh, &e2) == PKG_OK) {
            no_catalogue = st;
            nocat_err = e;
            st = PKG_OK;
            if (term && *term) {
                /* The search still applies, over the ids. */
                int a, b = 0;
                for (a = 0; a < fresh.n; a++)
                    if (strstr(fresh.v[a].id, term)) fresh.v[b++] = fresh.v[a];
                fresh.n = b;
            }
        }
    }
    news_fill(c, st == PKG_OK && !no_catalogue);
    pkg_close(c);

    show_catalogue();
    SetAttrs(lst, MUIA_List_Quiet, TRUE, TAG_DONE);
    DoMethod(lst, MUIM_List_Clear);
    row_images_free();
    pkg_entries_free(&rows);
    if (st == PKG_OK) {
        rows = fresh;
        row_images_make();
        rebuild_categories(all.n ? &all : &rows);
        pkg_entries_free(&all);
        for (i = 0; i < rows.n; i++) {
            if (rows.v[i].installed) inst++;
            /* The news views list in the news model's order, newest first. */
            if (news_view || !passes_filters(&rows.v[i])) continue;
            DoMethod(lst, MUIM_List_InsertSingle, (IPTR)&rows.v[i], MUIV_List_Insert_Bottom);
            if (keep_id && strcmp(rows.v[i].id, keep_id) == 0) again = shown;
            shown++;
        }
        for (k = 0; news_view && k < news.n; k++)
            for (i = 0; i < rows.n; i++) {
                if (strcmp(rows.v[i].id, news.rows[k].id) || strcmp(rows.v[i].arch, news.rows[k].arch)
                    || strcmp(rows.v[i].abi, news.rows[k].abi) || !passes_filters(&rows.v[i])) continue;
                DoMethod(lst, MUIM_List_InsertSingle, (IPTR)&rows.v[i], MUIV_List_Insert_Bottom);
                if (keep_id && strcmp(rows.v[i].id, keep_id) == 0) again = shown;
                shown++;
                break;
            }
        logline("refill (%s) term=\"%s\" rows=%d shown=%d installed=%d keep=%s updates=%d ready=%d blocked=%d catalogue=%s when=%s",
                why, term ? term : "", rows.n, shown, inst, keep_id ? keep_id : "-",
                upd.newer, upd.ready, upd.blocked, pkg_cat_word(upd.catalogue), upd.catalogue_when);
        if (no_catalogue)
            snprintf(msg, sizeof msg, "%d installed package%s shown; %s", shown, shown == 1 ? "" : "s",
                     no_catalogue == PKG_E_NOT_FOUND ? "no catalogue yet -- choose Update index in the Catalogue menu"
                                                     : "the catalogue cannot be read -- choose Update index in the Catalogue menu");
        else if (rows.n == 0 && hidden == 0 && !(term && *term))
            snprintf(msg, sizeof msg, "the index lists no packages");
        else if (state_now() == ST_RECENT || state_now() == ST_LAST)
            news_status(msg, sizeof msg, shown);
        else if (shown == 0 && passes_filters_is_updates())
            snprintf(msg, sizeof msg, "%s", upd.installed ? "no updates for the installed packages" : "nothing is installed");
        else if (shown == 0)
            snprintf(msg, sizeof msg, "no %spackages match%s%s%s", only_installed ? "installed " : "",
                     term && *term ? " \"" : "", term && *term ? term : "", term && *term ? "\"" : "");
        else
            snprintf(msg, sizeof msg, "%d package%s%s%s%s", shown, shown == 1 ? "" : "s",
                     shown < rows.n ? " shown" : "",
                     term && *term ? " matching " : "", term && *term ? term : "");
        if (hidden) {
            size_t n = strlen(msg);
            snprintf(msg + n, sizeof msg - n, "; %d hidden (not for this machine)", hidden);
        }
        status_list(msg);
    } else if (st == PKG_E_NOT_FOUND) {
        logline("refill (%s): no index", why);
        status("no index yet -- choose Update index in the Catalogue menu");
    } else {
        logline("refill (%s): index unreadable: %s", why, e.summary);
        show_err("the index cannot be read", st, &e);
    }
    SetAttrs(lst, MUIA_List_Quiet, FALSE, TAG_DONE);
    /* A list keeps a column's width when its rows are replaced by narrower
       ones (seen on mainline: the button column stayed pushed out after the
       icons arrived and the summary column was to be narrower). Redrawing
       all of it measures every column again. */
    DoMethod(lst_list, MUIM_List_Redraw, MUIV_List_Redraw_All);
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

/* ------------------------------------------------------------ pictures */

/* What the media drawer holds for the selected package (libpkg/media.h),
 * read on this task: local files only. The catalogue's gallery is fetched
 * by the worker the first time the package's page is opened. Every place a
 * picture goes has a fixed size, so a missing one changes no layout. */
static pkg_media media_now;
static char media_id[PKG_MAXID];
static int shot_now;
static char caption[200];

static void show_shot(int i)
{
    if (i < 0 || i >= media_now.nshots) return;
    shot_now = i;
    /* The strip is moved so that this picture starts at its left edge; the
       last ones stop where the strip ends. */
    SetAttrs(virt_car, MUIA_Virtgroup_Left, i * (car_w + CAR_GAP), TAG_DONE);
    snprintf(caption, sizeof caption, "\33c%s%s(%d of %d)", media_now.shots[i].caption,
             media_now.shots[i].caption[0] ? "  " : "", i + 1, media_now.nshots);
    SetAttrs(txt_caption, MUIA_Text_Contents, (IPTR)caption, TAG_DONE);
    SetAttrs(btn_prev, MUIA_Disabled, i == 0, TAG_DONE);
    SetAttrs(btn_next, MUIA_Disabled, i + 1 >= media_now.nshots, TAG_DONE);
}

/* `icon_file`: the installed program's own icon when it is a PNG icon this
   window can draw, else the one in the media drawer. */
static void show_media(const char *id, const char *icon_file)
{
    const char *icon = "";
    int i;
    pkg_media_get(root, no_catalogue ? NULL : index_path, id, &media_now);
    snprintf(media_id, sizeof media_id, "%s", id);
    if (icon_file && icon_file[0] && pkg_png_file(icon_file, PKG_ICON_W, PKG_ICON_H, NULL, 0) == 0) icon = icon_file;
    else if (media_now.icon[0]) icon = media_now.icon;
    if (pic_set(pic_icon2, icon, NULL, 0) != 0 && icon == icon_file && media_now.icon[0])
        /* The program's icon is a PNG this window does not draw (a palette,
           say): the drawer's, made by the catalogue, is. */
        pic_set(pic_icon2, media_now.icon, NULL, 0);
    /* The strip holds the pictures there are; without any it is not in
       the page at all and the text below has the room. */
    DoMethod(virt_car, MUIM_Group_InitChange);
    for (i = 0; i < PKG_MEDIA_SHOTS; i++) {
        int on = i < media_now.nshots && pic_set(pic_car[i], media_now.shots[i].file, NULL, 0) == 0;
        if (!on) pic_set(pic_car[i], "", NULL, 0);
        SetAttrs(pic_car[i], MUIA_ShowMe, on, TAG_DONE);
    }
    DoMethod(virt_car, MUIM_Group_ExitChange);
    SetAttrs(grp_car, MUIA_ShowMe, media_now.nshots > 0, TAG_DONE);
    SetAttrs(grp_carnav, MUIA_ShowMe, media_now.nshots > 0, TAG_DONE);
    if (media_now.nshots) show_shot(shot_now < media_now.nshots ? shot_now : 0);
}

static void no_media(void)
{
    memset(&media_now, 0, sizeof media_now);
    media_id[0] = 0;
    pic_set(pic_icon2, "", NULL, 0);
    SetAttrs(grp_car, MUIA_ShowMe, FALSE, TAG_DONE);
    SetAttrs(grp_carnav, MUIA_ShowMe, FALSE, TAG_DONE);
}

/* The buttons of the selected package, in both places they stand: under the
 * list and on the package's page. One main button does what the package's
 * state calls for -- Upgrade when a newer revision can be installed, else
 * Run, else Install -- and the menu has each of them by name. */
static int main_action;           /* ID_UPGRADE, ID_RUN, ID_INSTALL or 0 */
static struct { int install, run, upgrade, rollback, verify, open, remove; } can;

static void show_actions(int off)
{
    const char *label = can.upgrade ? "Upgrade" : can.run ? "Run" : can.install ? "Install" :
                        selected() && selected()->installed ? "Run" : "Install";
    main_action = can.upgrade ? ID_UPGRADE : can.run ? ID_RUN : can.install ? ID_INSTALL : 0;
    /* The page's header is laid out again when its buttons change: a
       button that appears, or a longer word on the main one, makes the
       column of buttons wider, and without this it kept its old width and
       the new buttons stood partly outside the window. */
    {
        static int was_open = -1, was_remove = -1;
        static const char *was_label;
        int change = was_open != !!can.open || was_remove != !!can.remove || was_label != label;
        if (change) DoMethod(grp_head, MUIM_Group_InitChange);
        SetAttrs(btn_main2, MUIA_Text_Contents, (IPTR)label, MUIA_Disabled, off || !main_action, TAG_DONE);
        /* Open folder and Remove are there only for an installed package. */
        SetAttrs(btn_open2,   MUIA_ShowMe, can.open, MUIA_Disabled, off, TAG_DONE);
        SetAttrs(btn_remove2, MUIA_ShowMe, can.remove, MUIA_Disabled, off, TAG_DONE);
        if (change) DoMethod(grp_head, MUIM_Group_ExitChange);
        was_open = !!can.open; was_remove = !!can.remove; was_label = label;
    }
    SetAttrs(btn_open,    MUIA_ShowMe, can.open, MUIA_Disabled, off, TAG_DONE);
    SetAttrs(btn_details, MUIA_Disabled, !selected(), TAG_DONE);
    DoMethod(menus, MUIM_SetUData, ID_DETAILS,  MUIA_Menuitem_Enabled, selected() != NULL);
    DoMethod(menus, MUIM_SetUData, ID_INSTALL,  MUIA_Menuitem_Enabled, !off && can.install);
    DoMethod(menus, MUIM_SetUData, ID_RUN,      MUIA_Menuitem_Enabled, !off && can.run);
    DoMethod(menus, MUIM_SetUData, ID_UPGRADE,  MUIA_Menuitem_Enabled, !off && can.upgrade);
    DoMethod(menus, MUIM_SetUData, ID_ROLLBACK, MUIA_Menuitem_Enabled, !off && can.rollback);
    DoMethod(menus, MUIM_SetUData, ID_VERIFY,   MUIA_Menuitem_Enabled, !off && can.verify);
    DoMethod(menus, MUIM_SetUData, ID_OPEN,     MUIA_Menuitem_Enabled, !off && can.open);
    DoMethod(menus, MUIM_SetUData, ID_REMOVE,   MUIA_Menuitem_Enabled, !off && can.remove);
    DoMethod(menus, MUIM_SetUData, ID_UPDATE,   MUIA_Menuitem_Enabled, !off);

}


/* The page of the selected package, in the place of the list. Opening it
   is what asks for the catalogue's pictures of that package, once. */
static const char *page_titles[] = { "About", "Details", NULL };
static rx_job *begin(job_kind kind, const char *pkg, const char *origin);

static void page_open(int tab)
{
    pkg_entry *e = selected();
    if (!e) return;
    if (tab >= 0) SetAttrs(reg_page, MUIA_Group_ActivePage, tab, TAG_DONE);
    SetAttrs(pg_main, MUIA_Group_ActivePage, 1, TAG_DONE);
    /* Its buttons may have changed while the page was not shown. */
    DoMethod(grp_head, MUIM_Group_InitChange);
    DoMethod(grp_head, MUIM_Group_ExitChange);
    if (media_now.nshots) show_shot(shot_now < media_now.nshots ? shot_now : 0);
    logline("page of %s opened, tab %d, pictures %d%s", e->id, tab, media_now.nshots, media_now.due ? ", gallery due" : "");
    if (media_now.due && !busy_now() && !no_auto) begin(JOB_MEDIA, e->id, "page");
}

static void page_close(void)
{
    SetAttrs(pg_main, MUIA_Group_ActivePage, 0, TAG_DONE);
}

/* The last Verify, kept for the package it was made for and shown in its
   detail pane until another package is verified or anything is changed. */
static struct { char id[PKG_MAXID]; char text[2048]; } last_verify;

static void verify_store(const char *id, pkg_status st, const pkg_verify_result *v,
                         const char *listing, const char *why)
{
    size_t k;
    snprintf(last_verify.id, sizeof last_verify.id, "%s", id);
    if (st != PKG_OK) {
        snprintf(last_verify.text, sizeof last_verify.text, "\33bVerify:\33n could not check -- %s\n", why);
        return;
    }
    k = (size_t)snprintf(last_verify.text, sizeof last_verify.text,
                         "\33bVerify:\33n %s\n%d file%s as installed, %d changed locally, %d missing, "
                         "%d in the drawer that are not the package's; drawer icon: %s\n",
                         v->changed || v->missing ? "differences found" : "every file is as installed",
                         v->ok, v->ok == 1 ? "" : "s", v->changed, v->missing, v->unknown, v->icon);
    /* Only the lines that need attention; "ok" lines would bury them. */
    if (listing) {
        const char *p = listing;
        int shown = 0;
        while (*p && k + 80 < sizeof last_verify.text) {
            size_t n = strcspn(p, "\n");
            if (strncmp(p, "ok", 2) != 0 && shown < 12) {
                k += (size_t)snprintf(last_verify.text + k, sizeof last_verify.text - k, "  %.*s\n", (int)n, p);
                shown++;
            }
            p += n; if (*p) p++;
        }
    }
}

/* What a person reads before pressing the main button, in three texts:
 * under the list, the few lines that decide (what it is, its state, what
 * needs attention, the package's notes); on the package's page, "About"
 * with the notes in full and what it needs or replaces, and "Details" with
 * what only matters when something is wrong: where it is, how it starts,
 * what this machine was probed for, the catalogue's checks. All three come
 * from the same calls `apkg show` makes. */
static void show_detail(void)
{
    static char tech[6144], head[1200], about[4096], phead[700];
    lk_field fields[2];                  /* source and catalogue: addresses as they stand */
    char icon_file[PKG_MAXPATH] = "", attention[400] = "", startline[200] = "", ver[80], size[32];
    char reqs[1024] = "";
    const char *notes = "", *state;
    pkg_entry *e = selected();
    pkg_details det;
    size_t k;

    memset(&can, 0, sizeof can);
    if (!e) {
        SetAttrs(txt_name,  MUIA_Text_Contents, (IPTR)"", TAG_DONE);
        SetAttrs(txt_name2, MUIA_Text_Contents, (IPTR)"", TAG_DONE);
        SetAttrs(txt_facts, MUIA_Text_Contents, (IPTR)"", TAG_DONE);
        SetAttrs(txt_sum2,  MUIA_Text_Contents, (IPTR)"", TAG_DONE);
        SetAttrs(txt_facts2, MUIA_Text_Contents, (IPTR)"\n", TAG_DONE);
        lk_show1(LK_PKGABOUT, "");
        lk_show1(LK_DETAIL, "");
        no_media();
        show_actions(busy_now());
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
    /* The version named is what is ON THIS MACHINE when the package is
       installed -- the registry's version and revision -- and the
       catalogue's otherwise. The revision is shown only when one is stated:
       "2.2" is upstream's string and must appear exactly as written. */
    rev_label(ver, sizeof ver, e->installed ? e->installed_version : e->version,
              e->installed ? e->installed_revision : e->revision);
    state = e->installed ? "installed" : (e->ours ? "not installed" : "other ABI");
    if (e->size >= 1024 * 1024) snprintf(size, sizeof size, " \267 %ld.%ld MB", e->size / (1024 * 1024), (e->size % (1024 * 1024)) * 10 / (1024 * 1024));
    else if (e->size > 0) snprintf(size, sizeof size, " \267 %ld KB", (e->size + 1023) / 1024);
    else size[0] = 0;

    tech[0] = 0; k = 0;
    /* Where it is, from its registry entry: what Open folder opens. */
    if (e->installed && det.installed_dir[0])
        k += (size_t)snprintf(tech + k, sizeof tech - k, "location: %s\n", det.installed_dir);
    /* Whether Run can start it, from the same call as `apkg run --check`:
       the drawer's own description, checked on the disk now. */
    if (e->installed) {
        pkg_app_info ai;
        pkg_ctx *c = NULL; pkg_err err; memset(&err, 0, sizeof err);
        memset(&ai, 0, sizeof ai);
        if (pkg_open(root, PKG_OPEN_READ, &c, &err) == PKG_OK) {
            if (pkg_app_info_get(c, e->id, &ai, &err) == PKG_OK) {
                can.run = ai.state == PKG_RUN_READY;
                if (ai.program[0]) snprintf(icon_file, sizeof icon_file, "%s.info", ai.program);
                if (can.run && ai.launch.mode == PKG_LAUNCH_WORKBENCH && ai.stack > 0)
                    snprintf(startline, sizeof startline, "start: from its icon, stack %ld bytes", ai.stack);
                else if (can.run && ai.launch.mode == PKG_LAUNCH_WORKBENCH)
                    snprintf(startline, sizeof startline, "start: from its icon");
                else if (can.run)
                    snprintf(startline, sizeof startline, "start: as a Shell command, in a window of its own");
                else if (ai.state == PKG_RUN_NO_DESCRIPTION)
                    snprintf(startline, sizeof startline, "start: not described by the package -- use Open folder");
                else {
                    snprintf(startline, sizeof startline, "\33bcannot start:\33n %s", ai.why);
                    snprintf(attention, sizeof attention, "\33bCannot start:\33n %s", ai.why);
                }
                logline("detail %s: run %s%s%s", e->id, pkg_run_word(ai.state), can.run ? "" : " why=", can.run ? "" : ai.why);
            }
            pkg_close(c);
        }
    } else if (det.in_index && det.launch.mode != PKG_LAUNCH_NONE)
        snprintf(startline, sizeof startline, "start: %s, once installed",
                 det.launch.mode == PKG_LAUNCH_WORKBENCH ? "from its icon" : "as a Shell command");
    if (startline[0] && k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "%s\n", startline);
    {
        /* The two questions the buttons need, answered by the library. */
        char why[240];
        if (e->installed) {
            /* The update state from the shared model: the same answer as
               the list, apkg outdated and the ScreenBar widget. */
            const pkg_update_row *u = pkg_updates_find(&upd, e->id);
            char a[80], b[80];
            can.upgrade = u && u->state == PKG_UPD_AVAILABLE;
            if (!u) snprintf(why, sizeof why, "not known");
            else snprintf(why, sizeof why, "%s", u->why);
            if (u) {
                rev_label(a, sizeof a, u->installed_version, u->installed_revision);
                if (u->available_version[0]) rev_label(b, sizeof b, u->available_version, u->available_revision);
                else snprintf(b, sizeof b, "-");
                if (u->newer) {
                    snprintf(attention, sizeof attention, "\33bUpdate %s:\33n %s -> %s%s%s",
                             can.upgrade ? "available" : "blocked", a, b, can.upgrade ? "" : " -- ", can.upgrade ? "" : u->why);
                    if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "update %s: %s -> %s%s%s\n",
                             can.upgrade ? "available" : "blocked", a, b, can.upgrade ? "" : " -- ", can.upgrade ? "" : u->why);
                } else if (u->state != PKG_UPD_CURRENT && k < sizeof tech)
                    k += (size_t)snprintf(tech + k, sizeof tech - k, "installed %s, catalogue %s -- %s\n", a, b, u->why);
            }
            {
                pkg_ctx *c = NULL; pkg_err err; memset(&err, 0, sizeof err);
                if (pkg_open(root, PKG_OPEN_READ, &c, &err) == PKG_OK) {
                    can.rollback = pkg_can_rollback(c, e->id, why, sizeof why);
                    pkg_close(c);
                }
                /* An upgrade happened but the way back is gone: say so
                   rather than leave the greyed menu item unexplained. */
                if (!can.rollback && strstr(why, "archive") && k < sizeof tech)
                    k += (size_t)snprintf(tech + k, sizeof tech - k, "rollback unavailable: %s\n", why);
            }
            {
                char a[80], b[80];
                rev_label(a, sizeof a, e->installed_version, e->installed_revision);
                rev_label(b, sizeof b, e->version, e->revision);
                logline("detail %s: installed %s, index %s, can_upgrade=%d can_rollback=%d%s%s",
                        e->id, a, b, can.upgrade, can.rollback, (!can.upgrade || !can.rollback) ? " why=" : "",
                        (!can.upgrade || !can.rollback) ? why : "");
            }
        }
    }
    if (k < sizeof tech)
        k += (size_t)snprintf(tech + k, sizeof tech - k, "%s   %s/%s   %s   id %s\n",
                              e->category, e->arch, e->abi[0] ? e->abi : "?", e->kind, e->id);
    if (det.compat == PKG_COMPAT_NATIVE) {
        if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "compatibility: native\n");
    } else {
        if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "\33bcompatibility: %s\33n -- %s\n",
                                                   pkg_compat_word(det.compat), det.compat_why);
        if (!attention[0] || !e->installed)
            snprintf(attention, sizeof attention, "\33bCompatibility: %s\33n -- %s", pkg_compat_word(det.compat), det.compat_why);
    }
    if (det.compat != PKG_COMPAT_NATIVE) {
        if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "requirements: not checked\n");
    } else if (e->requires[0]) {
        if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "requirements: %s, probed now:\n%s%s", det.requirements,
                                                   reqs[0] ? reqs : e->requires, reqs[0] && reqs[strlen(reqs) - 1] == '\n' ? "" : "\n");
        /* Said under the list only when this machine lacks something. */
        if (strcmp(det.requirements, "satisfied") != 0 && !e->installed)
            snprintf(attention, sizeof attention, "\33bRequirements: %s\33n -- see Details", det.requirements);
    } else if (k < sizeof tech) k += (size_t)snprintf(tech + k, sizeof tech - k, "requirements: none stated\n");

    /* The package's notes: the catalogue's current text, or the copy kept
       at installation when the catalogue no longer has it. They are plain
       printable ASCII (the parser refuses anything else), so they cannot
       carry the text views' escape codes. */
    if (det.in_index && det.notes_bad) notes = "(notes not shown: they are not plain lines this version can display)";
    else notes = det.in_index ? det.notes : det.notes_recorded;

    about[0] = 0; k = 0;
    if (notes[0] && k < sizeof about)
        k += (size_t)snprintf(about + k, sizeof about - k, "\33b%s\33n\n%s%s",
                              det.in_index || !det.notes_recorded[0] ? "Notes from the package" : "Notes recorded at installation",
                              notes, notes[strlen(notes) - 1] == '\n' ? "" : "\n");
    {
        /* What it needs, recommends, conflicts with and replaces, from the
           same call apkg show makes. */
        pkg_relation *rr = (pkg_relation *)malloc(sizeof *rr * PKG_REL_ROWS);
        int nr = 0, i;
        pkg_ctx *c = NULL; pkg_err err; memset(&err, 0, sizeof err);
        if (rr && pkg_open(root, PKG_OPEN_READ, &c, &err) == PKG_OK) {
            pkg_relations_get(c, index_path, e->id, rr, PKG_REL_ROWS, &nr, &err);
            pkg_close(c);
        }
        for (i = 0; rr && i < nr; i++) {
            char st[240], *hint;
            snprintf(st, sizeof st, "%s", rr[i].state);
            /* The window's way to install it, not the Shell's. */
            if ((hint = strstr(st, " (apkg install ")) != NULL) snprintf(hint, sizeof st - (size_t)(hint - st), " (select it and press Install)");
            if (k < sizeof about)
                k += (size_t)snprintf(about + k, sizeof about - k, "%s\33b%s\33n %s%s%s%s: %s\n", i || !k ? "" : "\n",
                         rr[i].kind == PKG_REL_DEPENDS ? "needs" : rr[i].kind == PKG_REL_RECOMMENDS ? "recommends" :
                         rr[i].kind == PKG_REL_CONFLICTS ? "conflicts with" : rr[i].kind == PKG_REL_REPLACES ? "replaces" :
                         "needed by",
                         rr[i].id, rr[i].min_version[0] ? " " : "", rr[i].min_version[0] ? ">= " : "", rr[i].min_version, st);
            if (rr[i].kind == PKG_REL_RECOMMENDS && rr[i].reason[0] && k < sizeof about)
                k += (size_t)snprintf(about + k, sizeof about - k, "  why: %s\n", rr[i].reason);
        }
        free(rr);
    }
    if (det.in_index) {
        /* Two separate facts: what the catalogue's automatic checks found
           about these bytes, and who started the program where. */
        char text[600]; int i;
        size_t t = strlen(tech);
        pkg_review_text(&det, text, sizeof text);
        if (t < sizeof tech) t += (size_t)snprintf(tech + t, sizeof tech - t, "\n\33bstatus:\33n %s\n", text);
        if (!det.ntests && t < sizeof tech)
            t += (size_t)snprintf(tech + t, sizeof tech - t, "runtime tests: none recorded for this archive\n");
        for (i = 0; i < det.ntests && i < PKG_TESTS_MAX && t < sizeof tech; i++)
            t += (size_t)snprintf(tech + t, sizeof tech - t, "runtime test: %s\n", det.tests[i]);
        pkg_added_text(&det, text, sizeof text);
        /* These two are fields: \1 and \2 stand for them, so their links
           come from the fields, not from a reading of this text. A full
           field may hold a cut address; a link must not open a different
           page. */
        if (det.source[0] && k < sizeof about) {
            int whole = strlen(det.source) + 1 < sizeof det.source;
            k += (size_t)snprintf(about + k, sizeof about - k, "%ssource: %s\n", k ? "\n" : "",
                                  whole && lk_exact(det.source) ? "\1"
                                  : whole ? det.source : "stated, but longer than this version can show");
        }
        if (k < sizeof about) k += (size_t)snprintf(about + k, sizeof about - k, "added: %s\n", text);
        if (t < sizeof tech)
            snprintf(tech + t, sizeof tech - t, "catalogue: %s",
                     !det.catalogue[0] ? "the catalogue file on this machine"
                     : lk_exact(det.catalogue) ? "\2" : det.catalogue);
    }
    if (e->installed && !strcmp(last_verify.id, e->id)) {
        /* First, where it is seen without scrolling: it answers the menu
           item that was just chosen. */
        size_t vl = strlen(last_verify.text), bl = strlen(tech);
        if (vl + bl + 2 < sizeof tech) {
            memmove(tech + vl + 1, tech, bl + 1);
            memcpy(tech, last_verify.text, vl);
            tech[vl] = '\n';
        }
    }
    /* Under the list: four lines. The line that needs attention comes
       before the notes, which fill what is left. */
    /* Under the list, one line for the selected package: its name, and
       smaller the facts (version, state, size) or the one thing that needs
       attention. The rest is on its page. */
    snprintf(head, sizeof head, "%s \267 %s%s%s%s", ver, state, size, attention[0] ? "   " : "", attention);
    snprintf(phead, sizeof phead, "%s \267 %s%s \267 %s\n%s", ver, state, size, e->category, attention);
    {
        static char name[80], sum[200];
        snprintf(name, sizeof name, "%s", shown_name(e));
        snprintf(sum, sizeof sum, "%s", e->summary);
        SetAttrs(txt_name,  MUIA_Text_Contents, (IPTR)name, TAG_DONE);
        SetAttrs(txt_name2, MUIA_Text_Contents, (IPTR)name, TAG_DONE);
        SetAttrs(txt_facts, MUIA_Text_Contents, (IPTR)head, TAG_DONE);
        SetAttrs(txt_sum2,  MUIA_Text_Contents, (IPTR)sum, TAG_DONE);
    }
    SetAttrs(txt_facts2, MUIA_Text_Contents, (IPTR)phead, TAG_DONE);
    fields[0].label = NULL; fields[0].url = det.source;
    fields[1].label = NULL; fields[1].url = det.catalogue;
    lk_begin(LK_PKGABOUT);
    lk_add(LK_PKGABOUT, about, fields, 2);
    lk_end(LK_PKGABOUT);
    lk_begin(LK_DETAIL);
    lk_add(LK_DETAIL, tech, fields, 2);
    lk_end(LK_DETAIL);
    /* Another package: its first picture. The same one again (after a job,
       or when its gallery arrived) keeps the picture that was open. */
    if (strcmp(media_id, e->id) != 0) shot_now = 0;
    show_media(e->id, icon_file);
    can.open = e->installed;
    can.verify = e->installed;
    can.remove = e->installed;
    can.install = !e->installed && e->ours;
    show_actions(busy_now());
}

/* Open folder: the drawer the registry entry records, through the same call
   as `apkg open`. It reads only; the registry and the drawer are not touched. */
static void open_folder(const char *id)
{
    char where[PKG_MAXPATH], line[PKG_MAXPATH + 200];
    pkg_ctx *c = NULL; pkg_err err; pkg_status st;
    memset(&err, 0, sizeof err);
    where[0] = 0;
    st = pkg_open(root, PKG_OPEN_READ, &c, &err);
    if (st == PKG_OK) { st = pkg_open_folder(c, id, where, sizeof where, &err); pkg_close(c); }
    if (st == PKG_OK) snprintf(line, sizeof line, "opened %s", where);
    else snprintf(line, sizeof line, "could not open the folder of %s: %s%s%s", id, err.summary,
                  where[0] ? " -- " : "", where);
    logline("open folder %s: %s", id, line);
    status(line);
}

/* ------------------------------------------------------------------ jobs */

static void busy(int on)
{
    SetAttrs(btn_check,   MUIA_Disabled, on, TAG_DONE);
    SetAttrs(btn_selfupdate, MUIA_Disabled, on || !su_offer, TAG_DONE);
    SetAttrs(btn_cancel,  MUIA_Disabled, TRUE, TAG_DONE);   /* until an event says otherwise */
    SetAttrs(gauge, MUIA_Gauge_Current, 0, MUIA_Gauge_InfoText, (IPTR)(on ? "starting" : ""), TAG_DONE);
    /* The progress bar and Cancel are in the window only while something
       runs. */
    SetAttrs(grp_progress, MUIA_ShowMe, on, TAG_DONE);
    if (on) show_actions(1); else show_detail();
}

static const char *opname(job_kind k)
{
    switch (k) {
    case JOB_INSTALL: return "install";
    case JOB_REMOVE:  return "remove";
    case JOB_UPDATE:  return "update";
    case JOB_PREVIEW_UPGRADE: case JOB_UPGRADE: return "upgrade";
    case JOB_PREVIEW_ROLLBACK: case JOB_ROLLBACK: return "rollback";
    case JOB_CHECK_RELEASE: return "check";
    case JOB_VERIFY: return "verify";
    case JOB_RUN: return "run";
    case JOB_SU_LOCATE: case JOB_SU_STAGE: return "self-update";
    case JOB_MEDIA: return "pictures";
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

static int install_confirmed;     /* the plan in the confirmation window was accepted */
static char install_sig[700];     /* that plan's steps (plan_signature) */

static int start(job_kind kind, const char *id)
{
    if (in_flight) return -1;
    memset(&cur, 0, sizeof cur);
    cur.kind = kind;
    cur.confirmed = kind == JOB_INSTALL && install_confirmed;
    if (cur.confirmed) snprintf(cur.plan_sig, sizeof cur.plan_sig, "%s", install_sig);
    install_confirmed = 0;
    cur.seq = active ? active->id : 0;
    snprintf(cur.id, sizeof cur.id, "%s", id ? id : "");
    snprintf(cur.root, sizeof cur.root, "%s", root);
    snprintf(cur.index, sizeof cur.index, "%s", index_path);
    snprintf(cur.index_url, sizeof cur.index_url, "%s", index_url);
    snprintf(cur.current, sizeof cur.current, "%s", PM_VERSION);
    cur.reply = port;
    cur.su = &sujob;
    last_phase[0] = 0; last_can_cancel = 0;
    busy(1);
    status(kind == JOB_INSTALL ? "installing..." : kind == JOB_REMOVE ? "removing..." :
           kind == JOB_UPDATE ? "updating the index..." :
           kind == JOB_UPGRADE ? "upgrading..." : kind == JOB_ROLLBACK ? "rolling back..." :
           kind == JOB_PREVIEW_UPGRADE ? "working out what the upgrade would do..." :
           kind == JOB_CHECK_RELEASE ? "asking for the latest release..." :
           kind == JOB_VERIFY ? "checking the installed files..." :
           kind == JOB_RUN ? "starting..." :
           kind == JOB_SU_LOCATE ? "looking at this installation of arospkg..." :
           kind == JOB_SU_STAGE ? "downloading and checking the new arospkg..." :
           kind == JOB_MEDIA ? (id && id[0] ? "fetching the pictures of this application..." : "fetching the icons of the catalogue...") :
           "working out what the rollback would do...");
    if (job_start(&cur) != 0) { busy(0); status("could not start the worker process"); return -1; }
    in_flight = 1;
    return 0;
}

/* Every operation starts here, from a button or from a script. */
static int install_gate(rx_job *j);
static void proceed(const char *how);

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
    if (kind == JOB_INSTALL && install_gate(j)) return j;
    if (start(kind, pkg) != 0) finish(j, RXJ_FAILED, "CANNOTSTART", "could not start the worker process");
    return j;
}

/* Before an install: its plan, from the same call apkg makes. A refusal
 * ends the job here; a plan that changes more than the package (what it
 * needs, what it replaces) waits in the confirmation window. Returns 1
 * when the job was settled or is waiting, 0 to start it at once. */
static int install_gate(rx_job *j)
{
    static char text[3000];
    pkg_install_plan *pl = (pkg_install_plan *)calloc(1, sizeof *pl);
    pkg_ctx *c = NULL; pkg_err err; pkg_status st;
    size_t k; int i;
    memset(&err, 0, sizeof err);
    if (!pl) return 0;
    if ((st = pkg_open(root, PKG_OPEN_READ, &c, &err)) == PKG_OK) {
        st = pkg_plan_install(c, index_path, j->package, pl, &err);
        pkg_close(c);
    }
    if (st != PKG_OK || !pl->ok) {
        char why[800];
        snprintf(why, sizeof why, "%s", st != PKG_OK ? err.summary : pl->why);
        logline("install %s refused: %s", j->package, why);
        finish(j, RXJ_FAILED, rx_statusword(st != PKG_OK ? st : pl->status), why);
        if (st == PKG_OK) {
            err.status = pl->status; snprintf(err.summary, sizeof err.summary, "cannot install %s", j->package);
            snprintf(err.detail, sizeof err.detail, "%s", pl->why);
        }
        show_err("cannot install", err.status, &err);
        free(pl);
        return 1;
    }
    if (!pl->extra) { free(pl); return 0; }
    k = (size_t)snprintf(text, sizeof text, "Install %s\n\nThis changes more than %s:\n\n", j->package, j->package);
    for (i = 0; i < pl->nops && k < sizeof text; i++) {
        const pkg_plan_op *o = &pl->ops[i];
        char v[80];
        rev_label(v, sizeof v, o->version, o->revision);
        k += (size_t)snprintf(text + k, sizeof text - k, "%d. %s %s %s%s%s\n", i + 1,
                              o->kind == PKG_OP_DEPENDENCY ? "install (needed)" : o->kind == PKG_OP_INSTALL ? "install" :
                              "remove (replaced)", o->id, v, o->note[0] ? "\n     " : "", o->note);
    }
    if (k < sizeof text)
        snprintf(text + k, sizeof text - k, "\nNothing else is removed. Files you changed in a removed package are kept.\n\nProceed?");
    logline("install %s: plan of %d steps waits for confirmation", j->package, pl->nops);
    plan_signature(pl, install_sig, sizeof install_sig);
    free(pl);
    j->state = RXJ_CONFIRM;
    snprintf(j->message, sizeof j->message, "the install plan waits for confirmation");
    confirming = 1;
    status("waiting for your decision");
    if (auto_yes) { proceed("confirmed automatically (--yes)"); return 1; }
    SetAttrs(win_confirm, MUIA_Window_Open, TRUE, TAG_DONE);
    confirm_text(text);
    return 1;
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

static void su_proceed(void);
static void su_decline(const char *how);

static void proceed(const char *how)
{
    rx_job *j = active;
    if (su_confirming) { su_proceed(); return; }
    if (!confirming || !j) return;
    confirm_close();
    logline("%s %s %s", j->op, j->package, how);
    j->state = RXJ_RUNNING;
    j->phase[0] = 0; j->done = j->total = 0; j->can_cancel = 0;
    if (!strcmp(j->op, "install")) install_confirmed = 1;
    if (start(!strcmp(j->op, "install") ? JOB_INSTALL : !strcmp(j->op, "upgrade") ? JOB_UPGRADE : JOB_ROLLBACK,
              j->package) != 0)
        finish(j, RXJ_FAILED, "CANNOTSTART", "could not start the worker process");
}

static void decline(const char *how)
{
    rx_job *j = active;
    if (su_confirming) { su_decline(how); return; }
    if (!confirming || !j) return;
    confirm_close();
    logline("%s %s %s", j->op, j->package, how);
    finish(j, RXJ_DECLINED, "", "nothing changed");
    busy(0);
    status("nothing changed");
}


/* ------------------------------------------------------ updating arospkg */

/* Said where the question was asked, in About, and on the status line. */
/* The answer, and below it always the release page: what the check talks
   about. */
static void about_text(const char *raw)
{
    lk_begin(LK_RELEASE);
    lk_add(LK_RELEASE, raw, NULL, 0);
    lk_add(LK_RELEASE, "\n", NULL, 0);
    lk_add_field(LK_RELEASE, "Release page", PKG_RELEASE_PAGE);
    lk_end(LK_RELEASE);
}

static void su_say(const char *line, const char *about)
{
    about_text(about ? about : line);
    status(line);
    logline("self-update: %s", line);
}

/* One Shell argument, quoted as ReadArgs reads it back. */
static void su_quote(char *out, size_t n, const char *arg)
{
    size_t k = 0;
    if (n < 3) { if (n) out[0] = 0; return; }
    out[k++] = '"';
    for (; *arg && k + 3 < n; arg++) {
        if (*arg == '"' || *arg == '*') out[k++] = '*';
        out[k++] = *arg;
    }
    out[k++] = '"';
    out[k] = 0;
}

/* Run a command and give back what it printed: whether an apkg
   understands the hand-off this window writes, before the window closes
   for good. */
static int su_output(const char *cmd, char *out, size_t n)
{
    char file[64], *t;
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR o, in, home, td, old;
    LONG rc;
    snprintf(file, sizeof file, "%sarospkg-proto-%s", u_exists("T:") ? "T:" : "RAM:", announce_nonce);
    if (!(o = Open((CONST_STRPTR)file, MODE_NEWFILE))) return -1;
    if (!(in = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE))) { Close(o); return -1; }
    /* Even a command that ends at once keeps a copy of the caller's home
       drawer locked until a reboot (~/Work/AROS/CLAUDE.md): this window's
       drawer is the one about to be replaced. */
    td = Lock((CONST_STRPTR)(u_exists("T:") ? "T:" : "RAM:"), SHARED_LOCK);
    home = me->pr_HomeDir; me->pr_HomeDir = BNULL;
    old = td ? CurrentDir(td) : BNULL;
    rc = SystemTags((CONST_STRPTR)cmd, SYS_Output, (IPTR)o, SYS_Input, (IPTR)in, TAG_DONE);
    if (td) { CurrentDir(old); UnLock(td); }
    me->pr_HomeDir = home;
    Close(in);
    Close(o);
    t = u_read_all(file, NULL);
    u_delete(file);
    if (rc != 0 || !t) { free(t); return -1; }
    snprintf(out, n, "%s", t);
    free(t);
    return 0;
}

/* Whether the apkg at `path` can finish this window's update. It is asked
   only for --version first: an apkg before 0.7.0 takes any other request
   as a command, and "self-update --protocol" made 0.6.0 update itself from
   the network. An apkg that names the protocol in --version is asked
   nothing more; 0.7.0, which does not, is asked the way it understands. */
static int su_speaks(const char *path, char *why, size_t n)
{
    char q[PKG_MAXPATH + 8], cmd[PKG_MAXPATH + 64], out[1024], want[40], ver[40] = "";
    const char *p;
    su_quote(q, sizeof q, path);
    snprintf(cmd, sizeof cmd, "%s --version", q);
    if (su_output(cmd, out, sizeof out) != 0) { snprintf(why, n, "no answer to --version"); return 0; }
    if (!strncmp(out, "apkg ", 5)) snprintf(ver, sizeof ver, "%.*s", (int)strcspn(out + 5, "\r\n"), out + 5);
    if ((p = strstr(out, "self-update protocol "))) {
        if (atoi(p + 21) == SU_PROTOCOL) return 1;
        snprintf(why, n, "apkg %s, protocol %d", ver, atoi(p + 21));
        return 0;
    }
    if (!ver[0] || pkg_release_vercmp(ver, "0.7.0") < 0) { snprintf(why, n, "apkg %s", ver[0] ? ver : "?"); return 0; }
    snprintf(want, sizeof want, "arospkg-self-update %d", SU_PROTOCOL);
    snprintf(cmd, sizeof cmd, "%s self-update --protocol", q);
    if (su_output(cmd, out, sizeof out) == 0) {
        out[strcspn(out, "\r\n")] = 0;
        if (!strcmp(out, want)) return 1;
    }
    snprintf(why, n, "apkg %s", ver);
    return 0;
}

/* The apkg that finishes the update: the installation's own, or the new
   one just checked, whichever reads this window's hand-off. */
static int su_helper(char *out, size_t n)
{
    char cand[5][PKG_MAXPATH], why[80];
    int i;
    /* After an interruption the drawer's apkg may be missing, as .new or
       .old beside it; any of them, or C:apkg, will do if it reads this
       hand-off. */
    u_join(cand[0], PKG_MAXPATH, sujob.in.drawer, "apkg");
    u_join(cand[1], PKG_MAXPATH, sujob.st.dir, "new/apkg");
    u_join(cand[2], PKG_MAXPATH, sujob.in.drawer, "apkg.new");
    u_join(cand[3], PKG_MAXPATH, sujob.in.drawer, "apkg.old");
    snprintf(cand[4], PKG_MAXPATH, "C:apkg");
    for (i = 0; i < 5; i++) {
        if (!sujob.st.dir[0] && i == 1) continue;
        if (!u_exists(cand[i])) continue;
        if (su_speaks(cand[i], why, sizeof why)) {
            snprintf(out, n, "%s", cand[i]);
            return 0;
        }
        logline("self-update: %s does not read this hand-off (%s)", cand[i], why);
    }
    return -1;
}

/* Hand the update to apkg and leave. apkg waits until this process is
   gone, replaces the files and starts PkgManager again with what it was
   started with. Returns 0 when apkg has it. */
static int su_hand_off(const su_stage *st, const char *what)
{
    su_restart rs;
    pkg_err e;
    char helper[PKG_MAXPATH], file[PKG_MAXPATH + 16], q[PKG_MAXPATH + 8], cmd[2 * PKG_MAXPATH + 64];
    struct Process *me = (struct Process *)FindTask(NULL);
    BPTR t, old, home;
    LONG rc;

    memset(&rs, 0, sizeof rs); memset(&e, 0, sizeof e);
    if (su_helper(helper, sizeof helper) != 0) {
        su_say("cannot update from this window: its apkg does not take the update over",
               "This installation's apkg cannot finish an update started here.\nUpdate from a Shell: apkg self-update");
        return -1;
    }
    rs.wb = wb_start;
    snprintf(rs.program, sizeof rs.program, "%s", sujob.program);
    snprintf(rs.root, sizeof rs.root, "%s", root);
    rs.task = (unsigned long)(IPTR)me;
    if (me->pr_CLI) rs.module = (unsigned long)(IPTR)((struct CommandLineInterface *)BADDR(me->pr_CLI))->cli_Module;
    snprintf(rs.nonce, sizeof rs.nonce, "%s", announce_nonce);
    {
        /* Started again as it was started now: the same root and options. */
        size_t k = (size_t)snprintf(rs.args, sizeof rs.args, "--after-update");
        if (strcmp(root, "SYS:Packages")) { su_quote(q, sizeof q, root); k += (size_t)snprintf(rs.args + k, sizeof rs.args - k, " --root %s", q); }
        if (log_path[0] && k < sizeof rs.args) { su_quote(q, sizeof q, log_path); k += (size_t)snprintf(rs.args + k, sizeof rs.args - k, " --log %s", q); }
        if (start_updates && k < sizeof rs.args) snprintf(rs.args + k, sizeof rs.args - k, " --updates");
    }
    u_join(file, sizeof file, st->dir, "handoff.json");
    if (su_handoff_save(file, &sujob.in, st, &rs, &e) != PKG_OK) { show_err("cannot update arospkg", e.status, &e); return -1; }
    su_quote(q, sizeof q, helper);
    {
        char qf[PKG_MAXPATH + 24];
        su_quote(qf, sizeof qf, file);
        snprintf(cmd, sizeof cmd, "%s self-update --apply %s", q, qf);
    }
    /* Its own current directory in T:, and no copy of this window's home
       drawer: either would hold a lock in the installation it replaces. */
    {
        BPTR nin = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE), nout = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
        if (!nin || !nout || !(t = Lock((CONST_STRPTR)(u_exists("T:") ? "T:" : "RAM:"), SHARED_LOCK))) {
            if (nin) Close(nin);
            if (nout) Close(nout);
            su_say("cannot start apkg to finish the update; nothing was changed", NULL);
            return -1;
        }
        home = me->pr_HomeDir; me->pr_HomeDir = BNULL;
        old = CurrentDir(t);
        rc = SystemTags((CONST_STRPTR)cmd, SYS_Input, (IPTR)nin, SYS_Output, (IPTR)nout,
                        SYS_Asynch, TRUE, NP_StackSize, 262144, TAG_DONE);
        CurrentDir(old);
        me->pr_HomeDir = home;
        UnLock(t);
        if (rc == -1) {
            Close(nin); Close(nout);
            su_say("cannot start apkg to finish the update; nothing was changed", NULL);
            return -1;
        }
    }
    logline("self-update: %s handed to %s (%s); quitting", what, helper, file);
    su_handed_off = 1;
    return 0;
}

static void su_done(job_kind kind, pm_msg *m, const char *why)
{
    char line[900];
    su_install *in = &sujob.in;
    if (kind == JOB_SU_LOCATE) {
        if (m->status != PKG_OK) {
            snprintf(line, sizeof line, "cannot update arospkg: %s", why);
            su_say(line, NULL); busy(0); return;
        }
        if (sujob.where.kind != SU_WHERE_HOME) {
            /* Only the drawer of a release or of the package is replaced
               from here; a renamed or moved PkgManager is updated by hand. */
            su_say("cannot update arospkg from here: this PkgManager is not in an arospkg drawer "
                   "(no apkg, BUILD.txt or package record beside it)", NULL);
            busy(0); return;
        }
        if (sujob.c_path[0]) {
            /* C:apkg is where the release README puts a copy; whether it is
               this installation's is the user's to say, once. */
            LONG a;
            pkg_err e;
            memset(&e, 0, sizeof e);
            status("waiting for your decision");
            if (auto_yes) a = 1;
            else a = MUI_Request(app, win_about, 0, (STRPTR)"Update arospkg",
                                 (STRPTR)"Update it too|Leave it|Cancel",
                                 (STRPTR)"%s (%s) is another apkg on this machine.\n"
                                         "It is not recorded with any installation.\n\n"
                                         "Update it together with this PkgManager?",
                                 (IPTR)sujob.c_path, (IPTR)sujob.c_version);
            if (a == 0) { su_say("update cancelled; nothing was changed", NULL); busy(0); return; }
            if (su_record(sujob.c_path, a == 1 ? in->drawer : "", &e) != PKG_OK) {
                snprintf(line, sizeof line, "cannot update arospkg: %s (%s); nothing was changed", e.summary, e.subject);
                su_say(line, NULL); busy(0); return;
            }
            if (a == 1 && su_add_copy(in, sujob.c_path) != 0) {
                snprintf(line, sizeof line, "cannot update arospkg: cannot read %s (recorded as part of this "
                         "installation); no file was changed", sujob.c_path);
                su_say(line, NULL); busy(0); return;
            }
            logline("self-update: %s recorded as %s", sujob.c_path, a == 1 ? "part of this installation" : "on its own");
        }
        snprintf(sujob.stage_dir, sizeof sujob.stage_dir, "%sarospkg-update-%s", u_exists("T:") ? "T:" : "RAM:", announce_nonce);
        about_text("Downloading and checking the new arospkg...\n"
                   "Progress is shown at the bottom of the main window; Cancel there stops it. Nothing is changed yet.");
        if (!begin(JOB_SU_STAGE, NULL, "window")) busy(0);
        return;
    }
    /* JOB_SU_STAGE */
    if (m->status != PKG_OK) {
        if (sujob.stage_dir[0]) u_deltree(sujob.stage_dir);
        if (m->status == PKG_E_CANCELLED) su_say("update cancelled; nothing was changed", NULL);
        else { snprintf(line, sizeof line, "cannot update arospkg: %s", why); su_say(line, NULL); }
        busy(0); return;
    }
    if (!sujob.st.nreplace && !sujob.st.managed) {
        u_deltree(sujob.stage_dir);
        su_say("arospkg is up to date: every file already is the release's", NULL);
        busy(0); return;
    }
    {
        static char text[2400];
        size_t k;
        int i;
        /* What happens to the window first: it is what the user has to
           know before pressing Proceed; the files follow. */
        k = (size_t)snprintf(text, sizeof text, "Update arospkg from %s to %s\n\n"
                             "PkgManager closes now and starts again\nwhen the new files are in place.\n"
                             "Icons and settings are kept.\n\n", PM_VERSION, sujob.st.version);
        if (sujob.st.managed && k < sizeof text)
            k += (size_t)snprintf(text + k, sizeof text - k, "The package arospkg in %s is replaced as an upgrade "
                                  "(%s -> %s); apkg rollback arospkg goes back.\n\n",
                                  in->root, in->managed_version, sujob.st.version);
        if (k < sizeof text)
            k += (size_t)snprintf(text + k, sizeof text - k, "In %s:\n  ", in->drawer);
        for (i = 0; i < in->n && k < sizeof text; i++)
            if (in->f[i].act == SU_REPLACE && !in->f[i].copy)
                k += (size_t)snprintf(text + k, sizeof text - k, "%s ", su_role_name(in->f[i].role));
        for (i = 0; i < in->n && k < sizeof text; i++)
            if (in->f[i].act == SU_REPLACE && in->f[i].copy)
                k += (size_t)snprintf(text + k, sizeof text - k, "\nAlso %s (%s)", in->f[i].path,
                                      in->f[i].version[0] ? in->f[i].version : "?");
        if (k < sizeof text)
            snprintf(text + k, sizeof text - k, "\n\nThe previous files stay beside them, named .old.\n\nProceed?");
        logline("self-update: plan to %s, %d file(s)%s, waits for confirmation", sujob.st.version,
                sujob.st.nreplace, sujob.st.managed ? " and the package" : "");
        su_confirming = confirming = 1;
        status("waiting for your decision");
        SetAttrs(btn_cancel, MUIA_Disabled, TRUE, TAG_DONE);
        SetAttrs(gauge, MUIA_Gauge_InfoText, (IPTR)"waiting for your decision", TAG_DONE);
        if (auto_yes) { su_proceed(); return; }
        SetAttrs(win_confirm, MUIA_Window_Open, TRUE, TAG_DONE);
        confirm_text(text);
    }
}

static void su_proceed(void)
{
    confirm_close();
    su_confirming = 0;
    logline("self-update: confirmed");
    if (su_hand_off(&sujob.st, "the update") != 0) { u_deltree(sujob.stage_dir); busy(0); }
}

static void su_decline(const char *how)
{
    confirm_close();
    su_confirming = 0;
    u_deltree(sujob.stage_dir);
    logline("self-update: %s", how);
    su_say("update cancelled; nothing was changed", NULL);
    busy(0);
}

/* Update arospkg, pressed in About. */
static void su_begin(void)
{
    char run[SU_MAXCAND][PKG_MAXPATH];
    int un = 0, n = su_running_pkgmanagers(run, SU_MAXCAND, &un), i, mine = 0;
    for (i = 0; i < n; i++) if (!strcasecmp(run[i], self_drawer)) mine++;
    if (mine > 1 || un) {
        su_say("close the other PkgManager windows first: they would keep running the old version", NULL);
        return;
    }
    if (su_journal_pending()) {
        su_say("an earlier update of arospkg did not finish: start PkgManager again to repair it", NULL);
        return;
    }
    memset(&sujob.in, 0, sizeof sujob.in);
    memset(&sujob.st, 0, sizeof sujob.st);
    sujob.stage_dir[0] = 0;
    begin(JOB_SU_LOCATE, NULL, "window");
}

/* At the start: what the last update did, or an update left unfinished. */
static void su_at_start(void)
{
    su_result r;
    if (su_result_take(&r)) {
        static char line[300];
        logline("self-update result: ok=%d changed=%d %s -> %s: %s | %s", r.ok, r.changed, r.from, r.to, r.summary, r.detail);
        snprintf(line, sizeof line, "%s", r.summary);
        status(line);
        {
            static char both[900];
            snprintf(both, sizeof both, "%s\n%s", r.summary, r.detail);
            about_text(both);
        }
        if (!auto_yes)
            MUI_Request(app, win, 0, (STRPTR)"arospkg update", (STRPTR)"OK", (STRPTR)"%s\n\n%s",
                        (IPTR)r.summary, (IPTR)r.detail);
    }
    if (su_journal_pending()) {
        /* Not repaired automatically under --yes: a repair that cannot
           succeed would start this window again, and again. */
        LONG a = auto_yes ? 0 : MUI_Request(app, win, 0, (STRPTR)"arospkg update", (STRPTR)"Repair now|Later",
                                            (STRPTR)"An update of arospkg did not finish.\n\n"
                                                    "PkgManager closes, the update is finished or undone,\n"
                                                    "and PkgManager starts again.", NULL);
        logline("self-update: unfinished update found; %s", a ? "repair" : "later");
        if (a) {
            su_stage none;
            memset(&none, 0, sizeof none);
            memset(&sujob.in, 0, sizeof sujob.in);
            snprintf(sujob.in.drawer, sizeof sujob.in.drawer, "%s", self_drawer);
            snprintf(none.dir, sizeof none.dir, "%sarospkg-repair-%s", u_exists("T:") ? "T:" : "RAM:", announce_nonce);
            sujob.st = none;
            if (u_mkdir(none.dir) != 0 || su_hand_off(&none, "the repair") != 0) u_deltree(none.dir);
        } else status("an update of arospkg did not finish: start PkgManager again to repair it");
    }
}

/* Returns 1 when the window should now quit. */
static void update_news_status(const char *done);

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
                                      "preview upgrade", "upgrade", "preview rollback", "rollback",
                                      "check for a newer version", "verify", "start",
                                      "update arospkg", "update arospkg", "fetch pictures" };
        static const char *past[] = { "", "installed", "removed", "index updated",
                                      "", "upgraded", "", "rolled back", "checked", "verified", "started",
                                      "", "", "pictures fetched" };
        job_kind kind = cur.kind;
        rx_job *j = active;
        char line[160], why[640];
        snprintf(line, sizeof line, "%s %s", past[kind], cur.id);
        logline("done %s %s status=%d%s%s", verb[kind], cur.id, (int)m->status,
                m->status == PKG_OK ? "" : " ", m->status == PKG_OK ? "" : m->err.summary);
        in_flight = 0;
        err_text(why, sizeof why, &m->err);
        if (kind == JOB_CHECK_RELEASE) {
            /* The answer goes where the question was asked, in About. It
               says plainly that the window is not replaced by anything. */
            static char text[600];
            const pkg_release *r = &m->rel;
            if (m->status != PKG_OK) snprintf(text, sizeof text, "Could not check: %s", why);
            else if (r->found != 1)
                snprintf(text, sizeof text, "The latest stable release (%s) has no archive for %s.\n"
                         "This PkgManager %s stays as it is.", r->other_version[0] ? r->other_version : "?",
                         r->target, PM_VERSION);
            else {
                /* The whole installation, not this window alone: its apkg,
                   the copies tied to it and a package record can be behind
                   while PkgManager is current. Nothing newer is moved back. */
                const su_install *in = &sujob.in;
                int older = 0, newer = 0, i, shown = 0;
                /* A package already at the release: its own files are verify's
                   business, as the plan says (su_plan). */
                int pkg_current = in->home == SU_HOME_MANAGED && in->managed_version[0]
                                  && pkg_release_vercmp(in->managed_version, r->version) >= 0;
                size_t k = (size_t)snprintf(text, sizeof text, "Latest stable release: %s\n", r->version);
                if (sujob.located && sujob.where.kind == SU_WHERE_HOME) {
                    for (i = 0; i < in->n; i++) {
                        const su_file *f = &in->f[i];
                        const char *w;
                        if (f->role != SU_APKG && f->role != SU_PKGMANAGER) continue;
                        w = su_state_word(f, r->version);
                        if (!(pkg_current && !f->copy)) {
                            if (!strcmp(w, "older") || !strcmp(w, "unknown")) older++;
                            if (!strcmp(w, "newer")) newer++;
                        }
                        if (k < sizeof text)
                            k += (size_t)snprintf(text + k, sizeof text - k, "%s%s %s (%s)", shown++ ? ", " : "",
                                                  f->copy ? (const char *)FilePart((STRPTR)f->path) : su_role_name(f->role),
                                                  f->version[0] ? f->version : "?", w);
                    }
                    if (su_managed_behind(in, r->version)) older++;
                    if (in->home == SU_HOME_MANAGED && in->managed_version[0]
                        && pkg_release_vercmp(in->managed_version, r->version) > 0) newer++;
                } else if (r->cmp < 0) older++;
                else if (r->cmp > 0) newer++;
                if (k < sizeof text)
                    snprintf(text + k, sizeof text - k, "\n%s", newer ? "Part of this installation is newer than the release; nothing is offered."
                             : older ? "Update arospkg brings everything to this release."
                             : "Everything is up to date.");
                su_offer = !newer && older > 0;
            }
            about_text(text);
            if (m->status != PKG_OK || r->found != 1) su_offer = 0;
            logline("release check: status=%d found=%d version=%s other=%s cmp=%d", (int)m->status,
                    r->found, r->version, r->other_version, r->cmp);
            if (j) {
                if (m->status == PKG_OK) finish(j, RXJ_DONE, "", text);
                else finish(j, RXJ_FAILED, rx_statusword(m->status), why);
            }
            if (quit_pending) return 1;
            busy(0);
            status(m->status == PKG_OK ? "release checked: see About" : "could not check for a newer version");
            return 0;
        }
        if (kind == JOB_SU_LOCATE || kind == JOB_SU_STAGE) {
            if (j) {
                if (m->status == PKG_OK) finish(j, RXJ_DONE, "", "");
                else finish(j, m->status == PKG_E_CANCELLED ? RXJ_CANCELLED : RXJ_FAILED, rx_statusword(m->status), why);
            }
            if (quit_pending) { if (sujob.stage_dir[0]) u_deltree(sujob.stage_dir); return 1; }
            su_done(kind, m, why);
            return 0;
        }
        if (kind == JOB_MEDIA) {
            /* Pictures are an extra: a failure is one line, and the window
               goes on with text. The rows are read again only for the
               catalogue's icons, which every row may have gained. */
            int icons = !cur.id[0];
            if (j) {
                if (m->status == PKG_OK) finish(j, RXJ_DONE, "", line);
                else finish(j, m->status == PKG_E_CANCELLED ? RXJ_CANCELLED : RXJ_FAILED, rx_statusword(m->status), why);
            }
            if (quit_pending) return 1;
            busy(0);
            if (icons) refill_keep("icons");
            if (m->status != PKG_OK) show_err(icons ? "the icons were not fetched" : "the pictures were not fetched", m->status, &m->err);
            else if (!icons) status("pictures fetched");
            return 0;
        }
        if (kind == JOB_VERIFY || kind == JOB_RUN) {
            /* Neither changes anything, so the list stays as it is; the
               outcome goes to the status line, and verify's to the detail
               pane, where it stays while the package is selected. */
            char text[300];
            if (kind == JOB_VERIFY) {
                verify_store(cur.id, m->status, &m->ver, m->listing, why);
                if (m->status == PKG_OK)
                    snprintf(text, sizeof text, "verified %s: %d as installed, %d changed locally, %d missing, %d not the package's",
                             cur.id, m->ver.ok, m->ver.changed, m->ver.missing, m->ver.unknown);
            } else if (m->status == PKG_OK) {
                snprintf(text, sizeof text, "%s", m->run.how);
            }
            if (m->listing) FreeVec(m->listing);
            m->listing = NULL;
            if (j) {
                if (m->status == PKG_OK) finish(j, RXJ_DONE, "", text);
                else finish(j, RXJ_FAILED, rx_statusword(m->status), why);
            }
            if (quit_pending) return 1;
            busy(0);
            if (m->status == PKG_OK) status(text);
            else show_err(verb[kind], m->status, &m->err);
            return 0;
        }
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
            /* The fallback's Floattext lays its text out only when it has
               a window, and text set before that stays blank. */
            SetAttrs(win_confirm, MUIA_Window_Open, TRUE, TAG_DONE);
            confirm_text(text);
            return 0;
        }
        if (j) {
            if (m->status == PKG_OK) finish(j, RXJ_DONE, "", line);
            else finish(j, m->status == PKG_E_CANCELLED ? RXJ_CANCELLED : RXJ_FAILED,
                        rx_statusword(m->status), why);
        }
        if (quit_pending) return 1;       /* the worker is gone; see worker.c */
        if (m->note[0]) logline("finished an interrupted replacement: %s", m->note);
        busy(0);
        {
            IPTR t = 0; char keep[PKG_MAXID];
            snprintf(keep, sizeof keep, "%s", cur.id);
            GetAttr(MUIA_String_Contents, str_search, &t);
            refill((const char *)t, keep[0] ? keep : NULL, "done");
        }
        /* After the refill, which writes its own count: the outcome is what
           the user is waiting to read. */
        if (m->status == PKG_OK && kind == JOB_REMOVE && (m->rem.kept || m->rem.others || m->rem.icon[0])) {
            /* What removal promises is to keep what the user changed or
               added; say what that was, as apkg remove does. */
            static char kept[600];
            size_t k = (size_t)snprintf(kept, sizeof kept, "removed %s; kept in %s:", cur.id,
                                        m->rem.dir[0] ? m->rem.dir : "its drawer");
            int i;
            for (i = 0; i < m->rem.named && k + 40 < sizeof kept; i++)
                k += (size_t)snprintf(kept + k, sizeof kept - k, " %s (changed)", m->rem.names[i]);
            for (i = 0; i < m->rem.others_named && k + 40 < sizeof kept; i++)
                k += (size_t)snprintf(kept + k, sizeof kept - k, " %s (yours)", m->rem.other_names[i]);
            if (m->rem.icon[0] && k + 40 < sizeof kept)
                snprintf(kept + k, sizeof kept - k, " and the changed drawer icon");
            logline("%s", kept);
            status(kept);
        }
        else if (m->status == PKG_OK && kind == JOB_REMOVE && m->rem.in_use) {
            /* An empty drawer another program holds: moved aside, or kept. */
            static char held[700];
            if (m->rem.moved_to[0])
                snprintf(held, sizeof held, "removed %s; its empty drawer was held by another program "
                         "and was moved to %s; the next install, upgrade or removal deletes it once nothing holds it", cur.id, m->rem.moved_to);
            else
                snprintf(held, sizeof held, "removed %s; its drawer %s is held by another program "
                         "and stays until that program ends or a restart", cur.id, m->rem.dir);
            logline("%s", held);
            status(held);
        }
        else if (m->status == PKG_OK && kind != JOB_UPDATE && m->note[0]) {
            /* Said where the user looks: an old package was removed now. */
            static char both[600];
            snprintf(both, sizeof both, "%s; finished an interrupted replacement: %s", line, m->note);
            status(both);
        }
        else if (m->status == PKG_OK && kind != JOB_UPDATE) status(line);
        else if (m->status == PKG_OK) update_news_status(past[kind]);
        else show_err(verb[kind], m->status, &m->err);
    }
    return 0;
}

/* After Update index: what the new catalogue brought for this machine, in
   one line, from the record apkg recent --since-last-update reads. The
   views behind it are Last refresh and Recently added. */
static void update_news_status(const char *done)
{
    static char line[400];
    pkg_ctx *c = NULL; pkg_err e; pkg_news nw;
    memset(&e, 0, sizeof e);
    memset(&nw, 0, sizeof nw);
    if (pkg_open(root, PKG_OPEN_READ, &c, &e) == PKG_OK) {
        pkg_news_last(c, index_path, &nw, &e);
        pkg_close(c);
    }
    if (!nw.recorded || !nw.current)
        snprintf(line, sizeof line, "%s; what is new in it could not be worked out", done);
    else if (nw.first)
        snprintf(line, sizeof line, "%s. %s Choose \"Recently added\" to see the newest.", done, nw.first_why);
    else if (!nw.n)
        snprintf(line, sizeof line, "%s; nothing new for this machine", done);
    else
        snprintf(line, sizeof line, "%s: %d new application%s, %d new build%s, %d updated -- choose \"Last refresh\" to list them",
                 done, nw.apps, nw.apps == 1 ? "" : "s", nw.builds, nw.builds == 1 ? "" : "s", nw.updated);
    logline("news after update: recorded=%d current=%d first=%d new=%d builds=%d updated=%d",
            nw.recorded, nw.current, nw.first, nw.apps, nw.builds, nw.updated);
    pkg_news_free(&nw);
    status(line);
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
        /* As the window's Upgrade button: from the updates model. */
        pkg_updates u; pkg_err ue; memset(&ue, 0, sizeof ue);
        if (pkg_updates_get(c, index_path, 1, &u, &ue) == PKG_OK) {
            const pkg_update_row *ur = pkg_updates_find(&u, e.id);
            up = ur && ur->state == PKG_UPD_AVAILABLE;
            pkg_updates_free(&u);
        } else up = pkg_can_upgrade(&e, why, sizeof why);
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
    view_state = ST_ALL;
    only_installed = 0;
    SetAttrs(chk_inst, MUIA_NoNotify, TRUE, MUIA_Selected, FALSE, TAG_DONE);
    page_close();
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
                    r->cmd == RXC_RUN ? JOB_RUN : r->cmd == RXC_VERIFY ? JOB_VERIFY :
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

/* OUTDATED [SHOW]: the updates model's counts and ids; with SHOW the window
   also comes forward with the Updates filter, which is what the ScreenBar
   widget's action asks for when PkgManager is already running. */
static void rx_outdated(rexx_msg *m, const rx_req *r)
{
    pkg_ctx *c; pkg_updates u; pkg_err e;
    if (r->show) {
        SetAttrs(str_search, MUIA_NoNotify, TRUE, MUIA_String_Contents, (IPTR)"", TAG_DONE);
        last_term[0] = 0; cat_chosen[0] = 0;
        view_state = ST_UPDATES;
        page_close();
        refill("", NULL, "arexx outdated show");
        SetAttrs(app, MUIA_Application_Iconified, FALSE, TAG_DONE);
        SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
        DoMethod(win, MUIM_Window_ToFront);
        SetAttrs(win, MUIA_Window_Activate, TRUE, TAG_DONE);
    }
    if (!(c = rx_open(m))) return;
    memset(&e, 0, sizeof e);
    if (pkg_updates_get(c, index_path, 1, &u, &e) != PKG_OK) {
        char t[600]; pkg_close(c); err_text(t, sizeof t, &e);
        rx_fail(m, RX_RC_ERROR, "%s %s", rx_statusword(e.status), t); return;
    }
    pkg_close(c);
    if (rx_format_updates(&u, rxout, sizeof rxout) != 0) rx_fail(m, RX_RC_ERROR, "TOOLONG the answer does not fit in one ARexx result");
    else rx_ok(m, rxout);
    pkg_updates_free(&u);
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
    case RXC_OUTDATED:     rx_outdated(m, &r); break;
    case RXC_UPDATE: case RXC_INSTALL: case RXC_REMOVE: case RXC_UPGRADE: case RXC_ROLLBACK:
    case RXC_RUN: case RXC_VERIFY:
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

    int after_update = 0;

    wb_start = argc == 0;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--after-update")) after_update = 1;
    for (i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--log") && i + 1 < argc) {
            /* Started again after an update: the same log goes on. */
            snprintf(log_path, sizeof log_path, "%s", argv[++i]);
            logf = fopen(log_path, after_update ? "a" : "w");
        }
        else if (!strcmp(argv[i], "--after-update")) ;
        else if (!strcmp(argv[i], "--release-from") && i + 1 < argc) su_set_base(argv[++i]);  /* tests */
        else if (!strcmp(argv[i], "--root") && i + 1 < argc) snprintf(root, sizeof root, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--slow") && i + 1 < argc) job_set_slow(atoi(argv[++i]));
        else if (!strcmp(argv[i], "--break-download-at") && i + 1 < argc)   /* tests */
            pkg_test_break_download(atol(argv[++i]), 1);
        else if (!strcmp(argv[i], "--corrupt-download-at") && i + 1 < argc) /* tests */
            pkg_test_break_download(atol(argv[++i]), 2);
        else if (!strcmp(argv[i], "--yes")) auto_yes = 1;        /* tests: no requester to click */
        else if (!strcmp(argv[i], "--no-text-view")) no_view = 1; /* tests: the fallback */
        else if (!strcmp(argv[i], "--no-auto")) no_auto = 1;     /* tests: nothing fetched at the start */
        else if (!strcmp(argv[i], "--updates") || !strcasecmp(argv[i], "UPDATES")) start_updates = 1;
        else if (!strcmp(argv[i], "--rexxlib") && i + 1 < argc) rexxlib = argv[++i];  /* tests: a machine without ARexx */
        else if (!strcmp(argv[i], "--test-nomem") && i + 1 < argc) {                   /* tests: allocation failures */
            const char *w = argv[++i];
            if (!strcmp(w, "start")) job_test_nomem();
            else if (!strcmp(w, "result")) rexx_test_nomem();
            else if (!strcmp(w, "prepare")) rexx_test_nomem_prepare();
        }

    snprintf(index_path, sizeof index_path, "%s/db/index.json", root);
    {
        /* This program's file and drawer, as DOS names them: what a
           self-update replaces, and what this window announces while it
           runs so that apkg can tell it is in use. */
        char name[256] = "";
        if (!GetProgramName((STRPTR)name, sizeof name) || !name[0])
            snprintf(name, sizeof name, "%s", FindTask(NULL)->tc_Node.ln_Name ? FindTask(NULL)->tc_Node.ln_Name : "PkgManager");
        if (GetProgramDir() && NameFromLock(GetProgramDir(), (STRPTR)self_drawer, sizeof self_drawer)) {
            snprintf(sujob.program, sizeof sujob.program, "%s", self_drawer);
            AddPart((STRPTR)sujob.program, FilePart((STRPTR)name), sizeof sujob.program);
        }
        su_announce_start(self_drawer, announce_nonce, sizeof announce_nonce);
        su_drop_old_demo();
    }
    display_hook.h_Entry = (HOOKFUNC)AROS_ASMSYMNAME(display_func);

    port = CreateMsgPort();
    if (!port) { su_announce_stop(); return RETURN_FAIL; }
    portsig = 1UL << port->mp_SigBit;

    rexx_open(rexxlib);
    rxsig = rexx_sigmask();
    if (logf) logline("start PkgManager %s sha256 %s root %s: %s", PM_VERSION, self_sha(), root, rexx_state());
    snprintf(title, sizeof title, "%s", rexx_active() ? "PkgManager" : "PkgManager (no ARexx port)");

    {
        lk_setup ls;
        ls.status = status_plain;
        ls.log = logline;
        ls.no_view = no_view;
        lk_init(&ls);
    }
    if (!pic_init()) {
        rexx_close("CLOSING PkgManager could not start");
        lk_finish(); lk_free(); DeleteMsgPort(port); su_announce_stop();
        return RETURN_FAIL;
    }
    {
        /* Everything a button does is in the menu too, by name; Verify and
           Roll back are only here. */
        static struct NewMenu nm[] = {
            { NM_TITLE, (STRPTR)"Project",        NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"About...",       (STRPTR)"?", 0, 0, (APTR)ID_ABOUT },
            { NM_ITEM,  NM_BARLABEL,              NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"Quit",           (STRPTR)"Q", 0, 0, (APTR)ID_QUIT },
            { NM_TITLE, (STRPTR)"Catalogue",      NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"Update index",   (STRPTR)"U", 0, 0, (APTR)ID_UPDATE },
            { NM_TITLE, (STRPTR)"Package",        NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"Details...",     (STRPTR)"D", 0, 0, (APTR)ID_DETAILS },
            { NM_ITEM,  NM_BARLABEL,              NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"Install",        (STRPTR)"I", 0, 0, (APTR)ID_INSTALL },
            { NM_ITEM,  (STRPTR)"Run",            (STRPTR)"R", 0, 0, (APTR)ID_RUN },
            { NM_ITEM,  (STRPTR)"Upgrade",        NULL, 0, 0, (APTR)ID_UPGRADE },
            { NM_ITEM,  (STRPTR)"Roll back",      NULL, 0, 0, (APTR)ID_ROLLBACK },
            { NM_ITEM,  (STRPTR)"Verify",         (STRPTR)"V", 0, 0, (APTR)ID_VERIFY },
            { NM_ITEM,  NM_BARLABEL,              NULL, 0, 0, NULL },
            { NM_ITEM,  (STRPTR)"Open folder",    (STRPTR)"O", 0, 0, (APTR)ID_OPEN },
            { NM_ITEM,  (STRPTR)"Remove",         NULL, 0, 0, (APTR)ID_REMOVE },
            { NM_END,   NULL,                     NULL, 0, 0, NULL }
        };
        menus = MUI_MakeObject(MUIO_MenustripNM, (IPTR)nm, 0);
    }
    title_font_open();
    for (i = 0; i < PKG_MEDIA_SHOTS; i++) pic_car[i] = pic_new(car_w, car_h, 0, 0, 0);
    app = ApplicationObject,
        MUIA_Application_Title,       (IPTR)"PkgManager",
        MUIA_Application_Author,      (IPTR)AROSPKG_AUTHOR,
        MUIA_Application_Version,     (IPTR)"$VER: PkgManager " PM_VERSION " (24.11.2026)",
        MUIA_Application_Description, (IPTR)"Install software from AROS Archives",
        MUIA_Application_Base,        (IPTR)"PKGMANAGER",
        SubWindow, (win = WindowObject,
            MUIA_Window_Title, (IPTR)title,
            MUIA_Window_ID,    MAKE_ID('P','K','G','M'),
            MUIA_Window_Menustrip, (IPTR)menus,
            MUIA_Window_LeftEdge, 100, MUIA_Window_TopEdge, 40,
            MUIA_Window_Width, 760,   MUIA_Window_Height, 560,
            WindowContents, (VGroup,
                Child, (pg_main = PageGroup,
                    /* The list and the selected package under it. */
                    Child, (VGroup,
                        /* On the left the search and, under it, what to
                           list: the views, then the categories. On the
                           right the name of what is listed (the place
                           beside it is kept for a choice of order) and
                           the list. */
                        Child, (HGroup,
                            Child, (VGroup,
                                MUIA_HorizWeight, 28,
                                Child, (HGroup,
                                    Child, Label2("Search:"),
                                    Child, (str_search = StringObject,
                                        MUIA_Frame, MUIV_Frame_String,
                                        MUIA_CycleChain, 1,
                                    End),
                                End),
                                Child, (lst_side = ListviewObject,
                                    MUIA_CycleChain, 1,
                                    MUIA_Listview_List, (lst_sidelist = ListObject,
                                        MUIA_Frame, MUIV_Frame_InputList,
                                    End),
                                End),
                            End),
                            Child, (BalanceObject, End),
                            Child, (VGroup,
                                Child, (HGroup,
                                    Child, (txt_view = name_text()),
                                    Child, HSpace(0),
                                    Child, (chk_inst = ImageObject,
                                        ImageButtonFrame,
                                        MUIA_InputMode,      MUIV_InputMode_Toggle,
                                        MUIA_Image_Spec,     MUII_CheckMark,
                                        MUIA_Image_FreeVert, TRUE,
                                        MUIA_Background,     MUII_ButtonBack,
                                        MUIA_ShowSelState,   FALSE,
                                        MUIA_CycleChain,     1,
                                        MUIA_ShortHelp,      (IPTR)"List only what is installed of the chosen view or category.",
                                    End),
                                    Child, Label1("Installed only"),
                                End),
                                Child, (lst = ListviewObject,
                                    MUIA_CycleChain, 1,
                                    MUIA_Listview_List, (lst_list = ListObject,
                                        MUIA_Frame,           MUIV_Frame_InputList,
                                        MUIA_List_Format,     (IPTR)",BAR,,",
                                        MUIA_List_Title,      TRUE,
                                        MUIA_List_DisplayHook, (IPTR)&display_hook,
                                    End),
                                End),
                                /* The selected package, by name, and
                                   where to go from here: its drawer once
                                   it is installed, and its page. Install,
                                   Run and Upgrade are the button of its
                                   row; Remove is on its page and in the
                                   menu, away from a stray click. */
                                Child, (HGroup,
                                    Child, (VGroup,
                                        Child, (txt_name = name_text()),
                                        Child, (txt_facts = TextObject,
                                            MUIA_Font, MUIV_Font_Tiny,
                                            MUIA_Text_Contents, (IPTR)"",
                                            MUIA_Text_SetMin, FALSE,
                                            MUIA_Text_Shorten, MUIV_Text_Shorten_ElideRight,
                                        End),
                                    End),
                                    Child, (VGroup, MUIA_HorizWeight, 0,
                                        Child, VSpace(0),
                                        Child, (HGroup,
                                            Child, (btn_open    = SimpleButton("Open folder")),
                                            Child, (btn_details = SimpleButton("Details...")),
                                        End),
                                        Child, VSpace(0),
                                    End),
                                End),
                            End),
                        End),
                    End),
                    /* The page of one package: a header with what it is and
                       what can be done with it, its pictures in a strip,
                       and the text. */
                    Child, (VGroup,
                        /* Back stands above the header, so the icon is
                           at the header's left edge. */
                        Child, (HGroup,
                            Child, (btn_back = glyph_button(glyph_back, 12, "Back", "Back to the list")),
                            Child, HSpace(0),
                        End),
                        Child, (grp_head = HGroup,
                            MUIA_Frame, MUIV_Frame_Group,
                            MUIA_Background, MUII_GroupBack,
                            Child, (VGroup, MUIA_HorizWeight, 0,
                                Child, (pic_icon2 = pic_new(PKG_ICON_W, PKG_ICON_H, 0, 0, 0)),
                                Child, VSpace(0),
                            End),
                            Child, (VGroup,
                                Child, (txt_name2 = name_text()),
                                Child, (txt_sum2 = summary_text()),
                                /* A plain text, not a text view: it has
                                   no addresses, and it takes the header's
                                   own background. */
                                Child, (txt_facts2 = TextObject,
                                    MUIA_Font, MUIV_Font_Tiny,
                                    MUIA_Text_Contents, (IPTR)"\n",
                                    MUIA_Text_SetMin, FALSE,
                                    MUIA_Text_Shorten, MUIV_Text_Shorten_ElideRight,
                                End),
                            End),
                            Child, (VGroup, MUIA_HorizWeight, 0,
                                /* The main button alone on top; under
                                   it the two that are rarely wanted, with
                                   Remove last. */
                                Child, (btn_main2 = main_button()),
                                Child, (HGroup,
                                    Child, (btn_open2   = SimpleButton("Open folder")),
                                    Child, (btn_remove2 = SimpleButton("Remove")),
                                End),
                                Child, VSpace(0),
                            End),
                        End),
                        Child, (grp_car = ScrollgroupObject,
                            MUIA_ShowMe, FALSE,
                            MUIA_Scrollgroup_FreeVert, FALSE,
                            MUIA_Scrollgroup_FreeHoriz, TRUE,
                            /* As high as its pictures and its scroller;
                               the text has the rest of the page. */
                            MUIA_FixHeight, car_h + 24,
                            MUIA_Scrollgroup_Contents, (virt_car = VirtgroupObject,
                                MUIA_Group_Horiz, TRUE,
                                MUIA_Group_HorizSpacing, CAR_GAP,
                                Child, pic_car[0], Child, pic_car[1], Child, pic_car[2],
                                Child, pic_car[3], Child, pic_car[4], Child, pic_car[5],
                            End),
                        End),
                        Child, (grp_carnav = HGroup,
                            MUIA_ShowMe, FALSE,
                            Child, (btn_prev = SimpleButton("Previous")),
                            Child, (txt_caption = TextObject,
                                MUIA_Text_Contents, (IPTR)"",
                                MUIA_Text_SetMin, FALSE,
                                MUIA_HorizWeight, 400,
                            End),
                            Child, (btn_next = SimpleButton("Next")),
                        End),
                        Child, (reg_page = RegisterGroup(page_titles),
                            MUIA_CycleChain, 1,
                            Child, lk_view(LK_PKGABOUT, 4, LK_V_SCROLL, 72),
                            Child, lk_view(LK_DETAIL, 4, LK_V_SCROLL, 72),
                        End),
                    End),
                End),
                /* In the window only while something runs. */
                Child, (grp_progress = HGroup,
                    MUIA_ShowMe, FALSE,
                    Child, (gauge = GaugeObject,
                        MUIA_Frame, MUIV_Frame_Gauge,
                        MUIA_Gauge_Horiz, TRUE,
                        MUIA_Gauge_Max, 100,
                        MUIA_Gauge_Current, 0,
                        MUIA_Gauge_InfoText, (IPTR)"",
                    End),
                    Child, (btn_cancel = SimpleButton("Cancel")),
                End),
                Child, lk_view(LK_STATUS, 2, LK_V_FRAME, 48),
            End),
        End),
        SubWindow, (win_about = WindowObject,
            MUIA_Window_Title, (IPTR)"About PkgManager",
            MUIA_Window_ID, MAKE_ID('P','K','G','A'),
            MUIA_Window_Width, 460,
            WindowContents, (VGroup,
                Child, lk_view(LK_ABOUT, 11, 0, 50),
                /* Six lines: the height every later answer fits in. */
                Child, lk_view(LK_RELEASE, 6, LK_V_FRAME, 46),
                Child, (HGroup,
                    Child, (btn_check = SimpleButton("Check for a newer version")),
                    Child, (btn_selfupdate = SimpleButton("Update arospkg")),
                    Child, (btn_about_close = SimpleButton("Close")),
                End),
            End),
        End),
        SubWindow, (win_confirm = WindowObject,
            MUIA_Window_Title, (IPTR)"PkgManager: confirm",
            MUIA_Window_ID,    MAKE_ID('P','K','G','C'),
            MUIA_Window_LeftEdge, 260, MUIA_Window_TopEdge, 180,
            MUIA_Window_Width, 440,
            WindowContents, (VGroup,
                Child, lk_view(LK_CONFIRM, 12, LK_V_SCROLL, 48),
                Child, (HGroup,
                    Child, (btn_proceed = SimpleButton("Proceed")),
                    Child, (btn_decline = SimpleButton("Cancel")),
                End),
            End),
        End),
    End;

    if (!app) { rexx_close("CLOSING PkgManager could not open its window"); lk_finish(); lk_free(); pic_exit(); DeleteMsgPort(port); su_announce_stop(); return RETURN_FAIL; }

    lk_owner(LK_ABOUT, win_about); lk_owner(LK_RELEASE, win_about);
    lk_owner(LK_DETAIL, win); lk_owner(LK_PKGABOUT, win);
    lk_owner(LK_STATUS, win); lk_owner(LK_CONFIRM, win_confirm);
    lk_begin(LK_ABOUT);
    lk_add(LK_ABOUT, "\33bPkgManager " PM_VERSION "\33n\n\n" AROSPKG_ABOUT_HEAD "\n", NULL, 0);
    lk_add_field(LK_ABOUT, "Project website", AROSPKG_HOMEPAGE);
    lk_add(LK_ABOUT, "\n\n" AROSPKG_ABOUT_TAIL, NULL, 0);
    lk_end(LK_ABOUT);
    about_text("Check for a newer version, then Update arospkg updates PkgManager and apkg together.");
    DoMethod(win, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)app, 2, MUIM_Application_ReturnID, MUIV_Application_ReturnID_Quit);
    DoMethod(str_search, MUIM_Notify, MUIA_String_Acknowledge, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_SEARCH);
    DoMethod(lst_side, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_FILTER);
    DoMethod(chk_inst, MUIM_Notify, MUIA_Selected, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ONLYINST);
    /* Both are set on the list itself, not on the view around it. */
    DoMethod(lst_list, MUIM_Notify, MUIA_Listview_ClickColumn, MUIV_EveryTime, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ROWCLICK);
    DoMethod(lst_list, MUIM_Notify, MUIA_Listview_DoubleClick, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ROWDOUBLE);
    DoMethod(btn_about_close, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ABOUT_CLOSE);
    DoMethod(btn_check, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CHECK);
    DoMethod(btn_selfupdate, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_SELFUPDATE);
    DoMethod(win_about, MUIM_Notify, MUIA_Window_CloseRequest, TRUE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_ABOUT_CLOSE);
    DoMethod(btn_main2,   MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_MAIN);
    DoMethod(btn_remove2, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_REMOVE);
    DoMethod(btn_open,    MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_OPEN);
    DoMethod(btn_open2,   MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_OPEN);
    DoMethod(btn_details, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_DETAILS);
    DoMethod(btn_back,    MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_BACK);
    DoMethod(btn_prev,    MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_PREV);
    DoMethod(btn_next,    MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_NEXT);
    DoMethod(btn_cancel,  MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_CANCEL);
    DoMethod(btn_proceed, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_PROCEED);
    DoMethod(btn_decline, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app, 2, MUIM_Application_ReturnID, ID_DECLINE);
    DoMethod(win_confirm, MUIM_Notify, MUIA_Window_CloseRequest, TRUE,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_DECLINE);
    DoMethod(lst, MUIM_Notify, MUIA_List_Active, MUIV_EveryTime,
             (IPTR)app, 2, MUIM_Application_ReturnID, ID_SELECT);

    SetAttrs(btn_cancel,   MUIA_Disabled, TRUE, MUIA_HorizWeight, 0, TAG_DONE);
    SetAttrs(btn_selfupdate, MUIA_Disabled, TRUE, TAG_DONE);
    SetAttrs(win, MUIA_Window_Open, TRUE, TAG_DONE);
    if (start_updates) view_state = ST_UPDATES;
    refill("", NULL, start_updates ? "start updates" : "start");
    su_at_start();
    /* At the start, on its own: a catalogue that is missing or more than two
       weeks old is fetched, and with it the icons; a current catalogue whose
       icons are not here yet gets them. Not under --yes or --no-auto: a test
       decides itself what is fetched. */
    if (!auto_yes && !no_auto && !busy_now() && !su_handed_off) {
        if (upd.catalogue == PKG_CAT_MISSING || (upd.catalogue == PKG_CAT_OK && upd.catalogue_stale)) {
            logline("catalogue %s: updating at start", upd.catalogue == PKG_CAT_MISSING ? "missing" : "stale");
            begin(JOB_UPDATE, NULL, "start");
        } else if (upd.catalogue == PKG_CAT_OK && pkg_media_icons_due(root, index_path)) {
            logline("icons of the catalogue not here: fetching at start");
            begin(JOB_MEDIA, NULL, "start");
        }
    }

    while (!quit) {
        ret = DoMethod(app, MUIM_Application_NewInput, (IPTR)&sigs);
        if (ret == MUIV_Application_ReturnID_Quit || ret == ID_QUIT) {
            if (confirming) { decline("declined (window closed)"); break; }
            if (in_flight) close_requested(); else break;
        }
        switch (ret) {
        case ID_ABOUT:
            SetAttrs(win_about, MUIA_Window_Open, TRUE, MUIA_Window_Activate, TRUE, TAG_DONE);
            logline("about opened");
            break;
        case ID_ABOUT_CLOSE:
            SetAttrs(win_about, MUIA_Window_Open, FALSE, TAG_DONE);
            logline("about closed");
            break;
        case ID_SEARCH: search_acknowledged(); break;
        case ID_FILTER: {
            /* A view of everything, or one category; the empty row
               between them is "All applications". */
            IPTR a = (IPTR)MUIV_List_Active_Off;
            GetAttr(MUIA_List_Active, lst_side, &a);
            char want_cat[32] = "";
            int want_view = ST_ALL;
            if ((LONG)a == MUIV_List_Active_Off) break;
            if ((LONG)a >= SIDE_FIRST_CAT && (LONG)a - SIDE_FIRST_CAT < cat_count)
                snprintf(want_cat, sizeof want_cat, "%s", cat_names[a - SIDE_FIRST_CAT]);
            else want_view = (LONG)a < SIDE_VIEWS ? (int)a : ST_ALL;
            /* Only a choice that differs is one: making the side list again
               (every refill does) sets its active row, and that must not
               be read as the user choosing it, or each refill would start
               the next (measured: the window refilled without end). */
            if (!strcmp(want_cat, cat_chosen) && want_view == view_state) break;
            snprintf(cat_chosen, sizeof cat_chosen, "%s", want_cat);
            view_state = want_view;
            logline("filter category=%s state=%d", cat_chosen[0] ? cat_chosen : "all", view_state);
            refill_keep("filter");
        } break;
        case ID_ONLYINST: {
            IPTR on = 0;
            GetAttr(MUIA_Selected, chk_inst, &on);
            if (!!on == only_installed) break;
            only_installed = !!on;
            logline("filter installed-only=%d", only_installed);
            refill_keep("installed only");
        } break;
        case ID_SELECT:  show_detail(); break;
        case ID_DETAILS: page_open(-1); break;
        case ID_ROWDOUBLE: {
            /* A double click on a row opens its page; on the row's small
               button it is two presses of that button, of which the
               second finds the window busy. */
            IPTR col = 0;
            GetAttr(MUIA_Listview_ClickColumn, lst_list, &col);
            if ((LONG)col != ACTION_COLUMN) page_open(-1);
        } break;
        case ID_ROWCLICK: {
            IPTR col = 0;
            GetAttr(MUIA_Listview_ClickColumn, lst_list, &col);
            if ((LONG)col == ACTION_COLUMN && selected() && !busy_now()) {
                /* The row was selected by the same click; its buttons are
                   worked out before one of them is pressed for it. */
                show_detail();
                logline("row button pressed for %s", selected()->id);
                if (main_action) DoMethod(app, MUIM_Application_ReturnID, main_action);
            }
        } break;
        case ID_BACK:    page_close(); break;
        case ID_PREV:    show_shot(shot_now - 1); break;
        case ID_NEXT:    show_shot(shot_now + 1); break;
        case ID_MAIN:
            /* What the button says, decided when it was last drawn. */
            if (main_action) DoMethod(app, MUIM_Application_ReturnID, main_action);
            break;
        case ID_UPDATE:  if (!busy_now()) { logline("update chosen"); begin(JOB_UPDATE, NULL, "window"); } break;
        case ID_INSTALL: { pkg_entry *e = selected(); if (e && can.install) { logline("install pressed for %s", e->id); begin(JOB_INSTALL, e->id, "window"); } } break;
        case ID_REMOVE:  { pkg_entry *e = selected(); if (e && can.remove) { logline("remove pressed for %s", e->id); begin(JOB_REMOVE, e->id, "window"); } } break;
        case ID_OPEN:    { pkg_entry *e = selected(); if (e && can.open) open_folder(e->id); } break;
        case ID_RUN:     { pkg_entry *e = selected(); if (e && can.run) { logline("run pressed for %s", e->id); begin(JOB_RUN, e->id, "window"); } } break;
        case ID_VERIFY:  { pkg_entry *e = selected(); if (e && can.verify) { logline("verify pressed for %s", e->id); begin(JOB_VERIFY, e->id, "window"); } } break;
        case ID_UPGRADE: { pkg_entry *e = selected(); if (e && can.upgrade) { logline("upgrade pressed for %s", e->id); begin(JOB_PREVIEW_UPGRADE, e->id, "window"); } } break;
        case ID_ROLLBACK:{ pkg_entry *e = selected(); if (e && can.rollback) { logline("rollback pressed for %s", e->id); begin(JOB_PREVIEW_ROLLBACK, e->id, "window"); } } break;
        case ID_CHECK:   logline("check for a newer version pressed"); begin(JOB_CHECK_RELEASE, NULL, "window"); break;
        case ID_SELFUPDATE: logline("update arospkg pressed"); su_begin(); break;
        case ID_CANCEL:  if (in_flight) { cur.cancel = 1; if (active) active->cancel_asked = 1; status("cancelling..."); logline("cancel pressed for %s", cur.id); } break;
        case ID_PROCEED: proceed("confirmed"); break;
        case ID_DECLINE: decline("declined"); break;
        }
        if (sigs) {
            sigs = Wait(sigs | portsig | rxsig | lk_signal() | SIGBREAKF_CTRL_C);
            if (sigs & SIGBREAKF_CTRL_C) {
                if (confirming) { decline("declined (break)"); break; }
                if (in_flight) close_requested(); else break;
            }
            if (sigs & portsig) {
                pm_msg *m;
                while ((m = (pm_msg *)GetMsg(port))) { if (on_message(m)) quit = 1; FreeVec(m); }
            }
            if (sigs & rxsig) rexx_poll(rexx_command);
            if (sigs & lk_signal()) lk_poll();
        }
        /* The summary column is sized to the list's width, so a window
           that changed size has its rows made again. */
        if (row_list_width && list_rows_width() > 0 && list_rows_width() != row_list_width && !busy_now()) {
            refill_keep("resize");
            /* Also when the refill made no rows: once per change of size. */
            row_list_width = list_rows_width();
        }
        if (rexx_quit && !busy_now()) break;
        if (su_handed_off) break;
    }

    /* The worker's last message says the operation ended; whether the task
       has left the system is a separate fact, measured here before the code
       it ran is unloaded. */
    logline("worker task gone %ld ms after done", job_wait_gone(3000, still_waiting));
    for (i = 0; i < MAXWAITS; i++)
        if (waits[i].m) { rexx_reply(waits[i].m, RX_RC_ERROR, NULL, "CLOSING PkgManager has quit"); waits[i].m = NULL; }
    rexx_close("CLOSING PkgManager has quit");
    row_images_free();
    if (side_badge) DoMethod(lst_sidelist, MUIM_List_DeleteImage, (IPTR)side_badge);
    if (side_badgeobj) MUI_DisposeObject(side_badgeobj);
    MUI_DisposeObject(app);
    /* A link still being opened runs this program's code. */
    lk_finish();
    lk_free();
    pic_exit();
    if (title_font) CloseFont(title_font);
    pkg_entries_free(&rows);
    pkg_updates_free(&upd);
    DeleteMsgPort(port);
    logline("quit");
    /* Last: an update waiting for this window starts once this is gone. */
    su_announce_stop();
    if (logf) fclose(logf);
    return RETURN_OK;
}

int main(int argc, char **argv)
{
    return pkg_run_with_stack(real_main, argc, argv);
}
