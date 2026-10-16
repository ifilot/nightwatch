# SPDX-License-Identifier: GPL-3.0-only
"""Exercise Ctrl-T menu and saved-manifest flows in every DOS display mode.

Uses the freshly compiled scripted navigator from tests/video.py. Target and
local evidence have separate DOS drive letters; no physical media is touched.
"""
from pathlib import Path
import os
import shutil
import subprocess as sp

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT/'build/video'
ENV = dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')

def text(value): return [21,*map(ord,value),13]

def run(work, local, mode, machine, keys):
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    args = ['dosbox','-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(work/'FAST.CONF'),'-machine',machine]
    for cmd in [f'mount d "{work}"',f'mount e "{local}"','d:',r'UITEST /'+mode+r' D:\CARD D:\CARD','exit']:
        args += ['-c',cmd]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=180)

for machine,mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga')]:
    work = BASE/('storage-ui-'+mode)
    local = BASE/('storage-log-'+mode)
    for path in (work,local):
        if path.exists(): shutil.rmtree(path)
        path.mkdir()
    (work/'CARD').mkdir()
    shutil.copy2(Path(os.environ.get('NW_STORAGE_TEST_EXE',str(BASE/'UITEST.EXE'))),work/'UITEST.EXE')
    (work/'FAST.CONF').write_text('[cpu]\ncycles=max\n')
    # Accept target; choose E: for evidence; provide reproducible seed.
    keys = [20,ord('q'),13]+text('E:\\')+text('42')+[ord('y'),13,0x4400]
    run(work,local,mode,machine,keys)
    root = work/'CARD/NWT0001'
    assert (root/'DONE.ID').exists(),(mode,'menu did not complete test')
    assert (local/'NWT0001.DAT').read_text().splitlines()[1] == '0 42'
    keys = [20,ord('v')]+text(r'E:\NWT0001.DAT')+[13,0x4400]
    run(work,local,mode,machine,keys)
    assert 'PASS storage test completed' in (local/'NWT0001.L01').read_text()
    keys = [20,ord('c')]+text(r'E:\NWT0001.DAT')+[ord('y'),13,0x4400]
    run(work,local,mode,machine,keys)
    assert not root.exists(),(mode,'menu cleanup failed')
    # The long profile is available; cancel confirmation without writing anything.
    keys = [20,ord('l'),13]+text('E:\\')+text('0')+[27,0x4400]
    run(work,local,mode,machine,keys)
    assert not root.exists() and not (local/'NWT0002.DAT').exists()
    print('PASS:',mode,'Ctrl-T quick, saved verification, cleanup and cancelled long test',flush=True)
