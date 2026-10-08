# Cross-platform ZIP fixtures

These are retained archives actually produced on Windows and Linux, not
Python ZIP files with an OS tag changed. CI consumes the checked-in bytes;
it does not require Windows or regenerate them during tests.

- `LINUX0.ZIP`, `LINUX9.ZIP`: Info-ZIP Zip 3.0 on Linux, `zip -0 -r` and
  `zip -9 -r`, including Unix metadata extra fields. Linux Zip switches to
  method 0 for incompressible or empty files even at level 9.
- `WINFAST.ZIP`, `WINBEST.ZIP`, `WINSTORE.ZIP`: Windows .NET `ZipArchive`
  using Fastest, Optimal and NoCompression. NoCompression still writes
  method 8 with uncompressed DEFLATE blocks for nonempty entries.
- `WINPS.ZIP`: Windows PowerShell 5.1 `Compress-Archive -CompressionLevel
  Optimal`. This writer uses backslashes in member paths, including folders.
  The reader normalizes and validates both separator conventions.

Windows/PowerShell/CLR versions are captured in `windows-provenance.txt`.
`manifest.json` records the uncompressed sizes and SHA-256 hashes, independent
of archive metadata. The data includes empty files/folders, >64 KiB binary and
text files, incompressible bytes and a repeat at a 30,000-byte distance.

Regeneration: run `python3 tests/fixtures/zip/generate.py` on Linux, then run
`generate-windows.ps1` with Windows PowerShell. The generators write only
repository fixtures and scratch input under build/. Retain the new provenance
when updating archives. ZIP timestamps/extra metadata can change on regeneration.

`tests/zip.py` adds synthetic fixed/dynamic/stored DEFLATE blocks, levels
0/1/6/9 and five strategies, signed/unsigned descriptors, UTF-8-flagged ASCII,
maximum comments, malformed containers, hostile names and filesystem faults.
Those generated edge cases supplement rather than replace these OS fixtures.
