arospkg 0.7.7 keeps a package on the Shell path after an upgrade or rollback.

On AROS One, a package's startup script can add its drawer to the Shell path. After `apkg upgrade` or a rollback, the program was then "object not found" in the Shell until the next restart, and the next upgrade or rollback of the same package could end with "conflict". The path and assigns point to the drawer itself, and apkg used to replace the whole drawer. Now the drawer stays and only its contents are replaced, so paths and assigns to it keep working.

A program still running from the drawer during an upgrade keeps the code it loaded until it is started again, and apkg says so. Quit the program before you upgrade it.

From 0.7.x, close PkgManager and run `apkg self-update`, or use **About → Check for a newer version → Update arospkg**. From 0.4.x–0.6.0, [follow the two-step update instructions](https://github.com/tomaszstaniak/arospkg/blob/main/docs/self-update.md).

Choose the archive for your system. Raspberry Pi, mainline x86_64 and i386 builds remain experimental. Checksums are in `SHA256SUMS`.
