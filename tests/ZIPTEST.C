/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared host/DOS extraction driver; test failures must return nonzero. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ZIP.H"
#ifdef NW_HOST
#include "fs_faults.h"
extern int test_short_write;
#endif
unsigned _stklen = 8192;
static int cancel, conflict = 1;
static unsigned long polls;
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
