# SPDX-License-Identifier: GPL-3.0-only
"""Stage ASCII source with DOS line endings, without changing working sources."""
from pathlib import Path
import sys
# The caller creates target first; it must be a disposable staging directory.
source, target = map(Path, sys.argv[1:])
# A build must not reuse an earlier executable or completion marker. Only the
# staged directory is cleaned; the source tree and external toolchain are intact.
for old in target.iterdir():
    if old.is_file() and old.suffix.upper() in {'.OBJ', '.EXE', '.MAP', '.LOG', '.OK', '.FAIL'}:
        old.unlink()
# Turbo C/TASM consume DOS CRLF and 8.3 uppercase names. Reject non-ASCII
# source here, rather than silently corrupting comments or string literals.
for src in [*source.iterdir(), *(source.parent/'vendor/zlib').iterdir()]:
    if src.suffix.upper() in {'.C', '.H', '.ASM'} or src.name == 'MAKEFILE':
        text = src.read_text(encoding='ascii').replace('\r\n', '\n')
        (target / src.name.upper()).write_bytes(text.replace('\n', '\r\n').encode('ascii'))
