/* libpkg -- the whole of arospkg's logic. `pkg` and `PkgManager` are fronts.
 *
 * This is the first implementation, deliberately smaller than section 3's API:
 * it covers the flow the design most needs verified -- local index, install one
 * package, registry, safe removal -- with the transaction journal and recovery
 * that section 2 specifies. Networking is not here yet; archives come from the
 * cache. What exists is meant to be correct, not complete.
 */
#ifndef PKG_H
#define PKG_H

#include <stddef.h>

#define PKG_MAXPATH 512
#define PKG_MAXID    64

typedef enum {
    PKG_OK = 0,
    PKG_E_LOCKED,          /* another instance holds the lock            */
    PKG_E_STALE_LOCK,      /* holder is gone; pkg unlock is needed       */
    PKG_E_UNRESOLVED_TXN,  /* recovery stopped short; needs a human      */
    PKG_E_NOT_FOUND,
    PKG_E_EXISTS,
    PKG_E_VERIFY,          /* hash or size mismatch                      */
    PKG_E_CONFLICT,
    PKG_E_ARCHIVE,         /* malformed or unsafe archive                */
    PKG_E_IO,
    PKG_E_NOMEM,
    PKG_E_INTERRUPTED,     /* deliberate, from --interrupt-at            */
    PKG_E_NETWORK,         /* no stack, no trust store, or the fetch failed */
    /* Appended, not inserted: --expect in the test scripts names these by
     * number, so renumbering would silently re-point every existing test. */
    PKG_E_REQUIRES,        /* the MACHINE does not provide something needed */
    PKG_E_CANCELLED        /* the caller's progress callback asked to stop  */
} pkg_status;

typedef struct {
    pkg_status status;
    char summary[160];     /* one line, already user-facing */
    char detail[320];
    char subject[PKG_MAXPATH];
} pkg_err;

typedef struct pkg_ctx pkg_ctx;

/* One row of the index or of the registry. Fixed-size fields on purpose: a
 * Zune list, a Shell table and a JSON printer all read the same struct with
 * no allocation of their own. The text `apkg search` prints is formatted from
 * these rows, so no front end can see a different index from another. */
typedef struct {
    char id[PKG_MAXID];
    char version[64];      /* upstream's string, compared only for equality */
    long revision;         /* our release of that version; 0 = not stated */
    char summary[160];
    char category[64];
    char arch[16];
    char abi[8];           /* "v1" or "v11"; empty when the index omits it */
    char kind[16];
    long size;             /* archive bytes from the index, or -1 */
    char requires[192];    /* requires_system ids, comma separated; "" if none */
    int  ours;             /* built for the ABI this binary runs on */
    int  installed;        /* a registry entry exists for this id */
    char installed_version[64];   /* from the registry, when installed */
    long installed_revision;      /* from the registry; 0 when it stated none */
} pkg_entry;

/* What an upgrade or a rollback would do, from the library's own planner --
 * a front end shows this, it never parses the Shell's printout. `allowed` is
 * the revision rule (same upstream version, higher revision) or, for a
 * rollback, whether the previous revision and its archive are still there;
 * `why` says so in the user's words when it is not. The counts are the plan;
 * `conflicts` > 0 means the operation would be refused, and `conflict_list`
 * names the files. */
typedef struct {
    char from_version[64]; long from_revision;
    char to_version[64];   long to_revision;
    int  allowed;
    char why[240];
    int  add, replace, remove, kept, unchanged, conflicts;
    char conflict_list[320];
    int  incomplete;       /* a dry run that may not fetch could not make the file plan; why says so */
} pkg_plan;

/* verify: the installed files against the registry, repairing nothing. */
typedef struct {
    int  ok, changed, missing, unknown;
    char icon[24];         /* "ours", "changed locally", "missing", "kept-existing", "none" */
} pkg_verify_result;

typedef struct { pkg_entry *v; int n, cap; } pkg_entries;   /* zero-init */

/* Progress for a front end. One event per step: the phase -- "download",
 * "verify", "extract", "publish" -- the package it concerns, done/total in
 * bytes where known (total 0 when it is not), and whether cancelling is
 * still possible. `can_cancel` is stated, not left for the caller to infer
 * from the phase name: a Cancel button must grey out the moment the library
 * stops taking the request, or it looks broken.
 *
 * Setting *cancel stops the operation only while can_cancel is 1 -- during
 * the download and after verification, before any transaction exists. Once a
 * transaction has begun the events keep coming with can_cancel 0 and the
 * flag is ignored, because a half-done transaction is what the journal exists
 * to finish, and a front end has no business leaving one for the next run.
 *
 * The callback runs on the task that called pkg_install; it must return
 * quickly and must not touch a GUI directly -- hand the event to the GUI's
 * own loop. */
typedef struct {
    const char   *phase;
    const char   *id;
    unsigned long done, total;
    int           can_cancel;
} pkg_progress_ev;

typedef void (*pkg_progress)(void *user, const pkg_progress_ev *ev, int *cancel);

typedef enum {
    PKG_OPEN_READ,    /* no lock, no recovery, never mutates              */
    PKG_OPEN_WRITE,   /* lock, recover, clean tmp; refuses if unresolved  */
    PKG_OPEN_DOCTOR   /* lock and recover, but do NOT refuse and do NOT   *
                       * clean tmp: this is the mode that has to work     *
                       * precisely when the root is in a state the normal *
                       * path declines to touch                           */
} pkg_mode;

/* Injection points for the interruption test. The design owes crash injection
 * between a mutation and its marker; this is how that is reached from a test
 * without patching the library. */
typedef enum {
    PKG_STOP_NONE = 0,
    PKG_STOP_AFTER_PLAN,        /* plan written, nothing done            */
    PKG_STOP_AFTER_DIR,         /* package directory in place, no marker */
    PKG_STOP_AFTER_DIR_MARKER,
    PKG_STOP_AFTER_REGISTRY,    /* registry written, no commit marker    */
    PKG_STOP_BEFORE_COMMIT,
    PKG_STOP_AFTER_REMOVE_FILES,/* remove: files gone, registry intact   */
    PKG_STOP_IN_PUBLISH,        /* inside u_publish, between the delete   *
                                 * and the rename: the window Rename()'s  *
                                 * refusal to overwrite forces on us      */
    PKG_STOP_AFTER_ICON,        /* drawer icon published, no marker, no  *
                                 * registry: the one file outside the    *
                                 * package directory, on disk and unowned */
    /* Upgrade and rollback swap two trees; these are the two states
     * between the renames. Appended, as --interrupt-at names them by number. */
    PKG_STOP_AFTER_OLD_MOVED,   /* live tree moved aside, nothing published */
    PKG_STOP_AFTER_NEW_PUBLISHED/* new tree in place, registry not written  */
} pkg_stop;

/* Fault injection for the paths that only run when a write fails. These are
 * not reachable by stopping early: the code has to actually try and fail. */
typedef enum {
    PKG_FAIL_NONE = 0,
    PKG_FAIL_COMMIT,     /* 005-commit cannot be written  */
    PKG_FAIL_DOCTOR      /* the leftovers report cannot be written */
} pkg_fault;

void pkg_test_fault(pkg_fault);

/* Testing only: slow things down so a person or a script can act mid-way.
 * ms per 16 KB received, and a pause of 20*ms after extraction begins --
 * the first gives a cancel test a window it can hit every time, the second
 * gives the non-cancellable phase a duration a close-during-it test needs.
 * 0 restores normal speed. */
void pkg_test_slow(int ms);
/* Testing only: in the next download whose answer is a 2xx, once that many
 * bytes of its body have arrived, the following receive fails in the TLS
 * adapter: as a reset socket (how = 1) or with corrupted bytes (how = 2).
 * It fires once, so a following attempt in the same process succeeds. A
 * negative byte count turns it off. */
void pkg_test_break_download(long body_bytes, int how);

/* Testing only: make recovery abort part-way through, so the next run has to
 * finish a rollback that was itself interrupted. Recovery claims to be
 * idempotent; this is how that claim is checked rather than asserted. */
void       pkg_test_break_recovery(int after_n_files);

pkg_status pkg_open (const char *root, pkg_mode, pkg_ctx **, pkg_err *);
void       pkg_close(pkg_ctx *);

/* Recovery runs inside pkg_open(PKG_OPEN_WRITE), before tmp/ is cleaned.
 * Reports how many transactions it resolved and how many it could not. */
void       pkg_recovery_counts(const pkg_ctx *, int *resolved, int *unresolved);

/* Uncommitted transactions seen when the root was opened read-only. A
 * read-only view must report these rather than showing their half-written
 * state as settled. */
int        pkg_pending_txns(const pkg_ctx *);

/* Identifies the build. The source hash is compiled in; it says nothing about
 * the toolchain, the flags or what was linked, which is why the provenance
 * below is recorded beside it and why the binary hashes ITSELF at run time. */
const char *pkg_build_id(void);
const char *pkg_toolchain(void);

/* The ABI this binary was built for: "v1" (mainline) or "v11" (distributions
 * such as AROS One).
 *
 * This is also the ABI of the machine it is running on, and not by assumption:
 * a binary built for one ABI does not start on the other, so the fact that this
 * code is executing at all is the evidence. ArosInquire() is a cross-check
 * rather than the source. */
const char *pkg_abi(void);

/* Testing and diagnosis only. --abi installs a package built for another ABI,
 * which will install correctly and then not start; --all-abi makes search list
 * those packages instead of hiding them. Neither is a user feature. */
void        pkg_set_abi_override(const char *);
const char *pkg_abi_override(void);
void        pkg_set_abi_show_all(int);

/* Testing only: require the certificate to match this name instead of the host
 * we connected to. The handshake must then FAIL. Without a test like this,
 * "the download worked" is not evidence that a wrong certificate would be
 * refused -- it only shows the right one was accepted. */
void        pkg_set_verify_name(const char *);
const char *pkg_verify_name(void);
int         pkg_abi_show_all(void);
const char *pkg_sdk(void);
const char *pkg_cc(void);

/* SHA-256 of the running executable, found through GetProgramName(). This is
 * the only identifier that covers the toolchain, the flags and the libraries
 * actually linked in. Returns 0 on success; on failure `out` is "unknown" and
 * a report must say so rather than fall back to the source hash. */
int pkg_self_sha256(char out[65]);

pkg_status pkg_install(pkg_ctx *, const char *index_path, const char *id,
                       pkg_stop, pkg_err *);
pkg_status pkg_remove (pkg_ctx *, const char *id, pkg_stop, pkg_err *);

/* Replace an installed package by the index's newer revision of the SAME
 * upstream version, or go back to the revision an upgrade replaced. Both
 * print their plan -- add, replace, remove, keep -- before touching the
 * drawer, refuse on any conflict with a file the user changed, and keep
 * every file the user changed or added. A rollback needs what the upgrade
 * saved: the previous registry entry and archive. Neither resolves
 * dependencies or touches anything outside the package's drawer and its
 * icon. See upgrade.h for the rules. */
pkg_status pkg_upgrade (pkg_ctx *, const char *index_path, const char *id,
                        pkg_stop, pkg_err *);
pkg_status pkg_rollback(pkg_ctx *, const char *id, pkg_stop, pkg_err *);

/* The plan without the operation. An upgrade preview fetches and stages the
 * target archive to compute it (with progress; cancellable), then discards
 * the staging; nothing in the drawer or the registry is touched. Returns
 * PKG_OK with the plan filled in, including the refusals (`allowed` 0, or
 * `conflicts` > 0), which are information here rather than errors. */
pkg_status pkg_upgrade_preview (pkg_ctx *, const char *index_path, const char *id,
                                int allow_fetch, pkg_plan *, pkg_err *);
pkg_status pkg_rollback_preview(pkg_ctx *, const char *id, pkg_plan *, pkg_err *);

/* The cheap questions a button needs answered from a row alone (upgrade:
 * the revision rule over the row's index and registry fields) or from the
 * registry (rollback: is there anything to go back to). 1 = yes; 0 = no,
 * with the reason in `why`. */
int pkg_can_upgrade (const pkg_entry *, char *why, size_t n);
int pkg_can_rollback(pkg_ctx *, const char *id, char *why, size_t n);

/* Compare an installed package with its registry entry: which files are as
 * installed, changed locally, missing, or in the drawer without being the
 * package's; and the drawer icon. Changes nothing. `listing` (caller frees)
 * is one line per file, "<state>  <path>", sorted. */
pkg_status pkg_verify(pkg_ctx *, const char *id, pkg_verify_result *, char **listing, pkg_err *);

/* What an operation would do, from the same planners the operation uses,
 * with nothing changed: not the drawer, not the registry, and no recovery of
 * pending transactions (a read-only context; pkg_pending_txns says whether
 * any exist, and the report repeats it). `op` is "install", "remove",
 * "upgrade" or "rollback". `allow_fetch` says whether an archive may be
 * downloaded INTO THE CACHE to complete the plan; without it, and without a
 * cached copy, the report says the plan is incomplete rather than guess.
 * `report` (caller frees) is the plan in the user's words. */
pkg_status pkg_dry_run(pkg_ctx *, const char *index_path, const char *op, const char *id,
                       int allow_fetch, char **report, pkg_err *);

/* Listing: ids of installed packages, newline separated, caller frees. */
/* Download the index to <root>/db/index.json. Networking is only ever entered
 * from here and from pkg_install; every other command works offline. */
pkg_status pkg_update(pkg_ctx *, const char *index_url, pkg_err *);

/* Substring match over id, summary and category in the local index. */
pkg_status pkg_search(pkg_ctx *, const char *index_path, const char *term,
                      char **out, pkg_err *);

/* The same query as rows. Rows for another ABI are counted in *hidden (if
 * given) and left out unless pkg_set_abi_show_all(1). `installed` is filled
 * from the registry. Free with pkg_entries_free. */
pkg_status pkg_query(pkg_ctx *, const char *index_path, const char *term,
                     pkg_entries *out, int *hidden, pkg_err *);

/* What is installed, as rows, sorted by id. With an index_path (may be NULL)
 * the summary, category and kind are filled in from the index, since the
 * registry does not record them. */
pkg_status pkg_installed(pkg_ctx *, const char *index_path,
                         pkg_entries *out, pkg_err *);
void       pkg_entries_free(pkg_entries *);

/* One line per system requirement of `id`, probed on THIS machine now:
 * "crt.library: satisfied (opens, version 3)". Undetermined is reported as
 * such, never as satisfied. A package with no requirements yields "". */
pkg_status pkg_requirements(pkg_ctx *, const char *index_path, const char *id,
                            char *out, size_t n, pkg_err *);

/* Whether a package is compatible with this machine: its declared CPU and
 * ABI against this client's. It is a judgement from the catalogue, not a
 * test that the program runs -- and the machine's requirements are a
 * separate question (pkg_details.requirements). A package's own target and
 * the way this machine could run it are two facts: running a 32-bit ABIv0
 * package through EmuV0 would be a third verdict of its own, never "native". */
typedef enum {
    PKG_COMPAT_NATIVE = 0,      /* built for this machine's CPU and ABI       */
    PKG_COMPAT_INCOMPATIBLE,    /* built for another CPU or ABI               */
    PKG_COMPAT_UNDETERMINED     /* the entry does not say enough to decide     */
} pkg_compat;

/* "native", "incompatible", "undetermined": the word CLI, window and ARexx
 * all show. */
const char *pkg_compat_word(pkg_compat);

/* The verdict for a package built for `arch`/`abi` on a machine that is
 * `my_arch`/`my_abi`, with the reason in the user's words. Pure. An empty
 * `my_arch` does not check the CPU. */
pkg_compat pkg_compat_of(const char *arch, const char *abi, const char *my_arch,
                     const char *my_abi, char *why, size_t n);

/* This build's CPU: "x86_64", "aarch64" or "i386". */
const char *pkg_arch(void);

/* Everything about one package that can be said without changing anything
 * or downloading anything: what the catalogue offers, whether it can run
 * here, and what is installed. `apkg show`, the window's detail panel and
 * ARexx INFO read this, so they cannot disagree. */
typedef struct {
    pkg_entry e;              /* the variant described; installed_* from the registry */
    int  in_index;            /* 0: only the registry knows it, or there is no index */
    int  index_missing;       /* 1: there is no index on this machine at all */
    char url[512];
    char sha256[65];
    char source[256];         /* where its source is published; "" when not stated */
    char license[64];         /* "" when not stated */
    char variants[160];       /* every target the index has for this id: "x86_64/v11, i386/v0" */
    int  nvariants;
    int  ambiguous;           /* >1: that many entries are for this machine; install refuses */
    pkg_compat compat;
    char compat_why[200];
    /* The machine's requirements, probed now, only for a compatible package:
     * "satisfied", "missing", "undetermined", "none" (it states none), or
     * "not checked". */
    char requirements[16];
    /* installed: */
    char installed_arch[16];
    char installed_abi[8];    /* "" in entries written before 0.3 */
    char installed_dir[PKG_MAXPATH];
    char installed_when[32];
} pkg_details;

/* PKG_E_NOT_FOUND when neither the index nor the registry knows `id`. */
pkg_status pkg_details_get(pkg_ctx *, const char *index_path, const char *id,
                           pkg_details *, pkg_err *);

/* NULL clears it. See pkg_progress for what cancelling can and cannot do. */
void       pkg_set_progress(pkg_ctx *, pkg_progress, void *user);

pkg_status pkg_list(pkg_ctx *, char **out, pkg_err *);
pkg_status pkg_info(pkg_ctx *, const char *id, char **out, pkg_err *);

pkg_status pkg_unlock(const char *root, pkg_err *);

/* A read-only account of what is unresolved and why. Never repairs, never
 * deletes: `doctor --retry` re-runs recovery, and there is deliberately no
 * "force" that throws away state nobody has looked at. */
pkg_status pkg_doctor(pkg_ctx *, char **out, pkg_err *);

const char *pkg_strstatus(pkg_status);


/* Run `fn` as the program's main on a stack of at least 256 KB: a Shell's
 * default 40960 bytes is not enough for a TLS download. See bigstack.c. */
typedef int (*pkg_main_fn)(int argc, char **argv);
int pkg_run_with_stack(pkg_main_fn fn, int argc, char **argv);

#endif
