arospkg 0.7.4-test1 -- TEST BUILD
=================================

A build for evaluating links inside text in PkgManager. It is not a release.
Unpack it into a drawer of its own and start PkgManager from there; it does
not change an installed arospkg.

What is different from 0.7.3
----------------------------

Web addresses in PkgManager now stand where they occur in the text: in a
package's details, in About, in confirmation windows and in messages. The
rows of addresses below the text, their Copy buttons and Previous/Next are
gone.

- Click a link to open it in the browser chosen in Prefs/OpenURL.
- The right mouse button on a link offers Open link and Copy link address.
- Dragging over text selects it and opens nothing; Right Amiga+C copies it.
- With the keyboard: Tab to the text, Left and Right go from link to link,
  Return opens, Right Amiga+C copies the link, Tab leaves.
- A long address is shortened on screen; its bubble help shows it whole, and
  opening and copying use it whole.

An address longer than 480 characters, or one with ' * ; or $, can only be
copied, as in 0.7.2. No class file is needed: the text view is part of
PkgManager. apkg is unchanged apart from its version.
