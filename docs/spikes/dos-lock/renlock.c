/* Is Rename() usable as atomic create-if-absent, i.e. as pkg's lock?
 *
 * The DOS exclusive lock was measured (see README) to survive its owner and
 * to be unbreakable, so it cannot guard anything a crash must be able to
 * recover from. Rename() has the opposite shape: it REFUSES to overwrite
 * (ERROR_OBJECT_EXISTS), it leaves no kernel state behind, and the object it
 * creates is an ordinary file that a later run can inspect and delete.
 *
 * Three questions:
 *   1. does Rename onto an existing name fail, and with which IoErr?
 *   2. under real contention, does exactly one process win?
 *   3. where does ENV: live -- is it volatile across a reboot by construction?
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <dos/dos.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define LOCKPATH  "T:pkglock.owner"
#define CRITPATH  "T:pkgcrit"

static int write_file(CONST_STRPTR path, const char *text)
{
    BPTR fh = Open(path, MODE_NEWFILE);
    if (!fh) return 0;
    FPuts(fh, (CONST_STRPTR)text);
    Close(fh);
    return 1;
}

static int read_file(CONST_STRPTR path, char *buf, int n)
{
    BPTR fh = Open(path, MODE_OLDFILE);
    LONG got;
    if (!fh) return 0;
    got = Read(fh, buf, n - 1);
    Close(fh);
    if (got < 0) got = 0;
    buf[got] = 0;
    return 1;
}

/* One acquire attempt. Returns 1 on success; *err gets the IoErr from the
   Rename itself, captured before the cleanup DeleteFile can overwrite it --
   the first version of this probe reported IoErr=0 for a failed acquire for
   exactly that reason. */
static int try_acquire(const char *id, int seq, LONG *err)
{
    char nonce[128], body[64];
    if (err) *err = 0;
    snprintf(nonce, sizeof nonce, "T:pkglock.%s.%d.new", id, seq);
    snprintf(body, sizeof body, "%s\n", id);
    if (!write_file((CONST_STRPTR)nonce, body)) return 0;

    if (Rename((CONST_STRPTR)nonce, (CONST_STRPTR)LOCKPATH))
        return 1;                       /* we created it: we hold the lock */

    if (err) *err = IoErr();            /* capture BEFORE cleaning up */
    DeleteFile((CONST_STRPTR)nonce);    /* someone else holds it */
    return 0;
}

static int child(const char *id, int rounds)
{
    int wins = 0, contended = 0, violations = 0, i;
    char buf[64], res[128], line[256];

    for (i = 0; i < rounds; i++) {
        if (try_acquire(id, i, NULL)) {
            wins++;
            /* critical section: stamp it, pause, check it is still ours */
            write_file((CONST_STRPTR)CRITPATH, id);
            Delay(1);
            if (read_file((CONST_STRPTR)CRITPATH, buf, sizeof buf)) {
                buf[strcspn(buf, "\r\n")] = 0;
                if (strcmp(buf, id) != 0) violations++;
            }
            DeleteFile((CONST_STRPTR)LOCKPATH);
        } else {
            contended++;
        }
    }

    snprintf(res, sizeof res, "T:res.%s", id);
    snprintf(line, sizeof line, "%s wins=%d contended=%d violations=%d\n",
             id, wins, contended, violations);
    write_file((CONST_STRPTR)res, line);
    return 0;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "help";

    if (!strcmp(cmd, "child"))
        return child(argc > 2 ? argv[2] : "a", argc > 3 ? atoi(argv[3]) : 100);

    if (!strcmp(cmd, "basic")) {
        char buf[64];
        DeleteFile((CONST_STRPTR)LOCKPATH);

        printf("1 acquire on free lock  %s\n", try_acquire("a", 0, NULL) ? "OK (want OK)" : "FAIL");
        {
            LONG err = 0;
            int got = try_acquire("b", 0, &err);
            printf("2 acquire when held     %s IoErr=%ld (want FAIL, 213)\n",
                   got ? "OK" : "FAIL", (long)err);
        }

        read_file((CONST_STRPTR)LOCKPATH, buf, sizeof buf);
        buf[strcspn(buf, "\r\n")] = 0;
        printf("3 holder is readable    '%s' (want 'a')\n", buf);

        printf("4 stale lock deletable  %s\n",
               DeleteFile((CONST_STRPTR)LOCKPATH) ? "OK (want OK)" : "FAIL");
        printf("5 acquire after delete  %s\n", try_acquire("b", 1, NULL) ? "OK (want OK)" : "FAIL");
        DeleteFile((CONST_STRPTR)LOCKPATH);
        return 0;
    }

    if (!strcmp(cmd, "race")) {
        int rounds = argc > 2 ? atoi(argv[2]) : 150;
        char cmd1[160], cmd2[160], buf[256];
        DeleteFile((CONST_STRPTR)LOCKPATH);
        DeleteFile((CONST_STRPTR)"T:res.a");
        DeleteFile((CONST_STRPTR)"T:res.b");

        snprintf(cmd1, sizeof cmd1, "Run >NIL: RAM:renlock child a %d", rounds);
        snprintf(cmd2, sizeof cmd2, "Run >NIL: RAM:renlock child b %d", rounds);
        SystemTags((CONST_STRPTR)cmd1, SYS_Input, (IPTR)BNULL, SYS_Output, (IPTR)BNULL, TAG_DONE);
        SystemTags((CONST_STRPTR)cmd2, SYS_Input, (IPTR)BNULL, SYS_Output, (IPTR)BNULL, TAG_DONE);

        printf("race: two children, %d rounds each; waiting\n", rounds);
        Delay(50 * 25);

        if (read_file((CONST_STRPTR)"T:res.a", buf, sizeof buf)) printf("  %s", buf);
        else printf("  child a produced no result\n");
        if (read_file((CONST_STRPTR)"T:res.b", buf, sizeof buf)) printf("  %s", buf);
        else printf("  child b produced no result\n");
        printf("violations must be 0 in both, and wins must be < rounds\n");
        return 0;
    }

    if (!strcmp(cmd, "env")) {
        char buf[256];
        BPTR l = Lock((CONST_STRPTR)"ENV:", SHARED_LOCK);
        if (l) {
            if (NameFromLock(l, (STRPTR)buf, sizeof buf - 1))
                printf("ENV: resolves to %s\n", buf);
            else
                printf("ENV: NameFromLock failed IoErr=%ld\n", (long)IoErr());
            UnLock(l);
        } else printf("ENV: cannot lock IoErr=%ld\n", (long)IoErr());
        return 0;
    }

    printf("usage: renlock basic|race [rounds]|child <id> <n>|env\n");
    return 0;
}
