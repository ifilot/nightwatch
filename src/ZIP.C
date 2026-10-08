/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded, streaming ZIP reader; zlib supplies raw DEFLATE, not ZIP parsing.
 * No archive-controlled allocation or recursive traversal. Pairwise preflight
 * trades scan time for constant memory and detects DOS case/name collisions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifdef __TURBOC__
#include <alloc.h>
#include <dos.h>
#endif
#include "ZIP.H"
#include "zlib.h"
#define ZIP_LIMIT 2147483647UL
#ifdef NW_HOST
#define SEP '/'
#else
#define SEP '\\'
#endif
typedef struct {
    int fd;
    unsigned count;
    unsigned long size, start, end;
    char source[NW_PATH], target[NW_PATH];
} Archive;
typedef struct {
    char name[NW_PATH];
    unsigned method, flags, directory;
    unsigned long size, packed, crc, local, data, finish, next;
    Entry info;
} Member;
static int fail(const char *message) { strcpy(nw_error, message); return 0; }
static unsigned word(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
static unsigned long dword(const unsigned char *p)
{ return word(p) | ((unsigned long)word(p + 2) << 16); }
static int poll(const char *name, unsigned long done, unsigned long total)
{
    if (operation_progress && !operation_progress(name, done, total))
        return fail("Operation cancelled");
    return 1;
}
static int read_at(Archive *a, unsigned long at, void *data, unsigned n)
{
    unsigned have = 0;
    int got;
    if (at > a->size || n > a->size - at)
        return fail("ZIP: truncated record");
    if (fs_seek(a->fd, (long)at, SEEK_SET) < 0) { fs_error("Seek ZIP"); return 0; }
    while (have < n) {
        got = fs_read(a->fd, (char *)data + have, n - have);
        if (got < 0) { fs_error("Read ZIP"); return 0; }
        if (!got) return fail("ZIP: unexpected end of file");
        have += got;
    }
    return 1;
}
static int equal_path(const char *a, const char *b)
{
    while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { ++a; ++b; }
    return *a == *b;
}
/* Reject NULs, non-ASCII, absolute paths, dot/device components and LFN loss.
 * Normalize both standard / and Windows PowerShell's legacy backslashes. */
static int member_name(Member *m, unsigned char *raw, unsigned length)
{
    unsigned i, at = 0, part = 0;
    char component[13];
    if (!length || length >= NW_PATH) return fail("ZIP: filename is too long");
    m->directory = raw[length - 1] == '/' || raw[length - 1] == '\\';
    for (i = 0; i < length; ++i) {
        unsigned char c = raw[i];
        if (c == '/' || c == '\\') {
            component[part] = 0;
            if (!valid_name(component)) return fail("ZIP: requires safe DOS 8.3 names");
            part = 0;
            if (i + 1 == length) break;
            m->name[at++] = SEP;
        } else {
            if (c < 33 || c >= 127 || part == 12)
                return fail("ZIP: requires ASCII DOS 8.3 names");
            component[part++] = (char)c;
            m->name[at++] = (char)toupper(c);
        }
    }
    if (!m->directory) {
        component[part] = 0;
        if (!valid_name(component)) return fail("ZIP: requires safe DOS 8.3 names");
    }
    m->name[at] = 0;
    return 1;
}
/* Parse extra-field framing, rejecting ZIP64 even when sentinel sizes are absent. */
static int extra(Archive *a, unsigned long at, unsigned length)
{
    unsigned char h[4];
    unsigned n;
    while (length) {
        if (length < 4 || !read_at(a, at, h, 4)) return fail("ZIP: invalid extra field");
        n = word(h + 2);
        if (word(h) == 1) return fail("ZIP64 is not supported");
        if (n > length - 4) return fail("ZIP: invalid extra field");
        at += 4UL + n; length -= 4 + n;
    }
    return 1;
}
static int member(Archive *a, unsigned long at, Member *m)
{
    unsigned char h[46], raw[NW_PATH], l[30], desc[16];
    unsigned name, extras, comment, version, creator, mode, lname, lextra;
    unsigned long stop, p;
    memset(m, 0, sizeof(*m));
    if (at > a->end || 46UL > a->end - at || !read_at(a, at, h, 46))
        return fail("ZIP: invalid central directory");
    if (dword(h) != 0x02014b50UL) return fail("ZIP: invalid central record");
    version = word(h + 6); creator = word(h + 4) >> 8;
    m->flags = word(h + 8); m->method = word(h + 10);
    m->info.time = word(h + 12); m->info.date = word(h + 14);
    m->crc = dword(h + 16); m->packed = dword(h + 20); m->size = dword(h + 24);
    name = word(h + 28); extras = word(h + 30); comment = word(h + 32);
    m->local = dword(h + 42);
    if (version > 20 || (m->flags & ~0x080eU) || word(h + 34))
        return fail("ZIP: encryption, split or newer format unsupported");
    if (m->method != 0 && m->method != 8) return fail("ZIP: compression method unsupported (use store or deflate)");
    if (m->size > ZIP_LIMIT || m->packed > ZIP_LIMIT || m->local > ZIP_LIMIT)
        return fail("ZIP: ZIP64 or oversized entry unsupported");
    m->next = at + 46UL + name + extras + comment;
    if (m->next > a->end || name >= NW_PATH) return fail("ZIP: invalid filename or directory bounds");
    if (!read_at(a, at + 46, raw, name) ||
        !member_name(m, raw, name) || !extra(a, at + 46 + name, extras)) return 0;
    mode = (unsigned)(dword(h + 38) >> 16) & 0170000;
    if (creator == 3 && mode && mode != 0100000 && mode != 0040000)
        return fail("ZIP: symbolic links and special files unsupported");
    if ((creator == 3 && mode == 0040000 && !m->directory) ||
        ((h[38] & NW_DIR) && !m->directory)) return fail("ZIP: inconsistent directory attributes");
    m->info.attr = creator == 0 ? h[38] & (NW_READONLY | NW_HIDDEN | NW_SYSTEM | 32) : 32;
    m->info.size = m->size;
    if (m->directory && (m->size || m->packed || m->crc)) return fail("ZIP: directory carries file data");
    if (!m->method && m->packed != m->size) return fail("ZIP: stored size mismatch");
    if (m->local >= a->start || 30UL > a->start - m->local || !read_at(a, m->local, l, 30))
        return fail("ZIP: invalid local offset");
    if (dword(l) != 0x04034b50UL || word(l + 4) != version || word(l + 6) != m->flags ||
        word(l + 8) != m->method || word(l + 10) != m->info.time || word(l + 12) != m->info.date)
        return fail("ZIP: local and central headers disagree");
    lname = word(l + 26); lextra = word(l + 28);
    if (lname != name) return fail("ZIP: local filename mismatch");
    p = m->local + 30UL;
    if (!read_at(a, p, h, 0)) return 0;
    /* raw retains the exact central spelling, including a final slash. */
    {
        unsigned i;
        unsigned char buf[NW_PATH];
        if (!read_at(a, p, buf, lname)) return 0;
        for (i = 0; i < lname; ++i) if (buf[i] != raw[i]) return fail("ZIP: local filename mismatch");
    }
    m->data = p + lname + lextra;
    if (m->data > a->start || m->packed > a->start - m->data || !extra(a, p + lname, lextra))
        return fail("ZIP: invalid compressed data bounds");
    stop = m->data + m->packed;
    if (!(m->flags & 8)) {
        if (dword(l + 14) != m->crc || dword(l + 18) != m->packed || dword(l + 22) != m->size)
            return fail("ZIP: local size or CRC mismatch");
    } else {
        unsigned n = 12;
        if ((dword(l + 14) && dword(l + 14) != m->crc) ||
            (dword(l + 18) && dword(l + 18) != m->packed) ||
            (dword(l + 22) && dword(l + 22) != m->size))
            return fail("ZIP: invalid descriptor local sizes");
        if (stop > a->start || 12UL > a->start - stop || !read_at(a, stop, desc, 12))
            return fail("ZIP: missing data descriptor");
        if (dword(desc) == 0x08074b50UL &&
            !(dword(desc) == m->crc && dword(desc + 4) == m->packed && dword(desc + 8) == m->size)) {
            if (16UL > a->start - stop || !read_at(a, stop, desc, 16)) return fail("ZIP: truncated data descriptor");
            memmove(desc, desc + 4, 12); n = 16;
        }
        if (dword(desc) != m->crc || dword(desc + 4) != m->packed || dword(desc + 8) != m->size)
            return fail("ZIP: data descriptor mismatch");
        stop += n;
    }
    m->finish = stop;
    return 1;
}
static int open_archive(Archive *a, const char *source, const char *target)
{
    unsigned char buf[534], h[22];
    unsigned long high, low, floor, pos;
    int i;
    char dir[NW_PATH], filename[NW_PATH], *cut;
    a->fd = -1;
    if (strlen(source) >= NW_PATH || !fs_canonical(target, a->target))
        return fail("ZIP: destination must be an existing directory");
    strcpy(dir, source); cut = strrchr(dir, SEP);
    if (cut) {
        strcpy(filename, cut + 1); *cut = 0;
        if (!dir[0]) strcpy(dir, "/");
    } else {
        if (strchr(source, ':')) return fail("ZIP: use an absolute drive path");
        strcpy(filename, source); strcpy(dir, ".");
    }
#ifndef NW_HOST
    if (strlen(dir) == 2 && dir[1] == ':') strcat(dir, "\\");
#endif
    if (!fs_canonical(dir, a->source)) return fail("ZIP: invalid archive directory");
    strcpy(dir, a->source);
    if (!path_join(a->source, dir, filename)) return 0;
    a->fd = fs_read_open(source);
    if (a->fd < 0) { fs_error("Open ZIP"); return 0; }
    {
        long size = fs_seek(a->fd, 0L, SEEK_END);
        if (size < 22) return fail("ZIP: missing end record");
        a->size = (unsigned long)size;
        if (a->size > ZIP_LIMIT) return fail("ZIP: archive exceeds DOS offset limits");
    }
    high = a->size - 22;
    floor = high > 65535UL ? high - 65535UL : 0;
    for (;;) {
        low = high - floor > 511 ? high - 511 : floor;
        if (!read_at(a, low, buf, (unsigned)(high - low + 22))) return 0;
        for (i = (int)(high - low); i >= 0; --i) {
            if (dword(buf + i) != 0x06054b50UL) continue;
            memcpy(h, buf + i, 22); pos = low + i;
            if (pos + 22UL + word(h + 20) != a->size) continue;
            if (word(h + 4) || word(h + 6) || word(h + 8) != word(h + 10))
                return fail("ZIP: split archives unsupported");
            a->count = word(h + 10); a->start = dword(h + 16);
            if (a->count == 65535U || dword(h + 12) > ZIP_LIMIT || a->start > pos ||
                dword(h + 12) != pos - a->start) return fail("ZIP: invalid directory or ZIP64");
            a->end = pos;
            return 1;
        }
        if (low == floor) break;
        high = low - 1;
        if (!poll(source, 0, 0)) return 0;
    }
    return fail("ZIP: missing end record");
}
/* Verify existing directories resolve to the intended spelling/location. This
 * also rejects host-test symlinks; DOS has no filesystem symbolic links. */
static int directories(Archive *a, const Member *m, int create)
{
    char path[NW_PATH], resolved[NW_PATH];
    unsigned i, begin;
    Entry e;
    if (!path_join(path, a->target, m->name)) return 0;
    begin = (unsigned)strlen(a->target);
    if (a->target[begin - 1] != SEP) ++begin;
    for (i = begin; ; ++i) {
        char c = path[i];
        if (c == SEP || (!c && m->directory)) {
            path[i] = 0;
            if (fs_info(path, &e)) {
                if (!(e.attr & NW_DIR) || !fs_canonical(path, resolved) ||
#ifdef NW_HOST
                    strcmp(path, resolved)
#else
                    !equal_path(path, resolved)
#endif
                    )
                    return fail("ZIP: destination ancestor is not a safe directory");
            } else if (create && !fs_mkdir(path)) { fs_error("Create ZIP directory"); return 0; }
            path[i] = c;
        }
        if (!c) break;
    }
    if (!m->directory && fs_info(path, &e) && (e.attr & NW_DIR))
        return fail("ZIP: destination is a directory");
    if (equal_path(path, a->source)) return fail("ZIP: cannot replace its source archive");
    return 1;
}
static int preflight(Archive *a, unsigned long *files, unsigned long *bytes)
{
    Member m, old;
    unsigned i, j;
    unsigned long at = a->start, before, n;
    *files = *bytes = 0;
    for (i = 0; i < a->count; ++i) {
        if (!member(a, at, &m) || !directories(a, &m, 0) || !poll(m.name, 0, 0)) return 0;
        if (!m.directory) {
            if (m.size > ZIP_LIMIT - *bytes) return fail("ZIP: total output exceeds DOS limits");
            ++*files; *bytes += m.size;
        }
        before = a->start;
        for (j = 0; j < i; ++j) {
            if (!member(a, before, &old)) return 0;
            n = (unsigned long)strlen(m.name);
            if (!strcmp(m.name, old.name) ||
                (!m.directory && !strncmp(m.name, old.name, (unsigned)n) && old.name[n] == SEP) ||
                (!old.directory && !strncmp(old.name, m.name, strlen(old.name)) && m.name[strlen(old.name)] == SEP))
                return fail("ZIP: duplicate or conflicting DOS names");
            if (m.local < old.finish && old.local < m.finish) return fail("ZIP: overlapping entries");
            before = old.next;
            if ((j & 31) == 31 && !poll(m.name, 0, 0)) return 0;
        }
        at = m.next;
    }
    if (at != a->end) return fail("ZIP: directory count or length mismatch");
    return 1;
}
#ifdef NW_ZIP_TEST
int zip_fail_allocation;
unsigned zip_live_allocations;
#endif
typedef struct { void NW_FAR *original; } Allocation;
static voidpf allocate(voidpf unused, unsigned items, unsigned size)
{
    unsigned long bytes = (unsigned long)items * size;
    void NW_FAR *p;
    Allocation NW_FAR *h;
    (void)unused;
#ifdef NW_ZIP_TEST
    if (zip_fail_allocation && !--zip_fail_allocation) return NULL;
#endif
    if (!bytes || bytes > 65500UL) return NULL;
#ifdef __TURBOC__
    p = farmalloc(bytes + sizeof(Allocation) + 15UL);
    if (!p) return NULL;
    h = MK_FP(FP_SEG(p) + (FP_OFF(p) >> 4), FP_OFF(p) & 15);
#else
    p = malloc((size_t)(bytes + sizeof(Allocation)));
    if (!p) return NULL;
    h = p;
#endif
    h->original = p;
#ifdef NW_ZIP_TEST
    ++zip_live_allocations;
#endif
    return (char NW_FAR *)h + sizeof(Allocation);
}
static void release(voidpf unused, voidpf pointer)
{
    Allocation NW_FAR *h;
    void NW_FAR *p;
    (void)unused;
    if (!pointer) return;
    h = (Allocation NW_FAR *)((char NW_FAR *)pointer - sizeof(Allocation)); p = h->original;
#ifdef NW_ZIP_TEST
    --zip_live_allocations;
#endif
#ifdef __TURBOC__
    farfree(p);
#else
    free(p);
#endif
}
static int write_all(int fd, const unsigned char *buf, unsigned n)
{
    unsigned at = 0;
    int written;
    while (at < n) {
        written = fs_write(fd, buf + at, n - at);
        if (written <= 0) { fs_error("Write ZIP output"); return 0; }
        at += written;
    }
    return 1;
}
static int unpack(Archive *a, Member *m, int out)
{
    unsigned char input[1024], output[1024];
    z_stream stream;
    unsigned long left = m->packed, written = 0, checksum = 0, offset = m->data;
    unsigned n, produced;
    int status = Z_OK, ok = 1, initialized = 0;
    memset(&stream, 0, sizeof(stream));
    if (m->method == 8) {
        stream.zalloc = allocate; stream.zfree = release;
        status = inflateInit2(&stream, -15);
        if (status != Z_OK) return fail("ZIP: insufficient decompression memory");
        initialized = 1;
    }
    for (;;) {
        n = 0;
        if (!stream.avail_in && left) {
            n = left > sizeof(input) ? sizeof(input) : (unsigned)left;
            if (!read_at(a, offset, input, n)) { ok = 0; break; }
            offset += n; left -= n; stream.next_in = input; stream.avail_in = n;
        }
        if (m->method == 8) {
            unsigned previous = stream.avail_in;
            stream.next_out = output; stream.avail_out = sizeof(output);
            status = inflate(&stream, Z_NO_FLUSH);
            produced = sizeof(output) - stream.avail_out;
            if (status != Z_OK && status != Z_STREAM_END) {
                ok = fail(status == Z_MEM_ERROR ? "ZIP: insufficient decompression memory" : "ZIP: invalid or truncated DEFLATE stream"); break;
            }
            if (status != Z_STREAM_END && !produced && previous == stream.avail_in) {
                ok = fail("ZIP: incomplete DEFLATE stream"); break;
            }
        } else {
            produced = stream.avail_in; memcpy(output, input, produced); stream.avail_in = 0;
            if (!left) status = Z_STREAM_END;
        }
        if (produced > m->size - written) { ok = fail("ZIP: output exceeds declared size"); break; }
        if (!write_all(out, output, produced)) { ok = 0; break; }
        checksum = crc32(checksum, output, produced); written += produced;
        operation_current_bytes = written; operation_transferred += produced;
        if (!poll(m->name, written, m->size)) { ok = 0; break; }
        if (status == Z_STREAM_END) break;
    }
    if (initialized) inflateEnd(&stream);
    if (ok && (left || stream.avail_in || written != m->size || checksum != m->crc))
        ok = fail("ZIP: CRC, size or compressed length mismatch");
    return ok;
}
static int extract_member(Archive *a, Member *m)
{
    char dst[NW_PATH], tmp[NW_PATH];
    Entry old;
    int exists, answer, fd, ok;
    operation_current_bytes = 0;
    if (!poll(m->name, 0, m->size) || !directories(a, m, 1)) return 0;
    if (m->directory) return 1;
    if (!path_join(dst, a->target, m->name)) return 0;
    exists = fs_info(dst, &old);
    if (exists) {
        if (!operation_conflict) return fail("ZIP: destination exists; Ctrl-U offers overwrite/skip");
        answer = operation_conflict ? operation_conflict(m->name, dst) : 0;
        if (answer == 2) { ++operation_skipped; operation_bytes += m->size; return 1; }
        if (answer != 1) return fail("Operation cancelled");
    }
    if (!operation_temp(dst, tmp, "TMP")) return 0;
    fd = fs_create(tmp);
    if (fd < 0) { fs_error("Create ZIP temporary"); return 0; }
    ok = unpack(a, m, fd);
    if (ok && !fs_stamp(fd, &m->info)) { fs_error("Stamp ZIP output"); ok = 0; }
    if (fs_close(fd) < 0 && ok) { fs_error("Close ZIP output"); ok = 0; }
    if (ok && !fs_attr(tmp, m->info.attr)) { fs_error("Set ZIP attributes"); ok = 0; }
    if (!ok) { operation_discard(tmp); return 0; }
    if (!operation_commit(tmp, dst, exists ? &old : NULL)) return 0;
    ++operation_files; operation_bytes += m->size; operation_current_bytes = 0;
    return 1;
}
int zip_measure(const char *archive, const char *target, unsigned long *files, unsigned long *bytes)
{
    Archive a;
    unsigned long count = 0, size = 0;
    int ok = open_archive(&a, archive, target);
    if (ok) ok = preflight(&a, &count, &size);
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ZIP"); ok = 0; }
    if (ok) { *files = count; *bytes = size; }
    return ok;
}
int zip_extract(const char *archive, const char *target)
{
    Archive a;
    Member m;
    unsigned i;
    unsigned long files, bytes, at;
    int ok = open_archive(&a, archive, target);
    if (ok) ok = preflight(&a, &files, &bytes);
    at = ok ? a.start : 0;
    for (i = 0; ok && i < a.count; ++i) {
        ok = member(&a, at, &m) && extract_member(&a, &m); at = m.next;
    }
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ZIP"); ok = 0; }
    operation_current_bytes = 0;
    return ok;
}
