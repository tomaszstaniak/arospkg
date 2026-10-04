# arospkg documentation

Current release: **0.4.0**, for x86_64 ABIv11.

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
- [Build from source](../README.md#building)
- [Package format RFC](rfc/0001-package-format.md)
- [Application folder RFC](rfc/0002-application-folder.md)

The RFCs are proposals. Use the authoring guide and metadata reference for
the supported format.

The packaging guide includes output from earlier releases. Documentation
examples are checked by `tests/test_doc_examples.py`.
