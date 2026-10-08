/* SPDX-License-Identifier: GPL-3.0-only */
/* Inject failures at filesystem boundaries, then inspect exact source,
 * destination and recovery bytes. Tests distinguish rollback attempts from
 * retained backups, earlier committed work and cancellation cleanup.
 */
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "NW.H"
#include "fs_faults.h"
static const char original[] = "original destination, never the replacement";
static unsigned char replacement[9001];
static void put(const char *path, const void *data, unsigned length)
{
    int fd = fs_create(path);
    assert(fd >= 0); assert(fs_write(fd, data, length) == (int)length); assert(!fs_close(fd));
}
static void exact(const char *path, const void *data, unsigned length)
{
    unsigned char bytes[10000];
    int fd = fs_read_open(path);
    assert(fd >= 0); assert(fs_read(fd, bytes, sizeof(bytes)) == (int)length);
    assert(!memcmp(bytes, data, length)); assert(!fs_close(fd));
}
static int overwrite(const char *a, const char *b) { (void)a; (void)b; return 1; }
static int skip_one(const char *a, const char *b)
{
    (void)b; return strstr(a, "KEEP.TXT") ? 2 : 1;
}
static int cancel(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)total; return done < 4096;
}
static int cancel_after_member(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)done; (void)total; return operation_files < 1;
}
static int cancel_scan(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)done; (void)total; return 0;
}
static int exists(const char *path) { Entry e; return fs_info(path, &e); }
static void no_artifacts(const char *root)
{
    DIR *dir = opendir(root);
    struct dirent *e;
    assert(dir);
    assert(!fs_test_copy_allocations);
    while ((e = readdir(dir)) != NULL)
        assert(!strstr(e->d_name,".TMP") && !strstr(e->d_name,".NEW") && !strstr(e->d_name,".BAK"));
    closedir(dir);
}
static void original_metadata(const char *path, const Entry *before)
{
    Entry after;
    exact(path, original, sizeof(original));
    assert(fs_info(path, &after));
    assert(after.attr == before->attr && after.date == before->date && after.time == before->time);
}
int main(void)
{
    char root[] = "/tmp/nw-operations-XXXXXX";
    char src[NW_PATH], dst[NW_PATH], backup[NW_PATH], staged[NW_PATH], tmp[NW_PATH];
    char a[NW_PATH], b[NW_PATH], path[NW_PATH], other[NW_PATH], nested[NW_PATH];
    char too_long[NW_PATH+40];
    const char *boundaries[] = {"create", "read", "write", "close", "stamp", "attr", "rename_to", "rename_from"};
    const char *suffixes[] = {"", "", "", "", "", ".TMP", ".BAK", ".NEW"};
    Entry old;
    unsigned i, allocation;
    unsigned long files, bytes;
    assert(mkdtemp(root));
    assert(path_join(src,root,"SOURCE.BIN")); assert(path_join(dst,root,"TARGET.BIN"));
    assert(path_join(backup,root,"NW0000.BAK")); assert(path_join(staged,root,"NW0000.NEW"));
    assert(path_join(tmp,root,"NW0000.TMP"));
    for (i = 0; i < sizeof(replacement); ++i) replacement[i] = (unsigned char)(i * 17 + 5);
    put(src,replacement,sizeof(replacement)); put(dst,original,sizeof(original));
    assert(fs_attr(dst,NW_READONLY)); assert(fs_info(dst,&old));
    operation_conflict = overwrite;
    for (allocation = 0; allocation < 2; ++allocation) {
        fs_test_copy_alloc_fail = allocation;
        for (i = 0; i < sizeof(boundaries)/sizeof(boundaries[0]); ++i) {
            fs_test_fail(boundaries[i],suffixes[i],1);
            assert(!tree_copy(src,dst,1)); fs_test_reset();
            original_metadata(dst,&old); exact(src,replacement,sizeof(replacement)); no_artifacts(root);
        }
    }
    fs_test_copy_alloc_fail = 0;
    operation_progress = cancel;
    assert(!tree_copy(src,dst,1)); operation_progress = NULL;
    original_metadata(dst,&old); exact(src,replacement,sizeof(replacement)); no_artifacts(root);
    /* If rollback fails, preserve and report the original backup, never the source. */
    fs_test_fail("rename_from",".NEW",1); fs_test_fail("rename_from",".BAK",1);
    assert(!tree_copy(src,dst,1)); fs_test_reset();
    assert(!exists(dst) && strstr(nw_error,backup)); original_metadata(backup,&old);
    exact(src,replacement,sizeof(replacement)); assert(!exists(staged)); assert(fs_rename(backup,dst));
    /* Commit and rollback plus cleanup failure: report both surviving artifacts. */
    fs_test_fail("rename_from",".NEW",1); fs_test_fail("rename_from",".BAK",1);
    fs_test_fail("delete",".NEW",1);
    assert(!tree_copy(src,dst,1)); fs_test_reset();
    assert(strstr(nw_error,backup) && strstr(nw_error,"NW0000.NEW"));
    original_metadata(backup,&old); exact(staged,replacement,sizeof(replacement));
    assert(fs_delete(staged,0) && fs_rename(backup,dst));
    /* A failed backup deletion leaves a complete replacement and original metadata. */
    fs_test_fail("delete",".BAK",1);
    assert(!tree_copy(src,dst,1)); fs_test_reset();
    exact(dst,replacement,sizeof(replacement)); exact(src,replacement,sizeof(replacement));
    original_metadata(backup,&old); assert(strstr(nw_error,backup));
    assert(fs_delete(dst,0) && fs_rename(backup,dst));
    /* Even failed attribute restoration must retain and identify the backup. */
    fs_test_fail("delete",".BAK",1); fs_test_fail("attr",".BAK",2);
    assert(!tree_copy(src,dst,1)); fs_test_reset();
    exact(dst,replacement,sizeof(replacement)); exact(backup,original,sizeof(original));
    exact(src,replacement,sizeof(replacement)); assert(strstr(nw_error,backup));
    assert(fs_info(backup,&old) && !(old.attr & NW_READONLY));
    assert(fs_attr(backup,NW_READONLY)); assert(fs_info(backup,&old));
    assert(fs_delete(dst,0) && fs_rename(backup,dst));
    /* Failed temporary deletion must be reported, with the original untouched. */
    fs_test_fail("write","",1); fs_test_fail("delete",".TMP",1);
    assert(!tree_copy(src,dst,1)); fs_test_reset();
    original_metadata(dst,&old); assert(exists(tmp) && strstr(nw_error,tmp)); assert(fs_delete(tmp,0));
    assert(tree_copy(src,dst,0)); exact(dst,replacement,sizeof(replacement)); assert(!exists(backup));
    /* A final destination must never be used as its own temporary file. */
    assert(file_copy(src,tmp)); exact(tmp,replacement,sizeof(replacement));
    assert(path_join(other,root,"NW0001.TMP"));
    assert(file_copy(src,other)); exact(other,replacement,sizeof(replacement));
    assert(fs_delete(tmp,0) && fs_delete(other,0));
    /* Staging cleanup can fail before the original is backed up. */
    fs_test_fail("rename_to",".BAK",1); fs_test_fail("delete",".NEW",1);
    assert(!tree_copy(src,dst,0)); fs_test_reset();
    exact(dst,replacement,sizeof(replacement)); exact(staged,replacement,sizeof(replacement));
    assert(strstr(nw_error,staged) && !exists(backup)); assert(fs_delete(staged,0));
    /* Ancestor/descendant overlap: refuse without mutating either tree. */
    assert(path_join(a,root,"A")); assert(fs_mkdir(a));
    assert(path_join(b,a,"A")); assert(fs_mkdir(b));
    assert(path_join(nested,b,"A")); assert(fs_mkdir(nested));
    assert(path_join(path,nested,"DATA.BIN")); put(path,original,sizeof(original));
    assert(path_join(other,root,"ENUM"));
    fs_test_fail("enumerate","",1);
    assert(!tree_copy(a,other,0)); fs_test_reset(); assert(strstr(nw_error,"Read directory"));
    assert(tree_delete(other));
    fs_test_fail("enumerate","",1);
    assert(!tree_delete(a)); fs_test_reset(); assert(strstr(nw_error,"Read directory"));
    exact(path,original,sizeof(original));
    assert(!tree_copy(b,a,0)); assert(!tree_copy(b,a,1)); exact(path,original,sizeof(original));
    assert(path_join(other,b,"DATA.BIN")); assert(!exists(other));
    assert(!tree_copy(a,b,0)); assert(tree_delete(a));
    /* Recursive totals include empty files, ignore directories, and exceed the
     * pane cache. Counting errors/cancellation never create a destination. */
    assert(path_join(a,root,"COUNT")); assert(fs_mkdir(a));
    assert(path_join(nested,a,"NEST")); assert(fs_mkdir(nested));
    for (i = 0; i < 520; ++i) {
        char name[13];
        sprintf(name, "F%03u.TXT", i);
        assert(path_join(path,nested,name)); put(path,replacement,i % 2);
    }
    assert(path_join(path,a,"DATA.BIN")); put(path,replacement,9001);
    files = bytes = 0;
    assert(tree_measure(a,&files,&bytes)); assert(files == 521 && bytes == 9261);
    assert(tree_measure(src,&files,&bytes)); assert(files == 522 && bytes == 18262);
    operation_progress = cancel_scan; files = bytes = 0;
    assert(!tree_measure(a,&files,&bytes)); operation_progress = NULL;
    assert(strstr(nw_error,"cancelled")); exact(path,replacement,9001);
    fs_test_fail("enumerate","",1); files = bytes = 0;
    assert(!tree_measure(a,&files,&bytes)); fs_test_reset();
    assert(strstr(nw_error,"Read directory"));
    files = 0; bytes = 0xffffffffUL;
    assert(!tree_measure(src,&files,&bytes)); assert(strstr(nw_error,"32-bit"));
    assert(tree_delete(a));
    /* Cross-drive merge move: one skipped child stays; others commit then disappear. */
    assert(path_join(a,root,"SRC")); assert(path_join(b,root,"DST")); assert(fs_mkdir(a)&&fs_mkdir(b));
    assert(path_join(path,a,"KEEP.TXT")); put(path,replacement,10);
    assert(path_join(other,b,"KEEP.TXT")); put(other,original,sizeof(original));
    assert(path_join(path,a,"MOVE.TXT")); put(path,replacement,20);
    operation_conflict = skip_one; operation_files = operation_skipped = 0; fs_test_cross_drive = 1;
    operation_bytes = operation_current_bytes = operation_transferred = 0;
    assert(tree_copy(a,b,1)); fs_test_reset();
    assert(operation_skipped == 1 && operation_files == 1);
    assert(operation_bytes == 30 && operation_transferred == 20 && operation_current_bytes == 0);
    assert(path_join(path,a,"KEEP.TXT")); exact(path,replacement,10); exact(other,original,sizeof(original));
    assert(path_join(path,a,"MOVE.TXT")); assert(!exists(path));
    assert(path_join(path,b,"MOVE.TXT")); exact(path,replacement,20);
    operation_conflict = overwrite;
    assert(tree_delete(a)&&tree_delete(b));
    /* Nested cancellation keeps earlier commits and cleans the current file. */
    assert(fs_mkdir(a)&&fs_mkdir(b));
    assert(path_join(path,a,"ONE.TXT")); put(path,replacement,20);
    assert(path_join(path,a,"TWO.TXT")); put(path,replacement,20);
    operation_files = operation_skipped = 0; operation_progress = cancel_after_member;
    assert(!tree_copy(a,b,0)); operation_progress = NULL;
    assert(operation_files == 1); no_artifacts(b);
    assert(path_join(path,a,"ONE.TXT")); exact(path,replacement,20);
    assert(path_join(path,a,"TWO.TXT")); exact(path,replacement,20);
    operation_files = 0; operation_progress = cancel_after_member;
    assert(!tree_delete(a)); operation_progress = NULL; assert(operation_files == 1 && exists(a));
    assert(tree_delete(a)&&tree_delete(b));
    /* A failure to remove a moved source is reported without losing either copy. */
    assert(path_join(a,root,"SRC")); assert(path_join(b,root,"DST")); assert(fs_mkdir(a)&&fs_mkdir(b));
    assert(path_join(path,a,"FILE.TXT")); put(path,replacement,30);
    assert(path_join(other,b,"FILE.TXT")); fs_test_cross_drive = 1; fs_test_fail("delete","FILE.TXT",1);
    assert(!tree_copy(path,other,1)); fs_test_reset(); exact(path,replacement,30); exact(other,replacement,30);
    assert(tree_delete(a)&&tree_delete(b));
    /* Invalid names, oversized paths, and deep traversal fail explicitly. */
    assert(!valid_name("CON.TXT")&&!valid_name("aUx")&&!valid_name("lpt9.bin")&&!valid_name("CLOCK$"));
    assert(valid_name("COM10") && valid_name("\202NAME.TXT"));
    memset(too_long,'X',sizeof(too_long)-1); too_long[sizeof(too_long)-1]=0;
    files = bytes = 0; assert(!tree_measure(too_long,&files,&bytes));
    assert(!tree_copy(too_long,dst,0)); assert(!tree_delete(too_long));
    assert(path_join(a,root,"DEEP")); assert(fs_mkdir(a)); strcpy(path,a);
    for (i=0;i<33;++i) { strcat(path,"/X"); assert(fs_mkdir(path)); }
    files = bytes = 0; assert(!tree_measure(a,&files,&bytes)); assert(strstr(nw_error,"32"));
    assert(path_join(b,root,"COPY")); assert(!tree_copy(a,b,0)); assert(strstr(nw_error,"32"));
    assert(!tree_delete(a)); assert(strstr(nw_error,"32"));
    /* Remove the extra level explicitly, then bounded traversal can clean both. */
    assert(fs_delete(path,1)); assert(tree_delete(a)&&tree_delete(b));
    assert(fs_delete(src,0)&&fs_delete(dst,0)&&fs_delete(root,1));
    puts("PASS: distinct overwrite rollback, retained backups, boundary faults, partial moves, recursive cancellation, overlap and depth rejection");
    return 0;
}
