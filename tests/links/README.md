# Links in PkgManager: guest test material

What the window test of PkgManager's links uses. The address logic itself
(which text is a link, how it is shortened, which address may be opened)
is covered on the host by `tests/test_linktext.c`.

- `links-index.json`: a hand-written catalogue for a test root
  (`<root>/db/index.json`). Its one package (x86_64, v11 and v1) has a
  193-character source address with `&` and `%`, notes with a second
  address, one with `;` (copy only) and an `ftp://` address (not a link),
  and a download address that cannot resolve, so **Install** fails with a
  message that holds an address. `linkmany` has eight addresses (two
  pages), a 1500-character source address ending in `!` (copied only),
  `<address!>` in its notes and a download address ending in `!`;
  `linkedge` has a 480-character source address, the longest that is
  opened.
- `lk_browser.c`: a stand-in browser. OpenURL starts it as
  `lk_browser %u`; it appends what it got to `RAM:lk/opened.txt`, so the
  test compares the exact address handed over.
- `lk_openurl.c`: sets OpenURL for the running system only (ENV:, gone
  after a reboot): `set COMMAND` (the stand-in), `none` (every browser
  off: a click must say it could not open), `slow COMMAND PORT` (OpenURL
  starts a program and waits for an ARexx port that never comes: the
  window must keep working meanwhile), `show`.
- `lk_clip.c`: prints the text on clipboard unit 0 with its length, to
  compare a copied address byte for byte.

Build each with the toolchain and SDK of the target, for example
`x86_64-aros-gcc -O2 -I$SDK/include lk_clip.c -L$SDK/lib -o lk_clip`.
Run PkgManager with `--root <root> --log <file>`: the log records every
link opened (and OpenURL's answer) and copied, and whether Urltext.mcc
draws the links.
