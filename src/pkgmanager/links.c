/* Web addresses in PkgManager; see links.h. */
#include "links.h"
#include "linktext.h"

/* iffparse and openurl are opened where they are used, by local bases: a
   global base would let the SDK's auto-open make them a start-up
   requirement. */
#define __IFFPARSE_NOLIBBASE__
#define __IFFPARSE_LIBBASE iff_base
#define OPENURL_BASE_NAME ou_base
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/intuition.h>
#include <proto/muimaster.h>
#include <proto/iffparse.h>
#include <proto/openurl.h>
#include <libraries/mui.h>
#include <libraries/iffparse.h>
#include <dos/dostags.h>
#include <clib/alib_protos.h>
#include <stdio.h>
#include <string.h>

/* Urltext.mcc's published interface (mui/Urltext_mcc.h, which the Pi SDK
   does not have): the few names used here. */
#define MUIC_Urltext            "Urltext.mcc"
#define MUIA_Urltext_Url        0xFEC900D0ul
#define MUIA_Urltext_Text       0xFEC900D1ul
#define MUIA_Urltext_SetMax     0xFEC900D7ul
#define MUIA_Urltext_DoOpenURL  0xFEC900D8ul
#define MUIA_Urltext_NoMenu     0xFEC900D9ul

#ifndef MAKE_ID
#define MAKE_ID(a,b,c,d) ((ULONG)(a)<<24 | (ULONG)(b)<<16 | (ULONG)(c)<<8 | (ULONG)(d))
#endif

typedef struct {
    Object *row, *box, *link, *copy;
    char url[LK_URLMAX];
    char label[96];
    char help[LK_URLMAX + 120];
    int  openable;                 /* 0: characters a Shell or ARexx line would act on */
} lk_row;

static struct {
    Object *group, *owner;
    int label_max;
    lk_row r[LK_ROWS];
} areas[LK_AREAS];

static lk_setup S;
static Object *app_obj;
static struct MsgPort *reply_port;
static int pending;                /* opens handed to a process, not yet replied */
static int urltext = -1;           /* -1 not tried, 0 class missing, 1 in use */

/* The request a link process carries, and its answer. */
typedef struct {
    struct Message m;
    int  result;                   /* LK_R_... */
    char url[LK_URLMAX];
    char label[96];
    int  area;
} lk_msg;
enum { LK_R_HANDED, LK_R_NOLIB, LK_R_REFUSED };

#define MAX_PENDING 4

static void say(const char *t) { if (S.status) S.status(t); }
#define LOG(...) do { if (S.log) S.log(__VA_ARGS__); } while (0)

/* ---------------------------------------------------------------- objects */

static Object *new_link(lk_row *r)
{
    Object *o = NULL;
    if (urltext != 0) {
        /* Drawing only: its own opening calls OpenURL in this task (the
           window would wait for a browser to start) and keeps 255
           characters of the address, so both are ours. */
        o = MUI_NewObject(MUIC_Urltext,
                          MUIA_Urltext_Url, (IPTR)r->url,
                          MUIA_Urltext_Text, (IPTR)r->label,
                          MUIA_Urltext_DoOpenURL, FALSE,
                          MUIA_Urltext_NoMenu, TRUE,
                          MUIA_Urltext_SetMax, FALSE,
                          MUIA_ShortHelp, (IPTR)r->help,
                          TAG_DONE);
        if (urltext < 0) {
            urltext = o != NULL;
            LOG("links: %s", o ? "Urltext.mcc draws them" : "Urltext.mcc not available, plain text links");
        }
    }
    if (!o)
        o = TextObject,
                MUIA_Text_Contents, (IPTR)r->label,
                MUIA_Text_PreParse, (IPTR)"\33u",
                MUIA_InputMode, MUIV_InputMode_RelVerify,
                MUIA_ShowSelState, FALSE,
                MUIA_ShortHelp, (IPTR)r->help,
                MUIA_CycleChain, 1,
            End;
    return o;
}

static void notify_link(Object *o, int id)
{
    if (!o || !app_obj) return;
    if (urltext == 1)
        /* Zune's Immediate mode (Urltext's) sets Selected on a press and
           never Pressed. */
        DoMethod(o, MUIM_Notify, MUIA_Selected, TRUE, (IPTR)app_obj, 2, MUIM_Application_ReturnID, id);
    else
        DoMethod(o, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app_obj, 2, MUIM_Application_ReturnID, id);
}

Object *lk_area(int area, int label_max)
{
    Object *g;
    int i;
    areas[area].label_max = label_max;
    g = VGroup, MUIA_ShowMe, FALSE, MUIA_Group_VertSpacing, 1, End;
    if (!g) return NULL;
    for (i = 0; i < LK_ROWS; i++) {
        lk_row *r = &areas[area].r[i];
        r->row = HGroup, MUIA_ShowMe, FALSE,
                /* The link as wide as its text, the rest a spacer: Urltext
                   limits its own width, and a row that cannot grow made
                   the whole window narrow (AROS One 1.3). */
                Child, (r->box = HGroup, MUIA_InnerLeft, 0, MUIA_InnerRight, 0,
                        MUIA_InnerTop, 0, MUIA_InnerBottom, 0, MUIA_Group_Spacing, 0,
                        MUIA_HorizWeight, 0,
                        Child, (r->link = TextObject, MUIA_Text_Contents, (IPTR)"", MUIA_Text_SetMin, FALSE, End),
                    End),
                Child, HSpace(0),
                Child, (r->copy = TextObject,
                        ButtonFrame,
                        MUIA_Background, MUII_ButtonBack,
                        MUIA_Text_Contents, (IPTR)"Copy",
                        MUIA_Text_PreParse, (IPTR)"\33c",
                        MUIA_InputMode, MUIV_InputMode_RelVerify,
                        MUIA_HorizWeight, 0,
                        MUIA_CycleChain, 1,
                        MUIA_ShortHelp, (IPTR)"Put the whole address on the clipboard",
                    End),
            End;
        if (r->row) DoMethod(g, OM_ADDMEMBER, (IPTR)r->row);
        r->url[0] = 0;
    }
    areas[area].group = g;
    return g;
}

void lk_owner(int area, Object *window) { areas[area].owner = window; }

void lk_connect(Object *app)
{
    int a, i;
    app_obj = app;
    for (a = 0; a < LK_AREAS; a++)
        for (i = 0; i < LK_ROWS; i++) {
            lk_row *r = &areas[a].r[i];
            int id = LK_ID_BASE + (a * LK_ROWS + i) * 2;
            if (r->copy) DoMethod(r->copy, MUIM_Notify, MUIA_Pressed, FALSE, (IPTR)app_obj, 2,
                                  MUIM_Application_ReturnID, id + 1);
        }
}

/* A new address for a row: a new link object, since Urltext takes its
   address only when it is made. Swapped like replace_pic in main.c. */
static void set_row(int area, int i, const char *url, size_t len)
{
    lk_row *r = &areas[area].r[i];
    Object *o, *old = r->link;
    memcpy(r->url, url, len); r->url[len] = 0;
    r->openable = !lk_unsafe(r->url);
    lk_label(r->url, r->label, sizeof r->label, areas[area].label_max);
    snprintf(r->help, sizeof r->help, "%s\n%s", r->url,
             r->openable ? "Click to open it in your browser; Copy puts it on the clipboard."
                         : "This address can only be copied: it holds characters a browser start would act on.");
    o = new_link(r);
    if (!o) return;
    DoMethod(r->box, MUIM_Group_InitChange);
    if (old) { DoMethod(r->box, OM_REMMEMBER, (IPTR)old); MUI_DisposeObject(old); }
    DoMethod(r->box, OM_ADDMEMBER, (IPTR)o);
    DoMethod(r->box, MUIM_Group_ExitChange);
    r->link = o;
    notify_link(o, LK_ID_BASE + (area * LK_ROWS + i) * 2);
}

void lk_show(int area, const char *const *texts, int n)
{
    char found[LK_ROWS][LK_URLMAX];
    int nf = 0, t, i;
    if (area < 0 || area >= LK_AREAS || !areas[area].group) return;
    for (t = 0; t < n && nf < LK_ROWS; t++) {
        const char *p = texts[t], *at;
        size_t len;
        if (!p) continue;
        while (nf < LK_ROWS && (at = p, len = lk_next_url(p, &at)) > 0) {
            int dup = 0;
            p = at + len;
            if (len >= LK_URLMAX) continue;
            for (i = 0; i < nf; i++)
                if (strlen(found[i]) == len && !memcmp(found[i], at, len)) dup = 1;
            if (dup) continue;
            memcpy(found[nf], at, len); found[nf][len] = 0;
            nf++;
        }
    }
    for (i = 0; i < LK_ROWS; i++) {
        lk_row *r = &areas[area].r[i];
        if (i < nf && strcmp(r->url, found[i])) set_row(area, i, found[i], strlen(found[i]));
        if (i >= nf) r->url[0] = 0;
        SetAttrs(r->row, MUIA_ShowMe, i < nf, TAG_DONE);
    }
    SetAttrs(areas[area].group, MUIA_ShowMe, nf > 0, TAG_DONE);
}

void lk_show1(int area, const char *text)
{
    const char *t[1];
    t[0] = text;
    lk_show(area, t, 1);
}

/* -------------------------------------------------------------- clipboard */

static int copy_text(const char *s)
{
    struct Library *iff_base = OpenLibrary((CONST_STRPTR)"iffparse.library", 39);
    struct IFFHandle *iff;
    int ok = 0;
    LONG len = (LONG)strlen(s);
    if (!iff_base) return 0;
    if ((iff = AllocIFF())) {
        if ((iff->iff_Stream = (IPTR)OpenClipboard(0))) {
            InitIFFasClip(iff);
            if (!OpenIFF(iff, IFFF_WRITE)) {
                ok = !PushChunk(iff, MAKE_ID('F','T','X','T'), MAKE_ID('F','O','R','M'), IFFSIZE_UNKNOWN)
                     && !PushChunk(iff, 0, MAKE_ID('C','H','R','S'), len)
                     && WriteChunkBytes(iff, (APTR)s, len) == len
                     && !PopChunk(iff)
                     && !PopChunk(iff);
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    CloseLibrary(iff_base);
    return ok;
}

/* ----------------------------------------------------------------- opening */

/* The link process: hands one address to OpenURL and answers. OpenURL may
   wait seconds for a browser it started to show its port; this process
   waits, not the window. */
static void open_proc(void)
{
    struct Process *me = (struct Process *)FindTask(NULL);
    lk_msg *m;
    struct Library *ou_base;
    WaitPort(&me->pr_MsgPort);
    m = (lk_msg *)GetMsg(&me->pr_MsgPort);
    ou_base = OpenLibrary((CONST_STRPTR)"openurl.library", 1);
    if (!ou_base) m->result = LK_R_NOLIB;
    else {
        /* No tags: show, bring to front, new window and launch as the user
           set them in Prefs/OpenURL. */
        m->result = URL_OpenA((STRPTR)m->url, NULL) ? LK_R_HANDED : LK_R_REFUSED;
        CloseLibrary(ou_base);
    }
    /* Under Forbid until this process is gone, so the window, which waits
       for this answer before it quits, never unloads code still running. */
    Forbid();
    ReplyMsg(&m->m);
}

static void open_url(int area, lk_row *r)
{
    lk_msg *m;
    struct Process *p;
    BPTR in, out;
    char line[200];
    if (!r->openable) {
        snprintf(line, sizeof line, "not opened: %s holds characters a browser start would act on -- use Copy", r->label);
        say(line);
        LOG("link open refused (unsafe characters): %s", r->url);
        return;
    }
    if (!reply_port || pending >= MAX_PENDING) {
        say(reply_port ? "still opening earlier links -- try again in a moment" : "links cannot be opened here -- use Copy");
        LOG("link open not started (%s): %s", reply_port ? "busy" : "no port", r->url);
        return;
    }
    m = (lk_msg *)AllocVec(sizeof *m, MEMF_PUBLIC | MEMF_CLEAR);
    if (!m) { say("not enough memory to open the link -- use Copy"); return; }
    m->m.mn_ReplyPort = reply_port;
    m->m.mn_Length = sizeof *m;
    m->area = area;
    snprintf(m->url, sizeof m->url, "%s", r->url);
    snprintf(m->label, sizeof m->label, "%s", r->label);
    in  = Open((CONST_STRPTR)"NIL:", MODE_OLDFILE);
    out = Open((CONST_STRPTR)"NIL:", MODE_NEWFILE);
    /* No current or home drawer: a browser OpenURL starts from here
       inherits them, and would keep PkgManager's drawer in use. */
    p = CreateNewProcTags(NP_Entry,       (IPTR)open_proc,
                          NP_Name,        (IPTR)"PkgManager link",
                          NP_StackSize,   32768,
                          NP_CurrentDir,  (IPTR)BNULL,
                          NP_HomeDir,     (IPTR)BNULL,
                          NP_Input,       (IPTR)in,
                          NP_Output,      (IPTR)out,
                          NP_CloseInput,  TRUE,
                          NP_CloseOutput, TRUE,
                          TAG_DONE);
    if (!p) {
        if (in) Close(in);
        if (out) Close(out);
        FreeVec(m);
        say("could not start opening the link -- use Copy");
        LOG("link open: no process for %s", r->url);
        return;
    }
    pending++;
    PutMsg(&p->pr_MsgPort, &m->m);
    snprintf(line, sizeof line, "opening %s ...", r->label);
    say(line);
    LOG("link open %s", r->url);
}

static void answered(lk_msg *m, int report)
{
    char line[300];
    pending--;
    LOG("link open %s: %s", m->url, m->result == LK_R_HANDED ? "handed to OpenURL"
        : m->result == LK_R_NOLIB ? "no openurl.library" : "OpenURL refused");
    if (!report) return;
    if (m->result == LK_R_HANDED) {
        /* All OpenURL confirms: a browser took the address or was started
           with it. Whether the page loads is the browser's business. */
        snprintf(line, sizeof line, "passed %s to your browser", m->label);
        say(line);
        return;
    }
    snprintf(line, sizeof line, "could not open %s -- use Copy", m->label);
    say(line);
    {
        Object *w = areas[m->area].owner;
        IPTR open = 0;
        if (w) GetAttr(MUIA_Window_Open, w, &open);
        MUI_Request(app_obj, open ? w : NULL, 0, (STRPTR)"PkgManager", (STRPTR)"OK",
                    m->result == LK_R_NOLIB
                    ? (STRPTR)"This address could not be opened:\nopenurl.library is not installed.\n\n%s\n\n"
                              "Copy puts it on the clipboard, to paste into your browser."
                    : (STRPTR)"This address could not be opened:\nOpenURL found no browser to pass it to\n"
                              "(see Prefs/OpenURL).\n\n%s\n\n"
                              "Copy puts it on the clipboard, to paste into your browser.",
                    (IPTR)m->label);
    }
}

int lk_init(const lk_setup *s)
{
    S = *s;
    reply_port = CreateMsgPort();
    return reply_port != NULL;
}

ULONG lk_signal(void) { return reply_port ? 1UL << reply_port->mp_SigBit : 0; }

void lk_poll(void)
{
    lk_msg *m;
    if (!reply_port) return;
    while ((m = (lk_msg *)GetMsg(reply_port))) { answered(m, 1); FreeVec(m); }
}

void lk_finish(void)
{
    lk_msg *m;
    if (!reply_port) return;
    while (pending > 0) {
        WaitPort(reply_port);
        while ((m = (lk_msg *)GetMsg(reply_port))) { answered(m, 0); FreeVec(m); }
    }
    DeleteMsgPort(reply_port);
    reply_port = NULL;
}

int lk_event(unsigned long id)
{
    int a, i;
    lk_row *r;
    char line[200];
    if (id < LK_ID_BASE || id >= LK_ID_END) return 0;
    id -= LK_ID_BASE;
    a = (int)(id / 2) / LK_ROWS;
    i = (int)(id / 2) % LK_ROWS;
    r = &areas[a].r[i];
    if (!r->url[0]) return 1;
    if (id % 2 == 0) {
        /* Urltext stays Selected after a press (Zune's Immediate mode) and
           is then drawn in a pen that can be the background's; it is the
           window's active object too. Neither should outlast the click. */
        SetAttrs(r->link, MUIA_NoNotify, TRUE, MUIA_Selected, FALSE, TAG_DONE);
        if (areas[a].owner)
            SetAttrs(areas[a].owner, MUIA_Window_ActiveObject, MUIV_Window_ActiveObject_None, TAG_DONE);
        open_url(a, r);
        return 1;
    }
    if (copy_text(r->url)) {
        snprintf(line, sizeof line, "copied %s to the clipboard", r->label);
        LOG("link copy %s: ok", r->url);
    } else {
        snprintf(line, sizeof line, "could not put %s on the clipboard", r->label);
        LOG("link copy %s: failed", r->url);
    }
    say(line);
    return 1;
}
