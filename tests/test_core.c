/* SPDX-License-Identifier: GPL-3.0-only */
/* Host regression coverage for byte-exact multi-block transfers, metadata,
 * exclusive destinations, cleanup after I/O/cancellation, and basic navigation.
 * Fixtures live in a private temporary directory and use the POSIX adapter.
 */
#define _XOPEN_SOURCE 700
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "NW.H"
extern int test_short_write, test_write_fail, test_read_fail, test_stamp_fail, test_list_calls, test_commit_fail;
static void assert_no_temps(const char *path)
{
    DIR *d = opendir(path);
    struct dirent *e;
    assert(d);
    while ((e = readdir(d)) != NULL) {
        assert(!strstr(e->d_name, ".TMP"));
        assert(!strstr(e->d_name, ".NEW")); assert(!strstr(e->d_name, ".BAK"));
    }
    closedir(d);
}
static int cancel_copy(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)total; return done < 4096UL;
}
static int overwrite(const char *src, const char *dst)
{
    (void)src; (void)dst; return 1;
}
static int skip(const char *src, const char *dst)
{
    (void)src; (void)dst; return 2;
}
int main(void)
{
    char root[] = "/tmp/nightwatch-test-XXXXXX";
    char a[NW_PATH], b[NW_PATH], c[NW_PATH], sub[NW_PATH], path[NW_PATH], name[13];
    Entry source, copy;
    int fd, i;
    unsigned char data[17003], got[17003];
    assert(mkdtemp(root));
    assert(path_join(a, root, "SOURCE.BIN"));
    assert(path_join(b, root, "COPY.BIN"));
    assert(path_join(c, root, "MOVED.BIN"));
    assert(path_join(sub, root, "SUB"));
    for (i = 0; i < (int)sizeof(data); ++i) data[i] = (unsigned char)i;
    fd = fs_create(a); assert(fd >= 0);
    assert(fs_write(fd, data, sizeof(data)) == sizeof(data)); assert(fs_close(fd) == 0);
    assert(fs_info(a, &source));
    test_short_write = 1;
    assert(file_copy(a, b));
    test_short_write = 0;
    fd = fs_read_open(b); assert(fd >= 0); assert(fs_read(fd, got, sizeof(got)) == sizeof(got)); fs_close(fd);
    assert(!memcmp(data, got, sizeof(data)));
    assert(fs_info(b, &copy)); assert(source.date == copy.date && source.time == copy.time);
    assert(!file_copy(a, b)); assert(!file_copy(a, a));
    assert(file_move(b, c)); assert(!fs_info(b, &copy)); assert(fs_info(c, &copy));
    assert(!file_move(a, c)); assert(fs_info(a, &copy));
    assert(fs_delete(c, 0));
    operation_progress = cancel_copy;
    assert(!file_copy(a, b)); assert(!fs_info(b, &copy)); assert_no_temps(root);
    operation_progress = NULL;
    assert(file_copy(a, b));
    operation_conflict = overwrite;
    test_write_fail = 1; assert(!tree_copy(a, b, 0)); test_write_fail = 0;
    fd = fs_read_open(b); assert(fd >= 0); assert(fs_read(fd, got, sizeof(got)) == sizeof(got)); fs_close(fd);
    assert(!memcmp(data, got, sizeof(data))); assert_no_temps(root);
    test_commit_fail = 1; assert(!tree_copy(a, b, 0)); test_commit_fail = 0;
    assert(fs_info(b, &copy)); assert_no_temps(root);
    fd = fs_read_open(b); assert(fd >= 0); assert(fs_read(fd, got, sizeof(got)) == sizeof(got)); fs_close(fd);
    assert(!memcmp(data, got, sizeof(data)));
    assert(tree_copy(a, b, 0)); assert(!tree_copy(a, a, 0));
    operation_conflict = skip;
    assert(tree_copy(a, b, 1)); assert(fs_info(a, &copy)); assert(operation_skipped == 1);
    operation_conflict = NULL; assert(fs_delete(b, 0));
    test_write_fail = 1; assert(!file_copy(a, b)); test_write_fail = 0;
    assert(!fs_info(b, &copy)); assert_no_temps(root);
    test_read_fail = 1; assert(!file_copy(a, b)); test_read_fail = 0;
    assert(!fs_info(b, &copy)); assert_no_temps(root);
    test_stamp_fail = 1; assert(!file_copy(a, b)); test_stamp_fail = 0;
    assert(!fs_info(b, &copy)); assert_no_temps(root);
    assert(fs_mkdir(sub)); assert(!file_copy(sub, b));
    strcpy(panes[0].path, root); assert(panel_load(&panes[0]));
    assert(!strcmp(panes[0].files[0].name, "..")); assert(!strcmp(panes[0].files[1].name, "SUB"));
    strcpy(panes[1].path, root); assert(panel_load(&panes[1]));
    panes[1].cursor = 2;
    panel_mark(&panes[1], 2, 1); assert(panes[1].marks == 1);
    i = test_list_calls;
    panel_snapshot(&panes[1], &panes[0]);
    assert(test_list_calls == i && panes[1].cursor == 2 && panes[1].marks == 0);
    panel_mark(&panes[1], 2, 1); assert(!panes[0].files[2].marked);
    panel_mark(&panes[1], 2, 1); assert(panes[1].marks == 1);
    panel_mark(&panes[1], 1, 1); assert(panes[1].marks == 2);
    panel_mark_all(&panes[1], 1); assert(panes[1].marks == 2);
    panel_mark_all(&panes[1], 0); assert(panes[1].marks == 0);
    panel_mark(&panes[1], 2, 1);
    strcpy(panes[1].path, "/no-such-nightwatch-directory");
    assert(!panel_load(&panes[1]) && panes[1].marks == 1);
    strcpy(panes[1].path, root); assert(panel_load(&panes[1]) && panes[1].marks == 0);
    assert(panel_enter(&panes[0], "SUB")); assert(panel_enter(&panes[0], ".."));
    assert(!strcmp(panes[0].path, root));
    assert(!strcmp(panes[0].files[panes[0].cursor].name, "SUB"));
    assert(name_match("*.bin", "SOURCE.BIN")); assert(name_match("*.*", "SUB"));
    assert(name_match("S?UR*", "SOURCE.BIN")); assert(!name_match("*.TXT", "SOURCE.BIN"));
    assert(panel_find(&panes[0], "SOURCE.*"));
    panel_pattern(&panes[0], "*.BIN", 1); assert(panes[0].marks == 1);
    panel_sort(&panes[0], 1, 1); assert(!strcmp(panes[0].files[panes[0].cursor].name, "SOURCE.BIN"));
    panel_sort(&panes[0], 0, 0);
    assert(!panel_enter(&panes[0], "NOPE")); assert(!strcmp(panes[0].path, root));
    panes[0].cursor = 999; panel_scroll(&panes[0], 17); assert(panes[0].cursor == panes[0].count - 1);
    assert(valid_name("NAME.TXT")); assert(valid_name("ABCDEFGH.XYZ"));
    assert(!valid_name("ABCDEFGHI.X")); assert(!valid_name("X.ABCD")); assert(!valid_name(".."));
    assert(!valid_name("X/Y")); assert(!valid_name("X?")); assert(!valid_name("A B"));
    memset(path, 'x', sizeof(path) - 1); path[sizeof(path)-1] = 0;
    assert(!path_join(b, path, "X"));
    for (i = 0; i < 520; ++i) {
        sprintf(name, "F%04d", i); assert(path_join(path, root, name));
        fd = fs_create(path); assert(fd >= 0); fs_close(fd);
    }
    assert(panel_load(&panes[0])); assert(panes[0].count == NW_FILES && panes[0].truncated);
    panel_mark_all(&panes[0], 1); assert(panes[0].marks == NW_FILES - 1);
    panel_mark(&panes[0], 2, 0); assert(panes[0].marks == NW_FILES - 2);
    panel_mark(&panes[0], 2, 1); assert(panes[0].marks == NW_FILES - 1);
    panel_mark_all(&panes[0], 0); assert(panes[0].marks == 0);
    for (i = 0; i < 520; ++i) { sprintf(name, "F%04d", i); path_join(path, root, name); assert(fs_delete(path, 0)); }
    /* Recursive traversal must include children omitted from the pane cache. */
    for (i = 0; i < 520; ++i) {
        sprintf(name, "F%04d", i); assert(path_join(path, sub, name));
        fd = fs_create(path); assert(fd >= 0); fs_close(fd);
    }
    assert(path_join(b, root, "TREE"));
    assert(tree_copy(sub, b, 0));
    assert(path_join(path, b, "F0519")); assert(fs_info(path, &copy));
    operation_conflict = skip;
    assert(tree_copy(sub, b, 1)); assert(fs_info(sub, &copy));
    assert(path_join(path, sub, "F0519")); assert(fs_info(path, &copy));
    operation_conflict = NULL;
    assert(path_join(path, sub, "LOOP")); assert(!tree_copy(sub, path, 0));
    assert(path_join(c, root, "TREE2")); assert(tree_copy(b, c, 1)); assert(!fs_info(b, &copy));
    assert(tree_delete(c)); assert(tree_delete(sub));
    assert(fs_delete(a, 0)); assert(fs_delete(root, 1));
    puts("PASS: byte-exact multi-buffer copy, timestamps, collision protection, move, failure cleanup, sorting, paths, names, bounds");
    return 0;
}
