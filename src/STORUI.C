/* SPDX-License-Identifier: GPL-3.0-only */
/* Storage dashboard. Text keeps ASCII status markers; graphics draw checkmarks
 * directly, including on CGA, without requiring a Unicode glyph or new font.
 * All authored labels stay in immutable far storage to preserve DOS near heap.
 */
#include <stdio.h>
#include <string.h>
#include "STORTEST.H"
#include "STORMSG.H"
#include "STORUI.H"
#include "VIDEO.H"
#include "GRAPH.H"

static const unsigned char checks[3][6] = {
    { ST_DASHPREPARE, ST_DASHWRITE, ST_DASHREAD, ST_DASHREUSE, ST_DASHMUTATE, ST_DASHAUDIT },
    { ST_DASHOWNER, ST_DASHDIRS, ST_DASHREAD, 0, 0, 0 },
    { ST_DASHOWNER, ST_DASHDIRS, ST_DASHREMOVE, 0, 0, 0 }
};
/* Native pixels are retained between updates. Cache semantic fields rather
 * than a framebuffer, so an unchanged report writes no VRAM on an 8088.
 */
static StorageReport prior;
static char prior_phase[32], prior_path[NW_PATH];
static unsigned prior_generation, prior_percent, prior_fill;
static unsigned long prior_total, prior_rate, prior_seconds, prior_done;
static int native_valid;
static void label_row(int x, int y, const char *label, int width, unsigned fg, unsigned bg)
{
    video_rect(x, y, width, video_font_height, bg);
    video_label(x, y, label, fg, bg, 0);
}
/* Scale without overflowing a DOS unsigned long or 16-bit intermediate. */
static unsigned portion(unsigned long done, unsigned long total, unsigned scale)
{
    unsigned lo = 0, hi = scale, mid;
    unsigned long q, r;
    if (!total) return 0;
    if (done >= total) return scale;
    q = total / scale; r = total % scale;
    while (lo < hi) {
        mid = (lo + hi + 1) / 2;
        if (done >= q * mid + (r * mid + scale - 1) / scale) lo = mid;
        else hi = mid - 1;
    }
    return lo;
}
static unsigned passed_count(void)
{
    unsigned i, count = 0;
    for (i = 0; (unsigned)i < storage_report.stages; ++i)
        if (storage_report.passed & (1U << i)) ++count;
    return count;
}
/* 0 pending, 1 running, 2 passed, 3 failed, 4 cancelled. */
static unsigned check_state(unsigned i)
{
    if (storage_report.passed & (1U << i)) return 2;
    if (i != storage_report.stage) return 0;
    if (storage_report.state == STORAGE_FAILED) return 3;
    if (storage_report.state == STORAGE_CANCELLED) return 4;
    return 1;
}
static unsigned profile(void)
{
    if (storage_report.action == 1) return ST_DASHVERIFY;
    if (storage_report.action == 2) return ST_DASHCLEAN;
    return storage_report.extended ? ST_DASHLONG : ST_DASHQUICK;
}
static unsigned result_label(void)
{
    return storage_report.state == STORAGE_PASSED ? ST_DASHPASS :
        storage_report.state == STORAGE_CANCELLED ? ST_DASHSTOP : ST_DASHFAIL;
}
static unsigned footer(void)
{
    if (storage_report.state == STORAGE_RUNNING) return ST_DASHRUNFOOT;
    if (storage_report.state != STORAGE_PASSED) return ST_DASHKEEP;
    return storage_report.action == 2 ? ST_DASHREMOVED : ST_DASHREADY;
}
static void subtitle(char *out)
{
    char fmt[80], label[80];
    storage_label(fmt, ST_DASHSUB); storage_label(label, profile());
    sprintf(out, fmt, label, storage_report.seed, NW_VERSION_TAG);
}
static void tail(char *out, const char *path, unsigned width)
{
    unsigned n = strlen(path);
    if (n > width) { strcpy(out, "..."); strcat(out, path + n - width + 3); }
    else strcpy(out, path);
}
/* A small solid status badge, with tick/arrow/cross strokes in native pixels. */
static void badge(int x, int y, int size, unsigned state, int mono)
{
    unsigned fill = state == 2 ? 2 : state == 3 ? 4 : state == 4 ? 6 : state == 1 ? 1 : 7;
    unsigned ink = mono ? 0 : 15;
    int i, a = size / 4, middle = size / 2;
    video_rect(x, y, size, size, mono ? 15 : (state ? fill : 8));
    if (!state) { video_rect(x + 1, y + 1, size - 2, size - 2, mono ? 0 : 7); return; }
    if (state == 2) {
        for (i = 0; i < a; ++i) video_rect(x + a - 1 + i, y + middle - 1 + i, 2, 2, ink);
        for (i = 0; i < middle; ++i) video_rect(x + middle - 2 + i, y + middle + a - 2 - i, 2, 2, ink);
    } else if (state == 3) {
        for (i = 0; i < middle; ++i) {
            video_rect(x + a + i, y + a + i, 2, 2, ink);
            video_rect(x + size - a - 2 - i, y + a + i, 2, 2, ink);
        }
    } else if (state == 4) video_rect(x + a, y + middle - 1, middle, 2, ink);
    else for (i = 0; i < a; ++i) {
        video_rect(x + a + i, y + a + i, 2, 2, ink);
        video_rect(x + a + i, y + size - a - 2 - i, 2, 2, ink);
    }
}
/* The card scales vertically with the native font, fitting 640x200 CGA too. */
static void graphics(const char *phase, const char *path, unsigned long done,
                     unsigned long total, unsigned long rate, unsigned long ticks,
                     const char *manifest)
{
    char line[100], fmt[80];
    int f = video_font_height, h = f * 20 + 24, y = (video_height - h) / 2;
    int body = y + 3 * f + 22, x = 336, right = 32 * video_font_width;
    int size = f < 12 ? 10 : f, row, i, overall = y + 16 * f + 10;
    int barheight = f < 12 ? 6 : 10;
    int mono = video_mode == VIDEO_CGA;
    unsigned paper = mono ? 0 : 7, ink = mono ? 15 : 0, blue = mono ? 15 : 1;
    unsigned fg, state, part = 576 / storage_report.stages;
    unsigned long seconds = ticks * 10UL / 182UL;
    unsigned percent = portion(done, total, 100), fill = portion(done, total, right - 2);
    int full;
    video_overlay_begin(0); video_flush();
    full = !native_valid || prior_generation != video_generation() ||
        prior.action != storage_report.action || prior.stage != storage_report.stage ||
        prior.passed != storage_report.passed || prior.state != storage_report.state ||
        prior.seed != storage_report.seed || prior.extended != storage_report.extended;
    if (full) {
        video_rect(16, y, 608, h, mono ? 15 : 7);
        video_rect(24, y + f + 12, 592, h - f - 20, paper);
        graph_window_frame(16, y, 608, h,
                            storage_label(line, ST_DASHTITLE), "", ICON_DRIVE);
        subtitle(line); video_label(32, y + f + 12, line, blue, paper, 0);
        if (storage_report.state != STORAGE_RUNNING) {
            badge(584, y + f + 12, size, storage_report.state == STORAGE_PASSED ? 2 :
                  storage_report.state == STORAGE_CANCELLED ? 4 : 3, mono);
        }
        video_label(32, body, storage_label(line, ST_DASHCHECKLIST), blue, paper, 0);
        video_rect(316, body, 1, 12 * f + 6, mono ? 15 : 8);
        for (i = 0; (unsigned)i < storage_report.stages; ++i) {
            row = body + f + 5 + i * (f + f / 2 - 2); state = check_state(i);
            badge(32, row, size, state, mono);
            fg = mono ? 15 : state == 2 ? 2 : state == 3 ? 4 : state == 1 ? 1 : 8;
            video_label(54, row, storage_label(line, checks[storage_report.action][i]), fg, paper, 0);
        }
        video_label(x, body, storage_label(line, storage_report.state == STORAGE_RUNNING ? ST_DASHCURRENT : ST_DASHRESULT), blue, paper, 0);
    }
    if (storage_report.state == STORAGE_RUNNING) {
        if (full || strcmp(phase, prior_phase))
            label_row(x, body + f + 5, phase, right, ink, paper);
        if (full || strcmp(path, prior_path)) {
            tail(line, path, 32); label_row(x, body + 3 * f, line, right, ink, paper);
        }
        if (full || strcmp(phase, prior_phase) || percent != prior_percent || total != prior_total || (!total && done != prior_done)) {
            storage_label(fmt, total ? ST_DASHPERCENT : ST_DASHENTRIES);
            if (total) sprintf(line, fmt, percent);
            else if (!strcmp(phase, "WRITE") || !strcmp(phase, "VERIFY")) storage_label(line, ST_DASHOPEN);
            else sprintf(line, fmt, done);
            label_row(x, body + 4 * f, line, right, ink, paper);
        }
        if (full || total != prior_total || fill != prior_fill) {
            if (full || total != prior_total || fill < prior_fill) {
                video_rect(x, body + 5 * f + 3, right, barheight, mono ? 15 : 8);
                video_rect(x + 1, body + 5 * f + 4, right - 2, barheight - 2, mono ? 0 : 15);
                if (total && fill) video_rect(x + 1, body + 5 * f + 4, fill, barheight - 2, mono ? 15 : 3);
            } else if (total && fill > prior_fill)
                video_rect(x + 1 + prior_fill, body + 5 * f + 4, fill - prior_fill, barheight - 2, mono ? 15 : 3);
        }
        if (full || rate != prior_rate || total != prior_total) {
            storage_label(fmt, ST_DASHRATE); sprintf(line, fmt, rate / 1024, (rate % 1024) * 10 / 1024);
            label_row(x, body + 6 * f + 3, total ? line : "", right, ink, paper);
        }
        if (full || storage_report.cycle != prior.cycle) {
            storage_label(fmt, ST_DASHCYCLE); sprintf(line, fmt, storage_report.cycle, storage_report.cycles);
            label_row(500, body, storage_report.cycle ? line : "", 100, blue, paper);
        }
    } else if (full) {
        fg = mono ? 15 : storage_report.state == STORAGE_PASSED ? 2 : storage_report.state == STORAGE_CANCELLED ? 6 : 4;
        video_label(x, body + f + 5, storage_label(line, result_label()), fg, paper, 0);
        if (storage_report.state != STORAGE_PASSED) {
            unsigned n = strlen(nw_error), at;
            for (i = 0; i < 4 && (unsigned)i * 32 < n; ++i) {
                at = i * 32; strncpy(line, nw_error + at, 32); line[32] = 0;
                video_label(x, body + (3 + i) * f, line, ink, paper, 0);
            }
        } else if (storage_report.action == 1)
            video_label(x, body + 3 * f, storage_label(line, ST_DASHREADONLY), ink, paper, 0);
    }
    if (full || storage_report.written / 1024 != prior.written / 1024) {
        storage_label(fmt, ST_DASHWRITTEN); sprintf(line, fmt, storage_report.written / 1024);
        label_row(x, body + 7 * f + 3, line, right, ink, paper);
    }
    if (full || storage_report.verified / 1024 != prior.verified / 1024) {
        storage_label(fmt, ST_DASHVERIFIED); sprintf(line, fmt, storage_report.verified / 1024);
        label_row(x, body + 8 * f + 3, line, right, ink, paper);
    }
    if (full || storage_report.files != prior.files) {
        storage_label(fmt, ST_DASHFILES); sprintf(line, fmt, storage_report.files);
        label_row(x, body + 9 * f + 3, line, right, ink, paper);
    }
    if (full || seconds != prior_seconds) {
        storage_label(fmt, ST_DASHTIME); sprintf(line, fmt, seconds / 60, seconds % 60);
        label_row(x, body + 10 * f + 3, line, right, ink, paper);
    }
    if (full) {
        storage_label(fmt, ST_DASHOVERALL); sprintf(line, fmt, passed_count(), storage_report.stages);
        video_label(32, overall - f - 4, line, blue, paper, 0);
        for (i = 0; (unsigned)i < storage_report.stages; ++i) {
            state = check_state(i); fg = mono ? (state == 2 ? 15 : 0) :
                state == 2 ? 2 : state == 3 ? 4 : state == 1 ? 1 : 8;
            video_rect(32 + i * part, overall, part - 3, 10, mono ? 15 : 8);
            video_rect(33 + i * part, overall + 1, part - 5, 8, fg);
        }
        if (manifest) {
            char shown[60];
            tail(shown, manifest, 56); storage_label(fmt, ST_DASHLOGFOOT); sprintf(line, fmt, shown);
        }
        else storage_label(line, ST_DASHRUNFOOT);
        video_label(32, y + 18 * f + 6, line, ink, paper, 0);
        if (manifest) video_label(32, y + 19 * f + 6, storage_label(line, footer()), ink, paper, 0);
    }
    prior = storage_report; prior_generation = video_generation(); native_valid = 1;
    prior_percent = percent; prior_fill = fill; prior_total = total;
    prior_rate = rate; prior_seconds = seconds; prior_done = done;
    strncpy(prior_phase, phase, sizeof(prior_phase) - 1); prior_phase[sizeof(prior_phase) - 1] = 0;
    strcpy(prior_path, path);
}
/* Text mode uses the same report and phases; [v]/[>]/[x] also work on MDA. */
static void text(const char *phase, const char *path, unsigned long done,
                 unsigned long total, unsigned long rate, unsigned long ticks,
                 const char *manifest)
{
    char line[100], fmt[80];
    int y = (video_rows - 22) / 2, i, row;
    unsigned state, count, filled;
    unsigned long seconds = ticks * 10UL / 182UL;
    video_overlay_begin(0);
    for (i = 0; i < 22; ++i) video_fill(2, y + i, 76, ' ', COLOR_DIALOG);
    for (i = 0; i < 22; ++i) { video_put(2, y + i, 186, COLOR_DIALOG); video_put(77, y + i, 186, COLOR_DIALOG); }
    video_fill(2, y, 76, 205, COLOR_DIALOG); video_fill(2, y + 21, 76, 205, COLOR_DIALOG);
    video_put(2, y, 201, COLOR_DIALOG); video_put(77, y, 187, COLOR_DIALOG);
    video_put(2, y + 21, 200, COLOR_DIALOG); video_put(77, y + 21, 188, COLOR_DIALOG);
    video_fill(3, y + 1, 74, ' ', COLOR_ACTIVE);
    video_text(4, y + 1, storage_label(line, ST_DASHTITLE), COLOR_ACTIVE);
    subtitle(line); video_text(4, y + 2, line, COLOR_DIALOG);
    video_text(4, y + 4, storage_label(line, ST_DASHCHECKLIST), 0x71);
    for (i = 0; (unsigned)i < storage_report.stages; ++i) {
        row = y + 5 + i * 2; state = check_state(i);
        strcpy(line, state == 2 ? "[v]" : state == 1 ? "[>]" : state == 3 ? "[x]" : state == 4 ? "[-]" : "[ ]");
        video_text(4, row, line, state == 2 ? 0x72 : state == 3 ? 0x74 : 0x71);
        video_text(8, row, storage_label(line, checks[storage_report.action][i]), COLOR_DIALOG);
    }
    video_text(42, y + 4, storage_label(line, storage_report.state == STORAGE_RUNNING ? ST_DASHCURRENT : ST_DASHRESULT), 0x71);
    if (storage_report.state == STORAGE_RUNNING) {
        video_text(42, y + 5, phase, COLOR_DIALOG); tail(line, path, 32); video_text(42, y + 7, line, COLOR_DIALOG);
        storage_label(fmt, total ? ST_DASHPERCENT : ST_DASHENTRIES);
        if (total) sprintf(line, fmt, portion(done, total, 100));
        else if (!strcmp(phase, "WRITE") || !strcmp(phase, "VERIFY")) storage_label(line, ST_DASHOPEN);
        else sprintf(line, fmt, done);
        video_text(42, y + 8, line, COLOR_DIALOG);
        filled = portion(done, total, 30);
        video_fill(42, y + 9, 30, 176, COLOR_DIALOG); video_fill(42, y + 9, filled, 219, 0x73);
        storage_label(fmt, ST_DASHRATE); sprintf(line, fmt, rate / 1024, (rate % 1024) * 10 / 1024);
        if (total) video_text(42, y + 10, line, COLOR_DIALOG);
    } else {
        video_text(42, y + 5, storage_label(line, result_label()), storage_report.state == STORAGE_PASSED ? 0x72 : 0x74);
        if (storage_report.state != STORAGE_PASSED) {
            for (i = 0; i < 4 && (unsigned)i * 32 < strlen(nw_error); ++i) {
                strncpy(line, nw_error + i * 32, 32); line[32] = 0;
                video_text(42, y + 7 + i, line, COLOR_DIALOG);
            }
        }
    }
    storage_label(fmt, ST_DASHWRITTEN); sprintf(line, fmt, storage_report.written / 1024); video_text(42, y + 12, line, COLOR_DIALOG);
    storage_label(fmt, ST_DASHVERIFIED); sprintf(line, fmt, storage_report.verified / 1024); video_text(42, y + 13, line, COLOR_DIALOG);
    storage_label(fmt, ST_DASHFILES); sprintf(line, fmt, storage_report.files); video_text(42, y + 14, line, COLOR_DIALOG);
    storage_label(fmt, ST_DASHTIME); sprintf(line, fmt, seconds / 60, seconds % 60); video_text(42, y + 15, line, COLOR_DIALOG);
    count = passed_count(); storage_label(fmt, ST_DASHOVERALL); sprintf(line, fmt, count, storage_report.stages);
    video_text(4, y + 17, line, 0x71);
    video_fill(4, y + 18, 70, 176, COLOR_DIALOG); video_fill(4, y + 18, count * 70 / storage_report.stages, 219, 0x72);
    if (manifest) tail(line, manifest, 70); else storage_label(line, ST_DASHRUNFOOT);
    video_text(4, y + 19, line, COLOR_DIALOG);
    if (manifest) video_text(4, y + 20, storage_label(line, footer()), COLOR_DIALOG);
    video_flush();
}
void storage_screen(const char *phase, const char *path, unsigned long done,
                    unsigned long total, unsigned long rate, unsigned long ticks,
                    const char *manifest)
{
    if (video_mode == VIDEO_TEXT) {
        native_valid = 0; text(phase, path, done, total, rate, ticks, manifest);
    }
    else graphics(phase, path, done, total, rate, ticks, manifest);
}
