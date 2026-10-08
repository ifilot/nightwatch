/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef GRAPH_H
#define GRAPH_H
#include "NW.H"
/* Native graphics desktop, sharing Panel data and navigation with text mode.
 * Call only after selecting a graphics mode. graph_rows() is the current
 * listing capacity; graph_draw() clamps panel scrolling to that capacity.
 * panels contains two valid Panel records; active is 0 or 1. Strings are
 * borrowed during the draw and NUL-terminated. Paths/command fit NW_PATH bytes,
 * including NUL; status fits 160 bytes, including NUL, for the renderer caches.
 * Drawing uses direct VRAM primitives, not the cell buffer/video_flush(). */
int graph_rows(void);
void graph_draw(Panel *panels, int active, const char *status, const char *command);
/* Native Help keeps its chrome fixed while scrolling the embedded document. */
int graph_help_rows(void);
void graph_help(const char *version, const char *commit, const char *repository,
                unsigned first, int full);
/* Draw after the cell overlay flush. Reset initializes a covered/new track;
 * otherwise touch only the difference from previous. Percent is clipped. */
void graph_progress(int row, unsigned percent, unsigned previous, int reset);
#endif
