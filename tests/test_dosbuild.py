# SPDX-License-Identifier: GPL-3.0-only
"""A failed DOS compiler invocation must never certify an old executable."""
from pathlib import Path
from tempfile import TemporaryDirectory
from dosbuild import compile_dos

def rejected(work, runner):
    (work/'TEST.EXE').write_bytes(b'old executable')
    (work/'CHECK.OK').write_text('old marker')
    try:
        compile_dos(work, runner, 'CHECK', ['tcc TEST.C'], ['TEST.EXE'], [])
    except AssertionError:
        return
    raise AssertionError('Failed/stale build was accepted')

with TemporaryDirectory(prefix='nw-build-gate-') as directory:
    work = Path(directory)
    rejected(work, lambda d, c: None)
    def missing_output(d, c): (d/'CHECK.OK').write_text('OK')
    rejected(work, missing_output)
    def compiler_failed(d, c):
        (d/'TEST.EXE').write_bytes(b'incomplete executable')
        (d/'CHECK.FAI').write_text('FAILED')
    rejected(work, compiler_failed)
    def success(d, c):
        (d/'TEST.EXE').write_bytes(b'fresh executable')
        (d/'CHECK.OK').write_text('OK')
    compile_dos(work, success, 'CHECK', ['tcc TEST.C'], ['TEST.EXE'], [])
    assert (work/'TEST.EXE').read_bytes() == b'fresh executable'
print('PASS: DOS build gate rejects stale outputs, missing output and compiler failure')
