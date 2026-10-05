/* SPDX-License-Identifier: GPL-3.0-only */
/* In-memory pane fixtures verify all sort orders, selected-name retention,
 * independent snapshots/marks and wildcard wrap without a directory scan.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "NW.H"
static int index_of(Panel *p, const char *name)
{
    int i;
    for (i=0;i<p->count;++i) if (!strcmp(p->files[i].name,name)) return i;
    assert(0); return 0;
}
static void fixture(Panel *p)
{
    static const char *names[] = {"..","ZZZDIR","BETA.BIN","DELTA","AAADIR","GAMMA.TXT","ALPHA.TXT"};
    int i;
    memset(p,0,sizeof(*p)); p->count=7;
    for (i=0;i<7;++i) strcpy(p->files[i].name,names[i]);
    p->files[0].attr=p->files[1].attr=p->files[4].attr=NW_DIR;
    p->files[2].size=50; p->files[2].date=11; p->files[2].time=2;
    p->files[3].size=500; p->files[3].date=10;
    p->files[5].size=10; p->files[5].date=13; p->files[5].time=5;
    p->files[6].size=50; p->files[6].date=11; p->files[6].time=1;
    p->cursor=2; panel_mark(p,2,1); panel_mark(p,6,1);
}
static void ordering(Panel *p, const char **expected, int reverse)
{
    int i;
    assert(!strcmp(p->files[0].name,".."));
    assert(p->files[1].attr & NW_DIR); assert(p->files[2].attr & NW_DIR);
    for (i=0;i<4;++i) assert(!strcmp(p->files[i+3].name,expected[reverse ? 3-i : i]));
    assert(!strcmp(p->files[p->cursor].name,"BETA.BIN"));
    assert(p->marks==2 && p->files[index_of(p,"ALPHA.TXT")].marked &&
           p->files[index_of(p,"BETA.BIN")].marked);
}
int main(void)
{
    static const char *name[] = {"ALPHA.TXT","BETA.BIN","DELTA","GAMMA.TXT"};
    static const char *size_order[] = {"GAMMA.TXT","ALPHA.TXT","BETA.BIN","DELTA"};
    static const char *date_order[] = {"DELTA","ALPHA.TXT","BETA.BIN","GAMMA.TXT"};
    static const char *extension[] = {"DELTA","BETA.BIN","ALPHA.TXT","GAMMA.TXT"};
    int mode, reverse, before;
    unsigned revision;
    for (mode=0;mode<4;++mode) for (reverse=0;reverse<2;++reverse) {
        fixture(&panes[0]); revision=panes[0].revision;
        panel_sort(&panes[0],mode,reverse);
        ordering(&panes[0],mode==0 ? name : mode==1 ? size_order : mode==2 ? date_order : extension,reverse);
        assert(panes[0].revision!=revision && panes[0].top==0);
    }
    fixture(&panes[0]); panel_sort(&panes[0],1,0);
    fixture(&panes[1]); panel_sort(&panes[1],3,1);
    panes[1].cursor=index_of(&panes[1],"ALPHA.TXT");
    panel_snapshot(&panes[1],&panes[0]);
    assert(panes[1].sort==3 && panes[1].reverse && panes[1].marks==0 && panes[0].marks==2);
    assert(!strcmp(panes[1].files[panes[1].cursor].name,"ALPHA.TXT"));
    assert(!strcmp(panes[1].files[3].name,"GAMMA.TXT"));
    assert(!strcmp(panes[0].files[3].name,"GAMMA.TXT"));
    panel_pattern(&panes[1],"*.txt",1); assert(panes[1].marks==2);
    panel_pattern(&panes[1],"A*.???",0); assert(panes[1].marks==1);
    assert(panel_find(&panes[1],"B?TA.*"));
    assert(!strcmp(panes[1].files[panes[1].cursor].name,"BETA.BIN"));
    before=panes[1].cursor; assert(!panel_find(&panes[1],"NOPE*")); assert(panes[1].cursor==before);
    assert(panel_find(&panes[1],"beta.*") && panes[1].cursor==before);
    panel_mark_all(&panes[1],1); assert(panes[1].marks==6 && !panes[1].files[0].marked);
    memset(&panes[1],0,sizeof(panes[1])); assert(!panel_find(&panes[1],"*"));
    puts("PASS: all sort orders/reversal/ties, independent snapshots, selection/marks, wildcard wrap and empty panes");
    return 0;
}
