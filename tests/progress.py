# SPDX-License-Identifier: GPL-3.0-only
"""Check differential copy-dialog output and prompt recovery on real VRAM."""
from pathlib import Path
import os
import subprocess as sp
import sys
from dosbuild import compile_dos

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root/'tools'))
from vram import decode, palette
work = root/'build/progress'
work.mkdir(parents=True, exist_ok=True)
toolchain = os.environ.get('DOS_TOOLCHAIN', str(root/'buildenv'))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
for source in (root/'src').iterdir():
    if source.suffix in ('.C', '.H', '.ASM'):
        (work/source.name).write_bytes(source.read_text().replace('\n', '\r\n').encode('ascii'))
(work/'UITEST.H').write_bytes((root/'tests/UITEST.H').read_text().replace('\n', '\r\n').encode('ascii'))
(work/'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
program = r'''
#define main navigator_main
#include "MAIN.C"
#undef main
#include "UITEST.H"
int main(int argc, char **argv)
{
    unsigned long before;
    (void)argc;
    video_init(); assert(video_set(atoi(argv[1])));
    begin_operation(); copy_progress=1; total_files=2; total_bytes=2000;
    operation_current_bytes=operation_transferred=250;
    assert(operation_ui("D:\\LONG_DIRECTORY\\FIRST_FILE.BIN",250,1000)); test_capture();
    before=video_bytes; force_progress=1; sample_tick=(unsigned long)biostime(0,0L);
    assert(operation_ui("D:\\LONG_DIRECTORY\\FIRST_FILE.BIN",250,1000));
    assert(video_bytes==before);
    operation_current_bytes=operation_transferred=500; force_progress=1;
    assert(operation_ui("D:\\LONG_DIRECTORY\\FIRST_FILE.BIN",500,1000)); test_capture();
    /* Shorter text and a fresh current file, while overall progress advances. */
    operation_files=1; operation_bytes=1000;
    operation_current_bytes=0; operation_transferred=1000; force_progress=1;
    assert(operation_ui("D:\\B.BIN",0,500)); test_capture();
    operation_current_bytes=250; operation_transferred=1250; force_progress=1;
    assert(operation_ui("D:\\B.BIN",250,500)); test_capture();
    /* Compare an incremental update with a complete dialog rebuild. */
    copy_dialog=0; force_progress=1;
    assert(operation_ui("D:\\B.BIN",250,500)); test_capture();
    /* The conflict prompt covers both tracks; resume must restore them. */
    pending_head=0; pending_count=1; pending_keys[0]='s';
    assert(conflict_ui("D:\\B.BIN","D:\\TARGET.BIN")==2);
    assert(operation_ui("D:\\B.BIN",250,500)); test_capture();
    before=video_bytes; force_progress=1; sample_tick=(unsigned long)biostime(0,0L);
    assert(operation_ui("D:\\B.BIN",250,500)); assert(video_bytes==before);
    end_operation(); video_restore(); puts("PASS: differential copy dialog"); return 0;
}
'''
(work/'PROGRESS.C').write_bytes(program.replace('\n', '\r\n').encode('ascii'))


def execute(directory, commands, machine='vgaonly'):
    args = ['dosbox', '-conf', str(root/'tools/dosbox.conf'),
            '-conf', str(work/'FAST.CONF'), '-machine', machine]
    for command in [f'mount c "{toolchain}"', f'mount d "{directory}"', 'd:', *commands, 'exit']:
        args += ['-c', command]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args, env=env, stdout=log, stderr=log, check=True, timeout=60)


compile_dos(work, execute, 'PRBUILD',
            [r'set PATH=C:\TC;C:\TASM', r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
             'tasm /mx FONT.ASM > ASSEMBLE.LOG',
             'tcc -DNW_DIAGNOSTICS -1- -ms -O -Z -ePRGR.EXE PROGRESS.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > COMPILE.LOG'],
            ['PRGR.EXE', 'FONT.OBJ'], ['ASSEMBLE.LOG', 'COMPILE.LOG'])
for machine, mode in [('cga', 0), ('cga', 1), ('ega', 2), ('vgaonly', 3)]:
    for pattern in ('F*.BIN', 'F*.POS', 'RESULT.LOG'):
        for capture in work.glob(pattern):
            capture.unlink()
    execute(work, [f'PRGR {mode} > RESULT.LOG'], machine)
    assert 'PASS: differential copy dialog' in (work/'RESULT.LOG').read_text(), (work/'RESULT.LOG').read_text()
    frames = [(work/f'F{i:03d}.BIN').read_bytes() for i in range(6)]
    assert frames[3] == frames[4] == frames[5], 'Incremental, rebuilt and post-prompt output differ'
    if mode:
        image = decode(work/'F000.BIN')
        pitch = {1:8, 2:14, 3:16}[mode]
        top = (25 // 2 - 5) * pitch if mode != 3 else (30 // 2 - 5) * pitch
        left, right, bottom = 24, 616, top + 11*pitch
        # Every outermost pixel of the filled dialog must be its black frame:
        # a centered text-cell stroke leaves a grey/white rim outside it.
        for box in ((left,top,right,top+1), (left,bottom-1,right,bottom),
                    (left,top,left+1,bottom), (right-1,top,right,bottom)):
            edge = image.crop(box)
            assert edge.getcolors() == [(edge.width*edge.height,palette[0])], (mode,'dialog rim',box)
    print(f'PASS: mode {mode} no-op progress writes zero VRAM; file reset, shorter path and prompt restoration match full redraw', flush=True)
