/* SPDX-License-Identifier: GPL-3.0-only */
/* Reproducible file-level storage tests, using bounded near buffers on 8086.
 * The versioned manifest defines a fixed plan; an identical target marker binds
 * cleanup to that plan. A separate completion marker gates post-reboot verify.
 * Scratch mutations never change the final dataset. Every failure stops writes;
 * no automatic cleanup destroys evidence or follows an unrecorded directory.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "STORTEST.H"
#define MASK32 0xffffffffUL
#define BOUNDARIES 22
#define BLOCK 512
StorageReport storage_report;
int (*storage_progress)(const char *, const char *, unsigned long, unsigned long);
typedef struct {
    char root[NW_PATH];
    char record[180];
    unsigned long seed;
    int extended, log;
} Test;
static const unsigned long lengths[BOUNDARIES] = {
    0UL, 1UL, 511UL, 512UL, 513UL, 4095UL, 4096UL, 4097UL, 32767UL, 32768UL, 32769UL,
    65535UL, 65536UL, 65537UL, 131071UL, 131072UL, 131073UL, 262143UL, 262144UL,
    262145UL, 513UL, 65537UL
};
/* Every block carries full seed/file/block identifiers, so repeated payloads or
 * aliased addresses cannot hide behind the short PRNG period. Zero/FF files
 * supplement this stream. All arithmetic is defined for both DOS and host C89.
 */
static void pattern(unsigned char *out, unsigned long offset, unsigned long seed,
                    unsigned id)
{
    unsigned i, state;
    unsigned long block = offset / BLOCK, value;
    state = (unsigned)((seed ^ (seed >> 16) ^ block ^ (block >> 16) ^ id) & 65535UL);
    for (i = 0; i < BLOCK; ++i) {
        state = (state * 25173U + 13849U) & 65535U;
        out[i] = (unsigned char)(state >> 8);
    }
    for (i = 0; i < 12; ++i) {
        value = i < 4 ? seed : (i < 8 ? (unsigned long)id : block);
        out[i] = (unsigned char)(value >> ((i & 3) * 8));
    }
}
static unsigned entries(const Test *t) { return t->extended ? 2200 : 40; }
static unsigned long big_size(const Test *t) { return t->extended ? 16777216UL : 1048576UL; }
/* Far immutable diagnostics leave the 64 KiB DOS near-data group available
 * for pane caches, the runtime stack and stdio allocations. */
#define NW_STORAGE_TEXT
#include "STORMSG.H"
char *storage_label(char *out, unsigned which)
{
    unsigned i = 0;
#ifdef NW_HOST
    const char *source = storage_messages[which];
#else
    const char NW_FAR *source = asset_stmsg(which);
#endif
    do { out[i] = source[i]; } while (out[i++]);
    return out;
}
static int fail(unsigned which)
{
    if (storage_report.state != STORAGE_CANCELLED) storage_report.state = STORAGE_FAILED;
    storage_label(nw_error, which); return 0;
}
static void io_error(unsigned which)
{
    char action[80];
    storage_label(action, which); fs_error(action);
    if (storage_report.state != STORAGE_CANCELLED) storage_report.state = STORAGE_FAILED;
}
/* Resolve the nearest existing ancestor without writing. Only missing DOS 8.3
 * components are accepted; unavailable roots and file/dir conflicts fail before
 * any mkdir. The UI calls again with create=1 only after explicit approval.
 * out may alias path. Parent folders are user-owned, outside run cleanup.
 */
int storage_directory(const char *path, char *out, int create, int *missing)
{
    char parent[NW_PATH], suffix[NW_PATH], joined[NW_PATH], *part, *end;
    unsigned n, cut, length;
    Entry entry;
    *missing = 0;
    n = strlen(path);
    if (!n || n >= NW_PATH) return fail(ST_DIRPATH);
    strcpy(parent, path); suffix[0] = 0;
    for (;;) {
        n = strlen(parent);
        while (n > 1 && (parent[n-1] == '/' || parent[n-1] == '\\') &&
               !(n == 3 && parent[1] == ':')) parent[--n] = 0;
        if (fs_info(parent, &entry) && !(entry.attr & NW_DIR)) return fail(ST_DIRFILE);
        if (fs_canonical(parent, out)) break;
        cut = n;
        while (cut && parent[cut-1] != '/' && parent[cut-1] != '\\' &&
               parent[cut-1] != ':') --cut;
        if (!valid_name(parent + cut)) return fail(ST_DIRPATH);
        length = n - cut;
        if (length + 1 + strlen(suffix) >= NW_PATH) return fail(ST_DIRPATH);
        memmove(suffix + length + 1, suffix, strlen(suffix) + 1);
        memcpy(suffix, parent + cut, length); suffix[length] = '/';
        if (!cut) strcpy(parent, ".");
        else if (parent[cut-1] == ':') { parent[cut] = '.'; parent[cut+1] = 0; }
        else if (cut == 1 || (cut == 3 && parent[1] == ':')) parent[cut] = 0;
        else parent[cut-1] = 0;
        /* A root/current directory that cannot resolve must not loop forever. */
        if (!strcmp(parent, ".") || (strlen(parent) == 3 && parent[1] == ':') ||
            !strcmp(parent, "/") || !strcmp(parent, "\\")) {
            if (!fs_canonical(parent, out)) { io_error(ST_CANONICAL); return 0; }
            break;
        }
    }
    *missing = suffix[0] != 0;
    part = suffix;
    while (*part) {
        end = strchr(part, '/'); *end = 0;
        if (!path_join(joined, out, part)) return 0;
        if (create && !fs_mkdir(joined)) { io_error(ST_CREATEDIR); return 0; }
        strcpy(out, joined); part = end + 1;
    }
    nw_error[0] = 0;
    return 1;
}
static int poll(const char *phase, const char *path, unsigned long done, unsigned long total)
{
    if (storage_progress && !storage_progress(phase, path, done, total))
    {
        storage_report.state = STORAGE_CANCELLED;
        return fail(ST_CANCEL);
    }
    return 1;
}
static void report_reset(unsigned action, int extended, unsigned long seed)
{
    memset(&storage_report, 0, sizeof(storage_report));
    storage_report.action = action; storage_report.stages = action ? 3 : 6;
    storage_report.extended = extended; storage_report.seed = seed;
    storage_report.cycles = extended ? 3 : 1;
}
/* The prior stage is passed only after its caller has checked every result. */
static int step(Test *t, unsigned stage)
{
    if (stage) storage_report.passed |= 1U << (stage - 1);
    storage_report.stage = stage;
    return poll("PHASE", t->root, 0, 0);
}
/* A short/failed log write stops the test before the next target operation. */
static int log_line(Test *t, const char *line)
{
    unsigned n = strlen(line);
    if (fs_write(t->log, line, n) != (int)n) return fail(ST_LOGWRITE);
    return 1;
}
static int event(Test *t, const char *phase, const char *path)
{
    char line[180];
    sprintf(line, "%s %.127s\n", phase, path);
    return log_line(t, line) && poll(phase, path, 0, 0);
}
static int joined(char *path, const Test *t, const char *name)
{
    return path_join(path, t->root, name);
}
/* Path components generated here are DOS 8.3 names. */
static void boundary_name(char *name, unsigned i) { sprintf(name, "B%02u.BIN", i); }
static void growth_name(char *name, unsigned i) { sprintf(name, "G%04u.BIN", i); }
static int file_path(char *path, const Test *t, int kind, unsigned i)
{
    char dir[NW_PATH], sub[NW_PATH], name[13];
    if (kind == 2) {
        if (!joined(dir, t, "GROW")) return 0;
        growth_name(name, i); return path_join(path, dir, name);
    }
    if (!joined(dir, t, "DATA") || !path_join(sub, dir, "NEST")) return 0;
    if (kind == 1) strcpy(name, "BIG.BIN"); else boundary_name(name, i);
    return path_join(path, sub, name);
}
static int close_checked(int fd)
{
    if (fs_close(fd) < 0) { io_error(ST_CLOSE); return 0; }
    return 1;
}
static int write_exact(int fd, const void *data, unsigned n)
{
    int count;
    /* A short write is evidence, rather than permission to continue allocating.
     * Its errno can be stale, so only a negative result reports a C I/O error.
     */
    count = fs_write(fd, data, n);
    if (count < 0) { io_error(ST_WRITE); return 0; }
    if (count != (int)n) return fail(ST_SHORTWRITE);
    return 1;
}
/* mode 0: normal; 1: zero; 2: FF; 3: scratch overwrite+append;
 * 4: same scratch after truncate and explicit zero-filled extension.
 */
static void expected(unsigned char *data, unsigned long offset, const Test *t,
                     unsigned id, int mode)
{
    unsigned i;
    pattern(data, offset, t->seed, id);
    if (mode == 1 || mode == 2) memset(data, mode == 1 ? 0 : 255, BLOCK);
    if (mode >= 3) {
        for (i = 0; i < BLOCK; ++i) {
            unsigned long pos = offset + i;
            if ((pos >= 511 && pos < 1024) || pos >= 70001UL) {
                /* Mutation bytes have a simple independent address pattern. */
                data[i] = (unsigned char)((pos ^ (pos >> 8) ^ t->seed ^ id ^ 0x5a) & 255);
            }
            if (mode == 4 && pos >= 32769UL) data[i] = 0;
        }
    }
}
/* Count explicit mutation data as well as streamed writes, excluding metadata
 * and evidence files. Gap bytes filled internally by DOS are not transfers.
 */
static int mutation_write(int fd, const void *data, unsigned n)
{
    if (!write_exact(fd, data, n)) return 0;
    storage_report.written += n; return 1;
}
/* Reopen files between writing and reading; compare every byte plus exact EOF.
 * Error messages retain the first differing absolute byte and expected/actual.
 */
static int stream(Test *t, const char *path, unsigned long size, unsigned id,
                  int mode, int writing)
{
    unsigned char want[BLOCK], got[BLOCK];
    unsigned long pos = 0;
    unsigned n, i;
    int fd, ok = 1, count;
    Entry e;
    const char *phase = writing ? "WRITE" : "VERIFY";
    if (!event(t, phase, path)) return 0;
    if (!writing && (!fs_info(path, &e) || (e.attr & NW_DIR) || e.size != size)) {
        sprintf(nw_error, "Storage test size/type mismatch: %.100s", path); return 0;
    }
    fd = writing ? fs_create(path) : fs_read_open(path);
    if (fd < 0) { io_error(writing ? ST_CREATEFILE : ST_OPENFILE); return 0; }
    while (pos < size && ok) {
        n = size - pos < BLOCK ? (unsigned)(size - pos) : BLOCK;
        expected(want, pos, t, id, mode);
        if (writing) ok = write_exact(fd, want, n);
        else {
            count = fs_read(fd, got, n);
            if (count != (int)n) ok = fail(ST_SHORTREAD);
            else for (i = 0; i < n; ++i) if (got[i] != want[i]) {
                sprintf(nw_error, "Mismatch %.72s byte %lu expected %02X got %02X",
                        path, pos + i, (unsigned)want[i], (unsigned)got[i]);
                ok = 0; break;
            }
        }
        if (ok) {
            if (writing) storage_report.written += n;
            else storage_report.verified += n;
        }
        pos += n;
        if (ok) ok = poll(phase, path, pos, size);
    }
    if (!writing && ok && fs_read(fd, got, 1) != 0) ok = fail(ST_EOF);
    if (ok) ok = close_checked(fd); else fs_close(fd);
    if (ok && !writing) ++storage_report.files;
    return ok;
}
static int make_dir(Test *t, const char *path)
{
    if (!event(t, "MKDIR", path)) return 0;
    if (!fs_mkdir(path)) { io_error(ST_MKDIR); return 0; }
    return 1;
}
static int remove_entry(Test *t, const char *path, int directory)
{
    Entry e;
    if (!event(t, directory ? "RMDIR" : "DELETE", path)) return 0;
    if (!fs_delete(path, directory)) { io_error(ST_REMOVE); return 0; }
    if (fs_info(path, &e)) return fail(ST_DELETED);
    return 1;
}
/* Strict manifest parser: no scanf overflow, unknown versions or extra fields.
 * Canonical target paths are retained verbatim for the ownership comparison.
 */
static int load_plan(Test *t, const char *manifest)
{
    unsigned char raw[180];
    unsigned n = 0, digit, mode;
    unsigned long seed = 0;
    char *p, *end;
    int fd, count;
    fd = fs_read_open(manifest);
    if (fd < 0) { io_error(ST_OPENPLAN); return 0; }
    while (n < sizeof(raw) - 1) {
        count = fs_read(fd, raw + n, sizeof(raw) - 1 - n);
        if (count < 0) { fs_close(fd); return fail(ST_READPLAN); }
        if (!count) break;
        n += count;
    }
    count = fs_read(fd, raw + n, 1);
    if (!close_checked(fd)) return 0;
    if (count || n == sizeof(raw) - 1 || memchr(raw, 0, n)) return fail(ST_PLANLENGTH);
    raw[n] = 0;
    if (strncmp((char *)raw, "NWSTEST1\n", 9)) return fail(ST_PLANVERSION);
    p = (char *)raw + 9;
    if ((*p != '0' && *p != '1') || p[1] != ' ') return fail(ST_PROFILE);
    mode = *p - '0'; p += 2;
    if (!isdigit((unsigned char)*p)) return fail(ST_SEED);
    while (isdigit((unsigned char)*p)) {
        digit = *p++ - '0';
        if (seed > (MASK32 - digit) / 10UL) return fail(ST_SEEDRANGE);
        seed = seed * 10UL + digit;
    }
    if (*p++ != '\n') return fail(ST_TARGET);
    end = strchr(p, '\n');
    if (!end || end[1] || end == p ||
        (unsigned)(end - p) >= NW_PATH) return fail(ST_TARGET);
    strcpy(t->record, (char *)raw); *end = 0;
    /* Only absolute canonical paths from storage_start are accepted. */
#ifndef NW_HOST
    if (!isalpha((unsigned char)p[0]) || p[1] != ':' || p[2] != '\\') return fail(ST_ABSOLUTE);
#else
    if (*p != '/') return fail(ST_ABSOLUTE);
#endif
    strcpy(t->root, p); t->seed = seed; t->extended = mode;
    return 1;
}
static int different_drive(const char *a, const char *b)
{
#ifndef NW_HOST
    if (!a[0] || !b[0] || a[1] != ':' || b[1] != ':' ||
        toupper((unsigned char)a[0]) == toupper((unsigned char)b[0]))
        return fail(ST_OTHERDRIVE);
#else
    (void)a; (void)b;
#endif
    return 1;
}
/* Exclusive numbered local logs preserve earlier runs and verification results. */
static int open_log(Test *t, const char *manifest, const char *action)
{
    char path[NW_PATH], *dot;
    unsigned i;
    Entry e;
    if (!different_drive(t->root, manifest)) return 0;
    if (strlen(manifest) + 4 >= NW_PATH) return fail(ST_PLANPATH);
    strcpy(path, manifest); dot = strrchr(path, '.');
    if (!dot || strchr(dot, '\\') || strchr(dot, '/')) dot = path + strlen(path);
    for (i = 0; i < 100; ++i) {
        sprintf(dot, ".L%02u", i);
        if (fs_info(path, &e)) continue;
        t->log = fs_create(path);
        if (t->log < 0) { io_error(ST_OPENLOG); return 0; }
        return log_line(t, t->record) && log_line(t, action);
    }
    return fail(ST_LOGFULL);
}
static int finish(Test *t, int ok)
{
    char saved[160], line[180];
    int closed;
    strcpy(saved, nw_error);
    if (t->log >= 0) {
        sprintf(line, "%s %.159s\n", ok ? "PASS" : "STOP", ok ? "storage test completed" : saved);
        if (!log_line(t, line) && ok) { ok = 0; strcpy(saved, nw_error); }
        closed = fs_close(t->log); t->log = -1;
        if (closed < 0 && ok) { io_error(ST_CLOSELOG); ok = 0; strcpy(saved, nw_error); }
    }
    if (!ok) strcpy(nw_error, saved);
    if (ok) {
        storage_report.passed |= 1U << storage_report.stage;
        storage_report.state = STORAGE_PASSED;
    } else if (storage_report.state != STORAGE_CANCELLED) storage_report.state = STORAGE_FAILED;
    return ok;
}
static int marker(Test *t, const char *name, int writing)
{
    char path[NW_PATH], data[180];
    unsigned n = strlen(t->record);
    int fd, ok;
    Entry e;
    if (!joined(path, t, name)) return 0;
    if (!writing && (!fs_info(path, &e) || (e.attr & NW_DIR) || e.size != n))
        return fail(ST_MARKER);
    fd = writing ? fs_create(path) : fs_read_open(path);
    if (fd < 0) { io_error(ST_OPENMARK); return 0; }
    if (writing) ok = write_exact(fd, t->record, n);
    else ok = fs_read(fd, data, n) == (int)n && !memcmp(data, t->record, n);
    if (!ok && !writing) fail(ST_MARKERMATCH);
    if (ok) ok = close_checked(fd); else fs_close(fd);
    return ok;
}
/* Enumerate against an exact allowlist before cleanup or accepting a dataset.
 * Unknown files/directories, including hidden ones, are preserved and reported.
 * The scan also exercises directory enumeration beyond the pane's 512-entry cap.
 */
static int numbered(const char *name, char prefix, unsigned digits, unsigned limit)
{
    unsigned i, value = 0;
    if (name[0] != prefix || strlen(name) != digits + 5 || strcmp(name + digits + 1, ".BIN")) return 0;
    for (i = 1; i <= digits; ++i) {
        if (name[i] < '0' || name[i] > '9') return 0;
        value = value * 10 + name[i] - '0';
    }
    return value < limit;
}
static int allowed(const Test *t, unsigned dir, const Entry *e)
{
    int isdir = (e->attr & NW_DIR) != 0;
    if (dir == 0) {
        if (isdir) return !strcmp(e->name, "DATA") || !strcmp(e->name, "GROW") || !strcmp(e->name, "SCRATCH");
        return !strcmp(e->name, "OWNER.ID") || !strcmp(e->name, "DONE.ID");
    }
    if (dir == 1) return isdir && !strcmp(e->name, "NEST");
    if (dir == 2 && !isdir)
        return !strcmp(e->name, "BIG.BIN") || numbered(e->name, 'B', 2, BOUNDARIES);
    if (dir == 3 && !isdir) return numbered(e->name, 'G', 4, entries(t));
    if (dir == 4) return isdir && (!strcmp(e->name, "A") || !strcmp(e->name, "B"));
    if (dir == 5 || dir == 6) return isdir ? !strcmp(e->name, "CHILD") :
        !strcmp(e->name, dir == 5 ? "WORK.BIN" : "MOVED.BIN");
    return 0;
}
static int dir_path(char *out, const Test *t, unsigned dir)
{
    static const char *names[] = { "", "DATA", "NEST", "GROW", "SCRATCH", "A", "B", "CHILD", "CHILD" };
    char parent[NW_PATH], sub[NW_PATH];
    if (!dir) { strcpy(out, t->root); return 1; }
    if (dir == 2) return joined(parent, t, "DATA") && path_join(out, parent, names[dir]);
    if (dir >= 5) {
        if (!joined(parent, t, "SCRATCH")) return 0;
        if (dir < 7) return path_join(out, parent, names[dir]);
        return path_join(sub, parent, dir == 7 ? "A" : "B") && path_join(out, sub, "CHILD");
    }
    return joined(out, t, names[dir]);
}
static int audit(Test *t, int complete)
{
    FsSearch search;
    Entry e, dir;
    unsigned d, counts[9], expect_count;
    char path[NW_PATH];
    int result;
    memset(counts, 0, sizeof(counts));
    for (d = 0; d < 9; ++d) {
        if (!dir_path(path, t, d) || !event(t, "LIST", path)) return 0;
        if (!fs_info(path, &dir)) {
            if (complete && d < 5) return fail(ST_DIRMISSING);
            continue;
        }
        if (!(dir.attr & NW_DIR)) return fail(ST_DIRTYPE);
        result = fs_first(&search, path, &e);
        while (result > 0) {
            if (!allowed(t, d, &e)) {
                fs_end(&search);
                sprintf(nw_error, "Unrecorded entry retained: %.100s/%.12s", path, e.name); return 0;
            }
            ++counts[d];
            if (!poll("LIST", path, counts[d], 0)) { fs_end(&search); return 0; }
            result = fs_next(&search, &e);
        }
        fs_end(&search);
        if (result < 0) { io_error(ST_ENUM); return 0; }
        if (complete) {
            expect_count = d == 0 ? (complete == 2 ? 4 : 5) : d == 1 ? 1 : d == 2 ? BOUNDARIES + 1 : d == 3 ? entries(t) : 0;
            if (counts[d] != expect_count) return fail(ST_DIRCOUNT);
        }
    }
    return 1;
}
static int dataset(Test *t, int writing, int reused)
{
    char path[NW_PATH];
    unsigned i, index;
    for (i = 0; i < BOUNDARIES; ++i) {
        index = writing ? i : BOUNDARIES - i - 1;
        if (!file_path(path, t, 0, index) || !stream(t, path, lengths[index], index,
            index == 20 ? 1 : index == 21 ? 2 : 0, writing)) return 0;
    }
    if (!file_path(path, t, 1, 0) || !stream(t, path, big_size(t), 100, 0, writing)) return 0;
    for (i = 0; i < entries(t); ++i) {
        index = writing ? i : entries(t) - i - 1;
        if (!file_path(path, t, 2, index) || !stream(t, path, 17,
            1000 + index + (reused && !(index & 1) ? 3000 : 0), 0, writing)) return 0;
    }
    return 1;
}
/* Explicit open/seek/write/resize path; untouched bytes are verified afterward. */
static int mutate(Test *t, unsigned cycle)
{
    char a[NW_PATH], b[NW_PATH], work[NW_PATH], moved[NW_PATH], child[NW_PATH], dest[NW_PATH];
    unsigned char bytes[513];
    unsigned i, id = 9000 + cycle;
    int fd, ok;
    if (!dir_path(a, t, 5) || !dir_path(b, t, 6) ||
        !make_dir(t, a) || !make_dir(t, b) || !path_join(child, a, "CHILD") ||
        !path_join(dest, b, "CHILD") || !make_dir(t, child) ||
        !path_join(work, a, "WORK.BIN") || !path_join(moved, b, "MOVED.BIN") ||
        !stream(t, work, 70001UL, id, 0, 1) || !stream(t, work, 70001UL, id, 0, 0)) return 0;
    if (!event(t, "OVERWRITE/APPEND", work)) return 0;
    fd = fs_update_open(work);
    if (fd < 0) { io_error(ST_MUTOPEN); return 0; }
    for (i = 0; i < 513; ++i) bytes[i] = (unsigned char)(((511UL+i) ^ ((511UL+i)>>8) ^ t->seed ^ id ^ 0x5a) & 255);
    ok = fs_seek(fd, 511L, SEEK_SET) == 511L && mutation_write(fd, bytes, 513);
    for (i = 0; i < 513; ++i) bytes[i] = (unsigned char)(((70001UL+i) ^ ((70001UL+i)>>8) ^ t->seed ^ id ^ 0x5a) & 255);
    if (ok) ok = fs_seek(fd, 0L, SEEK_END) == 70001L && mutation_write(fd, bytes, 513);
    if (!ok && !nw_error[0]) io_error(ST_MUTSEEK);
    if (ok) ok = close_checked(fd); else fs_close(fd);
    if (!ok || !stream(t, work, 70514UL, id, 3, 0) || !event(t, "TRUNCATE/EXTEND", work)) return 0;
    fd = fs_update_open(work);
    if (fd < 0) { io_error(ST_RESIZEOPEN); return 0; }
    ok = fs_resize(fd, 32769UL);
    if (!ok) io_error(ST_TRUNCATE);
    if (ok) ok = close_checked(fd); else fs_close(fd);
    if (!ok || !stream(t, work, 32769UL, id, 3, 0)) return 0;
    fd = fs_update_open(work);
    if (fd < 0) { io_error(ST_EXTENDOPEN); return 0; }
    /* Write beyond EOF: OTTERSD guarantees zero-filled gaps. */
    bytes[0] = 0;
    ok = fs_seek(fd, 70000L, SEEK_SET) == 70000L && mutation_write(fd, bytes, 1);
    if (!ok && !nw_error[0]) io_error(ST_EXTEND);
    if (ok) ok = close_checked(fd); else fs_close(fd);
    if (!ok || !stream(t, work, 70001UL, id, 4, 0) || !event(t, "MOVE FILE", work)) return 0;
    if (!fs_rename(work, moved)) { io_error(ST_MOVEFILE); return 0; }
    if (!stream(t, moved, 70001UL, id, 4, 0) || !event(t, "MOVE DIRECTORY", child)) return 0;
    if (!fs_rename(child, dest)) { io_error(ST_MOVEDIR); return 0; }
    return remove_entry(t, dest, 1) && remove_entry(t, moved, 0) &&
           remove_entry(t, a, 1) && remove_entry(t, b, 1);
}
int storage_start(const char *target, const char *local, int extended,
                  unsigned long seed, char *manifest)
{
    Test t;
    char full[NW_PATH], localdir[NW_PATH], name[13], path[NW_PATH];
    Entry e;
    unsigned i, cycle;
    int fd, ok;
    report_reset(0, extended, seed);
    memset(&t, 0, sizeof(t)); t.log = -1; manifest[0] = nw_error[0] = 0;
    if ((extended != 0 && extended != 1) || seed > MASK32) return fail(ST_ARGUMENTS);
    if (!fs_canonical(target, full) || !fs_canonical(local, localdir)) { io_error(ST_CANONICAL); return 0; }
    if (!different_drive(full, localdir)) return 0;
    t.seed = seed; t.extended = extended;
    for (i = 1; i <= 9999; ++i) {
        sprintf(name, "NWT%04u", i);
        if (!path_join(t.root, full, name)) return 0;
        if (fs_info(t.root, &e)) continue;
        strcat(name, ".DAT");
        if (!path_join(manifest, localdir, name)) return 0;
        if (!fs_info(manifest, &e)) break;
    }
    if (i > 9999) return fail(ST_RUNFULL);
    /* Reserve room for the deepest owned directory before creating anything. */
    if (strlen(t.root) + 20 >= NW_PATH) return fail(ST_TARGETLONG);
    sprintf(t.record, "NWSTEST1\n%d %lu\n%s\n", extended, seed, t.root);
    if (!open_log(&t, manifest, "START\n")) return finish(&t, 0);
    fd = fs_create(manifest);
    if (fd < 0) { io_error(ST_CREATEPLAN); return finish(&t, 0); }
    ok = write_exact(fd, t.record, strlen(t.record));
    if (ok) ok = close_checked(fd); else fs_close(fd);
    if (!ok || !make_dir(&t, t.root) || !marker(&t, "OWNER.ID", 1)) return finish(&t, 0);
    for (i = 1; i < 5; ++i) {
        if (!dir_path(path, &t, i) || !make_dir(&t, path)) return finish(&t, 0);
    }
    if (!step(&t, 1) || !dataset(&t, 1, 0) || !step(&t, 2) ||
        !dataset(&t, 0, 0) || !step(&t, 3)) return finish(&t, 0);
    /* Remove alternate files, recreate with new contents, then verify survivors. */
    for (i = 0; i < entries(&t); i += 2)
        if (!file_path(path, &t, 2, i) || !remove_entry(&t, path, 0)) return finish(&t, 0);
    for (i = 0; i < entries(&t); i += 2)
        if (!file_path(path, &t, 2, i) || !stream(&t, path, 17, 4000 + i, 0, 1)) return finish(&t, 0);
    if (!step(&t, 4)) return finish(&t, 0);
    for (cycle = 0; cycle < (extended ? 3 : 1); ++cycle) {
        storage_report.cycle = cycle + 1;
        if (!mutate(&t, cycle) || !dataset(&t, 0, 1)) return finish(&t, 0);
    }
    if (!step(&t, 5) || !event(&t, "FINAL CHECK", t.root) || !audit(&t, 2) || !marker(&t, "DONE.ID", 1)) return finish(&t, 0);
    return finish(&t, 1);
}
int storage_verify(const char *manifest)
{
    Test t;
    report_reset(1, 0, 0);
    memset(&t, 0, sizeof(t)); t.log = -1; nw_error[0] = 0;
    if (!load_plan(&t, manifest)) return 0;
    storage_report.seed = t.seed; storage_report.extended = t.extended;
    if (!open_log(&t, manifest, "VERIFY ONLY\n")) return finish(&t, 0);
    return finish(&t, marker(&t, "OWNER.ID", 0) && marker(&t, "DONE.ID", 0) &&
                  step(&t, 1) && audit(&t, 1) && step(&t, 2) && dataset(&t, 0, 1));
}
/* Delete only recorded children, in reverse directory order after a full audit.
 * Missing entries are normal after cancellation/partial cleanup. Markers are
 * removed last; an unknown entry causes the preflight to fail without deletion.
 */
int storage_cleanup(const char *manifest)
{
    Test t;
    char path[NW_PATH], child[NW_PATH];
    FsSearch search;
    Entry e, info;
    int d, result;
    report_reset(2, 0, 0);
    memset(&t, 0, sizeof(t)); t.log = -1; nw_error[0] = 0;
    if (!load_plan(&t, manifest)) return 0;
    storage_report.seed = t.seed; storage_report.extended = t.extended;
    if (!open_log(&t, manifest, "CLEANUP\n")) return finish(&t, 0);
    if (!marker(&t, "OWNER.ID", 0) || !step(&t, 1) || !audit(&t, 0) ||
        !step(&t, 2)) return finish(&t, 0);
    for (d = 8; d >= 0; --d) {
        if (!dir_path(path, &t, d)) return finish(&t, 0);
        if (!fs_info(path, &info)) continue;
        /* Restart enumeration after each deletion; directory cursors may move. */
        for (;;) {
            result = fs_first(&search, path, &e);
            while (result > 0 && ((e.attr & NW_DIR) ||
                (!d && (!strcmp(e.name, "OWNER.ID") || !strcmp(e.name, "DONE.ID")))))
            {
                if (!d && (e.attr & NW_DIR)) {
                    fs_end(&search); fail(ST_DIRCOUNT); return finish(&t, 0);
                }
                result = fs_next(&search, &e);
            }
            fs_end(&search);
            if (result < 0) { io_error(ST_CLEANENUM); return finish(&t, 0); }
            if (!result) break;
            if (!allowed(&t, d, &e) || !path_join(child, path, e.name) ||
                !remove_entry(&t, child, 0)) return finish(&t, 0);
        }
        if (d && !remove_entry(&t, path, 1)) return finish(&t, 0);
    }
    if (!joined(path, &t, "DONE.ID")) return finish(&t, 0);
    if (fs_info(path, &e) && !remove_entry(&t, path, 0)) return finish(&t, 0);
    /* Poll once before the final pair, so cancellation never removes ownership
     * while leaving an empty test root. Restore it after a failed final rmdir
     * when possible; retained local evidence still describes the failed cleanup.
     */
    if (!event(&t, "FINAL CLEANUP", t.root) || !joined(path, &t, "OWNER.ID")) return finish(&t, 0);
    if (!fs_delete(path, 0)) { io_error(ST_REMOVE); return finish(&t, 0); }
    if (!fs_delete(t.root, 1)) {
        char saved[160];
        io_error(ST_REMOVE); strcpy(saved, nw_error);
        marker(&t, "OWNER.ID", 1); strcpy(nw_error, saved);
        return finish(&t, 0);
    }
    return finish(&t, 1);
}
