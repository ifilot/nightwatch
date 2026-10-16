# SPDX-License-Identifier: GPL-3.0-only
"""Optional, isolated Turbo C 2.0 / zlib 1.3.2 raw-inflate feasibility checks."""
from pathlib import Path
import os, sys, subprocess as sp, random, zlib, re, argparse
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tests'))
from dosbuild import compile_dos
root=Path(__file__).resolve().parents[1]; work=root/'build/zlib-probe'
work.mkdir(parents=True,exist_ok=True)
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
tc=os.environ.get('DOS_TOOLCHAIN',str(root/'buildenv'))

parser=argparse.ArgumentParser(description='Optional zlib 1.3.2 / Turbo C 2.0 feasibility probe; no production changes')
parser.add_argument('--source',type=Path,required=True,help='Unmodified, extracted official zlib 1.3.2 source directory')
args=parser.parse_args()
assert args.source.resolve()!=work.resolve(), 'Keep upstream sources separate from the probe staging directory'
assert '#define ZLIB_VERSION \"1.3.2\"' in (args.source/'zlib.h').read_text(), 'Expected zlib 1.3.2'

# Changes are confined to staged files, retaining all upstream license notices.
patches={
 'zconf.h': [('typedef char  FAR charf;', '#define charf char FAR'),
             ('typedef int   FAR intf;', '#define intf int FAR'),
             ('typedef uInt  FAR uIntf;', '#define uIntf uInt FAR'),
             ('typedef uLong FAR uLongf;', '#define uLongf uLong FAR')],
 'zutil.h': [('typedef uch FAR uchf;', '#define uchf uch FAR'),
             ('typedef ush FAR ushf;', '#define ushf ush FAR'),
             ('#warning zlib not thread-safe', '/* No threads in DOS; Turbo C 2.0 cannot parse #warning. */')],
 'inffast.c': [('code const *here;', 'code const FAR *here;')],
 'zutil.c': [('const uchf *p = s1, *q = s2;', 'const uchf *p = s1; const uchf *q = s2;')],
 'crc32.h': [('local const z_crc_t FAR', 'local const unsigned long')],
}
files=('inflate.c','inffast.c','inftrees.c','zutil.c','adler32.c','crc32.c',
       'zlib.h','zconf.h','zutil.h','inflate.h','inftrees.h','inffast.h','inffixed.h','crc32.h')
for name in files:
    text=(args.source/name).read_text(encoding='ascii')
    for old,new in patches.get(name,()):
        assert old in text,(name,'Upstream source changed',old)
        text=text.replace(old,new)
    if name in patches:
        text='/* Nightwatch probe: Turbo C 2.0 compatibility adaptation; see docs/zip-support.md. */\n'+text
    (work/name.upper()).write_bytes(text.replace('\n','\r\n').encode('ascii'))
(work/'PROBE.C').write_bytes((root/'tests/ZPROBE.C').read_text().replace('\n','\r\n').encode('ascii'))
(work/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
def execute(directory,commands,cpu=False):
    args=['dosbox-x' if cpu else 'dosbox','-conf',str(root/'tools/dosbox.conf')]
    if cpu: args+=['-conf',str(work/'CPU.CONF'),'-fastlaunch','-nogui']
    for cmd in [f'mount c "{tc}"',f'mount d "{directory}"','d:',*commands,'exit']:args+=['-c',cmd]
    with (work/'DOSBOX.LOG').open('w') as log:sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=180)
for old in work.glob('*.OBJ'): old.unlink()
modules=['INFLATE','INFFAST','INFTREES','ZUTIL','ADLER32','CRC32']
commands=[r'set PATH=C:\TC',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB']
flags='-1- -ms -O -Z -DZ_SOLO -DNO_GZIP'
commands+=[f'tcc {flags} -c {m}.C > {m}.LOG' for m in modules]
commands+=[f'tcc {flags} -c PROBE.C > PROBE.LOG', 'tcc -1- -ms -M -eZPROBE.EXE *.OBJ > LINK.LOG']
compile_dos(work,execute,'ZBUILD',commands,['ZPROBE.EXE'],[m+'.LOG' for m in modules]+['PROBE.LOG','LINK.LOG'])
raw=random.Random(2026).randbytes(30000)
cases={'EMPTY':(b'',6,zlib.Z_DEFAULT_STRATEGY),'FIXED':(b'Nightwatch DOS inflate.\r\n'*4000,6,zlib.Z_FIXED),'DYNAMIC':(bytes(range(256))*800,6,zlib.Z_DEFAULT_STRATEGY),'WINDOW':(raw+raw[:16000]+b'END',9,zlib.Z_DEFAULT_STRATEGY),'STORED':(raw,0,zlib.Z_DEFAULT_STRATEGY)}
for name,(data,level,strategy) in cases.items():
    c=zlib.compressobj(level,zlib.DEFLATED,-15,8,strategy)
    compressed=c.compress(data)+c.flush()
    block_types={'FIXED':1, 'DYNAMIC':2, 'STORED':0}
    if name in block_types:
        assert ((compressed[0]>>1)&3)==block_types[name],(name,'Unexpected fixture block type')
    if name=='WINDOW':
        small=zlib.decompressobj(-14)
        try:
            for at in range(0,len(compressed),1024):
                pending=compressed[at:at+1024]
                while pending:
                    small.decompress(pending,1024)
                    pending=small.unconsumed_tail
        except zlib.error:
            pass
        else:
            raise AssertionError('Fixture must require more than 16 KiB of history')
    (work/(name+'.DFL')).write_bytes(compressed)
    (work/(name+'.OUT')).unlink(missing_ok=True)
    execute(work,[f'ZPROBE {name} > {name}.TXT'],True)
    result=(work/(name+'.TXT')).read_text()
    assert 'status=1' in result and 'int=2 pointer=2' in result,result
    assert (work/(name+'.OUT')).read_bytes()==data,(name,'bytes differ')
    assert f'crc={zlib.crc32(data):08X}' in result,result
    print(name,result.strip(),flush=True)
(work/'INVALID.DFL').write_bytes(b'\x07')
(work/'TRUNC.DFL').write_bytes((work/'DYNAMIC.DFL').read_bytes()[:-5])
for name,status in [('INVALID',-3),('TRUNC',-5)]:
    execute(work,[f'ZPROBE {name} > {name}.TXT'],True)
    result=(work/(name+'.TXT')).read_text(); assert f'status={status}' in result,result
    print(name,result.strip(),flush=True)
execute(work,['ZPROBE WINDOW FAIL > ALLOC.TXT'],True)
result=(work/'ALLOC.TXT').read_text(); assert 'status=-4' in result,result
print('ALLOC',result.strip(),flush=True)
print('EXE bytes:',(work/'ZPROBE.EXE').stat().st_size,flush=True)
