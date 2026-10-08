/* SPDX-License-Identifier: GPL-3.0-only */
/* Test-only BIOS/VRAM capture and scripted keys; never linked into NIGHT.EXE. */
#include <assert.h>
const unsigned char far *asset_font(int mode);
static FILE *test_keys;
static int test_frame;
/* Binary evidence: mode byte + little-endian height word, then VRAM bytes.
 * Text stores 4000 cell bytes plus 256 eight-row glyphs; graphics stores logical
 * scanlines, one CGA plane or four EGA/VGA planes. Reads do not count as writes.
 * POS companions record model/counter state for Python assertions. */
static void test_capture(void)
{
    char name[13];
    unsigned char buffer[640], head[3];
    unsigned char far *mem;
    unsigned char far *rom = (unsigned char far *)MK_FP(0xf000, 0xfa6e);
    unsigned offset, at;
    int y, x, plane, height, planes;
    FILE *f;
    union REGS r;
    sprintf(name, "F%03d.BIN", test_frame++);
    f = fopen(name, "wb"); assert(f);
    height = video_mode == VIDEO_EGA ? 350 : (video_mode == VIDEO_VGA ? 480 : 200);
    head[0] = video_mode; head[1] = height & 255; head[2] = height >> 8;
    fwrite(head, 1, 3, f);
    if (video_mode == VIDEO_TEXT) {
        r.h.ah = 15; int86(0x10, &r, &r);
        mem = (unsigned char far *)MK_FP(r.h.al == 7 ? 0xb000 : 0xb800, 0);
        for (y = 0; y < 50; ++y) {
            for (x = 0; x < 80; ++x) buffer[x] = mem[y * 80 + x];
            fwrite(buffer, 1, 80, f);
        }
        for (y = 0; y < 256; ++y) {
            for (x = 0; x < 8; ++x) buffer[x] = y < 128 ? rom[y * 8 + x] : asset_font(VIDEO_CGA)[y * 8 + x];
            fwrite(buffer, 1, 8, f);
        }
    } else {
        planes = video_mode == VIDEO_CGA ? 1 : 4;
        mem = (unsigned char far *)MK_FP(planes == 1 ? 0xb800 : 0xa000, 0);
        for (plane = 0; plane < planes; ++plane) {
            if (planes == 4) { outportb(0x3ce, 4); outportb(0x3cf, plane); }
            for (y = 0; y < height; ++y) {
                offset = planes == 1 ? (y / 2) * 80 + ((y & 1) ? 8192 : 0) : y * 80;
                at = (y & 7) * 80;
                movedata(FP_SEG(mem), offset, FP_SEG((void far *)buffer),
                         FP_OFF((void far *)buffer) + at, 80);
                if ((y & 7) == 7 || y == height - 1)
                    fwrite(buffer, 1, ((y & 7) + 1) * 80, f);
            }
        }
    }
    fclose(f);
    sprintf(name, "F%03d.POS", test_frame - 1);
    f = fopen(name, "w"); assert(f);
    fprintf(f, "%d %d %d %d %d %d %d %lu %lu %u %d %d\n", video_mode,
            video_mode == VIDEO_TEXT ? video_rows - 8 : graph_rows(), active,
            panes[0].top, panes[0].cursor, panes[1].top, panes[1].cursor,
            video_bytes, viewer_pages, fs_list_calls, panes[0].marks, panes[1].marks);
    fclose(f);
}
/* Preserve keyboard frame numbering while saving actual transfer dialogs and
 * their counters. Each emulator run starts a fresh progress sequence. */
static void test_progress_capture(unsigned long done, unsigned long total)
{
    static unsigned sequence;
    char old[13], name[13];
    FILE *f;
    if (!copy_progress) return;
    test_capture(); --test_frame;
    sprintf(old, "F%03d.BIN", test_frame);
    sprintf(name, "P%03u.BIN", sequence);
    remove(name); assert(!rename(old, name));
    sprintf(old, "F%03d.POS", test_frame); remove(old);
    sprintf(name, "P%03u.TXT", sequence++);
    f = fopen(name, "w"); assert(f);
    fprintf(f, "%d %lu %lu %lu %lu %lu %lu %lu %lu %lu\n", counting,
            total_files, total_bytes, operation_files, operation_skipped,
            done, total, operation_bytes, operation_current_bytes, transfer_rate);
    fclose(f);
}
/* Capture before consuming each hexadecimal scripted key, matching production
 * key_read results. Exhaustion is a test failure, not a normal navigator exit. */
static int test_key_read(void)
{
    unsigned key;
    test_capture();
    if (!test_keys) test_keys = fopen("KEYS.TXT", "r");
    if (!test_keys || fscanf(test_keys, "%x", &key) != 1) {
        video_restore(); puts("FAIL: script ran out of keys"); exit(1);
    }
    return key;
}
