/* SPDX-License-Identifier: GPL-3.0-only */
/* POSIX test adapter for the shared DOS-oriented CORE.C interface. Translate
 * timestamps and readonly state, reject destination replacement on rename, and
 * inject errors at public filesystem boundaries. This is not a DOS emulator.
 */
#define _XOPEN_SOURCE 700
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "NW.H"
#include "fs_faults.h"
int test_short_write, test_write_fail, test_read_fail, test_stamp_fail, test_list_calls, test_commit_fail;
typedef struct { const char *operation, *suffix; unsigned call; } Fault;
static Fault faults[8];
static unsigned fault_count;
int fs_test_cross_drive;
/* Clear scheduled boundary faults and cross-drive simulation, not legacy flags.
 */
void fs_test_reset(void) { fault_count = 0; fs_test_cross_drive = 0; }
/* Schedule failure on the numbered matching call. Strings are borrowed until
 * reset; suffix may be empty for descriptor calls, which have no path.
 */
void fs_test_fail(const char *operation, const char *suffix, unsigned call)
{
    if (fault_count == 8 || !call) abort();
    faults[fault_count].operation = operation; faults[fault_count].suffix = suffix;
    faults[fault_count++].call = call;
}
/* Count only matching operation/path-suffix calls. Fire each scheduled fault
 * once with EIO, then disable it so cleanup can normally proceed.
 */
static int fault(const char *operation, const char *path)
{
    unsigned i;
    for (i = 0; i < fault_count; ++i) {
        Fault *f = &faults[i];
        unsigned n = path ? strlen(path) : 0, m = strlen(f->suffix);
        if (!f->call || strcmp(f->operation, operation) || m > n ||
            (m && strcmp(path + n - m, f->suffix))) continue;
        if (!--f->call) { errno = EIO; return 1; }
    }
    return 0;
}
void fs_error(const char *action) { sprintf(nw_error, "%.60s: %.80s", action, strerror(errno)); }
/* Resolve an existing host directory without changing process cwd; unlike DOS,
 * the host has no per-drive directory state. out may alias the input.
 */
int fs_canonical(const char *path, char *out)
{
    char *p = realpath(path, NULL);
    struct stat s;
    if (!p || stat(p, &s) || !S_ISDIR(s.st_mode) || strlen(p) >= NW_PATH) { free(p); return 0; }
    strcpy(out, p); free(p); return 1;
}
/* Map stat metadata into DOS attribute and packed two-second timestamp fields.
 */
int fs_info(const char *path, Entry *e)
{
    struct stat s;
    struct tm *t;
    if (stat(path, &s)) return 0;
    memset(e, 0, sizeof(*e));
    e->size = s.st_size; e->attr = S_ISDIR(s.st_mode) ? NW_DIR : 32;
    if (!(s.st_mode & S_IWUSR)) e->attr |= NW_READONLY;
    t = localtime(&s.st_mtime);
    e->date = ((t->tm_year - 80) << 9) | ((t->tm_mon + 1) << 5) | t->tm_mday;
    e->time = (t->tm_hour << 11) | (t->tm_min << 5) | t->tm_sec / 2;
    return 1;
}
/* Bound the pane cache and count scans. Host fixtures expose only DOS 8.3 names
 * plus the parent away from root; recursive enumeration instead rejects bad names.
 */
int fs_list(Panel *p)
{
    DIR *d;
    struct dirent *item;
    char path[NW_PATH];
    ++test_list_calls; d = opendir(p->path);
    if (!d) return 0;
    p->count = p->truncated = 0;
    while ((item = readdir(d)) != NULL) {
        if (!strcmp(item->d_name, ".") || !valid_name(item->d_name)) {
            if (strcmp(item->d_name, "..") || !strcmp(p->path, "/")) continue;
        }
        if (p->count == NW_FILES) { p->truncated = 1; break; }
        if (!path_join(path, p->path, item->d_name) || !fs_info(path, &p->files[p->count])) continue;
        strcpy(p->files[p->count++].name, item->d_name);
    }
    closedir(d); return 1;
}
int fs_mkdir(const char *p) { return mkdir(p, 0700) == 0; }
int fs_delete(const char *p, int d) { if (fault("delete", p)) return 0; return (d ? rmdir(p) : unlink(p)) == 0; }
/* POSIX rename normally replaces destinations, so explicitly reject existing
 * targets to model the DOS contract. Different parent paths can simulate EXDEV.
 */
int fs_rename(const char *a, const char *b)
{
    struct stat s;
    if (fault("rename_from", a) || fault("rename_to", b)) return 0;
    if (fs_test_cross_drive) {
        const char *x = strrchr(a, '/'), *y = strrchr(b, '/');
        if (x && y && (x - a != y - b || strncmp(a, b, x - a))) { errno = EXDEV; return 0; }
    }
    if (test_commit_fail && strstr(a, ".NEW")) { errno = EIO; return 0; }
    if (!stat(b, &s)) { errno = EEXIST; return 0; }
    return rename(a, b) == 0;
}
int fs_read_open(const char *p) { return open(p, O_RDONLY); }
int fs_create(const char *p) { if (fault("create", p)) return -1; return open(p, O_WRONLY | O_CREAT | O_EXCL, 0600); }
int fs_read(int fd, void *p, unsigned n) { if (fault("read", NULL)) return -1; if (test_read_fail) { errno = EIO; return -1; } return read(fd, p, n); }
int fs_write(int fd, const void *p, unsigned n)
{
    if (fault("write", NULL)) return -1;
    if (test_write_fail) { errno = ENOSPC; return -1; }
    if (test_short_write && n > 7) n = 7;
    return write(fd, p, n);
}
/* Close the real descriptor even when returning an injected failure, so fault
 * tests do not leak handles while checking the caller commit decision.
 */
int fs_close(int fd)
{
    int result = close(fd);
    return fault("close", NULL) ? -1 : result;
}
/* Convert packed DOS timestamp words back to local host time; DST is inferred.
 */
int fs_stamp(int fd, const Entry *e)
{
    struct tm t;
    struct timespec times[2];
    if (fault("stamp", NULL)) return 0;
    if (test_stamp_fail) { errno = EIO; return 0; }
    memset(&t, 0, sizeof(t));
    t.tm_year = (e->date >> 9) + 80; t.tm_mon = ((e->date >> 5) & 15) - 1;
    t.tm_mday = e->date & 31; t.tm_hour = e->time >> 11;
    t.tm_min = (e->time >> 5) & 63; t.tm_sec = (e->time & 31) * 2; t.tm_isdst = -1;
    times[0].tv_sec = times[1].tv_sec = mktime(&t);
    times[0].tv_nsec = times[1].tv_nsec = 0;
    return futimens(fd, times) == 0;
}
/* Model readonly via owner permissions; hidden/system/archive bits are not
 * represented by this adapter and require the actual DOS integration tests.
 */
int fs_attr(const char *p, unsigned a)
{
    struct stat s;
    if (fault("attr", p) || stat(p, &s)) return 0;
    return chmod(p, (a & NW_READONLY ? 0400 : 0600) | (S_ISDIR(s.st_mode) ? 0100 : 0)) == 0;
}

typedef struct { DIR *dir; char path[NW_PATH]; } HostSearch;
/* Walk the owned directory handle. Return 1/0/-1 for entry/end/error, excluding
 * dot entries; reject names the DOS backend could not represent.
 */
int fs_next(FsSearch *search, Entry *e)
{
    HostSearch *h = search->handle;
    struct dirent *item;
    char path[NW_PATH];
    if (fault("enumerate", h->path)) { fs_error("Read directory"); return -1; }
    while ((item = readdir(h->dir)) != NULL) {
        if (!strcmp(item->d_name, ".") || !strcmp(item->d_name, "..")) continue;
        if (!valid_name(item->d_name)) { strcpy(nw_error, "Not an 8.3 name"); return -1; }
        if (!path_join(path, h->path, item->d_name) || !fs_info(path, e)) return -1;
        strcpy(e->name, item->d_name); return 1;
    }
    return 0;
}
/* Allocate one host search per recursion frame. fs_end owns cleanup, including
 * after a failed first enumeration; failed opendir clears the stored handle.
 */
int fs_first(FsSearch *search, const char *dir, Entry *e)
{
    HostSearch *h = malloc(sizeof(*h));
    search->handle = h;
    if (!h) return -1;
    strcpy(h->path, dir); h->dir = opendir(dir);
    if (!h->dir) { free(h); search->handle = NULL; return -1; }
    return fs_next(search, e);
}
/* Release the owned directory/heap state; a NULL handle is already finished.
 */
void fs_end(FsSearch *search)
{
    HostSearch *h = search->handle;
    if (h) { closedir(h->dir); free(h); search->handle = NULL; }
}
