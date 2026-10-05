/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef VIEW_H
#define VIEW_H
#include <stdio.h>

/* Binary, seekable FILEs and signed DOS long byte offsets; no ownership transfer. */
/* Page scans use 80 columns, CR/LF handling and tab stops shared with MAIN.C.
 * rows must be positive. A negative result means I/O/position failure. */
long view_next(FILE *f, long offset, int rows);
long view_previous(FILE *f, long offset, int rows);
long view_last(FILE *f, int rows);
/* Parse literal text or hex:DE AD 00 BE EF into caller storage (at least 63
 * bytes). Return one for a valid 1..63 byte pattern; discard outputs on zero. */
int view_pattern(const char *text, unsigned char *bytes, unsigned *length, int *hex);
/* Find inclusively from offset; hex selects exact rather than folded bytes.
 * Return the first byte address, or -1 for invalid input, failure or no match. */
long view_find(FILE *f, long offset, const unsigned char *bytes, unsigned length, int hex);
/* Accept decimal or 0x hexadecimal in 0..2147483647; reject signs/whitespace.
 * Return one and write offset on success, zero leaving offset unchanged. */
int view_offset(const char *text, long *offset);
#endif
