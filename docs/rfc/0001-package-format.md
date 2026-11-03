# RFC 0001: package format and embedded manifest — retired

The September 2026 schema 0 draft is obsolete and must not be used to create packages. Its original text remains in Git history.

The supported publishing workflow is implemented in **apkg-pack 0.2.0**. It writes `.arospkg/manifest.toml` into a ZIP, and `submit` reads it to create a catalogue entry. This is not an implementation of the old schema 0 syntax.

Use these documents instead:

- [Publishing a program](../guide/authoring.md): create, check, build and submit.
- [Archive layout and testing](../guide/packaging.md).
- [Metadata reference](../guide/metadata.md): supported fields and catalogue rules.
- [Application folder format](0002-application-folder.md): implemented `application.json`, schema 1, used by `apkg run`.
