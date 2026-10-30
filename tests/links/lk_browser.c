/* Test stand-in for a browser: OpenURL starts it as "lk_browser %u" and it
 * appends what it was given, one line per start, to RAM:lk/opened.txt, so a
 * link test checks the exact address handed over, not a rendered page. */
#include <stdio.h>

int main(int argc, char **argv)
{
    FILE *f = fopen("RAM:lk/opened.txt", "a");
    int i;
    if (!f) return 20;
    for (i = 1; i < argc; i++) fprintf(f, "%s%s", i > 1 ? "|" : "", argv[i]);
    fprintf(f, "\n");
    fclose(f);
    return 0;
}
