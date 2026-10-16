/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef VIDEO_H
#define VIDEO_H
/* Adapter-independent cell output plus direct graphics primitives. Cell
 * coordinates use an 80-column grid; native graphics use 640-pixel width.
 * Attributes follow DOS text layout: background high nibble, foreground low
 * nibble. Graphics colors are EGA palette indices; CGA maps to black/white. */
#define VIDEO_TEXT 0
#define VIDEO_CGA 1
#define VIDEO_EGA 2
#define VIDEO_VGA 3
#define COLOR_NORMAL 0x17
#define COLOR_ACTIVE 0x1f
#define COLOR_SELECT 0x30
#define COLOR_MARKED 0x1e
#define COLOR_BAR 0x30
#define COLOR_KEY 0x3f
#define COLOR_DIRECTORY 0x1f
#define COLOR_TITLE 0x1e
#define COLOR_DIALOG 0x70
#define ICON_FOLDER 0
#define ICON_PARENT 1
#define ICON_DOCUMENT 2
#define ICON_BINARY 3
#define ICON_PROGRAM 4
#define ICON_ARCHIVE 5
#define ICON_IMAGE 6
#define ICON_DRIVE 7
extern int video_rows, video_mode;
extern int video_height, video_font_height, video_font_width;
extern unsigned long video_bytes;
#ifdef NW_DIAGNOSTICS
extern unsigned long video_cells;
#endif
/* Pixel primitives require a selected graphics mode. Rectangle dimensions
 * exclude the right/bottom edge and are clipped to the display. Masks are
 * row-major bytes, most significant bit first; zero bits leave VRAM intact.
 * Masks reject negative x but clip vertically and at the right edge. Labels
 * reject negative origins and clip whole characters at the right edge;
 * Labels paint both foreground and background colors.
 * Callers supply valid near-memory mask storage and short terminated strings
 * whose length/advance products fit a DOS int. Coordinates/dimensions are
 * screen-scale values; clipping does not protect arbitrary int extremes. */
void video_rect(int x, int y, int width, int height, unsigned color);
void video_label(int x, int y, const char *s, unsigned fg, unsigned bg, int bold);
void video_icon(int x, int y, int icon, unsigned body, unsigned outline, unsigned highlight);
/* Scroll moves the overlapping interior by signed pixel dy. It rounds x
 * outward to byte boundaries: callers must keep those edge gutters uniform,
 * repaint exposed rows and any scrollbar. Returns zero when unsupported. */
int video_scroll(int x, int y, int width, int height, int dy);
void video_mask(int x, int y, const unsigned char *mask, int bytes, int height, unsigned color);
/* Switching desktop/overlay ownership changes generation. Native drawing
 * caches must compare this token and repaint after an overlay or mode switch.
 * overlay_begin(0) preserves pixels outside changed cells; nonzero requests
 * a full background clear at the next flush. */
void video_desktop_begin(void);
void video_overlay_begin(int clear);
unsigned video_generation(void);
/* init captures BIOS mode/page/cursor and probes the adapter. set returns
 * zero without changing mode when unsupported; success discards old content.
 * Success means the adapter check passed; BIOS/VRAM failures are not detected.
 * restore returns to saved BIOS state; resume reselects the app's mode after
 * a child program, invalidating contents and graphics register caches. */
void video_init(void);
int video_set(int mode);
int video_supported(int mode);
void video_restore(void);
void video_resume(void);
/* These routines change the near-memory cell buffer, never VRAM directly.
 * put clips to current rows/80 columns; flush sends changes to the adapter.
 * invalidate forces every cell to be resent, without changing generation.
 * Enter flush with interrupts enabled: the CGA snow path re-enables them. */
void video_clear(unsigned char attr);
void video_put(int x, int y, int ch, unsigned char attr);
void video_text(int x, int y, const char *s, unsigned char attr);
void video_fill(int x, int y, int width, int ch, unsigned char attr);
void video_flush(void);
void video_invalidate(void);
const char *video_name(void);
#endif
