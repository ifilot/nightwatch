/* SPDX-License-Identifier: GPL-3.0-only */
/* Pixel-coordinate desktop. No file-operation logic or framebuffer here. */
#include <stdio.h>
#include <string.h>
#include "VIDEO.H"
#include "GRAPH.H"
#include "HELP.H"
/* Every supported graph_rows() result must fit this painted-row capacity.
 * Current CGA/EGA/VGA layouts expose 18/21/24 rows respectively. */
#define MAX_VISIBLE 24
#define PANEL_WIDTH 310
static const char *actions[] = { "Help", "Mode", "View", "Edit", "Copy", "Move", "Mkdir", "Delete", "Path", "Quit" };
/* This is a cache of what is actually on screen, including the selected
 * appearance. Keep it separate from Panel so moving a cursor usually paints
 * two rows rather than re-reading or redrawing the whole directory. */
typedef struct {
    Entry entry;
    int present, selected;
} Painted;
static Painted painted[2][MAX_VISIBLE];
static char paths[2][NW_PATH], old_status[160], old_command[NW_PATH];
static int old_active = -1, old_count[2], old_cursor[2], old_marks[2], old_limit[2], old_top[2];
static unsigned seen_generation, old_revision[2];
static char summaries[2][64];
static int mono, top, bottom, list_top, row_height, caption_height, column_height, foot_height, command_y, command_height, button_y, icon_height;
static unsigned desktop_color, paper, ink, muted, selection, white;
static void line(int x, int y, int width, unsigned color) { video_rect(x, y, width, 1, color); }
static void outline(int x, int y, int width, int height, unsigned color)
{
    line(x, y, width, color); line(x, y + height - 1, width, color);
    video_rect(x, y, 1, height, color); video_rect(x + width - 1, y, 1, height, color);
}
/* Solid pixel tracks replace character bars only in graphics modes. Cell
 * overlay rows use BIOS spacing, independently of the desktop font metrics. */
void graph_progress(int row, unsigned percent, unsigned previous, int reset)
{
    int h = video_height / video_rows - 2, x = 120;
    int y = row * (video_height / video_rows) + 1, fill, old;
    int black_white = video_mode == VIDEO_CGA;
    if (percent > 100) percent = 100;
    if (previous > 100) previous = 100;
    fill = (318 * percent) / 100;
    old = reset ? 0 : (318 * previous) / 100;
    if (reset) {
        video_rect(x, y, 320, h, black_white ? 0 : 8);
        video_rect(x + 1, y + 1, 318, h - 2, black_white ? 15 : 7);
        if (!black_white) {
            line(x, y + h - 1, 320, 15);
            video_rect(x + 319, y, 1, h, 15);
        }
    }
    if (fill > old) {
        video_rect(x + 1 + old, y + 1, fill - old, h - 2, black_white ? 0 : 1);
        if (!black_white) line(x + 1 + old, y + 1, fill - old, 9);
    } else if (fill < old) {
        /* A new current file can shrink the fill; its surviving portion and
         * the frame remain untouched. Overall progress normally only grows. */
        video_rect(x + 1 + fill, y + 1, old - fill, h - 2, black_white ? 15 : 7);
    }
}
/* The document lives with the immutable fonts, outside DGROUP. Copy only
 * the visible line into near memory for the existing text primitives. */
void help_line(unsigned index, char *out)
{
    const char NW_FAR *source = asset_help(index);
    unsigned i = 0;
    while (i < HELP_WIDTH && source[i]) { out[i] = source[i]; ++i; }
    out[i] = 0;
}
int graph_help_rows(void)
{
    int f = video_font_height;
    return (14 * f - 4) / (f + 2);
}
void graph_help(const char *version, const char *commit, const char *repository,
                unsigned first, int full)
{
    int f = video_font_height, height = f * 17 + 52;
    int x = 18, y = (video_height - height) / 2, body = y + f * 2 + 24;
    int rows = graph_help_rows(), i, heading, length, columns = 548 / video_font_width;
    int track_x = x + 574, track_y = body + 4, track_h = rows * (f + 2);
    int thumb_h = track_h * rows / HELP_LINES, thumb_y;
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    unsigned title = video_mode == VIDEO_CGA ? 0 : 1;
    char text[96];
    video_overlay_begin(0);
    if (full) {
        video_rect(x + 3, y + 3, 604, height, 0);
        video_rect(x, y, 604, height, bg); outline(x, y, 604, height, 0);
        line(x + 1, y + 1, 602, 15);
        video_rect(x + 2, y + 2, 600, f + 6, title);
        video_icon(x + 10, y + 3, ICON_DRIVE, video_mode == VIDEO_CGA ? 0 : 7, 15, 15);
        sprintf(text, "Nightwatch %s", version);
        video_label(x + 32, y + 4, text, 15, title, 0);
        video_label(x + 550, y + 4, "HELP", 15, title, 0);
        video_label(x + 14, y + f + 14, repository, 0, bg, 0);
        sprintf(text, "Commit: %s", commit);
        video_label(x + 330, y + f + 14, text, 0, bg, 0);
        video_rect(x + 14, body, 576, rows * (f + 2) + 8, 15);
        outline(x + 14, body, 576, rows * (f + 2) + 8, video_mode == VIDEO_CGA ? 0 : 8);
        line(x + 14, y + height - f - 15, 576, video_mode == VIDEO_CGA ? 0 : 8);
        video_label(x + 14, y + height - f - 8,
                    "Up/Down PgUp/PgDn Home/End | Esc closes | A About", 0, bg, 0);
    }
    for (i = 0; i < rows; ++i) {
        if (first + i < HELP_LINES) help_line(first + i, text);
        else text[0] = 0;
        heading = text[0] == '#';
        if (heading) memmove(text, text + 1, strlen(text));
        length = strlen(text);
        while (length < columns) text[length++] = ' ';
        text[length] = 0;
        video_label(x + 22, body + 4 + i * (f + 2), text,
                    heading ? title : 0, heading ? bg : 15, 0);
    }
    if (thumb_h < 6) thumb_h = 6;
    thumb_y = track_y + (track_h - thumb_h) * first / (HELP_LINES - rows);
    video_rect(track_x, track_y, 10, track_h, bg);
    outline(track_x, track_y, 10, track_h, video_mode == VIDEO_CGA ? 0 : 8);
    video_rect(track_x + 2, thumb_y + 1, 6, thumb_h - 2, title);
}
/* Compact per-adapter fonts and margins preserve listing density. These
 * metrics also determine graph_rows(), so navigation and painting agree on
 * the usable area instead of assuming the text mode's 25-row geometry. */
static void metrics(void)
{
    mono = video_mode == VIDEO_CGA;
    top = 1;
    icon_height = mono ? 8 : 16;
    caption_height = icon_height + 2;
    column_height = video_font_height + 2;
    row_height = icon_height + (mono ? 0 : 1);
    foot_height = video_font_height + 2;
    button_y = video_height - video_font_height - 2;
    command_height = video_font_height + (mono ? 2 : 4);
    command_y = button_y - command_height;
    bottom = command_y - 2;
    list_top = top + caption_height + column_height;
    desktop_color = mono ? 0 : 7; paper = 15; ink = 0;
    white = 15; muted = mono ? 0 : 8; selection = mono ? 0 : 1;
}
int graph_rows(void)
{
    metrics(); return (bottom - list_top - foot_height - 1) / row_height;
}
static void buttons(void)
{
    int i, x, advance;
    char key[4];
    video_rect(0, button_y, 640, video_font_height + 2, mono ? 15 : 7);
    line(0, button_y, 640, muted);
    for (i = 0; i < 10; ++i) {
        x = i * 64 + 2;
        sprintf(key, "%d", i + 1);
        advance = strlen(key) * video_font_width;
        video_label(x, button_y + 1, key, 15, mono ? 0 : 1, 0);
        video_label(x + advance + 1, button_y + 1, actions[i], ink, mono ? 15 : 7, 0);
    }
}
static void chrome(void)
{
    int p, x, height = bottom - top;
    video_rect(0, 0, 640, video_height, desktop_color);
    for (p = 0; p < 2; ++p) {
        x = 6 + p * 318;
        video_rect(x + PANEL_WIDTH, top + 2, 2, height, muted);
        video_rect(x + 2, bottom, PANEL_WIDTH, 2, muted);
        video_rect(x, top, PANEL_WIDTH, height, paper);
        outline(x, top, PANEL_WIDTH, height, ink);
        video_rect(x + 1, top + caption_height, PANEL_WIDTH - 2, column_height, mono ? 0 : 7);
        video_label(x + 34, top + caption_height + 1, "Name", mono ? 15 : 0, mono ? 0 : 7, 0);
        video_label(x + 177, top + caption_height + 1, "Size", mono ? 15 : 0, mono ? 0 : 7, 0);
        video_label(x + 236, top + caption_height + 1, "Modified", mono ? 15 : 0, mono ? 0 : 7, 0);
        line(x + 1, list_top - 1, PANEL_WIDTH - 2, muted);
        line(x + 1, bottom - foot_height - 1, PANEL_WIDTH - 2, muted);
    }
    buttons();
}
/* DOS directory scans supply uppercase 8.3 names. Extension classification
 * is a visual hint only; it does not imply archive/image handling support. */
static int file_icon(const Entry *e)
{
    const char *ext = strrchr(e->name, '.');
    if (e->attr & NW_DIR) return !strcmp(e->name, "..") ? ICON_PARENT : ICON_FOLDER;
    if (!ext) return ICON_DOCUMENT;
    if (!strcmp(ext, ".EXE") || !strcmp(ext, ".COM") || !strcmp(ext, ".BAT")) return ICON_PROGRAM;
    if (!strcmp(ext, ".ZIP") || !strcmp(ext, ".ARJ") || !strcmp(ext, ".LZH")) return ICON_ARCHIVE;
    if (!strcmp(ext, ".GIF") || !strcmp(ext, ".PCX") || !strcmp(ext, ".BMP")) return ICON_IMAGE;
    if (!strcmp(ext, ".BIN") || !strcmp(ext, ".DAT") || !strcmp(ext, ".SYS")) return ICON_BINARY;
    return ICON_DOCUMENT;
}
static void size_text(char *text, const Entry *e)
{
    if (e->attr & NW_DIR) strcpy(text, "--");
    else if (e->size < 1024) sprintf(text, "%lu B", e->size);
    else if (e->size < 1048576UL) sprintf(text, "%lu K", (e->size + 512) / 1024);
    else sprintf(text, "%lu.%lu M", e->size / 1048576UL, ((e->size % 1048576UL) * 10) / 1048576UL);
}
/* A full desktop paint already provides the unselected paper background.
 * Incremental paints erase the whole row to remove old text, marks and icons;
 * selected rows need their distinct background even on the first paint.
 * VGA/EGA reserve the final scanline as white space between 16-pixel icons. */
static void entry_row(int panel, int row, const Entry *e, int selected, int fresh)
{
    int x = 6 + panel * 318, y = list_top + row * row_height, icon;
    unsigned bg = selected ? selection : paper;
    unsigned fg = selected ? white : ink, body, edge, light;
    char size[16], date[12];
    if (!fresh || selected) video_rect(x + 2, y, PANEL_WIDTH - 8, row_height, bg);
    if (!mono && selected) line(x + 2, y + icon_height, PANEL_WIDTH - 8, paper);
    if (!e) return;
    icon = file_icon(e);
    body = bg; edge = light = fg;
    if (e->marked) {
        video_rect(x + 5, y + row_height / 2 - 2, 4, 4, mono ? fg : 14);
        if (!mono) outline(x + 4, y + row_height / 2 - 3, 6, 6, selected ? 15 : 6);
    }
    video_icon(x + 16, y + (row_height - icon_height) / 2, icon, body, edge, light);
    video_label(x + 34, y + (row_height - video_font_height) / 2, e->name, fg, bg, 0);
    size_text(size, e);
    video_label(x + 226 - strlen(size) * video_font_width, y + (row_height - video_font_height) / 2, size, fg, bg, 0);
    if (e->date) {
        sprintf(date, "%02u-%02u-%02u", (e->date >> 5) & 15, e->date & 31, ((e->date >> 9) + 80) % 100);
        video_label(x + 236, y + (row_height - video_font_height) / 2, date, fg, bg, 0);
    }
}
/* Summaries change on nearly every cursor move. Paint only contiguous runs
 * of changed characters, padding a shorter replacement with spaces so its
 * old tail cannot remain visible. Fixed-width glyphs preserve alignment. */
static void summary(int p, int x, const char *text, int full)
{
    int i, start, length = strlen(text), old_length = strlen(summaries[p]), total;
    char run[64];
    unsigned bg = mono ? 15 : 7;
    if (full) {
        video_rect(x + 1, bottom - foot_height, PANEL_WIDTH - 2, foot_height - 1, bg);
        video_label(x + 8, bottom - foot_height + 1, text, ink, bg, 0);
    } else {
        total = length > old_length ? length : old_length;
        i = 0;
        while (i < total) {
            char now = i < length ? text[i] : ' ';
            char was = i < old_length ? summaries[p][i] : ' ';
            if (now == was) { ++i; continue; }
            start = i;
            do {
                run[i - start] = i < length ? text[i] : ' '; ++i;
            } while (i < total && (i < length ? text[i] : ' ') !=
                     (i < old_length ? summaries[p][i] : ' '));
            run[i - start] = 0;
            video_label(x + 8 + start * video_font_width, bottom - foot_height + 1, run, ink, bg, 0);
        }
    }
    strcpy(summaries[p], text);
}
/* Reconcile Panel state with the painted cache. A video generation change
 * means direct VRAM content was lost or covered; redraw chrome and all rows.
 * Panel revision tracks entry changes, while cursor/top/active track layout
 * and selection. Neither one alone is sufficient to validate a cached row. */
void graph_draw(Panel *panels, int active, const char *status, const char *command)
{
    int p, row, index, rows, marks, x, changed, full, selected, scan, content, delta;
    char text[100];
    unsigned caption, fg;
    video_desktop_begin(); rows = graph_rows();
    full = seen_generation != video_generation();
    if (full) {
        chrome(); memset(painted, 0, sizeof(painted));
        memset(paths, 0, sizeof(paths)); old_active = -1;
        old_status[0] = old_command[0] = 0;
        old_count[0] = old_count[1] = -1;
        seen_generation = video_generation();
    }
    for (p = 0; p < 2; ++p) {
        Panel *panel = &panels[p];
        x = 6 + p * 318;
        panel_scroll(panel, rows);
        if (full || old_active != active || strcmp(paths[p], panel->path)) {
            int max = (PANEL_WIDTH - 36) / video_font_width;
            const char *path = panel->path;
            if (strlen(path) > (unsigned)max) path += strlen(path) - max;
            caption = p == active ? (mono ? 0 : 1) : (mono ? 15 : 8);
            fg = mono && p != active ? 0 : 15;
            video_rect(x + 1, top + 1, PANEL_WIDTH - 2, caption_height - 1, caption);
            video_icon(x + 8, top + (caption_height - icon_height) / 2, ICON_DRIVE, caption, fg, fg);
            video_label(x + 30, top + (caption_height - video_font_height) / 2, path, fg, caption, 0);
            strcpy(paths[p], panel->path);
        }
        content = full || old_top[p] != panel->top || old_revision[p] != panel->revision;
        scan = content || old_cursor[p] != panel->cursor || old_active != active;
        delta = panel->top - old_top[p];
        /* Small unchanged-directory scrolls move pixels and cached rows in
         * tandem. Exposed rows are invalidated, while changed selection is
         * reconciled below. A rejected/unavailable scroll falls back to repaint. */
        if (!full && delta && delta < rows && delta > -rows && old_revision[p] == panel->revision &&
            video_scroll(x + 2, list_top, PANEL_WIDTH - 8, rows * row_height, -delta * row_height)) {
            if (delta > 0) {
                memmove(painted[p], painted[p] + delta, (rows - delta) * sizeof(Painted));
                memset(painted[p] + rows - delta, 0, delta * sizeof(Painted));
                video_rect(x + 2, list_top + (rows - delta) * row_height, PANEL_WIDTH - 8, delta * row_height, paper);
            } else {
                memmove(painted[p] - delta, painted[p], (rows + delta) * sizeof(Painted));
                memset(painted[p], 0, -delta * sizeof(Painted));
                video_rect(x + 2, list_top, PANEL_WIDTH - 8, -delta * row_height, paper);
            }
        }
        /* Entry comparison is needed only after content changes. Cursor-only
         * movement compares selection flags, avoiding whole Entry memcmps. */
        if (scan) {
            for (row = 0; row < rows; ++row) {
                Painted *old = &painted[p][row];
                Entry *e;
                index = panel->top + row;
                e = index < panel->count ? &panel->files[index] : NULL;
                selected = e && index == panel->cursor && p == active;
                changed = full || old->present != (e != NULL) || old->selected != selected;
                if (content && e && (!old->present || memcmp(&old->entry, e, sizeof(Entry)))) changed = 1;
                if (changed) {
                    entry_row(p, row, e, selected, full);
                    old->present = e != NULL; old->selected = selected;
                    if (e) old->entry = *e;
                }
            }
        }
        old_revision[p] = panel->revision;
        if (full || old_count[p] != panel->count || old_top[p] != panel->top || old_active != active) {
            /* Use long intermediates: pixel-height products can exceed the
             * 16-bit signed int range even though the result fits on screen. */
            int track = rows * row_height, thumb, thumb_y;
            video_rect(x + PANEL_WIDTH - 6, list_top, 3, track, paper);
            if (panel->count > rows) {
                thumb = (int)((long)rows * track / panel->count);
                if (thumb < 8) thumb = 8;
                thumb_y = list_top + (int)((long)panel->top * (track - thumb) / (panel->count - rows));
                video_rect(x + PANEL_WIDTH - 6, list_top, 3, track, mono ? 15 : 7);
                video_rect(x + PANEL_WIDTH - 6, thumb_y, 3, thumb, mono ? 0 : 8);
            }
            old_top[p] = panel->top;
        }
        marks = panel->marks;
        if (full || old_count[p] != panel->count || old_cursor[p] != panel->cursor ||
            old_marks[p] != marks || old_limit[p] != panel->truncated) {
            sprintf(text, "%d/%d entries  %d marked%s", panel->count ? panel->cursor + 1 : 0,
                    panel->count, marks, panel->truncated ? " [LIMIT]" : "");
            summary(p, x, text, full);
            old_count[p] = panel->count; old_cursor[p] = panel->cursor;
            old_marks[p] = marks; old_limit[p] = panel->truncated;
        }
    }
    old_active = active;
    if (full || strcmp(status, old_status) || strcmp(command, old_command)) {
        int max = (608 - video_font_width) / video_font_width;
        const char *visible = command;
        video_rect(6, command_y, 628, command_height, paper);
        outline(6, command_y, 628, command_height, ink);
        video_label(10, command_y + (mono ? 1 : 2), ">", ink, paper, 0);
        if (strlen(visible) > (unsigned)max) visible += strlen(visible) - max;
        video_label(24, command_y + (mono ? 1 : 2), visible, ink, paper, 0);
        video_rect(24 + strlen(visible) * video_font_width, command_y + command_height - 2,
                   video_font_width - 1, 1, ink);
        if (!*command && strncmp(status, "Tab switches panes.", 19)) {
            int limit = 400 / video_font_width;
            strncpy(text, status, limit); text[limit] = 0;
            video_label(628 - strlen(text) * video_font_width, command_y + (mono ? 1 : 2),
                        text, mono ? 0 : 1, paper, 0);
        }
        strcpy(old_status, status); strcpy(old_command, command);
    }
}
