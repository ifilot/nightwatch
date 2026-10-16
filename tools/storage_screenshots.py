# SPDX-License-Identifier: GPL-3.0-only
"""Capture actual running/completed storage dashboards from emulator VRAM."""
from pathlib import Path
import os, sys, shutil, subprocess as sp
root=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
sys.path.insert(0,str(root/'tools'))
from doszip import compile_app
from vram import decode
work=root/'build/storage-preview'
work.mkdir(exist_ok=True)
local=work/'LOCAL'; card=work/'CARD'
for folder in (local,card):
    if folder.exists(): shutil.rmtree(folder)
    folder.mkdir()
for src in (root/'src').iterdir():
    if src.suffix not in ('.C','.H','.ASM'):continue
    source=src.read_text()
    if src.name=='MAIN.C':
        begin=source.index('static int key_read(void)')
        end=source.index('static int pane_rows',begin)
        source=source[:begin]+'#include "UITEST.H"\nstatic int key_read(void) { return test_key_read(); }\nstatic int modal_key_read(void) { return test_key_read(); }\n'+source[end:]
        source=source.replace('    unsigned key;\n    while (bioskey(1))', '    unsigned key;\n    static int captured;\n    while (bioskey(1))')
        needle='        paint_tick = now; force_progress = 0;'
        assert needle in source
        source=source.replace(needle,needle+'''
        if (!captured && !strcmp(phase, "VERIFY") && strstr(path, "BIG.BIN") &&
            total && done >= total / 2 && done < total) {
            char name[13];
            captured = 1; test_capture(); --test_frame;
            sprintf(name,"F%03d.BIN",test_frame); remove("RUNNING.BIN"); rename(name,"RUNNING.BIN");
            sprintf(name,"F%03d.POS",test_frame); remove(name);
        }
''')
    (work/src.name).write_bytes(source.replace('\n','\r\n').encode('ascii'))
(work/'UITEST.H').write_bytes((root/'tests/UITEST.H').read_text().replace('\n','\r\n').encode('ascii'))
(work/'FAST.CONF').write_text('[cpu]\ncycles=15000\n')
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
def dos(directory,commands):
    args=['dosbox','-conf',str(root/'tools/dosbox.conf'),'-conf',str(work/'FAST.CONF'),'-machine','vgaonly']
    for command in [f'mount c "{root/"buildenv"}"',f'mount d "{work}"',f'mount e "{local}"','d:',*commands,'exit']:
        args+=['-c',command]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=240)
compile_app(work,dos,'SHOTBLD',
    [r'set PATH=C:\TC;C:\TASM',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
     'tasm /mx FONT.ASM > ASSEMBLE.LOG',
     'tcc -DNW_DIAGNOSTICS -1- -mm -O -Z -eUITEST.EXE MAIN.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > COMPILE.LOG'],
    ['UITEST.EXE','FONT.OBJ'],['ASSEMBLE.LOG','COMPILE.LOG'])
def text(value):return [21,*map(ord,value),13]
keys=[20,ord('q'),13]+text('E:\\')+text('42')+[ord('y'),13,0x4400]
(work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys))
dos(work,[r'UITEST /vga D:\CARD D:\CARD'])
assert (card/'NWT0001/DONE.ID').exists()
assert (work/'RUNNING.BIN').exists(),'No running capture'
out=root/'build/screenshots';out.mkdir(exist_ok=True)
for capture,name in [('F001.BIN','storage-menu-vga.png'),('RUNNING.BIN','storage-dashboard-vga.png'),(f'F{len(keys)-2:03d}.BIN','storage-result-vga.png')]:
    decode(work/capture,out/name)
print(out)
