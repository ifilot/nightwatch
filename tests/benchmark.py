# SPDX-License-Identifier: GPL-3.0-only
"""Measure rendering on the DOSBox-X 8086 core at fixed cycles."""
from pathlib import Path
import subprocess as sp
import os, sys, shutil, re
from dosbuild import compile_dos
root = Path(__file__).resolve().parents[1]
label = sys.argv[1] if len(sys.argv) > 1 else 'new'
work = root / 'build' / ('bench-' + label)
work.mkdir(exist_ok=True)
for source in (root / 'src').iterdir():
    if source.suffix in {'.C', '.H', '.ASM'}:
        (work/source.name).write_bytes(source.read_text().replace('\n','\r\n').encode('ascii'))
for name in ['LEFT','RIGHT']:
    (work/name).mkdir(exist_ok=True)
    (work/name/'DOCS').mkdir(exist_ok=True)
    for i in range(12): (work/name/f'FILE{i:02d}.TXT').write_bytes(b'example\r\n')
source = ('#define NW_BENCH_NEW\n' if (work/'GRAPH.C').exists() else '') + r'''
#define main navigator_main
#include "MAIN.C"
#undef main
static unsigned long ticks(void) {
    unsigned long far *clock = (unsigned long far *)MK_FP(0x40, 0x6c);
    unsigned long t;
    disable(); t = *clock; enable(); return t;
}
int main(int argc, char **argv) {
    unsigned long a, b, c, d;
    int mode = atoi(argv[1]), i;
#ifdef NW_BENCH_NEW
    unsigned long w0, w1, w2, w3;
#endif
    strcpy(panes[0].path, "D:\\LEFT"); strcpy(panes[1].path, "D:\\RIGHT");
    panel_load(&panes[0]); panel_load(&panes[1]);
    video_init(); video_set(mode);
#ifdef NW_BENCH_NEW
    w0 = video_bytes;
#endif
    a = ticks(); draw(); b = ticks();
#ifdef NW_BENCH_NEW
    w1 = video_bytes;
#endif
    for(i = 0; i < 10; ++i) { panes[0].cursor = 2 + (i & 1); draw(); }
    c = ticks();
#ifdef NW_BENCH_NEW
    w2 = video_bytes;
#endif
    for(i = 0; i < 10; ++i) draw();
    d = ticks();
#ifdef NW_BENCH_NEW
    w3 = video_bytes;
#endif
    video_restore();
    printf("mode=%d first=%lu motion10=%lu idle10=%lu ticks\n", mode, b-a, c-b, d-c);
#ifdef NW_BENCH_NEW
    printf("bytes: first=%lu motion10=%lu idle10=%lu\n", w1-w0, w2-w1, w3-w2);
    if (w3 != w2 || w2-w1 >= w1-w0) { puts("FAIL: damage tracking regression"); return 1; }
#endif
    return 0;
}
'''
(work/'BENCH.C').write_bytes(source.replace('\n','\r\n').encode('ascii'))
tc=os.environ.get('DOS_TOOLCHAIN',str(root / 'buildenv'))
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
def execute(exe, commands, extra=()):
    args=[exe,'-conf',str(root/'tools/dosbox.conf')]+list(extra)
    for cmd in [f'mount c "{tc}"',f'mount d "{work}"','d:']+commands+['exit']: args+=['-c',cmd]
    with (work/'DOSBOX.LOG').open('w') as log:sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=55)
modules = ['BENCH','CORE','FSDOS','VIDEO']
if (work/'GRAPH.C').exists(): modules += ['GRAPH','HEX']
if (work/'VIEW.C').exists(): modules += ['VIEW']
commands = [r'set PATH=C:\TC;C:\TASM', 'tasm /mx FONT.ASM > ASSEMBLE.LOG',
            r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB']
for module in modules:
    commands.append(f'tcc -DNW_DIAGNOSTICS -1- -ms -O -Z -c {module}.C > C{module}.LOG')
commands.append('tcc -ms -eBENCH.EXE '+' '.join(m+'.OBJ' for m in modules)+' FONT.OBJ > LINK.LOG')
compile_dos(work, lambda directory, cmds: execute('dosbox',cmds), 'BNBUILD', commands,
            ['BENCH.EXE','FONT.OBJ'], ['ASSEMBLE.LOG','LINK.LOG']+[f'C{m}.LOG' for m in modules])
(work/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
for mode in (1,2,3):
    execute('dosbox-x',[f'BENCH {mode} > TIME{mode}.LOG'],['-fastlaunch','-nogui','-conf',str(work/'CPU.CONF')])
    result = (work/f'TIME{mode}.LOG').read_text().strip()
    timing = re.search(r'mode=(\d+) first=(\d+) motion10=(\d+) idle10=(\d+) ticks', result)
    stores = re.search(r'bytes: first=(\d+) motion10=(\d+) idle10=(\d+)', result)
    assert timing and int(timing[1]) == mode and 'FAIL' not in result, result
    if (work/'GRAPH.C').exists():
        assert stores, result
        first, motion, idle = map(int, stores.groups())
        assert first > motion > 0 and idle == 0, result
    print(label,result,flush=True)
