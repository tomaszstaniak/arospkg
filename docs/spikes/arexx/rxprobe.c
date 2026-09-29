/* What the ARexx on AROS One actually does with a host's reply, before
 * PkgManager's contract relies on any of it: whether RESULT survives a
 * non-zero RC, what becomes of Result2, whether SetRexxVar reaches the
 * script, how long a result can be, and what arrives when the caller did
 * not ask for a result. Every message is logged as received.
 *
 *   rxprobe <logfile>      opens public port RXPROBE until QUIT arrives */
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/rexxsyslib.h>
#include <proto/alib.h>
#include <rexx/storage.h>
#include <rexx/errors.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct RxsLib *RexxSysBase;

static void reply_str(struct RexxMsg *m, LONG rc, const char *s, size_t n)
{
    m->rm_Result1 = rc;
    m->rm_Result2 = 0;
    if (rc == 0 && (m->rm_Action & RXFF_RESULT) && s)
        m->rm_Result2 = (IPTR)CreateArgstring((CONST_STRPTR)s, n);
}

int main(int argc, char **argv)
{
    struct MsgPort *port;
    FILE *log;
    int quit = 0;

    if (argc < 2) return 20;
    log = fopen(argv[1], "w");
    if (!log) return 20;
    RexxSysBase = (struct RxsLib *)OpenLibrary((CONST_STRPTR)"rexxsyslib.library", 0);
    fprintf(log, "rexxsyslib %s version %d\n", RexxSysBase ? "opened" : "MISSING",
            RexxSysBase ? ((struct Library *)RexxSysBase)->lib_Version : -1);
    fprintf(log, "REXX port before: %s\n", FindPort((CONST_STRPTR)"REXX") ? "present" : "absent");
    fflush(log);
    if (!RexxSysBase) { fclose(log); return 20; }

    port = CreateMsgPort();
    port->mp_Node.ln_Name = (char *)"RXPROBE";
    port->mp_Node.ln_Pri = 0;
    AddPort(port);

    while (!quit) {
        struct RexxMsg *m;
        WaitPort(port);
        while ((m = (struct RexxMsg *)GetMsg(port))) {
            const char *a0 = "";
            char cmd[32] = "", *rest;
            int isrx = IsRexxMsg(m);
            if (isrx && m->rm_Args[0]) a0 = (const char *)m->rm_Args[0];
            fprintf(log, "msg isrexx=%d action=0x%08lx result_wanted=%d argc=%d arg0=[%s] len=%lu\n",
                    isrx, (unsigned long)m->rm_Action, (m->rm_Action & RXFF_RESULT) ? 1 : 0,
                    (int)(m->rm_Action & RXARGMASK), a0,
                    isrx && m->rm_Args[0] ? (unsigned long)LengthArgstring((UBYTE *)m->rm_Args[0]) : 0UL);
            fflush(log);
            if (!isrx) { ReplyMsg(&m->rm_Node); continue; }
            sscanf(a0, "%31s", cmd);
            rest = (char *)a0 + strlen(cmd);
            while (*rest == ' ') rest++;

            if (!strcmp(cmd, "ECHO")) reply_str(m, 0, rest, strlen(rest));
            else if (!strcmp(cmd, "RCN")) {
                /* rc and an integer Result2, as the ARexx manual describes a failure */
                long rc = 0, r2 = 0; sscanf(rest, "%ld %ld", &rc, &r2);
                m->rm_Result1 = rc; m->rm_Result2 = (IPTR)r2;
                if (rc == 0 && (m->rm_Action & RXFF_RESULT))
                    m->rm_Result2 = (IPTR)CreateArgstring((CONST_STRPTR)"zero", 4);
            } else if (!strcmp(cmd, "SETVAR")) {
                char name[64] = "", *val; LONG r;
                sscanf(rest, "%63s", name);
                val = rest + strlen(name); while (*val == ' ') val++;
                r = SetRexxVar(m, (CONST_STRPTR)name, val, strlen(val));
                fprintf(log, "  SetRexxVar(%s) -> %ld\n", name, (long)r); fflush(log);
                { char b[32]; snprintf(b, sizeof b, "%ld", (long)r); reply_str(m, 0, b, strlen(b)); }
            } else if (!strcmp(cmd, "SETVARFAIL")) {
                /* the pattern an error path would use: variable set, then rc 10 */
                LONG r = SetRexxVar(m, (CONST_STRPTR)"RC2", (char *)"NOTFOUND no such package", 24);
                fprintf(log, "  SetRexxVar(RC2) on failure -> %ld\n", (long)r); fflush(log);
                m->rm_Result1 = 10; m->rm_Result2 = 0;
            } else if (!strcmp(cmd, "LONG")) {
                long n = atol(rest); char *b = malloc(n + 1);
                memset(b, 'x', n); b[n] = 0; reply_str(m, 0, b, n); free(b);
            } else if (!strcmp(cmd, "NL")) reply_str(m, 0, "one\ntwo", 7);
            else if (!strcmp(cmd, "EMPTY")) reply_str(m, 0, "", 0);
            else if (!strcmp(cmd, "QUIT")) { reply_str(m, 0, "bye", 3); quit = 1; }
            else { m->rm_Result1 = 20; m->rm_Result2 = 0; }
            ReplyMsg(&m->rm_Node);
        }
    }
    Forbid();
    RemPort(port);
    { struct Message *m; while ((m = GetMsg(port))) ReplyMsg(m); }
    Permit();
    port->mp_Node.ln_Name = NULL;
    DeleteMsgPort(port);
    CloseLibrary((struct Library *)RexxSysBase);
    fprintf(log, "end\n");
    fclose(log);
    return 0;
}
