/* How apkg decorates what it prints, decided once per command from the
 * handle it writes to. docs/backlog/terminal-presentation.md has the modes;
 * aros-xterm/docs/apkg-integration.md the terminal's side. Output that is
 * not interactive, and --json, are never decorated: the test suites grade
 * those byte for byte. */
#ifndef APKG_PRESENT_H
#define APKG_PRESENT_H

enum { PR_COLOR_AUTO, PR_COLOR_ALWAYS, PR_COLOR_NEVER };
enum { PR_GREEN, PR_YELLOW, PR_RED };

void pr_init(int json, int plain, int color_mode);
int  pr_rich(void);        /* the terminal answered the presentation query */
int  pr_cols(void);        /* the window's columns, 0 when unknown */
int  pr_wants_progress(void);
/* Escape sequences, or "" when the decoration is off. */
const char *pr_bold(void);
const char *pr_color(int c);
const char *pr_off(void);  /* ends whatever pr_bold or pr_color started */
/* A word with its colour; the word is always printed. */
void pr_word(int c, const char *s);
/* Install progress: one line rewritten in place on a terminal that can, a
 * line every 10% on another interactive window, nothing elsewhere. */
void pr_progress(const char *id, const char *phase, unsigned long done, unsigned long total);
void pr_end_line(void);    /* before anything else is printed */

#endif
