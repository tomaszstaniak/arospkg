# arospkg documentation

Current release: **0.7.3**. Start with the guide for your task.

## Using the manager

- [Install and use](guide/using.md)
- [Update apkg and PkgManager](self-update.md): from the Shell or from PkgManager
- [Update window screenshots (0.7.1)](screenshots/0.7.1/README.md)
- [Downloads and release notes](https://github.com/tomaszstaniak/arospkg/releases)

## Publishing an application

- [Authoring guide](guide/authoring.md): prepare, validate and submit a package.
- [Metadata reference](guide/metadata.md)
- [Archive layout and testing](guide/packaging.md): package structure, limits and runtime checks.

## Development and integration

[Integration with pkg](../README.md#integration-with-pkg) is in progress in apkg-fusion. A shared release and migration commitments remain subject to agreement; the guides above cover released arospkg.

- [Build from source](../README.md#building)
- [ARexx interface](arexx.md)
- [Application folder format](rfc/0002-application-folder.md): implemented schema.
- [Storefront metadata](rfc/0003-storefront-metadata.md): proposal and local demonstration, not a publishing format.
- [Retired package-format draft](rfc/0001-package-format.md): links to its replacements.

The storefront proposal is demonstrated locally in PkgManager; purchasing is not
available. Supported package fields are defined in the metadata reference.
