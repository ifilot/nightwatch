/* SPDX-License-Identifier: GPL-3.0-only */
/* Check the exact 79-column hex layout, including group spacing, ASCII
 * sanitization and cleared columns after partial/empty final rows.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "HEX.H"
int main(void)
{
    char row[80];
    unsigned char bytes[16] = {0, 1, 9, 10, 13, 31, 32, 65, 126, 127, 128, 255, 48, 90, 97, 122};
    hex_line(row, 0x1234ABCDUL, bytes, 16);
    assert(strlen(row) == 79);
    assert(!strncmp(row, "1234ABCD  00 01 09 0A 0D 1F 20 41  7E 7F 80 FF 30 5A 61 7A", 58));
    assert(!strcmp(row + 61, "|...... A~...0Zaz|"));
    hex_line(row, 0x10000UL, bytes + 7, 1);
    assert(!strncmp(row, "00010000  41 ", 13));
    assert(row[13] == ' ' && row[62] == 'A' && row[63] == ' ' && row[77] == ' ');
    hex_line(row, 0, bytes, 0);
    assert(row[10] == ' ' && row[62] == ' ' && row[78] == '|');
    puts("PASS: hex offsets, byte groups, printable ASCII and partial rows");
    return 0;
}
