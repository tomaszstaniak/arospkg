# Links in PkgManager

PkgManager 0.7.2. The test packages (`linktest`, `linkmany`, `linkedge`)
come from a hand-written test catalogue, `tests/links/links-index.json`.

- `pages-2026-09.png`: AROS 2026.09 (no Urltext.mcc: plain Zune text
  links). `linkmany` has eight addresses: four are shown, and
  **Previous**/**Next** turn to the others ("addresses 1-4 of 8").
- `exact-2026-09.png`: the same package, second page. The address written
  as `<https://example.org/n/7?token=abc!>` in the notes keeps its final
  `!`; `https://example.org/n/6!` in running text does not. The last row is
  the 1500-character source address, shortened on screen. Below, a failed
  download: its address comes from the catalogue field, final `!` included,
  with its own link and Copy button.
- `too-long-2026-09.png`: a click on the 1500-character address: it can
  only be copied, and the status line says why.
- `pages-aros-one.png`: AROS One 1.3, the same second page, links drawn by
  Urltext.mcc.

From the first version with links (main after 0.7.1):

- `details-aros-one.png`: AROS One 1.3, the arospkg entry of the
  catalogue. The source and catalogue addresses are links, drawn by
  Urltext.mcc, each with a Copy button; the long catalogue address is
  shortened on screen.
- `about-aros-one.png`: AROS One 1.3, About after **Check for a newer
  version**: the home page and the release page as links.
- `message-link-2026-09.png`: AROS 2026.09. A test package whose download
  address cannot be reached: the message on the status line shows the
  address shortened, and its link and Copy button stand beside it. Above,
  three addresses from the package's notes and source; the `ftp://`
  address in the notes is not a link.
- `no-browser-2026-09.png`: AROS 2026.09 with no browser enabled in
  Prefs/OpenURL: a click on a link says why it could not be opened.
- `mainline-v1.png`: mainline AROS (ABIv1), the same test package and
  About; the bubble help shows the whole address.
