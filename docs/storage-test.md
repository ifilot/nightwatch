# Storage integrity tests

Press **Ctrl-T**, then **Q** for quick, **L** for long, **V** to verify an
existing test, or **C** to clean up a recorded test. This feature is intended
for testing a Slot-otter SD card through OTTERSD, using ordinary DOS file calls.
It also runs against other writable DOS drives. It performs no raw-sector I/O,
formatting, driver installation or automatic unmounting.

## Profiles

| Profile | Stream file | Boundary files | Directory entries | Mutation cycles |
| --- | ---: | ---: | ---: | ---: |
| Quick | 1 MiB | 22 | 40 | 1 |
| Long | 16 MiB | 22 | 2,200 | 3 |

Boundary sizes include 0/1, 511/512/513, 4095/4096/4097,
32767/32768/32769, 65535/65536/65537, 131071/131072/131073 and
262143/262144/262145 bytes, plus all-zero and all-FF payloads. These cover
512-byte sectors and common cluster boundaries through 128 KiB. The boundary
files require about 1.49 MiB in addition to the stream. Directory allocation
and small-file cluster allocation add overhead; available bytes alone do not
predict exact disk-space requirements. Insufficient space stops the run and
retains partial files.

The long directory contains enough entries to grow beyond one 64 KiB FAT
cluster, and both profiles cross directory-sector boundaries. The test uses
streaming enumeration independent of Nightwatch's 512-entry pane cache.

A decimal 32-bit seed (0 through 4294967295) controls the data; the default
comes from the current time. Every generated 512-byte block contains the seed,
file identifier and block index, followed by a deterministic pseudorandom
stream. Different file identities and offsets help expose stale or misdirected
writes. Zero/FF files supplement the pseudorandom data. The generator is intended
for reproducible testing, not cryptographic use.

## Running a new test

1. Install OTTERSD with writable access and open its drive in Nightwatch.
2. Press Ctrl-T and Q or L, then confirm the target directory.
3. Choose a **local manifest/log directory on another drive**. For
   example, target `S:\` and logs in `C:\TESTLOG`. The default local directory
   is Nightwatch's startup directory. The guard compares DOS drive letters; choose
   a different physical disk for the evidence, rather than an alias of the card.
4. Accept or enter the seed, then confirm the profile. If either folder is
   missing, Nightwatch shows its full path and asks whether to create it and
   all missing parent folders. Press **Y** to approve each path; any other key
   cancels without creating folders or starting the test. Missing folder names
   must use DOS 8.3 format. Both paths are checked before creation.

Folders created by these prompts remain after test cleanup; cleanup removes
only the recorded `NWTxxxx` test folder. If folder creation fails (for example,
with a full or write-protected disk), the error is shown and the test does not
start. Any parent folders already created remain.

The program chooses an unused `NWT0001` through `NWT9999` directory on the
target and an equally named `.DAT` manifest locally. Existing directories,
manifests and logs are never overwritten. The completion screen shows the
manifest path. Each operation gets a new local `.L00` through `.L99` log.
The first lines record the plan version, profile, seed and target path; the
remaining lines identify operations, the result and any first failure.

Data files live under `DATA\NEST`; directory-growth files live under `GROW`.
All files are closed before a separate verification pass. Boundary and growth
files are read in reverse creation order. The test then deletes alternate
small files, recreates them with different contents, and checks all survivors.

Each mutation cycle creates temporary folders and a 70,001-byte file; overwrites
513 bytes beginning at offset 511; appends 513 bytes; truncates to 32,769 bytes;
and writes beyond EOF to extend back to 70,001 bytes. It verifies exact contents
and EOF after these changes, moves the file and a child directory across
parents, and removes the temporary entries. Permanent data is rechecked after
each cycle. Directory listings must match the expected names, types and counts.

**The extension check expects a zero-filled gap, matching OTTERSD's documented
behavior.** A different DOS filesystem may leave unspecified gap contents and
fail this check; such a failure alone does not establish faulty card hardware.
This test does not assess file sharing, locks, timestamps, attributes or power
loss; Slot-otter's HWRT suite covers additional DOS and driver behavior.

The dashboard shows a phase checklist with green checkmarks only after each
phase succeeds, a current-file progress bar and a segmented overall bar labelled
with completed phases. It also shows the seed, current operation/path, verified
KiB, file checks, elapsed time and sampled throughput for file I/O. Final results
retain the checklist and summary; failures/cancellation identify the interrupted
phase. CGA uses monochrome drawn marks; text mode uses `[v]`, `[>]` and `[x]`.
The overall bar counts completed phases, rather than estimating elapsed time.
Native graphics retain the panel and repaint changed fields, rather than
redrawing the full card for every data block. Escape cancels between blocks or metadata
operations. A blocking DOS call must return before cancellation can take effect.
Verified sector writes and large directory operations may be slow on an 8088.

## Verification after reboot

An immediate PASS covers the current DOS session. For persistence evidence:

1. Exit programs holding files on the SD drive and successfully run
   `OTTERSD /UNMOUNT` from another drive.
2. Reboot without reformatting or changing the card, then install/mount OTTERSD.
3. In Nightwatch, select the local `.DAT` manifest and press **Ctrl-T**, **V**,
   or enter its full path at the prompt.

Verification opens the saved plan and checks its ownership/completion markers,
directory structure, every byte and exact file lengths. It writes only a new
local log; the target is opened for reading only. An incomplete run cannot pass
saved verification. A corrupt/missing manifest or marker stops verification.
Safe unmount and reboot are manual: Nightwatch does not control a driver or
claim that closing a file alone proves persistence on physical media.

## Failures and cleanup

A write, read, close, resize, rename, directory or local-log error stops the
run. A byte mismatch records the file, absolute byte offset and expected/actual
values. Cancellation and errors preserve partial target files and local
evidence. Preserve these before retrying. A failed test is evidence of an
end-to-end problem; it does not by itself distinguish card, driver, filesystem,
RAM, power or bus faults.

Use **Ctrl-T**, **C** with the local manifest to remove recorded test data.
Cleanup requires a matching `OWNER.ID`, scans every known test directory before
deleting anything, and refuses if an unexpected entry or type is found. It
removes only the plan's filenames and directories, including partial runs.
Missing entries from an interrupted cleanup are accepted. It never calls the
ordinary recursive delete operation or deletes unrelated files in the parent.
The marker is removed last. If final directory removal fails, cleanup attempts
to restore the marker so it can be retried. A run interrupted before its ownership marker was
written is deliberately refused and needs manual inspection.

Local manifests and logs remain after cleanup. Their presence reserves the run
number for future tests, keeping evidence from different runs separate. Repeated
verification/cleanup stops after all 100 numbered logs are occupied.

The test covers allocated files and metadata reached by the chosen workload.
It does not certify all SD capacity, FAT consistency, physical write locations,
long-term retention, removal during I/O or power-loss recovery. Its DOSBox tests
validate DOS operation, not physical Slot-otter electrical behavior.

## Screenshot capture

Run `python3 tools/storage_screenshots.py` to build a disposable scripted
navigator and capture a real quick-test run in VGA. Running/completed dashboard
PNGs are written under `build/screenshots/`; capture hooks are present only in
the staged test sources. The public executable contains no screenshot hooks.
