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
#include "ARCHIVE.H"
/* Parse extra-field framing, rejecting ZIP64 even when sentinel sizes are absent. */
static int extra(Archive *a, unsigned long at, unsigned length)
{
    unsigned char h[4];
    unsigned n;
    while (length) {
        if (length < 4 || !archive_read_at(a, at, h, 4)) return archive_message(ARC_ZIP_INVALID_EXTRA_FIELD);
        n = archive_word(h + 2);
        if (archive_word(h) == 1) return archive_message(ARC_ZIP64_IS_NOT_SUPPORTED);
        if (n > length - 4) return archive_message(ARC_ZIP_INVALID_EXTRA_FIELD);
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
    if (at > a->end || 46UL > a->end - at || !archive_read_at(a, at, h, 46))
        return archive_message(ARC_ZIP_INVALID_CENTRAL_DIRECTORY);
    if (archive_dword(h) != 0x02014b50UL) return archive_message(ARC_ZIP_INVALID_CENTRAL_RECORD);
    version = archive_word(h + 6); creator = archive_word(h + 4) >> 8;
    m->flags = archive_word(h + 8); m->method = archive_word(h + 10);
    m->info.time = archive_word(h + 12); m->info.date = archive_word(h + 14);
    m->crc = archive_dword(h + 16); m->packed = archive_dword(h + 20); m->size = archive_dword(h + 24);
    name = archive_word(h + 28); extras = archive_word(h + 30); comment = archive_word(h + 32);
    m->local = archive_dword(h + 42);
    if (version > 20 || (m->flags & ~0x080eU) || archive_word(h + 34))
        return archive_message(ARC_ZIP_ENCRYPTION_SPLIT_OR_NEWER_FORMAT_UNSUPPORTED);
    if (m->method != 0 && m->method != 8) return archive_message(ARC_ZIP_COMPRESSION_METHOD_UNSUPPORTED_USE_STORE_OR_DEFLATE);
    if (m->size > ZIP_LIMIT || m->packed > ZIP_LIMIT || m->local > ZIP_LIMIT)
        return archive_message(ARC_ZIP_ZIP64_OR_OVERSIZED_ENTRY_UNSUPPORTED);
    m->next = at + 46UL + name + extras + comment;
    if (m->next > a->end || name >= NW_PATH) return archive_message(ARC_ZIP_INVALID_FILENAME_OR_DIRECTORY_BOUNDS);
    if (!archive_read_at(a, at + 46, raw, name) ||
        !archive_member_name(m, raw, name) || !extra(a, at + 46 + name, extras)) return 0;
    mode = (unsigned)(archive_dword(h + 38) >> 16) & 0170000;
    if (creator == 3 && mode && mode != 0100000 && mode != 0040000)
        return archive_message(ARC_ZIP_SYMBOLIC_LINKS_AND_SPECIAL_FILES_UNSUPPORTED);
    if ((creator == 3 && mode == 0040000 && !m->directory) ||
        ((h[38] & NW_DIR) && !m->directory)) return archive_message(ARC_ZIP_INCONSISTENT_DIRECTORY_ATTRIBUTES);
    m->info.attr = creator == 0 ? h[38] & (NW_READONLY | NW_HIDDEN | NW_SYSTEM | 32) : 32;
    m->info.size = m->size;
    if (m->directory && (m->size || m->packed || m->crc)) return archive_message(ARC_ZIP_DIRECTORY_CARRIES_FILE_DATA);
    if (!m->method && m->packed != m->size) return archive_message(ARC_ZIP_STORED_SIZE_MISMATCH);
    if (m->local >= a->start || 30UL > a->start - m->local || !archive_read_at(a, m->local, l, 30))
        return archive_message(ARC_ZIP_INVALID_LOCAL_OFFSET);
    if (archive_dword(l) != 0x04034b50UL || archive_word(l + 4) != version || archive_word(l + 6) != m->flags ||
        archive_word(l + 8) != m->method || archive_word(l + 10) != m->info.time || archive_word(l + 12) != m->info.date)
        return archive_message(ARC_ZIP_LOCAL_AND_CENTRAL_HEADERS_DISAGREE);
    lname = archive_word(l + 26); lextra = archive_word(l + 28);
    if (lname != name) return archive_message(ARC_ZIP_LOCAL_FILENAME_MISMATCH);
    p = m->local + 30UL;
    if (!archive_read_at(a, p, h, 0)) return 0;
    /* raw retains the exact central spelling, including a final slash. */
    {
        unsigned i;
        unsigned char buf[NW_PATH];
        if (!archive_read_at(a, p, buf, lname)) return 0;
        for (i = 0; i < lname; ++i) if (buf[i] != raw[i]) return archive_message(ARC_ZIP_LOCAL_FILENAME_MISMATCH);
    }
    m->data = p + lname + lextra;
    if (m->data > a->start || m->packed > a->start - m->data || !extra(a, p + lname, lextra))
        return archive_message(ARC_ZIP_INVALID_COMPRESSED_DATA_BOUNDS);
    stop = m->data + m->packed;
    if (!(m->flags & 8)) {
        if (archive_dword(l + 14) != m->crc || archive_dword(l + 18) != m->packed || archive_dword(l + 22) != m->size)
            return archive_message(ARC_ZIP_LOCAL_SIZE_OR_CRC_MISMATCH);
    } else {
        unsigned n = 12;
        if ((archive_dword(l + 14) && archive_dword(l + 14) != m->crc) ||
            (archive_dword(l + 18) && archive_dword(l + 18) != m->packed) ||
            (archive_dword(l + 22) && archive_dword(l + 22) != m->size))
            return archive_message(ARC_ZIP_INVALID_DESCRIPTOR_LOCAL_SIZES);
        if (stop > a->start || 12UL > a->start - stop || !archive_read_at(a, stop, desc, 12))
            return archive_message(ARC_ZIP_MISSING_DATA_DESCRIPTOR);
        if (archive_dword(desc) == 0x08074b50UL &&
            !(archive_dword(desc) == m->crc && archive_dword(desc + 4) == m->packed && archive_dword(desc + 8) == m->size)) {
            if (16UL > a->start - stop || !archive_read_at(a, stop, desc, 16)) return archive_message(ARC_ZIP_TRUNCATED_DATA_DESCRIPTOR);
            memmove(desc, desc + 4, 12); n = 16;
        }
        if (archive_dword(desc) != m->crc || archive_dword(desc + 4) != m->packed || archive_dword(desc + 8) != m->size)
            return archive_message(ARC_ZIP_DATA_DESCRIPTOR_MISMATCH);
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
    a->target[0] = 0;
    if (strlen(source) >= NW_PATH || (target && !fs_canonical(target, a->target)))
        return archive_message(ARC_ZIP_DESTINATION_MUST_BE_AN_EXISTING_DIRECTORY);
    strcpy(dir, source); cut = strrchr(dir, SEP);
    if (cut) {
        strcpy(filename, cut + 1); *cut = 0;
        if (!dir[0]) strcpy(dir, "/");
    } else {
        if (strchr(source, ':')) return archive_message(ARC_ZIP_USE_AN_ABSOLUTE_DRIVE_PATH);
        strcpy(filename, source); strcpy(dir, ".");
    }
#ifndef NW_HOST
    if (strlen(dir) == 2 && dir[1] == ':') strcat(dir, "\\");
#endif
    if (!fs_canonical(dir, a->source)) return archive_message(ARC_ZIP_INVALID_ARCHIVE_DIRECTORY);
    strcpy(dir, a->source);
    if (!path_join(a->source, dir, filename)) return 0;
    a->fd = fs_read_open(source);
    if (a->fd < 0) { fs_error("Open ZIP"); return 0; }
    {
        long size = fs_seek(a->fd, 0L, SEEK_END);
        if (size < 22) return archive_message(ARC_ZIP_MISSING_END_RECORD);
        a->size = (unsigned long)size;
        if (a->size > ZIP_LIMIT) return archive_message(ARC_ZIP_ARCHIVE_EXCEEDS_DOS_OFFSET_LIMITS);
    }
    high = a->size - 22;
    floor = high > 65535UL ? high - 65535UL : 0;
    for (;;) {
        low = high - floor > 511 ? high - 511 : floor;
        if (!archive_read_at(a, low, buf, (unsigned)(high - low + 22))) return 0;
        for (i = (int)(high - low); i >= 0; --i) {
            if (archive_dword(buf + i) != 0x06054b50UL) continue;
            memcpy(h, buf + i, 22); pos = low + i;
            if (pos + 22UL + archive_word(h + 20) != a->size) continue;
            if (archive_word(h + 4) || archive_word(h + 6) || archive_word(h + 8) != archive_word(h + 10))
                return archive_message(ARC_ZIP_SPLIT_ARCHIVES_UNSUPPORTED);
            a->count = archive_word(h + 10); a->start = archive_dword(h + 16);
            if (a->count == 65535U || archive_dword(h + 12) > ZIP_LIMIT || a->start > pos ||
                archive_dword(h + 12) != pos - a->start) return archive_message(ARC_ZIP_INVALID_DIRECTORY_OR_ZIP64);
            a->end = pos;
            return 1;
        }
        if (low == floor) break;
        high = low - 1;
        if (!archive_poll(source, 0, 0)) return 0;
    }
    return archive_message(ARC_ZIP_MISSING_END_RECORD);
}
static int preflight(Archive *a, unsigned long *files, unsigned long *bytes)
{
    Member m, old;
    unsigned i, j, n;
    unsigned long at = a->start, before;
    *files = *bytes = 0;
    for (i = 0; i < a->count; ++i) {
        if (!member(a, at, &m) || (a->target[0] && !archive_directories(a, &m, 0)) || !archive_poll(m.name, 0, 0)) return 0;
        if (!m.directory) {
            if (m.size > ZIP_LIMIT - *bytes) return archive_message(ARC_ZIP_TOTAL_OUTPUT_EXCEEDS_DOS_LIMITS);
            ++*files; *bytes += m.size;
        }
        before = a->start;
        for (j = 0; j < i; ++j) {
            if (!member(a, before, &old)) return 0;
            n = (unsigned)strlen(m.name);
            if (!strcmp(m.name, old.name) ||
                (!m.directory && !strncmp(m.name, old.name, (unsigned)n) && old.name[n] == SEP) ||
                (!old.directory && !strncmp(old.name, m.name, strlen(old.name)) && m.name[strlen(old.name)] == SEP))
                return archive_message(ARC_ZIP_DUPLICATE_OR_CONFLICTING_DOS_NAMES);
            if (m.local < old.finish && old.local < m.finish) return archive_message(ARC_ZIP_OVERLAPPING_ENTRIES);
            before = old.next;
            if ((j & 31) == 31 && !archive_poll(m.name, 0, 0)) return 0;
        }
        at = m.next;
    }
    if (at != a->end) return archive_message(ARC_ZIP_DIRECTORY_COUNT_OR_LENGTH_MISMATCH);
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
        if (status != Z_OK) return archive_message(ARC_ZIP_INSUFFICIENT_DECOMPRESSION_MEMORY);
        initialized = 1;
    }
    for (;;) {
        n = 0;
        if (!stream.avail_in && left) {
            n = left > sizeof(input) ? sizeof(input) : (unsigned)left;
            if (!archive_read_at(a, offset, input, n)) { ok = 0; break; }
            offset += n; left -= n; stream.next_in = input; stream.avail_in = n;
        }
        if (m->method == 8) {
            unsigned previous = stream.avail_in;
            stream.next_out = output; stream.avail_out = sizeof(output);
            status = inflate(&stream, Z_NO_FLUSH);
            produced = sizeof(output) - stream.avail_out;
            if (status != Z_OK && status != Z_STREAM_END) {
                ok = archive_message(status == Z_MEM_ERROR ? ARC_ZIP_INSUFFICIENT_DECOMPRESSION_MEMORY : ARC_ZIP_INVALID_OR_TRUNCATED_DEFLATE_STREAM); break;
            }
            if (status != Z_STREAM_END && !produced && previous == stream.avail_in) {
                ok = archive_message(ARC_ZIP_INCOMPLETE_DEFLATE_STREAM); break;
            }
        } else {
            produced = stream.avail_in; memcpy(output, input, produced); stream.avail_in = 0;
            if (!left) status = Z_STREAM_END;
        }
        if (produced > m->size - written) { ok = archive_message(ARC_ZIP_OUTPUT_EXCEEDS_DECLARED_SIZE); break; }
        if (!write_all(out, output, produced)) { ok = 0; break; }
        checksum = crc32(checksum, output, produced); written += produced;
        operation_current_bytes = written; operation_transferred += produced;
        if (!archive_poll(m->name, written, m->size)) { ok = 0; break; }
        if (status == Z_STREAM_END) break;
    }
    if (initialized) inflateEnd(&stream);
    if (ok && (left || stream.avail_in || written != m->size || checksum != m->crc))
        ok = archive_message(ARC_ZIP_CRC_SIZE_OR_COMPRESSED_LENGTH_MISMATCH);
    return ok;
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
        ok = member(&a, at, &m) && archive_extract_member(&a, &m, unpack); at = m.next;
    }
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ZIP"); ok = 0; }
    operation_current_bytes = 0;
    return ok;
}

int zip_walk(const char *source, ArchiveVisit visit, void *context)
{
    Archive a;
    Member m;
    unsigned i;
    unsigned long files, bytes, at;
    int ok = open_archive(&a,source,NULL);
    if (ok) ok = preflight(&a,&files,&bytes);
    at = ok ? a.start : 0;
    for (i = 0; ok && i < a.count; ++i) {
        ok = member(&a,at,&m);
        if (ok) ok = visit(&a,&m,unpack,context);
        at = m.next;
    }
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ZIP"); ok = 0; }
    return ok;
}
