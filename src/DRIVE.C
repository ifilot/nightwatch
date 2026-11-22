/* SPDX-License-Identifier: GPL-3.0-only */
#include "DRIVE.H"

static unsigned char kinds[26], scanned;

unsigned drive_scan(Drive *out, unsigned current)
{
    unsigned number, count=0, version=drive_dos_version();
    unsigned otter=26, otter_flags=0, value;
    drive_guard_begin();
    for (number=0; number<26; ++number) kinds[number]=DRIVE_GENERIC;
    if (!drive_otter(&otter,&otter_flags) || otter>25) otter=26;
    for (number=0; number<26; ++number) {
        int present=number==current, remote=0, removable=-1, result;
        unsigned kind=DRIVE_GENERIC, flags=0;
        if (version>=0x030a) {
            result=drive_remote(number,&value);
            if (result>0) { present=1; remote=value!=0; }
        }
        if (number==otter) {
            present=1; kind=DRIVE_SD; flags=otter_flags;
        } else if (remote) kind=DRIVE_REMOTE;
        else {
            result=drive_removable(number,&value);
            if (result>0) {
                present=1; removable=value!=0;
                kind=removable ? DRIVE_REMOVABLE : DRIVE_FIXED;
            }
            if (present && version>=0x0314 && drive_device(number,&value)>0) {
                if (value==0 || value==1) kind=DRIVE_FLOPPY525;
                else if (value==2 || value==9 || (value==7 && removable==1)) kind=DRIVE_FLOPPY35;
                else if (value==5) kind=DRIVE_FIXED;
            }
        }
        if (present) {
            kinds[number]=(unsigned char)kind;
            out[count].number=(unsigned char)number;
            out[count].kind=(unsigned char)kind;
            out[count++].flags=(unsigned char)flags;
        }
    }
    drive_guard_end();
    scanned=1;
    return count;
}

unsigned drive_kind(unsigned number)
{
    Drive drives[26];
    if (number>=26) return DRIVE_GENERIC;
    if (!scanned) drive_scan(drives,number);
    return kinds[number];
}

char *drive_label(char *out, unsigned index)
{
#ifndef NW_HOST
    const char NW_FAR *source=asset_drive_label(index);
    char *at=out;
    while ((*at++=*source++)!=0) {}
#else
    /* Host tests exercise classification; authored labels are checked by the
     * generator and DOS UI tests, without a duplicate near-memory table. */
    (void)index; out[0]=0;
#endif
    return out;
}
