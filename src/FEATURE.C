/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include <ctype.h>
#include "FEATURE.H"
#ifdef __TURBOC__
#include <alloc.h>
#include <dos.h>
#else
#include <stdlib.h>
#endif
char *feature_label(char *out, unsigned index)
{
    const char NW_FAR *source;
    char *at=out;
#ifdef NW_HOST
    switch (index) {
#define FEATURE_CASE(symbol,value) case symbol: source=value; break;
        FEATURE_MESSAGES(FEATURE_CASE)
#undef FEATURE_CASE
        default: out[0]=0; return out;
    }
#else
    source=asset_feature_label(index);
#endif
    while ((*at++=*source++)!=0) {}
    return out;
}

int input_key(char *text, unsigned max, unsigned *cursor, int key)
{
    unsigned n=strlen(text), at=*cursor;
    if (at>n) at=n;
    if (key==0x7300) { while (at && text[at-1]==' ') --at; while (at && text[at-1]!=' ') --at; }
    else if (key==0x7400) { while (at<n && text[at]!=' ') ++at; while (at<n && text[at]==' ') ++at; }
    else if (key==0x4b00 && at) --at;
    else if (key==0x4d00 && at<n) ++at;
    else if (key==0x4700) at=0;
    else if (key==0x4f00) at=n;
    else if (key==21) { text[0]=0; at=0; }
    else if (key==8 && at) { memmove(text+at-1,text+at,n-at+1); --at; }
    else if (key==0x5300 && at<n) memmove(text+at,text+at+1,n-at);
    else if (key>=32 && key<127 && n+1<max) {
        memmove(text+at+1,text+at,n-at+1); text[at++]=(char)key;
    } else { *cursor=at; return 0; }
    *cursor=at; return 1;
}

/* Session-only histories stay outside DOS's near-data segment. */
/* The viewer accepts a 193-character hexadecimal search expression. */
#define HISTORY_BYTES 194
typedef char History[5][HISTORY_BYTES];
static History NW_FAR *history;
static void NW_FAR *history_memory;
static unsigned counts[2], positions[2];
static void store(char NW_FAR *out, const char *in)
{
    unsigned n=0;
    while (n+1<HISTORY_BYTES && in[n]) { out[n]=in[n]; ++n; }
    out[n]=0;
}
static void fetch(char *out, const char NW_FAR *in, unsigned max)
{
    unsigned n=0;
    while (n+1<max && in[n]) { out[n]=in[n]; ++n; }
    out[n]=0;
}
void input_history_reset(unsigned channel, const char *draft)
{
    if (!history) {
#ifdef __TURBOC__
        history_memory=farmalloc(2UL*sizeof(History)+15UL);
        if (history_memory) history=MK_FP(FP_SEG(history_memory)+(FP_OFF(history_memory)>>4),FP_OFF(history_memory)&15);
#else
        history_memory=malloc(2*sizeof(History)); history=history_memory;
#endif
    }
    if (!history) return;
    positions[channel]=0; store(history[channel][4],draft);
}
int input_history(char *text, unsigned max, unsigned *cursor, unsigned channel, int key)
{
    unsigned i, n=counts[channel];
    if (!history) return 0;
    if (key==13) {
        if (!*text) return 0;
        for (i=n<4 ? n : 3;i>0;--i) {
            unsigned j=0;
            do { history[channel][i][j]=history[channel][i-1][j]; }
            while (history[channel][i-1][j++]);
        }
        store(history[channel][0],text); if (n<4) ++counts[channel];
        return 0;
    }
    if (key==0x4800) { if (positions[channel]<n) ++positions[channel]; }
    else if (key==0x5000) { if (positions[channel]) --positions[channel]; }
    else return 0;
    fetch(text,history[channel][positions[channel] ? positions[channel]-1 : 4],max);
    *cursor=strlen(text); return 1;
}

int input_complete(char *text, unsigned max, unsigned *cursor, const char *directory)
{
    char token[NW_PATH], parent[NW_PATH], pattern[16], common[14];
    FsSearch search;
    Entry e;
    unsigned start=*cursor, split, n, length=0, matches=0;
    int result, directory_match=0;
    if (text[*cursor] && text[*cursor]!=' ' && text[*cursor]!='\t') return 0;
    while (start && text[start-1]!=' ' && text[start-1]!='\t') --start;
    n=*cursor-start;
    if (n>=sizeof(token)) return 0;
    memcpy(token,text+start,n); token[n]=0;
    split=n;
    while (split && token[split-1]!='\\' && token[split-1]!='/' && token[split-1]!=':') --split;
    if (n-split>12) return 0;
    strcpy(pattern,token+split); strcat(pattern,"*");
    if (split) {
        token[split]=0;
        if (token[1]==':') strcpy(parent,token);
        else if (token[0]=='\\' || token[0]=='/') {
            if (directory[1]==':') { parent[0]=directory[0]; parent[1]=':'; strcpy(parent+2,token); }
            else strcpy(parent,token);
        } else if (!path_join(parent,directory,token)) return 0;
    }
    else strcpy(parent,directory);
    result=fs_first(&search,parent,&e);
    while (result>0) {
        if (name_match(pattern,e.name)) {
            unsigned i;
            if (!matches++) { strcpy(common,e.name); length=strlen(common); directory_match=(e.attr&NW_DIR)!=0; }
            else {
                for (i=0;i<length && toupper((unsigned char)common[i])==toupper((unsigned char)e.name[i]);++i) {}
                length=i; common[length]=0;
            }
        }
        result=fs_next(&search,&e);
    }
    fs_end(&search);
    if (result<0 || !matches || length<n-split) return 0;
    if (matches==1 && directory_match) {
#ifdef NW_HOST
        common[length++]='/';
#else
        common[length++]='\\';
#endif
        common[length]=0;
    }
    n=start+split+length;
    if (n+strlen(text+*cursor)>=max) return 0;
    memmove(text+n,text+*cursor,strlen(text+*cursor)+1);
    memcpy(text+start+split,common,length); *cursor=n;
    return 1;
}

void input_history_free(void)
{
#ifdef __TURBOC__
    if (history_memory) farfree(history_memory);
#else
    free(history_memory);
#endif
    history_memory=NULL; history=NULL;
    counts[0]=counts[1]=positions[0]=positions[1]=0;
}
