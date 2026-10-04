#include "hint.h"
#include <stdio.h>
#include <strings.h>

/* The command a user can copy to reach the drawer. Without --root it would
   look in SYS:Packages, so a custom root is passed on, quoted for the Shell:
   inside quotes AmigaDOS escapes '"' and '*' with a leading '*'. */
void open_hint(char *out, size_t n, const char *root, const char *id)
{
    size_t k;
    const char *p;
    if (!root || !strcasecmp(root, "SYS:Packages")) {
        snprintf(out, n, "apkg open %s", id);
        return;
    }
    k = (size_t)snprintf(out, n, "apkg --root \"");
    for (p = root; *p && k + 3 < n; p++) {
        if (*p == '"' || *p == '*') out[k++] = '*';
        out[k++] = *p;
    }
    snprintf(out + k, n - k, "\" open %s", id);
}
