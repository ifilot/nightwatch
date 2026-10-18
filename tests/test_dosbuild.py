# SPDX-License-Identifier: GPL-3.0-only
"""A failed DOS compiler invocation must never certify an old executable."""
from pathlib import Path
from tempfile import TemporaryDirectory
from dosbuild import compile_dos
from dos_link import link_response, make_objects

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

with TemporaryDirectory(prefix='nw-link-list-') as directory:
    work = Path(directory)
    names = [f'PART{i:02d}.OBJ' for i in range(28)]
    for name in names: (work/name).write_bytes(b'object')
    def checked_link(d, commands):
        batch = (d/'CHECK.BAT').read_text()
        assert 'tlink /m /c @TEST.RSP > LINK.LOG' in batch and '*.OBJ' not in batch
        response = (d/'TEST.RSP').read_text()
        assert len(response) > 126
        assert all(name in response for name in names+['EXTRA.OBJ'])
        assert r'C:\TC\LIB\C0M.OBJ' in response and r'C:\TC\LIB\CM.LIB' in response
        (d/'TEST.EXE').write_bytes(b'complete executable')
        (d/'CHECK.OK').write_text('OK')
    compile_dos(work, checked_link, 'CHECK',
                ['tcc -mm -c EXTRA.C', 'tcc -mm -M -eTEST.EXE *.OBJ > LINK.LOG'],
                ['TEST.EXE'], ['LINK.LOG'])
root = Path(__file__).resolve().parents[1]
assert (root/'src/NW.RSP').read_bytes() == link_response(
    make_objects(root/'src/MAKEFILE'), 'NW.EXE').encode('ascii')
print('PASS: complete response-file object lists exceed 126 bytes; production list matches DOS Makefile')
