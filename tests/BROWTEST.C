/* SPDX-License-Identifier: GPL-3.0-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "BROWSE.H"
#ifdef NW_HOST
#include "fs_faults.h"
#endif
unsigned _stklen = 8192;
static int cancel_progress(const char *name, unsigned long done, unsigned long total)
{ (void)name; (void)total; return !done; }
static int choose(Panel *p, const char *name)
{
    int i;
    for (i = 0; i < p->count; ++i) if (!strcmp(p->files[i].name,name)) { p->cursor = i; return 1; }
    strcpy(nw_error,"Missing selected entry"); return 0;
}
int main(int argc, char **argv)
{
    Panel *p = &panes[0];
    char home[NW_PATH], *cut;
    unsigned long files = 0, bytes = 0;
    int i, ok;
    if (argc < 3) return 2;
    strcpy(home,argv[1]); cut = strrchr(home,SEP); if (!cut) return 2;
    if (cut == home || (cut == home+2 && home[1] == ':')) cut[1] = 0; else *cut = 0;
    if (!fs_canonical(home,p->path) || !panel_load(p)) return 2;
    ok = browse_open(p,argv[1]);
    for (i = 3; ok && i < argc; ++i) {
        if (!strcmp(argv[i],"cancel")) operation_progress = cancel_progress;
        else if (!strcmp(argv[i],"nested")) ok = browse_open(p,argv[1]);
#ifdef NW_HOST
        else if (!strncmp(argv[i],"fault=",6)) fs_test_fail(argv[i]+6,"",1);
#endif
        else if (!strcmp(argv[i],"refresh")) ok = browse_load(p);
        else if (!strcmp(argv[i],"copy")) {
            ok = browse_copy(p,argv[2],1,&files,&bytes);
            if (ok) ok = browse_copy(p,argv[2],0,&files,&bytes);
        } else if (!strcmp(argv[i],"view")) ok = browse_view(p,argv[2]);
        else if (!strcmp(argv[i],"home")) ok = browse_home(p,home);
        else if (!strcmp(argv[i],"second")) ok = browse_open(&panes[1],argv[1]);
        else if (!strncmp(argv[i],"select=",7)) ok = choose(p,argv[i]+7);
        else if (!strncmp(argv[i],"mark=",5)) panel_pattern(p,argv[i]+5,1);
        else if (!strcmp(argv[i],"reverse")) panel_sort(p,0,1);
        else ok = browse_enter(p,argv[i]);
    }
    printf("ok=%d active=%d count=%d truncated=%d marks=%d cursor=%d files=%lu bytes=%lu\n",
           ok,browse_active(p),p->count,p->truncated,p->marks,p->cursor,files,bytes);
    printf("PATH %s\nHOME %s\n",p->path,home);
    if (p->count && p->cursor >= 0 && p->cursor < p->count) printf("SELECTED %s\n",p->files[p->cursor].name);
    for (i = 0; i < p->count; ++i) printf("ENTRY %s %u %lu\n",p->files[i].name,p->files[i].attr,p->files[i].size);
    if (!ok) puts(nw_error);
    browse_release(p); browse_release(&panes[1]);
    return ok ? 0 : 1;
}
