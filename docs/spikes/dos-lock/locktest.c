/* Does Lock(path, EXCLUSIVE_LOCK) work as a process mutex for pkg?
 *
 * Three questions, in the order that matters:
 *   1. does an exclusive lock actually exclude a second one?
 *   2. is it released when the holder exits WITHOUT calling UnLock?
 *   3. can a locked file still be deleted, opened, renamed over?
 *
 * (2) decides the design. If the lock survives its owner, a crashed pkg
 * wedges every later run until reboot -- and a crash is exactly when the
 * next run must be able to start, because that is when recovery is owed.
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void report(const char *what, BPTR l)
{
    if (l)
        printf("%-22s OK   (lock=%p)\n", what, (void *)(IPTR)l);
    else
        printf("%-22s FAIL IoErr=%ld\n", what, (long)IoErr());
}


/* The whole experiment in one process, because an AROS Shell reliably
   accepts one command and then loses keyboard focus. */
static int suite(CONST_STRPTR path)
{
    BPTR a, b, fh;
    LONG rc;
    int fails = 0;

    printf("== locktest suite on %s ==\n", (const char *)path);

    DeleteFile(path);
    fh = Open(path, MODE_NEWFILE);
    if (!fh) { printf("setup: cannot create, IoErr=%ld\n", (long)IoErr()); return 20; }
    FPuts(fh, (CONST_STRPTR)"x\n");
    Close(fh);

    /* 1. does an exclusive lock exclude a second one? */
    a = Lock(path, EXCLUSIVE_LOCK);
    report("1 first exclusive", a);
    if (!a) return 20;

    b = Lock(path, EXCLUSIVE_LOCK);
    printf("2 second exclusive      %s IoErr=%ld  (want FAIL)\n",
           b ? "OK" : "FAIL", (long)IoErr());
    if (b) { fails++; UnLock(b); }

    b = Lock(path, SHARED_LOCK);
    printf("3 shared while excl     %s IoErr=%ld  (want FAIL)\n",
           b ? "OK" : "FAIL", (long)IoErr());
    if (b) { fails++; UnLock(b); }

    rc = DeleteFile(path);
    printf("4 delete while locked   %s IoErr=%ld  (want FAIL)\n",
           rc ? "OK" : "FAIL", (long)IoErr());
    if (rc) fails++;

    fh = Open(path, MODE_OLDFILE);
    printf("5 open while excl       %s IoErr=%ld\n",
           fh ? "OK" : "FAIL", (long)IoErr());
    if (fh) Close(fh);

    UnLock(a);

    b = Lock(path, EXCLUSIVE_LOCK);
    report("6 relock after UnLock", b);
    if (b) UnLock(b); else fails++;

    /* 7. THE ONE THAT DECIDES THE DESIGN.
       A child takes the exclusive lock and exits without releasing it.
       If the lock is gone afterwards, the OS reclaims it and a crashed
       pkg cannot wedge the next run. If it survives, it can. */
    {
        char cmd[256];
        snprintf(cmd, sizeof cmd, "RAM:locktest leak %s", (const char *)path);
        rc = SystemTags((CONST_STRPTR)cmd,
                        SYS_Input, (IPTR)BNULL, SYS_Output, (IPTR)BNULL, TAG_DONE);
    }
    printf("7 child 'leak' rc=%ld  (0 = it really held the lock)\n", (long)rc);
    if (rc != 0) {
        printf("   child never took the lock -- step 8 proves NOTHING\n");
        DeleteFile(path);
        return 20;
    }

    b = Lock(path, EXCLUSIVE_LOCK);
    printf("8 lock after child died %s IoErr=%ld\n",
           b ? "OK" : "FAIL", (long)IoErr());
    if (b) {
        printf("   => OS RECLAIMS the lock. No stale-lock logic needed.\n");
        UnLock(b);
    } else {
        printf("   => lock SURVIVES its owner. A crash wedges pkg until reboot.\n");
    }

    DeleteFile(path);
    printf("== unexpected results: %d ==\n", fails);
    return 0;
}

int main(int argc, char **argv)
{
    const char *cmd  = argc > 1 ? argv[1] : "help";
    CONST_STRPTR path = (CONST_STRPTR)(argc > 2 ? argv[2] : "T:locktest.tmp");
    BPTR l;

    if (!strcmp(cmd, "suite"))
        return suite(path);

    if (!strcmp(cmd, "make")) {
        BPTR fh = Open(path, MODE_NEWFILE);
        if (!fh) { printf("make: cannot create %s IoErr=%ld\n", (const char *)path, (long)IoErr()); return 20; }
        FPuts(fh, (CONST_STRPTR)"x\n");
        Close(fh);
        printf("make                   created %s\n", (const char *)path);
        return 0;
    }

    if (!strcmp(cmd, "try")) {              /* lock, report, release */
        l = Lock(path, EXCLUSIVE_LOCK);
        report("try exclusive", l);
        if (l) UnLock(l);
        return l ? 0 : 10;
    }

    if (!strcmp(cmd, "tryshared")) {
        l = Lock(path, SHARED_LOCK);
        report("try shared", l);
        if (l) UnLock(l);
        return l ? 0 : 10;
    }

    if (!strcmp(cmd, "hold")) {             /* hold, then release properly */
        long secs = argc > 3 ? atol(argv[3]) : 20;
        l = Lock(path, EXCLUSIVE_LOCK);
        report("hold exclusive", l);
        if (!l) return 10;
        printf("holding for %ld s, then UnLock\n", secs);
        Delay(secs * 50);
        UnLock(l);
        printf("hold                   released\n");
        return 0;
    }

    /* The one that decides the design: take the lock and exit without
       releasing it, the way a crash would. */
    if (!strcmp(cmd, "leak")) {
        l = Lock(path, EXCLUSIVE_LOCK);
        report("leak exclusive", l);
        if (!l) return 10;
        printf("exiting WITHOUT UnLock\n");
        return 0;
    }

    if (!strcmp(cmd, "delete")) {
        LONG ok = DeleteFile(path);
        printf("%-22s %s IoErr=%ld\n", "delete", ok ? "OK" : "FAIL", (long)IoErr());
        return ok ? 0 : 10;
    }

    if (!strcmp(cmd, "open")) {
        BPTR fh = Open(path, MODE_OLDFILE);
        printf("%-22s %s IoErr=%ld\n", "open oldfile", fh ? "OK" : "FAIL", (long)IoErr());
        if (fh) Close(fh);
        return fh ? 0 : 10;
    }

    printf("usage: locktest suite|make|try|tryshared|hold|leak|delete|open [path] [secs]\n");
    return 0;
}
