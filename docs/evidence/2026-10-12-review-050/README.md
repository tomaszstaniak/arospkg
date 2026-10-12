# Review of arospkg 0.5.0 for the 0.6 milestone

Date: 2026-10-12. Baseline: main c67ad68. Static review by a separate reviewer,
followed by coordinator inspection of each cited code path. No guest or host
runtime tests were executed in this review. Findings require regression tests
and fixes; they are not claims of reproduced physical-hardware failures.

## Important findings

1. **Unknown/missing ABI does not prevent launch.** `src/libpkg/app.c:249-252`
   omits the presence check after reading abi. `launch.c:182` refuses only
   INCOMPATIBLE, whereas `entries.c:46-53` returns UNDETERMINED for missing or
   unknown ABI. ELF CPU checks cannot distinguish v1 and v11. Require a known
   matching ABI before launch and cover missing/empty/unknown/mismatched values.
2. **One PkgManager can wait for another's worker on shutdown.**
   `src/pkgmanager/worker.c:139-150` uses the global worker name, not this
   instance's child identity. Different roots do not distinguish the workers.
   Replace the name-based lifetime check with an instance-specific safe protocol;
   do not unload executable code while the instance's child still runs. Test
   closing one idle window while another performs a delayed operation.

## Minor finding

3. **Wrong diagnosis for a CLI tooltype.** `launch.c:255-256` assigns -2,
   but `:262` handles all negative values as a missing icon before `:267` checks
   the CLI case. Start is refused, but repair instructions are misleading. Check
   -2 first and add a guest test using a valid WBTOOL icon with CLI.

## Known limitations and missing validation

- Workbench process-name confirmation is heuristic: separately test quick exits,
  renamed/singleton/already-running processes; do not equate unconfirmed with
  definitely failed or observe an unrelated process as owned success.
- GUI Shell launch retaining the PkgManager drawer, narrow Limpet word wrapping
  and absence of removal confirmation remain recorded follow-ups.
- Mainline network package installation and Workbench launch are unverified.
  Evidence README line 27 describes a network install using `apkg update`, which
  only fetches a catalogue; correct that wording during implementation.
- Pi remains compile/link verified only. Experimental publication is authorized
  by the user and does not depend on obtaining a Pi test machine.
- Eight changed upstream archives need individual content review, not blind hash
  replacement. The checksum refusal is expected behavior.
- Do not treat every GUI lifecycle path as tested with final binaries merely
  because the final gallery exists; state exactly which binaries each run used.

These findings and 0.6 acceptance checks are tracked in
[configuration checklist](../../backlog/configuration-0.6.0.md).
