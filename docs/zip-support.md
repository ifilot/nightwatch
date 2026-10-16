# ZIP extraction in v1.2.0

Select an archive and press **Ctrl-U** to unpack into the inactive pane. Choose
an existing destination directory. Overwrite, skip and Escape cancellation use
the usual operation controls and progress dialog. For command-line extraction:

```
NW /unzip D:\PACK.ZIP D:\OUT
```

The command-line operation refuses overwrites and returns nonzero on failure.
Relative archive filenames also work. It does not invoke an external unzip
program or require a zlib DLL.

## Formats and compression details

Supported: classic single-volume ZIP with stored entries (method 0) or standard
DEFLATE (method 8), subdirectories/empty files, optional data descriptors with
or without their signature, comments and well-formed non-ZIP64 extra fields.
Names must be ASCII DOS 8.3 components and fit the program's 127-byte paths.
UTF-8-flagged names are accepted when their bytes are representable ASCII.
Windows PowerShell backslashes and standard ZIP slashes are both validated and
normalized. Long or Unicode names are rejected instead of silently shortened.
Archive offsets and total declared output must fit signed DOS long (2 GiB − 1).

Rejected: ZIP64, encryption, split archives, Deflate64 (method 9), other
compression methods, symbolic links and special files. No ZIP writing is added.

Compression **level** does not affect compatibility: fast, optimal and maximum
DEFLATE compression all decode through the same [zlib raw-inflate
API](https://zlib.net/manual.html), with a full 32 KiB history window. Tests
exercise levels 0/1/6/9 and default, filtered, Huffman-only, RLE and fixed
strategies. Fixed Huffman codes, dynamic codes and uncompressed DEFLATE blocks
are all supported. Compression method, rather than level or strategy, determines
whether a ZIP can be opened.

Two observed writer details are covered by retained fixtures:

- Windows .NET's `NoCompression` uses method 8 with uncompressed DEFLATE blocks
  for nonempty files; this differs from a method 0 stored ZIP entry.
- Linux Info-ZIP can choose method 0 for incompressible/empty files even when
  invoked with `-9`. PowerShell 5.1 `Compress-Archive` uses backslashes in names.

## Integrity and recovery

Following the [PKWARE ZIP specification](https://pkware.cachefly.net/webdocs/casestudies/APPNOTE.TXT),
the reader validates the end record, streamed central directory, matching local
headers/names, extra-field framing, bounded compressed ranges and descriptors.
Preflight rejects overlapping records, duplicate names after DOS case/separator
normalization, file/directory conflicts, traversal, absolute paths, reserved
DOS device names, and destination ancestors that are files or aliases.

The complete structural/name validation runs before any output is written.
The directory is streamed with constant memory; pairwise collision checks are
O(n²), so very large archives can have a noticeable startup delay. Escape is
polled during that scan. Entry count is independent of the 512-entry pane cache.

Each file writes to an exclusively created sibling temporary file. The reader
bounds output by the accepted size, requires complete DEFLATE/compressed input
consumption and verifies the uncompressed size and CRC-32. Only then are its
DOS timestamp/attributes applied and its destination committed. Existing files
remain untouched until the replacement is ready; shared copy recovery retains
a named backup if restoration is impossible. Short writes, close/stamp failures,
corruption, cancellation and allocation failures all follow staging cleanup.
A cleanup failure reports the retained temporary or backup pathname.

Successful earlier entries and created directories remain on a later failure;
extraction is transactional per file, not per archive. Skipped entries advance
resolved-byte and skipped-file totals without decompressing. The source archive
is never modified and cannot itself be an output destination. Directory timestamps
and Unix permissions are not restored; Unix files retain their DOS timestamp.

## Library and memory integration

The vendored [zlib 1.3.2 source](https://zlib.net/) is pinned to the official
archive SHA-256:

```
bb329a0a2cd0274d05519d61c667c062e06990d72e125ee2dfa8de64f0119d16
```

Only inflate/inffast/inftrees/zutil/adler32/crc32 are built, with `Z_SOLO` and
`NO_GZIP`; compression and gzip APIs are excluded. Turbo C 2.0 adaptations are
marked in source and documented in `vendor/zlib/README.md`. They address
unsupported far scalar typedefs/#warning, far decoder and zmemcmp pointers,
and CRC table declarations. The fixed Huffman tables are generated into
`FIXED.ASM`, outside DGROUP. The zlib license accompanies the source and DOS
package as `ZLIBLIC.TXT`.

A direct small-model link initially exceeded the 64 KiB data/stack budget and
left too little code space for archive handling. The program now uses medium
model: code calls are far, ordinary data stays near. Font/help assembly uses the
matching far-call ABI. The redundant 4 KiB font cache is removed; glyphs are read
from their existing immutable far segment. The two 512-entry pane caches and
16 KiB copy buffer with 2 KiB fallback remain intact.

Inflater allocations use normalized far pointers and retain the original pointer
for freeing. The measured payload peak from the original 8086 probe was 39,842
bytes (7,074 bytes state plus 32,768 bytes history), excluding allocator overhead
and the input/output buffers. Input and output are separate 1 KiB buffers; no
archive-controlled large allocation occurs. Allow **256 KiB free conventional
RAM**. The measured executable is 104,008 bytes. Static near data is 53,472 bytes;
with the 8,192-byte stack this leaves 3,872 bytes of near heap. The build
enforces at least 2 KiB of near heap after that stack.

## Tests and reproducibility

`tests/fixtures/zip/` contains **actual OS-created archives**, retained for CI:
Linux Info-ZIP `-0`/`-9`, Windows .NET Fastest/Optimal/NoCompression, and Windows
PowerShell Compress-Archive Optimal. Their independent uncompressed manifest,
archive hashes, Windows/CLR versions and regeneration scripts accompany them.
The Windows fixtures were created with Windows PowerShell, not by changing a
host-side ZIP's creator flag. CI tests the saved bytes without requiring Windows.

```
make test          # ASan/UBSan, OS fixtures, compression matrix and fault tests
make test-zip-dos  # same decoder on the real 16-bit 8086 ABI, plus FAT12
make test-video    # Ctrl-U and existing UI/graphics workflows in all modes
```

Tests include >64 KiB outputs, a match at a 30,000-byte distance, empty files and
folders, >512 entries, both descriptor forms, maximum comments, extra fields,
corrupt/truncated payloads, CRC/size mismatches, excess compressed/output data,
unsupported formats, hostile paths, duplicate/case collisions, filesystem faults,
overwrite/skip/cancel, far-allocation failures and retained cleanup filenames.
Allocation balance is asserted on successful and failed codec paths.

The original isolated codec probe remains available:

```
python3 tools/probe_zlib.py --source build/zlib-source
```

It expects untouched official zlib 1.3.2 sources and stages its adaptations only
under `build/zlib-probe`. It does not download sources or alter the vendored
production code. The probe's original small-model link measurements are
historical; the production implementation resolves those limits as above.
