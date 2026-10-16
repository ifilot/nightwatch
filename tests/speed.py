# SPDX-License-Identifier: GPL-3.0-only
"""Compare preserved and current 8086 builds with identical heavy workloads.

Before sources must be saved in build/speed-before/src. No timing claims about
physical hardware: DOSBox-X uses fixed 3,000 cycles and BIOS ticks.
"""
from pathlib import Path
import os
import subprocess as sp
import re
import sys
from dosbuild import compile_dos
from doszip import compile_app
root = Path(__file__).resolve().parents[1]
toolchain = Path(os.environ.get('DOS_TOOLCHAIN',str(root / 'buildenv')))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
bench = r'''
static int speed_key_read(void);
#define main navigator_main
#include "MAIN.C"
#undef main
static int keys_left;
static int speed_key_read(void) { return keys_left-- > 0 ? '?' : 27; }
static unsigned long ticks(void) {
    unsigned long far *p = (unsigned long far *)MK_FP(0x40,0x6c);
    unsigned long n; disable(); n = *p; enable(); return n;
}
int main(int argc, char **argv) {
    int mode = atoi(argv[1]), kind = atoi(argv[2]), i, p, rows;
    unsigned long a,b,w;
    for(p=0;p<2;++p) {
        strcpy(panes[p].path,"D:\\LEFT");
        panes[p].count = 512;
        for(i=0;i<512;++i) {
            sprintf(panes[p].files[i].name,"FILE%04d.BIN",i);
            panes[p].files[i].size = 76803L;
        }
    }
    video_init(); video_set(mode); draw(); rows=pane_rows();
    if(kind==1) { panes[0].cursor=rows-1; draw(); }
    if(kind==3) {
        strcpy(panes[0].files[0].name,"VIEW.BIN"); panes[0].cursor=0; keys_left=5;
    }
    w=video_bytes; a=ticks();
    if(kind==0) for(i=0;i<40;++i) { panes[0].cursor=2+(i&1); draw(); }
    if(kind==1) for(i=0;i<20;++i) { ++panes[0].cursor; draw(); }
    if(kind==2) for(i=0;i<20;++i) { command[i]='W'; command[i+1]=0; draw(); }
    if(kind==3) viewer(1);
    if(kind==4) for(i=0;i<20;++i) draw();
    b=ticks(); video_restore();
    printf("mode=%d kind=%d ticks=%lu stores=%lu\n",mode,kind,b-a,video_bytes-w);
    return 0;
}
'''
results={}
scroll_only = '--scroll-only' in sys.argv
if scroll_only:
    for label in ('before','after'):
        for mode in range(4):
            for kind in range(5):
                log=(root/'build'/('speed-'+label+'-bench')/f'M{mode}K{kind}.LOG').read_text()
                numbers=re.search(rf'mode={mode} kind={kind} ticks=(\d+) stores=(\d+)',log)
                assert numbers,log
                results[label,mode,kind]=tuple(map(int,numbers.groups()))
for label in (('no-scroll',) if scroll_only else ('before','after','no-scroll')):
    sources = root/'build/speed-before/src' if label=='before' else root/'src'
    if not sources.exists(): raise SystemExit('Save baseline source in build/speed-before/src first')
    work=root/'build'/('speed-'+label+'-bench'); work.mkdir(exist_ok=True)
    (work/'LEFT').mkdir(exist_ok=True)
    (work/'LEFT/VIEW.BIN').write_bytes(bytes(range(256))*300+b'ABC')
    for source in sources.iterdir():
        if source.suffix not in {'.C','.H','.ASM'}: continue
        text=source.read_text()
        if source.name=='MAIN.C':
            start=text.index('static int key_read(void)'); end=text.index('static int pane_rows',start)
            text=text[:start]+('static int key_read(void) { return speed_key_read(); }\n'
                              'static int modal_key_read(void) { return speed_key_read(); }\n')+text[end:]
        (work/source.name).write_bytes(text.replace('\n','\r\n').encode('ascii'))
    (work/'SPEED.C').write_bytes(bench.replace('\n','\r\n').encode('ascii'))
    (work/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
    def run(commands, xt=False):
        args=['dosbox-x','-fastlaunch','-nogui'] if xt else ['dosbox']
        args+=['-conf',str(root/'tools/dosbox.conf'),'-conf',str(work/'CPU.CONF')]
        for command in [f'mount c "{toolchain}"',f'mount d "{work}"','d:']+commands+['exit']:
            args+=['-c',command]
        with (work/'DOSBOX.LOG').open('w') as log:
            sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=55)
    flags='-DNW_DIAGNOSTICS '+ ('-DNW_NO_SCROLL ' if label=='no-scroll' else '')
    # Compile separately: DOS command tails have a 126-character limit.
    commands=[r'set PATH=C:\TC;C:\TASM','tasm /mx FONT.ASM > ASSEMBLE.LOG']
    modules=('SPEED','CORE','FSDOS','VIDEO','GRAPH','HEX') + (('VIEW',) if (sources/'VIEW.C').exists() else ())
    for module in modules:
        commands.append(rf'tcc {flags}-1- -mm -O -Z -IC:\TC\INCLUDE -c {module}.C > C{module}.LOG')
    commands.append(r'tcc -mm -LC:\TC\LIB -eSPEED.EXE SPEED.OBJ CORE.OBJ FSDOS.OBJ VIDEO.OBJ GRAPH.OBJ HEX.OBJ ' + ('VIEW.OBJ ' if 'VIEW' in modules else '') + 'FONT.OBJ > LINK.LOG')
    compile_app(work, lambda directory, cmds: run(cmds), 'SPBUILD', commands,
                ['SPEED.EXE','FONT.OBJ'], ['ASSEMBLE.LOG','LINK.LOG']+[f'C{m}.LOG' for m in modules])
    for mode in range(4):
        for kind in range(5):
            if label=='no-scroll' and (kind!=1 or mode==0): continue
            name=f'M{mode}K{kind}.LOG'
            run([f'SPEED {mode} {kind} > {name}'],True)
            result=(work/name).read_text().strip()
            numbers=re.search(rf'mode={mode} kind={kind} ticks=(\d+) stores=(\d+)',result)
            assert numbers,result
            results[label,mode,kind]=tuple(map(int,numbers.groups()))
            print(label,result,flush=True)
for mode in range(4):
    for kind in range(5):
        old,new=results['before',mode,kind],results['after',mode,kind]
        # BIOS clock phase permits one tick of noise.
        assert new[0]<=old[0]+1,(mode,kind,'speed regression',old,new)
        if kind==4: assert new[1]==0,(mode,'idle writes')
for mode in (1,2,3):
    copy,redraw=results['after',mode,1],results['no-scroll',mode,1]
    assert copy[0]<=redraw[0]+1,(mode,'viewport copy slower',copy,redraw)
print('PASS: identical 512-entry workloads; motion, scrolling, typing, viewer, idle; viewport copy versus redraw')
