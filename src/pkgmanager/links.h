/* Web addresses in PkgManager: one way to show, open and copy them.
 *
 * Every http:// or https:// address found in a text the window shows gets a
 * row below that text: the address (shortened on screen when it is long,
 * the full one as bubble help) and a Copy button. Clicking the address hands
 * the full address to openurl.library, which passes it to the browser the
 * user chose in Prefs/OpenURL; that happens in a small process of its own,
 * so the window never waits for a browser to start. Copy puts the full
 * address on the clipboard (unit 0). Other schemes stay text.
 *
 * Urltext.mcc draws the address when the class is installed; otherwise a
 * plain Zune Text does. Neither is required to start: without
 * openurl.library a click says so and Copy still works. */
#ifndef PM_LINKS_H
#define PM_LINKS_H

#include <exec/types.h>
#include <intuition/classusr.h>
#include <stddef.h>
#include "linktext.h"

/* The places with links. Each is a group made by lk_area. */
enum { LK_HOME, LK_RELEASE, LK_DETAIL, LK_CONFIRM, LK_STATUS, LK_AREAS };
#define LK_ROWS 4                  /* at most this many links per place     */

/* Return IDs of the link rows: LK_ID_BASE + (area * LK_ROWS + row) * 2,
   +1 for Copy. Kept clear of the window's own IDs. */
#define LK_ID_BASE 1000
#define LK_ID_END  (LK_ID_BASE + LK_AREAS * LK_ROWS * 2)

typedef struct {
    /* The main window's status line, set as it is: what links report there
       is not searched for addresses again. */
    void (*status)(const char *);
    void (*log)(const char *fmt, ...);      /* --log, or nothing             */
} lk_setup;

/* Once, before the windows are made; 0 when the reply port could not be
   made (links then show and copy, and say they cannot open). */
int lk_init(const lk_setup *s);
/* The group for one place, for the window tree; `label_max` is how many
   characters of an address are shown; `owner` is set later (lk_owner). */
Object *lk_area(int area, int label_max);
void lk_owner(int area, Object *window);
/* After the application object exists: connects the rows' notifications. */
void lk_connect(Object *app);

/* The addresses now shown in `area`: those in texts[0..n-1], in order,
   without repeats, at most LK_ROWS. Rows that keep their address are left
   alone. n == 0 hides the area. */
void lk_show(int area, const char *const *texts, int n);
void lk_show1(int area, const char *text);

/* A return ID from the main loop: 1 when it was a link's. */
int lk_event(unsigned long id);
/* The signal of the reply port (0 without one), and what to do when it
   arrives: report finished opens. */
ULONG lk_signal(void);
void lk_poll(void);
/* At quit, after the windows are gone: waits for every open still in
   progress, since its process runs this program's code. */
void lk_finish(void);

#endif
