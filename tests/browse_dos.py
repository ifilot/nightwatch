# SPDX-License-Identifier: GPL-3.0-only
"""Archive browsing on Turbo C/8086 and the actual navigator in every mode."""
from pathlib import Path
import hashlib,json,os,shutil,subprocess as sp,sys
from doszip import prepare_zip
from dosbuild import compile_dos
from ui_build import require_prebuilt,build_ui
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/browse-dos'; WORK.mkdir(parents=True,exist_ok=True)
ENV=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
TC=os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv'))
(WORK/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
def execute(directory,commands,cpu=False,extra=()):
    args=['dosbox-x' if cpu else 'dosbox','-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(WORK/'CPU.CONF')]
    if cpu: args+=['-fastlaunch','-nogui']
    for command in [f'mount c "{TC}" -ro',f'mount d "{directory}"','d:',*extra,*commands,'exit']:
        args+=['-c',command]
    with (directory/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=120)
prepare_zip(WORK,execute)
for name,source in [('CORE.C',ROOT/'src/CORE.C'),('FSDOS.C',ROOT/'src/FSDOS.C'),('BROWTEST.C',ROOT/'tests/BROWTEST.C')]:
    (WORK/name).write_bytes(source.read_text().replace('\n','\r\n').encode('ascii'))
compile_dos(WORK,execute,'BTEST',
            [r'set PATH=C:\TC',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
             'tcc -1- -mm -O -Z -c BROWTEST.C CORE.C FSDOS.C > TEST.LOG',
             'tcc -1- -mm -M -eBROWTEST.EXE *.OBJ > LINK.LOG'],['BROWTEST.EXE'],['TEST.LOG','LINK.LOG'])
fixtures=[ROOT/'tests/fixtures/zip/WINBEST.ZIP',ROOT/'tests/fixtures/arj/METHOD1.ARJ',ROOT/'tests/fixtures/arj/METHOD4.ARJ']
for fixture in fixtures:
    work=WORK/fixture.stem
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'OUT').mkdir()
    shutil.copy2(WORK/'BROWTEST.EXE',work/'BROWTEST.EXE'); shutil.copy2(fixture,work/('IN'+fixture.suffix))
    member='BYTES.BIN' if fixture.suffix=='.ZIP' else 'DATA.BIN'
    execute(work,[f'BROWTEST D:\\IN{fixture.suffix} D:\\VIEW.TMP SUB select={member} view > VIEW.TXT',
                  f'BROWTEST D:\\IN{fixture.suffix} D:\\OUT select=SUB copy > COPY.TXT',
                  f'BROWTEST D:\\IN{fixture.suffix} D:\\OUT SUB .. .. > EXIT.TXT'],True)
    for result in ('VIEW.TXT','COPY.TXT','EXIT.TXT'): assert 'ok=1' in (work/result).read_text(),(fixture,result,(work/result).read_text())
    manifest=json.loads((fixture.parent/'manifest.json').read_text())
    assert hashlib.sha256((work/'VIEW.TMP').read_bytes()).hexdigest()==manifest['SUB/'+member]['sha256']
    for name,info in manifest.items():
        if name.startswith('SUB/'): assert hashlib.sha256((work/'OUT'/name).read_bytes()).hexdigest()==info['sha256'],name
    assert 'active=0' in (work/'EXIT.TXT').read_text()
    print('PASS: 8086',fixture.name,'browse, >64 KiB view, subtree extraction and parent exit',flush=True)

try: require_prebuilt()
except (AssertionError,FileNotFoundError): build_ui()
sys.path.insert(0,str(ROOT/'tools'))
from vram import decode
for mode,fixture in [('vga',fixtures[0]),('vga',fixtures[1]),('text',fixtures[0]),('cga',fixtures[0]),('ega',fixtures[0])]:
    work=WORK/(mode+fixture.suffix.lower().replace('.','-'))
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(ROOT/'build/video/UITEST.EXE',work/'UITEST.EXE')
    name='PACK'+fixture.suffix
    shutil.copy2(fixture,work/'LEFT'/name)
    before=hashlib.sha256(fixture.read_bytes()).hexdigest()
    member='BYTES.BIN' if fixture.suffix=='.ZIP' else 'DATA.BIN'
    keys=[]; frames={}
    def press(*values): keys.extend(values)
    def text(value): press(21,*map(ord,value),13)
    def find(value): press(6); text(value)
    def frame(name): frames[name]=len(keys)
    press(0x5000,13); frame('root')
    find('SUB'); press(13); frame('subfolder')
    find(member); press(0x5600); frame('viewer')
    press(0x3e00); frame('text-viewer'); press(27)
    press(0x3f00,13); frame('extracted-file')
    for key in (0x3e00,0x4000,0x4100,0x4200,20,15): press(key,13)
    press(23,8); frame('root-again')
    press(0x3f00,13); frame('extracted-tree')
    press(8); frame('closed')
    press(13,0x4300,ord('.')); text('D:\\RIGHT'); frame('physical-path')
    press(0x4400)
    (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys)+'\n')
    execute(work,[f'UITEST /{mode} D:\\LEFT D:\\RIGHT'])
    assert len(list(work.glob('F*.BIN')))==len(keys),(mode,'script failed')
    manifest=json.loads((fixture.parent/'manifest.json').read_text())
    assert hashlib.sha256((work/'RIGHT'/member).read_bytes()).hexdigest()==manifest['SUB/'+member]['sha256']
    for path,info in manifest.items():
        if path.startswith('SUB/'): assert hashlib.sha256((work/'RIGHT'/path).read_bytes()).hexdigest()==info['sha256'],path
    assert hashlib.sha256((work/'LEFT'/name).read_bytes()).hexdigest()==before
    assert not list(work.glob('*.TMP')) and not list((work/'RIGHT').rglob('*.TMP'))
    assert (work/'NIGHT.CFG').read_text().splitlines()[2:4]==['D:\\LEFT','D:\\RIGHT']
    closed=list(map(int,(work/f'F{frames["closed"]:03}.POS').read_text().split()))
    assert closed[4]==1,(mode,'archive filename was not reselected')
    if mode=='text':
        payload=(WORK/fixture.stem/'VIEW.TMP').read_bytes()
        lines=payload.count(b'\n')+int(bool(payload) and not payload.endswith(b'\n'))
        header=(work/f'F{frames["text-viewer"]:03}.BIN').read_bytes()[3:163:2].decode('ascii')
        assert f'Line 1 / {lines}' in header,header
    for label,index in frames.items(): decode(work/f'F{index:03}.BIN',work/(label+'.png'))
    print('PASS:',mode,fixture.suffix,'Enter, nested directories, viewer cleanup, F5 file/subtree, read-only guards, Ctrl-W, F9 and Backspace',flush=True)
