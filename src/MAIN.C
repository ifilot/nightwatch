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
#include <time.h>
#include <dos.h>
#include <dir.h>
#include <bios.h>
#include "NW.H"
#include "FEATURE.H"
#include "ZIP.H"
#include "ARJ.H"
#include "BROWSE.H"
#include "DRIVE.H"
#include "STORTEST.H"
#include "STORUI.H"
#include "STORMSG.H"
#include "VIDEO.H"
#include "GRAPH.H"
#include "HELP.H"
#include "BUILD.H"
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
static unsigned text_command_cursor=65535U;
static int active;
static char command[NW_PATH];
unsigned command_cursor;
static int conflict_policy, operation_cancelled, searching;
static char status[160] = "Tab switches panes. F1 shows help. F2 selects display mode.";
static char startup[NW_PATH];
/* Keep manually saved pane settings while automatic saves update only mode. */
/* Byte after the fixed "NIGHTWATCH1\n" header. */
#define SETTINGS_MODE_AT 12
static char settings_record[320];
static int remember_mode;
static int settings_write(const char *record);
static void settings_remember(void);
/* Preserve queued commands while progress scans the entire BIOS queue for Esc. */
static unsigned pending_keys[16];
static unsigned pending_head, pending_count;
static int copy_progress, counting, force_progress;
static int copy_dialog;
static unsigned copy_generation, file_percent, overall_percent;
static unsigned long total_files, total_bytes;
static unsigned long sample_tick, paint_tick, sample_bytes, transfer_rate;
static unsigned long storage_started;
static unsigned storage_paint_stage;
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
static int key_value(int k)
{
    if ((k & 255) == 0 || (k & 255) == 224) return k & 0xff00;
    return k & 255;
}
static int key_read(void)
{
    int k;
    if (pending_count) {
        k = pending_keys[pending_head];
        pending_head = (pending_head + 1) & 15; --pending_count;
    } else k = bioskey(0);
    return key_value(k);
}
/* Commands deferred by progress belong to the pane, never to an overwrite
 * decision. Modal input reads the BIOS queue without consuming that ring. */
static int modal_key_read(void) { return key_value(bioskey(0)); }
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
        if (!browse_active(p) && (p->offset || p->truncated)) {
            sprintf(line," Page %lu: %d entries%s ",p->offset/(NW_FILES-(strlen(p->path)>3))+1,
                    p->count,p->truncated ? " [MORE]" : "");
        }
        video_text(x + 1, video_rows - 4, line, attr);
    }
    text_revision[which] = p->revision; text_cursor[which] = p->cursor; text_top[which] = p->top;
}
static void disk_status(char *out)
{
    Panel *p=&panes[active];
    char format[100], suffix[40], left[16], right[16];
    unsigned directories;
    unsigned long selected;
    panel_scroll(p,pane_rows());
    selected=panel_selected_bytes(p,&directories);
    if (p->measured_revision==p->revision && p->measured_cursor==p->cursor) { selected=p->measured_bytes; directories=0; }
    suffix[0]=0;
    if (directories) feature_label(suffix,FM_DIRS);
    if (p->free_kib==0xffffffffUL || panes[1-active].free_kib==0xffffffffUL) {
        feature_label(format,FM_SELECTED); sprintf(out,format,selected,suffix); return;
    }
    sprintf(left,"%luK",p->free_kib);
    sprintf(right,"%luK",panes[1-active].free_kib);
    if (toupper(p->path[0])==toupper(panes[1-active].path[0])) {
        feature_label(format,FM_SAMEFREE); sprintf(out,format,p->path[0],left,selected,suffix);
    } else {
        feature_label(format,FM_FREE);
        sprintf(out,format,p->path[0],left,panes[1-active].path[0],right,selected,suffix);
    }
}
/* Draw the shared desktop through the active renderer. Text mode retains only
 * change-detection state, not a screen buffer; a video generation change forces
 * a full redraw after an overlay or mode switch.
 */
static void draw(void)
{
    static const char *actions[] = { "Help", "Mode", "View", "Edit", "Copy", "Move", "Mkdir", "Delete", "Drive", "Quit" };
    char line[160], shown[160];
    int i, full, focus;
    if (!strncmp(status,"Tab switches panes.",19)) disk_status(shown);
    else strcpy(shown,status);
    graph_command_cursor(command_cursor);
    if (video_mode != VIDEO_TEXT) { graph_draw(panes, active, shown, command); return; }
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
    if (full || strcmp(text_status, shown)) {
        video_fill(0, video_rows - 3, 80, ' ', COLOR_NORMAL);
        video_text(0, video_rows - 3, shown, COLOR_MARKED); strcpy(text_status, shown);
    }
    if (full || strcmp(text_command, command) || text_command_cursor!=command_cursor) {
        video_fill(0, video_rows - 2, 80, ' ', COLOR_NORMAL);
        sprintf(line, "> %.76s", command+(command_cursor>75 ? command_cursor-75 : 0));
        video_text(0, video_rows - 2, line, COLOR_ACTIVE); strcpy(text_command, command);
        video_put(2+(command_cursor>75 ? 75 : command_cursor),video_rows-2,
                  command[command_cursor] ? command[command_cursor] : '_',COLOR_SELECT);
        text_command_cursor=command_cursor;
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
    unsigned cursor=strlen(text), start;
    char visible[68];
    int k, y=video_rows/2;
    input_history_reset(0,text);
    for (;;) {
        dialog(title, "Enter accepts, Esc cancels, Up/Down history, Tab completes");
        video_fill(5,y+1,70,' ',COLOR_DIALOG);
        start=cursor>67 ? cursor-67 : 0;
        strncpy(visible,text+start,67); visible[67]=0;
        video_text(5,y+1,visible,COLOR_DIALOG);
        video_put(5+cursor-start,y+1,text[cursor] ? text[cursor] : '_',COLOR_ACTIVE);
        video_flush(); k=key_read();
        if (k==27) return 0;
        if (k==13) { input_history(text,max,&cursor,0,k); return *text!=0; }
        if (k==9) input_complete(text,max,&cursor,panes[active].path);
        else if (!input_history(text,max,&cursor,0,k)) {
            input_key(text,max,&cursor,k); input_history_reset(0,text);
        }
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
/* Native graphics fields share the Help frame; text mode uses cell bars. */
static void progress_bar(int row, const char *label, unsigned long done,
                         unsigned long total)
{
    char line[76];
    unsigned i, filled = fraction(done, total, 40);
    if (video_mode != VIDEO_TEXT) {
        sprintf(line,"%3u%%",fraction(done,total,100));
        graph_copy_text(row == video_rows / 2 - 1 ? 5 : 6,line); return;
    }
    sprintf(line, "%-8s [", label);
    for (i = 0; i < 40; ++i) line[10 + i] = i < filled ? '#' : '-';
    sprintf(line + 50, "] %3u%%", fraction(done, total, 100));
    video_text(5, row, line, COLOR_DIALOG);
}
/* Each renderer clears shortened fields without disturbing progress tracks. */
static void progress_text(unsigned field, const char *text)
{
    int row = video_rows / 2 - 5 + (field == 0 ? 2 : (field == 1 ? 3 :
              (field == 2 ? 6 : (field == 3 ? 8 : 7))));
    char clipped[71];
    if (video_mode != VIDEO_TEXT) { graph_copy_text(field,text); return; }
    strncpy(clipped,text,70); clipped[70]=0;
    video_fill(5,row,70,' ',COLOR_DIALOG);
    video_text(5,row,clipped,COLOR_DIALOG);
}
/* Poll every transfer block, retaining unrelated keys. Painting and rate samples
 * use BIOS time so slow disks update without repeatedly repainting on an 8088. */
static int operation_ui(const char *path, unsigned long done, unsigned long total)
{
    unsigned long now = (unsigned long)biostime(0, 0L), elapsed, bytes, seconds, rate_base;
    int changed = strcmp(progress_path, path) != 0, y, i, reset = 0;
    unsigned current, overall;
    char line[100], eta[32], format[100];
    while (bioskey(1)) {
        unsigned key = bioskey(0);
        if ((key & 255) == 27) { operation_cancelled=1; return 0; }
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
    /* Coalesce tiny-file copy updates across path/count changes. Escape is
     * still checked on every callback; forced first/final/prompt paints bypass
     * the timer. Labels, counters and bars always share this callback snapshot. */
    if (force_progress || tick_elapsed(now, paint_tick) >= 4 ||
        (!copy_progress && !counting && (changed || shown_files != operation_files ||
                                       shown_skipped != operation_skipped))) {
        if (counting) {
            copy_dialog = 0;
            sprintf(line, "%.70s", path);
            dialog(searching ? feature_label(format,FM_SEARCHING) : "Counting files - Esc cancels",line);
            if (searching) { feature_label(format,FM_MATCHES); sprintf(line,format,total_files); }
            else sprintf(line,"%lu files; %lu bytes found",total_files,total_bytes);
            video_text(5, video_rows / 2 + 1, line, COLOR_DIALOG);
        } else if (!copy_progress) {
            copy_dialog = 0;
            dialog("File operation - Esc cancels", path);
            sprintf(line, "%lu / %lu bytes; %lu completed, %lu skipped", done, total,
                    operation_files, operation_skipped);
            video_text(5, video_rows / 2 + 1, line, COLOR_DIALOG);
        } else {
            y = video_rows / 2 - 5;
            reset = !copy_dialog || copy_generation != video_generation();
            if (video_mode != VIDEO_TEXT) {
                if (reset) graph_copy_begin(copy_progress == 2);
            } else {
                video_overlay_begin(0);
                if (reset) {
                    for (i = 0; i < 11; ++i) video_fill(3, y + i, 74, ' ', COLOR_DIALOG);
                    frame(3, y, 74, 11, COLOR_DIALOG);
                    video_text(5, y + 1, copy_progress == 2 ? "Unpacking archive - Esc cancels" : "Copying - Esc cancels", COLOR_DIALOG);
                }
            }
            copy_dialog = 1; copy_generation = video_generation();
            /* Keep long DOS paths inside the dialog border. */
            progress_text(0,path);
            sprintf(line, "Files: %lu / %lu completed; %lu skipped",
                    operation_files, total_files, operation_skipped);
            progress_text(1,line);
            progress_bar(y + 4, "File", done, total);
            if (total_bytes)
                progress_bar(y + 5, "Overall", operation_bytes + operation_current_bytes, total_bytes);
            else progress_bar(y + 5, "Overall", operation_files + operation_skipped, total_files);
            sprintf(line, "%lu / %lu bytes", done, total);
            progress_text(2,line);
            strcpy(eta, "--");
            if (transfer_rate) {
                bytes = done < total ? total - done : 0;
                seconds = bytes / transfer_rate + (bytes % transfer_rate != 0);
                sprintf(eta, "%lu:%02lu", seconds / 60, seconds % 60);
                sprintf(line, "%lu.%lu KiB/s   Time left: %s", transfer_rate / 1024,
                        (transfer_rate % 1024) * 10 / 1024, eta);
            } else strcpy(line, "-- KiB/s   Time left: --");
            progress_text(4,line);
            sprintf(line, "Overall: %lu / %lu bytes processed", operation_bytes + operation_current_bytes, total_bytes);
            progress_text(3,line);
        }
        if (video_mode == VIDEO_TEXT || !copy_progress || counting) video_flush();
        if (copy_progress && !counting && video_mode != VIDEO_TEXT) {
            current = fraction(done, total, 100);
            overall = total_bytes ?
                fraction(operation_bytes + operation_current_bytes, total_bytes, 100) :
                fraction(operation_files + operation_skipped, total_files, 100);
            graph_progress(0, current, file_percent, reset);
            graph_progress(1, overall, overall_percent, reset);
            file_percent = current; overall_percent = overall;
        }
        paint_tick = now;
        shown_files = operation_files; shown_skipped = operation_skipped;
    }
    if (changed) strcpy(progress_path, path);
    last_total = total;
    force_progress = 0;
    return 1;
}
/* Map overwrite/skip/cancel keys to the backend callback results 1/2/0.
 */
static void conflict_details(int row, const char *path, unsigned label)
{
    Entry e;
    char line[100], format[100], name[32];
    if (label==FM_SOURCE && operation_source_info) e=*operation_source_info;
    else if (!fs_info(path,&e)) return;
    feature_label(name,label);
    if (!e.date) { feature_label(format,FM_NODATE); sprintf(line,format,name,e.size); }
    else {
        feature_label(format,FM_COMPARE);
        sprintf(line,format,name,e.size,(e.date>>5)&15,e.date&31,
                (e.date>>9)+1980,e.time>>11,(e.time>>5)&63);
    }
    video_text(5,row,line,COLOR_DIALOG);
}
static int conflict_ui(const char *src, const char *dst)
{
    int k, y=video_rows/2;
    char title[100], keys[100];
    if (conflict_policy) return conflict_policy;
    copy_dialog=0;
    video_overlay_begin(0);
    for (k=y-4;k<=y+4;++k) video_fill(3,k,74,' ',COLOR_DIALOG);
    frame(3,y-4,74,9,COLOR_DIALOG);
    video_text(5,y-3,feature_label(title,FM_CONFLICT),COLOR_DIALOG);
    video_text(5,y-2,src,COLOR_DIALOG);
    conflict_details(y-1,src,FM_SOURCE);
    video_text(5,y,dst,COLOR_DIALOG);
    conflict_details(y+1,dst,FM_DEST);
    video_text(5,y+2,feature_label(keys,FM_CHOICES),COLOR_DIALOG);
    video_flush(); k=modal_key_read();
    sample_tick=(unsigned long)biostime(0,0L);
    sample_bytes=operation_transferred; transfer_rate=0; force_progress=1;
    if (k=='a' || k=='A') conflict_policy=1;
    if (k=='k' || k=='K') conflict_policy=2;
    if (k=='o' || k=='O') return 1;
    if (k=='s' || k=='S') return 2;
    if (!conflict_policy) operation_cancelled=1;
    return conflict_policy;
}
static int transfer_error(const char *src, const char *dst, const char *message)
{
    char title[100], keys[100], line[100], format[100];
    int k, y=video_rows/2;
    (void)dst;
    if (operation_cancelled) return 0;
    copy_dialog=0;
    dialog(feature_label(title,FM_RETRY),message);
    feature_label(format,FM_RETRYINFO); sprintf(line,format,src);
    video_text(5,y+1,line,COLOR_DIALOG);
    video_text(5,y+2,feature_label(keys,FM_RETRYKEYS),COLOR_DIALOG);
    video_flush(); k=modal_key_read();
    force_progress=1; sample_tick=(unsigned long)biostime(0,0L);
    sample_bytes=operation_transferred; transfer_rate=0;
    if (k=='r' || k=='R') return 1;
    if (k=='s' || k=='S') return 2;
    operation_cancelled=1; return 0;
}
/* Start per-command counters and install synchronous UI callbacks in the backend.
 */
static void begin_operation(void)
{
    conflict_policy=operation_cancelled=0;
    operation_files = operation_skipped = 0;
    operation_bytes = operation_current_bytes = operation_transferred = 0;
    copy_progress = counting = 0; total_files = total_bytes = 0;
    copy_dialog = 0;
    progress_path[0] = 0; force_progress = 1;
    sample_tick = paint_tick = (unsigned long)biostime(0, 0L);
    sample_bytes = transfer_rate = shown_files = shown_skipped = 0;
    operation_progress = operation_ui; operation_conflict = conflict_ui; operation_error=transfer_error;
}
/* Remove UI callbacks so unrelated filesystem calls do not open transfer dialogs.
 */
static void end_operation(void)
{
    graph_copy_end();
    operation_progress = NULL; operation_conflict = NULL; operation_error=NULL;
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
        p->offset=0;
    }
}
/* Reload affected panes (bits 0/1) and clear marks in both panes. Shared paths
 * need only one successful enumeration, but receive independent mark state.
 * Failed loads must not be copied into the other pane as a valid snapshot.
 */
static void refresh_mask(unsigned mask)
{
    int i;
    for (i = 0; i < 2; ++i) if ((mask & (1 << i)) && !browse_active(&panes[i])) surviving_directory(&panes[i]);
    if (mask && !stricmp(panes[0].path, panes[1].path)) mask = 3;
    /* Keep the established refresh behavior: marks clear in both panes. */
    for (i = 0; i < 2; ++i) {
        if (!(mask & (1 << i))) { panel_mark_all(&panes[i], 0); panes[i].top = 0; }
        else if (i == 1 && mask == 3 && panes[0].offset==panes[1].offset && !browse_active(&panes[0]) && !browse_active(&panes[1]) && !stricmp(panes[0].path, panes[1].path)) {
            panel_snapshot(&panes[1], &panes[0]);
        } else if (!browse_load(&panes[i])) {
            notice(nw_error);
            /* Do not clone stale/failed data into the second pane. */
            if (i == 0 && mask == 3 && panes[0].offset==panes[1].offset && !browse_active(&panes[0]) && !browse_active(&panes[1]) && !stricmp(panes[0].path, panes[1].path)) {
                if (!panel_load(&panes[1])) notice(nw_error);
                return;
            }
        }
    }
}
/* Reload both pane directories without querying capacity.
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
/* Show version and license credits in a native window or text overlay.
 */
static void about(void)
{
    int y = video_rows / 2 - 5, i;
    static const char *credits[] = {
        "Two-pane file navigator for MS-DOS",
        "8088/8086 | Text, CGA, EGA and VGA",
        "GPLv3 - no warranty. See LICENSE.TXT",
        "Spleen: BSD-2-Clause (FONTLIC.TXT)",
        "16pxls: Paul Mackenzie (ICONLIC.TXT)"
    };
    if (video_mode != VIDEO_TEXT) {
        graph_about(NW_VERSION_TAG, NW_BUILD_COMMIT, NW_REPOSITORY, credits);
        key_read(); return;
    }
    video_overlay_begin(0);
    for (i = 0; i < 11; ++i) video_fill(13, y + i, 54, ' ', COLOR_DIALOG);
    frame(13, y, 54, 11, COLOR_DIALOG);
    video_text(15, y + 1, "Nightwatch " NW_VERSION_TAG, COLOR_DIALOG);
    video_text(15, y + 3, credits[0], COLOR_DIALOG);
    video_text(15, y + 4, credits[1], COLOR_DIALOG);
    video_text(15, y + 6, credits[2], COLOR_DIALOG);
    video_text(15, y + 7, credits[3], COLOR_DIALOG);
    video_text(15, y + 8, credits[4], COLOR_DIALOG);
    video_text(15, y + 9, "Press any key to return", COLOR_DIALOG);
    video_flush(); key_read();
}
/* Scroll the same documentation in a native modal or text screen. The title,
 * repository, revision and controls remain fixed while the body moves. */
static void help(void)
{
    char text[HELP_WIDTH + 1], line[80];
    int first = 0, next, rows = video_mode == VIDEO_TEXT ? 18 : graph_help_rows();
    int key, i, full = 1, heading;
    for (;;) {
        if (video_mode != VIDEO_TEXT)
            graph_help(NW_VERSION_TAG, NW_BUILD_COMMIT, NW_REPOSITORY, first, full);
        else {
            if (full) {
                video_overlay_begin(1); video_clear(COLOR_NORMAL);
                video_text(2, 1, "Nightwatch " NW_VERSION_TAG " - Help", COLOR_TITLE);
                video_text(2, 2, NW_REPOSITORY, COLOR_ACTIVE);
                video_text(2, 3, "Commit: " NW_BUILD_COMMIT, COLOR_ACTIVE);
                video_text(2, 24, "Up/Down PgUp/PgDn Home/End | Esc closes | A About", COLOR_TITLE);
            }
            for (i = 0; i < rows; ++i) {
                if (first + i < HELP_LINES) help_line(first + i, text);
                else text[0] = 0;
                heading = text[0] == '#';
                video_fill(2, i + 5, 76, ' ', COLOR_NORMAL);
                video_text(2, i + 5, text + heading, heading ? COLOR_MARKED : COLOR_ACTIVE);
            }
            sprintf(line, "Lines %u-%u of %u", first + 1, first + rows, HELP_LINES);
            video_fill(2, 23, 76, ' ', COLOR_NORMAL);
            video_text(2, 23, line, COLOR_TITLE); video_flush();
        }
        full = 0;
        do {
            key = key_read(); next = first;
            if (key == 27 || key == KEY_F1 || key == 'q' || key == 'Q') return;
            if (key == 'a' || key == 'A') { draw(); about(); return; }
            switch (key) {
                case 0x4800: --next; break;
                case 0x5000: ++next; break;
                case 0x4900: next -= rows - 1; break;
                case 0x5100: next += rows - 1; break;
                case 0x4700: next = 0; break;
                case 0x4f00: next = HELP_LINES - rows; break;
            }
            if (next < 0) next = 0;
            if (next > HELP_LINES - rows) next = HELP_LINES - rows;
        } while (next == first);
        first = next;
    }
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
    char path[NW_PATH], display[NW_PATH], temporary[NW_PATH], header[160], marker[40];
    ViewPosition location;
    long pages[128], next, size, offset = 0, found;
    char search[194] = "", jump[24] = "";
    unsigned char needle[64];
    unsigned needle_length = 0;
    int needle_hex = 0;
    /* Viewer and recursive transfers never nest. Use the 8 KB stack here
     * to preserve near-heap room for the copy progress state and strings. */
    unsigned char bytes[(NW_ROWS - 3) * 16];
    char row[81];
    int n, rows, columns, native, got, dirty = 1, first = 1;
    long position;
    int page = 0, count = 1, y, x, ch, k, eof;
    FILE *f;
    int staged = 0;
    Entry *e = current();
    if (!e || (e->attr & NW_DIR) || !current_path(path)) return;
    strcpy(display,path);
    if (browse_active(&panes[active])) {
        if (!path_join(path,startup,"VIEW.TMP") || !operation_temp(path,temporary,"TMP")) { notice(nw_error); return; }
        begin_operation(); counting = 1;
        n = browse_view(&panes[active],temporary);
        end_operation();
        if (!n) { notice(nw_error); return; }
        strcpy(path,temporary); staged = 1;
    }
    f = fopen(path, "rb");
    if (!f) { fs_error("View"); if (staged) operation_discard(temporary); notice(nw_error); return; }
    if (fseek(f, 0L, SEEK_END) || (size = ftell(f)) < 0) {
        fs_error("View size"); fclose(f); if (staged) operation_discard(temporary); notice(nw_error); return;
    }
    /* Page offsets are raw file bytes: CRLF conversion would break hex/search. */
    pages[0] = 0;
    location.offset = -1;
    native = video_mode != VIDEO_TEXT;
    columns = native ? graph_view_columns() : 80;
    for (;;) {
        rows = native ? graph_view_rows() - hex_mode : video_rows - (hex_mode ? 3 : 2);
        if (dirty) {
#ifdef NW_DIAGNOSTICS
            ++viewer_pages;
#endif
            if (fseek(f, hex_mode ? offset : pages[page], SEEK_SET)) {
                fs_error("View seek"); notice(nw_error); break;
            }
            if (!hex_mode && !view_position(f, pages[page], &location)) {
                fs_error("View lines"); notice(nw_error); break;
            }
            if (native) graph_view_begin(hex_mode, display, first);
            else { video_overlay_begin(first); video_clear(COLOR_NORMAL); }
            first = 0;
            if (hex_mode) sprintf(header, " Hex: %.65s", display);
            else {
                sprintf(marker, "Line %lu / %lu", location.breaks < location.total ?
                        location.breaks + 1 : location.total, location.total);
                sprintf(header, " View: %.*s", 71 - (int)strlen(marker), display);
            }
            if (!native) {
                video_text(0, 0, header, COLOR_BAR);
                if (!hex_mode) video_text(79 - strlen(marker), 0, marker, COLOR_BAR);
            }
            if (hex_mode) {
                if (!native) video_text(0, 1, "Offset    00 01 02 03 04 05 06 07  08 09 0A 0B 0C 0D 0E 0F   ASCII", COLOR_ACTIVE);
                got = (int)fread(bytes, 1, rows * 16, f);
                for (y = 2; (y - 2) * 16 < got; ++y) {
                    n = got - (y - 2) * 16; if (n > 16) n = 16;
                    hex_line(row, (unsigned long)(offset + (y - 2) * 16L), bytes + (y - 2) * 16, n);
                    if (native) graph_view_line(y - 1, row, 1);
                    else {
                        video_text(0, y, row, COLOR_NORMAL);
                        for (x = 0; x < 8; ++x) video_put(x, y, row[x], COLOR_MARKED);
                        for (x = 61; x < 79; ++x) video_put(x, y, row[x], COLOR_ACTIVE);
                    }
                }
                next = ftell(f);
                sprintf(header, "%08lX/%08lX PgUp/Dn Home/End F4 Text F7 Find F8 Next ^G Go Esc%s",
                        (unsigned long)offset, (unsigned long)size,
                        next >= size ? " | EOF" : "");
                if (native) {
                    sprintf(header, "Bytes %08lX / %08lX%s", (unsigned long)offset,
                            (unsigned long)size, next >= size ? " | EOF" : "");
                    graph_view_footer(header, "PgUp/Dn Home/End | F4 Text F7 Find F8 Next ^G Go | Esc");
                } else video_text(0, video_rows - 1, header, COLOR_BAR);
            } else {
                y = native ? 0 : 1; x = 0; eof = 0;
                while (y < (native ? rows : video_rows - 1)) {
                    ch = getc(f);
                    if (ch == EOF) { eof = 1; break; }
                    if (ch == '\r') continue;
                    if (ch == '\n') {
                        if (native) { row[x] = 0; graph_view_line(y, row, 0); }
                        ++y; x = 0; continue;
                    }
                    n = ch == '\t' ? 8 - (x & 7) : 1;
                    if (ch == '\t') ch = ' ';
                    while (n--) {
                        if (native) row[x] = ch >= 32 && ch < 127 ? ch : '.';
                        else video_put(x, y, ch >= 32 && ch < 127 ? ch : '.', COLOR_NORMAL);
                        if (++x == columns) {
                            if (native) { row[x] = 0; graph_view_line(y, row, 0); }
                            x = 0; ++y;
                            if (y == (native ? rows : video_rows - 1)) break;
                        }
                    }
                }
                if (native && x) { row[x] = 0; graph_view_line(y, row, 0); }
                next = ftell(f);
                if (native) {
                    if (eof) strcat(marker, " | EOF");
                    graph_view_footer(marker, "PgUp/Dn Home/End | F4 Hex F7 Find F8 Next ^G Go | Esc");
                } else video_text(0, video_rows - 1, eof ? "EOF | PgUp Home/End | F4 Hex F7 Find F8 Next Ctrl-G Go | Esc" : "PgUp/Dn Home/End | F4 Hex F7 Find F8 Next Ctrl-G Go | Esc", COLOR_BAR);
            }
            if (ferror(f)) { fs_error("View read"); notice(nw_error); break; }
            if (!native) video_flush();
            dirty = 0;
        }
        /* Keep the painted origin for detecting navigation that changes nothing. */
        position = hex_mode ? offset : pages[page];
        k = key_read();
        if (k == 7 || k == KEY_F7 || k == KEY_F8) first = 1;
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
            hex_mode = !hex_mode; dirty = first = 1;
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
                found = view_previous_columns(f, pages[0], rows, columns);
                if (found < 0) { notice("Read previous page failed"); break; }
                pages[0] = found; count = 1;
            }
        }
        if (k == 0x4f00 && next < size) {
            found = view_last_columns(f, rows, columns);
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
    if (staged && !operation_discard(temporary)) notice(nw_error);
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
    if (browse_active(&panes[active])) { archive_message(ARC_BROWSER_READONLY); notice(nw_error); return; }
    if (!getcwd(old, sizeof(old))) { fs_error("Current directory"); notice(nw_error); return; }
    setdisk(target);
    if (!getcwd(other, sizeof(other))) { setdisk(drive); notice("Cannot read shell drive directory"); return; }
    if (chdir(panes[active].path)) { setdisk(drive); fs_error("Shell directory"); notice(nw_error); return; }
    video_restore(); drive_guard_end();
    if (line && *line) {
        result = system(line);
        printf("\nCommand returned %d. Press any key to return to Nightwatch.\n", result);
        bioskey(0);
    } else system(shell ? shell : "COMMAND.COM");
    if (target != drive) { setdisk(target); chdir(other); }
    setdisk(drive); chdir(old);
    drive_guard_begin(); video_resume(); refresh();
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
    char old;
    dialog(remember_mode ? "Display mode (R remember: ON)" : "Display mode (R remember: OFF)",
        "T text | C CGA | E EGA | V VGA | R remember mode");
    video_flush(); k = key_read();
    if (k == 't' || k == 'T') mode = VIDEO_TEXT;
    if (k == 'c' || k == 'C') mode = VIDEO_CGA;
    if (k == 'e' || k == 'E') mode = VIDEO_EGA;
    if (k == 'v' || k == 'V') mode = VIDEO_VGA;
    if (k == 'r' || k == 'R') {
        old = settings_record[SETTINGS_MODE_AT];
        settings_record[SETTINGS_MODE_AT] = '0' + video_mode;
        remember_mode = !remember_mode;
        if (!settings_write(settings_record)) {
            remember_mode = !remember_mode; settings_record[SETTINGS_MODE_AT] = old;
        }
        else strcpy(status, remember_mode ? "Display mode will be remembered" : "Automatic display mode saving disabled");
    }
    if (mode >= 0) {
        if (!video_set(mode)) notice("This display mode requires a different video adapter");
        else settings_remember();
    }
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
    if (browse_active(p)) {
        begin_operation(); counting = 1;
        ok = browse_copy(p,target,1,&total_files,&total_bytes);
        counting = 0; copy_progress = 2; progress_path[0] = 0; force_progress = 1;
        if (ok) ok = browse_copy(p,target,0,&total_files,&total_bytes);
        end_operation(); if (!ok) notice(nw_error); refresh(); return;
    }
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
/* Unpack the selected archive into the inactive pane with shared operation
 * controls. Detect ARJ by content; each reader validates its complete format.
 */
static void extract_archive(void)
{
    char source[NW_PATH], target[NW_PATH];
    int ok, arj;
    unsigned char magic[2];
    Entry *e = current();
    if (browse_active(&panes[active])) { operate(0); return; }
    if (!e || (e->attr & NW_DIR) || !current_path(source)) return;
    ok = fs_read_open(source);
    if (ok < 0) { fs_error("Open archive"); notice(nw_error); return; }
    arj = fs_read(ok, magic, 2);
    if (fs_close(ok) < 0 || arj != 2) { notice("Cannot read archive"); return; }
    arj = magic[0] == 0x60 && magic[1] == 0xea;
    strcpy(target, panes[1-active].path);
    if (!prompt("Unpack ZIP/ARJ into existing directory", target, sizeof(target))) return;
    begin_operation(); counting = 1;
    ok = arj ? arj_measure(source, target, &total_files, &total_bytes) :
               zip_measure(source, target, &total_files, &total_bytes);
    counting = 0; copy_progress = 2; progress_path[0] = 0; force_progress = 1;
    if (ok) ok = arj ? arj_extract(source, target) : zip_extract(source, target);
    if (ok && progress_path[0]) {
        force_progress = 1; operation_ui(progress_path, last_total, last_total);
    }
    end_operation();
    if (!ok) notice(nw_error);
    sprintf(status, "%lu unpacked, %lu skipped (%s)", operation_files, operation_skipped, arj ? "ARJ" : "ZIP");
    refresh();
}
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
static void open_path(const char *path)
{
    char full[NW_PATH], old[NW_PATH], error[160];
    unsigned long old_offset=panes[active].offset;
    strcpy(old,panes[active].path);
    drive_guard_begin();
    if (!fs_canonical(path, full)) { drive_guard_end(); notice(nw_error); return; }
    strcpy(panes[active].path, full); panes[active].offset=0;
    if (!panel_load(&panes[active])) {
        strcpy(error, nw_error); strcpy(panes[active].path, old); panes[active].offset=old_offset;
        browse_load(&panes[active]); drive_guard_end(); notice(error);
    } else { browse_release(&panes[active]); drive_guard_end(); }
    panes[active].cursor = panes[active].top = 0;
}
static void selection_size(void)
{
    Panel *p=&panes[active];
    char path[NW_PATH];
    int i, ok=1;
    if (browse_active(p)) { archive_message(ARC_BROWSER_READONLY); notice(nw_error); return; }
    begin_operation(); counting=1;
    for (i=0;i<p->count;++i) {
        Entry *e=&p->files[i];
        if ((p->marks ? !e->marked : i!=p->cursor) || !strcmp(e->name,"..")) continue;
        if (!path_join(path,p->path,e->name) || !tree_measure(path,&total_files,&total_bytes)) { ok=0; break; }
    }
    counting=0; end_operation();
    if (!ok) notice(nw_error);
    else {
        p->measured_bytes=total_bytes; p->measured_revision=p->revision; p->measured_cursor=p->cursor;
        strcpy(status,"Tab switches panes. F1 shows help. F2 selects display mode.");
    }
}
typedef struct { char selected[NW_PATH]; unsigned long matches; int cancelled; } FindContext;
static int search_match(const char *path, const Entry *info, void *context)
{
    FindContext *found=context;
    char title[100], keys[100];
    int k;
    unsigned length=strlen(path);
    (void)info;
    ++found->matches; total_files=found->matches;
    dialog(feature_label(title,FM_RESULT),path+(length>70 ? length-70 : 0));
    video_text(5,video_rows/2+2,feature_label(keys,FM_RESULTKEYS),COLOR_DIALOG);
    video_flush();
    for (;;) {
        k=key_read();
        if (k==13) { strcpy(found->selected,path); return 0; }
        if (k=='n' || k=='N') return 1;
        if (k==27) { found->cancelled=1; return -1; }
    }
}
static void recursive_find(void)
{
    char root[NW_PATH], full[NW_PATH], pattern[64], label[100], name[13], *cut;
    FindContext found;
    int ok, i;
    if (browse_active(&panes[active])) { archive_message(ARC_BROWSER_READONLY); notice(nw_error); return; }
    strcpy(root,panes[active].path); strcpy(pattern,"*.*");
    if (!prompt(feature_label(label,FM_SEARCHROOT),root,sizeof(root)) ||
        !prompt(feature_label(label,FM_SEARCH),pattern,sizeof(pattern))) return;
    if (!fs_canonical(root,full)) { notice(nw_error); return; }
    memset(&found,0,sizeof(found));
    begin_operation(); counting=searching=1;
    ok=tree_find(full,pattern,search_match,&found);
    counting=searching=0; end_operation();
    if (found.selected[0]) {
        cut=strrchr(found.selected,'\\');
        if (!cut) return;
        strcpy(name,cut+1);
        if (cut==found.selected+2) cut[1]=0; else *cut=0;
        open_path(found.selected);
        for (;;) {
            Panel *p=&panes[active];
            for (i=0;i<p->count;++i) if (!stricmp(p->files[i].name,name)) { p->cursor=i; return; }
            if (!p->truncated || !panel_page(p,1)) break;
        }
    } else if (!ok && !found.cancelled && !operation_cancelled) notice(nw_error);
    feature_label(label,FM_SEARCHDONE); sprintf(status,label,found.matches);
}
static void properties(void)
{
    Entry *e=current();
    char path[NW_PATH], label[100], format[100], line[100];
    unsigned attr;
    int k, y=video_rows/2;
    if (!e || !strcmp(e->name,"..") || !current_path(path)) return;
    if (browse_active(&panes[active])) { archive_message(ARC_BROWSER_READONLY); notice(nw_error); return; }
    attr=e->attr;
    for (;;) {
        dialog(feature_label(label,FM_ATTR),path);
        feature_label(format,FM_ATTRLINE);
        sprintf(line,format,attr&1 ? 'X':' ',attr&2 ? 'X':' ',attr&4 ? 'X':' ',attr&32 ? 'X':' ');
        video_text(5,y+1,line,COLOR_DIALOG);
        conflict_details(y+2,path,FM_SOURCE);
        video_flush(); k=key_read();
        if (k==27) return;
        if (k==13) {
            if (attr!=e->attr && !fs_attr(path,attr)) { fs_error(feature_label(label,FM_ATTRFAIL)); notice(nw_error); }
            refresh_mask(1<<active); return;
        }
        if (k=='r' || k=='R') attr^=1;
        if (k=='h' || k=='H') attr^=2;
        if (k=='s' || k=='S') attr^=4;
        if (k=='a' || k=='A') attr^=32;
    }
}
static void directory_page(int forward)
{
    Panel *p=&panes[active];
    char label[100];
    if (browse_active(p)) { notice(feature_label(label,FM_ARCHPAGE)); return; }
    if ((forward && !p->truncated) || (!forward && !p->offset)) return;
    if (p->marks && !confirm(feature_label(label,FM_PAGING))) return;
    if (!panel_page(p,forward)) notice(nw_error);
    feature_label(label,FM_OFFSET);
    sprintf(status,label,p->offset/(NW_FILES-(strlen(p->path)>3))+1);
}
/* F9 lists configured drives without opening their roots. Only Enter on a
 * drive performs filesystem access. The final row and '.' retain direct path
 * entry, including from a virtual archive pane. Lists fit all 26 letters. */
static void change_drive(void)
{
    Drive drives[26];
    char path[NW_PATH], label[72], line[40];
    unsigned count, current, selected=0, i, rows;
    int k, top, x, y, height;
    browse_home(&panes[active],path);
    current=(unsigned)(toupper(path[0])-'A');
    count=drive_scan(drives,current);
    for (i=0;i<count;++i) if (drives[i].number==current) selected=i;
    rows=count<12 ? count+1 : (count+2)/2;
    height=(int)rows+6; top=(video_rows-height)/2;
    for (;;) {
        video_overlay_begin(0);
        for (y=0;y<height;++y) video_fill(3,top+y,74,' ',COLOR_DIALOG);
        frame(3,top,74,height,COLOR_DIALOG);
        video_text(5,top+1,drive_label(label,DL_TITLE),COLOR_DIALOG);
        for (i=0;i<=count;++i) {
            unsigned char attr=i==selected ? COLOR_ACTIVE : COLOR_DIALOG;
            x=6+(int)(i/rows)*35; y=top+3+(int)(i%rows);
            video_fill(x,y,33,' ',attr);
            if (i==count) video_text(x+3,y,drive_label(label,DL_PATH),attr);
            else {
                drive_label(label,(drives[i].flags&DRIVE_OFFLINE) ? DL_OFFLINE : drives[i].kind);
                sprintf(line,"%c: %.27s",'A'+drives[i].number,label);
                video_text(x+3,y,line,attr);
            }
        }
        video_text(5,top+height-2,drive_label(label,DL_KEYS),COLOR_DIALOG);
        video_flush();
        for (i=0;i<count;++i) {
            x=6+(int)(i/rows)*35; y=top+3+(int)(i%rows);
            video_drive_icon(x*8,y*video_font_height,drives[i].kind);
        }
        k=key_read();
        if (k==27) return;
        if (k=='.' || (k==13 && selected==count)) {
            browse_home(&panes[active],path);
            if (prompt("Directory or drive path (example C:\\)",path,sizeof(path))) open_path(path);
            return;
        }
        if (k==13) {
            path[0]=(char)('A'+drives[selected].number);
            path[1]=':'; path[2]='\\'; path[3]=0;
            open_path(path); return;
        }
        if (k==0x4800 && selected) --selected;
        if (k==0x5000 && selected<count) ++selected;
        if (k==0x4b00 && selected>=rows) selected-=rows;
        if (k==0x4d00 && selected+rows<=count) selected+=rows;
        if (k==0x4700) selected=0;
        if (k==0x4f00) selected=count;
        if (k==9) selected=selected<count ? selected+1 : 0;
        if (k>=0 && k<128 && isalpha(k)) for (i=0;i<count;++i)
            if (drives[i].number==(unsigned)(toupper(k)-'A')) { selected=i; break; }
    }
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
    int values[5], i, remember = 0;
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
    /* Optional extension: old four-line NIGHTWATCH1 files remain valid. */
    if (fgets(line, sizeof(line), f)) {
        if (strcmp(line, "REMEMBER 1\n")) { fclose(f); return; }
        remember = 1;
    }
    fclose(f);
    remember_mode = remember;
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
static int settings_write(const char *record)
{
    char path[NW_PATH], temp[NW_PATH], backup[NW_PATH], data[340];
    int fd, n, existed;
    Entry e;
    if (!settings_file(path) || !path_join(temp, startup, "NIGHT.NEW") ||
        !path_join(backup, startup, "NIGHT.BAK")) { notice(nw_error); return 0; }
    n = sprintf(data, "%s%s", record, remember_mode ? "REMEMBER 1\n" : "");
    fd = fs_create(temp);
    if (fd < 0) { fs_error("Create settings temporary file"); notice(nw_error); return 0; }
    existed = fs_write(fd, data, n) == n;
    if (fs_close(fd) < 0) existed = 0;
    if (!existed) { fs_delete(temp, 0); notice("Could not write settings"); return 0; }
    existed = fs_info(path, &e);
    if (existed && !fs_rename(path, backup)) { fs_delete(temp, 0); notice("Could not back up settings; check NIGHT.BAK"); return 0; }
    if (!fs_rename(temp, path)) {
        if (existed) fs_rename(backup, path);
        fs_delete(temp, 0); notice("Could not commit settings"); return 0;
    }
    if (existed && !fs_delete(backup, 0)) { notice("Settings saved; NIGHT.BAK could not be removed"); return 1; }
    return 1;
}
/* Format the established four-line record, without the optional extension. */
static void settings_format(char *data, int mode)
{
    char left[NW_PATH], right[NW_PATH];
    browse_home(&panes[0],left); browse_home(&panes[1],right);
    sprintf(data, "NIGHTWATCH1\n%d %u %u %u %u\n%s\n%s\n", mode,
        (unsigned)panes[0].sort, (unsigned)panes[0].reverse,
        (unsigned)panes[1].sort, (unsigned)panes[1].reverse,
        left, right);
}
/* Ctrl-W explicitly saves pane state as well as the current display mode. */
static void settings_save(void)
{
    char data[320];
    settings_format(data, video_mode);
    if (settings_write(data)) {
        strcpy(settings_record, data);
        strcpy(status, "Saved display mode, paths and sorting to NIGHT.CFG (startup directory)");
    }
}
/* Save successful mode selections immediately, retaining manually saved panes.
 * A failed write leaves the snapshot unchanged so a later selection can retry.
 */
static void settings_remember(void)
{
    char old = settings_record[SETTINGS_MODE_AT];
    if (!remember_mode || old == '0' + video_mode) return;
    settings_record[SETTINGS_MODE_AT] = '0' + video_mode;
    if (!settings_write(settings_record)) settings_record[SETTINGS_MODE_AT] = old;
}
/* Storage tests share the ordinary DOS filesystem path with file operations.
 * Poll every block; repaint/rate samples use BIOS ticks to stay usable on 8088.
 */
static int storage_ui(const char *phase, const char *path,
                      unsigned long done, unsigned long total)
{
    unsigned long now = (unsigned long)biostime(0, 0L), elapsed;
    unsigned key;
    while (bioskey(1)) {
        key = bioskey(0);
        if ((key & 255) == 27) { operation_cancelled=1; return 0; }
        if (pending_count < 16) {
            pending_keys[(pending_head + pending_count) & 15] = key;
            ++pending_count;
        }
    }
    if (!done || strcmp(progress_path, path)) {
        sample_tick = now; sample_bytes = done; transfer_rate = 0;
        strcpy(progress_path, path);
    }
    elapsed = tick_elapsed(now, sample_tick);
    if (total && elapsed >= 9 && done >= sample_bytes) {
        transfer_rate = (done - sample_bytes) * 182UL / (elapsed * 10UL);
        sample_tick = now; sample_bytes = done;
    }
    if (force_progress || storage_paint_stage != storage_report.stage || tick_elapsed(now, paint_tick) >= 4) {
        storage_screen(phase, path, done, total, transfer_rate,
                       tick_elapsed(now, storage_started), NULL);
        storage_paint_stage = storage_report.stage;
        paint_tick = now; force_progress = 0;
    }
    return 1;
}
/* Decimal seeds have a fixed 32-bit contract, including on host builds. */
static int storage_seed(const char *text, unsigned long *seed)
{
    unsigned long value = 0;
    unsigned digit;
    if (!*text) return 0;
    while (*text) {
        if (*text < '0' || *text > '9') return 0;
        digit = *text++ - '0';
        if (value > (0xffffffffUL - digit) / 10UL) return 0;
        value = value * 10UL + digit;
    }
    *seed = value; return 1;
}
static int storage_create_confirm(const char *path)
{
    char title[80], line[80];
    unsigned n = strlen(path);
    int k, y = video_rows / 2 - 3;
    strncpy(line, path, 70); line[70] = 0;
    dialog(storage_label(title, ST_UICREATEDIR), line);
    if (n > 70) video_text(5, y + 4, path + 70, COLOR_DIALOG);
    video_text(5, y + 5, storage_label(line, ST_UICREATEKEY), COLOR_DIALOG);
    video_flush(); k = key_read();
    return k == 'y' || k == 'Y';
}
/* Keep run creation, saved verification and recorded-file cleanup distinct.
 * Never unmount a user's DOS drive implicitly; persistence verification follows
 * the card driver's successful unmount and a subsequent reboot by the user.
 */
static void storage_menu(void)
{
    char target[NW_PATH], local[NW_PATH], manifest[NW_PATH], seed_text[16], line[80], title[80];
    unsigned long seed;
    int k, ok, extended, target_missing, local_missing;
    dialog(storage_label(title, ST_UITITLE), storage_label(line, ST_UIMENU));
    video_flush(); k = key_read();
    if (k >= 'A' && k <= 'Z') k += 'a' - 'A';
    if (k != 'q' && k != 'l' && k != 'v' && k != 'c') return;
    if (k == 'q' || k == 'l') {
        extended = k == 'l'; strcpy(target, panes[active].path); strcpy(local, startup);
        if (!prompt(storage_label(line, ST_UITARGET), target, sizeof(target)) ||
            !prompt(storage_label(line, ST_UILOCAL), local, sizeof(local))) return;
        sprintf(seed_text, "%lu", (unsigned long)time(NULL) & 0xffffffffUL);
        if (!prompt(storage_label(line, ST_UISEED), seed_text, sizeof(seed_text))) return;
        if (!storage_seed(seed_text, &seed)) { notice(storage_label(line, ST_UIBADSEED)); return; }
        if (!storage_directory(target, target, 0, &target_missing) ||
            !storage_directory(local, local, 0, &local_missing)) { notice(nw_error); return; }
        if (toupper(target[0]) == toupper(local[0])) {
            notice(storage_label(line, ST_OTHERDRIVE)); return;
        }
        if (!confirm(storage_label(line, extended ? ST_UILONG : ST_UIQUICK))) return;
        /* Collect both approvals before creating either directory tree. */
        if ((target_missing && !storage_create_confirm(target)) ||
            (local_missing && !storage_create_confirm(local))) return;
        if ((target_missing && !storage_directory(target, target, 1, &target_missing)) ||
            (local_missing && !storage_directory(local, local, 1, &local_missing))) {
            notice(nw_error); refresh(); return;
        }
    } else {
        if (current() && name_match("NWT????.DAT", current()->name)) {
            if (!current_path(manifest)) { notice(nw_error); return; }
        } else if (!path_join(manifest, startup, "NWT0001.DAT")) { notice(nw_error); return; }
        if (!prompt(storage_label(line, ST_UIPLAN), manifest, sizeof(manifest))) return;
        if (k == 'c' && !confirm(storage_label(line, ST_UICLEAN))) return;
    }
    progress_path[0] = 0; force_progress = 1; storage_progress = storage_ui;
    storage_started = (unsigned long)biostime(0, 0L); storage_paint_stage = 65535U;
    if (k == 'q' || k == 'l') ok = storage_start(target, local, extended, seed, manifest);
    else if (k == 'v') ok = storage_verify(manifest);
    else ok = storage_cleanup(manifest);
    storage_progress = NULL;
    if (!ok && storage_report.state == STORAGE_RUNNING) storage_report.state = STORAGE_FAILED;
    storage_screen("", progress_path, 0, 0, 0,
                   tick_elapsed((unsigned long)biostime(0, 0L), storage_started), manifest);
    video_flush(); key_read(); refresh();
}
/* Apply defaults, saved settings and command-line overrides in that order.
 * Load panes before changing the display, then run one blocking keyboard loop.
 * Each draw clamps navigation state and each modal action returns to this loop.
 */
int main(int argc, char **argv)
{
    int mode = VIDEO_TEXT, k, i, paths = 0, quit = 0, explicit_mode = 0;
    if (argc > 1 && !stricmp(argv[1], "/unzip")) {
        if (argc != 4) { puts("NW /unzip archive.zip existing-directory"); return 1; }
        /* Command-line extraction never implicitly overwrites existing files. */
        if (!zip_extract(argv[2], argv[3])) { puts(nw_error); return 1; }
        printf("%lu files unpacked\n", operation_files); return 0;
    }
    if (argc > 1 && !stricmp(argv[1], "/unarj")) {
        if (argc != 4) { puts("NW /unarj archive.arj existing-directory"); return 1; }
        if (!arj_extract(argv[2], argv[3])) { puts(nw_error); return 1; }
        printf("%lu files unpacked\n", operation_files); return 0;
    }
    if (!getcwd(startup, sizeof(startup))) { puts("Cannot read current directory"); return 1; }
    strcpy(panes[0].path, startup); strcpy(panes[1].path, startup);
    settings_load(&mode);
    settings_format(settings_record, mode);
    for (i = 1; i < argc; ++i) {
        if (!stricmp(argv[i], "/text")) { mode = VIDEO_TEXT; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/cga")) { mode = VIDEO_CGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/ega")) { mode = VIDEO_EGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/vga")) { mode = VIDEO_VGA; explicit_mode = 1; }
        else if (!stricmp(argv[i], "/?")) {
            puts("NW [/text|/cga|/ega|/vga] [left-directory] [right-directory]\nNW /version | Alt-F1 About"); return 0;
        } else if (!stricmp(argv[i], "/version") || !stricmp(argv[i], "--version")) {
            puts("Nightwatch " NW_VERSION_TAG); return 0;
        } else if (paths < 2 && fs_canonical(argv[i], panes[paths].path)) ++paths;
        else { printf("Invalid argument/path: %s\n", argv[i]); return 1; }
    }
    drive_guard_begin();
    if (!panel_load(&panes[0])) { drive_guard_end(); puts(nw_error); return 1; }
    if (!stricmp(panes[0].path, panes[1].path)) panel_snapshot(&panes[1], &panes[0]);
    else if (!panel_load(&panes[1])) { drive_guard_end(); puts(nw_error); return 1; }
    video_init();
    /* A stale saved mode falls back to text; an explicit unsupported mode fails. */
    if (!explicit_mode && !video_supported(mode)) mode = VIDEO_TEXT;
    if (!video_set(mode)) { drive_guard_end(); puts("Requested display mode is unavailable on this adapter."); return 1; }
    settings_remember();
    while (!quit) {
        Panel *p = &panes[active];
        Entry *e;
        unsigned n;
        draw(); k = key_read(); e = current(); n = strlen(command);
        if (k==0x4800 || k==0x5000 || k==0x4900 || k==0x5100 || k==9 ||
            k==0x5200 || k==' ' || k==16 || k==14 || k==18)
            strcpy(status,"Tab switches panes. F1 shows help. F2 selects display mode.");
        if ((browse_active(p) && (k == KEY_F4 || k == KEY_F6 || k == KEY_F7 || k == KEY_F8 || k == 20)) ||
            (browse_active(&panes[1-active]) && (k == KEY_F5 || k == KEY_F6 || k == 21))) {
            archive_message(ARC_BROWSER_READONLY); notice(nw_error); continue;
        }
        if ((n && (k==0x4800 || k==0x5000)) || k==5 || k==24) {
            input_history(command,sizeof(command),&command_cursor,1,
                          (k==0x4800 || k==5) ? 0x4800 : 0x5000); continue;
        }
        if (k==0x4b00 || k==0x4d00 || k==0x7300 || k==0x7400 || k==0x5300 || (n && (k==0x4700 || k==0x4f00))) {
            input_key(command,sizeof(command),&command_cursor,k); continue;
        }
        if (k==9 && n) { input_complete(command,sizeof(command),&command_cursor,p->path); continue; }
        switch (k) {
        case 12: selection_size(); break;
        case 0x6e00: recursive_find(); break;
        case 1: case 0x1c00: properties(); break;
        case 0x7600: directory_page(1); break;
        case 0x8400: directory_page(0); break;
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
        case KEY_F9: change_drive(); break;
        case KEY_F10: quit = 1; break;
        case 9: active = 1 - active; break;
        case 0x4800: --p->cursor; break;
        case 0x5000: ++p->cursor; break;
        case 0x4900: p->cursor -= pane_rows(); break;
        case 0x5100: p->cursor += pane_rows(); break;
        case 0x4700: p->cursor = 0; break;
        case 0x4f00: p->cursor = p->count - 1; break;
        case 18:
            refresh();
            /* Refresh each distinct drive once; both panes share the cache. */
            panel_space(&panes[0],1);
            panel_space(&panes[1],toupper(panes[0].path[0])!=toupper(panes[1].path[0]));
            break;
        case 6: find_name(0); break;
        case 16: find_name(1); break;
        case 14: find_name(2); break;
        case 19: sort_menu(); break;
        case 23: settings_save(); break;
        case 21: extract_archive(); break;
        case 20: storage_menu(); break;
        case 15: run_shell(NULL); break;
        case 27: command[0] = 0; command_cursor=0; input_history_reset(1,command); break;
        case 8:
            if (n) { input_key(command,sizeof(command),&command_cursor,8); input_history_reset(1,command); }
            else if (!browse_enter(p, "..")) notice(nw_error);
            break;
        case 13:
            if (n) { input_history(command,sizeof(command),&command_cursor,1,13); run_shell(command); command[0] = 0; command_cursor=0; input_history_reset(1,command); }
            else if (e && (e->attr & NW_DIR)) { if (!browse_enter(p, e->name)) notice(nw_error); }
            else if (e && browse_type(e->name)) {
                char source[NW_PATH];
                if (!current_path(source) || !browse_open(p,source)) notice(nw_error);
            } else if (e && !browse_active(p) &&
                       (name_match("*.EXE",e->name) || name_match("*.COM",e->name) || name_match("*.BAT",e->name))) {
                char program[NW_PATH];
                if (current_path(program)) run_shell(program);
            } else viewer(0);
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
            } else if (k>=32 && k<127) { input_key(command,sizeof(command),&command_cursor,k); input_history_reset(1,command); }
            break;
        }
    }
    browse_release(&panes[0]); browse_release(&panes[1]);
    input_history_free(); drive_guard_end(); video_restore(); return 0;
}
