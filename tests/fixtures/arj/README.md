# Independent ARJ fixtures

`METHOD0.ARJ` through `METHOD4.ARJ` were generated with Debian's ARJ 3.10.22
package `arj_3.10.22-26_amd64.deb`, using methods `-m0` through `-m4`, binary
mode `-t0`, directory inclusion `-a1` and recursion `-r`. Incompressible and
empty entries may use method 0 even when another method is requested. Tests
verify that each archive contains its requested method.

The producer package was downloaded from
https://deb.debian.org/debian/pool/main/a/arj/arj_3.10.22-26_amd64.deb
with SHA-256 `e97f6cef13c840e353bffed80668bf89055b7b1485534cd537c76ced36bea676`.
Its binary was extracted under `/tmp` with `dpkg-deb -x`; no producer executable
is shipped or used at runtime. The producer identifies itself as ARJ32 v3.10.

`generate.py` defines the original input bytes independently of the decoder,
including >64 KiB files and empty files/directories. `WINDOW.ARJ` adds a
method-1 archive with a 26,000-byte match distance and history wraparound. `manifest.json` records
input lengths and SHA-256 digests. `archive-sha256.json` pins the retained
archive bytes. The `.log` files retain the producer's output. Method 0–4 file timestamps are fixed at Unix epoch 946684800
(2000-01-01 UTC). Archive creation times and the WINDOW fixture timestamp
vary on regeneration, so regenerated archive hashes will differ.

```sh
python3 tests/fixtures/arj/generate.py --arj /path/to/arj
```

These are actual Linux-produced ARJ archives. DOS metadata is additionally
covered by synthetic records in `tests/arj.py`; the retained fixtures are run
through the production decoder on the real 16-bit ABI and DOS FAT12.
The fixture payloads and generator are original GPL-3.0-only test material.
