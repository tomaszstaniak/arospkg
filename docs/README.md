# arospkg documentation

Current release: **0.4.2**, for x86_64 ABIv11 and i386 ABIv0, with experimental
mainline x86_64 ABIv1 and native Raspberry Pi aarch64 ABIv1 builds. PkgManager
is included in the x86_64 ABIv11 and Pi archives. The Pi build is compile/link
verified only. Hosted Macaros remains at 0.3.1. See each target's test coverage.

[0.4.2 screenshots on AROS One](screenshots/0.4.2/README.md).

## Install and use

- [Install arospkg](../README.md#install)
- [Find, install, open and remove programs](guide/using.md)
- [Update apkg itself](self-update.md)
- [Release notes and downloads](https://github.com/tomaszstaniak/arospkg/releases)

## Prepare and publish packages

- [Authoring guide](guide/authoring.md): create a manifest, check, pack and submit.
- [Packaging guide](guide/packaging.md): archive layout and installation checks.
- [Metadata reference](guide/metadata.md): fields, types and limits.

## Integrate and develop

- [ARexx interface](arexx.md)
- [ScreenBar item](screenbar.md): PkgUpdates, an optional update counter
- [Build from source](../README.md#building)
- [Package format RFC](rfc/0001-package-format.md)
- [Application folder RFC](rfc/0002-application-folder.md)

The RFCs are proposals. Use the authoring guide and metadata reference for
the supported format.

The packaging guide includes output from earlier releases. Documentation
examples are checked by `tests/test_doc_examples.py`.
