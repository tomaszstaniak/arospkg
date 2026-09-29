/*
 * dl: spike downloader for arospkg.
 *
 * Answers the questions in README.md: verified HTTPS on AROS, redirects,
 * SHA-256, and a hostname-mismatch test that must fail.
 *
 * Deliberately unlike aros-hardcode's transport in two ways, because both
 * were the point of writing this:
 *   - the CA bundle is mandatory; if it will not load we stop, we do not
 *     "continue unchecked". An unverified download is the whole attack
 *     against a package manager.
 *   - SSL_set1_host() is called, so the certificate must match the name we
 *     asked for. SSL_VERIFY_PEER alone validates the chain and would accept
 *     any valid certificate for any name.
 */

#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/bsdsocket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAFILE      "ENV:SYS/Certificates/ca-bundle.crt"
#define MAXREDIR    5
#define DL_MAXBODY  (32 * 1024 * 1024)   /* MAXBODY is taken by intuition.h */

/* libnet.a defines SocketBase but nothing opens it, and our patched OpenSSL
 * calls bsdsocket's recv/send/CloseSocket, so the base has to be ours and
 * has to be live before any TLS work. */
struct Library *SocketBase;

typedef struct { char *p; size_t len, cap; } str;

static int str_add(str *s, const char *d, size_t n)
{
    if (s->len + n + 1 > s->cap) {
        size_t c = s->cap ? s->cap : 8192;
        while (c < s->len + n + 1) c *= 2;
        if (c > DL_MAXBODY) { fprintf(stderr, "dl: response exceeds %d bytes\n", DL_MAXBODY); return 0; }
        char *q = realloc(s->p, c);
        if (!q) { fprintf(stderr, "dl: out of memory\n"); return 0; }
        s->p = q; s->cap = c;
    }
    memcpy(s->p + s->len, d, n);
    s->len += n;
    s->p[s->len] = 0;
    return 1;
}

/* https://host/path -> host, path. Only https; plain HTTP is refused
 * on purpose, so a downgrade cannot happen by accident. */
static int url_split(const char *url, char *host, size_t hn, char *path, size_t pn)
{
    const char *p;
    size_t n;

    if (strncmp(url, "https://", 8) != 0) {
        fprintf(stderr, "dl: not an https URL: %s\n", url);
        return 0;
    }
    p = url + 8;
    n = strcspn(p, "/");
    if (n == 0 || n >= hn) { fprintf(stderr, "dl: bad host in %s\n", url); return 0; }
    memcpy(host, p, n); host[n] = 0;
    if (strchr(host, ':')) { fprintf(stderr, "dl: ports other than 443 not supported\n"); return 0; }
    snprintf(path, pn, "%s", p[n] ? p + n : "/");
    return 1;
}

static int tcp_connect(const char *host)
{
    struct hostent *he;
    struct sockaddr_in sa;
    int s;

    he = gethostbyname((char *)host);
    if (!he || !he->h_addr_list[0]) { fprintf(stderr, "dl: cannot resolve %s\n", host); return -1; }

    memset(&sa, 0, sizeof sa);
    sa.sin_family = AF_INET;
    sa.sin_port = htons(443);
    memcpy(&sa.sin_addr, he->h_addr_list[0], 4);

    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { fprintf(stderr, "dl: socket() failed\n"); return -1; }
    if (connect(s, (struct sockaddr *)&sa, sizeof sa) < 0) {
        fprintf(stderr, "dl: cannot connect to %s:443\n", host);
        CloseSocket(s);
        return -1;
    }
    return s;
}

/* Body of a chunked response, decoded in place. Returns new length. */
static size_t dechunk(char *b, size_t len)
{
    char *out = b, *p = b, *end = b + len;

    for (;;) {
        char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) break;
        unsigned long sz = strtoul(p, NULL, 16);
        p = nl + 1;
        if (sz == 0) break;
        if ((size_t)(end - p) < sz) sz = (unsigned long)(end - p);
        memmove(out, p, sz);
        out += sz;
        p += sz;
        while (p < end && (*p == '\r' || *p == '\n')) p++;
    }
    return (size_t)(out - b);
}

/*
 * One request. Returns 1 on a 2xx (body in *body), 2 on a redirect (target
 * in loc), 0 on failure. verify_name is what the certificate must match:
 * normally the host, but overridable so the mismatch test can force a name
 * the server cannot possibly present.
 */
static int fetch(const char *host, const char *path, const char *verify_name,
                 str *body, char *loc, size_t locn)
{
    SSL_CTX *ctx = NULL;
    SSL *ssl = NULL;
    int s = -1, rc = 0, n;
    str raw = {0};
    char req[2048], *hdr_end, *st;
    long vr;

    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) { fprintf(stderr, "dl: SSL_CTX_new failed\n"); goto out; }

    /* Mandatory. A package manager that downloads without a trust store is
     * worse than one that refuses to download. */
    if (SSL_CTX_load_verify_locations(ctx, CAFILE, NULL) != 1) {
        fprintf(stderr, "dl: cannot load CA bundle %s, refusing to continue\n", CAFILE);
        goto out;
    }
    SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, NULL);

    if ((s = tcp_connect(host)) < 0) goto out;

    ssl = SSL_new(ctx);
    if (!ssl) goto out;
    SSL_set_fd(ssl, s);
    SSL_set_tlsext_host_name(ssl, host);           /* SNI: which vhost */
    /* SSL_set1_dnsname() is the OpenSSL 4.0 name; SSL_set1_host() still
     * exists but is deprecated in 4.0. */
    if (SSL_set1_dnsname(ssl, verify_name) != 1) {  /* which name the cert must match */
        fprintf(stderr, "dl: SSL_set1_dnsname(%s) failed\n", verify_name);
        goto out;
    }

    if (SSL_connect(ssl) != 1) {
        fprintf(stderr, "dl: TLS handshake with %s failed\n", host);
        vr = SSL_get_verify_result(ssl);
        if (vr != X509_V_OK)
            fprintf(stderr, "dl: certificate rejected: %s\n", X509_verify_cert_error_string(vr));
        ERR_print_errors_fp(stderr);
        goto out;
    }

    snprintf(req, sizeof req,
             "GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: arospkg-dl/0\r\n"
             "Accept: */*\r\nConnection: close\r\n\r\n", path, host);
    if (SSL_write(ssl, req, (int)strlen(req)) <= 0) {
        fprintf(stderr, "dl: cannot send request\n");
        goto out;
    }

    for (;;) {
        char chunk[16384];
        n = SSL_read(ssl, chunk, sizeof chunk);
        if (n <= 0) break;
        if (!str_add(&raw, chunk, (size_t)n)) goto out;
    }
    if (!raw.len) { fprintf(stderr, "dl: empty response\n"); goto out; }

    hdr_end = strstr(raw.p, "\r\n\r\n");
    if (!hdr_end) { fprintf(stderr, "dl: malformed response\n"); goto out; }
    *hdr_end = 0;

    st = strchr(raw.p, ' ');
    if (!st) { fprintf(stderr, "dl: no status line\n"); goto out; }
    int code = atoi(st + 1);

    if (code >= 300 && code < 400) {
        char *l = strstr(raw.p, "\nLocation:");
        if (!l) l = strstr(raw.p, "\nlocation:");
        if (!l) { fprintf(stderr, "dl: %d without Location\n", code); goto out; }
        l += 10;
        while (*l == ' ') l++;
        size_t ln = strcspn(l, "\r\n");
        if (ln >= locn) { fprintf(stderr, "dl: Location too long\n"); goto out; }
        memcpy(loc, l, ln); loc[ln] = 0;
        printf("  %d -> %s\n", code, loc);
        rc = 2;
        goto out;
    }
    if (code < 200 || code > 299) { fprintf(stderr, "dl: HTTP %d\n", code); goto out; }

    int chunked = (strstr(raw.p, "\nTransfer-Encoding: chunked") ||
                   strstr(raw.p, "\ntransfer-encoding: chunked")) ? 1 : 0;

    char *b = hdr_end + 4;
    size_t blen = raw.len - (size_t)(b - raw.p);
    if (chunked) blen = dechunk(b, blen);
    if (!str_add(body, b, blen)) goto out;
    rc = 1;

out:
    if (ssl) { SSL_shutdown(ssl); SSL_free(ssl); }
    if (ctx) SSL_CTX_free(ctx);
    if (s >= 0) CloseSocket(s);
    free(raw.p);
    return rc;
}

static void sha256_print(const unsigned char *d, size_t n)
{
    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int mdlen = 0, i;
    EVP_MD_CTX *c = EVP_MD_CTX_new();

    if (!c) return;
    if (EVP_DigestInit_ex(c, EVP_sha256(), NULL) == 1 &&
        EVP_DigestUpdate(c, d, n) == 1 &&
        EVP_DigestFinal_ex(c, md, &mdlen) == 1) {
        printf("  sha256 ");
        for (i = 0; i < mdlen; i++) printf("%02x", md[i]);
        printf("\n");
    }
    EVP_MD_CTX_free(c);
}

static void usage(void)
{
    printf("usage: dl [-o FILE] [--verify-name NAME] <https-url>\n"
           "  --verify-name forces the name the certificate must match;\n"
           "  give a name the server cannot present and the fetch must fail.\n");
}

int main(int argc, char **argv)
{
    char host[256], path[1024], loc[1024];
    const char *url = NULL, *out = NULL, *vname = NULL;
    str body = {0};
    int i, redirs = 0, rc, ret = 20;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) out = argv[++i];
        else if (!strcmp(argv[i], "--verify-name") && i + 1 < argc) vname = argv[++i];
        else if (argv[i][0] != '-') url = argv[i];
        else { usage(); return 20; }
    }
    if (!url) { usage(); return 20; }

    if (!url_split(url, host, sizeof host, path, sizeof path)) return 20;

    SocketBase = OpenLibrary((STRPTR)"bsdsocket.library", 4);
    if (!SocketBase) {
        fprintf(stderr, "dl: cannot open bsdsocket.library: is the network stack up?\n");
        return 20;
    }

    for (;;) {
        printf("  GET https://%s%s\n", host, path);
        rc = fetch(host, path, vname ? vname : host, &body, loc, sizeof loc);
        if (rc == 2) {
            if (++redirs > MAXREDIR) { fprintf(stderr, "dl: too many redirects\n"); goto done; }
            if (!url_split(loc, host, sizeof host, path, sizeof path)) goto done;
            continue;
        }
        break;
    }
    if (rc != 1) goto done;

    printf("  %lu bytes\n", (unsigned long)body.len);
    sha256_print((const unsigned char *)body.p, body.len);

    if (out) {
        BPTR fh = Open((STRPTR)out, MODE_NEWFILE);
        if (!fh) { fprintf(stderr, "dl: cannot open %s\n", out); goto done; }
        Write(fh, body.p, (LONG)body.len);
        Close(fh);
        printf("  wrote %s\n", out);
    }
    ret = 0;

done:
    free(body.p);
    CloseLibrary(SocketBase);
    return ret;
}
