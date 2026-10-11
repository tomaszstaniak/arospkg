# Experimental Pi archive added to 0.4.2

2026-10-11. User authorized publication of compile/link-verified experimental
Pi builds without requiring Pi runtime availability. ScreenBar remains an internal
demonstration and is not a release prerequisite.

Built with tools/make-raspi-gui.sh 0.4.2 from 6da6625, whose program sources
are identical to release tag bbefb2f. Updated archive README and BUILD metadata
before packaging. Includes apkg, PkgManager, icon, licences and file checksums.
Both executables are ELF64 AROS ABI v1 AArch64; llvm-nm -u reports no undefined
symbols. No Pi runtime, network, GUI or self-update test was performed.
Adding the target archive to release SHA256SUMS makes it discoverable by Pi
self-update; the public release notes explicitly label that target experimental.
The three pre-existing binary archives remain unchanged.

ZIP SHA-256: 77cd5840f5f47bd730ab8cd3869331a5906d708bf4cd2487f9d64154e85dd282

131f0b4fb0a49292df199b6dc5a5e2bbe9631fa754bfc70e5031c5c46f0c9513  apkg
d80f05381b1006c7a5afcd3f941d63eba14e67aa7d4402a34303d0d59058f92a  PkgManager
87e68d8ac179018fc2273310c4f397f725fed7118b77c270d8e0c2040604b4e1  PkgManager.info
941c360b1ccc73286f118131af7b18763bd3c3c955a12971674a5a9bfa9b5f20  README.txt
