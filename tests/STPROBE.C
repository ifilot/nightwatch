/* SPDX-License-Identifier: GPL-3.0-only */
/* Host/DOS entry point for the production engine; no alternative test plan. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "STORTEST.H"
#ifdef NW_HOST
#include "fs_faults.h"
extern int test_short_write;
#endif
#ifdef __TURBOC__
unsigned _stklen = 8192;
#endif
static int cancel_write, cancel_final, cancel_cleanup;
#ifdef NW_HOST
static int data_fault;
#endif
static int progress(const char *phase, const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)total;
#ifdef NW_HOST
    if (data_fault && !strcmp(phase, "WRITE") && strstr(path, "BIG.BIN") && !done) {
        if (data_fault == 2) test_short_write = 1;
        else fs_test_fail("write", "", 1);
        data_fault = 0;
    }
#endif
    if ((cancel_final && !strcmp(phase, "LIST")) ||
        (cancel_cleanup && !strcmp(phase, "FINAL CLEANUP"))) return 0;
    return !(cancel_write && !strcmp(phase, "WRITE") && done > 0);
}
int main(int argc, char **argv)
{
    char manifest[NW_PATH] = "";
    int ok = 0;
#ifdef NW_HOST
    const char *fault = getenv("NW_STORAGE_FAULT");
    if (fault) {
        if (!strcmp(fault, "create")) fs_test_fail("create", "BIG.BIN", 1);
        if (!strcmp(fault, "resize")) fs_test_fail("resize", "", 1);
        if (!strcmp(fault, "close")) fs_test_fail("close", "", 1);
        if (!strcmp(fault, "read")) fs_test_fail("read", "", 1);
        if (!strcmp(fault, "short")) test_short_write = 1;
        if (!strcmp(fault, "cancel")) cancel_write = 1;
        if (!strcmp(fault, "data-write")) data_fault = 1;
        if (!strcmp(fault, "shortdata")) data_fault = 2;
        if (!strcmp(fault, "cancel-final")) cancel_final = 1;
        if (!strcmp(fault, "cancel-cleanup")) cancel_cleanup = 1;
        if (!strcmp(fault, "root-delete")) fs_test_fail("delete", "NWT0001", 1);
    }
#endif
    if (argc == 3 && (!strcmp(argv[1], "plan") || !strcmp(argv[1], "mkdir"))) {
        int missing;
        ok = storage_directory(argv[2], manifest, !strcmp(argv[1], "mkdir"), &missing);
        printf("%s %s missing=%d %s\n", ok ? "PASS" : "FAIL", manifest, missing, nw_error);
        return ok ? 0 : 1;
    }
    storage_progress = progress;
    if (argc == 6 && !strcmp(argv[1], "start"))
        ok = storage_start(argv[2], argv[3], atoi(argv[4]), strtoul(argv[5], NULL, 10), manifest);
    else if (argc == 3 && !strcmp(argv[1], "verify")) ok = storage_verify(argv[2]);
    else if (argc == 3 && !strcmp(argv[1], "cleanup")) ok = storage_cleanup(argv[2]);
    else return 2;
    printf("MANIFEST=%s\n%s %s\n", manifest, ok ? "PASS" : "FAIL", ok ? "" : nw_error);
    printf("REPORT state=%u passed=%u stages=%u written=%lu verified=%lu files=%lu\n",
        storage_report.state,storage_report.passed,storage_report.stages,
        storage_report.written,storage_report.verified,storage_report.files);
    return ok ? 0 : 1;
}
