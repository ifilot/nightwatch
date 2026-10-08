/* SPDX-License-Identifier: GPL-3.0-only */
#include <dos.h>
#include <conio.h>
#include <string.h>
#include <mem.h>
#include "NW.H"
#include "VIDEO.H"
#include "ASSETS.H"
const unsigned char far *asset_font(int mode);
/* All mutable buffers live in the near data segment. Keeping
 * full pixel frames here would exhaust its 64K budget; graphics instead go
 * directly to far VRAM. screen is desired cell state, previous is last sent
 * state. Glyphs are read directly from their immutable far segment. */
static unsigned screen[NW_COLS * NW_ROWS];
static unsigned previous[NW_COLS * NW_ROWS];
static const unsigned char far *font;
/* Precompute multiplications and CGA bank selection outside hot loops.
 * CGA stores alternate scanlines in banks 8192 bytes apart; EGA/VGA use
 * linear 80-byte scanlines in each of four parallel color planes. */
static unsigned glyph_offset[256], scan_offset[480];
/* One 256-pixel label chunk, not a screen framebuffer. */
static unsigned char raster[16][32];
static unsigned char inverse[81], solid[80];
static int reg_mask = -1, reg_color = -1, reg_enable = -1, reg_planes = -1;
static int desktop, overlay_clear;
static unsigned generation;
static volatile unsigned char far *pixels;
static unsigned far *textmem;
static int original_mode, original_page, original_cursor, original_pos;
static int adapter, ega_memory = -1, glyph_height, invalid = 1;
int video_rows = 25, video_mode = VIDEO_TEXT;
int video_height = 200, video_font_height = 8, video_font_width = 8;
unsigned long video_bytes;
#ifdef NW_DIAGNOSTICS
unsigned long video_cells;
#define COUNT_VIDEO(n) (video_bytes += (n))
#else
#define COUNT_VIDEO(n) ((void)0)
#endif
static void set_bios_mode(int mode)
{
    union REGS r;
    r.h.ah = 0; r.h.al = mode; int86(0x10, &r, &r);
}
/* BIOS display-combination probing is available on VGA, but older EGA BIOS
 * uses AH=12h/BL=10h. The unchanged BL sentinel prevents treating a BIOS
 * which ignores that call as an EGA. Mode 7 is the monochrome text fallback. */
void video_init(void)
{
    union REGS r;
    r.h.ah = 15; int86(0x10, &r, &r);
    original_mode = r.h.al & 127; original_page = r.h.bh;
    r.h.ah = 3; r.h.bh = original_page; int86(0x10, &r, &r);
    original_cursor = r.x.cx; original_pos = r.x.dx;
    adapter = original_mode == 7 ? 0 : 1;
    r.x.ax = 0x1a00; r.x.bx = 0; int86(0x10, &r, &r);
    if (r.h.al == 0x1a) {
        if (r.h.bl == 7 || r.h.bl == 8) adapter = 3;
        else if (r.h.bl == 4 || r.h.bl == 5) adapter = 2;
    }
    if (adapter < 3) {
        r.h.ah = 0x12; r.h.bl = 0x10; int86(0x10, &r, &r);
        if (r.h.bl != 0x10 && r.h.bl <= 3) {
            adapter = 2; ega_memory = r.h.bl;
        }
    }
}
/* EGA memory code zero means 64K: insufficient for our 16-color 640x350
 * mode. Unknown EGA memory remains allowed; VGA has enough for both modes. */
int video_supported(int mode)
{
    if (mode == VIDEO_TEXT) return 1;
    if (mode == VIDEO_CGA) return adapter >= 1;
    if (mode == VIDEO_EGA) return adapter >= 2 && ega_memory != 0;
    return mode == VIDEO_VGA && adapter >= 3;
}
int video_set(int mode)
{
    union REGS r;
    int i;
    if (!video_supported(mode)) return 0;
    video_mode = mode;
    video_rows = mode == VIDEO_VGA ? 30 : 25;
    /* Cell overlays retain 8-pixel advance and BIOS-like row spacing. The
     * native desktop uses tighter 6/7/8-pixel fonts independently, allowing
     * more directory content without changing shared dialog coordinates. */
    glyph_height = mode == VIDEO_CGA ? 8 : (mode == VIDEO_EGA ? 14 : 16);
    video_height = mode == VIDEO_EGA ? 350 : (mode == VIDEO_VGA ? 480 : 200);
    video_font_height = mode == VIDEO_CGA ? 8 : (mode == VIDEO_EGA ? 12 : 16);
    video_font_width = mode == VIDEO_CGA ? 6 : (mode == VIDEO_EGA ? 7 : 8);
    desktop = overlay_clear = 0; ++generation;
    reg_mask = reg_color = reg_enable = reg_planes = -1;
    if (mode == VIDEO_TEXT) {
        set_bios_mode(original_mode == 7 ? 7 : 3);
        textmem = (unsigned far *)MK_FP(original_mode == 7 ? 0xb000 : 0xb800, 0);
        r.h.ah = 1; r.x.cx = 0x2000; int86(0x10, &r, &r);
    } else {
        set_bios_mode(mode == VIDEO_CGA ? 6 : (mode == VIDEO_EGA ? 0x10 : 0x12));
        pixels = (unsigned char far *)MK_FP(mode == VIDEO_CGA ? 0xb800 : 0xa000, 0);
        font = asset_font(mode);
        for (i = 0; i < 256; ++i) glyph_offset[i] = i * video_font_height;
        for (i = 0; i < video_height; ++i)
            scan_offset[i] = mode == VIDEO_CGA ? (unsigned)(i >> 1) * 80 + ((i & 1) ? 8192 : 0) : (unsigned)i * 80;
        if (mode != VIDEO_CGA) {
            /* Write mode 0, unrotated bytes, no set/reset, full bit mask. */
            outportb(0x3ce, 1); outportb(0x3cf, 0);
            outportb(0x3ce, 3); outportb(0x3cf, 0);
            outportb(0x3ce, 5); outportb(0x3cf, 0);
            outportb(0x3ce, 8); outportb(0x3cf, 255);
        }
    }
    video_invalidate(); return 1;
}
void video_restore(void)
{
    union REGS r;
    set_bios_mode(original_mode);
    r.h.ah = 5; r.h.al = original_page; int86(0x10, &r, &r);
    r.h.ah = 1; r.x.cx = original_cursor; int86(0x10, &r, &r);
    r.h.ah = 2; r.h.bh = original_page; r.x.dx = original_pos; int86(0x10, &r, &r);
}
void video_resume(void) { video_set(video_mode); }
const char *video_name(void)
{
    static const char *names[] = { "TEXT", "CGA 640x200", "EGA 640x350", "VGA 640x480" };
    return names[video_mode];
}
void video_invalidate(void) { invalid = 1; }
void video_put(int x, int y, int ch, unsigned char attr)
{
    if (x >= 0 && x < NW_COLS && y >= 0 && y < video_rows) {
        screen[y * NW_COLS + x] = ((unsigned)attr << 8) | (ch & 255);
#ifdef NW_DIAGNOSTICS
        ++video_cells;
#endif
    }
}
void video_fill(int x, int y, int width, int ch, unsigned char attr)
{
    int i; for (i = 0; i < width; ++i) video_put(x + i, y, ch, attr);
}
void video_text(int x, int y, const char *s, unsigned char attr)
{
    while (*s && x < NW_COLS) video_put(x++, y, (unsigned char)*s++, attr);
}
void video_clear(unsigned char attr)
{
    int y; for (y = 0; y < video_rows; ++y) video_fill(0, y, NW_COLS, ' ', attr);
}
/* Only this backend may write tracked GC/sequencer registers while active.
 * Mode selection/resume invalidates caches after external code. Full fills
 * broadcast one CPU store to all four planes; cached values avoid slow port I/O. */
static void gc(int index, int value, int *cache)
{
    if (*cache == value) return;
    outportb(0x3ce, index); outportb(0x3cf, value); *cache = value;
}
static void planes(int value)
{
    if (reg_planes == value) return;
    outportb(0x3c4, 2); outportb(0x3c5, value); reg_planes = value;
}
static unsigned offset_at(int byte_x, int y)
{
    return scan_offset[y] + byte_x;
}
/* On planar adapters a VRAM read loads all four hardware latches. A write
 * then combines the set/reset color with those latches under the bit mask.
 * The volatile far access preserves that read even though its C value is
 * unused. CGA has no latches and needs an ordinary read/modify/write. */
static void masked_byte(unsigned offset, unsigned char mask, unsigned color)
{
    unsigned char latch;
    if (!mask) return;
    if (video_mode == VIDEO_CGA) {
        latch = pixels[offset];
        pixels[offset] = color ? latch | mask : latch & (unsigned char)~mask;
    } else {
        gc(8, mask, &reg_mask);
        latch = pixels[offset]; pixels[offset] = 0;
    }
    COUNT_VIDEO(1);
}
/* Only partial edge bytes need latches. Full interior bytes can be copied
 * from a near zero/one run using movedata; EGA/VGA set/reset broadcasts the
 * chosen four-bit color to all enabled planes in a single CPU write. */
void video_rect(int x, int y, int width, int height, unsigned color)
{
    int left, right, start, end, row, count;
    unsigned char lm, rm;
    struct SREGS s;
    if (video_mode == VIDEO_TEXT || width <= 0 || height <= 0) return;
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 640) width = 640 - x;
    if (y + height > video_height) height = video_height - y;
    if (width <= 0 || height <= 0) return;
    left = x >> 3; right = (x + width - 1) >> 3;
    lm = 255 >> (x & 7); rm = 255 << (7 - ((x + width - 1) & 7));
    if (video_mode != VIDEO_CGA) {
        planes(15); gc(1, 15, &reg_enable); gc(0, color, &reg_color);
    }
    if (left == right) {
        for (row = y; row < y + height; ++row) masked_byte(offset_at(left, row), lm & rm, color);
        return;
    }
    if (lm != 255)
        for (row = y; row < y + height; ++row) masked_byte(offset_at(left, row), lm, color);
    if (rm != 255)
        for (row = y; row < y + height; ++row) masked_byte(offset_at(right, row), rm, color);
    start = left + (lm != 255); end = right - (rm != 255); count = end - start + 1;
    if (count <= 0) return;
    if (video_mode != VIDEO_CGA) gc(8, 255, &reg_mask);
    memset(solid, video_mode == VIDEO_CGA && color ? 255 : 0, count);
    segread(&s);
    for (row = y; row < y + height; ++row)
        movedata(s.ds, FP_OFF(solid), FP_SEG(pixels), offset_at(start, row), count);
    COUNT_VIDEO((unsigned long)count * height);
}
/* Shift each source byte over at most two VRAM bytes. Adjacent mask pieces
 * may overlap, but only their set bits are painted, so other pixels survive.
 * Callers provide bytes*height bytes; this routine does not allocate memory. */
void video_mask(int x, int y, const unsigned char *mask, int bytes, int height, unsigned color)
{
    int row, col, left = x >> 3, shift = x & 7, py;
    unsigned char bits;
    if (video_mode == VIDEO_TEXT || x < 0 || bytes < 1) return;
    if (video_mode != VIDEO_CGA) { planes(15); gc(1, 15, &reg_enable); gc(0, color, &reg_color); }
    for (row = 0; row < height; ++row) {
        py = y + row;
        if (py < 0 || py >= video_height) continue;
        for (col = 0; col < bytes; ++col) {
            bits = mask[row * bytes + col];
            if (left + col < 80) masked_byte(offset_at(left + col, py), bits >> shift, color);
            if (shift && left + col + 1 < 80)
                masked_byte(offset_at(left + col + 1, py), bits << (8 - shift), color);
        }
    }
}
/* Edge writes retain pixels outside the span by loading VRAM latches (or
 * masking the original CGA byte). type is zero, one, glyph or inverted glyph;
 * unlike masked_byte(), planar set/reset is disabled by opaque_span(). */
static void span_edge(int left, int y, int height, int col, unsigned char mask, int type)
{
    int row;
    unsigned char bits, latch;
    unsigned at;
    if (video_mode != VIDEO_CGA) gc(8, mask, &reg_mask);
    for (row = 0; row < height; ++row) {
        bits = type < 2 ? (type ? 255 : 0) : raster[row][col];
        if (type == 3) bits = ~bits;
        at = offset_at(left, y + row); latch = pixels[at];
        pixels[at] = video_mode == VIDEO_CGA ?
            (latch & (unsigned char)~mask) | (bits & mask) : bits;
    }
    COUNT_VIDEO(height);
}
/* Each plane's foreground/background bit pair selects one of four byte
 * patterns: zero, one, glyph, inverted glyph. Group matching planes so each
 * raster chunk is transferred once per distinct pattern, not once per plane.
 * This avoids repeated glyph lookup and costly port writes on the 8088. */
static void opaque_span(int x, int y, int width, int count, int height, unsigned fg, unsigned bg)
{
    int type, row, col, start, end, left = x >> 3, group, chosen;
    unsigned char lm = 255 >> (x & 7);
    unsigned char rm = 255 << (7 - ((x + width - 1) & 7));
    const unsigned char *source;
    struct SREGS s;
    segread(&s);
    chosen = fg ? (bg ? 1 : 2) : (bg ? 3 : 0);
    for (type = 0; type < 4; ++type) {
        if (video_mode == VIDEO_CGA) { if (type != chosen) continue; }
        else {
            group = type == 0 ? (~fg & ~bg) & 15 : (type == 1 ? fg & bg :
                    (type == 2 ? fg & ~bg : ~fg & bg)) & 15;
            if (!group) continue;
            planes(group); gc(1, 0, &reg_enable);
        }
        if (count == 1) { span_edge(left, y, height, 0, lm & rm, type); continue; }
        start = 0; end = count;
        if (lm != 255) { span_edge(left, y, height, 0, lm, type); ++start; }
        if (rm != 255) { span_edge(left + count - 1, y, height, count - 1, rm, type); --end; }
        if (end <= start) continue;
        if (video_mode != VIDEO_CGA) gc(8, 255, &reg_mask);
        if (type < 2) memset(solid, type ? 255 : 0, count);
        for (row = 0; row < height; ++row) {
            if (type < 2) source = solid;
            else if (type == 2) source = raster[row];
            else {
                for (col = start; col < end; ++col) inverse[col] = ~raster[row][col];
                source = inverse;
            }
            movedata(s.ds, FP_OFF(source + start), FP_SEG(pixels), offset_at(left + start, y + row), end - start);
        }
        COUNT_VIDEO((unsigned long)(end - start) * height);
    }
}
/* Rasterize bounded 256-pixel chunks in near RAM, then transfer them as
 * opaque spans. Advance can be the compact desktop font width or eight for
 * shared cell overlays; glyph data always uses the selected native font.
 * The 16x32 raster covers the largest font without a full-screen buffer. */
static void label_advance(int x, int y, const char *text, unsigned fg, unsigned bg, int bold, int advance)
{
    int length, chunk, row, col, pos, byte, shift, count, height;
    unsigned char bits;
    if (x < 0 || x >= 640 || y < 0 || y >= video_height) return;
    length = strlen(text);
    if (length * advance > 640 - x) length = (640 - x) / advance;
    height = video_font_height;
    if (height > video_height - y) height = video_height - y;
    while (length) {
        chunk = (256 - (x & 7)) / advance;
        if (chunk > length) chunk = length;
        count = ((x & 7) + chunk * advance + 7) >> 3;
        for (row = 0; row < height; ++row) {
            memset(raster[row], 0, count);
            pos = x & 7;
            for (col = 0; col < chunk; ++col) {
                bits = font[glyph_offset[(unsigned char)text[col]] + row];
                if (bold) bits |= bits >> 1;
                byte = pos >> 3; shift = pos & 7;
                raster[row][byte] |= bits >> shift;
                if (shift && byte + 1 < count) raster[row][byte + 1] |= bits << (8 - shift);
                pos += advance;
            }
        }
        opaque_span(x, y, chunk * advance, count, height, fg, bg);
        x += chunk * advance; text += chunk; length -= chunk;
    }
}
void video_label(int x, int y, const char *text, unsigned fg, unsigned bg, int bold)
{
    label_advance(x, y, text, fg, bg, bold, video_font_width);
}
/* VGA/EGA icons have one monochrome mask. CGA keeps its three native layers
 * (outline, body, highlight), drawn in order over transparent zero bits. */
void video_icon(int x, int y, int icon, unsigned body, unsigned outline, unsigned highlight)
{
    const unsigned char *mask;
    int height;
    if (icon < 0 || icon > ICON_DRIVE) return;
    height = video_mode == VIDEO_CGA ? 8 : 16;
    mask = video_mode == VIDEO_CGA ? cga_icon_masks[icon] : icon_masks[icon];
    video_mask(x, y, mask, 2, height, outline);
    if (video_mode == VIDEO_CGA) {
        video_mask(x, y, mask + height * 2, 2, height, body);
        video_mask(x, y, mask + height * 4, 2, height, highlight);
    }
}
/* Copy whole-byte listing interiors; callers repaint the scrollbar afterwards.
 * Gutters inside rounded edge bytes are constant throughout the listing. */
int video_scroll(int x, int y, int width, int height, int dy)
{
#ifdef NW_NO_SCROLL
    (void)x; (void)y; (void)width; (void)height; (void)dy;
    return 0;
#else
    int left, bytes, row, src_y, dst_y, step, lines, col, shift;
    unsigned src, dst;
    unsigned char latch;
    if (video_mode == VIDEO_TEXT || !dy || x < 0 || x + width > 640 ||
        y < 0 || y + height > video_height) return 0;
    shift = dy < 0 ? -dy : dy;
    if (shift >= height) return 0;
    left = x >> 3; bytes = ((x + width + 7) >> 3) - left;
    lines = height - shift;
    /* Copy upward top-to-bottom and downward bottom-to-top, like memmove,
     * so overlapping source rows are read before they can be overwritten. */
    step = dy < 0 ? 1 : -1;
    src_y = dy < 0 ? y + shift : y + lines - 1;
    dst_y = src_y + dy;
    if (video_mode != VIDEO_CGA) {
        planes(15); gc(8, 255, &reg_mask);
        /* Write mode 1 copies the four latches, loaded by each source read,
         * to the destination in one store; the CPU byte value is ignored. */
        outportb(0x3ce, 5); outportb(0x3cf, 1);
    }
    for (row = 0; row < lines; ++row) {
        src = offset_at(left, src_y); dst = offset_at(left, dst_y);
        if (video_mode == VIDEO_CGA) movedata(FP_SEG(pixels), src, FP_SEG(pixels), dst, bytes);
        else for (col = 0; col < bytes; ++col) { latch = pixels[src + col]; pixels[dst + col] = latch; }
        src_y += step; dst_y += step;
    }
    if (video_mode != VIDEO_CGA) { outportb(0x3ce, 5); outportb(0x3cf, 0); }
    COUNT_VIDEO((unsigned long)bytes * lines);
    return 1;
#endif
}
unsigned video_generation(void) { return generation; }
/* Generation invalidates native desktop caches; invalid only controls cell
 * flushes. Keeping these separate permits small overlays over a desktop
 * without first rebuilding every underlying pixel as a cell representation. */
void video_desktop_begin(void)
{
    if (!desktop) { desktop = 1; ++generation; }
}
void video_overlay_begin(int clear)
{
    if (desktop) {
        desktop = 0;
        /* Use matching all-space cell buffers as a synthetic overlay baseline.
         * No desktop pixels are captured; they survive until a cell changes.
         * Generation invalidation restores the native desktop on the next draw. */
        if (video_mode != VIDEO_TEXT) {
            video_clear(COLOR_NORMAL); memcpy(previous, screen, sizeof(screen)); invalid = 0;
        }
        ++generation;
    }
    if (clear) { overlay_clear = 1; invalid = 1; }
}
static int box_char(int ch)
{
    return ch == 205 || ch == 186 || ch == 201 || ch == 187 || ch == 200 || ch == 188;
}
/* Find the corner joined to a frame edge in the logical cell buffer. Native
 * dialog borders belong at the outside of their cells, enclosing all of the
 * background fill. Standalone box-drawing lines retain their centered stroke. */
static int box_edge(int row, int col, int vertical)
{
    int at = row * 80 + col, left = vertical ? row : col, ch;
    while (left-- > 0) {
        at -= vertical ? 80 : 1;
        ch = screen[at] & 255;
        if (ch == (vertical ? 186 : 205)) continue;
        if (ch == 201) return 0;
        if (ch == (vertical ? 187 : 200)) return vertical ? 7 : glyph_height - 1;
        break;
    }
    return vertical ? 4 : glyph_height / 2;
}
/* Translate changed cell runs to pixels for shared dialogs and viewers.
 * CP437 double-line frame glyphs are drawn geometrically, joining at cell
 * boundaries even when the compact font is narrower than the 8-pixel grid.
 * Uniform-attribute text runs use one opaque label transfer per run. */
static void graphics_cells(int row, int first, int last)
{
    int col = first, start, ch, x, y, length, padding;
    unsigned attr, bg, fg;
    char text[81];
    while (col <= last) {
        start = col; attr = screen[row * 80 + col] >> 8;
        bg = (attr >> 4) & 15; fg = attr & 15;
        if (video_mode == VIDEO_CGA) {
            bg = bg == 3 || bg == 7 ? 15 : 0; fg = bg ? 0 : 15;
        }
        ch = screen[row * 80 + col] & 255;
        x = col * 8; y = row * glyph_height;
        if (box_char(ch)) {
            video_rect(x, y, 8, glyph_height, bg);
            if (ch == 205) video_rect(x, y + box_edge(row, col, 0), 8, 1, fg);
            else if (ch == 186) video_rect(x + box_edge(row, col, 1), y, 1, glyph_height, fg);
            else {
                int right = ch == 201 || ch == 200;
                int down = ch == 201 || ch == 187;
                video_rect(x, y + (down ? 0 : glyph_height - 1), 8, 1, fg);
                video_rect(x + (right ? 0 : 7), y, 1, glyph_height, fg);
            }
            ++col; continue;
        }
        length = 0;
        while (col <= last && (screen[row * 80 + col] >> 8) == attr &&
               !box_char(screen[row * 80 + col] & 255))
            text[length++] = screen[row * 80 + col++] & 255;
        text[length] = 0;
        padding = (glyph_height - video_font_height) / 2;
        if (padding) {
            video_rect(x, y, length * 8, padding, bg);
            video_rect(x, y + padding + video_font_height, length * 8,
                       glyph_height - padding - video_font_height, bg);
        }
        label_advance(start * 8, y + padding, text, fg, bg, 0, 8);
    }
}
/* Text sends only changed cells; graphics sends the span between the first
 * and last change in each row, trading some unchanged pixels for fewer label
 * transfers. previous is updated only for that span. On original CGA, wait
 * for the display-safe interval before each text store to avoid snow; keep
 * interrupts off only around that wait/store, then enable them unconditionally.
 * The caller must enter with interrupts enabled; this does not preserve IF. */
void video_flush(void)
{
    int row, col, first, last;
    unsigned at;
    if (video_mode != VIDEO_TEXT && overlay_clear) {
        video_rect(0, 0, 640, video_height, video_mode == VIDEO_CGA ? 0 : 1);
        overlay_clear = 0;
    }
    for (row = 0; row < video_rows; ++row) {
        at = row * NW_COLS;
        first = -1; last = -1;
        for (col = 0; col < 80; ++col) {
            if (!invalid && screen[at + col] == previous[at + col]) continue;
            if (first < 0) first = col;
            last = col;
            if (video_mode == VIDEO_TEXT) {
                if (adapter == 1) {
                    while (inportb(0x3da) & 1) {}
                    disable(); while (!(inportb(0x3da) & 1)) {}
                    textmem[at + col] = screen[at + col]; enable();
                } else textmem[at + col] = screen[at + col];
                COUNT_VIDEO(1);
            }
        }
        if (first < 0) continue;
        if (video_mode != VIDEO_TEXT) graphics_cells(row, first, last);
        memcpy(previous + at + first, screen + at + first, (last - first + 1) * sizeof(unsigned));
    }
    invalid = 0;
}
