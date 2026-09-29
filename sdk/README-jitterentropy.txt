jitterentropy-library 3.7.0 for AROS x86_64 (ABIv11), port revision aros1
=========================================================================

Static libraries of jitterentropy-library, a CPU-jitter entropy source, for
programs built for AROS x86_64 with ABIv11 (AROS One and the other current
distributions), which have no system entropy source.

Upstream:  https://github.com/smuellerDD/jitterentropy-library, release
           3.7.0, v3.7.0.tar.gz, SHA-256
           f5eaccc9d2977c83308651be9379f09f34348398f419e8f8b5bbd95928c777ed
           The library sources are unchanged. The platform header
           include/jitterentropy-base-user.h is ours: it replaces upstream's,
           which uses POSIX and Linux facilities AROS does not have.
Licence:   BSD-3-Clause or GPL-2.0 (LICENSE, LICENSE.bsd, LICENSE.gplv2);
           this port uses it under the BSD licence. Copyright Stephan Mueller.
Build:     BUILD.txt; src/make-packages.sh rebuilds it.


What is in it
-------------

  lib/libjitterentropy.a         the library, compiled with -O0 as upstream
                                 requires (it refuses to build otherwise),
                                 without the internal timer thread
  lib/libjitterentropy_mbedtls.a an adapter that provides
                                 mbedtls_hardware_poll() for Mbed TLS
  include/jitterentropy.h        upstream's interface
  include/jitterentropy-base-user.h  the AROS platform layer
  examples/jent_read.c           read 32 bytes from the library directly
  src/                           the adapter, the platform header, the build
                                 script


How it decides it can run
-------------------------

The platform layer gives the library the CPU's time stamp counter (RDTSC).
At jent_entropy_init() the library measures whether that timer varies
enough; if not, it returns an error and must not be used. The adapter never
overrides that answer, has no fallback, and returns a failure to Mbed TLS
instead. Under QEMU without KVM the time stamp counter moves in coarse steps;
on AROS One 1.3 under QEMU the library accepted it, but whether it does on a
given machine is decided on that machine, at run time.


Using it with Mbed TLS
----------------------

The package MbedTLS-3.6.7-aros1 is configured to take its entropy from
mbedtls_hardware_poll(). Link, after the Mbed TLS libraries:

    -ljitterentropy_mbedtls -ljitterentropy

The adapter creates one collector per program on first use and serialises
access to it with an exec semaphore, so several tasks of one program may
use TLS. Call aros_jent_shutdown() at the end to free it;
aros_jent_status() returns 0 once the source is usable and a negative or
jitterentropy error code if it is not.


Using it directly
-----------------

    x86_64-aros-gcc -O2 examples/jent_read.c -I$JE/include -L$JE/lib \
        -ljitterentropy -o jent_read

jent_entropy_init(), jent_entropy_collector_alloc(), jent_read_entropy(),
jent_entropy_collector_free(): see include/jitterentropy.h. Check every
return value; a failed health test is reported as a negative return from
jent_read_entropy(), and the data must then not be used.


What this does not claim
------------------------

This is a port and a build, not a certification. The entropy rate on a
given machine depends on that machine. The port's test reports describe
what was measured, on which configuration, with upstream's own SP 800-90B
procedure, and what was not.
