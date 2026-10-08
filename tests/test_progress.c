/* SPDX-License-Identifier: GPL-3.0-only */
/* Check every pixel write, not just the final image: growth must never clear
 * or repaint the existing fill/frame, and unchanged bars must write nothing. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "VIDEO.H"
#include "GRAPH.H"
int video_mode, video_height, video_rows;
static unsigned char pixels[640 * 480], expected[640 * 480];
static int restricted, left, right, top, bottom, calls;
void video_rect(int x, int y, int width, int height, unsigned color)
{
    int row;
    assert(width > 0 && height > 0);
    if (restricted) {
        assert(x >= left && x + width <= right);
        assert(y >= top && y + height <= bottom);
    }
    ++calls;
    for (row = y; row < y + height; ++row)
        memset(pixels + row * 640 + x, color, width);
}
static void delta(unsigned percent, unsigned previous)
{
    int a = 318 * percent / 100, b = 318 * previous / 100;
    left = 121 + (a < b ? a : b); right = 121 + (a > b ? a : b);
    top = 4 * (video_height / video_rows) + 2;
    bottom = top + video_height / video_rows - 4;
    restricted = 1; calls = 0;
    graph_progress(4, percent, previous, 0);
    assert((percent == previous) == (calls == 0));
    restricted = 0;
    memcpy(expected, pixels, sizeof(pixels));
    memset(pixels, 3, sizeof(pixels));
    graph_progress(4, percent, 0, 1);
    assert(!memcmp(expected, pixels, sizeof(pixels)));
}
int main(void)
{
    for (video_mode = VIDEO_CGA; video_mode <= VIDEO_VGA; ++video_mode) {
        video_height = video_mode == VIDEO_CGA ? 200 : video_mode == VIDEO_EGA ? 350 : 480;
        video_rows = video_mode == VIDEO_VGA ? 30 : 25;
        memset(pixels, 3, sizeof(pixels));
        graph_progress(4, 25, 0, 1);
        delta(50, 25); delta(50, 50); delta(100, 50);
        delta(0, 100); delta(10, 0); delta(5, 10);
        graph_progress(4, 200, 5, 0);
        memcpy(expected, pixels, sizeof(pixels));
        memset(pixels, 3, sizeof(pixels));
        graph_progress(4, 100, 0, 1);
        assert(!memcmp(expected, pixels, sizeof(pixels)));
    }
    puts("PASS: graphical bar growth touches only new pixels; unchanged bars write nothing; resets preserve the frame");
    return 0;
}
