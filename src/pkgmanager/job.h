/* What the window and the worker share. One job at a time, by design: the
 * window disables its buttons while one runs, and the root lock in libpkg
 * would refuse a second anyway. */
#ifndef PKGMANAGER_JOB_H
#define PKGMANAGER_JOB_H

#include "../libpkg/pkg.h"
#include <exec/ports.h>

typedef enum { JOB_NONE, JOB_INSTALL, JOB_REMOVE, JOB_UPDATE,
               JOB_PREVIEW_UPGRADE, JOB_UPGRADE, JOB_PREVIEW_ROLLBACK, JOB_ROLLBACK } job_kind;

struct pm_msg_s;

typedef struct {
    job_kind        kind;
    long            seq;        /* the job's number; every message carries it */
    char            id[PKG_MAXID];
    char            root[PKG_MAXPATH];
    char            index[PKG_MAXPATH];
    char            index_url[512];
    struct MsgPort *reply;      /* the window's port; the worker sends here */
    volatile int    cancel;     /* set by the window, read by the callback  */
    volatile int    running;
    /* The final message, allocated by job_start before the worker exists:
       an operation must never end with nothing to say so because memory ran
       out after the work was done. */
    struct pm_msg_s *done;
} job;

/* Every message the worker sends. PM_PROGRESS carries one pkg_progress_ev,
 * copied, because the event's strings belong to the worker's stack frame.
 * PM_DONE carries the outcome. The window frees each with FreeVec. */
enum { PM_PROGRESS = 1, PM_DONE = 2 };

typedef struct pm_msg_s {
    struct Message msg;
    int            kind;
    long           seq;         /* job.seq of the job it reports on */
    char           phase[16];
    char           id[PKG_MAXID];
    unsigned long  done, total;
    int            can_cancel;
    pkg_status     status;
    pkg_err        err;
    pkg_plan       plan;        /* filled for the preview jobs */
} pm_msg;

/* Start the worker process for `j`. Returns 0 on success; the worker will
 * send PM_DONE exactly once, whatever happens. -1 when the final message or
 * the process could not be had; nothing was started then. */
int job_start(job *j);
void job_set_slow(int ms);      /* testing only */
void job_test_nomem(void);      /* testing only */
long job_wait_gone(long report_ms, void (*still)(long ms));

#endif
