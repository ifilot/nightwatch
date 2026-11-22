/* SPDX-License-Identifier: GPL-3.0-only */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <dos.h>
#include <dir.h>
#include <process.h>
#include "DRIVE.H"
void drv_install(void interrupt (*previous)(void), unsigned number, unsigned online);
void interrupt drv_fake(void);
unsigned drv_calls(void);
void drv_dos_install(void interrupt (*previous)(void));
void interrupt drv_dos_fake(void);
unsigned _stklen=4096;
int main(int argc, char **argv)
{
    Drive drives[26];
    char before[NW_PATH], after[NW_PATH];
    unsigned i,n,disk=getdisk();
    void interrupt (*previous)(void)=getvect(0x2f);
    void interrupt (*previous_dos)(void)=getvect(0x21);
    void interrupt (*critical)(void)=getvect(0x24);
    union REGS r;
    int found=0, result;
    if (argc==5) {
        drv_install(previous,4,1); setvect(0x2f,drv_fake);
        drv_dos_install(previous_dos); setvect(0x21,drv_dos_fake);
        result=spawnl(P_WAIT,argv[1],argv[1],argv[2],argv[3],argv[4],NULL);
        setvect(0x21,previous_dos); setvect(0x2f,previous); return result;
    }
    assert(getcwd(before,sizeof(before)));
    n=drive_scan(drives,disk);
    for (i=0;i<n;++i) printf("%c %u %u\n",'A'+drives[i].number,drives[i].kind,drives[i].flags);
    assert(getcwd(after,sizeof(after)) && !strcmp(before,after) && getdisk()==disk);
    assert(getvect(0x24)==critical);
    drv_install(previous,4,0); setvect(0x2f,drv_fake);
    drv_dos_install(previous_dos); setvect(0x21,drv_dos_fake);
    n=drive_scan(drives,disk);
    assert(drives[0].number==0 && drives[0].kind==DRIVE_FLOPPY35);
    assert(drives[1].number==1 && drives[1].kind==DRIVE_FLOPPY525);
    for (i=0;i<n;++i) if (drives[i].number==4) {
        assert(drives[i].kind==DRIVE_SD && drives[i].flags==DRIVE_OFFLINE); found=1;
    }
    assert(found && drv_calls()==1); setvect(0x21,previous_dos); setvect(0x2f,previous);
    memset(&r,0,sizeof(r)); drive_guard_begin(); drive_guard_begin(); drive_guard_end(); int86(0x24,&r,&r); drive_guard_end();
    assert(r.h.al==3 && getvect(0x24)==critical);
    puts("PASS: DOS metadata, current-directory preservation, OTTERSD identity/offline and critical-error restoration");
    return 0;
}
