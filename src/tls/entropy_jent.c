/* Randomness for Mbed TLS where the system has no source (AROS One and the
 * other ABIv11 distributions, the i386 line, hosted aarch64): the CPU jitter
 * entropy source of jitterentropy-library 3.7.0 (third_party/jitterentropy),
 * with its own start-up tests and SP 800-90B health tests.
 *
 * Nothing here tunes it. jent_entropy_init() decides whether the timer is
 * good enough; if it says no, or a later read fails a health test, the
 * source fails and the handshake is refused. There is no fallback to any
 * other source. */
#include <mbedtls/platform_util.h>
#include <jitterentropy.h>
#include <stddef.h>

static struct rand_data *collector;
static int init_rc = 1;          /* 1: not tried yet */

int apkg_jent_status(void) { return init_rc; }

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    ssize_t got;
    (void)data;
    *olen = 0;
    if (init_rc == 1) {
        init_rc = jent_entropy_init();
        if (init_rc == 0) {
            collector = jent_entropy_collector_alloc(0, 0);
            if (!collector) init_rc = -1;
        }
    }
    if (init_rc != 0) return -1;
    got = jent_read_entropy(collector, (char *)output, len);
    if (got < 0 || (size_t)got != len) {
        mbedtls_platform_zeroize(output, len);
        return -1;
    }
    *olen = len;
    return 0;
}
