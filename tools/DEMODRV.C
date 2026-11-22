/* SPDX-License-Identifier: GPL-3.0-only */
/* Keep demo identity hooks in the parent; run the unchanged release binary. */
#include <dos.h>
#include <process.h>
#include <stdio.h>

void drv_install(void interrupt (*previous)(void), unsigned number, unsigned online);
void interrupt drv_fake(void);
void drv_dos_install(void interrupt (*previous)(void));
void interrupt drv_dos_fake(void);
unsigned _stklen=2048;

int main(int argc, char **argv)
{
    void interrupt (*old_mux)(void)=getvect(0x2f);
    void interrupt (*old_dos)(void)=getvect(0x21);
    int result;
    if (argc!=5) {
        puts("Usage: DEMODRV program /mode left-path right-path");
        return 1;
    }
    drv_install(old_mux,18,1); /* S:, zero based. */
    drv_dos_install(old_dos);
    setvect(0x2f,drv_fake);
    setvect(0x21,drv_dos_fake);
    result=spawnl(P_WAIT,argv[1],argv[1],argv[2],argv[3],argv[4],NULL);
    setvect(0x21,old_dos);
    setvect(0x2f,old_mux);
    if (result<0) {
        perror("Cannot start Nightwatch");
        return 1;
    }
    return result;
}
