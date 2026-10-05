/* SPDX-License-Identifier: GPL-3.0-only */
/* Test-only launcher: Escape behind an unrelated key during the second file. */
#include <dos.h>
#include <process.h>
#include <stdio.h>
void cancel_set(unsigned offset, unsigned segment);
int cancel_count(void);
void interrupt cancel_hook(void);
static unsigned keys[] = {0x5000,0x0d2b,0x3f00,0x1c0d};
int main(int argc, char **argv)
{
    unsigned far *buffer = (unsigned far *)MK_FP(0x40,0x1e);
    unsigned far *head = (unsigned far *)MK_FP(0x40,0x1a);
    unsigned far *tail = (unsigned far *)MK_FP(0x40,0x1c);
    void interrupt (*previous)();
    int i, result;
    disable();
    for (i=0;i<sizeof(keys)/sizeof(keys[0]);++i) buffer[i]=keys[i];
    *head=0x1e; *tail=0x1e+sizeof(keys);
    enable();
    /* The child executes synchronously; restore INT 21h before this launcher
     * exits so DOS never retains a vector into freed program memory. */
    previous=getvect(0x21); cancel_set(FP_OFF(previous),FP_SEG(previous));
    setvect(0x21,cancel_hook);
    result=spawnl(P_WAIT,"NIGHT.EXE","NIGHT",argc>1 ? argv[1] : "/text","D:\\LEFT","D:\\RIGHT",NULL);
    setvect(0x21,previous);
    printf("Cancellation returned %d; injected %d\n",result,cancel_count());
    return result || cancel_count()!=1;
}
