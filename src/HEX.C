/* SPDX-License-Identifier: GPL-3.0-only */
#include <string.h>
#include "HEX.H"

/*
 * Produce one fixed-width row without sprintf or a temporary formatting
 * buffer. The caller owns an 80-byte destination and supplies up to 16 bytes;
 * the loop also clips excessive counts. Zero (or negative) count leaves both
 * data fields blank. On DOS unsigned long is 32 bits, hence eight hex digits.
 * Only ASCII 32..126 is shown literally: control characters and high bytes
 * cannot accidentally invoke text-mode controls or misleading CP437 glyphs.
 */
void hex_line(char *out, unsigned long offset, const unsigned char *bytes, int count)
{
    static const char digits[] = "0123456789ABCDEF";
    int i, x;
    /* Clear the whole row so short final blocks cannot retain old bytes. */
    memset(out, ' ', 79);
    out[79] = '\0';
    for (i = 7; i >= 0; --i) {
        out[i] = digits[(unsigned)(offset & 15UL)];
        offset >>= 4;
    }
    /* Fixed columns agree with MAIN.C's per-field recoloring: ASCII begins
     * at 62, and the extra hex separator falls between byte seven and eight. */
    out[61] = out[78] = '|';
    for (i = 0; i < count && i < 16; ++i) {
        x = 10 + i * 3 + (i >= 8);
        out[x] = digits[bytes[i] >> 4];
        out[x + 1] = digits[bytes[i] & 15];
        out[62 + i] = bytes[i] >= 32 && bytes[i] <= 126 ? bytes[i] : '.';
    }
}
