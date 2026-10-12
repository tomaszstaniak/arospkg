---
title: "arospkg 0.6.0: shared configuration and daily administration"
status: planned
created: 2026-10-12
updated: 2026-10-12
---

# arospkg 0.6.0: shared configuration

Outcome: configure the package root and catalogue sources once; CLI, PkgManager
and the internal PkgUpdates demonstration use the same effective settings after
restart. Manage cache without losing rollback or rejected-download evidence.

This is an implementation checklist, not a claim that configuration exists.
Baseline: released 0.5.0, local main c67ad68 at review on 2026-10-12. Refresh
repository state before implementation. Preserve existing commands and roots.

## 1. Baseline and integration

- [ ] Read shared/project AGENTS.md, coding standards and testbench.
- [ ] Review relevant upstream v1/v11 changes, reusing a current documented review.
- [ ] Inventory all current CLI/GUI/widget defaults and overrides; identify supported
      versus testing-only options. Reuse existing launch/updates/recovery code.
- [ ] Review 0.5.0 evidence and the accompanying review findings before editing.
- [ ] Use temporary isolation only as needed; finish by integrating, pushing and
      removing the completed branch/worktree. Do not leave completed features pending.

## 2. Shared configuration contract in libpkg

- [ ] Define a versioned, bounded configuration format using existing JSON support
      where practical; no second parser or independent GUI settings model.
- [ ] Define settings, types, ranges, defaults, unknown-field behavior and migration.
- [ ] Specify precedence: explicit command/startup arguments > active user settings
      > defaults. Document ENV:/ENVARC: session and persistent behavior precisely.
- [ ] Support AROS Use/Save behavior: Apply/Use changes the active session; Save
      persists it for restart; Cancel changes nothing.
- [ ] Distinguish absent configuration (compatible defaults) from invalid or
      unsupported configuration (explicit diagnostic, no silent root fallback).
- [ ] Provide effective values with their origins: argument, session/persistent
      setting, default. Redact credentials if any supported setting contains them.
- [ ] Write atomically; validate before replacement and retain a recoverable prior
      version. Prevent lost updates from concurrent CLI/GUI writers.
- [ ] Snapshot operational settings when an operation starts. Editing preferences
      must not change its root, source, destination or verification halfway through.
- [ ] Exclude dangerous overrides: configuration cannot disable checksum/TLS
      verification, ABI checks or silently activate test/demo behavior.

## 3. CLI configuration

- [ ] Implement config show/get/set/reset/validate with useful help and stable exit codes.
- [ ] Support clean JSON output and selecting session versus persistent changes.
- [ ] Show configuration location, schema and effective value origins.
- [ ] Invalid edits leave the current configuration untouched.
- [ ] Existing --root and catalogue overrides still win for one invocation, without
      saving themselves as user preferences.
- [ ] Quoted roots, AROS assigns and paths with spaces work in Shell and Limpet.

## 4. PkgManager Settings

- [ ] Add Settings with package root, sources, networking/refresh, cache/diagnostics
      and presentation controls; distinguish Apply/Use, Save and Cancel.
- [ ] Show the currently managed root visibly in the main window.
- [ ] Select an existing root or initialize a new one deliberately; changing a
      default root does not move, delete or merge existing installed packages.
- [ ] Explain root/source changes pending until the current operation ends.
- [ ] Remember window size/position, sorting, filters and image visibility.
- [ ] Keep the UI usable at 1024x768 and without images or a catalogue.
- [ ] Demo purchases remain explicitly invoked demonstration data; saved settings
      never present a fictional offer as a production package.

## 5. Sources and package identity

- [ ] Persist source IDs, display names, catalogue URLs and enabled state.
- [ ] First run preserves the existing official catalogue behavior.
- [ ] Display source provenance in CLI, JSON and GUI package details.
- [ ] Define duplicate IDs/variants across sources; never choose an arbitrary
      first entry or silently replace one publisher with another during upgrade.
- [ ] Record installation source identity; upgrades stay with it unless the user
      deliberately selects a source change with a preview.
- [ ] Store source catalogues separately and retain the previous valid catalogue
      if a refresh fails; report success/failure and freshness per source.
- [ ] Validate URLs and keep test/local-source use explicit. Existing --index and
      --index-url behavior remains compatible and documented.
- [ ] Decide how disabled/removed sources affect installed packages and rollback;
      installed packages remain visible, launchable and removable.

## 6. Network and update preferences

- [ ] Add bounded, cancellable timeout and retry settings to the existing downloader.
- [ ] Retry only eligible transient failures; checksum mismatches, cancellation,
      certificate failures and partial file mutations are not silently retried.
- [ ] Keep rejected-file evidence when enabled and explain retry results.
- [ ] Configure optional catalogue/update checks at GUI startup and an interval.
      Default behavior does not unexpectedly perform installations or upgrades.
- [ ] Distinguish catalogue refresh, installed-app updates and manager release checks.
- [ ] Document where periodic checks run: only while the responsible program is
      running unless a separately explicit scheduler is supplied. No hidden daemon.
- [ ] Show last success, stale/offline status and cancellation without calling them
      'up to date'. Self-update continues to update apkg only.

## 7. Cache, rollback and diagnostics

- [ ] Show space usage: cached downloads, rollback archives, rejected evidence.
- [ ] Add cache inspection and cleanup CLI plus a matching GUI preview.
- [ ] Implement an optional size limit with a documented eviction policy.
- [ ] Never delete files in an active transaction or archives referenced by rollback
      as routine cleanup; reference checks are authoritative.
- [ ] Keep .rejected evidence separate; deletion is explicit, not hidden under eviction.
- [ ] Serialize cleanup with package operations using the existing locking model.
- [ ] Allow log destination/detail and --keep-rejected behavior to be configured.
- [ ] Export a diagnostic report with manager/platform/build information, redacted
      effective configuration, catalogue state and operation errors.
- [ ] Export never silently uploads files or includes whole application/user data.

## 8. Follow-ups from 0.5.0

- [ ] Reject absent, empty or unknown ABI in application launch metadata; the
      current parser accepts missing ABI and launch allows UNDETERMINED.
- [ ] Track each GUI instance's own worker lifetime instead of waiting for every
      process with the shared worker name; test two PkgManager instances.
- [ ] Check the CLI tooltype sentinel before the generic missing-icon branch;
      verify the correct error and test a valid icon with CLI.
- [ ] Review Workbench start confirmation: process-name observation is heuristic,
      not proof of launch ownership. Test fast-exiting, renamed, singleton and
      already-running applications; use honest confirmed/unconfirmed messages.
- [ ] Investigate Shell programs launched by GUI holding its drawer locked. Fix
      process lifetime/current-directory ownership if supported; otherwise clearly
      explain and test the limitation. Never unload executing code early.
- [ ] Fix apkg update summary wrapping inside words in a narrow Limpet window.
- [ ] Add a GUI removal preview/confirmation showing target and preserved files;
      keep scripted CLI semantics explicit and compatible.
- [ ] Review eight changed AROS Archives downloads independently: amiberry,
      gicriptofilex, giocodel15, pkg-7mezzoplus, scopagame, tressette, xrick,
      zgloom. Verify source/content/metadata before updating hashes; quarantine
      unreconciled entries with a clear unavailable reason. Never bypass hashes.
- [ ] Check published Pi/mainline manager catalogue entries against current release
      policy. Untested Pi builds are publishable as experimental; lack of hardware
      does not automatically force an older catalogue version.
- [ ] Correct documentation that conflates a mainline catalogue download with a
      demonstrated package installation over the network.

## 9. Validation

- [ ] Host tests: parsing/migration, precedence, origins, atomic failure recovery,
      concurrent edits, invalid values, source collisions and rollback-safe cleanup.
- [ ] Configuration absent: 0.5.0 behavior remains compatible.
- [ ] v11 final binaries: set root/source in GUI, Save, reboot, confirm CLI and GUI
      use the same settings; override once through CLI and verify nothing was saved.
- [ ] Session-only Apply/Use and persistent Save have distinct, tested behavior.
- [ ] Change settings during a download: active operation uses its original snapshot.
- [ ] Run full install/run/verify/upgrade/rollback/remove regressions plus Ctrl-C,
      redirected JSON, malformed configuration and no/offline catalogue.
- [ ] Limpet final-build tests, including narrow-window summaries and one prompt.
- [ ] Mainline network install from a controlled valid catalogue on a ready leased
      network-capable slot; unavailable infrastructure is recorded, not invented.
- [ ] Mainline GUI/config persistence and Workbench launch where supported.
- [ ] Build all maintained targets, including Pi with its native SDK. Experimental
      compile/link-only Pi can be published with its actual test status.
- [ ] Internal PkgUpdates uses the shared configuration and remains optional;
      neither ScreenBar publication nor real payments is a release dependency.

## 10. Release and acceptance

- [ ] Review and fix important findings; run final-binary tests.
- [ ] Fresh v11 screenshots: Settings, root/source state, cache preview, CLI config
      and normal application lifecycle. Demo material remains clearly labeled.
- [ ] Document configuration schema, migration, CLI, GUI and experimental targets.
- [ ] Integrate all completed changes to main and push. Reconcile duplicate local
      changes safely; remove only own completed branches/worktrees.
- [ ] Publish 0.6.0 with BUILD provenance, licenses, checksums and screenshots.
- [ ] Update relevant catalogue entries without fictional purchase/image data.
- [ ] Download public artifacts and verify hashes, self-update and manual GUI update.
- [ ] Leave Bohun's original diagnostic ZIP unchanged; root cause of his checksum
      issue remains open unless evidence actually identifies it.

Acceptance: choose root and source once in GUI, save and reboot; CLI and GUI
agree. Refresh, install, run and update an application, clean cache and still
rollback. Invalid configuration never silently redirects an installation.
