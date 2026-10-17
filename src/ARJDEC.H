/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ARJDEC_H
#define ARJDEC_H
#include "NW.H"
/* Input descriptor is positioned at the compressed payload. */
int arj_decode(int input, int output, unsigned method, unsigned long packed,
               unsigned long size, unsigned long crc, const char *name);
#ifdef NW_ARJ_TEST
extern int arj_fail_allocation;
extern unsigned arj_live_allocations;
#endif
#endif
