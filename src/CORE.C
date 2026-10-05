/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Shared panel model and file operations.  All filesystem access goes through
 * fs_* so the DOS program and host tests exercise the same decisions.
 * Operations are synchronous; callback and sort state is not reentrant.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "NW.H"
Panel panes[2];
char nw_error[160];
/* Reuse one near-data buffer instead of spending stack space at each depth. */
static char copy_buffer[2048];
int (*operation_progress)(const char *, unsigned long, unsigned long);
int (*operation_conflict)(const char *, const char *);
unsigned long operation_files, operation_skipped;
static int sort_order, sort_reverse;

/* Join a directory and one name into an NW_PATH-sized, distinct output
 * buffer.  Reject overflow before writing; report failure in nw_error. */
int path_join(char *out, const char *dir, const char *name)
{
    unsigned n = strlen(dir), m = strlen(name);
    int slash = n && dir[n-1] != '\\' && dir[n-1] != '/';
    if (n + m + slash >= NW_PATH) {
        strcpy(nw_error, "Path is too long"); return 0;
    }
    strcpy(out, dir);
#ifdef NW_HOST
    if (slash) strcat(out, "/");
#else
    if (slash) strcat(out, "\\");
#endif
    strcat(out, name); return 1;
}
/* Keep the synthetic parent first, then directories, regardless of reverse
 * order.  Apply the selected key only within these groups; names break ties.
 * sort_order and sort_reverse are set immediately before each qsort call. */
static int compare(const void *a, const void *b)
{
    const Entry *x = a, *y = b;
    if (!strcmp(x->name, "..")) return strcmp(y->name, "..") ? -1 : 0;
    if (!strcmp(y->name, "..")) return 1;
    if ((x->attr & NW_DIR) != (y->attr & NW_DIR))
        return (x->attr & NW_DIR) ? -1 : 1;
    {
        int value = 0;
        if (sort_order == 1) value = x->size < y->size ? -1 : x->size > y->size;
        else if (sort_order == 2) {
            value = x->date < y->date ? -1 : x->date > y->date;
            if (!value) value = x->time < y->time ? -1 : x->time > y->time;
        } else if (sort_order == 3) {
            const char *a = strrchr(x->name, '.'), *b = strrchr(y->name, '.');
            value = strcmp(a ? a + 1 : "", b ? b + 1 : "");
        }
        if (!value) value = strcmp(x->name, y->name);
        return sort_reverse ? -value : value;
    }
}
/* Reload and sort one panel, preserving the cursor by filename when possible.
 * A successful reload clears marks and scrolling and invalidates render caches. */
int panel_load(Panel *p)
{
    char selected[13];
    selected[0] = 0;
    if (p->cursor >= 0 && p->cursor < p->count)
        strcpy(selected, p->files[p->cursor].name);
    if (!fs_list(p)) return 0;
    sort_order = p->sort; sort_reverse = p->reverse;
    qsort(p->files, p->count, sizeof(Entry), compare);
    p->cursor = 0;
    if (selected[0]) {
        int i;
        for (i = 0; i < p->count; ++i)
            if (!strcmp(selected, p->files[i].name)) { p->cursor = i; break; }
    }
    p->top = p->marks = 0; ++p->revision; return 1;
}
/*
 * Reuse a freshly loaded directory listing without another disk scan.  Copy
 * entry data, but retain the receiving panel's sort and selection preferences.
 * Marks belong to each panel and are cleared rather than shared.
 */
void panel_snapshot(Panel *p, const Panel *source)
{
    char selected[13];
    int i;
    if (p == source) return;
    selected[0] = 0;
    if (p->cursor >= 0 && p->cursor < p->count)
        strcpy(selected, p->files[p->cursor].name);
    memcpy(p->files, source->files, source->count * sizeof(Entry));
    p->count = source->count; p->truncated = source->truncated;
    if (p->sort != source->sort || p->reverse != source->reverse) {
        sort_order = p->sort; sort_reverse = p->reverse;
        qsort(p->files, p->count, sizeof(Entry), compare);
    }
    p->cursor = p->top = p->marks = 0;
    for (i = 0; i < p->count; ++i) {
        p->files[i].marked = 0;
        if (!strcmp(selected, p->files[i].name)) p->cursor = i;
    }
    ++p->revision;
}
/* Change one mark and the cached mark count together.  The parent entry is
 * never markable; unchanged or invalid requests do not invalidate the panel. */
void panel_mark(Panel *p, int index, int marked)
{
    Entry *e;
    if (index < 0 || index >= p->count) return;
    e = &p->files[index]; marked = marked != 0;
    if (!strcmp(e->name, "..") || e->marked == marked) return;
    e->marked = marked; p->marks += marked ? 1 : -1; ++p->revision;
}
/* Set all eligible marks and rebuild the count.  Bump the revision only
 * when an entry changes, so repeated requests do not force a repaint. */
void panel_mark_all(Panel *p, int marked)
{
    int i, changed = 0;
    marked = marked != 0;
    p->marks = 0;
    for (i = 0; i < p->count; ++i) {
        int value = marked && strcmp(p->files[i].name, "..");
        if (p->files[i].marked != value) changed = 1;
        p->files[i].marked = value; p->marks += value;
    }
    if (changed) ++p->revision;
}
/* Reorder existing entries, retaining their marks and selected filename.
 * The caller supplies a valid cursor when the panel is nonempty. */
void panel_sort(Panel *p, int order, int reverse)
{
    char selected[13];
    int i;
    selected[0] = 0;
    if (p->count) strcpy(selected, p->files[p->cursor].name);
    p->sort = order; p->reverse = reverse != 0;
    sort_order = order; sort_reverse = p->reverse;
    qsort(p->files, p->count, sizeof(Entry), compare);
    for (i = 0; i < p->count; ++i)
        if (!strcmp(selected, p->files[i].name)) { p->cursor = i; break; }
    p->top = 0; ++p->revision;
}
/* Match case-insensitive DOS-style wildcards without recursion.  The last
 * star records a retry point; advance it until the remaining suffix matches.
 * Treat *.* as * so extensionless names match the usual DOS all-files mask. */
int name_match(const char *pattern, const char *name)
{
    const char *star = NULL, *retry = NULL;
    if (!strcmp(pattern, "*.*")) pattern = "*";
    while (*name) {
        if (*pattern == '?' || toupper((unsigned char)*pattern) == toupper((unsigned char)*name)) {
            ++pattern; ++name;
        } else if (*pattern == '*') { star = ++pattern; retry = name; }
        else if (star) { pattern = star; name = ++retry; }
        else return 0;
    }
    while (*pattern == '*') ++pattern;
    return !*pattern;
}
/* Find the next matching non-parent entry, wrapping once from the cursor.
 * An empty panel performs no search; failure leaves the cursor unchanged. */
int panel_find(Panel *p, const char *pattern)
{
    int n, i;
    for (n = 1; n <= p->count; ++n) {
        i = (p->cursor + n) % p->count;
        if (strcmp(p->files[i].name, "..") && name_match(pattern, p->files[i].name)) {
            p->cursor = i; return 1;
        }
    }
    return 0;
}
/* Apply a wildcard to panel marks through the normal count/revision helper. */
void panel_pattern(Panel *p, const char *pattern, int mark)
{
    int i;
    for (i = 0; i < p->count; ++i)
        if (name_match(pattern, p->files[i].name)) panel_mark(p, i, mark);
}
/* Enter an existing directory; fs_canonical attempts to restore DOS drive state.
 * On failure, attempt to reload the old panel and restore its cursor.
 * Returning to a parent selects the child directory just left. */
int panel_enter(Panel *p, const char *name)
{
    char path[NW_PATH], full[NW_PATH], old[NW_PATH];
    int old_cursor = p->cursor, parent = !strcmp(name, "..");
    char child[13], *cut;
    child[0] = 0;
    if (parent) {
        cut = strrchr(p->path, '\\');
#ifdef NW_HOST
        cut = strrchr(p->path, '/');
#endif
        if (cut) strcpy(child, cut + 1);
    }
    if (!path_join(path, p->path, name) || !fs_canonical(path, full)) return 0;
    strcpy(old, p->path); strcpy(p->path, full);
    if (!panel_load(p)) { strcpy(p->path, old); panel_load(p); p->cursor = old_cursor; return 0; }
    p->cursor = p->top = 0;
    if (parent && child[0]) {
        int i;
        for (i = 0; i < p->count; ++i)
            if (!strcmp(p->files[i].name, child)) { p->cursor = i; break; }
    }
    return 1;
}
/* Clamp selection and keep it inside a viewport of rows entries.  Empty
 * panels retain cursor zero; drawing code must still check the entry count. */
void panel_scroll(Panel *p, int rows)
{
    if (p->cursor >= p->count) p->cursor = p->count - 1;
    if (p->cursor < 0) p->cursor = 0;
    if (p->cursor < p->top) p->top = p->cursor;
    if (p->cursor >= p->top + rows) p->top = p->cursor - rows + 1;
}
/* Accept a single 8.3 filename, never a path or wildcard.  Reject DOS device
 * basenames even with an extension, since opening CON.TXT still opens CON. */
int valid_name(const char *name)
{
    unsigned base = 0, ext = 0;
    int dot = 0;
    char device[9];
    const unsigned char *s = (const unsigned char *)name;
    while (*s) {
        if (*s == '.') { if (dot || !base) return 0; dot = 1; }
        else {
            if (*s <= 32 || *s == 127 || strchr("\"/\\[]:;|=,+*?<>", *s)) return 0;
            if (dot) { if (++ext > 3) return 0; }
            else if (++base > 8) return 0;
        }
        ++s;
    }
    if (!base || (dot && !ext)) return 0;
    for (ext = 0; ext < base; ++ext) device[ext] = toupper((unsigned char)name[ext]);
    device[base] = 0;
    if (!strcmp(device, "CON") || !strcmp(device, "PRN") || !strcmp(device, "AUX") ||
        !strcmp(device, "NUL") || !strcmp(device, "CLOCK$")) return 0;
    if (base == 4 && device[3] >= '1' && device[3] <= '9' &&
        (!strncmp(device, "COM", 3) || !strncmp(device, "LPT", 3))) return 0;
    return 1;
}
/* Remove a staging file even if copied attributes made it readonly.  A
 * cleanup failure replaces the prior error with its recoverable pathname. */
static int discard_temporary(const char *path)
{
    /* Staged files may already carry the source's readonly attribute. */
    fs_attr(path, 32);
    if (fs_delete(path, 0)) return 1;
    sprintf(nw_error, "Temporary remains: %s", path); return 0;
}
/* Compare already-normalized paths using the platform case rules; this
 * is not a canonicalizer and does not resolve relative paths or separators. */
static int same_path(const char *a, const char *b)
{
#ifdef NW_HOST
    return !strcmp(a, b);
#else
    while (*a && *b && toupper((unsigned char)*a) == toupper((unsigned char)*b)) { ++a; ++b; }
    return *a == *b;
#endif
}
/* Copy one regular file to a new absolute destination without exposing a
 * partial destination.  Exclusive staging creation protects existing files;
 * preserve date/time and attributes before the final same-directory rename. */
int file_copy(const char *src, const char *dst)
{
    Entry source, exists;
    char tmp[NW_PATH], dir[NW_PATH], name[13];
    char *cut;
    int in, out = -1, n, i, ok = 1;
    unsigned long copied = 0;
    if (!fs_info(src, &source)) { fs_error("Read source"); return 0; }
    if (source.attr & NW_DIR) { strcpy(nw_error, "Directory copy is not supported; enter the directory first"); return 0; }
    if (fs_info(dst, &exists)) { strcpy(nw_error, "Destination exists; rename or remove it first"); return 0; }
    strcpy(dir, dst);
    cut = strrchr(dir, '\\');
#ifdef NW_HOST
    cut = strrchr(dir, '/');
#endif
    if (!cut) { strcpy(nw_error, "Destination must be an absolute path"); return 0; }
    cut[1] = 0;
    in = fs_read_open(src);
    if (in < 0) { fs_error("Open source"); return 0; }
    for (i = 0; i < 1000; ++i) {
        sprintf(name, "NW%04d.TMP", i);
        if (!path_join(tmp, dir, name)) break;
        if (same_path(tmp, dst)) continue;
        out = fs_create(tmp);
        if (out >= 0) break;
        /* Only retry collisions, not a full/unwritable destination. */
        if (!fs_info(tmp, &exists)) break;
    }
    if (out < 0) { fs_close(in); fs_error("Create temporary file"); return 0; }
    /* A short write is legal: finish this block before reading another. */
    while ((n = fs_read(in, copy_buffer, sizeof(copy_buffer))) > 0) {
        int at = 0, written;
        while (at < n) {
            written = fs_write(out, copy_buffer + at, n - at);
            if (written <= 0) { ok = 0; break; }
            at += written;
        }
        if (!ok) break;
        copied += n;
        if (operation_progress && !operation_progress(src, copied, source.size)) { ok = -1; break; }
    }
    /* ok is 1 for success, 0 for I/O failure, -1 for user cancellation. */
    if (n < 0 && ok == 1) ok = 0;
    /* Stamp the still-open handle; closing can itself reveal a write error. */
    if (ok == 1 && !fs_stamp(out, &source)) ok = 0;
    if (fs_close(out) < 0 && ok == 1) ok = 0;
    fs_close(in);
    if (ok != 1) {
        if (ok < 0) strcpy(nw_error, "Operation cancelled"); else fs_error("Copy failed");
        discard_temporary(tmp); return 0;
    }
    /* Do not propagate directory/volume flags; 32 is DOS archive. */
    if (!fs_attr(tmp, source.attr & (NW_READONLY | NW_HIDDEN | NW_SYSTEM | 32))) {
        fs_error("Preserve attributes"); discard_temporary(tmp); return 0;
    }
    if (!fs_rename(tmp, dst)) { fs_error("Commit copy"); discard_temporary(tmp); return 0; }
    return 1;
}
/* Try a rename first, then copy/delete for moves DOS cannot rename.  If
 * source deletion fails, retain the destination and report the deletion error. */
int file_move(const char *src, const char *dst)
{
    Entry e;
    if (fs_info(dst, &e)) { strcpy(nw_error, "Destination exists"); return 0; }
    if (fs_rename(src, dst)) return 1;
    if (!file_copy(src, dst)) return 0;
    if (!fs_delete(src, 0)) { fs_error("Copied, but source could not be removed"); return 0; }
    return 1;
}

/* Poll the optional synchronous cancellation callback and set a useful error. */
static int progress(const char *path, unsigned long done, unsigned long total)
{
    if (operation_progress && !operation_progress(path, done, total)) {
        strcpy(nw_error, "Operation cancelled"); return 0;
    }
    return 1;
}
/* Find an unused sibling 8.3 name for a replacement or backup.  This is a
 * probe, not a reservation; the operation assumes no concurrent DOS writer. */
static int unused_name(const char *dst, char *out, const char *extension)
{
    char dir[NW_PATH], name[13], *cut;
    Entry e;
    int i;
    strcpy(dir, dst);
    cut = strrchr(dir, '\\');
#ifdef NW_HOST
    cut = strrchr(dir, '/');
#endif
    if (!cut) { strcpy(nw_error, "Use an absolute destination path"); return 0; }
    cut[1] = 0;
    for (i = 0; i < 1000; ++i) {
        sprintf(name, "NW%04d.%s", i, extension);
        if (!path_join(out, dir, name)) return 0;
        if (!fs_info(out, &e)) return 1;
    }
    strcpy(nw_error, "No free temporary filename"); return 0;
}
/*
 * Finish the replacement before renaming the original to a sibling backup.
 * On commit failure, try to restore the original and remove staging data.
 * If restoration or cleanup fails, report the retained recovery filename.
 * A committed replacement can still return failure if its backup remains.
 */
static int replace_file(const char *src, const char *dst, const Entry *old)
{
    char staged[NW_PATH], backup[NW_PATH];
    int committed, restored, cleaned;
    if (!unused_name(dst, staged, "NEW")) return 0;
    if (!file_copy(src, staged)) return 0;
    if (!unused_name(dst, backup, "BAK")) { discard_temporary(staged); return 0; }
    if (!fs_rename(dst, backup)) {
        fs_error("Preserve original"); discard_temporary(staged); return 0;
    }
    committed = fs_rename(staged, dst);
    if (!committed) {
        restored = fs_rename(backup, dst); cleaned = discard_temporary(staged);
        if (!restored) {
            const char *name = strrchr(staged, '\\');
#ifdef NW_HOST
            name = strrchr(staged, '/');
#endif
            if (cleaned) sprintf(nw_error, "Original backup: %s", backup);
            else sprintf(nw_error, "Backup %s; temp %.12s", backup, name ? name + 1 : staged);
        } else if (cleaned) strcpy(nw_error, "Commit failed; original restored");
        return 0;
    }
    if (!fs_attr(backup, old->attr & ~NW_READONLY) || !fs_delete(backup, 0)) {
        fs_attr(backup, old->attr);
        sprintf(nw_error, "Original backup: %s", backup); return 0;
    }
    return 1;
}
/*
 * Walk a tree using shared path buffers and one search state per stack frame.
 * Append a child name, recurse, then restore both parent terminators.  Limit
 * directory depth to bound the 8088 stack: root depth is zero, directories
 * through 32 are allowed and files can be one level deeper. Preserve skips on moves.
 * Completed files are not rolled back if a later child fails.
 */
static int copy_tree(char *src, char *dst, int move, unsigned depth)
{
    Entry e, existing;
    FsSearch search;
    unsigned a = strlen(src), b = strlen(dst);
    unsigned long skipped = operation_skipped;
    int result, ok = 1, exists;
    if (!fs_info(src, &e)) { fs_error("Read source"); return 0; }
    if ((e.attr & NW_DIR) && depth > 32) { strcpy(nw_error, "Directory nesting exceeds 32 levels"); return 0; }
    if (!progress(src, 0, e.size)) return 0;
    exists = fs_info(dst, &existing);
    if (!(e.attr & NW_DIR)) {
        if (exists) {
            if (existing.attr & NW_DIR) { strcpy(nw_error, "File destination is a directory"); return 0; }
            result = operation_conflict ? operation_conflict(src, dst) : 0;
            if (result == 2) { ++operation_skipped; return 1; }
            if (result != 1) { strcpy(nw_error, "Destination exists or operation cancelled"); return 0; }
            ok = replace_file(src, dst, &existing);
            if (ok && move && !fs_delete(src, 0)) { fs_error("Remove moved source"); ok = 0; }
        } else ok = move ? file_move(src, dst) : file_copy(src, dst);
        if (ok) ++operation_files;
        return ok;
    }
    if (exists && !(existing.attr & NW_DIR)) { strcpy(nw_error, "Directory destination is a file"); return 0; }
    if (move && !exists && fs_rename(src, dst)) { ++operation_files; return 1; }
    if (!exists && !fs_mkdir(dst)) { fs_error("Create destination directory"); return 0; }
    result = fs_first(&search, src, &e);
    while (result > 0) {
        /* Append without overlapping strcpy/memcpy arguments. */
#ifdef NW_HOST
        const char separator = '/';
#else
        const char separator = '\\';
#endif
        unsigned n = strlen(e.name);
        if (a + n + 1 >= NW_PATH || b + n + 1 >= NW_PATH) {
            strcpy(nw_error, "Recursive path is too long"); ok = 0; break;
        }
        src[a] = separator; strcpy(src + a + 1, e.name);
        dst[b] = separator; strcpy(dst + b + 1, e.name);
        ok = copy_tree(src, dst, move, depth + 1);
        src[a] = dst[b] = 0;
        if (!ok) break;
        result = fs_next(&search, &e);
    }
    fs_end(&search);
    if (result < 0) ok = 0;
    /* Delay new-directory attributes until its children have been created. */
    if (ok && !exists) {
        if (!fs_info(src, &existing) || !fs_attr(dst, existing.attr & (NW_READONLY | NW_HIDDEN | NW_SYSTEM | 32))) {
            fs_error("Preserve directory attributes"); ok = 0;
        }
    }
    /* A skipped descendant leaves this source directory needed and nonempty. */
    if (ok && move && operation_skipped == skipped && !fs_delete(src, 1)) {
        fs_error("Remove moved directory"); ok = 0;
    }
    return ok;
}
/* Canonicalize the existing directory part, allowing a not-yet-created
 * final filename.  Existing files contribute the filesystem spelling of
 * their name; existing directories are canonicalized as a whole. */
static int canonical_target(const char *path, char *out)
{
    char parent[NW_PATH], full[NW_PATH], name[13], *cut;
    Entry e;
    int exists;
    if (strlen(path) >= NW_PATH) { strcpy(nw_error, "Path is too long"); return 0; }
    exists = fs_info(path, &e);
    if (exists && (e.attr & NW_DIR)) return fs_canonical(path, out);
    strcpy(parent, path); cut = strrchr(parent, '\\');
#ifdef NW_HOST
    cut = strrchr(parent, '/');
#endif
    if (!cut || !valid_name(cut + 1)) { strcpy(nw_error, "Invalid destination filename"); return 0; }
    strcpy(name, exists && e.name[0] ? e.name : cut + 1); cut[1] = 0;
    if (!fs_canonical(parent, full)) return 0;
    return path_join(out, full, name);
}
/* Copy or move a tree after canonicalizing both endpoints.  Reject equal
 * paths and ancestor/descendant relationships on directory boundaries,
 * preventing recursion into a destination or destruction of a source.
 * The caller owns callbacks and resets operation counters for a new batch. */
int tree_copy(const char *src, const char *dst, int move)
{
    char a[NW_PATH], b[NW_PATH];
    unsigned n, m, i;
    if (!canonical_target(src, a) || !canonical_target(dst, b)) return 0;
    n = strlen(a); m = strlen(b);
    for (i = 0; i < n && i < m; ++i) {
#ifdef NW_HOST
        if (a[i] != b[i]) break;
#else
        if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) break;
#endif
    }
    if ((i == n && (!b[n] || a[n-1] == '\\' || a[n-1] == '/' || b[n] == '\\' || b[n] == '/')) ||
        (i == m && (b[m-1] == '\\' || b[m-1] == '/' || a[m] == '\\' || a[m] == '/'))) {
        strcpy(nw_error, "Source and destination overlap"); return 0;
    }
    return copy_tree(a, b, move, 0);
}
/* Delete children before their directory, sharing one mutable path buffer
 * through bounded recursion.  Cancellation stops at the next progress poll;
 * earlier successful deletions remain committed. The root is depth zero;
 * directory depth is limited to 32, with files allowed one level deeper. */
static int delete_tree(char *path, unsigned depth)
{
    Entry e;
    FsSearch search;
    unsigned a = strlen(path);
    int result, ok = 1;
    if (!fs_info(path, &e)) { fs_error("Read deletion target"); return 0; }
    if ((e.attr & NW_DIR) && depth > 32) { strcpy(nw_error, "Directory nesting exceeds 32 levels"); return 0; }
    if (!progress(path, 0, e.size)) return 0;
    if (!(e.attr & NW_DIR)) {
        if (!fs_delete(path, 0)) { fs_error("Delete file"); return 0; }
        ++operation_files; return 1;
    }
    result = fs_first(&search, path, &e);
    while (result > 0) {
        if (a + strlen(e.name) + 1 >= NW_PATH) { strcpy(nw_error, "Recursive path is too long"); ok = 0; break; }
#ifdef NW_HOST
        path[a] = '/';
#else
        path[a] = '\\';
#endif
        strcpy(path + a + 1, e.name);
        ok = delete_tree(path, depth + 1); path[a] = 0;
        if (!ok) break;
        result = fs_next(&search, &e);
    }
    fs_end(&search);
    if (result < 0) ok = 0;
    if (ok && !fs_delete(path, 1)) { fs_error("Delete directory"); ok = 0; }
    return ok;
}
/* Canonicalize a deletion target and explicitly protect DOS drive roots
 * (and the host-test root) before beginning irreversible recursive removal. */
int tree_delete(const char *path)
{
    char full[NW_PATH];
    if (!canonical_target(path, full)) return 0;
    if (!strcmp(full, "/") || (strlen(full) == 3 && full[1] == ':')) {
        strcpy(nw_error, "Cannot delete a drive root"); return 0;
    }
    return delete_tree(full, 0);
}
