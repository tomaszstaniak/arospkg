/* Randomness for Mbed TLS on AROS, and the time functions it needs.
 *
 * AROS One has no getentropy() and no entropy.resource, and the SDK's
 * OpenSSL seeds itself from the C library's rand(). So the source is here:
 *
 *   1. RDRAND, when CPUID says the CPU has it;
 *   2. always, timing jitter: for each sample, the number of loop
 *      iterations until the cycle counter next changes, together with the
 *      size of that change; many samples per output byte, folded through
 *      SHA-256.
 *
 * Jitter is credited at 1/16 bit per sample, ten times below what was
 * measured. The SP 800-90B health tests run on every sample; if one fails,
 * or the counter never moves, the source reports failure and the handshake
 * is refused rather than run on bad randomness. */
#include <mbedtls/sha256.h>
#include <mbedtls/platform_time.h>
#include <mbedtls/platform_util.h>
#include <sys/time.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

#if defined(__x86_64__) || defined(__i386__)
static inline uint64_t cycles(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}
static int have_rdrand(void)
{
    uint32_t a = 1, b, c = 0, d;
    __asm__ volatile("cpuid" : "+a"(a), "=b"(b), "+c"(c), "=d"(d));
    return (c >> 30) & 1;
}
static int rdrand64(uint64_t *v)
{
    unsigned char ok;
    int i;
    for (i = 0; i < 10; i++) {
#if defined(__x86_64__)
        __asm__ volatile("rdrand %0; setc %1" : "=r"(*v), "=qm"(ok));
#else
        uint32_t lo, hi;
        __asm__ volatile("rdrand %0; setc %1" : "=r"(lo), "=qm"(ok));
        if (ok) __asm__ volatile("rdrand %0; setc %1" : "=r"(hi), "=qm"(ok));
        *v = ((uint64_t)hi << 32) | lo;
#endif
        if (ok) return 1;
    }
    return 0;
}
#elif defined(__aarch64__)
static inline uint64_t cycles(void)
{
    uint64_t v;
    __asm__ volatile("isb; mrs %0, cntvct_el0" : "=r"(v));
    return v;
}
static int have_rdrand(void) { return 0; }
static int rdrand64(uint64_t *v) { (void)v; return 0; }
#else
#  error "no cycle counter for this CPU"
#endif

/* Credit: 1/16 bit per sample. Measured on AROS One under QEMU TCG, the
 * most-common-value estimate was 0.665 bits per sample over 50,000 samples
 * (docs/reports/2026-09-29-tls-entropy/), ten times more than credited. */
#define SAMPLES_PER_BIT 16
/* SP 800-90B 4.4 health tests for H = 1/16 bit/sample, alpha = 2^-20:
 * Repetition Count Test cutoff 1 + ceil(20/H) = 321; Adaptive Proportion
 * Test, window 512, cutoff 509. They catch a source that has stopped
 * varying, not a subtly weak one; the estimate above is what argues for
 * the credit. */
#define RCT_CUTOFF 321
#define APT_WINDOW 512
#define APT_CUTOFF 509
#define SPIN_CAP 200000

/* One sample: wait for the cycle counter to change and count the loop
 * iterations that took. Under QEMU's TCG the counter advances in coarse
 * steps, so timing a short piece of work mostly measures 0; the number of
 * iterations that fit between two steps depends on how the host schedules
 * the emulator, which nothing in the guest can predict. On hardware the
 * counter moves every cycle and the count is small but still varies. The
 * delta of the counter itself is folded in as well. */
static volatile uint32_t spin;
static uint64_t sample(uint64_t prev)
{
    uint64_t t0 = cycles(), t1;
    uint32_t n = 0;
    (void)prev;
    do { t1 = cycles(); spin += n; } while (t1 == t0 && ++n < SPIN_CAP);
    return ((uint64_t)n << 32) ^ (t1 - t0);
}

/* Exported for the entropy test in tests/. */
int apkg_jitter_raw(uint64_t *out, size_t n)
{
    uint64_t d = 0;
    size_t i;
    for (i = 0; i < n; i++) out[i] = d = sample(d);
    return 0;
}

int mbedtls_hardware_poll(void *data, unsigned char *output, size_t len, size_t *olen)
{
    mbedtls_sha256_context h;
    unsigned char block[32];
    size_t done = 0, i, stuck = 0;
    uint64_t d = 0, r, rct_val = 0, apt_ref = 0;
    unsigned rct_n = 0, apt_i = 0, apt_n = 0;
    int rd = have_rdrand(), bad = 0;
    (void)data;

    while (done < len && !bad) {
        size_t want = len - done < 32 ? len - done : 32;
        size_t nsamp = want * 8 * SAMPLES_PER_BIT;
        mbedtls_sha256_init(&h);
        mbedtls_sha256_starts(&h, 0);
        for (i = 0; i < nsamp; i++) {
            d = sample(d);
            if ((d >> 32) >= SPIN_CAP) stuck++;          /* counter never moved */
            if (d == rct_val) { if (++rct_n >= RCT_CUTOFF) bad = 1; }
            else { rct_val = d; rct_n = 1; }
            if (apt_i == 0) { apt_ref = d; apt_n = 1; }
            else if (d == apt_ref && ++apt_n >= APT_CUTOFF) bad = 1;
            if (++apt_i == APT_WINDOW) apt_i = 0;
            mbedtls_sha256_update(&h, (const unsigned char *)&d, sizeof d);
        }
        if (stuck > nsamp / 8) bad = 1;
        if (rd)
            for (i = 0; i < 4; i++)
                if (rdrand64(&r))
                    mbedtls_sha256_update(&h, (const unsigned char *)&r, sizeof r);
        mbedtls_sha256_finish(&h, block);
        mbedtls_sha256_free(&h);
        if (!bad) memcpy(output + done, block, want);
        done += want;
    }
    mbedtls_platform_zeroize(block, sizeof block);
    if (bad) { mbedtls_platform_zeroize(output, len); *olen = 0; return -1; }
    *olen = len;
    return 0;
}

mbedtls_ms_time_t mbedtls_ms_time(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (mbedtls_ms_time_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

/* The ABIv11 C library has gmtime() but not gmtime_r(). apkg makes one TLS
 * connection at a time, from one task, so copying the static result out is
 * safe here. */
struct tm *mbedtls_platform_gmtime_r(const mbedtls_time_t *tt, struct tm *out)
{
    time_t t = (time_t)*tt;
    struct tm *g = gmtime(&t);
    if (!g) return NULL;
    *out = *g;
    return out;
}
