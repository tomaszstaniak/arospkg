---
title: "SDK example: report socket errors as such"
status: noted, not scheduled (2026-10-02)
---

# SDK example: report socket errors as such

`sdk/examples/https_get.c` in the Mbed TLS 3.6.7-aros2 package turns every
failed `send`/`recv` into `MBEDTLS_ERR_SSL_INTERNAL_ERROR` and drops
`Errno()`. A connection reset then prints "SSL - Internal error", and a
developer who copies the example inherits the same confusion apkg had
(`docs/reports/2026-10-02-slow-download-reset/`).

The fix is the one apkg got on `mbedtls`: keep `Errno()` from the failed
call on the connection, and report it ("connection reset", "socket error
N") when the TLS layer gives back its internal error. Only the example
changes; the libraries do not.

It needs a new packaging revision of the Mbed TLS package, a build from a
named commit, and a run of the example on ABIv11 before it is published.
Not part of the transport fix in 0.3.1-rc2.
