/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef ARCHIVE_H
#define ARCHIVE_H
#include "NW.H"
#include "ARCMSG.H"
#ifdef NW_HOST
#define SEP '/'
#else
#define SEP '\\'
#endif
typedef struct {
    int fd;
    unsigned count;
    unsigned long size, start, end;
    char source[NW_PATH], target[NW_PATH];
} Archive;
typedef struct {
    char name[NW_PATH];
    unsigned method, flags, directory, end;
    unsigned long size, packed, crc, local, data, finish, next;
    Entry info;
} Member;
int archive_message(unsigned index);
unsigned archive_word(const unsigned char *p);
unsigned long archive_dword(const unsigned char *p);
int archive_poll(const char *name, unsigned long done, unsigned long total);
int archive_read_at(Archive *a, unsigned long at, void *data, unsigned n);
int archive_equal_path(const char *a, const char *b);
int archive_member_name(Member *m, unsigned char *raw, unsigned length);
int archive_directories(Archive *a, const Member *m, int create);
int archive_extract_member(Archive *a, Member *m, int (*unpack)(Archive *, Member *, int));
#endif
