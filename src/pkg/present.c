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
static int line_open, last_tenth;
static char last_phase[16];

void pr_init(int json, int plain, int color_mode)
{
    XtyControl q;
    BPTR out = Output();
    interactive = out && IsInteractive(out);
    plain_mode = plain;
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
            if ((known & XTY_PRESENT_KNOWN_LINES) &&
                (q.values[3] & (XTY_LINE_CR | XTY_LINE_ERASE_EOL)) == (XTY_LINE_CR | XTY_LINE_ERASE_EOL))
                inplace = 1;
            if ((known & XTY_PRESENT_KNOWN_GEOMETRY) && q.values[4] > 0 && q.values[5] > 0)
                cols = (int)q.values[5];
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
    return c == PR_GREEN ? "\033[32m" : c == PR_YELLOW ? "\033[33m" : "\033[31m";
}
/* SGR 0 ends bold and colour alike; with colour only, 39 is the promised
   reset for the foreground. */
const char *pr_off(void) { return bold ? "\033[0m" : color ? "\033[39m" : ""; }

void pr_word(int c, const char *s)
{
    if (color) printf("%s%s%s", pr_color(c), s, pr_off());
    else fputs(s, stdout);
}

static void kb(char *b, size_t n, unsigned long v) { snprintf(b, n, "%lu KB", (v + 1023) / 1024); }

void pr_progress(const char *id, const char *phase, unsigned long done, unsigned long total)
{
    char a[32], b[32], bar[24], line[160];
    int w, filled, i, pct;
    /* Only phases that take a while are shown: the download by bytes, and
       on a line that can be redrawn, the extraction by files. verify and
       publish come as one step each, where a percentage says nothing. */
    if (!interactive || total == 0) return;
    if (strcmp(phase, "download") && (strcmp(phase, "extract") || !inplace || total < 2)) return;
    if (strcmp(phase, last_phase)) {
        pr_end_line();
        snprintf(last_phase, sizeof last_phase, "%s", phase);
        last_tenth = 0;
    }
    pct = (int)(done * 100 / total);
    if (!inplace) {
        /* The plain interactive path: a complete line every 10%, the first
           at 10%; not on a terminal that answered but cannot redraw a line. */
        if (rich || done == 0 || pct / 10 == last_tenth) return;
        last_tenth = pct / 10;
        if (!strcmp(phase, "download")) {
            kb(a, sizeof a, done); kb(b, sizeof b, total);
            printf("  %s %d%% (%s of %s)\n", phase, pct, a, b);
        } else printf("  %s %d%% (%lu of %lu)\n", phase, pct, done, total);
        fflush(stdout);
        return;
    }
    /* libpkg prints its own lines before the first byte (fetching, redirect)
       and after the last (verified and cached), so the line is drawn only
       between them and taken down at the end. */
    if (done == 0) return;
    if (!strcmp(phase, "download")) { kb(a, sizeof a, done); kb(b, sizeof b, total); }
    else { snprintf(a, sizeof a, "%lu", done); snprintf(b, sizeof b, "%lu files", total); }
    w = (cols && cols < 60) ? 0 : 20;      /* no bar in a narrow window */
    filled = (int)((unsigned long)w * done / total);
    for (i = 0; i < w; i++) bar[i] = i < filled ? '#' : '.';
    bar[w] = 0;
    snprintf(line, sizeof line, w ? "  %s  %s  [%s]  %3d%%  %s of %s" : "  %s  %s  %s%3d%%  %s of %s",
             id, phase, bar, pct, a, b);
    if (cols && (int)strlen(line) >= cols) line[cols - 1] = 0;   /* never wrap */
    printf("\r%s\033[K", line);
    fflush(stdout);
    line_open = 1;
    if (done >= total) pr_end_line();
}

void pr_end_line(void)
{
    if (!line_open) return;
    printf("\r\033[K");
    fflush(stdout);
    line_open = 0;
}
