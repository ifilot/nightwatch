/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "DRIVE.H"
typedef struct { int present, remote, removable, type; } Probe;
static Probe probes[26];
static unsigned version, otter_number, otter_flags, guarded;
static unsigned remote_calls, type_calls, otter_calls;
unsigned drive_dos_version(void) { return version; }
void drive_guard_begin(void) { assert(!guarded); guarded=1; }
void drive_guard_end(void) { assert(guarded); guarded=0; }
int drive_remote(unsigned n, unsigned *value)
{
    assert(guarded && version>=0x030a); ++remote_calls;
    *value=(unsigned)probes[n].remote; return probes[n].present;
}
int drive_removable(unsigned n, unsigned *value)
{
    assert(guarded); *value=(unsigned)probes[n].removable;
    return probes[n].removable<0 ? 0 : probes[n].present;
}
int drive_device(unsigned n, unsigned *value)
{
    assert(guarded && version>=0x0314); ++type_calls;
    assert(!probes[n].remote); *value=(unsigned)probes[n].type;
    return probes[n].type>=0;
}
int drive_otter(unsigned *n, unsigned *flags)
{
    assert(guarded); ++otter_calls; *n=otter_number; *flags=otter_flags;
    return otter_number<26;
}
static void reset(unsigned v)
{
    unsigned i;
    memset(probes,0,sizeof(probes));
    for (i=0;i<26;++i) { probes[i].present=-1; probes[i].type=-1; }
    version=v; otter_number=26; otter_flags=0;
    remote_calls=type_calls=otter_calls=0;
}
static void add(unsigned n, int removable, int type, int remote)
{
    probes[n].present=1; probes[n].removable=removable;
    probes[n].type=type; probes[n].remote=remote;
}
int main(void)
{
    struct { Drive drives[26]; unsigned char canary[16]; } out;
    unsigned n, i;
    reset(0x0616); add(2,0,5,0);
    assert(drive_kind(2)==DRIVE_FIXED && otter_calls==1);
    assert(drive_kind(2)==DRIVE_FIXED && drive_kind(26)==DRIVE_GENERIC && otter_calls==1);
    reset(0x300); add(0,1,0,0); add(2,0,5,0);
    n=drive_scan(out.drives,2);
    assert(n==2 && out.drives[0].kind==DRIVE_REMOVABLE && out.drives[1].kind==DRIVE_FIXED);
    assert(!remote_calls && !type_calls && !guarded && otter_calls==1);
    reset(0x030a); add(2,-1,-1,0); add(18,-1,-1,1);
    n=drive_scan(out.drives,26);
    assert(n==2 && out.drives[0].kind==DRIVE_GENERIC && out.drives[1].kind==DRIVE_REMOTE);
    assert(remote_calls==26 && !type_calls);
    reset(0x0314);
    add(0,1,0,0); add(1,1,1,0); add(2,0,5,0); add(3,1,2,0);
    add(4,1,7,0); add(5,1,9,0); add(6,1,8,0); add(7,-1,-1,1);
    n=drive_scan(out.drives,2); assert(n==8);
    assert(out.drives[0].kind==DRIVE_FLOPPY525 && out.drives[1].kind==DRIVE_FLOPPY525);
    assert(out.drives[2].kind==DRIVE_FIXED && out.drives[3].kind==DRIVE_FLOPPY35);
    assert(out.drives[4].kind==DRIVE_FLOPPY35 && out.drives[5].kind==DRIVE_FLOPPY35);
    assert(out.drives[6].kind==DRIVE_REMOVABLE && out.drives[7].kind==DRIVE_REMOTE);
    reset(0x0616); add(2,0,5,0); add(18,-1,-1,1);
    otter_number=18; otter_flags=DRIVE_OFFLINE;
    n=drive_scan(out.drives,2);
    assert(n==2 && out.drives[1].kind==DRIVE_SD && out.drives[1].flags==DRIVE_OFFLINE);
    assert(otter_calls==1 && type_calls==1);
    assert(drive_kind(18)==DRIVE_SD && drive_kind(0)==DRIVE_GENERIC && otter_calls==1);
    /* Offline OTTERSD still appears if DOS does not advertise it; a current
     * drive with unsupported queries gets a generic entry rather than vanishing. */
    reset(0x500); otter_number=25;
    n=drive_scan(out.drives,3);
    assert(n==2 && out.drives[0].number==3 && out.drives[0].kind==DRIVE_GENERIC);
    assert(out.drives[1].number==25 && out.drives[1].kind==DRIVE_SD);
    assert(drive_kind(18)==DRIVE_GENERIC && drive_kind(25)==DRIVE_SD && otter_calls==1);
    reset(0x0616);
    memset(out.canary,0xa5,sizeof(out.canary));
    for (i=0;i<26;++i) add(i,0,5,0);
    n=drive_scan(out.drives,25); assert(n==26);
    for (i=0;i<26;++i) assert(out.drives[i].number==i);
    for (i=0;i<sizeof(out.canary);++i) assert(out.canary[i]==0xa5);
    assert(!guarded);
    puts("PASS: DOS-version fallbacks, floppy/fixed/remote types, OTTERSD precedence/offline state and 26-drive bounds");
    return 0;
}
