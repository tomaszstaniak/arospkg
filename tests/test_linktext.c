/* Host tests for src/pkgmanager/linktext.c: which addresses PkgManager
 * links, how it shortens them, which it will not hand to a browser. */
#include "../src/pkgmanager/linktext.h"
#include <stdio.h>
#include <string.h>

static int fails;

static void check(int ok, const char *what)
{
    if (!ok) { printf("FAIL %s\n", what); fails++; }
}

/* Every address in `text`, joined with " | ". */
static const char *all(const char *text)
{
    static char out[2048];
    const char *p = text, *at;
    size_t len, k = 0;
    out[0] = 0;
    for (;;) {
        at = text;
        len = lk_next_url(p, &at);
        if (!len) break;
        k += (size_t)snprintf(out + k, sizeof out - k, "%s%.*s", k ? " | " : "", (int)len, at);
        p = at + len;
    }
    return out;
}

static void found(const char *text, const char *want)
{
    const char *got = all(text);
    if (strcmp(got, want)) { printf("FAIL in [%s]\n  got  [%s]\n  want [%s]\n", text, got, want); fails++; }
}

int main(void)
{
    char buf[512];

    found("source: https://github.com/a/b", "https://github.com/a/b");
    found("see https://example.org/x.", "https://example.org/x");
    found("(https://example.org/x) and http://e.org/y, too", "https://example.org/x | http://e.org/y");
    found("https://en.wikipedia.org/wiki/A_(B)", "https://en.wikipedia.org/wiki/A_(B)");
    found("q https://e.org/?a=1&b=%2F&c=%41 end", "https://e.org/?a=1&b=%2F&c=%41");
    found("\"https://e.org/q\"", "https://e.org/q");
    found("<https://e.org/q>", "https://e.org/q");
    found("HTTPS://E.ORG/", "HTTPS://E.ORG/");
    found("ftp://e.org/x mailto:a@b.c file:///x", "");
    found("xhttps://e.org/ is a word", "");
    found("https:// alone", "");
    found("two https://a.org/1 https://a.org/1", "https://a.org/1 | https://a.org/1");
    found("line\nhttps://a.org/\nnext", "https://a.org/");

    check(lk_unsafe("https://e.org/a;b"), "; is unsafe");
    check(lk_unsafe("https://e.org/$HOME"), "$ is unsafe");
    check(lk_unsafe("https://e.org/a*b"), "* is unsafe");
    check(lk_unsafe("https://e.org/it's"), "' is unsafe");
    check(!lk_unsafe("https://e.org/?a=1&b=%2F#frag"), "& % # are safe");

    lk_label("https://example.org/a/very/long/path/that/goes/on/and/on/index.html", buf, sizeof buf, 32);
    check(strlen(buf) == 32, "label is max characters");
    check(!strncmp(buf, "https://example.org", 19), "label keeps the start");
    check(strstr(buf, "...") != NULL, "label marks the cut");
    check(!strcmp(buf + strlen(buf) - 10, "index.html"), "label keeps the end");
    lk_label("https://e.org/", buf, sizeof buf, 32);
    check(!strcmp(buf, "https://e.org/"), "short address unchanged");

    lk_shorten("failed (https://example.invalid/a/very/long/name-1.0.x86_64-aros-v11.zip) -- why",
               buf, sizeof buf, 30);
    check(strlen(buf) < 80 && strstr(buf, "failed (https://") == buf && strstr(buf, ") -- why"),
          "shorten keeps the text around");
    lk_shorten("no address here", buf, sizeof buf, 10);
    check(!strcmp(buf, "no address here"), "shorten without address");
    lk_shorten("abc https://a.org/1234567890", buf, 8, 40);
    check(strlen(buf) == 7, "shorten respects the buffer");

    if (fails) { printf("test_linktext: %d FAILED\n", fails); return 1; }
    printf("test_linktext: all passed\n");
    return 0;
}
