/* SPDX-License-Identifier: GPL-3.0-only */
/* Bounded ARJ bitstream decoder. Canonical Huffman counts avoid large lookup
 * tables; the history and codebooks share one normalized far allocation.
 * Format references and independent producer provenance: docs/arj-support.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __TURBOC__
#include <alloc.h>
#include <dos.h>
#endif
#include "ARJDEC.H"
#include "ARCHIVE.H"
#include "zlib.h"
#define WINDOW 26624U
#define SYMBOLS 510
/* All codebooks use the largest alphabet to keep one checked implementation.
 * Only the literal book needs 510 slots; the two small books use 19 each. */
typedef struct {
    unsigned short counts[17], offsets[17];
    unsigned short values[SYMBOLS];
    int single;
} Book;
typedef struct {
    unsigned short counts[17], offsets[17], values[19];
    int single;
} SmallBook;
typedef struct {
    unsigned char history[WINDOW], lengths[SYMBOLS];
    Book literal;
    SmallBook length, distance;
} State;
typedef struct {
    State NW_FAR *state;
    unsigned char input[256], output[1024];
    unsigned in_at, in_size, out_size, byte, bits, position;
    unsigned long left, size, produced, checksum;
    int in, out, ok;
    const char *name;
} Decoder;
#ifdef NW_ARJ_TEST
int arj_fail_allocation;
unsigned arj_live_allocations;
#endif
static int bad(Decoder *d)
{ d->ok = 0; archive_message(ARC_ARJ_INVALID_OR_TRUNCATED_COMPRESSED_DATA); return 0; }
static unsigned get(Decoder *d, unsigned n)
{
    unsigned result = 0;
    while (n-- && d->ok) {
        if (!d->bits) {
            if (d->in_at == d->in_size) {
                int got;
                unsigned want = d->left > sizeof(d->input) ? sizeof(d->input) : (unsigned)d->left;
                if (!want) { bad(d); break; }
                got = fs_read(d->in, d->input, want);
                if (got <= 0) {
                    d->ok = 0;
                    if (got < 0) fs_error("Read ARJ"); else bad(d);
                    break;
                }
                d->left -= got; d->in_size = got; d->in_at = 0;
            }
            d->byte = d->input[d->in_at++]; d->bits = 8;
        }
        result = (result << 1) | ((d->byte >> --d->bits) & 1);
    }
    return result;
}
/* A complete prefix tree must use exactly 2^16 leaves at depth 16.
 * Singleton trees are represented by a separate zero-bit symbol. */
static int build(Decoder *d, unsigned short NW_FAR *counts,
                 unsigned short NW_FAR *offsets, unsigned short NW_FAR *values,
                 unsigned n)
{
    unsigned i, j, cursor[17], sum = 0;
    unsigned long leaves = 0;
    for (i = 0; i <= 16; ++i) counts[i] = 0;
    for (i = 0; i < n; ++i) {
        j = d->state->lengths[i];
        if (j > 16) return bad(d);
        if (j) ++counts[j];
    }
    for (i = 1; i <= 16; ++i) {
        leaves += (unsigned long)counts[i] << (16-i);
        offsets[i] = cursor[i] = sum; sum += counts[i];
    }
    if (leaves != 65536UL) return bad(d);
    for (i = 0; i < n; ++i) {
        j = d->state->lengths[i];
        if (j) values[cursor[j]++] = i;
    }
    return 1;
}
static unsigned symbol(Decoder *d, unsigned short NW_FAR *counts,
                       unsigned short NW_FAR *offsets, unsigned short NW_FAR *values,
                       int single)
{
    unsigned width, code = 0;
    unsigned long first = 0;
    if (single >= 0) return (unsigned)single;
    for (width = 1; width <= 16 && d->ok; ++width) {
        code = (code << 1) | get(d,1);
        if (code >= first && code - first < counts[width])
            return values[offsets[width] + (unsigned)(code-first)];
        first = (first + counts[width]) << 1;
    }
    bad(d); return 0;
}
static int small(Decoder *d, SmallBook NW_FAR *b, unsigned alphabet, int special)
{
    unsigned n = get(d,5), i = 0, v, zeros;
    b->single = -1;
    if (!n) {
        v = get(d,5);
        if (v >= alphabet) return bad(d);
        b->single = v; return d->ok;
    }
    if (n > alphabet) return bad(d);
    while (i < n && d->ok) {
        v = get(d,3);
        if (v == 7) {
            while (get(d,1) && d->ok) {
                if (++v > 16) return bad(d);
            }
        }
        d->state->lengths[i++] = (unsigned char)v;
        if ((int)i == special) {
            zeros = get(d,2);
            if (zeros > n-i) return bad(d);
            while (zeros--) d->state->lengths[i++] = 0;
        }
    }
    while (i < alphabet) d->state->lengths[i++] = 0;
    return d->ok && build(d,b->counts,b->offsets,b->values,alphabet);
}
static int tables(Decoder *d)
{
    unsigned n, i = 0, v, zeros;
    State NW_FAR *s = d->state;
    if (!small(d,&s->length,19,3)) return 0;
    n = get(d,9); s->literal.single = -1;
    if (!n) {
        v = get(d,9);
        if (v >= SYMBOLS) return bad(d);
        s->literal.single = v;
    } else {
        if (n > SYMBOLS) return bad(d);
        while (i < n && d->ok) {
            v = symbol(d,s->length.counts,s->length.offsets,s->length.values,s->length.single);
            if (v > 2) s->lengths[i++] = (unsigned char)(v-2);
            else {
                zeros = v == 0 ? 1 : v == 1 ? get(d,4)+3 : get(d,9)+20;
                if (zeros > n-i) return bad(d);
                while (zeros--) s->lengths[i++] = 0;
            }
        }
        while (i < SYMBOLS) s->lengths[i++] = 0;
        if (!d->ok || !build(d,s->literal.counts,s->literal.offsets,s->literal.values,SYMBOLS)) return 0;
    }
    return small(d,&s->distance,17,-1);
}
static int flush(Decoder *d)
{
    unsigned at = 0;
    int written;
    while (at < d->out_size) {
        written = fs_write(d->out,d->output+at,d->out_size-at);
        if (written <= 0) { fs_error("Write ARJ"); d->ok = 0; return 0; }
        at += written;
    }
    d->checksum = crc32(d->checksum,d->output,d->out_size);
    operation_transferred += d->out_size;
    operation_current_bytes = d->produced;
    d->out_size = 0;
    if (operation_progress && !operation_progress(d->name,d->produced,d->size)) {
        strcpy(nw_error,"Operation cancelled"); d->ok = 0;
    }
    return d->ok;
}
static int emit(Decoder *d, unsigned value)
{
    if (d->produced == d->size) return bad(d);
    if (d->state) {
        d->state->history[d->position++] = (unsigned char)value;
        if (d->position == WINDOW) d->position = 0;
    }
    d->output[d->out_size++] = (unsigned char)value; ++d->produced;
    return d->out_size < sizeof(d->output) || flush(d);
}
static int match(Decoder *d, unsigned length, unsigned distance)
{
    unsigned at;
    if (!distance || distance > WINDOW || distance > d->produced || length > d->size-d->produced)
        return bad(d);
    at = d->position >= distance ? d->position-distance : d->position+WINDOW-distance;
    while (length-- && d->ok) {
        if (!emit(d,d->state->history[at])) return 0;
        if (++at == WINDOW) at = 0;
    }
    return d->ok;
}
static unsigned unary(Decoder *d, unsigned start, unsigned stop)
{
    unsigned width, base = 0;
    for (width = start; width < stop && d->ok; ++width) {
        if (!get(d,1)) break;
        base += 1U << width;
    }
    return base + get(d,width);
}
int arj_decode(int input, int output, unsigned method, unsigned long packed,
               unsigned long size, unsigned long crc, const char *name)
{
    Decoder d;
    void NW_FAR *allocation = NULL;
    unsigned block = 0, v, distance, length;
    memset(&d,0,sizeof(d)); d.ok = 1; d.in = input; d.out = output;
    d.left = packed; d.size = size; d.name = name;
    if (method) {
#ifdef NW_ARJ_TEST
        if (!arj_fail_allocation || --arj_fail_allocation)
#endif
        {
#ifdef __TURBOC__
            allocation = farmalloc((unsigned long)sizeof(State)+15UL);
            if (allocation) d.state = MK_FP(FP_SEG(allocation)+(FP_OFF(allocation)>>4),FP_OFF(allocation)&15);
#else
            allocation = malloc(sizeof(State)); d.state = allocation;
#endif
        }
        if (!allocation) { archive_message(ARC_ARJ_INSUFFICIENT_DECOMPRESSION_MEMORY); return 0; }
#ifdef NW_ARJ_TEST
        ++arj_live_allocations;
#endif
    }
    while (d.produced < size && d.ok) {
        if (!method) { v = get(&d,8); if (d.ok) emit(&d,v); }
        else if (method == 4) {
            length = unary(&d,0,7);
            if (!length) { v = get(&d,8); if (d.ok) emit(&d,v); }
            else {
                distance = unary(&d,9,13)+1;
                if (d.ok) match(&d,length+2,distance);
            }
        } else {
            if (!block) {
                block = get(&d,16);
                if (!block || !tables(&d)) { if (d.ok) bad(&d); break; }
            }
            --block;
            v = symbol(&d,d.state->literal.counts,d.state->literal.offsets,d.state->literal.values,d.state->literal.single);
            if (v < 256) { if (d.ok) emit(&d,v); }
            else {
                length = v-253;
                v = symbol(&d,d.state->distance.counts,d.state->distance.offsets,d.state->distance.values,d.state->distance.single);
                distance = v ? (1U << (v-1)) + get(&d,v-1) : 0;
                if (d.ok) match(&d,length,distance+1);
            }
        }
    }
    if (d.ok && d.out_size) flush(&d);
    /* ARJ pads the last byte, but must not carry additional full bytes. */
    if (d.ok && (d.left || d.in_at != d.in_size || (method < 4 && method && block) || d.checksum != crc)) {
        archive_message(ARC_ARJ_CRC_SIZE_OR_COMPRESSED_LENGTH_MISMATCH); d.ok = 0;
    }
    if (allocation) {
#ifdef __TURBOC__
        farfree(allocation);
#else
        free(allocation);
#endif
#ifdef NW_ARJ_TEST
        --arj_live_allocations;
#endif
    }
    return d.ok;
}
