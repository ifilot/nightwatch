# Built-in ARJ extraction

Select an ARJ archive and press **Ctrl-U** to unpack into the inactive pane.
The destination must already exist. Overwrite, skip, Escape cancellation and
progress use the same controls as ZIP. Archive detection uses the header bytes,
so an ARJ file need not have an `.ARJ` extension.

For unattended extraction:

```dos
NW /unarj D:\PACK.ARJ D:\OUT
```

This command refuses existing files and returns nonzero on failure. ARJ.EXE is
not needed; the decoder is embedded in the standalone executable.

## Supported subset

Single-volume ARJ with binary files, directories and empty files; stored method
0 and compressed methods 1–4. Methods 1–3 use a shared canonical Huffman decoder;
method 4 uses unary-coded lengths and distances. Both use a bounded 26,624-byte
history. Names must use ASCII DOS 8.3 components and fit Nightwatch's 127-byte
path limit. Both slash styles are normalized. Long names are rejected.

Unsupported features are rejected during preflight: encrypted/split/secured
archives, dual names, chapters, text-mode and special file entries, extended
headers, newer extraction versions above 3, methods above 4, and executable
self-extractors. The end marker must terminate the archive with no trailing data.
No ARJ writing is provided. Text mode is excluded because it has platform-specific
newline and character conversion semantics.

DOS timestamps and readonly/hidden/system/archive attributes are restored for
DOS-origin archives. Unix/NeXT timestamps are converted to DOS UTC, with dates
before 1980 clamped to 1980-01-01. Other hosts retain the header's DOS timestamp;
non-DOS permissions and directory timestamps are not restored.

## Validation and recovery

The reader streams every header with bounded buffers and checks its CRC before
creating output. Preflight checks the full record sequence, sizes, names, DOS
case collisions, file/directory conflicts, safe destination ancestors and source
archive preservation. Declared sizes, offsets and total output must fit signed
DOS long (2 GiB − 1); at most 65,534 entries are accepted. Pairwise name checks
use constant memory and O(n²) scan time. Escape can cancel the scan.

Each file is written to an exclusively created sibling temporary file. The
reader checks compressed bounds, Huffman tables, history references, output
length and CRC-32 before applying metadata and committing the file. Failed
replacement preserves the destination; shared recovery reports retained temporary
or backup paths if cleanup/restoration fails. Successful earlier files and
created directories remain after a later failure, as with ZIP.

Skipped files advance the operation totals without decoding. The decoder uses
one normalized far allocation for history/codebooks, and fixed near input/output
buffers. The Turbo C history/codebook allocation is 28,440 bytes. It allocates
no archive-sized table. ZIP and ARJ share path validation,
bounded reads and staged replacement in `src/ARCHIVE.C`. Archive error text
lives in an immutable assembly segment outside DGROUP. Production-wide sizes
change as other features evolve; use the linked map and memory check described
in [the manual](manual.md#display-design-rendering-and-memory) for current values.

## Implementation and verification

The decoder in `src/ARJDEC.C` is an original GPL-3.0-only implementation of the
ARJ bitstream. The historical UNARJ decoder is not vendored or linked. Format
references are the [ARJ technical notes](https://raw.githubusercontent.com/tripsin/unarj/master/TECHNOTE.TXT)
and the [ARJ producer sources](https://github.com/joncampbell123/arj), including
host-specific timestamps and file-mode definitions. CRC-32 uses the existing
vendored zlib implementation and its existing license notice.

Retained fixtures in `tests/fixtures/arj/` were produced independently by Debian
ARJ 3.10.22, with all five methods and original-file hashes. The fixture README
records producer provenance and regeneration. Synthetic records exercise DOS
metadata, singleton Huffman trees, invalid tables and unsafe input.

```sh
make test          # strict C89, ASan/UBSan, ARJ and ZIP regression tests
make test-arj-dos  # actual Turbo C / 8086 decoding and FAT12 staging
make test-video    # Ctrl-U dispatch, progress and skip handling in the navigator
```
