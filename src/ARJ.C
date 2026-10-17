/* SPDX-License-Identifier: GPL-3.0-only */
/* Sequential ARJ reader with a complete bounded preflight before output.
 * Name validation and staged replacement follow the ZIP reader's policy. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "ARJ.H"
#include "ARJDEC.H"
#include "zlib.h"
#define ARJ_LIMIT 2147483647UL
#include "ARCHIVE.H"
/* Unix producers store epoch seconds. Convert to DOS UTC without the runtime
 * timezone/environment machinery; clamp dates before DOS's 1980 epoch. */
static void unix_stamp(Entry *e, unsigned long stamp)
{
    unsigned long days = stamp / 86400UL, seconds = stamp % 86400UL;
    unsigned year = 1970, month = 1, length, leap;
    for (;;) {
        leap = !(year % 4) && ((year % 100) || !(year % 400));
        length = leap ? 366 : 365;
        if (days < length) break;
        days -= length; ++year;
    }
    while (month < 12) {
        length = month == 2 ? 28+leap :
                 (month == 4 || month == 6 || month == 9 || month == 11) ? 30 : 31;
        if (days < length) break;
        days -= length; ++month;
    }
    if (year < 1980) { e->date = 33; e->time = 0; return; }
    e->date = ((year-1980) << 9) | (month << 5) | (unsigned)(days+1);
    e->time = ((unsigned)(seconds/3600UL) << 11) |
              ((unsigned)(seconds/60UL % 60) << 5) | (unsigned)(seconds%60)/2;
}
/* Stream header CRCs, including comments, without allocating from their size.
 * Extended headers are deliberately outside the initial supported subset. */
static int member(Archive *a, unsigned long at, Member *m, int main_header)
{
    unsigned char h[34], buf[128];
    unsigned length, fixed, n, i, name_length = 0, type;
    unsigned long offset, left, checksum = 0;
    int name_done = 0, comment_done = 0;
    memset(m,0,sizeof(*m));
    if (!archive_read_at(a,at,h,4) || archive_word(h) != 0xea60U) return archive_message(ARC_ARJ_INVALID_HEADER_SIGNATURE);
    length = archive_word(h+2); m->next = at+4;
    if (!length) {
        m->end = 1;
        if (main_header || m->next != a->size) return archive_message(ARC_ARJ_INVALID_END_RECORD_OR_TRAILING_DATA);
        return 1;
    }
    if (length < 32 || length > 2600 || !archive_read_at(a,at+4,h,30))
        return archive_message(ARC_ARJ_INVALID_HEADER_SIZE);
    fixed = h[0]; type = h[6];
    if (fixed < 30 || fixed > length-2 || h[2] > 3 || h[28] || h[29] ||
        (h[4] & ~(main_header ? 0x10U : 0x30U)))
        return archive_message(ARC_ARJ_SPLIT_ENCRYPTED_SECURED_OR_NEWER_FORMAT_UNSUPPORTED);
    if (main_header ? type != 2 : type != 0 && type != 3)
        return archive_message(ARC_ARJ_ONLY_BINARY_FILES_AND_DIRECTORIES_SUPPORTED);
    m->method = h[5]; m->directory = type == 3;
    m->info.time = archive_word(h+8); m->info.date = archive_word(h+10);
    if (h[3] == 2 || h[3] == 8) unix_stamp(&m->info,archive_dword(h+8));
    m->packed = archive_dword(h+12); m->size = archive_dword(h+16); m->crc = archive_dword(h+20);
    m->info.attr = h[3] == 0 ? h[26] & (NW_READONLY | NW_HIDDEN | NW_SYSTEM | 32) : 32;
    if (!main_header && (h[3] == 2 || h[3] == 8) && (archive_word(h+26) & 0x4000))
        return archive_message(ARC_ARJ_SPECIAL_FILES_UNSUPPORTED);
    m->info.size = m->size;
    if (!main_header && (m->method > 4 || m->size > ARJ_LIMIT || m->packed > ARJ_LIMIT ||
        (!m->method && m->packed != m->size) ||
        (m->directory && (m->size || m->packed || m->crc))))
        return archive_message(ARC_ARJ_UNSUPPORTED_METHOD_OR_INVALID_SIZES);
    offset = at+4; left = length;
    while (left) {
        n = left > sizeof(buf) ? sizeof(buf) : (unsigned)left;
        if (!archive_read_at(a,offset,buf,n)) return 0;
        checksum = crc32(checksum,buf,n);
        for (i = 0; i < n; ++i) {
            if (offset+i < at+4+fixed) continue;
            if (!name_done) {
                if (!buf[i]) name_done = 1;
                else {
                    if (name_length == NW_PATH-1) return archive_message(ARC_ARJ_FILENAME_IS_TOO_LONG);
                    m->name[name_length++] = (char)buf[i];
                }
            } else if (!buf[i]) comment_done = 1;
        }
        offset += n; left -= n;
    }
    if (!name_done || !comment_done || !archive_read_at(a,offset,buf,6))
        return archive_message(ARC_ARJ_UNTERMINATED_FILENAME_OR_COMMENT);
    if (archive_dword(buf) != checksum || archive_word(buf+4)) return archive_message(ARC_ARJ_HEADER_CRC_OR_UNSUPPORTED_EXTENDED_HEADER);
    m->data = offset+6;
    if (main_header) { m->next = m->data; return 1; }
    /* The shared policy derives directory status from a trailing separator;
     * ARJ instead records the directory type in the fixed header. */
    {
        unsigned char raw[NW_PATH];
        unsigned directory = m->directory;
        memcpy(raw,m->name,name_length);
        if (!archive_member_name(m,raw,name_length)) return 0;
        if (m->directory && !directory) return archive_message(ARC_ARJ_INCONSISTENT_DIRECTORY_NAME);
        m->directory = directory;
    }
    if (m->data > a->size || m->packed > a->size-m->data) return archive_message(ARC_ARJ_INVALID_DATA_BOUNDS);
    m->next = m->data+m->packed;
    return 1;
}
static int open_archive(Archive *a, const char *source, const char *target)
{
    char dir[NW_PATH], filename[NW_PATH], *cut;
    Member main_header;
    long size;
    a->fd = -1;
    a->target[0] = 0;
    if (strlen(source) >= NW_PATH || (target && !fs_canonical(target,a->target)))
        return archive_message(ARC_ARJ_DESTINATION_MUST_BE_AN_EXISTING_DIRECTORY);
    strcpy(dir,source); cut = strrchr(dir,SEP);
    if (cut) {
        strcpy(filename,cut+1); *cut = 0;
        if (!dir[0]) strcpy(dir,"/");
    } else {
        if (strchr(source,':')) return archive_message(ARC_ARJ_USE_AN_ABSOLUTE_DRIVE_PATH);
        strcpy(filename,source); strcpy(dir,".");
    }
#ifndef NW_HOST
    if (strlen(dir) == 2 && dir[1] == ':') strcat(dir,"\\");
#endif
    if (!fs_canonical(dir,a->source)) return archive_message(ARC_ARJ_INVALID_ARCHIVE_DIRECTORY);
    strcpy(dir,a->source);
    if (!path_join(a->source,dir,filename)) return 0;
    a->fd = fs_read_open(source);
    if (a->fd < 0) { fs_error("Open ARJ"); return 0; }
    size = fs_seek(a->fd,0L,SEEK_END);
    if (size < 0) { fs_error("Seek ARJ"); return 0; }
    a->size = (unsigned long)size;
    if (a->size > ARJ_LIMIT || !member(a,0,&main_header,1)) return 0;
    a->start = main_header.next;
    return 1;
}
static int preflight(Archive *a, unsigned long *files, unsigned long *bytes)
{
    Member m, old;
    unsigned long at = a->start, before;
    unsigned n, count = 0;
    *files = *bytes = 0;
    for (;;) {
        if (!member(a,at,&m,0)) return 0;
        if (m.end) return 1;
        if (++count == 65535U) return archive_message(ARC_ARJ_TOO_MANY_ENTRIES);
        if ((a->target[0] && !archive_directories(a,&m,0)) || !archive_poll(m.name,0,0)) return 0;
        if (!m.directory) {
            if (m.size > ARJ_LIMIT-*bytes) return archive_message(ARC_ARJ_TOTAL_OUTPUT_EXCEEDS_DOS_LIMITS);
            ++*files; *bytes += m.size;
        }
        before = a->start;
        while (before < at) {
            if (!member(a,before,&old,0)) return 0;
            n = (unsigned)strlen(m.name);
            if (!strcmp(m.name,old.name) ||
                (!m.directory && !strncmp(m.name,old.name,n) && old.name[n] == SEP) ||
                (!old.directory && !strncmp(old.name,m.name,strlen(old.name)) && m.name[strlen(old.name)] == SEP))
                return archive_message(ARC_ARJ_DUPLICATE_OR_CONFLICTING_DOS_NAMES);
            before = old.next;
            if (!archive_poll(m.name,0,0)) return 0;
        }
        at = m.next;
    }
}
int arj_measure(const char *archive, const char *target, unsigned long *files, unsigned long *bytes)
{
    Archive a;
    unsigned long count = 0, size = 0;
    int ok = open_archive(&a,archive,target);
    if (ok) ok = preflight(&a,&count,&size);
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ARJ"); ok = 0; }
    if (ok) { *files = count; *bytes = size; }
    return ok;
}
static int unpack(Archive *a, Member *m, int out)
{
    if (fs_seek(a->fd,(long)m->data,SEEK_SET) < 0) { fs_error("Seek ARJ"); return 0; }
    return arj_decode(a->fd,out,m->method,m->packed,m->size,m->crc,m->name);
}
int arj_extract(const char *archive, const char *target)
{
    Archive a;
    Member m;
    unsigned long files, bytes, at;
    int ok = open_archive(&a,archive,target);
    if (ok) ok = preflight(&a,&files,&bytes);
    at = ok ? a.start : 0;
    while (ok) {
        if (!member(&a,at,&m,0)) { ok = 0; break; }
        if (m.end) break;
        ok = archive_extract_member(&a,&m,unpack); at = m.next;
    }
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ARJ"); ok = 0; }
    operation_current_bytes = 0;
    return ok;
}

int arj_walk(const char *source, ArchiveVisit visit, void *context)
{
    Archive a;
    Member m;
    unsigned long files, bytes, at;
    int ok = open_archive(&a,source,NULL);
    if (ok) ok = preflight(&a,&files,&bytes);
    at = ok ? a.start : 0;
    while (ok) {
        if (!member(&a,at,&m,0)) { ok = 0; break; }
        if (m.end) break;
        ok = visit(&a,&m,unpack,context); at = m.next;
    }
    if (a.fd >= 0 && fs_close(a.fd) < 0 && ok) { fs_error("Close ARJ"); ok = 0; }
    return ok;
}
