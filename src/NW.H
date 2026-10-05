/* SPDX-License-Identifier: GPL-3.0-only */
/* NW.H - Shared panel model and filesystem boundary, with no video dependencies.
 * Counts and offsets use explicit long types where a 16-bit DOS int is too small.
 * Unless noted otherwise, action functions return nonzero for success and zero
 * for failure. nw_error carries user-facing errors from higher-level operations.
 */
#ifndef NW_H
#define NW_H
/* NW_PATH includes the terminating NUL; NW_FILES counts entries, including
 * the synthetic parent. Two directory caches dominate DGROUP usage; recursive
 * traversal uses FsSearch instead of these pane caches.
 */
#define NW_PATH 128
#define NW_FILES 512
#define NW_DIR 16
#define NW_HIDDEN 2
#define NW_SYSTEM 4
#define NW_READONLY 1
#define NW_COLS 80
#define NW_ROWS 30
#include "VERSION.H"

/* One DOS 8.3 directory entry. attr uses DOS attribute bits; marked is UI state.
 * date/time retain packed DOS words (year since 1980, two-second time units).
 */
typedef struct {
    char name[13];
    unsigned char attr, marked;
    unsigned long size;
    unsigned date, time;
} Entry;
/* Each pane owns its path, selection, sort and marks. top is the first visible
 * entry; cursor is an index, clamped by panel_scroll before dereferencing.
 * truncated flags omitted entries beyond NW_FILES, including the parent slot.
 */
typedef struct {
    char path[NW_PATH];
    Entry files[NW_FILES];
    int count, cursor, top, truncated, marks;
    /* Renderers rely on revisions: mutate entries through load/mark helpers. */
    unsigned revision;
    unsigned char sort, reverse;
} Panel;
extern Panel panes[2];
extern char nw_error[160];
#ifdef NW_DIAGNOSTICS
extern unsigned fs_list_calls;
#endif
/* out must be a distinct NW_PATH-byte buffer; inputs are NUL-terminated.
 * Joining checks capacity but does not canonicalize or validate the component.
 */
int path_join(char *out, const char *dir, const char *name);
/* Reload/sort from p->path, preserving selected filename when possible.
 * Successful loads clear marks/top and bump revision; failure is not a snapshot.
 */
int panel_load(Panel *p);
/* Copy a successful same-directory listing, preserving receiving sort/selection.
 * The receiving path is already set by the caller; marks clear independently.
 */
void panel_snapshot(Panel *p, const Panel *source);
/* Mutate marks through these helpers to keep marks and revision consistent.
 * The synthetic parent entry is never eligible for marking.
 */
void panel_mark(Panel *p, int index, int marked);
void panel_mark_all(Panel *p, int marked);
/* Enter a directory and load it; returning to a parent selects the departed child.
 * On load failure, attempt to reload the old path. fs_canonical does not leave
 * the process in the panel directory intentionally; DOS restoration is best effort.
 */
int panel_enter(Panel *p, const char *name);
/* Clamp cursor/top to show the cursor in a positive-sized viewport. Empty panes
 * use cursor zero but have no valid Entry to dereference.
 */
void panel_scroll(Panel *p, int rows);
/* Sort order is 0..3: name, size, date/time, extension. Parent/directories stay
 * first; marks and selection are retained. Nonempty panes need a valid cursor.
 */
void panel_sort(Panel *p, int order, int reverse);
/* Case-insensitive * and ? matching; *.* includes extensionless DOS filenames.
 */
int name_match(const char *pattern, const char *name);
/* Find after the cursor with one wrap; zero leaves selection unchanged.
 * Pattern marking uses the same matcher and skips the parent through helpers.
 */
int panel_find(Panel *p, const char *pattern);
void panel_pattern(Panel *p, const char *pattern, int mark);
/* Enumeration state is per search, so child recursion cannot overwrite its parent.
 * DOS stores an ffblk/DTA image here; the host test backend uses handle.
 * fs_first/fs_next return 1 for an entry, 0 for exhaustion, -1 for failure.
 * They omit . and ..; call fs_end after success, exhaustion or an error.
 * The DOS search has no resource to release, but host handles do.
 */
typedef struct { unsigned char state[48]; void *handle; } FsSearch;
int fs_first(FsSearch *search, const char *dir, Entry *e);
int fs_next(FsSearch *search, Entry *e);
void fs_end(FsSearch *search);
/* Progress returns zero to cancel. Conflict returns 1 overwrite, 2 skip, 0 cancel. */
extern int (*operation_progress)(const char *, unsigned long, unsigned long);
extern int (*operation_conflict)(const char *, const char *);
/* Per-command completed/skipped counts; caller resets them before a batch.
 * A file operation counts once; a whole-directory rename also counts once.
 * Directory creation/removal do not increment the completed count.
 */
extern unsigned long operation_files, operation_skipped;
/* Recursive operations enumerate beyond the pane cache. The root is depth zero;
 * directories through depth 32 are accepted, with files allowed one level deeper.
 * move removes source entries only after their copy/rename succeeds; cancellation
 * leaves earlier commits in place. tree_copy validates canonical paths/overlap
 * and supports overwrite/skip callbacks; tree_delete does not delete drive roots.
 */
int tree_copy(const char *src, const char *dst, int move);
int tree_delete(const char *path);
/* Low-level exclusive-destination copy/move. These do not replace existing files;
 * tree_copy handles conflicts and attempts per-replacement recovery; completed
 * tree work is never rolled back. A failed copy removes its temporary
 * destination when possible; move falls back from rename to copy/delete.
 */
int file_copy(const char *src, const char *dst);
int file_move(const char *src, const char *dst);
/* Validate one nonempty DOS 8.3 component, excluding wildcards and device names.
 */
int valid_name(const char *name);
/* Platform boundary: DOS implementation and a POSIX test implementation. */
/* Fill a bounded pane listing with a synthetic parent except at the drive root.
 * Metadata is raw filesystem data; panel_load owns sorting and revisions.
 */
int fs_list(Panel *p);
/* Resolve an existing directory into an NW_PATH-byte output, which may alias path.
 * DOS temporarily changes drive/directories, then attempts to restore both.
 * Relative paths are relative to process DOS state, not the active pane.
 */
int fs_canonical(const char *path, char *out);
/* Fetch attributes, size and packed timestamp for an existing file/directory.
 * A missing path returns zero; callers decide whether absence is an error.
 */
int fs_info(const char *path, Entry *e);
/* Boolean mutations. rename must not replace an existing destination; callers
 * use distinct temporary/backup names. Errors may require an fs_error call.
 */
int fs_mkdir(const char *path);
int fs_delete(const char *path, int directory);
int fs_rename(const char *src, const char *dst);
/* Descriptor API: open/create return fd >= 0 or -1. Creation is exclusive and
 * binary; read/write return a byte count or -1, close returns zero or -1.
 * Callers must check short writes and close errors before committing a copy.
 */
int fs_read_open(const char *path);
int fs_create(const char *path);
int fs_read(int fd, void *data, unsigned count);
int fs_write(int fd, const void *data, unsigned count);
int fs_close(int fd);
/* Set the open output file timestamp from packed DOS words; fs_attr sets final
 * path attributes after closing. Both return nonzero on success.
 */
int fs_stamp(int fd, const Entry *e);
int fs_attr(const char *path, unsigned attr);
/* Format the current errno with a bounded action label into nw_error.
 */
void fs_error(const char *action);
#endif
