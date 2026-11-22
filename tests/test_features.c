/* SPDX-License-Identifier: GPL-3.0-only */
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "NW.H"
#include "FEATURE.H"
#include "fs_faults.h"
static unsigned matches, errors;
static int decision, expected_errno;
static int visit(const char *path, const Entry *e, void *context)
{
    (void)context;
    assert(path[0]=='/' && !(e->attr&NW_DIR)); ++matches; return 1;
}
static int failure(const char *src, const char *dst, const char *message)
{
    assert(*src && *dst && *message);
    if (expected_errno) { assert(strstr(message,strerror(expected_errno))); expected_errno=0; }
    ++errors; return decision;
}
static void put(const char *path, const char *data)
{
    FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(data,1,strlen(data),f)==strlen(data)); assert(!fclose(f));
}
int main(void)
{
    char root[]="/tmp/nw-features-XXXXXX", path[NW_PATH], dst[NW_PATH], text[NW_PATH], pattern[100];
    unsigned cursor, directories, pages=0, seen=0, i;
    Panel *p=&panes[0];
    assert(mkdtemp(root)); strcpy(p->path,root);
    for (i=0;i<1100;++i) { sprintf(text,"F%04u.TXT",i); assert(path_join(path,root,text)); put(path,"data"); }
    assert(panel_load(p) && p->truncated);
    do {
        for (i=0;i<(unsigned)p->count;++i) if (strcmp(p->files[i].name,"..")) ++seen;
        ++pages;
        if (!p->truncated) break;
        assert(panel_page(p,1));
    } while (pages<5);
    assert(seen==1100 && pages==3 && p->offset==1022);
    assert(panel_page(p,0) && p->offset==511);
    assert(panel_page(p,0) && p->offset==0);
    assert(tree_find(root,"*.TXT",visit,NULL) && matches==1100);
    assert(path_join(path,root,"NEST")); assert(fs_mkdir(path));
    assert(path_join(dst,path,"DEEP.TXT")); put(dst,"nested");
    matches=0; assert(tree_find(root,"DEEP.*",visit,NULL) && matches==1);
    assert(panel_load(p)); p->marks=0; p->cursor=1;
    for (i=0;i<(unsigned)p->count;++i) if (!(p->files[i].attr&NW_DIR)) { p->cursor=i; break; }
    assert(panel_selected_bytes(p,&directories)==4 && !directories);
    panel_mark(p,p->cursor,1);
    assert(panel_selected_bytes(p,&directories)==4 && !directories);
    assert(p->free_kib!=0xffffffffUL && fs_test_space_calls==1);
    strcpy(panes[1].path,root);
    assert(panel_load(&panes[1]) && panes[1].free_kib==p->free_kib);
    assert(fs_test_space_calls==1); /* Shared volume, independent pane loads. */
    panel_space(p,1); panel_space(&panes[1],0);
    assert(fs_test_space_calls==2 && panes[1].free_kib==p->free_kib);
    assert(panel_load(p) && fs_test_space_calls==2);
    { /* A failed sample is cached once per drive, including letter case. */
        unsigned long sampled=p->free_kib;
        strcpy(panes[1].path,"Q:\\"); panel_space(&panes[1],0);
        assert(panes[1].free_kib==0xffffffffUL && fs_test_space_calls==3);
        strcpy(panes[1].path,"q:\\OTHER"); panel_space(&panes[1],0);
        assert(fs_test_space_calls==3);
        strcpy(panes[1].path,root); panel_space(&panes[1],0);
        assert(panes[1].free_kib==sampled && fs_test_space_calls==3);
    }

    strcpy(text,"ABCD"); cursor=2;
    assert(input_key(text,sizeof(text),&cursor,'x') && !strcmp(text,"ABxCD"));
    assert(input_key(text,sizeof(text),&cursor,8) && !strcmp(text,"ABCD") && cursor==2);
    assert(input_key(text,sizeof(text),&cursor,0x5300) && !strcmp(text,"ABD"));
    assert(input_key(text,sizeof(text),&cursor,0x4700) && !cursor);
    assert(input_key(text,sizeof(text),&cursor,'Z') && !strcmp(text,"ZABD"));
    assert(input_key(text,sizeof(text),&cursor,0x4f00) && cursor==4);
    input_history_reset(0,"draft"); strcpy(text,"first"); input_history(text,sizeof(text),&cursor,0,13);
    strcpy(text,"second"); input_history(text,sizeof(text),&cursor,0,13);
    strcpy(text,"draft"); input_history_reset(0,text);
    assert(input_history(text,sizeof(text),&cursor,0,0x4800) && !strcmp(text,"second"));
    assert(input_history(text,sizeof(text),&cursor,0,0x4800) && !strcmp(text,"first"));
    assert(input_history(text,sizeof(text),&cursor,0,0x5000) && !strcmp(text,"second"));
    assert(input_history(text,sizeof(text),&cursor,0,0x5000) && !strcmp(text,"draft"));
    strcpy(text,"NE"); cursor=2;
    assert(input_complete(text,sizeof(text),&cursor,root) && !strcmp(text,"NEST/"));
    strcpy(text,"NEST/DE"); cursor=strlen(text);
    assert(input_complete(text,sizeof(text),&cursor,root) && !strcmp(text,"NEST/DEEP.TXT"));
    strcpy(text,"F10"); cursor=3;
    assert(input_complete(text,sizeof(text),&cursor,root) && !strcmp(text,"F10"));
    for (i=0;i<FM_COUNT;++i) assert(*feature_label(pattern,i));
    { /* Long viewer expressions must not overwrite command history. */
        char expression[194], restored[194];
        memset(expression,'A',193); expression[193]=0;
        strcpy(text,"command"); input_history_reset(1,text);
        input_history(text,sizeof(text),&cursor,1,13);
        input_history_reset(0,expression);
        input_history(expression,sizeof(expression),&cursor,0,13);
        input_history_reset(0,"draft");
        assert(input_history(restored,sizeof(restored),&cursor,0,0x4800));
        assert(!strcmp(restored,expression));
        input_history_reset(1,"");
        assert(input_history(text,sizeof(text),&cursor,1,0x4800) && !strcmp(text,"command"));
    }
    input_history_free();

    assert(path_join(path,root,"F0000.TXT")); assert(path_join(dst,root,"RETRY.TXT"));
    { /* Fail after one complete block, then retry from a clean staged file. */
        char data[40000]; FILE *f;
        memset(data,'R',sizeof(data)); f=fopen(path,"wb"); assert(f);
        assert(fwrite(data,1,sizeof(data),f)==sizeof(data)); assert(!fclose(f));
    }
    operation_error=failure; decision=1; fs_test_fail("write","",2);
    assert(tree_copy(path,dst,0) && errors==1 && operation_files==1);
    assert(!fs_delete(dst,1)); assert(fs_delete(dst,0));
    expected_errno=ENOSPC; fs_test_fail("zero","",1);
    assert(tree_copy(path,dst,0) && errors==2 && operation_files==2);
    assert(fs_delete(dst,0));
    decision=2; fs_test_fail("rename_from","F0000.TXT",1); fs_test_fail("write","",1);
    assert(tree_copy(path,dst,1) && errors==3 && operation_skipped==1);
    { Entry e; assert(fs_info(path,&e) && !fs_info(dst,&e)); }
    decision=0; fs_test_fail("write","",1);
    assert(!tree_copy(path,dst,0) && errors==4);
    operation_error=NULL;
    assert(tree_delete(root));
    puts("PASS: 1100-entry paging/search, nested search, byte totals, editing/history/completion and retry/skip/cancel");
    return 0;
}
