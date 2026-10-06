arospkg for native AROS on Raspberry Pi (aarch64, ABIv1) -- local test build
============================================================================

This archive carries apkg (Shell) and PkgManager (Zune window) built with
the Pi's own Developer SDK (Clang 20, ABIv1). It is a local build for
testing, not a published release. It does not run on hosted Macaros
aarch64 or on x86_64 AROS.

INSTALL

  1. Unpack the archive, e.g. into SYS:arospkg.
  2. Copy apkg C:          (Shell use)
  3. Start PkgManager from its icon, or in the Shell: Run PkgManager
     Keep PkgManager.info next to PkgManager.

Online operations (Update index, network install) need a TCP/IP stack
(bsdsocket.library) and ENV:SYS/Certificates/ca-bundle.crt. Without them
apkg and PkgManager can still work from a local catalogue and cached
archives. PkgManager needs Zune (muimaster.library). Its ARexx port is
optional: when RexxMast is not running, the window works without it.

Packages go into SYS:Packages unless another root is given
(apkg --root DIR ...). Downloads are checked against the catalogue's
SHA-256; a mismatch stops the installation and is never skipped.

Verify the files with SHA256SUMS. See BUILD.txt for the commit.
Licences: LICENSE and licenses/.
