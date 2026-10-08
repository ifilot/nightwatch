# SPDX-License-Identifier: GPL-3.0-only
"""Check scrollable Help using the fresh UITEST built by tests/video.py."""
from pathlib import Path
import os
import shutil
import subprocess as sp
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tools'))
from vram import decode
base = root/'build/video'
env = dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
for machine, mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga'),('hercules','text')]:
    work = base/('help-scroll-'+machine+'-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    keys = [0x3b00,0x4700,0x4800,0x5100,0x4900,0x5000,0x4800,
            0x4f00,0x5000,0x5100,0x4900,0x4700,27,0x4400]
    (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys))
    args=['dosbox','-conf',str(root/'tools/dosbox.conf'),'-conf',str(base/'FAST.CONF'),'-machine',machine]
    for cmd in [f'mount d "{work}"','d:',r'UITEST /'+mode+r' D:\LEFT D:\RIGHT','exit']:
        args += ['-c',cmd]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=45)
    frames=[(work/f'F{i:03d}.BIN').read_bytes() for i in range(len(keys))]
    assert frames[0]==frames[13],(mode,'Help did not restore the desktop')
    assert all(frames[i]==frames[1] for i in (2,3,5,7,12)),(mode,'Help top/round trips')
    assert frames[8]==frames[9]==frames[10],(mode,'Help bottom clamp')
    assert frames[1]!=frames[4] and frames[1]!=frames[6] and frames[8]!=frames[11]
    writes=[int((work/f'F{i:03d}.POS').read_text().split()[7]) for i in range(len(keys))]
    assert writes[1]==writes[2]==writes[3] and writes[8]==writes[9]==writes[10], 'Boundary keys repainted'
    if mode=='text':
        last=frames[8][3:4003:2].decode('cp437')
        assert 'FONTLIC.TXT' in last and 'ICONLIC.TXT' in last and 'GPLv3' in last and '#ABOUT' not in last
    else:
        images=[decode(work/f'F{i:03d}.BIN') for i in range(len(keys))]
        f={'cga':8,'ega':12,'vga':16}[mode]
        y=(images[1].height-(17*f+52))//2
        for index in (4,6,8,11):
            assert images[index].crop((18,y,622,y+2*f+24)).tobytes()==images[1].crop((18,y,622,y+2*f+24)).tobytes(), 'Help metadata moved'
            assert images[index].crop((18,y+16*f+37,622,y+17*f+52)).tobytes()==images[1].crop((18,y+16*f+37,622,y+17*f+52)).tobytes(), 'Help controls moved'
    print('PASS:',machine,mode,'Help line/page scrolling, Home/End, bounds, fixed metadata and desktop restoration',flush=True)
