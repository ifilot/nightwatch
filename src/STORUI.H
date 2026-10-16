/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef STORUI_H
#define STORUI_H
/* Draw the live storage report in cells or native pixels. All paths/labels are
 * borrowed for the call. manifest is NULL while running and local on completion.
 */
void storage_screen(const char *phase, const char *path, unsigned long done,
                    unsigned long total, unsigned long rate, unsigned long ticks,
                    const char *manifest);
#endif
