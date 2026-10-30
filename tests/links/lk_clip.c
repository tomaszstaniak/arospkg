/* Prints the text on clipboard unit 0 between [ and ], with its length, so
 * a test can compare it byte for byte with the address that was copied. */
#include <proto/exec.h>
#include <proto/iffparse.h>
#include <libraries/iffparse.h>
#include <stdio.h>

struct Library *IFFParseBase;

int main(void)
{
    struct IFFHandle *iff;
    static char buf[4096];
    LONG n = -1;
    if (!(IFFParseBase = OpenLibrary((CONST_STRPTR)"iffparse.library", 39))) return 20;
    if ((iff = AllocIFF())) {
        if ((iff->iff_Stream = (IPTR)OpenClipboard(0))) {
            InitIFFasClip(iff);
            if (!OpenIFF(iff, IFFF_READ)) {
                if (!StopChunk(iff, MAKE_ID('F','T','X','T'), MAKE_ID('C','H','R','S'))
                    && !ParseIFF(iff, IFFPARSE_SCAN))
                    n = ReadChunkBytes(iff, buf, sizeof buf - 1);
                CloseIFF(iff);
            }
            CloseClipboard((struct ClipboardHandle *)iff->iff_Stream);
        }
        FreeIFF(iff);
    }
    CloseLibrary(IFFParseBase);
    if (n < 0) { printf("clip: no text\n"); fflush(stdout); return 5; }
    buf[n] = 0;
    printf("clip %ld [%s]\n", (long)n, buf);
    fflush(stdout);
    return 0;
}
