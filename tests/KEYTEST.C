/* SPDX-License-Identifier: GPL-3.0-only */
/* Exercise the unmodified production BIOS keyboard loop. */
#include <dos.h>
#include <process.h>
#include <stdio.h>
#include <string.h>
/* BIOS ring has 16 word slots but leaves one empty: this script uses 15.
 * Words contain scan code in the high byte and ASCII in the low byte. */
static unsigned keys[] = {
    0x6800, 0x011b, 0x3c00, 0x1474, 0x0f09, 0x0f09,
    0x4700, 0x5000, 0x5000, 0x5600, 0x3e00, 0x011b, 0x3f00, 0x1c0d, 0x4400
};
int main(int argc, char **argv)
{
    unsigned far *buffer = (unsigned far *)MK_FP(0x40, 0x1e);
    unsigned far *head = (unsigned far *)MK_FP(0x40, 0x1a);
    unsigned far *tail = (unsigned far *)MK_FP(0x40, 0x1c);
    int i, result;
    /* Seed BDA head/tail atomically so the timer/keyboard cannot see half a ring. */
    disable();
    for (i = 0; i < sizeof(keys)/sizeof(keys[0]); ++i) buffer[i] = keys[i];
    *head = 0x1e; *tail = 0x1e + sizeof(keys);
    enable();
    result = spawnl(P_WAIT, "NIGHT.EXE", "NIGHT", argc > 1 ? argv[1] : "/text", "D:\\LEFT", "D:\\RIGHT", NULL);
    printf("Production navigator returned %d\n", result);
    return result;
}
