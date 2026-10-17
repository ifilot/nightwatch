/* SPDX-License-Identifier: GPL-3.0-only */
/* Use a binary temporary stream to check byte offsets, paging beyond the
 * bounded UI history, overlapping text/binary search and DOS long bounds.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "VIEW.H"
static void line_position(void)
{
    ViewPosition p;
    FILE *f = tmpfile();
    long saved;
    int i;
    assert(f); p.offset = -1;
    assert(view_position(f, 0, &p) && p.total == 0 && p.breaks == 0);
    fclose(f);
    f = tmpfile(); assert(f);
    /* CRLF, blank lines, wrapping/tabs and a final unterminated line. */
    fputs("a\r\n\n", f);
    for (i = 0; i < 100; ++i) fputc('W', f);
    fputs("\tend", f); fflush(f);
    assert(!fseek(f, 2, SEEK_SET)); saved = ftell(f); p.offset = -1;
    assert(view_position(f, 0, &p) && p.total == 3 && p.breaks == 0);
    assert(ftell(f) == saved);
    assert(view_position(f, 2, &p) && p.breaks == 0);
    assert(view_position(f, 3, &p) && p.breaks == 1);
    assert(view_position(f, 4, &p) && p.breaks == 2);
    assert(view_position(f, 90, &p) && p.breaks == 2 && p.total == 3);
    assert(view_position(f, 3, &p) && p.breaks == 1);
    assert(view_position(f, 3, &p) && p.breaks == 1);
    assert(view_position(f, 108, &p) && p.breaks == 2);
    assert(ftell(f) == saved); fclose(f);
    f = tmpfile(); assert(f); fputs("\n\r\n", f); fflush(f); p.offset = -1;
    assert(view_position(f, 0, &p) && p.total == 2 && p.breaks == 0);
    assert(view_position(f, 3, &p) && p.breaks == 2 && p.total == 2);
    assert(view_position(f, 1, &p) && p.breaks == 1);
    fclose(f);
    f = tmpfile(); assert(f);
    for (i = 0; i < 20000; ++i) fputs("long line\r\n", f);
    fflush(f); p.offset = -1;
    assert(view_position(f, 100001L, &p) && p.total == 20000 && p.breaks == 9091);
    assert(view_position(f, 0, &p) && p.breaks == 0);
    fclose(f);
}
int main(void)
{
    FILE *f = tmpfile();
    unsigned char needle[64];
    char pattern[200], negative[40];
    unsigned length;
    int hex, i;
    long at = 0, old = 0, end, value;
    assert(f);
    line_position();
    for (i = 0; i < 20000; ++i) fputs("line\r\n", f);
    fputs("AaBaBaC\0", f); fputc(0, f); fputc(255, f); fflush(f);
    for (i = 0; i < 150; ++i) { old = at; at = view_next(f, at, 23); assert(at > old); }
    assert(view_previous(f, at, 23) == old);
    assert(view_previous(f, 0, 23) == 0);
    end = view_last(f, 23); assert(end >= at);
    assert(view_pattern("abac", needle, &length, &hex));
    assert(view_find(f, 0, needle, length, hex) == 120003L);
    assert(view_pattern("hex:43 00 FF", needle, &length, &hex));
    assert(view_find(f, 0, needle, length, hex) == 120006L);
    assert(!view_pattern("hex:A", needle, &length, &hex));
    assert(!view_pattern("hex:GG", needle, &length, &hex));
    assert(!view_pattern("", needle, &length, &hex));
    assert(view_offset("0x10000", &value) && value == 65536L);
    assert(view_offset("010", &value) && value == 10);
    assert(!view_offset("-1", &value)); assert(!view_offset("0xFFFFFFFF", &value));
    assert(!view_offset("12x", &value)); assert(!view_offset("", &value));
    sprintf(negative, " -%lu", ULONG_MAX); assert(!view_offset(negative, &value));
    assert(!view_offset(" +1", &value)); assert(!view_offset("0x", &value));
    assert(!view_offset("2147483648", &value));
    memset(pattern,'A',63); pattern[63]=0;
    assert(view_pattern(pattern,needle,&length,&hex) && length==63);
    pattern[63]='A'; pattern[64]=0; assert(!view_pattern(pattern,needle,&length,&hex));
    strcpy(pattern,"hex:");
    for (i=0;i<63;++i) strcat(pattern,"41 ");
    assert(view_pattern(pattern,needle,&length,&hex) && length==63 && hex);
    strcat(pattern,"41"); assert(!view_pattern(pattern,needle,&length,&hex));
    fclose(f);
    f = tmpfile(); assert(f); fputs("aBaBaBaC",f); fflush(f);
    assert(view_pattern("aba",needle,&length,&hex));
    assert(view_find(f,0,needle,length,hex)==0);
    assert(view_find(f,1,needle,length,hex)==2);
    assert(view_find(f,3,needle,length,hex)==4);
    assert(view_find(f,5,needle,length,hex)==-1);
    fclose(f);
    f = tmpfile(); assert(f); assert(view_last(f, 23) == 0); fclose(f);
    puts("PASS: logical line positions, unbounded text paging, overlapping search, binary patterns and offsets");
    return 0;
}
