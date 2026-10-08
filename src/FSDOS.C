/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * Turbo C / DOS filesystem adapter.  Entry dates and times remain in DOS
 * packed format, and attributes use DOS bits without translation.  Binary
 * handle I/O avoids text-mode CR/LF conversion and Ctrl-Z end-of-file rules.
 */
#include <dos.h>
#include <alloc.h>
#include <dir.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "NW.H"
#ifdef NW_DIAGNOSTICS
unsigned fs_list_calls;
#endif

/* Format the current C-library errno within the fixed shared error buffer.
 * Call after failing C-library I/O, before unrelated calls can change errno.
 * Raw intdos failures (fs_stamp) do not translate their DOS error into errno. */
void fs_error(const char *action)
{
    sprintf(nw_error, "%.65s: %.80s", action, strerror(errno));
}
/* Resolve an existing directory using DOS itself.  DOS stores a separate
 * current directory per drive: save the original drive/directory and also
 * the target drive directory, then attempt to restore both after the chdir.
 * Restoration errors are unchecked; success describes resolution, not cleanup.
 * out has NW_PATH bytes and may overlap the input (memmove is intentional). */
int fs_canonical(const char *path, char *out)
{
    char old[NW_PATH], other[NW_PATH];
    int drive = getdisk(), target = drive, ok;
    unsigned n = strlen(path);
    if (n >= NW_PATH) { strcpy(nw_error, "Path is too long"); return 0; }
    if (!getcwd(old, sizeof(old))) { fs_error("Current directory"); return 0; }
    if (path[0] && path[1] == ':') {
        int d = path[0];
        if (d >= 'a' && d <= 'z') d -= 32;
        if (d < 'A' || d > 'Z') { strcpy(nw_error, "Invalid drive"); return 0; }
        target = d - 'A'; setdisk(target);
        if (getdisk() != target || !getcwd(other, sizeof(other))) {
            setdisk(drive); strcpy(nw_error, "Cannot access drive"); return 0;
        }
    }
    memmove(out, path, n + 1);
    while (n > 1 && (out[n-1] == '\\' || out[n-1] == '/') && !(n == 3 && out[1] == ':')) out[--n] = 0;
    ok = chdir(out) == 0;
    if (ok) ok = getcwd(out, NW_PATH) != NULL;
    if (!ok) fs_error("Change directory");
    /* Restore target first; old names the original drive's full directory. */
    if (target != drive) chdir(other);
    setdisk(drive); chdir(old);
    return ok;
}
/* Convert one Turbo C search result and clear UI-owned fields such as marks. */
static void entry_from_ff(Entry *e, const struct ffblk *f)
{
    memset(e, 0, sizeof(*e));
    strcpy(e->name, f->ff_name); e->attr = f->ff_attrib;
    e->size = f->ff_fsize; e->date = f->ff_fdate; e->time = f->ff_ftime;
}
/* Load a bounded panel listing including hidden/system entries, but not
 * volume labels.  Synthesize one parent entry away from drive roots; mark
 * truncation when capacity is reached rather than overrunning near data.
 * Any findnext termination ends this bounded scan successfully; only the initial
 * search distinguishes errors. Recursive fs_next separately checks exhaustion. */
int fs_list(Panel *p)
{
    struct ffblk f;
    char spec[NW_PATH];
    int done;
    Entry dir;
#ifdef NW_DIAGNOSTICS
    ++fs_list_calls;
#endif
    if (!fs_info(p->path, &dir) || !(dir.attr & NW_DIR)) {
        fs_error("Read directory"); return 0;
    }
    if (!path_join(spec, p->path, "*.*")) return 0;
    done = findfirst(spec, &f, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIREC | FA_ARCH);
    /* DOS returns file-not-found for an empty directory. */
    if (done && errno != ENOENT) { fs_error("Read directory"); return 0; }
    p->count = p->truncated = 0;
    if (strlen(p->path) > 3) {
        Entry *e = &p->files[p->count++];
        memset(e, 0, sizeof(*e)); strcpy(e->name, ".."); e->attr = NW_DIR;
    }
    while (!done) {
        if (strcmp(f.ff_name, ".") && strcmp(f.ff_name, "..")) {
            if (p->count == NW_FILES) { p->truncated = 1; break; }
            entry_from_ff(&p->files[p->count++], &f);
        }
        done = findnext(&f);
    }
    return 1;
}
/* Query a file or directory.  DOS findfirst cannot describe a drive root,
 * so verify it through canonicalization and synthesize its directory entry. */
int fs_info(const char *path, Entry *e)
{
    struct ffblk f;
    if (strlen(path) == 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')) {
        char full[NW_PATH];
        if (!fs_canonical(path, full)) return 0;
        memset(e, 0, sizeof(*e)); e->attr = NW_DIR; return 1;
    }
    if (findfirst(path, &f, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIREC | FA_ARCH)) return 0;
    entry_from_ff(e, &f); return 1;
}
/* Create one directory; these mutators use nonzero success, unlike DOS/C. */
int fs_mkdir(const char *path) { return mkdir(path) == 0; }
/* Remove one file or an empty directory; recursion belongs to CORE.C. */
int fs_delete(const char *path, int directory) { return (directory ? rmdir(path) : unlink(path)) == 0; }
/* Rename within DOS filesystem rules; cross-drive moves need copy/delete. */
int fs_rename(const char *src, const char *dst) { return rename(src, dst) == 0; }
/* Open a binary read handle.  Negative handles report failure via errno. */
int fs_read_open(const char *path) { return open(path, O_RDONLY | O_BINARY); }
/* Create exclusively: existing data must never be truncated by staging I/O. */
int fs_create(const char *path) { return open(path, O_WRONLY | O_CREAT | O_EXCL | O_BINARY, 0666); }
/* Read up to count bytes: zero is EOF, negative is an I/O error. */
int fs_read(int fd, void *data, unsigned count) { return read(fd, data, count); }
/* Return bytes written, which may be fewer than count; callers complete
 * short writes and treat zero as failure when data remains. */
int fs_write(int fd, const void *data, unsigned count) { return write(fd, data, count); }
/* Turbo C owns the remaining conventional memory through its far heap.
 * Normalize DS:DX so a complete 16 KiB transfer cannot wrap a segment. */
void fs_copy_buffer_init(FsCopyBuffer *buffer, void *fallback, unsigned size)
{
    unsigned offset;
    buffer->allocation = farmalloc(16384UL);
    buffer->data = fallback; buffer->size = size;
    if (buffer->allocation) {
        offset = FP_OFF(buffer->allocation);
        buffer->data = MK_FP(FP_SEG(buffer->allocation) + (offset >> 4), offset & 15);
        buffer->size = 16384;
    }
}
void fs_copy_buffer_free(FsCopyBuffer *buffer)
{
    int saved = errno;
    if (buffer->allocation) farfree(buffer->allocation);
    buffer->allocation = NULL;
    errno = saved;
}
/* The ordinary C read/write functions accept near pointers in this model.
 * Copy buffers instead use DOS handle I/O with an explicit far DS:DX. DOS
 * file errors here map to the same C errno values used by the other wrappers. */
static int copy_io(int fd, void far *data, unsigned count, unsigned function)
{
    union REGS r;
    struct SREGS s;
    segread(&s); s.ds = FP_SEG(data);
    r.x.ax = function; r.x.bx = fd; r.x.cx = count; r.x.dx = FP_OFF(data);
    intdosx(&r, &r, &s);
    if (!r.x.cflag) return r.x.ax;
    _doserrno = r.x.ax;
    switch (r.x.ax) {
        case 5: errno = EACCES; break;
        case 6: errno = EBADF; break;
        case 8: errno = ENOMEM; break;
        case 19: errno = EROFS; break;
        default: errno = EIO; break;
    }
    return -1;
}
int fs_copy_read(int fd, FsCopyBuffer *buffer)
{
    return copy_io(fd, buffer->data, buffer->size, 0x3f00);
}
int fs_copy_write(int fd, FsCopyBuffer *buffer, unsigned offset, unsigned count)
{
    return copy_io(fd, buffer->data + offset, count, 0x4000);
}
/* Close a DOS handle; preserve the C-library zero-success convention. */
int fs_close(int fd) { return close(fd); }
/* Set the open file timestamp with INT 21h/AH=57h, AL=01h.  CX holds
 * packed time and DX packed date; DOS carry reports failure directly.
 * This wrapper does not update errno from the DOS error code in AX. */
int fs_stamp(int fd, const Entry *e)
{
    union REGS r;
    r.x.ax = 0x5701; r.x.bx = fd; r.x.cx = e->time; r.x.dx = e->date;
    intdos(&r, &r); return !r.x.cflag;
}
/* Set DOS attribute bits using Turbo C _chmod mode 1; nonnegative succeeds. */
int fs_attr(const char *path, unsigned attr) { return _chmod(path, 1, attr) >= 0; }

/* Fail compilation if Turbo C search state outgrows FsSearch.state. */
typedef char SearchSizeCheck[sizeof(struct ffblk) <= 48 ? 1 : -1];

/*
 * Turbo C keeps DOS findfirst/findnext continuation data in ffblk.  Save
 * the whole image per recursive search so child enumeration cannot consume
 * its parent's position.  ENOENT denotes a normal exhausted search.
 */
static int search_entry(FsSearch *search, struct ffblk *f, int done, Entry *e)
{
    while (!done && (!strcmp(f->ff_name, ".") || !strcmp(f->ff_name, ".."))) done = findnext(f);
    if (done) {
        if (errno == ENOENT) return 0;
        fs_error("Enumerate directory"); return -1;
    }
    memcpy(search->state, f, sizeof(*f)); entry_from_ff(e, f); return 1;
}
/* Start a caller-owned search: return 1 for an entry, 0 for exhaustion,
 * or -1 for an error.  Entries . and .. are filtered by search_entry. */
int fs_first(FsSearch *search, const char *dir, Entry *e)
{
    char spec[NW_PATH];
    struct ffblk f;
    int done;
    if (!path_join(spec, dir, "*.*")) return -1;
    done = findfirst(spec, &f, FA_RDONLY | FA_HIDDEN | FA_SYSTEM | FA_DIREC | FA_ARCH);
    return search_entry(search, &f, done, e);
}
/* Resume from this search's saved DTA rather than another nested search.
 * The return convention is the same as fs_first. */
int fs_next(FsSearch *search, Entry *e)
{
    struct ffblk f;
    int done;
    memcpy(&f, search->state, sizeof(f)); done = findnext(&f);
    return search_entry(search, &f, done, e);
}
/* DOS ffblk enumeration owns no open handle; keep the shared cleanup API. */
void fs_end(FsSearch *search) { (void)search; }
