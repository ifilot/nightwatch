/* SPDX-License-Identifier: GPL-3.0-only */
/* Optional isolated zlib feasibility probe; not linked into NW.EXE.
 * Raw .DFL inputs and scratch .OUT outputs live only in build/zlib-probe.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <alloc.h>
#include <dos.h>
#include "zlib.h"
unsigned _stklen=8192;
static unsigned char input[1024], output[1024];
static unsigned long live,peak;
static int allocation_calls, fail_allocation;
typedef struct { void far *allocation; unsigned long size; } Header;
static voidpf allocate(voidpf opaque, unsigned items, unsigned size)
{
    unsigned long bytes=(unsigned long)items*size;
    void far *original;
    Header far *h;
    unsigned offset;
    (void)opaque;
    if(fail_allocation && ++allocation_calls==2) return NULL;
    original=farmalloc(bytes+sizeof(Header)+15UL);
    if(!original) return NULL;
    offset=FP_OFF(original);
    h=MK_FP(FP_SEG(original)+(offset>>4),offset&15);
    h->allocation=original; h->size=bytes;
    live+=bytes; if(live>peak) peak=live;
    return (char far *)h+sizeof(Header);
}
static void release(voidpf opaque, voidpf address)
{
    Header far *h=(Header far *)((char far *)address-sizeof(Header));
    void far *original;
    (void)opaque;
    if(!address)return;
    original=h->allocation; live-=h->size; farfree(original);
}
int main(int argc,char **argv)
{
    z_stream stream;
    char name[20];
    FILE *in,*out;
    int status,eof=0;
    unsigned produced;
    unsigned long crc=0,total;
    if(argc<2||strlen(argv[1])>8)return 1;
    fail_allocation=argc>2;
    sprintf(name,"%s.DFL",argv[1]); in=fopen(name,"rb");
    sprintf(name,"%s.OUT",argv[1]); out=fopen(name,"wb");
    if(!in||!out)return 1;
    memset(&stream,0,sizeof(stream)); stream.zalloc=allocate; stream.zfree=release;
    status=inflateInit2(&stream,-15);
    if(status!=Z_OK)return 1;
    for(;;){
        if(!stream.avail_in&&!eof){
            stream.avail_in=fread(input,1,sizeof(input),in); stream.next_in=input;
            if(!stream.avail_in)eof=1;
        }
        stream.next_out=output; stream.avail_out=sizeof(output);
        status=inflate(&stream,Z_NO_FLUSH);
        produced=sizeof(output)-stream.avail_out;
        if(fwrite(output,1,produced,out)!=produced||ferror(in))return 1;
        crc=crc32(crc,output,produced);
        if(status==Z_STREAM_END||(status!=Z_OK&&status!=Z_BUF_ERROR))break;
        if(eof&&!stream.avail_in&&!produced){status=Z_BUF_ERROR;break;}
    }
    total=stream.total_out;
    if(inflateEnd(&stream)!=Z_OK||live||fclose(in)||fclose(out))return 1;
    printf("status=%d bytes=%lu crc=%08lX far_peak=%lu int=%u pointer=%u\n",status,total,crc,peak,sizeof(int),sizeof(void *));
    return 0;
}
