/* apkg self-update; see selfupdate.c and docs/self-update.md. */
#ifndef APKG_SELFUPDATE_H
#define APKG_SELFUPDATE_H
/* current: this apkg's version ("0.3.2"); root: the package root, only to
   see whether an operation holds its lock. Returns a Shell return code. */
int apkg_self_update(const char *current, const char *root, int check_only);
/* Testing only: another release base URL (https, ending in "/"), a local
   SHA256SUMS instead of the release's, and a failure at the swap
   ("swap": the second rename fails; "stop": stop after the first). */
void su_test_from(const char *url);
void su_test_sums(const char *file);
void su_test_fail(const char *where);
#endif
