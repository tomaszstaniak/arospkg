/* Test setup for OpenURL, for the running system only (ENV:, never ENVARC:;
 * gone after a reboot):
 *   lk_openurl set "RAM:lk/lk_browser %u"   the first browser starts this, the rest are off
 *   lk_openurl none                         every browser off: URL_Open has nobody to give it to
 *   lk_openurl slow COMMAND PORT            started without the address, then OpenURL waits
 *                                           for PORT to appear (it never does: a slow failure)
 *   lk_openurl show                         the browsers as they are now */
#include <proto/exec.h>
#include <proto/dos.h>
#include <libraries/openurl.h>
#include <proto/openurl.h>
#include <stdio.h>
#include <string.h>

struct Library *OpenURLBase;

int main(int argc, char **argv)
{
    struct URL_Prefs *p;
    struct URL_BrowserNode *b;
    struct TagItem get[] = {{URL_GetPrefs_Mode, URL_GetPrefs_Mode_InUse}, {TAG_DONE, 0}};
    struct TagItem set[] = {{URL_SetPrefs_Save, FALSE}, {TAG_DONE, 0}};
    const char *mode = argc > 1 ? argv[1] : "";
    int first = 1, ok = 1;
    if (!(OpenURLBase = OpenLibrary((CONST_STRPTR)"openurl.library", 7))) {
        printf("openurl.library 7+: not available\n"); fflush(stdout); return 20;
    }
    p = URL_GetPrefsA(get);
    if (!p || p->up_Version != PREFS_VERSION) { printf("prefs: not available\n"); CloseLibrary(OpenURLBase); return 20; }
    for (b = (struct URL_BrowserNode *)p->up_BrowserList.mlh_Head; b->ubn_Node.mln_Succ;
         b = (struct URL_BrowserNode *)b->ubn_Node.mln_Succ) {
        if (!strcmp(mode, "show")) {
            printf("browser %s path=%s port=%s %s\n", b->ubn_Name, b->ubn_Path, b->ubn_Port,
                   (b->ubn_Flags & UNF_DISABLED) ? "off" : "on");
            continue;
        }
        if (first && !strcmp(mode, "set") && argc == 3 && strlen(argv[2]) < PATH_LEN) {
            strcpy(b->ubn_Name, "lk_browser"); strcpy(b->ubn_Path, argv[2]); b->ubn_Port[0] = 0;
            b->ubn_Flags &= ~UNF_DISABLED; first = 0;
        } else if (first && !strcmp(mode, "slow") && argc == 4) {
            strcpy(b->ubn_Name, "lk_slow"); strcpy(b->ubn_Path, argv[2]);
            snprintf((char *)b->ubn_Port, sizeof b->ubn_Port, "%s", argv[3]);
            strcpy(b->ubn_OpenURLCmd, "OPENURL %u"); strcpy(b->ubn_OpenURLWCmd, "OPENURL %u");
            b->ubn_Flags &= ~UNF_DISABLED; first = 0;
        } else b->ubn_Flags |= UNF_DISABLED;
    }
    if (strcmp(mode, "show")) {
        p->up_DefLaunch = TRUE;
        ok = (!strcmp(mode, "none") || !first) && URL_SetPrefsA(p, set);
    }
    printf("openurl %u.%u %s %s\n", (unsigned)OpenURLBase->lib_Version, (unsigned)OpenURLBase->lib_Revision,
           mode, ok ? "ok" : "FAILED");
    fflush(stdout);
    URL_FreePrefsA(p, NULL);
    CloseLibrary(OpenURLBase);
    return ok ? 0 : 20;
}
