/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ARJ_H
#define ARJ_H
#include "NW.H"
/* Single-volume ARJ, methods 0..4, binary files and DOS 8.3 paths. */
int arj_measure(const char *archive, const char *target,
                unsigned long *files, unsigned long *bytes);
int arj_extract(const char *archive, const char *target);
#endif
