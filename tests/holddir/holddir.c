/* holddir -- hold a shared lock on a directory, as gl.library's helper does
 * with the drawer of the first program after a restart that opens it.
 *
 *   holddir DIR SECONDS [EXCLUSIVE]
 *
 * Testing only; not part of any release archive. Started with Run, it keeps
 * DIR in use for SECONDS, so apkg remove and install can be tried on a
 * package drawer that another program holds (issue #1). */
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    BPTR l;
    if (argc < 3) { printf("usage: holddir DIR SECONDS [EXCLUSIVE]\n"); return 5; }
    l = Lock((CONST_STRPTR)argv[1], argc > 3 ? EXCLUSIVE_LOCK : SHARED_LOCK);
    if (!l) { printf("holddir: cannot lock %s (%ld)\n", argv[1], (long)IoErr()); return 10; }
    printf("holddir: holding %s\n", argv[1]);
    Delay(atol(argv[2]) * 50);
    UnLock(l);
    return 0;
}
