/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef GRAPH_H
#define GRAPH_H
#include "NW.H"
#include "MINE.H"
/* Native graphics desktop, sharing Panel data and navigation with text mode.
 * Call only after selecting a graphics mode. graph_rows() is the current
 * listing capacity; graph_draw() clamps panel scrolling to that capacity.
 * panels contains two valid Panel records; active is 0 or 1. Strings are
 * borrowed during the draw and NUL-terminated. Paths/command fit NW_PATH bytes,
 * including NUL; status fits 160 bytes, including NUL, for the renderer caches.
 * Drawing uses direct VRAM primitives, not the cell buffer/video_flush(). */
int graph_rows(void);
void graph_command_cursor(unsigned cursor);
void graph_draw(Panel *panels, int active, const char *status, const char *command);
/* Shared native modal chrome. Coordinates are pixels. Paint body first;
 * this adds only shadow, frame, title strip and icon, preserving body pixels. */
void graph_window_frame(int x, int y, int width, int height,
                        const char *title, const char *badge, int icon);
/* Native Help keeps its chrome fixed while scrolling the embedded document. */
int graph_help_rows(void);
void graph_help(const char *version, const char *commit, const char *repository,
                unsigned first, int full);
/* Native document windows share Help's frame, paper and fixed controls. */
int graph_view_rows(void);
int graph_view_columns(void);
void graph_view_begin(int hex, const char *path, int full);
void graph_view_line(int row, const char *text, int hex);
void graph_view_footer(const char *position, const char *controls);
void graph_about(const char *version, const char *commit, const char *repository,
                 const char **credits);
/* Null previous paints the full game; otherwise repaint changed tiles only. */
void graph_mine(const Mine *game, const Mine *previous);
/* Native copy/unpack modal with cached text fields and differential tracks.
 * End releases its small far cache. Track row 0 is file, row 1 is overall.
 * Reset initializes a new/covered track; percentages are clipped. */
void graph_copy_begin(int unpacking);
void graph_copy_text(unsigned field, const char *text);
void graph_copy_end(void);
void graph_progress(int row, unsigned percent, unsigned previous, int reset);
#endif
