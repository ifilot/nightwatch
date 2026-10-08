/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ZIP_H
#define ZIP_H
#include "NW.H"
/* Classic single-volume ZIP, methods 0/8, ASCII DOS 8.3 paths only.
 * Preflight validates all names, headers, collisions and destination ancestors
 * before writing. CRC/length verification precedes each atomic replacement.
 * Earlier completed entries/directories remain on cancellation or failure.
 */
int zip_measure(const char *archive, const char *target,
                unsigned long *files, unsigned long *bytes);
int zip_extract(const char *archive, const char *target);
#ifdef NW_ZIP_TEST
extern int zip_fail_allocation;
extern unsigned zip_live_allocations;
#endif
#endif
