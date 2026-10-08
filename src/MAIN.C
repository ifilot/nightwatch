/* SPDX-License-Identifier: GPL-3.0-only */
/*
 * MAIN.C - Keyboard-driven application and modal user interface.
 *
 * The two Panel records are shared by all renderers. Ordinary navigation only
 * changes the cursor; directory/mark helpers own the content revision. Dialogs
 * invalidate the desktop through VIDEO.C, so the next draw restores the panes
 * without retaining a second screen image in the small-model data segment.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dos.h>
#include <dir.h>
#include <bios.h>
#include "NW.H"
#include "VIDEO.H"
#include "GRAPH.H"
#include "HEX.H"
#include "VIEW.H"
/* BIOS extended keys keep the scan code in the high byte. Ordinary keys are
 * reduced to their ASCII byte by key_read(); control keys therefore use ASCII.
 */
#define KEY_F1 0x3b00
#define KEY_ALT_F1 0x6800
#define KEY_F2 0x3c00
#define KEY_F3 0x3d00
#define KEY_SHIFT_F3 0x5600
#define KEY_F4 0x3e00
#define KEY_F5 0x3f00
#define KEY_F6 0x4000
#define KEY_F7 0x4100
#define KEY_F8 0x4200
#define KEY_F9 0x4300
#define KEY_F10 0x4400
static int active;
static char command[NW_PATH];
static char status[160] = "Tab switches panes. F1 shows help. F2 selects display mode.";
static char startup[NW_PATH];
/* Preserve queued commands while progress scans the entire BIOS queue for Esc. */
static unsigned pending_keys[16];
static unsigned pending_head, pending_count;
static int copy_progress, counting, force_progress;
static unsigned long total_files, total_bytes;
static unsigned long sample_tick, paint_tick, sample_bytes, transfer_rate;
static unsigned long shown_files, shown_skipped, last_total;
static char progress_path[NW_PATH];
/* Turbo C startup reserves this stack inside DGROUP. Recursive operations
 * and viewer locals must share it; tools/check_memory.py guards near-heap room.
 */
unsigned _stklen = 8192;
#ifdef NW_DIAGNOSTICS
static unsigned long viewer_pages;
#endif

/* Read a queued or BIOS key using the common ASCII/scan-code convention.
 * Transfer progress can defer non-Escape keys in the bounded pending ring.
 */
static int key_read(void)
{
    int k;
    if (pending_count) {
        k = pending_keys[pending_head];
        pending_head = (pending_head + 1) & 15; --pending_count;
    } else k = bioskey(0);
    if ((k & 255) == 0 || (k & 255) == 224) return k & 0xff00;
    return k & 255;
}
/* Return visible entry rows, independent of the character grid used by dialogs.
 */
static int pane_rows(void) { return video_mode == VIDEO_TEXT ? video_rows - 8 : graph_rows(); }
/* Draw a double-line CP437 frame in character cells, including all four corners.
 */
static void frame(int x, int y, int width, int height, unsigned char attr)
{
    int i;
    video_fill(x, y, width, 205, attr);
    video_fill(x, y + height - 1, width, 205, attr);
    for (i = 1; i < height - 1; ++i) {
        video_put(x, y + i, 186, attr); video_put(x + width - 1, y + i, 186, attr);
    }
    video_put(x, y, 201, attr); video_put(x + width - 1, y, 187, attr);
    video_put(x, y + height - 1, 200, attr); video_put(x + width - 1, y + height - 1, 188, attr);
}
/* Text desktop cache. Generation belongs to the video lifecycle; revisions
 * belong to pane contents. Cursor/top/focus changes need no directory reread.
 */
static unsigned text_generation, text_revision[2];
static int text_active = -1, text_cursor[2], text_top[2];
static char text_status[160], text_command[NW_PATH];
/* Paint one text-pane row; clear missing entries so shortened lists leave no tail.
 */
static void text_entry(int which, int row)
{
    Panel *p = &panes[which];
    int x = which * 40, index = p->top + row;
    unsigned char attr = COLOR_NORMAL;
    char line[100], size[16], date[12];
    if (index < p->count) {
        Entry *e = &p->files[index];
        attr = index == p->cursor && which == active ? COLOR_SELECT :
               (e->marked ? COLOR_MARKED : ((e->attr & NW_DIR) ? COLOR_DIRECTORY : COLOR_NORMAL));
        if (e->attr & NW_DIR) strcpy(size, "<DIR>");
        else sprintf(size, "%lu", e->size);
        if (e->date) sprintf(date, "%02u-%02u-%02u", (e->date >> 5) & 15, e->date & 31, ((e->date >> 9) + 80) % 100);
        else strcpy(date, "        ");
        sprintf(line, "%c %-12s %10s %s", e->marked ? 254 : ((e->attr & NW_DIR) ? (!strcmp(e->name, "..") ? 24 : 16) : ' '), e->name, size, date);
        video_fill(x + 1, 4 + row, 38, ' ', attr);
        video_text(x + 1, 4 + row, line, attr);
    } else video_fill(x + 1, 4 + row, 38, ' ', attr);
    video_put(x + 15, 4 + row, 179, attr); video_put(x + 26, 4 + row, 179, attr);
}
/* Update a text pane from revision, focus, viewport and cursor changes.
 * A cursor-only change repaints its old and new rows; scrolling repaints the list.
 */
static void draw_panel(int which, int full, int focus)
{
    Panel *p = &panes[which];
    int x = which * 40, i, rows = pane_rows();
    int content = full || text_revision[which] != p->revision;
    unsigned char attr = which == active ? COLOR_ACTIVE : COLOR_NORMAL;
    char line[100];
    panel_scroll(p, rows);
    if (full || focus) {
        frame(x, 1, 40, video_rows - 4, attr);
        video_fill(x + 1, 3, 38, ' ', COLOR_BAR);
        video_text(x + 3, 3, "Name", COLOR_BAR);
        video_text(x + 21, 3, "Bytes", COLOR_BAR);
        video_text(x + 27, 3, "Date", COLOR_BAR);
        video_put(x + 15, 3, 179, COLOR_BAR); video_put(x + 26, 3, 179, COLOR_BAR);
    }
    if (content || focus) {
        unsigned n = strlen(p->path);
        video_fill(x + 1, 2, 38, ' ', COLOR_NORMAL);
        video_text(x + 1, 2, p->path + (n > 38 ? n - 38 : 0), attr);
    }
    if (content || focus || text_top[which] != p->top) {
        for (i = 0; i < rows; ++i) text_entry(which, i);
    } else if (text_cursor[which] != p->cursor) {
        i = text_cursor[which] - p->top;
        if (i >= 0 && i < rows) text_entry(which, i);
        i = p->cursor - p->top;
        if (i >= 0 && i < rows) text_entry(which, i);
    }
    if (content || focus) {
        video_fill(x + 1, video_rows - 4, 38, 205, attr);
        sprintf(line, " %d entries, %d marked%s ", p->count, p->marks, p->truncated ? " [LIMIT]" : "");
        video_text(x + 1, video_rows - 4, line, attr);
    }
    text_revision[which] = p->revision; text_cursor[which] = p->cursor; text_top[which] = p->top;
}
/* Draw the shared desktop through the active renderer. Text mode retains only
 * change-detection state, not a screen buffer; a video generation change forces
 * a full redraw after an overlay or mode switch.
 */
static void draw(void)
{
    static const char *actions[] = { "Help", "Mode", "View", "Edit", "Copy", "Move", "Mkdir", "Delete", "Path", "Quit" };
    char line[160];
    int i, full, focus;
    if (video_mode != VIDEO_TEXT) { graph_draw(panes, active, status, command); return; }
    video_desktop_begin(); full = text_generation != video_generation();
    focus = text_active != active;
    if (full) {
        video_clear(COLOR_NORMAL);
        video_fill(0, video_rows - 1, 80, ' ', COLOR_BAR);
        for (i = 0; i < 10; ++i) {
            sprintf(line, "%2d", i + 1);
            video_text(i * 8, video_rows - 1, line, COLOR_KEY);
            video_text(i * 8 + 2, video_rows - 1, actions[i], COLOR_BAR);
        }
    }
    if (full || focus) {
        video_fill(0, 0, 80, ' ', COLOR_BAR);
        sprintf(line, " Nightwatch %s | %-12s | %s pane", NW_VERSION_TAG, video_name(), active ? "Right" : "Left");
        video_text(0, 0, line, COLOR_BAR);
    }
    draw_panel(0, full, focus); draw_panel(1, full, focus);
    if (full || strcmp(text_status, status)) {
        video_fill(0, video_rows - 3, 80, ' ', COLOR_NORMAL);
        video_text(0, video_rows - 3, status, COLOR_MARKED); strcpy(text_status, status);
    }
    if (full || strcmp(text_command, command)) {
        video_fill(0, video_rows - 2, 80, ' ', COLOR_NORMAL);
        sprintf(line, "> %.76s", command);
        video_text(0, video_rows - 2, line, COLOR_ACTIVE); strcpy(text_command, command);
    }
    text_generation = video_generation(); text_active = active;
    video_flush();
}
/* Paint a centered, non-clearing overlay. Callers add controls and flush it.
 */
static void dialog(const char *title, const char *message)
{
    int y = video_rows / 2 - 3, i;
    video_overlay_begin(0);
    for (i = 0; i < 7; ++i) video_fill(3, y + i, 74, ' ', COLOR_DIALOG);
    frame(3, y, 74, 7, COLOR_DIALOG);
    video_text(5, y + 1, title, COLOR_DIALOG);
    video_text(5, y + 3, message, COLOR_DIALOG);
}
/* Display a message until any key; the main loop restores the desktop afterward.
 */
static void notice(const char *message)
{
    dialog("Nightwatch", message);
    video_text(5, video_rows / 2 + 2, "Press any key", COLOR_DIALOG);
    video_flush(); key_read();
}
/* Edit a caller-owned NUL-terminated buffer of max bytes, including the terminator.
 * Return nonzero for a nonempty Enter submission, zero for Escape/empty Enter.
 * Edits survive cancellation; callers must copy first if rollback is required.
 */
static int prompt(const char *title, char *text, unsigned max)
{
    unsigned n = strlen(text), start;
    int k, y = video_rows / 2;
    for (;;) {
        dialog(title, "Enter accepts, Esc cancels, Ctrl-U clears");
        video_fill(5, y + 1, 70, ' ', COLOR_DIALOG);
        start = n > 67 ? n - 67 : 0;
        video_text(5, y + 1, text + start, COLOR_DIALOG);
        video_put(5 + n - start, y + 1, '_', COLOR_DIALOG);
        video_flush(); k = key_read();
        if (k == 27) return 0;
        if (k == 13) return n != 0;
        if (k == 21) n = 0;
        else if (k == 8 && n) --n;
        else if (k >= 32 && k < 127 && n + 1 < max) text[n++] = k;
        text[n] = 0;
    }
}
/* Accept only Y/y. All other keys, including Escape, cancel the operation.
 */
static int confirm(const char *message)
{
    int k;
    dialog("Confirm", message);
    video_text(5, video_rows / 2 + 2, "Y = yes; any other key = cancel", COLOR_DIALOG);
    video_flush(); k = key_read(); return k == 'y' || k == 'Y';
}
/* BIOS ticks wrap at midnight; intervals here are always less than one day. */
static unsigned long tick_elapsed(unsigned long now, unsigned long before)
{
    return now >= before ? now - before : 0x1800b0UL - before + now;
}
/* Scale a bounded ratio without multiplying a potentially 4 GB byte count. */
static unsigned fraction(unsigned long done, unsigned long total, unsigned scale)
{
    unsigned low = 0, high = scale, mid, remainder;
    unsigned long whole;
    if (!total || done >= total) return scale;
    whole = total / scale; remainder = (unsigned)(total % scale);
    while (low < high) {
        mid = (low + high + 1) / 2;
        if (done >= whole * mid + (remainder * mid + scale - 1) / scale)
            low = mid;
        else high = mid - 1;
    }
    return low;
}
/* ASCII bars work in every renderer and avoid adapter-specific drawing paths. */
static void progress_bar(int row, const char *label, unsigned long done,
                         unsigned long total)
{
    char line[76];
    unsigned i, filled = fraction(done, total, 40);
    sprintf(line, "%-8s [", label);
    for (i = 0; i < 40; ++i) line[10 + i] = i < filled ? '#' : '-';
    sprintf(line + 50, "] %3u%%", fraction(done, total, 100));
    video_text(5, row, line, COLOR_DIALOG);
}
/* Poll every transfer block, retaining unrelated keys. Painting and rate samples
 * use BIOS time so slow disks update without repeatedly repainting on an 8088. */
static int operation_ui(const char *path, unsigned long done, unsigned long total)
{
    unsigned long now = (unsigned long)biostime(0, 0L), elapsed, bytes, seconds, rate_base;
    int changed = strcmp(progress_path, path) != 0, y, i;
    char line[100], eta[32];
    while (bioskey(1)) {
        unsigned key = bioskey(0);
        if ((key & 255) == 27) return 0;
        if (pending_count < 16) {
            pending_keys[(pending_head + pending_count) & 15] = key;
            ++pending_count;
        }
    }
    if (changed) {
        sample_tick = now; sample_bytes = operation_transferred; transfer_rate = 0;
    }
    elapsed = tick_elapsed(now, sample_tick);
    if (!counting && elapsed >= 9) {
        bytes = operation_transferred - sample_bytes;
        /* 18.2 ticks/s; divide first only when the product would overflow. */
        if (bytes <= 0xffffffffUL / 182UL) bytes = bytes * 182UL / (elapsed * 10UL);
        else {
            rate_base = bytes / elapsed;
            bytes = rate_base > 0xffffffffUL / 19UL ? 0xffffffffUL :
                    rate_base * 18UL + rate_base / 5UL +
                    (bytes % elapsed) * 182UL / (elapsed * 10UL);
        }
        transfer_rate = transfer_rate ? transfer_rate / 2 + bytes / 2 : bytes;
        sample_tick = now; sample_bytes = operation_transferred;
    }
    if (force_progress || (changed && !counting) || shown_files != operation_files ||
        shown_skipped != operation_skipped || tick_elapsed(now, paint_tick) >= 4) {
        if (counting) {
            sprintf(line, "%.70s", path);
            dialog("Counting files - Esc cancels", line);
            sprintf(line, "%lu files; %lu bytes found", total_files, total_bytes);
            video_text(5, video_rows / 2 + 1, line, COLOR_DIALOG);
        } else if (!copy_progress) {
            dialog("File operation - Esc cancels", path);
            sprintf(line, "%lu / %lu bytes; %lu completed, %lu skipped", done, total,
                    operation_files, operation_skipped);
            video_text(5, video_rows / 2 + 1, line, COLOR_DIALOG);
        } else {
            y = video_rows / 2 - 5;
            video_overlay_begin(0);
            for (i = 0; i < 11; ++i) video_fill(3, y + i, 74, ' ', COLOR_DIALOG);
            frame(3, y, 74, 11, COLOR_DIALOG);
            video_text(5, y + 1, "Copying - Esc cancels", COLOR_DIALOG);
            /* Keep long DOS paths inside the dialog border. */
            sprintf(line, "%.70s", path); video_text(5, y + 2, line, COLOR_DIALOG);
            sprintf(line, "Files: %lu / %lu completed; %lu skipped",
                    operation_files, total_files, operation_skipped);
            video_text(5, y + 3, line, COLOR_DIALOG);
            progress_bar(y + 4, "File", done, total);
            if (total_bytes)
                progress_bar(y + 5, "Overall", operation_bytes + operation_current_bytes, total_bytes);
            else progress_bar(y + 5, "Overall", operation_files + operation_skipped, total_files);
            sprintf(line, "%lu / %lu bytes", done, total);
            video_text(5, y + 6, line, COLOR_DIALOG);
            strcpy(eta, "--");
            if (transfer_rate) {
                bytes = done < total ? total - done : 0;
                seconds = bytes / transfer_rate + (bytes % transfer_rate != 0);
                sprintf(eta, "%lu:%02lu", seconds / 60, seconds % 60);
                sprintf(line, "%lu.%lu KiB/s   Time left: %s", transfer_rate / 1024,
                        (transfer_rate % 1024) * 10 / 1024, eta);
            } else strcpy(line, "-- KiB/s   Time left: --");
            video_text(5, y + 7, line, COLOR_DIALOG);
            sprintf(line, "Overall: %lu / %lu bytes processed", operation_bytes + operation_current_bytes, total_bytes);
            video_text(5, y + 8, line, COLOR_DIALOG);
        }
        video_flush(); paint_tick = now;
        shown_files = operation_files; shown_skipped = operation_skipped;
    }
    if (changed) strcpy(progress_path, path);
    last_total = total;
    force_progress = 0;
    return 1;
}
/* Map overwrite/skip/cancel keys to the backend callback results 1/2/0.
 */
static int conflict_ui(const char *src, const char *dst)
{
    int k;
    (void)src;
    dialog("Destination exists", dst);
    video_text(5, video_rows / 2 + 1, "O overwrite | S skip | Esc cancel", COLOR_DIALOG);
    video_flush(); k = key_read();
    /* Exclude time spent answering the conflict prompt from transfer samples. */
    sample_tick = (unsigned long)biostime(0, 0L);
    sample_bytes = operation_transferred; transfer_rate = 0; force_progress = 1;
    if (k == 'o' || k == 'O') return 1;
    if (k == 's' || k == 'S') return 2;
    return 0;
}
/* Start per-command counters and install synchronous UI callbacks in the backend.
 */
static void begin_operation(void)
{
    operation_files = operation_skipped = 0;
    operation_bytes = operation_current_bytes = operation_transferred = 0;
    copy_progress = counting = 0; total_files = total_bytes = 0;
    progress_path[0] = 0; force_progress = 1;
    sample_tick = paint_tick = (unsigned long)biostime(0, 0L);
    sample_bytes = transfer_rate = shown_files = shown_skipped = 0;
    operation_progress = operation_ui; operation_conflict = conflict_ui;
}
/* Remove UI callbacks so unrelated filesystem calls do not open transfer dialogs.
 */
static void end_operation(void)
{
    operation_progress = NULL; operation_conflict = NULL;
}
/* Reuse the last filename pattern: select=0 finds, 1 marks, and 2 unmarks.
 */
static void find_name(int select)
{
    static char pattern[64] = "*";
    if (!prompt(select == 2 ? "Unmark filename pattern (* and ?)" : (select ? "Mark filename pattern (* and ?)" : "Find filename pattern (* and ?)" ), pattern, sizeof(pattern))) return;
    if (select) panel_pattern(&panes[active], pattern, select == 1);
    else if (!panel_find(&panes[active], pattern)) notice("No matching entry in this pane");
}
/* Change only the active pane sort order or direction through the revision helper.
 */
static void sort_menu(void)
{
    int k, order = -1;
    Panel *p = &panes[active];
    dialog("Sort active pane", "N name | S size | D date | E extension | R reverse");
    video_flush(); k = key_read();
    if (k == 'n' || k == 'N') order = 0;
    if (k == 's' || k == 'S') order = 1;
    if (k == 'd' || k == 'D') order = 2;
    if (k == 'e' || k == 'E') order = 3;
    if (k == 'r' || k == 'R') panel_sort(p, p->sort, !p->reverse);
    else if (order >= 0) panel_sort(p, order, p->reverse);
}
/* After a directory move/delete, walk the pane path toward its surviving parent.
 * Never strip the final separator of a DOS drive root.
 */
static void surviving_directory(Panel *p)
{
    Entry e;
    char *cut;
    while (!fs_info(p->path, &e) || !(e.attr & NW_DIR)) {
        cut = strrchr(p->path, '\\');
        if (!cut) cut = strrchr(p->path, '/');
        if (!cut || (cut == p->path + 2 && !cut[1])) break;
        if (cut == p->path + 2) cut[1] = 0;
        else *cut = 0;
    }
}
/* Reload affected panes (bits 0/1) and clear marks in both panes. Shared paths
 * need only one successful enumeration, but receive independent mark state.
 * Failed loads must not be copied into the other pane as a valid snapshot.
 */
static void refresh_mask(unsigned mask)
{
    int i;
    for (i = 0; i < 2; ++i) if (mask & (1 << i)) surviving_directory(&panes[i]);
    if (mask && !stricmp(panes[0].path, panes[1].path)) mask = 3;
    /* Keep the established refresh behavior: marks clear in both panes. */
    for (i = 0; i < 2; ++i) {
        if (!(mask & (1 << i))) { panel_mark_all(&panes[i], 0); panes[i].top = 0; }
        else if (i == 1 && mask == 3 && !stricmp(panes[0].path, panes[1].path)) {
            panel_snapshot(&panes[1], &panes[0]);
        } else if (!panel_load(&panes[i])) {
            notice(nw_error);
            /* Do not clone stale/failed data into the second pane. */
            if (i == 0 && mask == 3 && !stricmp(panes[0].path, panes[1].path)) {
                if (!panel_load(&panes[1])) notice(nw_error);
                return;
            }
        }
    }
}
/* Explicit refresh reloads both pane directories.
 */
static void refresh(void) { refresh_mask(3); }
/* Find panes displaying a destination directory, or the parent of a file target.
 * If resolution fails, conservatively refresh both panes rather than leave stale
 * entries. Recursive directory operations separately force both bits.
 */
static unsigned destination_mask(const char *target, int directory)
{
    char parent[NW_PATH], full[NW_PATH], *cut;
    unsigned mask = 0;
    int i;
    strcpy(parent, target);
    if (!directory) {
        cut = strrchr(parent, '\\');
        if (!cut) cut = strrchr(parent, '/');
        if (!cut) return 3;
        cut[1] = 0;
    }
    if (!fs_canonical(parent, full)) return 3;
    for (i = 0; i < 2; ++i) if (!stricmp(full, panes[i].path)) mask |= 1 << i;
    return mask;
}
/* Show version and license credits in the common cell overlay on every adapter.
 */
static void about(void)
{
    int y = video_rows / 2 - 5, i;
    video_overlay_begin(0);
    for (i = 0; i < 11; ++i) video_fill(13, y + i, 54, ' ', COLOR_DIALOG);
    frame(13, y, 54, 11, COLOR_DIALOG);
    video_text(15, y + 1, "Nightwatch " NW_VERSION_TAG, COLOR_DIALOG);
    video_text(15, y + 3, "Two-pane file navigator for MS-DOS", COLOR_DIALOG);
    video_text(15, y + 4, "8088/8086 | Text, CGA, EGA and VGA", COLOR_DIALOG);
    video_text(15, y + 6, "GPLv3 - no warranty. See LICENSE.TXT", COLOR_DIALOG);
    video_text(15, y + 7, "Spleen: BSD-2-Clause (FONTLIC.TXT)", COLOR_DIALOG);
    video_text(15, y + 9, "Press any key to return", COLOR_DIALOG);
    video_flush(); key_read();
}
/* Show full-screen keyboard help; A opens About over a restored desktop.
 */
static void help(void)
{
    static const char *lines[] = {
        "NIGHTWATCH - two-pane DOS file navigator",
        "Tab switches panes; arrows/Home/End/PgUp/PgDn navigate",
        "Enter opens directory/file or runs the command line",
        "Backspace returns to parent when command line is empty",
        "Insert/Space mark entry; + marks all, - clears marks",
        "F1 Help / A About  F2 Text / CGA / EGA / VGA display mode",
        "F3 Text viewer  Shift-F3 Hex viewer  F4 External editor",
        "F5 Copy         F6 Move/rename      F7 Make directory",
        "F8 Recursive delete (confirmation) F9 Change path/drive",
        "F10 Quit        Ctrl-R Refresh     Ctrl-O DOS shell",
        "Ctrl-F Find filename pattern (* and ?)",
        "Ctrl-P Mark / Ctrl-N Unmark pattern; Ctrl-S Sort pane",
        "Ctrl-W Save display mode, paths and sorting to NIGHT.CFG",
        "Viewer: F4 Text/hex, F7 Search, F8 Next, Ctrl-G Offset",
        "Viewer search: literal ASCII or hex:DE AD 00 BE EF",
        "Copy/move/delete recurse; Esc cancels between blocks",
        "Existing files: O overwrite, S skip, Esc cancel",
        "Completed work remains after cancellation or an error",
        "512 entries/pane; [LIMIT] reports omitted entries",
        "Recursive operations include entries beyond this limit",
        "DOS 8.3 names; dates MM-DD-YY; hidden/system files shown",
        "A About; any other key returns"
    };
    unsigned i;
    int key;
    video_overlay_begin(1);
    video_clear(COLOR_NORMAL);
    for (i = 0; i < sizeof(lines)/sizeof(lines[0]); ++i) video_text(2, i + 1, lines[i], COLOR_ACTIVE);
    video_flush();
    key = key_read();
    if (key == 'a' || key == 'A') { draw(); about(); }
}
/* Return the selected entry, or NULL for an empty pane. Drawing clamps the cursor.
 */
static Entry *current(void)
{
    Panel *p = &panes[active];
    return p->count ? &p->files[p->cursor] : NULL;
}
/* Build a selected-entry path, rejecting the synthetic parent entry and emptiness.
 */
static int current_path(char *path)
{
    Entry *e = current();
    return e && strcmp(e->name, "..") && path_join(path, panes[active].path, e->name);
}
/* Modal text/hex viewer sharing one open binary stream. Text page starts form a
 * bounded sliding cache; VIEW.C rescans when PgUp/End lies outside it. Hex mode
 * uses byte offsets and a fixed page buffer. Only dirty pages read and repaint,
 * so irrelevant keys and boundary navigation perform no disk or video work.
 */
static void viewer(int hex_mode)
{
    char path[NW_PATH], header[160];
    long pages[128], next, size, offset = 0, found;
    char search[194] = "", jump[24] = "";
    unsigned char needle[64];
    unsigned needle_length = 0;
    int needle_hex = 0;
    /* Viewer and recursive transfers never nest. Use the 8 KB stack here
     * to preserve near-heap room for the copy progress state and strings. */
    unsigned char bytes[(NW_ROWS - 3) * 16];
    char row[80];
    int n, rows, got, dirty = 1, first = 1;
    long position;
    int page = 0, count = 1, y, x, ch, k, eof;
    FILE *f;
    Entry *e = current();
    if (!e || (e->attr & NW_DIR) || !current_path(path)) return;
    f = fopen(path, "rb");
    if (!f) { fs_error("View"); notice(nw_error); return; }
    if (fseek(f, 0L, SEEK_END) || (size = ftell(f)) < 0) {
        fs_error("View size"); fclose(f); notice(nw_error); return;
    }
    /* Page offsets are raw file bytes: CRLF conversion would break hex/search. */
    pages[0] = 0;
    rows = video_rows - 3;
    for (;;) {
        if (dirty) {
#ifdef NW_DIAGNOSTICS
            ++viewer_pages;
#endif
            if (fseek(f, hex_mode ? offset : pages[page], SEEK_SET)) {
                fs_error("View seek"); notice(nw_error); break;
            }
            video_overlay_begin(first); first = 0;
            video_clear(COLOR_NORMAL);
            sprintf(header, " %s: %.65s", hex_mode ? "Hex" : "View", path);
            video_text(0, 0, header, COLOR_BAR);
            if (hex_mode) {
                video_text(0, 1, "Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII", COLOR_ACTIVE);
                got = (int)fread(bytes, 1, rows * 16, f);
                for (y = 2; (y - 2) * 16 < got; ++y) {
                    n = got - (y - 2) * 16; if (n > 16) n = 16;
                    hex_line(row, (unsigned long)(offset + (y - 2) * 16L), bytes + (y - 2) * 16, n);
                    video_text(0, y, row, COLOR_NORMAL);
                    for (x = 0; x < 8; ++x) video_put(x, y, row[x], COLOR_MARKED);
                    for (x = 61; x < 79; ++x) video_put(x, y, row[x], COLOR_ACTIVE);
                }
                next = ftell(f);
                sprintf(header, "%08lX/%08lX PgUp/Dn Home/End F4 Text F7 Find F8 Next ^G Go Esc%s",
                        (unsigned long)offset, (unsigned long)size,
                        next >= size ? " | EOF" : "");
                video_text(0, video_rows - 1, header, COLOR_BAR);
            } else {
                y = 1; x = 0; eof = 0;
                while (y < video_rows - 1) {
                    ch = getc(f);
                    if (ch == EOF) { eof = 1; break; }
                    if (ch == '\r') continue;
                    if (ch == '\n') { ++y; x = 0; continue; }
                    if (ch == '\t') {
                        int spaces = 8 - (x & 7);
                        while (spaces--) {
                            video_put(x++, y, ' ', COLOR_NORMAL);
                            if (x == 80) { x = 0; ++y; }
                        }
                    } else {
                        video_put(x++, y, ch >= 32 && ch < 127 ? ch : '.', COLOR_NORMAL);
                        if (x == 80) { x = 0; ++y; }
                    }
                }
                next = ftell(f);
                video_text(0, video_rows - 1, eof ? "EOF | PgUp Home/End | F4 Hex F7 Find F8 Next Ctrl-G Go | Esc" : "PgUp/Dn Home/End | F4 Hex F7 Find F8 Next Ctrl-G Go | Esc", COLOR_BAR);
            }
            if (ferror(f)) { fs_error("View read"); notice(nw_error); break; }
            video_flush(); dirty = 0;
        }
        /* Keep the painted origin for detecting navigation that changes nothing. */
        position = hex_mode ? offset : pages[page];
        k = key_read();
        if (k == 27 || k == KEY_F3 || k == KEY_F10) break;
        if (k == 7) {
            if (prompt("Go to byte offset (decimal or 0x hexadecimal)", jump, sizeof(jump))) {
                if (!view_offset(jump, &found) || found > size) notice("Offset is outside this file");
                else {
                    offset = found; pages[0] = found; page = 0; count = 1;
                }
            }
            dirty = 1; continue;
        }
        if (k == KEY_F7 || k == KEY_F8) {
            if (k == KEY_F7 || !needle_length) {
                if (!prompt("Search ASCII, or hex:DE AD 00 (F8 finds next)", search, sizeof(search))) { dirty = 1; continue; }
                if (!view_pattern(search, needle, &needle_length, &needle_hex)) {
                    needle_length = 0; notice("Invalid search pattern"); dirty = 1; continue;
                }
            }
            found = view_find(f, position + (k == KEY_F8 && position < size ? 1 : 0), needle, needle_length, needle_hex);
            if (found < 0) notice(ferror(f) ? "Search read failed" : "No further match");
            else { offset = found; pages[0] = found; page = 0; count = 1; }
            dirty = 1; continue;
        }
        /* Text starts at the hex origin; text-to-hex rounds down to a 16-byte row. */
        if (k == KEY_F4) {
            if (hex_mode) { pages[0] = offset; page = 0; count = 1; }
            else offset = pages[page] - pages[page] % 16;
            hex_mode = !hex_mode; dirty = 1;
            continue;
        }
        if (hex_mode) {
            if (k == 0x4700) offset = 0;
            if (k == 0x4f00) offset = size ? (size - 1) / (rows * 16L) * (rows * 16L) : 0;
            if (k == 0x4900) offset = offset >= rows * 16L ? offset - rows * 16L : 0;
            if ((k == 0x5100 || k == ' ' || k == 13) && size - offset > rows * 16L)
                offset += rows * 16L;
            if (k == 0x4800 && offset >= 16) offset -= 16;
            if (k == 0x5000 && size - offset > 16) offset += 16;
            dirty = offset != position;
            continue;
        }
        if (k == 0x4900) {
            if (page) --page;
            else if (pages[0] > 0) {
                found = view_previous(f, pages[0], video_rows - 2);
                if (found < 0) { notice("Read previous page failed"); break; }
                pages[0] = found; count = 1;
            }
        }
        if (k == 0x4f00 && next < size) {
            found = view_last(f, video_rows - 2);
            if (found < 0) { notice("Read last page failed"); break; }
            pages[0] = found; page = 0; count = 1;
        }
        if (k == 0x4700) { pages[0] = 0; page = 0; count = 1; }
        if ((k == 0x5100 || k == ' ' || k == 13) && !eof) {
            if (page + 1 < count) ++page;
            else {
                /* Drop the oldest cached start, not access to earlier file pages. */
                if (count == 128) {
                    memmove(pages, pages + 1, 127 * sizeof(long)); --count; --page;
                }
                pages[count++] = next; ++page;
            }
        }
        dirty = pages[page] != position;
    }
    fclose(f);
}
/* Run in the active pane directory with the original display restored. DOS keeps
 * a current directory per drive, so preserve both affected drive directories,
 * then attempt to restore both directories, resume the display and refresh.
 */
static void run_shell(const char *line)
{
    int drive = getdisk(), result, target = toupper(panes[active].path[0]) - 'A';
    char old[NW_PATH], other[NW_PATH];
    const char *shell = getenv("COMSPEC");
    if (!getcwd(old, sizeof(old))) { fs_error("Current directory"); notice(nw_error); return; }
    setdisk(target);
    if (!getcwd(other, sizeof(other))) { setdisk(drive); notice("Cannot read shell drive directory"); return; }
    if (chdir(panes[active].path)) { setdisk(drive); fs_error("Shell directory"); notice(nw_error); return; }
    video_restore();
    if (line && *line) {
        result = system(line);
        printf("\nCommand returned %d. Press any key to return to Nightwatch.\n", result);
        bioskey(0);
    } else system(shell ? shell : "COMMAND.COM");
    if (target != drive) { setdisk(target); chdir(other); }
    setdisk(drive); chdir(old);
    video_resume(); refresh();
}
/* Launch EDITOR (or EDIT) on the current file via the same shell/display handoff.
 */
static void editor(void)
{
    char path[NW_PATH], line[256];
    Entry *e = current();
    const char *edit = getenv("EDITOR");
    if (!e || (e->attr & NW_DIR) || !current_path(path)) return;
    if (!edit) edit = "EDIT";
    if (strlen(edit) + strlen(path) + 2 >= sizeof(line)) { notice("EDITOR command is too long"); return; }
    sprintf(line, "%s %s", edit, path); run_shell(line);
}
/* Select a supported display mode without changing the pane/backend state.
 */
static void change_mode(void)
{
    int k, mode = -1;
    dialog("Display mode", "T text | C CGA 640x200 | E EGA 640x350 | V VGA 640x480");
    video_flush(); k = key_read();
    if (k == 't' || k == 'T') mode = VIDEO_TEXT;
    if (k == 'c' || k == 'C') mode = VIDEO_CGA;
    if (k == 'e' || k == 'E') mode = VIDEO_EGA;
    if (k == 'v' || k == 'V') mode = VIDEO_VGA;
    if (mode >= 0 && !video_set(mode)) notice("This display mode requires a different video adapter");
}
/* Copy/move marked entries, or the cursor entry when there are no marks.
 * Multiple selections require an existing target directory. Keep the list stable
 * while iterating, stop at the first failure, then refresh even after partial work.
 */
static void operate(int move)
{
    Panel *p = &panes[active];
    char target[NW_PATH], src[NW_PATH], dst[NW_PATH];
    Entry info;
    int i, marked = p->marks, directory, ok;
    unsigned affected;
    if (!p->count || (!marked && !strcmp(current()->name, ".."))) return;
    strcpy(target, panes[1-active].path);
    if (!prompt(move ? "Move/rename to absolute path" : "Copy to absolute path", target, sizeof(target))) return;
    if (target[1] != ':' || (target[2] != '\\' && target[2] != '/')) { notice("Use an absolute DOS path, such as C:\\FILES"); return; }
    directory = fs_info(target, &info) && (info.attr & NW_DIR);
    if (marked > 1 && !directory) { notice("Multiple files require an existing destination directory"); return; }
    affected = destination_mask(target, directory) | (move ? 1 << active : 0);
    begin_operation();
    copy_progress = !move;
    if (!move) {
        counting = 1; ok = 1;
        for (i = 0; i < p->count; ++i) {
            Entry *e = &p->files[i];
            if (marked ? !e->marked : i != p->cursor) continue;
            if (!path_join(src, p->path, e->name) ||
                !tree_measure(src, &total_files, &total_bytes)) { ok = 0; break; }
        }
        if (ok && progress_path[0]) {
            force_progress = 1;
            if (!operation_ui(progress_path, 0, 0)) {
                strcpy(nw_error, "Operation cancelled"); ok = 0;
            }
        }
        counting = 0; progress_path[0] = 0; force_progress = 1;
        if (!ok) { end_operation(); notice(nw_error); refresh(); return; }
    }
    for (i = 0; i < p->count; ++i) {
        Entry *e = &p->files[i];
        if (marked ? !e->marked : i != p->cursor) continue;
        if (!path_join(src, p->path, e->name)) { notice(nw_error); break; }
        if (e->attr & NW_DIR) affected = 3;
        if (directory) { if (!path_join(dst, target, e->name)) { notice(nw_error); break; } }
        else strcpy(dst, target);
        ok = tree_copy(src, dst, move);
        if (!ok) { notice(nw_error); break; }
    }
    if (copy_progress && progress_path[0] &&
        operation_files + operation_skipped == total_files) {
        force_progress = 1;
        operation_ui(progress_path, last_total, last_total);
    }
    end_operation();
    sprintf(status, "%lu completed, %lu skipped (%s)", operation_files, operation_skipped, move ? "move" : "copy"); refresh_mask(affected);
}
/* Confirm once, recursively delete the marked/current selection, and refresh
 * affected directories even when cancellation leaves earlier deletions committed.
 */
static void delete_entries(void)
{
    Panel *p = &panes[active];
    int i, marked = p->marks;
    unsigned affected = 1 << active;
    char path[NW_PATH], message[100];
    if (!p->count || (!marked && !strcmp(current()->name, ".."))) return;
    if (marked) sprintf(message, "Delete %d marked entries and directory contents?", marked);
    else sprintf(message, "Delete %s and all its contents?", current()->name);
    if (!confirm(message)) return;
    begin_operation();
    for (i = 0; i < p->count; ++i) {
        Entry *e = &p->files[i];
        if (marked ? !e->marked : i != p->cursor) continue;
        if (!path_join(path, p->path, e->name)) { notice(nw_error); break; }
        if (e->attr & NW_DIR) affected = 3;
        if (!tree_delete(path)) { notice(nw_error); break; }
    }
    end_operation();
    sprintf(status, "%lu files deleted", operation_files); refresh_mask(affected);
}
/* Validate one DOS 8.3 component, create it, then reload the active directory.
 */
static void make_dir(void)
{
    char name[13] = "", path[NW_PATH];
    if (!prompt("New directory (8.3 name)", name, sizeof(name))) return;
    if (!valid_name(name)) { notice("Use a valid DOS 8.3 name"); return; }
    if (!path_join(path, panes[active].path, name)) { notice(nw_error); return; }
    if (!fs_mkdir(path)) { fs_error("Make directory"); notice(nw_error); }
    refresh_mask(1 << active);
}
/* Try an existing directory path. If its listing fails, reload the old directory
 * while retaining the original error for the user.
 */
static void change_path(void)
{
    char path[NW_PATH], full[NW_PATH], old[NW_PATH], error[160];
    strcpy(path, panes[active].path); strcpy(old, path);
    if (!prompt("Directory or drive path (example C:\\)", path, sizeof(path))) return;
    if (!fs_canonical(path, full)) { notice(nw_error); return; }
    strcpy(panes[active].path, full);
    if (!panel_load(&panes[active])) {
        strcpy(error, nw_error); strcpy(panes[active].path, old);
        panel_load(&panes[active]); notice(error);
    }
    panes[active].cursor = panes[active].top = 0;
}
/* Locate configuration in the launch directory, independent of pane navigation.
 */
static int settings_file(char *path)
{
    return path_join(path, startup, "NIGHT.CFG");
}
/* Read the versioned settings record into temporary locals before applying it.
 * Missing/malformed records leave defaults intact; unavailable saved directories
 * leave their startup paths in place. Command-line arguments are applied later.
 */
static void settings_load(int *mode)
{
    char path[NW_PATH], line[NW_PATH + 2], full[NW_PATH], paths[2][NW_PATH], *at;
    int values[5], i;
    FILE *f;
    if (!settings_file(path)) return;
    f = fopen(path, "rt");
    if (!f) return;
    if (!fgets(line, sizeof(line), f) || strcmp(line, "NIGHTWATCH1\n") ||
        !fgets(line, sizeof(line), f) || !strchr(line, '\n')) { fclose(f); return; }
    /* Values are single digits. Avoid scanf's 16-bit overflow on corrupt input. */
    at = line;
    for (i = 0; i < 5; ++i) {
        while (isspace((unsigned char)*at)) ++at;
        if (*at < '0' || *at > ((i == 2 || i == 4) ? '1' : '3')) { fclose(f); return; }
        values[i] = *at++ - '0';
        if (*at && !isspace((unsigned char)*at)) { fclose(f); return; }
    }
    while (isspace((unsigned char)*at)) ++at;
    if (*at) { fclose(f); return; }
    /* Apply values only after both bounded path fields have been read. */
    for (i = 0; i < 2; ++i) {
        if (!fgets(line, sizeof(line), f)) { fclose(f); return; }
        line[strcspn(line, "\r\n")] = 0;
        if (strlen(line) >= NW_PATH) { fclose(f); return; }
        strcpy(paths[i], line);
    }
    fclose(f);
    *mode = values[0];
    panes[0].sort = values[1]; panes[0].reverse = values[2];
    panes[1].sort = values[3]; panes[1].reverse = values[4];
    for (i = 0; i < 2; ++i)
        if (paths[i][0] && fs_canonical(paths[i], full)) strcpy(panes[i].path, full);
}
/* Write settings to an exclusively created NIGHT.NEW, then rename into place.
 * Existing NIGHT.CFG is backed up to NIGHT.BAK; a failed commit attempts rollback.
 * Never overwrite a pre-existing temporary/backup file to make a save succeed.
 * Rollback and cleanup are best effort; NIGHT.BAK/NEW can remain for recovery.
 */
static void settings_save(void)
{
    char path[NW_PATH], temp[NW_PATH], backup[NW_PATH], data[320];
    int fd, n, existed;
    Entry e;
    if (!settings_file(path) || !path_join(temp, startup, "NIGHT.NEW") ||
        !path_join(backup, startup, "NIGHT.BAK")) { notice(nw_error); return; }
    n = sprintf(data, "NIGHTWATCH1\n%d %u %u %u %u\n%s\n%s\n", video_mode,
        (unsigned)panes[0].sort, (unsigned)panes[0].reverse,
        (unsigned)panes[1].sort, (unsigned)panes[1].reverse, panes[0].path, panes[1].path);
    fd = fs_create(temp);
    if (fd < 0) { fs_error("Create settings temporary file"); notice(nw_error); return; }
    existed = fs_write(fd, data, n) == n;
    if (fs_close(fd) < 0) existed = 0;
    if (!existed) { fs_delete(temp, 0); notice("Could not write settings"); return; }
    existed = fs_info(path, &e);
    if (existed && !fs_rename(path, backup)) { fs_delete(temp, 0); notice("Could not back up settings; check NIGHT.BAK"); return; }
    if (!fs_rename(temp, path)) {
        if (existed) fs_rename(backup, path);
        fs_delete(temp, 0); notice("Could not commit settings"); return;
    }
    if (existed && !fs_delete(backup, 0)) { notice("Settings saved; NIGHT.BAK could not be removed"); return; }
    strcpy(status, "Saved display mode, paths and sorting to NIGHT.CFG (startup directory)");
}
/* Apply defaults, saved settings and command-line overrides in that order.
 * Load panes before changing the display, then run one blocking keyboard loop.
 * Each draw clamps navigation state and each modal action returns to this loop.
 */
int main(int argc, char **argv)
{
    int mode = VIDEO_TEXT, k, i, paths = 0, quit = 0, explicit_mode = 0;
    if (!getcwd(startup, sizeof(startup))) { puts("Cannot read current directory"); return 1; }
    strcpy(panes[0].path, startup); strcpy(panes[1].path, startup);
    settings_load(&mode);
    for (i = 1; i < argc; ++i) {
        if (!stricmp(argv[i], "/text")) { mode = VIDEO_TEXT; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/cga")) { mode = VIDEO_CGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/ega")) { mode = VIDEO_EGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/vga")) { mode = VIDEO_VGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/?")) {
            puts("NIGHT [/text|/cga|/ega|/vga] [left-directory] [right-directory]\nNIGHT /version | Alt-F1 About"); return 0;
        } else if (!stricmp(argv[i], "/version") || !stricmp(argv[i], "--version")) {
            puts("Nightwatch " NW_VERSION_TAG); return 0;
        } else if (paths < 2 && fs_canonical(argv[i], panes[paths].path)) ++paths;
        else { printf("Invalid argument/path: %s\n", argv[i]); return 1; }
    }
    if (!panel_load(&panes[0])) { puts(nw_error); return 1; }
    if (!stricmp(panes[0].path, panes[1].path)) panel_snapshot(&panes[1], &panes[0]);
    else if (!panel_load(&panes[1])) { puts(nw_error); return 1; }
    video_init();
    /* A stale saved mode falls back to text; an explicit unsupported mode fails. */
    if (!explicit_mode && !video_supported(mode)) mode = VIDEO_TEXT;
    if (!video_set(mode)) { puts("Requested display mode is unavailable on this adapter."); return 1; }
    while (!quit) {
        Panel *p = &panes[active];
        Entry *e;
        unsigned n;
        draw(); k = key_read(); e = current(); n = strlen(command);
        switch (k) {
        case KEY_F1: help(); break;
        case KEY_ALT_F1: about(); break;
        case KEY_F2: change_mode(); break;
        case KEY_F3: viewer(0); break;
        case KEY_SHIFT_F3: viewer(1); break;
        case KEY_F4: editor(); break;
        case KEY_F5: operate(0); break;
        case KEY_F6: operate(1); break;
        case KEY_F7: make_dir(); break;
        case KEY_F8: delete_entries(); break;
        case KEY_F9: change_path(); break;
        case KEY_F10: quit = 1; break;
        case 9: active = 1 - active; break;
        case 0x4800: --p->cursor; break;
        case 0x5000: ++p->cursor; break;
        case 0x4900: p->cursor -= pane_rows(); break;
        case 0x5100: p->cursor += pane_rows(); break;
        case 0x4700: p->cursor = 0; break;
        case 0x4f00: p->cursor = p->count - 1; break;
        case 18: refresh(); break;
        case 6: find_name(0); break;
        case 16: find_name(1); break;
        case 14: find_name(2); break;
        case 19: sort_menu(); break;
        case 23: settings_save(); break;
        case 15: run_shell(NULL); break;
        case 27: command[0] = 0; break;
        case 8:
            if (n) command[n-1] = 0;
            else if (!panel_enter(p, "..")) notice(nw_error);
            break;
        case 13:
            if (n) { run_shell(command); command[0] = 0; }
            else if (e && (e->attr & NW_DIR)) { if (!panel_enter(p, e->name)) notice(nw_error); }
            else viewer(0);
            break;
        case 0x5200:
            if (e && strcmp(e->name, "..")) { panel_mark(p, p->cursor, !e->marked); ++p->cursor; }
            break;
        /* Space and +/- act on marks only while the command line is empty. */
        default:
            if (k == ' ' && !n) {
                if (e && strcmp(e->name, "..")) { panel_mark(p, p->cursor, !e->marked); ++p->cursor; }
            } else if ((k == '+' || k == '-') && !n) {
                panel_mark_all(p, k == '+');
            } else if (k >= 32 && k < 127 && n + 1 < sizeof(command)) { command[n] = k; command[n+1] = 0; }
            break;
        }
    }
    video_restore(); return 0;
}
