/* Web addresses in running text: where they are, how they are shown
 * shortened, and which may be handed to a browser. No AROS calls, so the
 * host tests cover it (tests/test_linktext.c). */
#ifndef PM_LINKTEXT_H
#define PM_LINKTEXT_H

#include <stddef.h>

#define LK_URLMAX 1024             /* longer addresses are left as text */

/* The next http:// or https:// address in `p`: its length, and *at its
   start; 0 when there is none. On entry *at is the start of the whole text
   (an address must begin there or after a character that cannot be part of
   a word). Sentence punctuation after it is left out; other schemes are
   not addresses here. */
size_t lk_next_url(const char *p, const char **at);

/* `url` as shown in at most `max` characters: its start, "...", its end.
   Never what is opened or copied. */
void lk_label(const char *url, char *out, size_t n, int max);

/* `in` with every address longer than `max` characters written as its
   label: for one-line texts that would otherwise widen their window. */
void lk_shorten(const char *in, char *out, size_t n, int max);

/* 1 when the address holds a character that would act if it reached a
   Shell or ARexx command line: OpenURL puts the address into one unquoted
   (to start a browser, or to tell a running one). Such an address can be
   copied, not opened. */
int lk_unsafe(const char *url);

#endif
