/* SPDX-License-Identifier: GPL-3.0-only */
/* Check native dashboard damage and geometry using the production renderer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "STORTEST.H"
#include "STORUI.H"
#include "VIDEO.H"
char nw_error[160];
int video_mode, video_rows, video_height, video_font_height, video_font_width;
static unsigned generation = 1, calls, touched_left, opening_text, entries_text;
unsigned video_generation(void) { return generation; }
void video_overlay_begin(int clear) { (void)clear; }
void video_flush(void) {}
void video_rect(int x, int y, int w, int h, unsigned color)
{
    (void)color;
    assert(x >= 0 && y >= 0 && w >= 0 && h >= 0);
    assert(x + w <= 640 && y + h <= video_height);
    if (w && h) { ++calls; if (x < 316) ++touched_left; }
}
void video_label(int x, int y, const char *s, unsigned fg, unsigned bg, int bold)
{
    (void)fg; (void)bg; (void)bold;
    assert(x >= 0 && y >= 0 && x + (int)strlen(s) * video_font_width <= 640);
    assert(y + video_font_height <= video_height);
    if (strstr(s,"Opening file")) ++opening_text;
    if (strstr(s,"directory entries")) ++entries_text;
    ++calls; if (x < 316) ++touched_left;
}
void video_text(int x, int y, const char *s, unsigned char a)
{ (void)x; (void)y; (void)s; (void)a; }
void video_put(int x, int y, int ch, unsigned char a)
{ (void)x; (void)y; (void)ch; (void)a; }
void video_fill(int x, int y, int w, int ch, unsigned char a)
{ (void)x; (void)y; (void)w; (void)ch; (void)a; }
int main(void)
{
    unsigned before, state;
    for (video_mode = VIDEO_CGA; video_mode <= VIDEO_VGA; ++video_mode) {
        video_font_height = video_mode == VIDEO_CGA ? 8 : video_mode == VIDEO_EGA ? 12 : 16;
        video_font_width = video_mode == VIDEO_CGA ? 5 : video_mode == VIDEO_EGA ? 6 : 8;
        video_height = video_mode == VIDEO_CGA ? 200 : video_mode == VIDEO_EGA ? 350 : 480;
        ++generation;
        memset(&storage_report,0,sizeof(storage_report));
        storage_report.stages=6; storage_report.stage=2; storage_report.passed=3;
        storage_report.seed=4294967295UL;
        storage_screen("VERIFY","S:\\NWT0001\\DATA\\NEST\\BIG.BIN",512,1048576UL,4096,18,NULL);
        before=calls;
        storage_screen("VERIFY","S:\\NWT0001\\DATA\\NEST\\BIG.BIN",512,1048576UL,4096,18,NULL);
        assert(calls==before);
        touched_left=0;
        storage_report.verified=65536;
        storage_screen("VERIFY","S:\\NWT0001\\DATA\\NEST\\BIG.BIN",65536,1048576UL,4096,18,NULL);
        assert(calls>before && !touched_left);
        before=opening_text;
        storage_screen("WRITE","S:\\NWT0001\\DATA\\NEST\\B00.BIN",0,0,0,18,NULL);
        assert(opening_text>before);
        before=entries_text;
        storage_screen("LIST","S:\\NWT0001",0,0,0,18,NULL);
        assert(entries_text>before);
        before=calls;
        storage_screen("LIST","S:\\NWT0001",0,0,0,18,NULL);
        assert(calls==before);
        for (state=STORAGE_PASSED;state<=STORAGE_CANCELLED;++state) {
            storage_report.state=state;
            storage_report.passed=state==STORAGE_PASSED?63:3;
            strcpy(nw_error,"Mismatch S:\\NWT0001\\DATA\\NEST\\B13.BIN byte 65536 expected 42 got 00");
            before=calls;
            storage_screen("","",0,0,0,360,"C:\\TESTLOG\\NWT0001.DAT");
            assert(calls>before);
        }
    }
    puts("PASS: storage dashboard fits CGA/EGA/VGA; unchanged reports draw nothing; progress preserves checklist; result/failure/cancellation render");
    return 0;
}
