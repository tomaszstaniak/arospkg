# Implementation prompt: arospkg 0.6.0

Implement and release arospkg 0.6.0: shared configuration, sources, cache and daily
administration. The user authorizes implementation, review, integration, push and
publication. Do not stop to ask whether already-requested features should be enabled.

Repositories: ~/Work/AROS-dev/arospkg and arospkg-index. Locate Limpet and ScreenBar
from project documentation when needed. Read AGENTS.md, coding standards and testbench;
review relevant v1/v11 upstream changes or reuse a current scoped platform review.

The authoritative scope is docs/backlog/configuration-0.6.0.md. Read the complete
checklist and docs/evidence/2026-10-12-review-050/README.md before implementation.
Use latest main, inspect current branches/worktrees and preserve unrelated work.
Temporary isolation is acceptable; finished work must end integrated on main.

FIRST: fix the reviewed 0.5.0 bugs with meaningful regression tests:
- app.c accepts missing ABI and launch.c allows UNDETERMINED compatibility. Require
  known matching ABI; test absent, empty, unknown and mismatched ABI.
- worker shutdown waits by the shared name. Track this instance's worker safely;
  closing one PkgManager must not wait for another's operation. Test two instances.
- the CLI-icon sentinel is swallowed by the generic missing-icon branch. Return
  the intended explanation and test an existing WBTOOL icon with CLI.

SHARED CONFIGURATION:
Implement one versioned, validated model in libpkg for CLI, GUI and the internal
widget. Prefer existing JSON infrastructure. Define precedence: explicit invocation
arguments > active user settings > defaults, with precise ENV:/ENVARC: Use/Save
semantics. Absence preserves 0.5 defaults; invalid or future configuration must not
silently redirect installations. Expose effective values and origins. Provide
migration, atomic replacement/recovery and protection against concurrent lost edits.
Operations hold a settings snapshot; edits never switch their root/source mid-flight.
Config must not disable checksum, TLS or ABI verification or enable testing overrides.

CLI AND GUI:
Add config show/get/set/reset/validate, clean JSON and documented exit codes. Existing
--root/--index/--index-url remain compatible one-shot overrides. Add Settings with
Apply/Use, Save, Cancel and defaults. Show the active package root in the main window.
Changing root selects a separate installation database; it does not move or merge
files. Remember window geometry, filter/sort and picture visibility. Demo purchase
remains explicitly invoked; do not put fictional offers into production catalogues.

SOURCES:
Persist source identity, URL and enabled state. Preserve the official source by
default. Keep source provenance for installed packages; never select arbitrary
cross-source duplicates or change publisher during upgrade. Define collisions,
source removal, freshness and failed refresh behavior. Retain previous valid data
when refresh fails. Installed apps remain visible/run/removable with sources disabled.

NETWORK AND REFRESH:
Configure bounded timeouts/retries and optional GUI-start/interval checks. Make
waiting cancellable. Do not silently retry hash mismatches, certificate failures
or cancellation, and do not install or upgrade automatically. State whether periodic
checks run only while GUI/widget is active; no hidden daemon. Distinguish catalogue
refresh, application updates and manager checks. Stale/offline is not 'up to date'.

CACHE AND DIAGNOSTICS:
Add inspection, size usage, cleanup preview and optional limit. Protect rollback
archives, active transactions and rejected evidence. Use existing operation locking.
Rejected files need explicit deletion, not automatic eviction. Configure logs and
keep-rejected. Export a redacted diagnostic report; no upload or copying arbitrary
user/application data. Test cleanup followed by a successful rollback.

FOLLOW-UPS:
Review Workbench launch confirmation for quick exits, renamed/singleton/already-running
apps, and report confirmed/unconfirmed honestly. Investigate GUI Shell launch holding
its drawer without unloading live code. Fix narrow Limpet summary wrapping. Add GUI
remove confirmation/preview. Review each of the eight changed AROS Archives files
listed in the checklist before updating its hash, or mark it clearly unavailable.
Correct evidence wording: apkg update demonstrates catalogue download, not install.

PLATFORMS AND TESTS:
Run meaningful host tests and final v11 binaries in Shell, Limpet and GUI. Acceptance:
set root/source in GUI, Save, reboot, verify CLI and GUI agree; one-shot CLI override
does not persist. Test session-only settings, bad config, concurrent writes, config
changes during operations, source collisions and cache/rollback protection.
Keep 0.5 launch/update/recovery/Ctrl-C regressions. Obtain a ready leased mainline
network slot for a controlled actual package install and test mainline GUI/persistence
and Workbench launch; document genuine infrastructure gaps. Build maintained targets.
Pi can be published as experimental after compile/link without hardware tests; record
actual status and ask for user testing in notes. Apply the same explicit experimental
policy to catalogue entries rather than automatically keeping an obsolete version.
Do not imply support for combinations the SDK cannot build.

ScreenBar is internal demonstration only, not a publication prerequisite. Its provider
uses shared configuration, remains optional and publishes no network work each tick.
Do not push its private repository or publish ScreenBar in this task. Preserve Bohun's
original diagnostic ZIP and the current checksum verification; his root cause remains
open unless new evidence proves it.

FINISH:
Review/fix important findings, test final binaries, capture fresh v11 screenshots of
Settings, sources/root, cache preview and CLI. Update schema/user/author documentation
and checklist with links to evidence. Integrate completed work into main, push, remove
own completed branches/worktrees and publish 0.6.0 with correct target status, BUILD,
licenses, hashes and gallery. Update relevant catalogue entries, preserving legacy
bootstrap access and its size limit. Verify downloaded public artifacts, self-update
and manual PkgManager update. Send no forum messages; prepare English announcement.

Do not turn existing authorization into another approval cycle. Resolve routine
design choices, implement coherent defaults and document decisions. If external
infrastructure blocks a target, complete independent work and label that limitation.
Report release URL, commits, test evidence, screenshots and remaining real issues.
