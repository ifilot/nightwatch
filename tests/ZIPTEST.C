/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared host/DOS extraction driver; test failures must return nonzero. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef NW_ARJ_TEST
#include "ARJ.H"
#include "ARJDEC.H"
#define zip_measure arj_measure
#define zip_extract arj_extract
#define zip_fail_allocation arj_fail_allocation
#define zip_live_allocations arj_live_allocations
#define NW_ZIP_TEST
#else
#include "ZIP.H"
#endif
#ifdef NW_HOST
#include "fs_faults.h"
extern int test_short_write;
#endif
#ifndef NW_ARJ_TEST
#include <assert.h>
#include "zutil.h"
#include "inftrees.h"
#include "inflate.h"
#ifndef NW_HOST
#include <alloc.h>
#endif
static voidpf prime_alloc(voidpf opaque, uInt items, uInt size)
{
    (void)opaque;
#ifdef NW_HOST
    return malloc((size_t)items * size);
#else
    return farmalloc((unsigned long)items * size);
#endif
}
static void prime_free(voidpf opaque, voidpf memory)
{
    (void)opaque;
#ifdef NW_HOST
    free(memory);
#else
    farfree(memory);
#endif
}
/* Exercise the 16-bit conversion boundaries and relocation of copied tables
 * using the same vendored inflater on both the host and real 8086 tests. */
static void check_inflater(void)
{
    z_stream stream, copy;
    struct inflate_state FAR *state, FAR *copied;
    memset(&stream,0,sizeof(stream)); memset(&copy,0,sizeof(copy));
    stream.zalloc=prime_alloc; stream.zfree=prime_free;
    assert(inflateInit2(&stream,-15)==Z_OK);
    state=(struct inflate_state FAR *)stream.state;
    assert(inflatePrime(&stream,16,-1)==Z_OK);
    assert(state->hold==65535UL && state->bits==16);
    assert(inflatePrime(&stream,16,0x1234)==Z_OK);
    assert(state->hold==0x1234ffffUL && state->bits==32);
    assert(inflatePrime(&stream,1,1)==Z_STREAM_ERROR);
    assert(inflatePrime(&stream,-1,0)==Z_OK && !state->hold && !state->bits);
    state->lencode=state->codes+511;
    state->distcode=state->codes+1000;
    state->next=state->codes+1440;
    assert(inflateCopy(&copy,&stream)==Z_OK);
    copied=(struct inflate_state FAR *)copy.state;
    assert(copied->lencode==copied->codes+511);
    assert(copied->distcode==copied->codes+1000);
    assert(copied->next==copied->codes+1440);
    assert(inflateEnd(&copy)==Z_OK && inflateEnd(&stream)==Z_OK);
}
#endif
unsigned _stklen = 8192;
static int cancel, conflict = 1;
static unsigned long polls;
#ifdef NW_HOST
static int retry_decision;
static int recover(const char *src, const char *dst, const char *message)
{ (void)src; (void)dst; (void)message; return retry_decision; }
#endif
static int progress(const char *name, unsigned long done, unsigned long total)
{
    (void)name;
    ++polls;
    if (done > total) abort();
    return !cancel || !done || (cancel == 2 && !operation_files);
}
static int resolve(const char *src, const char *dst)
{ (void)src; (void)dst; return conflict; }
int main(int argc, char **argv)
{
    unsigned long files = 0, bytes = 0;
    int ok;
    if (argc < 3) return 2;
#ifndef NW_ARJ_TEST
    check_inflater();
#endif
    if (argc > 3) {
        if (!strcmp(argv[3], "skip")) conflict = 2;
        if (!strcmp(argv[3], "refuse")) conflict = 0;
        if (!strcmp(argv[3], "cancel")) cancel = 1;
        if (!strcmp(argv[3], "cancel2")) cancel = 2;
#ifdef NW_ZIP_TEST
        if (!strcmp(argv[3], "alloc1")) zip_fail_allocation = 1;
        if (!strcmp(argv[3], "alloc2")) zip_fail_allocation = 2;
#endif
    }
    operation_progress = progress; operation_conflict = resolve;
    ok = zip_measure(argv[1], argv[2], &files, &bytes);
#ifdef NW_HOST
    if (argc > 3) {
        if (!strcmp(argv[3], "retry-write") || !strcmp(argv[3], "skip-write")) {
            retry_decision=!strcmp(argv[3],"retry-write") ? 1 : 2;
            operation_error=recover; fs_test_fail("write","",2);
        }
        if (!strcmp(argv[3], "short")) test_short_write = 7;
        if (!strcmp(argv[3], "write")) fs_test_fail("write", "", 1);
        if (!strcmp(argv[3], "read")) fs_test_fail("read", "", 1);
        if (!strcmp(argv[3], "seek")) fs_test_fail("seek", "", 1);
        if (!strcmp(argv[3], "stamp")) fs_test_fail("stamp", "", 1);
        if (!strcmp(argv[3], "close")) fs_test_fail("close", "", 1);
        if (!strcmp(argv[3], "attr")) fs_test_fail("attr", ".TMP", 1);
        if (!strcmp(argv[3], "commit")) fs_test_fail("rename_to", "HELLO.TXT", 1);
        if (!strcmp(argv[3], "restore")) {
            fs_test_fail("rename_to", "HELLO.TXT", 1);
            fs_test_fail("rename_to", "HELLO.TXT", 1);
        }
        if (!strcmp(argv[3], "backup-delete")) fs_test_fail("delete", ".BAK", 1);
        if (!strcmp(argv[3], "close-source")) fs_test_fail("close", "", 2);
        if (!strcmp(argv[3], "cleanup")) {
            fs_test_fail("write", "", 1); fs_test_fail("delete", ".TMP", 1);
        }
    }
#endif
    if (ok) ok = zip_extract(argv[1], argv[2]);
#ifdef NW_ZIP_TEST
    if (zip_live_allocations) abort();
#endif
    printf("ok=%d files=%lu bytes=%lu completed=%lu skipped=%lu resolved=%lu transferred=%lu polls=%lu\n",
           ok, files, bytes, operation_files, operation_skipped, operation_bytes, operation_transferred, polls);
    if (!ok) puts(nw_error);
    return ok ? 0 : 1;
}
