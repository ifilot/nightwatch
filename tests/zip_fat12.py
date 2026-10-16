# SPDX-License-Identifier: GPL-3.0-only
"""Verify staged ZIP overwrite and timestamps on a real DOS FAT12 volume."""
from pathlib import Path
import hashlib, json, os, shutil, subprocess as sp
ROOT=Path(__file__).resolve().parents[1]
WORK=ROOT/'build/zip-fat12'; WORK.mkdir(parents=True,exist_ok=True)
IMAGE=WORK/'ZIP.IMG'
ENV=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy')
TC=os.environ.get('DOS_TOOLCHAIN',str(ROOT/'buildenv'))
def mtool(*args): return sp.check_output(args,env=ENV,text=True)
def dos(command):
    args=['dosbox-x','-fastlaunch','-nogui','-conf',str(ROOT/'tools/dosbox.conf'),
          '-conf',str(ROOT/'build/zip-dos/CPU.CONF')]
    for c in [f'mount c "{TC}" -ro',f'mount d "{ROOT}/build/zip-dos"',
              f'imgmount e "{IMAGE}" -t floppy','d:',command,'exit']:args+=['-c',c]
    with (WORK/'DOSBOX.LOG').open('w') as log:
        sp.run(args,env=ENV,stdout=log,stderr=log,check=True,timeout=180)
mtool('mformat','-f','1440','-C','-i',str(IMAGE),'::')
mtool('mcopy','-i',str(IMAGE),str(ROOT/'tests/fixtures/zip/WINBEST.ZIP'),'::IN.ZIP')
mtool('mmd','-i',str(IMAGE),'::OUT')
(WORK/'ORIGINAL.TXT').write_bytes(b'original')
mtool('mcopy','-i',str(IMAGE),str(WORK/'ORIGINAL.TXT'),'::OUT/HELLO.TXT')
dos(r'ZIPTEST E:\IN.ZIP E:\OUT > FAT.TXT')
result=(ROOT/'build/zip-dos/FAT.TXT').read_text()
assert 'ok=1 files=5 bytes=428803 completed=5' in result,result
export=WORK/'export'
if export.exists(): shutil.rmtree(export)
export.mkdir()
mtool('mcopy','-s','-i',str(IMAGE),'::OUT',str(export))
manifest=json.loads((ROOT/'tests/fixtures/zip/manifest.json').read_text())
for name,info in manifest.items(): assert hashlib.sha256((export/'OUT'/name).read_bytes()).hexdigest()==info['sha256'],name
listing=mtool('mdir','-i',str(IMAGE),'::OUT/HELLO.TXT')
assert '2026-10-08' in listing and '12:34' in listing,listing
assert not list((export/'OUT').rglob('*.TMP')) and not list((export/'OUT').rglob('*.BAK'))
print('PASS: FAT12 extraction/overwrite, CRC-exact output, timestamps and staging cleanup',flush=True)
# A bad payload/CRC must leave the already committed original unchanged.
bad=bytearray((ROOT/'tests/fixtures/zip/WINBEST.ZIP').read_bytes())
bad[39]^=8
(WORK/'BAD.ZIP').write_bytes(bad)
mtool('mcopy','-o','-i',str(IMAGE),str(WORK/'BAD.ZIP'),'::IN.ZIP')
dos(r'ZIPTEST E:\IN.ZIP E:\OUT > BAD.TXT')
result=(ROOT/'build/zip-dos/BAD.TXT').read_text(); assert 'ok=0' in result,result
mtool('mcopy','-o','-i',str(IMAGE),'::OUT/HELLO.TXT',str(WORK/'AFTER.TXT'))
assert hashlib.sha256((WORK/'AFTER.TXT').read_bytes()).hexdigest()==manifest['HELLO.TXT']['sha256']
print('PASS: FAT12 corrupted ZIP preserves overwritten file',flush=True)
