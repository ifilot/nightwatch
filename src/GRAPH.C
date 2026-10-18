/* SPDX-License-Identifier: GPL-3.0-only */
/* Pixel-coordinate desktop. No file-operation logic or framebuffer here. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef __TURBOC__
#include <alloc.h>
#endif
#include "VIDEO.H"
#include "GRAPH.H"
#include "HELP.H"
#include "DRIVE.H"
#include "BROWSE.H"
/* Every supported graph_rows() result must fit this painted-row capacity.
 * Current CGA/EGA/VGA layouts expose 18/15/22 rows respectively. */
#define MAX_VISIBLE 22
/* Reach both screen edges while retaining the eight-pixel center gutter. */
#define PANEL_WIDTH 316
#define PANEL_STRIDE (640 - PANEL_WIDTH)
static const char *actions[] = { "Help", "Mode", "View", "Edit", "Copy", "Move", "Mkdir", "Delete", "Drive", "Quit" };
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
static unsigned command_cursor, painted_command_cursor=65535U;
void graph_command_cursor(unsigned cursor) { command_cursor=cursor; }
static char summaries[2][64];
static int mono, top, bottom, list_top, row_height, caption_height, column_height, foot_height, command_y, command_height, button_y, icon_height;
static unsigned desktop_color, paper, ink, muted, selection, white;
static void line(int x, int y, int width, unsigned color) { video_rect(x, y, width, 1, color); }
static void outline(int x, int y, int width, int height, unsigned color)
{
    line(x, y, width, color); line(x, y + height - 1, width, color);
    video_rect(x, y, 1, height, color); video_rect(x + width - 1, y, 1, height, color);
}
/* Native copy tracks share the document font and white-paper layout. */
void graph_progress(int row, unsigned percent, unsigned previous, int reset)
{
    int f = video_font_height, h = f - 2, x = 120;
    int height = 9 * f + 58;
    int body = (video_height - height) / 2 + f + 16;
    int y = body + 4 + (row ? 4 : 2) * (f + 2) + 1, fill, old;
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
/* Shared native window chrome: Help, documents and credits use the same
 * title strip, inset paper and shadow instead of BIOS-sized cell dialogs. */
void graph_window_frame(int x, int y, int width, int height,
                        const char *title, const char *badge, int icon)
{
    int f = video_font_height, columns, length = strlen(badge);
    unsigned bar = video_mode == VIDEO_CGA ? 0 : 1;
    char clipped[80];
    /* Narrow shadow strips also let cell-backed dialogs share this chrome
     * after flushing their body, without erasing the body or its selection. */
    video_rect(x + width, y + 3, 3, height, 0);
    video_rect(x + 3, y + height, width, 3, 0);
    outline(x, y, width, height, 0);
    line(x + 1, y + 1, width - 2, 15);
    video_rect(x + 2, y + 2, width - 4, f + 6, bar);
    video_icon(x + 10, y + 3, icon, video_mode == VIDEO_CGA ? 0 : 7, 15, 15);
    columns = (width - 40) / video_font_width - length;
    if (length) columns -= 2;
    if (columns > 79) columns = 79;
    if (columns < 0) columns = 0;
    strncpy(clipped, title, columns); clipped[columns] = 0;
    video_label(x + 32, y + 4, clipped, 15, bar, 0);
    video_label(x + width - 4 - length * video_font_width, y + 4, badge, 15, bar, 0);
}
static int document_window(const char *title, const char *badge, int height, int icon,
                           int width)
{
    int x = (640 - width) / 2, y = (video_height - height) / 2;
    video_native_overlay();
    video_rect(x, y, width, height, video_mode == VIDEO_CGA ? 15 : 7);
    graph_window_frame(x, y, width, height, title, badge, icon);
    return y;
}
static void document_text(int x, int y, const char *text, int columns, unsigned fg, unsigned bg)
{
    char clipped[128];
    strncpy(clipped, text, columns); clipped[columns] = 0;
    video_label(x, y, clipped, fg, bg, 0);
}
/* Transfer fields live outside DGROUP, leaving the small near heap for DOS
 * stdio. Only changed character runs are painted during timed updates. */
static char NW_FAR *copy_fields;
void graph_copy_end(void)
{
    if (copy_fields) {
#ifdef __TURBOC__
        farfree(copy_fields);
#else
        free(copy_fields);
#endif
        copy_fields = NULL;
    }
}
void graph_copy_begin(int unpacking)
{
    int f = video_font_height, height = 9 * f + 58, y, body, i;
    unsigned muted = video_mode == VIDEO_CGA ? 0 : 8;
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    y = document_window(unpacking ? "Unpacking archive" : "Copying files",
                        unpacking ? "UNPACK" : "COPY", height,
                        unpacking ? ICON_ARCHIVE : ICON_DOCUMENT, 604);
    body = y + f + 16;
    video_rect(32, body, 576, 7 * (f + 2) + 8, 15);
    outline(32, body, 576, 7 * (f + 2) + 8, muted);
    video_label(40, body + 4 + 2 * (f + 2), "File", 0, 15, 0);
    video_label(40, body + 4 + 4 * (f + 2), "Overall", 0, 15, 0);
    line(32, y + height - f - 15, 576, muted);
    video_label(32, y + height - f - 8, "Esc cancels", 0, bg, 0);
    if (!copy_fields) {
#ifdef __TURBOC__
        copy_fields = (char NW_FAR *)farmalloc(7UL * 80);
#else
        copy_fields = (char *)malloc(7 * 80);
#endif
    }
    if (copy_fields) for (i = 0; i < 7; ++i) copy_fields[i * 80] = 0;
}
/* Fields: path, file count, current bytes, overall bytes, speed/ETA, and
 * the two percentages. Paths keep their filename visible when clipped. */
void graph_copy_text(unsigned field, const char *text)
{
    int f = video_font_height, width = video_font_width, row, x = 40, y;
    int length, previous = 0, total, i, start;
    char run[80];
    char NW_FAR *old;
    unsigned fg = (field == 4 || field >= 5) && video_mode != VIDEO_CGA ? 1 : 0;
    if (field > 6) return;
    old = copy_fields ? copy_fields + field * 80 : NULL;
    row = field < 2 ? field : (field == 2 ? 3 : (field == 3 ? 5 :
          (field == 4 ? 6 : (field == 5 ? 2 : 4))));
    if (field >= 5) x = 456;
    y = (video_height - (9 * f + 58)) / 2 + f + 20 + row * (f + 2);
    length = strlen(text);
    if (length > 70) {
        if (!field) text += length - 70;
        length = 70;
    }
    if (!old) {
        video_rect(x, y, (field >= 5 ? 5 : 70) * width, f, 15);
        memcpy(run, text, length); run[length] = 0;
        video_label(x, y, run, fg, 15, 0); return;
    }
    while (old[previous]) ++previous;
    total = length > previous ? length : previous;
    i = 0;
    while (i < total) {
        char now = i < length ? text[i] : ' ';
        char was = i < previous ? old[i] : ' ';
        if (now == was) { ++i; continue; }
        start = i;
        do {
            run[i - start] = i < length ? text[i] : ' '; ++i;
        } while (i < total && (i < length ? text[i] : ' ') !=
                 (i < previous ? old[i] : ' '));
        run[i - start] = 0;
        video_label(x + start * width, y, run, fg, 15, 0);
    }
    for (i = 0; i < length; ++i) old[i] = text[i];
    old[length] = 0;
}
int graph_view_columns(void)
{
    int columns = 560 / video_font_width;
    return columns > 80 ? 80 : columns;
}
int graph_view_rows(void)
{
    return (video_height - 20 - 4 * video_font_height - 62) / (video_font_height + 2);
}
static int document_body(void) { return 10 + 2 * video_font_height + 24; }
void graph_view_begin(int hex, const char *path, int full)
{
    int f = video_font_height, body = document_body(), rows = graph_view_rows();
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    if (full) {
        document_window(hex ? "Hex viewer" : "Text viewer", hex ? "HEX" : "VIEW",
                        video_height - 20, ICON_DOCUMENT, 604);
        document_text(32, 10 + f + 14, path, 576 / video_font_width, 0, bg);
    }
    video_rect(32, body, 576, rows * (f + 2) + 8, 15);
    outline(32, body, 576, rows * (f + 2) + 8, video_mode == VIDEO_CGA ? 0 : 8);
    if (hex) video_label(40, body + 4,
        "Offset    0001 0203 0405 0607 0809 0A0B 0C0D 0E0F   ASCII",
        video_mode == VIDEO_CGA ? 0 : 1, 15, 0);
}
void graph_view_line(int row, const char *text, int hex)
{
    int y = document_body() + 4 + row * (video_font_height + 2), i, j, at;
    char compact[80], address[9];
    unsigned accent = video_mode == VIDEO_CGA ? 0 : 1;
    if (!hex) { video_label(40, y, text, 0, 15, 0); return; }
    /* Preserve sixteen bytes and their ASCII, grouping hex pairs to fit the
     * same paper width even with VGA's full eight-pixel font. */
    memcpy(compact, text, 10); at = 10;
    for (i = 0; i < 16; ++i) {
        j = 10 + i * 3 + (i >= 8);
        compact[at++] = text[j]; compact[at++] = text[j + 1];
        if ((i & 1) && i < 15) compact[at++] = ' ';
    }
    compact[at++] = ' '; compact[at++] = ' ';
    memcpy(compact + at, text + 61, 18); compact[at + 18] = 0;
    video_label(40, y, compact, 0, 15, 0);
    memcpy(address, text, 8); address[8] = 0;
    video_label(40, y, address, accent, 15, 0);
    video_label(40 + at * video_font_width, y, compact + at, accent, 15, 0);
}
void graph_view_footer(const char *position, const char *controls)
{
    int f = video_font_height, height = video_height - 20;
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    line(32, 10 + height - 2 * f - 25, 576, video_mode == VIDEO_CGA ? 0 : 8);
    video_rect(32, 10 + height - 2 * f - 20, 576, 2 * f + 14, bg);
    document_text(32, 10 + height - 2 * f - 20, position, 576 / video_font_width,
                  video_mode == VIDEO_CGA ? 0 : 1, bg);
    document_text(32, 10 + height - f - 8, controls, 576 / video_font_width, 0, bg);
}
void graph_about(const char *version, const char *commit, const char *repository,
                 const char **credits)
{
    int f = video_font_height, paper_height = 6 * (f + 2) + 8;
    int portrait = video_mode == VIDEO_EGA || video_mode == VIDEO_VGA;
    int image_width = portrait ? 160 : 0, image_x, text_x, text_y;
    int height, x, y, i, body, width, needed;
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    char text[80];
    sprintf(text, "Nightwatch %s", version);
    /* Fit the longest field, allowing for the icon, badge and paper margins. */
    width = (strlen(text) + 5) * video_font_width + 52;
    needed = strlen(repository) * video_font_width + 28;
    if (needed > width) width = needed;
    needed = strlen("Press any key to return") * video_font_width + 28;
    if (needed > width) width = needed;
    for (i = 0; i < 5; ++i) {
        needed = strlen(credits[i]) * video_font_width + 44;
        if (needed > width) width = needed;
    }
    sprintf(text, "Commit: %s", commit);
    needed = strlen(text) * video_font_width + 44;
    if (needed > width) width = needed;
    width += image_width;
    if (width > 604) width = 604;
    if (portrait && paper_height < 184) paper_height = 184;
    height = 3 * f + 46 + paper_height;
    x = (640 - width) / 2;
    sprintf(text, "Nightwatch %s", version);
    y = document_window(text, "ABOUT", height, ICON_DRIVE, width);
    document_text(x + 14, y + f + 14, repository, (width - 28) / video_font_width, 0, bg);
    body = y + 2 * f + 24;
    video_rect(x + 14 + image_width, body, width - 28 - image_width, paper_height, 15);
    outline(x + 14 + image_width, body, width - 28 - image_width, paper_height,
            video_mode == VIDEO_CGA ? 0 : 8);
    if (portrait) {
        image_x = (x + 23) & ~7;
        /* Two single-pixel rings inset the unscaled artwork into the gray
         * dialog surface, separately from the white credits paper. */
        outline(image_x - 2, body + 2, 148, 180, 15);
        outline(image_x - 1, body + 3, 146, 178, 8);
        video_about_image(image_x, body + 4);
    }
    text_x = x + 22 + image_width;
    text_y = body + (paper_height - 6 * (f + 2)) / 2;
    for (i = 0; i < 5; ++i)
        document_text(text_x, text_y + i * (f + 2), credits[i], (width - 44 - image_width) / video_font_width,
                      i < 2 && video_mode != VIDEO_CGA ? 1 : 0, 15);
    sprintf(text, "Commit: %s", commit);
    document_text(text_x, text_y + 5 * (f + 2), text, (width - 44 - image_width) / video_font_width, 0, 15);
    line(x + 14, y + height - f - 15, width - 28, video_mode == VIDEO_CGA ? 0 : 8);
    video_label(x + 14, y + height - f - 8, "Press any key to return", 0, bg, 0);
    video_label(x + width - 14 - 11 * video_font_width, y + height - f - 8,
                "G: OFF WORK", 0, bg, 0);
}
/* Original geometric tiles use the desktop palette and remain readable in
 * one-bit CGA. No framebuffer, sprites or Windows assets are needed. */
static void mine_tile(const Mine *game, int at, int x, int y, int height)
{
    unsigned char cell = game->cells[at];
    int mono = video_mode == VIDEO_CGA, cx = 12, cy = height / 2, row;
    int ended = game->state >= MINE_WON;
    int open = (cell & MINE_OPEN) || (ended && (cell & MINE_BOMB));
    unsigned face = open ? 15 : (mono ? 0 : 7);
    unsigned fg;
    char number[2];
    static const unsigned colors[] = {0,1,2,4,5,6,3,0,8};
    /* Fill the selected tile as well as outlining it: a one-pixel edge
     * disappears into the white grid. CGA reverses the tile's polarity. */
    if (at == game->cursor) face = mono ? (open ? 0 : 15) : 7;
    fg = mono && face == 0 ? 15 : 0;
    video_rect(x, y, 24, height, mono ? 0 : 8);
    video_rect(x + 1, y + 1, 22, height - 2, face);
    if (!open) {
        line(x + 1, y + 1, 22, 15);
        video_rect(x + 1, y + 1, 1, height - 2, 15);
    }
    if (at == game->hit) {
        face = mono ? 15 : 12;
        fg = 0;
        video_rect(x + 1, y + 1, 22, height - 2, face);
    }
    if ((cell & MINE_FLAG) && !(game->state == MINE_LOST && (cell & MINE_BOMB))) {
        if (game->state == MINE_LOST && !(cell & MINE_BOMB)) {
            video_label(x + (24-video_font_width)/2, y + (height-video_font_height)/2,
                        "X", fg, face, 0);
        } else {
            video_rect(x + cx - 2, y + cy - 4, 1, 8, fg);
            line(x + cx - 4, y + cy + 3, 6, fg);
            for (row = 0; row < 4; ++row)
                line(x + cx - 1, y + cy - 4 + row, 4-row, mono ? fg : 4);
        }
    } else if (open && (cell & MINE_BOMB)) {
        video_rect(x + cx - 2, y + cy - 2, 5, 5, fg);
        line(x + cx - 4, y + cy, 9, fg);
        video_rect(x + cx, y + cy - 4, 1, 9, fg);
        video_rect(x + cx - 1, y + cy - 1, 1, 1, fg ? 0 : 15);
    } else if (open && (cell & 15)) {
        number[0] = '0' + (cell & 15); number[1] = 0;
        video_label(x + (24-video_font_width)/2, y + (height-video_font_height)/2,
                    number, mono ? fg : colors[cell & 15], face, 0);
    }
    if (at == game->cursor) {
        outline(x, y, 24, height, 0);
        outline(x + 1, y + 1, 22, height - 2, mono ? 15 : 1);
    }
}
void graph_mine(const Mine *game, const Mine *previous)
{
    int f = video_font_height, size = video_mode == VIDEO_CGA ? 12 :
                                 (video_mode == VIDEO_EGA ? 20 : 24);
    int height = 9 * size + 3 * f + 54;
    int y = (video_height - height) / 2, bx = 72, by = y + 2 * f + 28;
    int sx = bx + 216 + 24, i;
    unsigned bg = video_mode == VIDEO_CGA ? 15 : 7;
    unsigned accent = video_mode == VIDEO_CGA ? 0 : 1;
    char text[48];
    if (video_mode == VIDEO_TEXT) return;
    if (!previous) {
        document_window("Nightwatch Minefield", "OFF DUTY", height, ICON_IMAGE, 540);
        video_label(64, y + f + 14, "9 x 9 field / 10 mines", 0, bg, 0);
        /* Tall CGA/EGA pixels need wider tiles to approach square cells on
         * a 4:3 display. The board keeps a shared 216-pixel width. */
        outline(bx - 2, by - 2, 220, 9 * size + 4, video_mode == VIDEO_CGA ? 0 : 8);
        outline(bx - 1, by - 1, 218, 9 * size + 2, 15);
        video_label(sx, by + 3*(f+4), "Arrows   Move", 0, bg, 0);
        video_label(sx, by + 4*(f+4), "Enter    Reveal / chord", 0, bg, 0);
        video_label(sx, by + 5*(f+4), "F        Flag / unflag", 0, bg, 0);
        video_label(sx, by + 6*(f+4), "N        New game", 0, bg, 0);
        video_label(sx, by + 7*(f+4), "Match adjacent flags", 0, bg, 0);
        video_label(sx, by + 8*(f+4), "to a number to chord.", 0, bg, 0);
        line(64, y + height - f - 15, 512, video_mode == VIDEO_CGA ? 0 : 8);
        video_label(64, y + height - f - 8, "Space also reveals | Esc returns to About", 0, bg, 0);
    }
    for (i = 0; i < MINE_CELLS; ++i)
        if (!previous || previous->cells[i] != game->cells[i] ||
            previous->state != game->state ||
            (previous->cursor != game->cursor && (i == previous->cursor || i == game->cursor)))
            mine_tile(game, i, bx + i%9*24, by + i/9*size, size);
    if (!previous || previous->flags != game->flags || previous->opened != game->opened ||
        previous->turns != game->turns || previous->state != game->state) {
        video_rect(sx, by, 576-sx, 3*(f+4), bg);
        sprintf(text, "Flags: %2d / 10", game->flags);
        video_label(sx, by, text, accent, bg, 0);
        sprintf(text, "Safe:  %2d / 71", game->opened);
        video_label(sx, by + f+4, text, 0, bg, 0);
        sprintf(text, "Turns: %d", game->turns);
        video_label(sx, by + 2*(f+4), text, 0, bg, 0);
        video_rect(248, y + f + 14, 328, f, bg);
        video_label(256, y + f + 14,
            game->state == MINE_READY ? "First reveal is safe" :
            (game->state == MINE_WON ? "Field cleared! N plays again" :
            (game->state == MINE_LOST ? "Mine hit. N tries again" : "Clear every safe square")), accent, bg, 0);
    }
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
        sprintf(text, "Nightwatch %s", version);
        document_window(text, "HELP", height, ICON_DRIVE, 604);
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
    int rows;
    metrics(); rows = (bottom - list_top - foot_height - 1) / row_height;
    /* Keep future layout changes within the fixed painted-row cache. */
    return rows > MAX_VISIBLE ? MAX_VISIBLE : rows;
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
        x = p * PANEL_STRIDE;
        if (!p) video_rect(x + PANEL_WIDTH, top + 2, 2, height, muted);
        video_rect(x, bottom, PANEL_WIDTH, 2, muted);
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
    int x = panel * PANEL_STRIDE, y = list_top + row * row_height, icon;
    unsigned bg = selected ? selection : paper;
    unsigned fg = selected ? white : ink, body, edge, light;
    char size[16], date[12];
    if (!fresh || selected) video_rect(x + 1, y, PANEL_WIDTH - 2, row_height, bg);
    if (!mono && selected) line(x + 1, y + icon_height, PANEL_WIDTH - 2, paper);
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
    char text[160];
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
        x = p * PANEL_STRIDE;
        panel_scroll(panel, rows);
        if (full || old_active != active || strcmp(paths[p], panel->path)) {
            int max = (PANEL_WIDTH - 36) / video_font_width;
            const char *path = panel->path;
            if (strlen(path) > (unsigned)max) path += strlen(path) - max;
            caption = p == active ? (mono ? 0 : 1) : (mono ? 15 : 8);
            fg = mono && p != active ? 0 : 15;
            video_rect(x + 1, top + 1, PANEL_WIDTH - 2, caption_height - 1, caption);
            if (video_mode==VIDEO_VGA) {
                unsigned letter=(unsigned char)panel->path[0];
                if (letter>='a' && letter<='z') letter-='a'-'A';
                video_drive_icon(x+8,top+(caption_height-icon_height)/2,
                                 drive_kind(letter-'A'));
            } else video_icon(x + 8, top + (caption_height - icon_height) / 2, ICON_DRIVE, caption, fg, fg);
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
            video_scroll(x + 1, list_top, PANEL_WIDTH - 2, rows * row_height, -delta * row_height)) {
            if (delta > 0) {
                memmove(painted[p], painted[p] + delta, (rows - delta) * sizeof(Painted));
                memset(painted[p] + rows - delta, 0, delta * sizeof(Painted));
                video_rect(x + 1, list_top + (rows - delta) * row_height, PANEL_WIDTH - 2, delta * row_height, paper);
            } else {
                memmove(painted[p] - delta, painted[p], (rows + delta) * sizeof(Painted));
                memset(painted[p], 0, -delta * sizeof(Painted));
                video_rect(x + 1, list_top, PANEL_WIDTH - 2, -delta * row_height, paper);
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
        if (scan || old_count[p] != panel->count) {
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
            /* Keep the active row continuous through the scrollbar lane.
             * Repainting the track on cursor changes restores the old row. */
            if (p == active && panel->count && panel->cursor >= panel->top &&
                panel->cursor < panel->top + rows) {
                int y = list_top + (panel->cursor - panel->top) * row_height;
                video_rect(x + PANEL_WIDTH - 6, y, 3, icon_height, selection);
                if (!mono) line(x + PANEL_WIDTH - 6, y + icon_height, 3, paper);
            }
            old_top[p] = panel->top;
        }
        marks = panel->marks;
        if (full || content || old_count[p] != panel->count || old_cursor[p] != panel->cursor ||
            old_marks[p] != marks || old_limit[p] != panel->truncated) {
            sprintf(text, "%d/%d entries  %d marked%s", panel->count ? panel->cursor + 1 : 0,
                    panel->count, marks, panel->truncated ? " [LIMIT]" : "");
            if (!browse_active(panel) && (panel->offset || panel->truncated))
                sprintf(text,"Page %lu: %d/%d %d marked%s",panel->offset/(NW_FILES-(strlen(panel->path)>3))+1,
                        panel->count ? panel->cursor+1 : 0,panel->count,marks,panel->truncated ? " [MORE]" : "");
            summary(p, x, text, full);
            old_count[p] = panel->count; old_cursor[p] = panel->cursor;
            old_marks[p] = marks; old_limit[p] = panel->truncated;
        }
    }
    old_active = active;
    if (full || strcmp(status, old_status) || strcmp(command, old_command) || command_cursor!=painted_command_cursor) {
        int max = (608 - video_font_width) / video_font_width;
        const char *visible = command;
        unsigned start=command_cursor>(unsigned)max ? command_cursor-max : 0;
        video_rect(0, command_y, 640, command_height, paper);
        outline(0, command_y, 640, command_height, ink);
        video_label(4, command_y + (mono ? 1 : 2), ">", ink, paper, 0);
        visible+=start;
        strncpy(text,visible,max); text[max]=0;
        video_label(24, command_y + (mono ? 1 : 2), text, ink, paper, 0);
        video_rect(24 + (command_cursor-start) * video_font_width, command_y + command_height - 2,
                   video_font_width - 1, 1, ink);
        if (!*command && strncmp(status, "Tab switches panes.", 19)) {
            int limit = 592 / video_font_width;
            strncpy(text, status, limit); text[limit] = 0;
            video_label(628 - strlen(text) * video_font_width, command_y + (mono ? 1 : 2),
                        text, mono ? 0 : 1, paper, 0);
        }
        strcpy(old_status, status); strcpy(old_command, command);
        painted_command_cursor=command_cursor;
    }
}
