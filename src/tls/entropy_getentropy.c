/* Randomness for Mbed TLS where the system has a source: mainline's posixc
 * getentropy(), answered from entropy.resource. A system source is used
 * alone and preferred to anything in this program; if it fails, the
 * handshake is refused. */
#include <mbedtls/platform_util.h>
#include <unistd.h>
#include <stddef.h>

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    size_t at = 0;
    (void)data;
    while (at < len) {
        size_t n = len - at > 256 ? 256 : len - at;   /* getentropy's limit */
        if (getentropy(output + at, n) != 0) {
            mbedtls_platform_zeroize(output, len);
            *olen = 0;
            return -1;
        }
        at += n;
    }
    *olen = len;
    return 0;
}
