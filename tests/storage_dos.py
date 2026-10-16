# SPDX-License-Identifier: GPL-3.0-only
"""Run production storage operations in DOS, then restart and verify persistence."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess as sp
from dosbuild import compile_dos

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'build/storage-dos'
WORK.mkdir(parents=True, exist_ok=True)
for name in ('CORE.C','FSDOS.C','NW.H','VERSION.H','STORTEST.C','STORTEST.H','STORMSG.H','FONT.ASM'):
    (WORK/name).write_bytes((ROOT/'src'/name).read_text().replace('\n','\r\n').encode('ascii'))
(WORK/'STPROBE.C').write_bytes((ROOT/'tests/STPROBE.C').read_text().replace('\n','\r\n').encode('ascii'))
(WORK/'FAST.CONF').write_text('[cpu]\ncycles=max\n')
(WORK/'XT.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
toolchain = Path(os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv')))
env = dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
card = WORK/'CARD'; local = WORK/'LOCAL'
for directory in (card,local):
    if directory.exists(): shutil.rmtree(directory)
    directory.mkdir()
(card/'KEEP.TXT').write_bytes(b'preserve existing data')

def run(directory, commands, xt=False):
    args = ['dosbox','-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(WORK/'FAST.CONF')]
    if xt:
        args = ['dosbox-x','-fastlaunch','-nogui','-conf',str(ROOT/'tools/dosbox.conf'),
                '-conf',str(WORK/'XT.CONF'),'-machine','cga']
    for cmd in [f'mount c "{toolchain}"',f'mount d "{WORK}"',f'mount e "{card}"','d:',*commands,'exit']:
        args += ['-c',cmd]
    with (WORK/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=180)

compile_dos(WORK,run,'STBUILD',
    [r'set PATH=C:\TC;C:\TASM','tasm /mx FONT.ASM > ASSEMBLE.LOG',r'tcc -1- -mm -O -Z -IC:\TC\INCLUDE -LC:\TC\LIB -eSTPROBE.EXE STPROBE.C STORTEST.C CORE.C FSDOS.C FONT.OBJ > COMPILE.LOG'],
    ['STPROBE.EXE','FONT.OBJ'],['ASSEMBLE.LOG','COMPILE.LOG'])
run(WORK,[r'STPROBE start E:\ D:\LOCAL 0 305419896 > RUN.LOG'])
assert 'PASS' in (WORK/'RUN.LOG').read_text(),(WORK/'RUN.LOG').read_text()
root = card/'NWT0001'
assert (root/'DONE.ID').exists() and (local/'NWT0001.DAT').exists()
assert len(list((root/'GROW').iterdir())) == 40
assert (root/'DATA/NEST/B13.BIN').stat().st_size == 65537
assert list((root/'SCRATCH').iterdir()) == []

def hashes():
    return {str(p.relative_to(card)):hashlib.sha256(p.read_bytes()).hexdigest()
            for p in card.rglob('*') if p.is_file()}

before = hashes()
run(WORK,[r'STPROBE verify D:\LOCAL\NWT0001.DAT > VERIFY.LOG'])
assert 'PASS' in (WORK/'VERIFY.LOG').read_text(),(WORK/'VERIFY.LOG').read_text()
assert hashes() == before, 'Verification changed target files'
# A different drive is required for evidence, before target mutation.
run(WORK,[r'STPROBE start E:\ E:\ 0 1 > GUARD.LOG'])
assert 'another drive' in (WORK/'GUARD.LOG').read_text()
assert hashes() == before
with (root/'DATA/NEST/B13.BIN').open('r+b') as f:
    f.seek(65536); byte=f.read(1)[0]; f.seek(65536); f.write(bytes([byte ^ 255]))
run(WORK,[r'STPROBE verify D:\LOCAL\NWT0001.DAT > BAD.LOG'])
assert 'byte 65536 expected' in (WORK/'BAD.LOG').read_text(),(WORK/'BAD.LOG').read_text()
run(WORK,[r'STPROBE cleanup D:\LOCAL\NWT0001.DAT > CLEAN.LOG'])
assert 'PASS' in (WORK/'CLEAN.LOG').read_text(),(WORK/'CLEAN.LOG').read_text()
assert not root.exists()
assert (card/'KEEP.TXT').read_bytes() == b'preserve existing data'
assert (local/'NWT0001.DAT').exists()
print('PASS: DOS quick storage test, cross-directory moves, seek/append/truncate/gap, fresh-launch verify, same-drive guard, exact mismatch and recorded cleanup',flush=True)

# The same optimized production C and far-message ABI must run on an 8086.
run(WORK,[r'STPROBE start E:\ D:\LOCAL 0 7 > XTRUN.LOG'],xt=True)
assert 'PASS' in (WORK/'XTRUN.LOG').read_text(),(WORK/'XTRUN.LOG').read_text()
assert (card/'NWT0002/DONE.ID').exists()
before = hashes()
run(WORK,[r'STPROBE verify D:\LOCAL\NWT0002.DAT > XTVRF.LOG'],xt=True)
assert 'PASS' in (WORK/'XTVRF.LOG').read_text(),(WORK/'XTVRF.LOG').read_text()
assert hashes() == before
run(WORK,[r'STPROBE cleanup D:\LOCAL\NWT0002.DAT > XTCLN.LOG'],xt=True)
assert 'PASS' in (WORK/'XTCLN.LOG').read_text(),(WORK/'XTCLN.LOG').read_text()
assert not (card/'NWT0002').exists()
print('PASS: production storage engine and far-message access on DOSBox-X 8086 CPU',flush=True)
