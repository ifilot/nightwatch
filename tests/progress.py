# SPDX-License-Identifier: GPL-3.0-only
"""Check differential copy-dialog output and prompt recovery on real VRAM."""
from pathlib import Path
import os
import subprocess as sp
import sys
from dosbuild import compile_dos
from doszip import compile_app

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
#define bioskey test_bioskey
#define biostime test_biostime
#define main navigator_main
#include "MAIN.C"
#undef main
#undef bioskey
#undef biostime
#include "UITEST.H"
#undef assert
#define assert(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static unsigned long test_tick=100;
static unsigned bios_keys[16], bios_count, bios_at;
long test_biostime(int command, long value)
{
    (void)command; (void)value; return test_tick;
}
int test_bioskey(int command)
{
    if (command) return bios_at < bios_count ? bios_keys[bios_at] : 0;
    assert(bios_at < bios_count); return bios_keys[bios_at++];
}
int main(int argc, char **argv)
{
    unsigned long before;
    (void)argc;
    video_init(); assert(video_set(atoi(argv[1])));
    begin_operation(); copy_progress=1; total_files=2; total_bytes=2000;
    operation_current_bytes=operation_transferred=250;
    assert(operation_ui("D:\\LONG_DIRECTORY\\FIRST_FILE.BIN",250,1000)); test_capture();
    before=video_bytes; force_progress=1; sample_tick=test_tick;
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
    /* Deferred O/navigation keys cannot answer a later overwrite prompt.
     * Fresh input skips, and deferred pane commands remain in order. */
    pending_head=0; pending_count=2; pending_keys[0]='o'; pending_keys[1]=0x5000;
    bios_at=0; bios_count=1; bios_keys[0]='s';
    assert(conflict_ui("D:\\B.BIN","D:\\TARGET.BIN")==2);
    assert(pending_count==2 && key_read()=='o' && key_read()==0x5000);
    assert(operation_ui("D:\\B.BIN",250,500)); test_capture();
    before=video_bytes; force_progress=1; sample_tick=test_tick;
    assert(operation_ui("D:\\B.BIN",250,500)); assert(video_bytes==before);
    /* Successive tiny files within one BIOS tick must not repaint for
     * each path/count change. The next timed paint uses a coherent snapshot. */
    before=video_bytes;
    total_files=5;
    operation_files=2; operation_bytes=1500; operation_current_bytes=0;
    assert(operation_ui("D:\\SMALL.C",0,100)); assert(video_bytes==before);
    operation_files=3; operation_bytes=1600; operation_current_bytes=50;
    assert(operation_ui("D:\\SMALL.D",50,100)); assert(video_bytes==before);
    test_tick+=4;
    assert(operation_ui("D:\\SMALL.D",50,100));
    assert(video_bytes>before && !strcmp(progress_path,"D:\\SMALL.D"));
    if(video_mode!=VIDEO_TEXT) assert(file_percent==50);
    test_capture();
    /* Forced completion bypasses throttling, even in the same tick. */
    before=video_bytes; operation_current_bytes=100; force_progress=1;
    assert(operation_ui("D:\\SMALL.D",100,100));
    assert(video_bytes>before);
    if(video_mode!=VIDEO_TEXT) assert(file_percent==100);
    /* Escape polling still runs during suppressed paints and retains keys. */
    bios_at=0; bios_count=2; bios_keys[0]=0x4800; bios_keys[1]=27;
    assert(!operation_ui("D:\\SMALL.E",0,10));
    assert(pending_count==1 && key_read()==0x4800);
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


compile_app(work, execute, 'PRBUILD',
            [r'set PATH=C:\TC;C:\TASM', r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
             'tasm /mx FONT.ASM > ASSEMBLE.LOG',
             'tcc -DNW_DIAGNOSTICS -1- -mm -O -Z -ePRGR.EXE PROGRESS.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > COMPILE.LOG'],
            ['PRGR.EXE', 'FONT.OBJ'], ['ASSEMBLE.LOG', 'COMPILE.LOG'])
for machine, mode in [('cga', 0), ('cga', 1), ('ega', 2), ('vgaonly', 3)]:
    for pattern in ('F*.BIN', 'F*.POS', 'RESULT.LOG'):
        for capture in work.glob(pattern):
            capture.unlink()
    execute(work, [f'PRGR {mode} > RESULT.LOG'], machine)
    assert 'PASS: differential copy dialog' in (work/'RESULT.LOG').read_text(), (work/'RESULT.LOG').read_text()
    frames = [(work/f'F{i:03d}.BIN').read_bytes() for i in range(6)]
    assert frames[3] == frames[4] == frames[5], 'Incremental, rebuilt and post-prompt output differ'
    if not mode:
        snapshot = (work/'F006.BIN').read_bytes()[3:4003:2].decode('cp437')
        assert 'D:\\SMALL.D' in snapshot and '50%' in snapshot and 'Files: 3 / 5' in snapshot
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
