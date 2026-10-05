/* SPDX-License-Identifier: GPL-3.0-only */
#ifndef FS_FAULTS_H
#define FS_FAULTS_H
/* Fail one selected boundary call, rather than duplicating transfer internals. */
/* reset clears queued faults and cross-drive simulation. fail borrows its
 * strings and fails once on the 1-based matching call; empty suffix matches all.
 * The fixed schedule holds eight faults. Invalid schedules abort the test. */
void fs_test_reset(void);
void fs_test_fail(const char *operation, const char *suffix, unsigned call);
extern int fs_test_cross_drive;
#endif
