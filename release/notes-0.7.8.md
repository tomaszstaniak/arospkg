arospkg 0.7.8 says why a package cannot be installed on your system.

When the catalogue has a package only for another system, for example only for AROS One while you run mainline AROS, `apkg install` printed "Installing", installed nothing and ended without an error. It now stops with the reason:

    Could not install up: cannot install it
      Only a build for ABIv11 is available; this system is ABIv1.

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
