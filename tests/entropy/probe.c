/* Entropy probe for src/tls/entropy_aros.c, run on the target.
 *   probe RAW <file> <n>   write n raw timing deltas (8 bytes each, LE)
 *   probe POLL             run the source as Mbed TLS does, report pass/fail
 * The raw file is analysed on the host (tests/entropy/estimate.py). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
int apkg_jitter_raw(uint64_t *out, size_t n);
int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen);
int main(int argc, char **argv)
{
    if (argc == 4 && !strcmp(argv[1], "RAW")) {
        size_t n = (size_t)atol(argv[3]);
        uint64_t *v = malloc(n * sizeof *v);
        FILE *f = fopen(argv[2], "wb");
        if (!v || !f) { printf("cannot allocate or open\n"); return 20; }
        apkg_jitter_raw(v, n);
        fwrite(v, sizeof *v, n, f);
        fclose(f);
        printf("wrote %lu samples\n", (unsigned long)n);
        return 0;
    }
    if (argc == 2 && !strcmp(argv[1], "POLL")) {
        unsigned char b[64]; size_t ol = 0; int i, r = mbedtls_hardware_poll(NULL, b, sizeof b, &ol);
        printf("poll %s, %lu bytes: ", r == 0 ? "PASS" : "FAIL", (unsigned long)ol);
        for (i = 0; i < 16; i++) printf("%02x", b[i]);
        printf("\n");
        return r == 0 ? 0 : 10;
    }
    printf("usage: probe RAW <file> <n> | probe POLL\n");
    return 20;
}
