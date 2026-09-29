arospkg 0.3.1-rc1 -- RELEASE CANDIDATE, for testing
===================================================

This is a test build, not a release. It is published so that it can be
tried on more systems than ours. Do not replace a working arospkg 0.3 with
it; test it on its own, as described below.

What changed from 0.3
---------------------

TLS moved from the SDK's OpenSSL 1.1.0h to Mbed TLS 3.6.7 (TLS 1.2 and 1.3),
compiled into the programs; they are about a quarter of the size. On
systems without a system entropy source (AROS One and the other ABIv11
systems, the i386 line, hosted aarch64) randomness comes from
jitterentropy-library 3.7.0; on mainline AROS from getentropy(). If the
entropy source is unusable, apkg refuses to connect; it never falls back to
anything else.

This candidate is NOT yet an approved security update: the release
regression and the analysis of the entropy source on our test configuration
are still in progress. See the release notes on GitHub for what was tested.

Targets
-------

Each archive is for one CPU and ABI; a binary for one does not run on
another. What was checked for each archive is in the table below: "built"
means compiled only; "runs" means started on that system; "TLS" means it
fetched over HTTPS and refused bad certificates; "full" means the whole
package cycle (install, start, remove) was done with it.

STATUS_TABLE

Testing it without touching your installation
---------------------------------------------

Unpack the archive anywhere (not over C: or SYS:Packages), then from a
Shell in that drawer:

    apkg --version
    apkg --root RAM:RcTest update
    apkg --root RAM:RcTest search
    apkg --root RAM:RcTest show zaphod
    apkg --root RAM:RcTest install zaphod
    apkg --root RAM:RcTest list
    apkg --root RAM:RcTest remove zaphod

--root keeps everything (index, cache, installed programs) in RAM:RcTest,
away from SYS:Packages and from an installed arospkg 0.3. Nothing is
written to C:, LIBS: or SYS:Packages. Delete RAM:RcTest when done.

It needs a running TCP/IP stack and the certificate bundle
ENV:SYS/Certificates/ca-bundle.crt. The catalogue offers x86_64 ABIv11
packages only; on other targets search hides them and install refuses them,
which is itself worth reporting.

Please report: the system and its version, the archive's name and the
output of apkg --version, what you ran and what it printed. Issues:
https://github.com/tomaszstaniak/arospkg/issues

Licences
--------

arospkg: MIT (LICENSE). Included: Mbed TLS 3.6.7 (Apache-2.0),
jitterentropy-library 3.7.0 (BSD-3-Clause; not in the mainline archive),
zlib; see licenses/.
