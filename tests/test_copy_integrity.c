/* SPDX-License-Identifier: GPL-3.0-only */
/* Premature EOF must never commit a partial copy or remove a moved source. */
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include "NW.H"
#include "fs_faults.h"
static unsigned char data[17003], got[17004];
static int grew;
static int grow_source(const char *path, unsigned long done, unsigned long total)
{
    FILE *f;
    (void)done; (void)total;
    if (!grew) {
        f = fopen(path, "ab"); assert(f);
        assert(fputc(0x5a, f) != EOF && !fclose(f)); grew = 1;
    }
    return 1;
}
static int overwrite(const char *a, const char *b) { (void)a; (void)b; return 1; }
static void exact(const char *path, const void *bytes, unsigned length)
{
    int fd = fs_read_open(path);
    assert(fd >= 0 && fs_read(fd, got, sizeof(got)) == (int)length);
    assert(!memcmp(got, bytes, length) && !fs_close(fd));
}
static void clean(const char *root)
{
    DIR *dir = opendir(root);
    struct dirent *e;
    assert(dir && !fs_test_copy_allocations);
    while ((e = readdir(dir)) != NULL)
        assert(!strstr(e->d_name, ".TMP") && !strstr(e->d_name, ".NEW") && !strstr(e->d_name, ".BAK"));
    closedir(dir);
}
int main(void)
{
    char root[] = "/tmp/nw-integrity-XXXXXX", src[NW_PATH], dst[NW_PATH];
    static const char original[] = "keep existing destination";
    Entry e;
    unsigned fallback, call, move, i;
    int fd;
    assert(mkdtemp(root));
    assert(path_join(src, root, "SOURCE.BIN") && path_join(dst, root, "TARGET.BIN"));
    for (i = 0; i < sizeof(data); ++i) data[i] = (unsigned char)(i * 13);
    fd = fs_create(src); assert(fd >= 0);
    assert(fs_write(fd, data, sizeof(data)) == sizeof(data) && !fs_close(fd));
    for (fallback = 0; fallback < 2; ++fallback) {
        fs_test_copy_alloc_fail = fallback;
        for (call = 1; call <= 2; ++call) for (move = 0; move < 2; ++move) {
            fs_test_cross_drive = move;
            /* Force the direct file_move rename to fail so it exercises copying. */
            if (move) fs_test_fail("rename_from", "SOURCE.BIN", 1);
            fs_test_fail("eof", "", call);
            assert(!(move ? file_move(src, dst) : file_copy(src, dst)));
            assert(strstr(nw_error, "size")); fs_test_reset();
            exact(src, data, sizeof(data)); assert(!fs_info(dst, &e)); clean(root);
            fd = fs_create(dst); assert(fd >= 0);
            assert(fs_write(fd, original, sizeof(original)) == sizeof(original) && !fs_close(fd));
            operation_conflict = overwrite; fs_test_fail("eof", "", call);
            assert(!tree_copy(src, dst, move)); fs_test_reset();
            exact(src, data, sizeof(data)); exact(dst, original, sizeof(original)); clean(root);
            assert(fs_delete(dst, 0));
        }
    }
    fs_test_copy_alloc_fail = 0;
    assert(file_copy(src, dst)); exact(dst, data, sizeof(data));
    assert(fs_delete(dst, 0));
    /* A source that grows while being read must also fail the length check. */
    operation_progress = grow_source;
    assert(!file_copy(src, dst)); operation_progress = NULL;
    assert(grew && strstr(nw_error, "size") && !fs_info(dst, &e)); clean(root);
    fd = fs_read_open(src); assert(fd >= 0);
    assert(fs_read(fd, got, sizeof(got)) == sizeof(got) && !fs_close(fd));
    assert(!memcmp(got, data, sizeof(data)) && got[sizeof(data)] == 0x5a);
    assert(fs_delete(src, 0) && fs_delete(root, 1));
    puts("PASS: premature EOF and source growth retain sources/overwrite targets; both copy buffers");
    return 0;
}
