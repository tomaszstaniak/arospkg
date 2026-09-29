# arospkg documentation

Start here. Each question has one document that answers it.

| you want to know | read |
|---|---|
| **how** to package a program, test it and get it into the index | [guide/packaging.md](guide/packaging.md) |
| **exactly what** a field means, who writes it and who checks it | [reference/metadata.md](reference/metadata.md) |
| **what we propose** for formats that do not exist yet | [rfc/0001-package-format.md](rfc/0001-package-format.md), [rfc/0002-application-folder.md](rfc/0002-application-folder.md) |
| what a script can do with PkgManager over ARexx | [arexx.md](arexx.md) |
| what the ABI tags mean and why they matter | [reference/abi.md](reference/abi.md) |
| where the project stands, release by release | [STATUS.md](STATUS.md) |

The guide and the reference describe arospkg 0.3 and the tools in `tools/`.
The RFCs are drafts, and no released client implements them. Their examples
are marked as proposals. The current-format examples in the guide and the
reference are checked against `tools/mkindex.py` by
`tests/test_doc_examples.py`, which `tests/run-host-tests.sh` runs.

## For each audience

- **Application authors**: the guide, sections 1 to 5 and 6A. RFC 0001 if
  you want to ship a manifest inside your archive.
- **Index maintainers**: the guide, sections 1 to 5 and 6B, and the
  reference's table [What is checked where](reference/metadata.md#what-is-checked-where).
- **Tool writers**: the reference (index format and local registry), and
  `arexx.md` for driving PkgManager.

## Everything else

Records, not instructions:

- `reports/`: evidence from runs on AROS, one directory per run. The run
  the guide is built on is `reports/2026-09-29-docs-walk/`.
- `spikes/`: bounded experiments and what they measured.
- `backlog/`: design documents. `package-manager-mvp.md` holds the reasoning
  behind the installer's guarantees. `package-standard-proposal.md` holds
  the reasoning behind RFC 0001, which now carries the definition.
- `adr/`: architecture decisions.
- `release/`: release notes.
