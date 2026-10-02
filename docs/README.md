# arospkg documentation

Start here. Each question has one document that answers it.

| you want to know | read |
|---|---|
| **how to install arospkg** | [Install](../README.md#install) |
| **how to use it** | [Graphical interface](../README.md#use-the-graphical-interface) · [Shell commands](../README.md#use-the-shell) |
| **how** to package a program, test it and get it into the index | [guide/packaging.md](guide/packaging.md) |
| **exactly what** a field means, who writes it and who checks it | [guide/metadata.md](guide/metadata.md) |
| **what we propose** for formats that do not exist yet | [rfc/0001-package-format.md](rfc/0001-package-format.md), [rfc/0002-application-folder.md](rfc/0002-application-folder.md) |
| what a script can do with PkgManager over ARexx | [arexx.md](arexx.md) |
| what changed and which targets were tested | [Release notes](https://github.com/tomaszstaniak/arospkg/releases) |

The current release is **0.3.1**. Start with the
[installation instructions](../README.md#install) and
[release notes](https://github.com/tomaszstaniak/arospkg/releases/tag/v0.3.1).
The packaging guide and metadata reference are based on the recorded 0.3
walkthrough; their captured outputs are historical, not new 0.3.1 runs.
The RFCs are drafts, and no released client implements them. Their examples
are marked as proposals. The current-format examples in the guide and the
reference are checked against `tools/mkindex.py` by
`tests/test_doc_examples.py`, which `tests/run-host-tests.sh` runs.

## For each audience

- **Application authors**: the guide, sections 1 to 5 and 6A. RFC 0001 if
  you want to ship a manifest inside your archive.
- **Index maintainers**: the guide, sections 1 to 5 and 6B, and the
  reference's table [What is checked where](guide/metadata.md#what-is-checked-where).
- **Tool writers**: the reference (index format and local registry), and
  `arexx.md` for driving PkgManager.

## Development records

Experiments, run reports, plans and coordination notes are local working
material, not user documentation. They are ignored by Git in the current
tree; previously published records remain in Git history. The small
fixtures needed by public tests live in `tests/fixtures/`.
