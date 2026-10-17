# SPDX-License-Identifier: GPL-3.0-only
"""Verify ARJ methods on the actual Turbo C / 8086 ABI and DOS FAT12."""
from pathlib import Path
import hashlib, json, os, random, shutil, subprocess as sp
from doszip import prepare_zip
from dosbuild import compile_dos
from arj import archive, header, bits, mutate_header

ROOT = Path(__file__).resolve().parents[1]
WORK = ROOT/'build/arj-dos'; WORK.mkdir(parents=True,exist_ok=True)
ENV = dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',TZ='UTC')
TC = os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv'))
(WORK/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')

def execute(directory, commands, cpu=False, extra=()):
    args = ['dosbox-x' if cpu else 'dosbox','-conf',str(ROOT/'tools/dosbox.conf')]
    if cpu: args += ['-conf',str(WORK/'CPU.CONF'),'-fastlaunch','-nogui']
    for cmd in [f'mount c "{TC}" -ro',f'mount d "{directory}"','d:',*extra,*commands,'exit']:
        args += ['-c',cmd]
    with (WORK/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=180)

prepare_zip(WORK,execute,flags='-DNW_ARJ_TEST')
for name,source in [('CORE.C',ROOT/'src/CORE.C'),('FSDOS.C',ROOT/'src/FSDOS.C'),('ARJTEST.C',ROOT/'tests/ZIPTEST.C')]:
    (WORK/name).write_bytes(source.read_text().replace('\n','\r\n').encode('ascii'))
compile_dos(WORK,execute,'ATEST',
            [r'set PATH=C:\TC',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
             'tcc -1- -mm -O -Z -DNW_ARJ_TEST -c ARJTEST.C CORE.C FSDOS.C > TEST.LOG',
             'tcc -1- -mm -M -eARJTEST.EXE *.OBJ > LINK.LOG'],
            ['ARJTEST.EXE'],['TEST.LOG','LINK.LOG'])
manifest = json.loads((ROOT/'tests/fixtures/arj/manifest.json').read_text())
for method in range(5):
    work = WORK/f'M{method}'
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'OUT').mkdir()
    shutil.copy2(WORK/'ARJTEST.EXE',work/'ARJTEST.EXE')
    shutil.copy2(ROOT/f'tests/fixtures/arj/METHOD{method}.ARJ',work/'IN.ARJ')
    execute(work,[r'ARJTEST D:\IN.ARJ D:\OUT > RESULT.TXT'],True)
    result = (work/'RESULT.TXT').read_text()
    assert 'ok=1 files=4 bytes=197406 completed=4' in result,result
    actual = {p.relative_to(work/'OUT').as_posix() for p in (work/'OUT').rglob('*') if p.is_file()}
    assert actual == set(manifest),actual
    for name,info in manifest.items():
        assert hashlib.sha256((work/'OUT'/name).read_bytes()).hexdigest() == info['sha256'],name
    assert (work/'OUT/SUB/EMPTY').is_dir()
    print(f'PASS: 8086 ARJ method {method}, >64 KiB outputs and directory tree',flush=True)

work = WORK/'WINDOW'
if work.exists(): shutil.rmtree(work)
work.mkdir(); (work/'OUT').mkdir()
shutil.copy2(WORK/'ARJTEST.EXE',work/'ARJTEST.EXE')
shutil.copy2(ROOT/'tests/fixtures/arj/WINDOW.ARJ',work/'IN.ARJ')
execute(work,[r'ARJTEST D:\IN.ARJ D:\OUT > RESULT.TXT'],True)
assert 'ok=1' in (work/'RESULT.TXT').read_text()
assert (work/'OUT/HELLO.TXT').read_bytes() == random.Random(26624).randbytes(26000)*3
print('PASS: 8086 ARJ 26,000-byte history distance and ring wraparound',flush=True)

# Check 16-bit bounds, singleton codebooks, allocation failure and cancellation.
work = WORK/'FAULTS'; work.mkdir(exist_ok=True)
(work/'OUT').mkdir(exist_ok=True)
shutil.copy2(WORK/'ARJTEST.EXE',work/'ARJTEST.EXE')
payload = bits((1000,16),(0,5),(0,5),(0,9),(65,9),(0,5),(0,5))
valid = archive(header(data=b'A'*1000,packed=payload,method=1))
for name,blob,mode,ok in [('SINGLE',valid,'',True),('ALLOC',valid,'alloc1',False),
                          ('CANCEL',valid,'cancel',False),
                          ('CRC',mutate_header(valid,1,20,b'\0'*4),'',False),
                          ('BROKEN',archive(header(data=b'abc',method=1,packed=b'\0\0')),'',False)]:
    (work/'IN.ARJ').write_bytes(blob)
    (work/'OUT/HELLO.TXT').write_bytes(b'original')
    execute(work,[f'ARJTEST D:\\IN.ARJ D:\\OUT {mode} > RESULT.TXT'],True)
    result = (work/'RESULT.TXT').read_text()
    assert f'ok={int(ok)}' in result,(name,result)
    assert (work/'OUT/HELLO.TXT').read_bytes() == (b'A'*1000 if ok else b'original')
    assert not list((work/'OUT').glob('*.TMP'))
print('PASS: 8086 ARJ singleton, corruption, allocation failure and cancellation preserve destinations',flush=True)

# Use a real DOS FAT12 filesystem for replacement and timestamp validation.
image = WORK/'ARJ.IMG'
def mtool(*args): return sp.check_output(args,env=ENV,text=True)
mtool('mformat','-f','1440','-C','-i',str(image),'::')
mtool('mcopy','-i',str(image),str(ROOT/'tests/fixtures/arj/METHOD1.ARJ'),'::IN.ARJ')
mtool('mmd','-i',str(image),'::OUT')
(WORK/'ORIGINAL.TXT').write_bytes(b'original')
mtool('mcopy','-i',str(image),str(WORK/'ORIGINAL.TXT'),'::OUT/HELLO.TXT')
execute(WORK,[r'ARJTEST E:\IN.ARJ E:\OUT > FAT.TXT'],True,[f'imgmount e "{image}" -t floppy'])
result = (WORK/'FAT.TXT').read_text()
assert 'ok=1 files=4 bytes=197406 completed=4' in result,result
export = WORK/'export'
if export.exists(): shutil.rmtree(export)
export.mkdir()
mtool('mcopy','-s','-i',str(image),'::OUT',str(export))
for name,info in manifest.items():
    assert hashlib.sha256((export/'OUT'/name).read_bytes()).hexdigest() == info['sha256'],name
listing = mtool('mdir','-i',str(image),'::OUT/HELLO.TXT')
assert '2000-01-01' in listing and '0:00' in listing,listing
assert not list((export/'OUT').rglob('*.TMP')) and not list((export/'OUT').rglob('*.BAK'))
bad = mutate_header((ROOT/'tests/fixtures/arj/METHOD1.ARJ').read_bytes(),1,20,b'\0'*4)
(WORK/'BAD.ARJ').write_bytes(bad)
mtool('mcopy','-o','-i',str(image),str(WORK/'BAD.ARJ'),'::IN.ARJ')
execute(WORK,[r'ARJTEST E:\IN.ARJ E:\OUT > BAD.TXT'],True,[f'imgmount e "{image}" -t floppy'])
assert 'ok=0' in (WORK/'BAD.TXT').read_text()
mtool('mcopy','-o','-i',str(image),'::OUT/HELLO.TXT',str(WORK/'AFTER.TXT'))
assert hashlib.sha256((WORK/'AFTER.TXT').read_bytes()).hexdigest() == manifest['HELLO.TXT']['sha256']
print('PASS: FAT12 ARJ overwrite, timestamps, CRC-exact output and corrupt replacement recovery',flush=True)

# Exercise the real standalone executable's unattended command and exit codes.
work = WORK/'CLI'
if work.exists(): shutil.rmtree(work)
work.mkdir(); (work/'OUT').mkdir()
shutil.copy2(ROOT/'build/NW.EXE',work/'NW.EXE')
shutil.copy2(ROOT/'tests/fixtures/arj/METHOD1.ARJ',work/'IN.ARJ')
commands = [r'NW /unarj D:\IN.ARJ D:\OUT > CLI.TXT',
            'if errorlevel 1 goto failed',
            r'NW /unarj D:\IN.ARJ D:\OUT > EXISTS.TXT',
            'if not errorlevel 1 goto failed',
            'NW /unarj > USAGE.TXT', 'if not errorlevel 1 goto failed',
            'echo OK>CLI.OK', 'goto done', ':failed', 'echo FAIL>CLI.FAI', ':done']
(work/'CLI.BAT').write_bytes(('\r\n'.join(['@echo off',*commands])+'\r\n').encode('ascii'))
execute(work,['call CLI.BAT'],True)
assert (work/'CLI.OK').exists() and not (work/'CLI.FAI').exists()
assert '4 files unpacked' in (work/'CLI.TXT').read_text()
assert 'destination exists' in (work/'EXISTS.TXT').read_text()
assert 'NW /unarj archive.arj existing-directory' in (work/'USAGE.TXT').read_text()
for name,info in manifest.items():
    assert hashlib.sha256((work/'OUT'/name).read_bytes()).hexdigest() == info['sha256'],name
print('PASS: production NW /unarj, no-overwrite policy, usage and DOS exit status on 8086',flush=True)
