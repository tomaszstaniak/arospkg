/* apkg's presentation: see present.h. The one question asked of the system
 * is operation 13 on our own Output(), and only when that handle is
 * interactive. Nothing is inferred from anything else: not the window's
 * title, not TERM, not Input() (the contract's rule 2). */
#include "present.h"
#include "../../third_party/aros-xterm/aros_tty_client.h"
#include <proto/dos.h>
#include <stdio.h>
#include <string.h>

static int interactive, plain_mode, rich, color, bold, inplace, cols;
/* The window reads SGR 30-37 as screen pens (XTY_COLOR_PENS): colours then
   go as 38;5;n, which keeps the palette meaning. It changes how a colour is
   written, never whether one is. */
static int pens;
static int last_tenth;
static char last_phase[16];
static unsigned long last_unknown_mib;
static int unknown_reported;

static void refresh_geometry(void)
{
    XtyControl q;
    if (!rich) return;
    /* Called only for a report we will actually print. A time-based cache
       misses a resize between two milestones in the same second. */
    cols = 0;
    memset(&q, 0, sizeof q);
    q.version = XTY_CONTROL_VERSION;
    q.operation = XTY_GET_PRESENTATION;
    if (aros_tty_control(Output(), &q) == DOSTRUE &&
        (q.values[0] & XTY_PRESENT_KNOWN_GEOMETRY) && q.values[4] > 0 && q.values[5] > 0)
        cols = q.values[5] > 4096 ? 4096 : (int)q.values[5];
}

void pr_init(int json, int plain, int color_mode)
{
    XtyControl q;
    BPTR out = Output();
    rich = color = bold = inplace = cols = last_tenth = pens = 0;
    last_phase[0] = 0;
    unknown_reported = 0;
    last_unknown_mib = 0;
    interactive = out && IsInteractive(out);
    plain_mode = plain || json;
    if (json || !interactive) return;     /* as today, byte for byte */
    if (!plain) {
        memset(&q, 0, sizeof q);
        q.version = XTY_CONTROL_VERSION;
        q.operation = XTY_GET_PRESENTATION;
        /* CON: and every other handler that does not know the action answers
           ERROR_ACTION_NOT_KNOWN at once: the plain path. */
        fflush(stdout);
        if (aros_tty_control(out, &q) == DOSTRUE) {
            unsigned known = q.values[0];
            rich = 1;
            if ((known & XTY_PRESENT_KNOWN_COLORS) && q.values[1] >= XTY_COLOR_ANSI16) color = 1;
            if ((known & XTY_PRESENT_KNOWN_STYLES) && (q.values[2] & XTY_STYLE_BOLD_RESET)) bold = 1;
            /* Only a known interpretation counts; an unknown one keeps the
               basic codes, which every terminal before this field meant as
               ANSI. */
            if ((known & XTY_PRESENT_KNOWN_INTERPRETATION) && (q.values[6] & XTY_COLOR_PENS)) pens = 1;
            if ((known & XTY_PRESENT_KNOWN_LINES) &&
                (q.values[3] & (XTY_LINE_CR | XTY_LINE_ERASE_EOL)) == (XTY_LINE_CR | XTY_LINE_ERASE_EOL))
                inplace = 1;
            if ((known & XTY_PRESENT_KNOWN_GEOMETRY) && q.values[4] > 0 && q.values[5] > 0)
                cols = q.values[5] > 4096 ? 4096 : (int)q.values[5];
        }
    }
    if (plain) color = 0;
    if (color_mode == PR_COLOR_NEVER) color = 0;
    if (color_mode == PR_COLOR_ALWAYS && !plain) color = 1;
}

int pr_rich(void) { return rich; }
int pr_cols(void) { return cols; }
int pr_wants_progress(void) { return interactive && !plain_mode && (inplace || !rich); }

const char *pr_bold(void) { return bold ? "\033[1m" : ""; }
const char *pr_color(int c)
{
    if (!color) return "";
    if (pens) return c == PR_GREEN ? "\033[38;5;2m" : c == PR_YELLOW ? "\033[38;5;3m" : "\033[38;5;1m";
    return c == PR_GREEN ? "\033[32m" : c == PR_YELLOW ? "\033[33m" : "\033[31m";
}
/* SGR 0 ends bold and colour alike; with colour only, 39 is the promised
   reset for the foreground. 39 returns to the theme's default foreground,
   not to whatever pen was in use before; in pens mode too. */
const char *pr_off(void) { return bold ? "\033[0m" : color ? "\033[39m" : ""; }

void pr_word(int c, const char *s)
{
    if (color) printf("%s%s%s", pr_color(c), s, pr_off());
    else fputs(s, stdout);
}

static void wrap(const char *s, int indent, int first_indent)
{
    int limit = cols > 1 ? cols - 1 : cols == 1 ? 1 : 79;
    int room, first = 1;
    if (indent >= limit) indent = 0;
    room = limit - indent;
    do {
        size_t n = strcspn(s, "\n"), take = n;
        if (take > (size_t)room) {
            take = room;
            while (take && s[take] != ' ') take--;
            if (!take) take = room; /* paths and identifiers must not disappear */
        }
        printf("%*s%.*s\n", first && !first_indent ? 0 : indent, "", (int)take, s);
        s += take;
        if (*s == '\n') s++;
        else while (*s == ' ') s++;
        first = 0;
    } while (*s);
}

void pr_text(const char *s, int indent)
{
    wrap(s, indent, 1);
}

void pr_heading(const char *s)
{
    fputs(pr_bold(), stdout);
    pr_text(s, 0);
    fputs(pr_off(), stdout);
}

void pr_field(const char *label, const char *value, int c)
{
    char line[2048];
    snprintf(line, sizeof line, "%s: %s", label, value);
    if (c >= 0) fputs(pr_color(c), stdout);
    pr_text(line, 2);
    if (c >= 0) fputs(pr_off(), stdout);
}

void pr_search_heading(void)
{
    if (cols >= 80)
        pr_heading("Package                   Version           State          Description");
}

void pr_search_row(const char *id, const char *version, const char *state,
                   const char *summary, const char *target_note)
{
    if (cols >= 80 && strlen(id) <= 24 && strlen(version) <= 16 && strlen(state) <= 13) {
        printf("%s%-24s%s  %-16s  %-13s  ", pr_bold(), id, pr_off(), version, state);
        wrap(summary, 59, 0);
    } else {
        char title[160];
        snprintf(title, sizeof title, "%s %s", id, version);
        pr_heading(title);
        pr_field("State", state, !strcmp(state, "installed") ? PR_GREEN : -1);
        pr_text(summary, 2);
    }
    if (*target_note) pr_field("Target", target_note,
        strstr(target_note, "other ") || strstr(target_note, "unknown ") ? PR_YELLOW : -1);
    putchar('\n');
}

static void kb(char *b, size_t n, unsigned long v)
{
    if (v < 1024) snprintf(b, n, "%lu B", v);
    else snprintf(b, n, "%lu KiB", v / 1024);
}

void pr_progress(const char *id, const char *phase, unsigned long done, unsigned long total)
{
    char a[32], b[32], line[160];
    int pct, limit;
    const char *label;
    (void)id; /* The command heading identifies the package, not every frame. */
    if (!interactive || plain_mode) return;
    label = !strcmp(phase, "download") ? "Downloading" :
            !strcmp(phase, "verify") ? "Verifying archive" :
            !strcmp(phase, "extract") ? "Extracting files" :
            !strcmp(phase, "publish") ? "Publishing" : NULL;
    if (!label) return;
    if (strcmp(phase, last_phase)) {
        pr_end_line();
        snprintf(last_phase, sizeof last_phase, "%s", phase);
        last_tenth = 0;
        unknown_reported = 0;
    }
    /* Only download supplies a meaningful byte fraction. Other phases are
       stage notifications, including extract's sentinel 1/1 completion. */
    pct = total ? (done >= total ? 100 : (int)(100.0L * done / total)) : 0;
    if (!inplace) {
        /* The plain interactive path: a complete line every 10%, the first
           at 10%; not on a terminal that answered but cannot redraw a line. */
        if (rich || strcmp(phase, "download") || !total || done == 0 || pct / 10 == last_tenth) return;
        last_tenth = pct / 10;
        if (!strcmp(phase, "download")) {
            kb(a, sizeof a, done); kb(b, sizeof b, total);
            printf("  %s %d%% (%s of %s)\n", phase, pct, a, b);
        } else printf("  %s %d%% (%lu of %lu)\n", phase, pct, done, total);
        fflush(stdout);
        return;
    }
    if (!strcmp(phase, "download")) {
        if (!done) return; /* fetching/redirect messages precede the first byte */
        if (total && pct / 10 == last_tenth) return;
        last_tenth = pct / 10;
        refresh_geometry();
        limit = cols ? cols - 1 : 79;
        if (limit < 1) return;
        kb(a, sizeof a, done); kb(b, sizeof b, total);
        if (total) {
            snprintf(line, sizeof line, "Downloading %3d%%  %s / %s", pct, a, b);
            if ((int)strlen(line) > limit) {
                snprintf(line, sizeof line, "Downloading %d%%", pct);
                if ((int)strlen(line) > limit) snprintf(line, sizeof line, "%d%%", pct);
            }
        } else {
            /* With no total the transport has no completion callback. Leave
               complete lines so the engine's next printf cannot collide. */
            unsigned long mib = done / (1024UL * 1024UL);
            if (unknown_reported && mib == last_unknown_mib) return;
            unknown_reported = 1;
            last_unknown_mib = mib;
            pr_end_line();
            snprintf(line, sizeof line, "Downloading %s (total unknown)", a);
            if ((int)strlen(line) > limit) snprintf(line, sizeof line, "%s", a);
            printf("%.*s\n", limit, line);
            fflush(stdout);
            return;
        }
    } else {
        refresh_geometry();
        limit = cols ? cols - 1 : 79;
        if (limit < 1) return;
        if (done) {
            pr_end_line();
            if (!strcmp(phase, "verify")) { printf("%.*s\n", limit, "Archive verified"); fflush(stdout); }
            return;
        }
        snprintf(line, sizeof line, "%s...", label);
    }
    if (limit < (int)sizeof line && (int)strlen(line) > limit) line[limit] = 0;
    /* Operation 13 promises CR and erase-to-EOL, not a resize-stable
       cursor anchor or erasure of a previous wrapped logical line. Even a
       fresh width can race a resize. Complete milestone lines need neither:
       wrapped output remains history, never a half-erased transient frame. */
    printf("%s\n", line);
    fflush(stdout);
}

void pr_end_line(void)
{
    /* Progress reports are complete lines, including before error/cancel.
       Keep the boundary API so callers need not know the rendering mode. */
}
