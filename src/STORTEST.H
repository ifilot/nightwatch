/* SPDX-License-Identifier: GPL-3.0-only */
/* DOS-file storage qualification. No raw sectors or driver control calls.
 * Manifests/logs live on another drive; target directories are exclusive.
 * Failures/cancellation preserve evidence. Verification never writes target data.
 */
#ifndef STORTEST_H
#define STORTEST_H
#include "NW.H"
/* A live, allocation-free summary for renderers. Passed bits are set only
 * after a whole phase succeeds; counters count successful stream/check passes.
 */
#define STORAGE_RUNNING 0
#define STORAGE_PASSED 1
#define STORAGE_FAILED 2
#define STORAGE_CANCELLED 3
typedef struct {
    unsigned action, extended, stage, stages, passed, state, cycle, cycles;
    unsigned long seed, written, verified, files;
} StorageReport;
extern StorageReport storage_report;
/* Return zero to cancel. Polled between blocks and metadata operations. */
extern int (*storage_progress)(const char *phase, const char *path,
                              unsigned long done, unsigned long total);
/* Quick: boundary files + 1 MiB + 40 directory entries, one mutation cycle.
 * Long: boundary files + 16 MiB + 2200 entries, three mutation cycles.
 * seed is an explicit 32-bit value, including on wider host platforms.
 * manifest receives the chosen local manifest path, even after a partial failure.
 */
int storage_start(const char *target, const char *local, int extended,
                  unsigned long seed, char *manifest);
/* Reopen a saved manifest. Verify requires a completed run; cleanup also handles
 * incomplete runs, but requires the ownership marker and rejects unknown entries.
 * Cleanup retains local manifests/logs. No recursive wildcard deletion is used.
 */
int storage_verify(const char *manifest);
int storage_cleanup(const char *manifest);
#endif
