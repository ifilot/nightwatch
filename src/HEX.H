/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef HEX_H
#define HEX_H
/* Format an 80-byte buffer as 79 visible columns plus NUL: eight-digit offset,
 * two groups of eight bytes, and printable ASCII. count is normally 0..16;
 * larger counts are clipped, and bytes need contain only the displayed count.
 * Empty positions remain spaces; nonprintable bytes display as dots. */
void hex_line(char *out, unsigned long offset, const unsigned char *bytes, int count);
#endif
