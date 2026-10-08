/* SPDX-License-Identifier: GPL-3.0-only */
/* Long body/heading lines must fit beside the scrollbar in every adapter,
 * and every Help label must use the regular font. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "VIDEO.H"
#include "GRAPH.H"
#include "HELP.H"
int video_mode, video_height, video_rows, video_font_height, video_font_width;
static int body_top, body_bottom, body_labels;
const char *asset_help(unsigned index)
{
    static char text[HELP_WIDTH + 1];
    assert(index < HELP_LINES);
    memset(text, 'W', HELP_WIDTH); text[HELP_WIDTH] = 0;
    if (!(index & 1)) text[0] = '#';
    return text;
}
void video_overlay_begin(int clear) { (void)clear; }
void video_rect(int x, int y, int width, int height, unsigned color)
{
    (void)color;
    assert(x >= 0 && y >= 0 && width > 0 && height > 0);
    assert(x + width <= 640 && y + height <= video_height);
}
void video_icon(int x, int y, int icon, unsigned body, unsigned edge, unsigned highlight)
{
    (void)x; (void)y; (void)icon; (void)body; (void)edge; (void)highlight;
}
void video_label(int x, int y, const char *text, unsigned fg, unsigned bg, int bold)
{
    (void)fg; (void)bg;
    assert(!bold);
    assert(x + strlen(text) * video_font_width <= 622);
    if (y >= body_top && y < body_bottom) {
        assert(x == 40 && x + strlen(text) * video_font_width <= 592);
        assert(y + video_font_height <= body_bottom);
        ++body_labels;
    }
}
int main(void)
{
    int y, rows;
    for (video_mode = VIDEO_CGA; video_mode <= VIDEO_VGA; ++video_mode) {
        video_height = video_mode == VIDEO_CGA ? 200 : video_mode == VIDEO_EGA ? 350 : 480;
        video_font_height = video_mode == VIDEO_CGA ? 8 : video_mode == VIDEO_EGA ? 12 : 16;
        video_font_width = video_mode == VIDEO_CGA ? 6 : video_mode == VIDEO_EGA ? 7 : 8;
        y = (video_height - (17 * video_font_height + 52)) / 2;
        rows = graph_help_rows();
        assert(rows < HELP_LINES);
        body_top = y + video_font_height * 2 + 28;
        body_bottom = body_top + rows * (video_font_height + 2);
        body_labels = 0;
        graph_help("v1.1.0", "1234567-dirty", "github.com/ifilot/nightwatch", 0, 1);
        assert(body_labels == rows);
        body_labels = 0;
        graph_help("v1.1.0", "1234567-dirty", "github.com/ifilot/nightwatch", HELP_LINES - rows, 0);
        assert(body_labels == rows);
    }
    puts("PASS: Help uses regular font and long lines fit CGA/EGA/VGA viewports");
    return 0;
}
