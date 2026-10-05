/* SPDX-License-Identifier: GPL-3.0-only */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "VIEW.H"

/*
 * Streaming helpers for the text and hexadecimal viewers. Files are opened
 * in binary mode by the caller so every saved long offset is a byte address,
 * even when CR/LF pairs are displayed as a single line break. These routines
 * borrow the FILE and change its position; they never close it or allocate
 * storage proportional to the file size.
 */
/*
 * Return the byte offset after rows display lines, or the byte position at EOF
 * if reached sooner. EOF here is an offset, not the C-library EOF sentinel.
 * The caller supplies a nonnegative offset and a positive row count. Layout
 * follows the viewer's 80-column wrapping and eight-column tab stops; CR is
 * ignored. A byte which fills the last column belongs to this page, including
 * a tab whose expansion reaches the boundary. Seek/read/position failure is
 * reported as -1. No page contents are retained here.
 */
long view_next(FILE *f, long offset, int rows)
{
    int x = 0, y = 0, ch;
    if (fseek(f, offset, SEEK_SET)) return -1;
    while (y < rows && (ch = getc(f)) != EOF) {
        if (ch == '\r') continue;
        if (ch == '\n') { ++y; x = 0; }
        else if (ch == '\t') {
            int spaces = 8 - (x & 7);
            while (spaces--) if (++x == 80) { x = 0; ++y; }
        } else if (++x == 80) { x = 0; ++y; }
    }
    return ferror(f) ? -1 : ftell(f);
}
/*
 * Recover a page before offset by walking the normal page boundaries from
 * byte zero. This is the slow fallback when MAIN.C's bounded page history no
 * longer contains the predecessor. A jump/search offset inside a normal page
 * returns that page's start; an exact boundary returns its predecessor.
 * Stop on lack of progress at EOF as well as on reaching offset, so a caller
 * positioned beyond EOF cannot make this loop run forever. Return -1 on I/O
 * failure, or zero when there is no earlier page.
 */
long view_previous(FILE *f, long offset, int rows)
{
    long at = 0, next;
    while (at < offset) {
        next = view_next(f, at, rows);
        if (next < 0) return -1;
        if (next >= offset || next == at) return at;
        at = next;
    }
    return 0;
}
/*
 * Find the start of the final nonempty page using the same layout rules as
 * view_next. Keeping only the current and preceding offsets avoids a page
 * table for arbitrarily large files. At EOF the extra scan makes no progress;
 * return the preceding start rather than displaying an empty trailing page.
 * An empty file has start zero; I/O failure returns -1.
 */
long view_last(FILE *f, int rows)
{
    long at = 0, next, previous = 0;
    for (;;) {
        next = view_next(f, at, rows);
        if (next < 0) return -1;
        if (next == at) return previous;
        previous = at; at = next;
    }
}
/* Decode one ASCII hexadecimal digit without relying on locale digit ranges. */
static int digit(int ch)
{
    ch = toupper((unsigned char)ch);
    if (ch >= '0' && ch <= '9') return ch - '0';
    if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
    return -1;
}
/*
 * Parse a nonempty literal or a lowercase "hex:" prefix followed by byte
 * pairs. Spaces may separate pairs (or precede/follow them); adjacent pairs
 * are also accepted. A pair cannot contain whitespace. The 63-byte limit is
 * shared with the fixed search failure table and the caller's needle buffer.
 * Return one with length/hex describing bytes, or zero on malformed/oversized
 * input. Parsing may partially overwrite bytes and hex on failure, so callers
 * must discard the result rather than retaining an earlier valid pattern.
 */
int view_pattern(const char *text, unsigned char *bytes, unsigned *length, int *hex)
{
    unsigned n = 0;
    *hex = !strncmp(text, "hex:", 4);
    if (!*hex) {
        n = strlen(text); if (!n || n > 63) return 0;
        memcpy(bytes, text, n);
    } else {
        text += 4;
        while (*text) {
            int a, b;
            while (*text == ' ') ++text;
            if (!*text) break;
            a = digit(*text++);
            if (!*text) return 0;
            b = digit(*text++);
            if (a < 0 || b < 0 || n == 63) return 0;
            bytes[n++] = (unsigned char)(a * 16 + b);
        }
    }
    *length = n; return n != 0;
}
/*
 * Search from offset inclusively and return the first match's byte address.
 * Hex patterns compare exact bytes; literals use the C library's case fold
 * (the UI promises ASCII). The caller chooses offset+1 for Find Next, allowing
 * overlapping matches without rediscovering the same occurrence. Patterns
 * must contain 1..63 bytes. Return -1 for invalid input, seek failure, read
 * failure or no match; callers can inspect ferror for a read failure. The
 * stream is left just after a match or at the unsuccessful scan's endpoint.
 * File positions must fit the DOS signed 32-bit long used by fseek/ftell.
 */
long view_find(FILE *f, long offset, const unsigned char *bytes, unsigned length, int hex)
{
    /*
     * KMP needs only one input pass and 64 bytes of automatic prefix storage.
     * failure[i] is the length of the longest proper prefix matching a suffix
     * through i. Applying the same case fold while building and consuming the
     * table is essential: otherwise mixed-case overlapping matches are lost.
     */
    unsigned char failure[64];
    unsigned i, matched = 0;
    int ch;
    long at = offset;
    if (!length || length > 63 || fseek(f, offset, SEEK_SET)) return -1;
    failure[0] = 0;
    for (i = 1; i < length; ++i) {
        int a = hex ? bytes[i] : toupper(bytes[i]);
        while (matched && a != (hex ? bytes[matched] : toupper(bytes[matched]))) matched = failure[matched - 1];
        if (a == (hex ? bytes[matched] : toupper(bytes[matched]))) ++matched;
        failure[i] = (unsigned char)matched;
    }
    /* Prefix fallback reuses bytes already matched; the FILE never rewinds. */
    matched = 0;
    while ((ch = getc(f)) != EOF) {
        if (!hex) ch = toupper((unsigned char)ch);
        while (matched && ch != (hex ? bytes[matched] : toupper(bytes[matched]))) matched = failure[matched - 1];
        if (ch == (hex ? bytes[matched] : toupper(bytes[matched]))) ++matched;
        ++at;
        if (matched == length) return at - length;
    }
    return -1;
}
/*
 * Parse a complete unsigned decimal offset or explicit 0x/0X hexadecimal.
 * Leading zeroes remain decimal rather than selecting C's octal convention.
 * Require the entire string and a value representable by DOS signed long;
 * this prevents a successful conversion from becoming a negative file seek.
 * Return one and set *offset on success; return zero without changing it on
 * failure. Whether the value is within the current file is a caller check.
 */
int view_offset(const char *text, long *offset)
{
    char *end;
    unsigned long value;
    int base = 10;
    /* strtoul accepts signed/whitespace input and wraps negatives: forbid both. */
    if (!isdigit((unsigned char)*text)) return 0;
    if (text[0] == '0' && (text[1] == 'x' || text[1] == 'X')) base = 16;
    errno = 0; value = strtoul(text, &end, base);
    if (errno || *end || value > 2147483647UL) return 0;
    *offset = (long)value; return 1;
}
