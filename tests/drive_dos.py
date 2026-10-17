# SPDX-License-Identifier: GPL-3.0-only
"""Real DOS drive queries, synthetic OTTERSD identity and picker UI in all modes."""
from pathlib import Path
import os,shutil,subprocess as sp,sys
from dosbuild import compile_dos
from ui_build import require_prebuilt,build_ui
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/drive-dos'
WORK.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
TC=Path(os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv')))
(WORK/'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
(WORK/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
def run(directory,commands,machine='svga_s3',floppies=False,cpu=False):
    args=['dosbox-x' if cpu else 'dosbox']
    if cpu: args+=['-fastlaunch','-nogui']
    args+=['-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(WORK/('CPU.CONF' if cpu else 'FAST.CONF')),'-machine',machine]
    mounts=[f'mount c "{TC}" -ro',f'mount d "{directory}"',f'mount e "{directory / "BACKUP"}"']
    if floppies:
        mounts+=[f'imgmount a "{WORK / "A.IMG"}" -t floppy -fs fat',f'imgmount b "{WORK / "B.IMG"}" -t floppy -fs fat']
    for command in [*mounts,'d:',*commands,'exit']: args+=['-c',command]
    with (directory/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=90)
for name in ('DRIVE.C','DRIVE.H','DRIVEDOS.C','NW.H','VERSION.H','FONT.ASM'):
    (WORK/name).write_bytes((ROOT/'src'/name).read_text().replace('\n','\r\n').encode('ascii'))
for name in ('DRVTEST.C',):
    (WORK/name).write_bytes((ROOT/'tests'/name).read_text().replace('\n','\r\n').encode('ascii'))
(WORK/'DRVHOOK.ASM').write_bytes((ROOT/'tools/DRVMETA.ASM').read_text().replace('\n','\r\n').encode('ascii'))
(WORK/'BACKUP').mkdir(exist_ok=True)
compile_dos(WORK,run,'DRBUILD',
    [r'set PATH=C:\TC;C:\TASM',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
     'tasm /mx FONT.ASM > FONT.LOG','tasm /mx DRVHOOK.ASM > HOOK.LOG',
     'tcc -1- -mm -O -Z -eDRVTEST.EXE DRVTEST.C DRIVE.C DRIVEDOS.C FONT.OBJ DRVHOOK.OBJ > COMPILE.LOG'],
    ['DRVTEST.EXE'],['FONT.LOG','HOOK.LOG','COMPILE.LOG'])
for filename,size in [('A.IMG','1440'),('B.IMG','360')]:
    (WORK/filename).unlink(missing_ok=True)
    sp.run(['mformat','-C','-i',str(WORK/filename),'-f',size,'::'],check=True)
run(WORK,['DRVTEST > RESULT.TXT'],floppies=True)
result=(WORK/'RESULT.TXT').read_text()
assert 'PASS:' in result,result
print(result,flush=True)
run(WORK,['DRVTEST > CPU.TXT'],floppies=True,cpu=True)
assert 'PASS:' in (WORK/'CPU.TXT').read_text(),(WORK/'CPU.TXT').read_text()
print('PASS: production drive queries and interrupt handlers on an 8086 CPU',flush=True)
try: require_prebuilt()
except (AssertionError,FileNotFoundError): build_ui()
sys.path.insert(0,str(ROOT/'tools'))
from vram import decode
for machine,mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga'),('hercules','text')]:
    work=WORK/(machine+'-'+mode)
    if work.exists(): shutil.rmtree(work)
    (work/'LEFT').mkdir(parents=True); (work/'RIGHT').mkdir(); (work/'BACKUP').mkdir()
    shutil.copy2(ROOT/'build/video/UITEST.EXE',work/'UITEST.EXE')
    shutil.copy2(WORK/'DRVTEST.EXE',work/'DRVTEST.EXE')
    (work/'LEFT/FILE.TXT').write_bytes(b'Picker test\r\n')
    (work/'BACKUP/NOTE.TXT').write_bytes(b'Backup drive\r\n')
    keys=[]; frames={}
    def press(*values): keys.extend(values)
    def frame(name): frames[name]=len(keys)
    def text(value): press(21,*map(ord,value),13)
    press(0x5000,0x5200); frame('marked')
    press(0x4300); frame('picker')
    press(27); frame('cancelled')
    press(0x4300,ord('e'),13); frame('drive-e')
    press(0x4300,0x4f00,13); frame('path-prompt')
    text('D:\\LEFT'); frame('typed-path')
    press(0x4300,ord('.')); text('Y:\\MISSING'); frame('unavailable')
    press(13); frame('failed-preserved')
    press(0x4300,ord('.'),27); frame('path-cancelled')
    press(9,0x4300,ord('e'),13); frame('right-e')
    press(0x4400)
    (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys)+'\n')
    run(work,[f'DRVTEST UITEST.EXE /{mode} D:\\LEFT D:\\RIGHT'],machine,True)
    assert len(list(work.glob('F*.BIN')))==len(keys),(mode,'script stopped early')
    def info(label): return list(map(int,(work/f'F{frames[label]:03}.POS').read_text().split()))
    assert (work/f'F{frames["marked"]:03}.BIN').read_bytes()==(work/f'F{frames["cancelled"]:03}.BIN').read_bytes(),mode
    assert info('marked')[10]==info('cancelled')[10]==1,mode
    # Settings save is not needed: text-mode pane captions provide path evidence.
    if mode=='text':
        def screen(label): return (work/f'F{frames[label]:03}.BIN').read_bytes()[3:4003:2].decode('cp437')
        picker=screen('picker')
        assert '3.5-inch floppy' in picker and '5.25-inch floppy' in picker,picker
        assert 'Slot-otter SD' in picker and 'Fixed disk' in picker,picker
        assert 'E:\\' in screen('drive-e')[160:200] and 'D:\\RIGHT' in screen('drive-e')[200:240]
        assert 'D:\\LEFT' in screen('typed-path')[160:200]
        assert 'D:\\LEFT' in screen('failed-preserved')[160:200]
        assert 'D:\\LEFT' in screen('right-e')[160:200] and 'E:\\' in screen('right-e')[200:240]
    for label in ('picker','drive-e','path-prompt','unavailable'):
        decode(work/f'F{frames[label]:03}.BIN',work/(label+'.png'))
    print('PASS:',machine,mode,'drive types, SD identity, Escape/marks, active pane, path entry and failed selection',flush=True)
