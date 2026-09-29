# Moved

This file used to mix a how-to, a field reference and a proposal. They are
now separate:

- how to package and test: [guide/packaging.md](guide/packaging.md)
- what each field means: [reference/metadata.md](reference/metadata.md)
- the embedded manifest (`.arospkg/manifest.toml`), which is proposed:
  [rfc/0001-package-format.md](rfc/0001-package-format.md)

One piece of advice here was wrong: it said to omit `abi` for mainline. The
client refuses an entry with no `abi`. Write `abi = "v1"`.
