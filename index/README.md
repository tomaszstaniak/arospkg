# Candidates

Packages imported from the AROS Archives catalogue by
`tools/import_catalogue.py`, not yet reviewed.

- `candidates.json`: every catalogue entry offered, with the alternatives
  that were not chosen.
- `candidates/<id>.<arch>.toml`: a skeleton manifest, `status = "skeleton"`,
  or one marked `excluded` with its reason.

Nothing here is published. A reviewed and tested manifest moves to
`manifests/` in [arospkg-index](https://github.com/tomaszstaniak/arospkg-index),
which holds the approved manifests and the index clients read. See
`docs/guide/packaging.md`, sections 6B and 7.
