/* SPDX-License-Identifier: GPL-3.0-only */
/* Shared bounded archive I/O, DOS path policy and per-file staged commit. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#define NW_ARCHIVE_TEXT
#include "ARCHIVE.H"
int archive_message(unsigned index)
{
    unsigned at = 0;
    const char NW_FAR *message;
#ifdef NW_HOST
    message = archive_messages[index];
#else
    message = asset_arcmsg(index);
#endif
    while (message[at]) { nw_error[at] = message[at]; ++at; }
    nw_error[at] = 0;
    return 0;
}
unsigned archive_word(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
unsigned long archive_dword(const unsigned char *p)
{ return archive_word(p) | ((unsigned long)archive_word(p + 2) << 16); }
int archive_poll(const char *name, unsigned long done, unsigned long total)
{
    if (operation_progress && !operation_progress(name, done, total))
        return archive_message(ARC_OPERATION_CANCELLED);
    return 1;
}
int archive_read_at(Archive *a, unsigned long at, void *data, unsigned n)
{
    unsigned have = 0;
    int got;
    if (at > a->size || n > a->size - at)
        return archive_message(ARC_ARCHIVE_TRUNCATED_RECORD);
    if (fs_seek(a->fd, (long)at, SEEK_SET) < 0) { fs_error("Seek archive"); return 0; }
    while (have < n) {
        got = fs_read(a->fd, (char *)data + have, n - have);
        if (got < 0) { fs_error("Read archive"); return 0; }
        if (!got) return archive_message(ARC_ARCHIVE_UNEXPECTED_END_OF_FILE);
        have += got;
    }
    return 1;
}
int archive_equal_path(const char *a, const char *b)
{
    while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { ++a; ++b; }
    return *a == *b;
}
/* Reject NULs, non-ASCII, absolute paths, dot/device components and LFN loss.
 * Normalize both standard / and Windows PowerShell's legacy backslashes. */
int archive_member_name(Member *m, unsigned char *raw, unsigned length)
{
    unsigned i, at = 0, part = 0;
    char component[13];
    if (!length || length >= NW_PATH) return archive_message(ARC_ARCHIVE_FILENAME_IS_TOO_LONG);
    m->directory = raw[length - 1] == '/' || raw[length - 1] == '\\';
    for (i = 0; i < length; ++i) {
        unsigned char c = raw[i];
        if (c == '/' || c == '\\') {
            component[part] = 0;
            if (!valid_name(component)) return archive_message(ARC_ARCHIVE_REQUIRES_SAFE_DOS_8_3_NAMES);
            part = 0;
            if (i + 1 == length) break;
            m->name[at++] = SEP;
        } else {
            if (c < 33 || c >= 127 || part == 12)
                return archive_message(ARC_ARCHIVE_REQUIRES_ASCII_DOS_8_3_NAMES);
            component[part++] = (char)c;
            m->name[at++] = (char)toupper(c);
        }
    }
    if (!m->directory) {
        component[part] = 0;
        if (!valid_name(component)) return archive_message(ARC_ARCHIVE_REQUIRES_SAFE_DOS_8_3_NAMES);
    }
    m->name[at] = 0;
    return 1;
}
/* Verify existing directories resolve to the intended spelling/location. This
 * also rejects host-test symlinks; DOS has no filesystem symbolic links. */
int archive_directories(Archive *a, const Member *m, int create)
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
                    !archive_equal_path(path, resolved)
#endif
                    )
                    return archive_message(ARC_ARCHIVE_DESTINATION_ANCESTOR_IS_NOT_A_SAFE_DIRECTORY);
            } else if (create && !fs_mkdir(path)) { fs_error("Create archive directory"); return 0; }
            path[i] = c;
        }
        if (!c) break;
    }
    if (!m->directory && fs_info(path, &e) && (e.attr & NW_DIR))
        return archive_message(ARC_ARCHIVE_DESTINATION_IS_A_DIRECTORY);
    if (archive_equal_path(path, a->source)) return archive_message(ARC_ARCHIVE_CANNOT_REPLACE_ITS_SOURCE_ARCHIVE);
    return 1;
}
int archive_extract_member(Archive *a, Member *m, int (*unpack)(Archive *, Member *, int))
{
    char dst[NW_PATH], tmp[NW_PATH];
    Entry old;
    int exists, answer, fd, ok;
    operation_current_bytes = 0;
    if (!archive_poll(m->name, 0, m->size) || !archive_directories(a, m, 1)) return 0;
    if (m->directory) return 1;
    if (!path_join(dst, a->target, m->name)) return 0;
    exists = fs_info(dst, &old);
    if (exists) {
        if (!operation_conflict) return archive_message(ARC_ARCHIVE_DESTINATION_EXISTS_CTRL_U_OFFERS_OVERWRITE_SKIP);
        answer = operation_conflict ? operation_conflict(m->name, dst) : 0;
        if (answer == 2) { ++operation_skipped; operation_bytes += m->size; return 1; }
        if (answer != 1) return archive_message(ARC_OPERATION_CANCELLED);
    }
    if (!operation_temp(dst, tmp, "TMP")) return 0;
    fd = fs_create(tmp);
    if (fd < 0) { fs_error("Create archive temporary"); return 0; }
    ok = unpack(a, m, fd);
    if (ok && !fs_stamp(fd, &m->info)) { fs_error("Stamp archive output"); ok = 0; }
    if (fs_close(fd) < 0 && ok) { fs_error("Close archive output"); ok = 0; }
    if (ok && !fs_attr(tmp, m->info.attr)) { fs_error("Set archive attributes"); ok = 0; }
    if (!ok) { operation_discard(tmp); return 0; }
    if (!operation_commit(tmp, dst, exists ? &old : NULL)) return 0;
    ++operation_files; operation_bytes += m->size; operation_current_bytes = 0;
    return 1;
}
