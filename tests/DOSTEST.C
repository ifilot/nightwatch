/* SPDX-License-Identifier: GPL-3.0-only */
/* Run only in an isolated writable DOS directory. */
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
/* Keep assertion diagnostics in COMPILE/RESULT logs rather than abort through
 * a runtime dialog. Run on disposable FAT volumes: this test mutates files. */
#undef assert
#define assert(x) do { if (!(x)) { printf("FAIL line %d: %s: %s\n", __LINE__, #x, nw_error); exit(1); } } while (0)
#include <dir.h>
#include <alloc.h>
#include "NW.H"
#include "VIEW.H"
unsigned _stklen = 8192;
static unsigned char data[17003], result[1024];
static void far *reservations[64];
static unsigned long last_copy_done, largest_copy_chunk;
static int observe_copy(const char *path, unsigned long done, unsigned long total)
{
    unsigned long chunk = done - last_copy_done;
    (void)path; (void)total;
    if (chunk > largest_copy_chunk) largest_copy_chunk = chunk;
    last_copy_done = done; return 1;
}
static int overwrite(const char *src, const char *dst)
{
    (void)src; (void)dst; return 1;
}
static int cancel(const char *src, unsigned long done, unsigned long total)
{
    (void)src; (void)total; return done < 4096UL;
}
int main(void)
{
    char root[NW_PATH], a[NW_PATH], b[NW_PATH], c[NW_PATH], sub[NW_PATH];
    Entry e, f;
    int i, fd, n, at;
    long offset;
    assert(getcwd(root, sizeof(root)));
    assert(path_join(a, root, "SOURCE.BIN"));
    assert(path_join(b, root, "COPY.BIN"));
    assert(path_join(c, root, "MOVE.BIN"));
    assert(path_join(sub, root, "EMPTY"));
    assert(fs_info(root, &e) && (e.attr & NW_DIR));
    for (i = 0; i < sizeof(data); ++i) data[i] = i;
    fd = fs_create(a); assert(fd >= 0);
    assert(fs_write(fd, data, sizeof(data)) == sizeof(data)); assert(!fs_close(fd));
    assert(fs_info(a, &e)); operation_progress = observe_copy;
    assert(file_copy(a, b)); operation_progress = NULL;
    assert(largest_copy_chunk == 16384UL && last_copy_done == sizeof(data));
    assert(fs_info(b, &f) && e.date == f.date && e.time == f.time);
    fd = fs_read_open(b); assert(fd >= 0);
    at = 0;
    while ((n = fs_read(fd, result, sizeof(result))) > 0) {
        assert(!memcmp(data + at, result, n)); at += n;
    }
    assert(n == 0 && at == sizeof(data)); fs_close(fd);
    assert(fs_delete(b, 0));
    /* Exhaust actual conventional far memory, rather than compiling away the
     * allocation path. The next copy must fall back to its near-data buffer. */
    for (i = 0; i < 64; ++i) {
        reservations[i] = farmalloc(16384UL);
        if (!reservations[i]) break;
    }
    assert(i < 64);
    last_copy_done = largest_copy_chunk = 0; operation_progress = observe_copy;
    assert(file_copy(a, b)); operation_progress = NULL;
    assert(largest_copy_chunk == 2048UL && last_copy_done == sizeof(data));
    while (i) farfree(reservations[--i]);
    fd = fs_read_open(b); assert(fd >= 0); at = 0;
    while ((n = fs_read(fd, result, sizeof(result))) > 0) {
        assert(!memcmp(data + at, result, n)); at += n;
    }
    assert(n == 0 && at == sizeof(data)); assert(!fs_close(fd));
    puts("PASS: far-buffer copy and low-memory fallback are byte-exact on DOS");
    /* Case-insensitive final names must not collide with internal temporaries. */
    assert(path_join(sub,root,"nw0000.tmp")); assert(file_copy(a,sub));
    assert(path_join(b,root,"NW0001.TMP")); assert(file_copy(a,b));
    assert(fs_info(sub,&f) && f.size == sizeof(data));
    assert(fs_info(b,&f) && f.size == sizeof(data));
    assert(fs_delete(sub,0) && fs_delete(b,0));
    assert(path_join(sub,root,"EMPTY")); assert(path_join(b,root,"COPY.BIN"));
    assert(!file_copy(a, b)); assert(!file_copy(a, a));
    assert(file_move(b, c)); assert(!fs_info(b, &f));
    assert(fs_mkdir(sub));
    strcpy(panes[0].path, root); assert(panel_load(&panes[0]));
    assert(panel_enter(&panes[0], "EMPTY"));
    assert(panel_load(&panes[0])); assert(panes[0].count == 1);
    assert(panel_enter(&panes[0], "..")); assert(!strcmp(panes[0].path, root));
    assert(!panel_enter(&panes[0], "MISSING")); assert(!strcmp(panes[0].path, root));
    assert(fs_attr(a, NW_READONLY));
    assert(fs_info(a, &e)); printf("Source attributes after setting readonly: %u\n", (unsigned)e.attr);
    assert(file_copy(a, b));
    assert(fs_info(b, &f)); printf("Copied attributes: %u\n", (unsigned)f.attr);
    assert(fs_info(b, &f) && (f.attr & NW_READONLY));
    assert(fs_attr(a, 0)); assert(fs_attr(b, 0));
    assert(fs_delete(b, 0));
    operation_progress = cancel; assert(!file_copy(a, b)); operation_progress = NULL;
    assert(!fs_info(b, &f)); assert(file_copy(a, b));
    operation_conflict = overwrite; assert(tree_copy(a, b, 0)); operation_conflict = NULL;
    assert(fs_delete(a, 0)); assert(fs_delete(b, 0)); assert(fs_delete(c, 0));
    assert(path_join(a, sub, "INNER")); assert(fs_mkdir(a));
    assert(path_join(b, a, "DATA.BIN")); assert(file_copy("D:\\DOSTEST.C", b));
    assert(path_join(c, root, "TREE")); assert(tree_copy(sub, c, 0));
    assert(path_join(a, c, "INNER")); assert(path_join(b, a, "DATA.BIN")); assert(fs_info(b, &f));
    assert(fs_attr(b, NW_HIDDEN | NW_SYSTEM));
    assert(path_join(a, root, "TREE2")); assert(tree_copy(c, a, 0));
    assert(tree_delete(c)); assert(tree_delete(a));
    assert(tree_copy(sub, "F:\\XDRIVE", 1)); assert(!fs_info(sub, &f));
    setdisk(5); assert(getcwd(b, sizeof(b))); assert(!strcmp(b, "F:\\")); setdisk(4);
    assert(tree_delete("F:\\XDRIVE"));
    assert(path_join(sub, root, "DEPTH")); assert(fs_mkdir(sub)); strcpy(a, sub);
    for (i = 0; i < 32; ++i) { strcat(a, "\\X"); assert(fs_mkdir(a)); }
    strcat(a, "\\LEAF.TXT"); fd = fs_create(a); assert(fd >= 0); assert(!fs_close(fd));
    assert(path_join(b, root, "DEEP")); assert(tree_copy(sub, b, 0));
    assert(tree_delete(sub)); assert(tree_delete(b));
    assert(!view_offset(" -4294967295", &offset));
    assert(!valid_name("con.txt") && !valid_name("LPT1.BIN"));
    assert(path_join(a, root, "\202NAME.TXT")); fd = fs_create(a); assert(fd >= 0);
    assert(fs_write(fd, data, 17) == 17); assert(!fs_close(fd));
    assert(path_join(b, root, "\202COPY.TXT")); assert(tree_copy(a,b,0));
    assert(path_join(c, root, "\202MOVE.TXT")); assert(tree_copy(a,c,1));
    assert(!fs_info(a,&e)); assert(tree_delete(b) && tree_delete(c));
    puts("PASS: actual DOS backend recursive copy/delete, cross-drive move, overwrite, cancellation, attributes, paths and root");
    return 0;
}
