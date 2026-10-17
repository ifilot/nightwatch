/* SPDX-License-Identifier: GPL-3.0-only */
#include <dos.h>
#include <string.h>
#include "DRIVE.H"

void interrupt drive_fail(void);
static void interrupt (*old_critical)(void);
static unsigned guard_depth;
void drive_guard_begin(void)
{ if (!guard_depth++) { old_critical=getvect(0x24); setvect(0x24,drive_fail); } }
void drive_guard_end(void) { if (guard_depth && !--guard_depth) setvect(0x24,old_critical); }

unsigned drive_dos_version(void)
{
    union REGS r;
    memset(&r,0,sizeof(r)); r.h.ah=0x30; intdos(&r,&r);
    return ((unsigned)r.h.al<<8)|r.h.ah;
}
static int query(unsigned number, unsigned function, unsigned *value)
{
    union REGS r;
    memset(&r,0,sizeof(r)); r.x.ax=function; r.x.bx=number+1;
    intdos(&r,&r);
    if (r.x.cflag) return r.x.ax==15 ? -1 : 0;
    *value=function==0x4409 ? (r.x.dx&0x1000)!=0 : r.x.ax==0;
    return 1;
}
int drive_remote(unsigned number, unsigned *value)
{ return query(number,0x4409,value); }
int drive_removable(unsigned number, unsigned *value)
{ return query(number,0x4408,value); }

static unsigned word(const unsigned char *at)
{ return (unsigned)at[0]|((unsigned)at[1]<<8); }
int drive_device(unsigned number, unsigned *value)
{
    union REGS r;
    struct SREGS s;
    /* DOS 3.2's generic disk parameter packet includes the BPB and a variable
     * track layout. A zero special-functions byte requests default geometry,
     * rather than reading the inserted disk's BPB. Leave room for 63 sectors. */
    unsigned char parameters[320];
    memset(&r,0,sizeof(r)); memset(parameters,0,sizeof(parameters));
    segread(&s); s.ds=FP_SEG((void far *)parameters);
    r.x.ax=0x440d; r.x.bx=number+1; r.x.cx=0x0860;
    r.x.dx=FP_OFF((void far *)parameters); intdosx(&r,&r,&s);
    if (r.x.cflag) return 0;
    *value=parameters[1];
    /* Type 7 also meant "other block device" in older interfaces. Only use
     * the 3.5-inch icon when default geometry confirms a 1.44 MB floppy. */
    if (*value==7 && (word(parameters+4)!=80 || word(parameters+7)!=512 ||
                     word(parameters+20)!=18 || word(parameters+22)!=2)) return 0;
    return 1;
}
int drive_otter(unsigned *number, unsigned *flags)
{
    union REGS r;
    memset(&r,0,sizeof(r)); r.x.ax=0xd74f;
    int86(0x2f,&r,&r);
    if (r.x.cflag || r.x.ax!=0x4f54 || r.x.bx!=0x524f || r.x.cx>25) return 0;
    *number=r.x.cx; *flags=r.x.dx ? 0 : DRIVE_OFFLINE;
    return 1;
}
