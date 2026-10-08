# SPDX-License-Identifier: GPL-3.0-only
"""Run Windows/Linux ZIP fixtures through the real 16-bit 8086 extractor."""
from pathlib import Path
import os, shutil, subprocess as sp, sys, hashlib, json
from doszip import prepare_zip
from dosbuild import compile_dos
from zip import raw_zip
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/zip-dos'; WORK.mkdir(parents=True,exist_ok=True)
FIX=ROOT/'tests/fixtures/zip'
ENV=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
TC=os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv'))
(WORK/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
def execute(directory,commands,cpu=False,extra=()):
    args=['dosbox-x' if cpu else 'dosbox','-conf',str(ROOT/'tools/dosbox.conf')]
    if cpu: args+=['-conf',str(WORK/'CPU.CONF'),'-fastlaunch','-nogui']
    for command in [f'mount c "{TC}" -ro',f'mount d "{directory}"','d:',*extra,*commands,'exit']:
        args+=['-c',command]
    with (WORK/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=180)
prepare_zip(WORK,execute,flags='-DNW_ZIP_TEST')
for name in ('CORE.C','FSDOS.C'):
    (WORK/name).write_bytes((ROOT/'src'/name).read_text().replace('\n','\r\n').encode('ascii'))
(WORK/'ZIPTEST.C').write_bytes((ROOT/'tests/ZIPTEST.C').read_text().replace('\n','\r\n').encode('ascii'))
compile_dos(WORK,execute,'ZTEST',
            [r'set PATH=C:\TC',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
             'tcc -1- -mm -O -Z -DNW_ZIP_TEST -c ZIPTEST.C CORE.C FSDOS.C > TEST.LOG',
             'tcc -1- -mm -M -eZIPTEST.EXE *.OBJ > LINK.LOG'],
            ['ZIPTEST.EXE'],['TEST.LOG','LINK.LOG'])
manifest=json.loads((FIX/'manifest.json').read_text())
for archive in sorted(FIX.glob('*.ZIP')):
    work=WORK/archive.stem
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'OUT').mkdir()
    shutil.copy2(WORK/'ZIPTEST.EXE',work/'ZIPTEST.EXE'); shutil.copy2(archive,work/'IN.ZIP')
    execute(work,[r'ZIPTEST D:\IN.ZIP D:\OUT > RESULT.TXT'],True)
    result=(work/'RESULT.TXT').read_text()
    assert 'ok=1 files=5 bytes=428803 completed=5' in result,result
    # DOSBox-X stores nondefault DOS attributes as host sidecar metadata.
    actual={p.relative_to(work/'OUT').as_posix():p for p in (work/'OUT').rglob('*')
            if p.is_file() and not p.name.startswith('.DBLOCALFILE_')}
    assert set(actual)==set(manifest),(archive,actual.keys())
    for name,path in actual.items():
        assert hashlib.sha256(path.read_bytes()).hexdigest()==manifest[name]['sha256'],(archive,name)
    assert (work/'OUT/SUB/EMPTY').is_dir()
    print('PASS: DOS 8086',archive.name,result.strip(),flush=True)
# Small synthetic streams exercise every level/strategy in the 16-bit decoder,
# including the fixed tables now stored outside DGROUP.
import zlib
work=WORK/'matrix'; work.mkdir(exist_ok=True)
shutil.copy2(WORK/'ZIPTEST.EXE',work/'ZIPTEST.EXE')
commands=[]; cases=[]
for level in (0,1,6,9):
    for strategy in (0,1,2,3,4):
        name=f'T{level}{strategy}'; dest=work/name
        if dest.exists(): shutil.rmtree(dest)
        dest.mkdir(); data=bytes(range(256))*20+b'abc'*5000
        (work/(name+'.ZIP')).write_bytes(raw_zip(data,level=level,strategy=strategy,descriptor=16))
        commands.append(f'ZIPTEST D:\\{name}.ZIP D:\\{name} > {name}.TXT'); cases.append((name,data))
(work/'MATRIX.BAT').write_bytes(('\r\n'.join(commands)+'\r\n').encode('ascii'))
execute(work,['call MATRIX.BAT'],True)
for name,data in cases:
    result=(work/(name+'.TXT')).read_text(); assert 'ok=1' in result,result
    assert (work/name/'HELLO.TXT').read_bytes()==data,name
print('PASS: DOS 8086 all 20 DEFLATE level/strategy combinations with signed descriptors',flush=True)
# Allocation failure and cancellation retain the original and release far data.
for mode in ('alloc1','alloc2','cancel','skip'):
    dest=work/'FAIL'
    if dest.exists(): shutil.rmtree(dest)
    dest.mkdir(); (dest/'HELLO.TXT').write_bytes(b'original')
    execute(work,[f'ZIPTEST D:\\T60.ZIP D:\\FAIL {mode} > FAIL.TXT'],True)
    result=(work/'FAIL.TXT').read_text()
    assert ('ok=1' if mode=='skip' else 'ok=0') in result,result
    assert (dest/'HELLO.TXT').read_bytes()==b'original'
    assert [p for p in dest.iterdir() if not p.name.startswith('.DBLOCALFILE_')]==[dest/'HELLO.TXT']
print('PASS: DOS far allocation failures, Escape-style cancellation and skipping preserve originals',flush=True)
# Production command-line entry point, not only the test driver.
work=WORK/'cli'; work.mkdir(exist_ok=True)
shutil.copy2(ROOT/'build/NW.EXE',work/'NW.EXE'); shutil.copy2(FIX/'WINPS.ZIP',work/'IN.ZIP')
if (work/'OUT').exists(): shutil.rmtree(work/'OUT')
(work/'OUT').mkdir()
execute(work,[r'NW /unzip IN.ZIP OUT > RESULT.TXT'],True)
assert '5 files unpacked' in (work/'RESULT.TXT').read_text(),(work/'RESULT.TXT').read_text()
for name,info in manifest.items(): assert hashlib.sha256((work/'OUT'/name).read_bytes()).hexdigest()==info['sha256']
# A second invocation refuses overwrites and retains output.
execute(work,[r'NW /unzip D:\IN.ZIP D:\OUT > AGAIN.TXT'],True)
assert 'destination exists' in (work/'AGAIN.TXT').read_text()
print('PASS: production NW /unzip and refusal to overwrite from the command line',flush=True)
