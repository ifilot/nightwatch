/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef DRIVE_H
#define DRIVE_H
#include "NW.H"
enum {
    DRIVE_GENERIC, DRIVE_FIXED, DRIVE_REMOVABLE, DRIVE_FLOPPY35,
    DRIVE_FLOPPY525, DRIVE_SD, DRIVE_REMOTE
};
#define DRIVE_OFFLINE 1
typedef struct { unsigned char number, kind, flags; } Drive;
/* Caller owns space for 26 entries. Enumeration does not open drive roots or
 * change DOS's current drive/directory. current is 0..25, or 26 if unknown.
 * OTTERSD discovery checks its media-present signal once per scan; the driver
 * may mark a removed card offline. No mount/unmount controls are requested. */
unsigned drive_scan(Drive *out, unsigned current);
/* Reuse the latest scan for pane captions; first lookup scans lazily. */
unsigned drive_kind(unsigned number);
/* DOS boundary: queries return 1 success, 0 unsupported/unknown, -1 invalid
 * drive. Values are meaningful only on success. Numbers are zero-based. */
/* Raw AH=30h version: major in high byte, decimal minor in low byte.
 * DOS 3.10 therefore returns 0x030a; DOS 3.20 returns 0x0314. */
unsigned drive_dos_version(void);
int drive_remote(unsigned number, unsigned *value);
int drive_removable(unsigned number, unsigned *value);
int drive_device(unsigned number, unsigned *value);
int drive_otter(unsigned *number, unsigned *flags);
/* Scope DOS critical errors to Fail, restoring the caller's vector afterward.
 * Foreground scopes may nest; only the outer scope owns the DOS vector. */
void drive_guard_begin(void);
void drive_guard_end(void);
/* Labels and sprites live in FONTDATA, outside the DOS near heap. */
enum { DL_TITLE=7, DL_PATH, DL_KEYS, DL_OFFLINE };
char *drive_label(char *out, unsigned index);
#ifndef NW_HOST
const char NW_FAR *asset_drive_label(unsigned index);
const unsigned char NW_FAR *asset_drive_icon(unsigned index);
#endif
#endif
