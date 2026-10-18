# SPDX-License-Identifier: GPL-3.0-only
"""DOS workflows for the eight usability improvements, including FAT attributes."""
from pathlib import Path
import os
import shutil
import struct
import subprocess as sp
import sys
from ui_build import require_prebuilt, build_ui

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from vram import decode
try: require_prebuilt()
except (AssertionError,FileNotFoundError): build_ui()
BASE=ROOT/'build/workflows'
BASE.mkdir(parents=True,exist_ok=True)
(BASE/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
for mode in ('text','vga'):
    work=BASE/mode
    if work.exists(): shutil.rmtree(work)
    (work/'BIG/SUB').mkdir(parents=True)
    (work/'DEST').mkdir()
    (work/'RUN').mkdir()
    (work/'ERROR').mkdir()
    for n in range(1100): (work/'BIG'/f'F{n:04}.TXT').write_bytes(b'page sample\r\n')
    for name in ('OTHER.TXT','TARGET.TXT'):
        (work/'BIG/SUB'/name).write_bytes((name+' new contents\r\n').encode('ascii'))
        (work/'DEST'/name).write_bytes(b'old destination\r\n')
    (work/'RUN/RUN.BAT').write_bytes(b'@echo off\r\necho Launched selected BAT>D:\\LAUNCH.TXT\r\n')
    (work/'ERROR/FILE.BIN').write_bytes(bytes(range(256))*160)
    shutil.copy2(ROOT/'build/video/UITEST.EXE',work/'UITEST.EXE')
    keys=[]; frames={}
    def press(*values): keys.extend(values)
    def type_text(value): press(21,*map(ord,value),13)
    def frame(name): frames[name]=len(keys)
    def path(value): press(0x4300,ord('.')); type_text(value)
    frame('initial')
    press(0x7600); frame('page2')
    press(0x7600); frame('page3')
    press(0x8400,0x8400)
    press(0x6e00,13); type_text('TARGET.TXT'); frame('match')
    press(13); frame('search-location')
    press(1,ord('r'),13,1); frame('readonly')
    press(ord('r'),ord('h'),13)
    press(16); type_text('*.TXT')
    press(0x3f00,13); frame('conflict')
    press(ord('a')); frame('overwrite-all')
    press(16); type_text('*.TXT')
    press(0x3f00,13,ord('k')); frame('skip-all')
    press(ord('-'))
    path(r'D:\RUN')
    press(6); type_text('RUN.BAT'); press(13,13); frame('launched')
    press(*map(ord,'echo HISTORY>HIST.TXT'),13,13)
    press(5); frame('command-history')
    press(27)
    # Middle insertion, End and Backspace create ABC rather than ACB or ABCD.
    press(0x4100,ord('A'),ord('C'),0x4b00,ord('B'),0x4f00,ord('D'),8,13)
    path(r'D:\ERROR')
    press(0x5000,0xf101,0x3f00,13); frame('error-retry')
    press(ord('r')); frame('retried')
    # A second injected error is skipped, preserving both original files.
    press(0xf101,0x3f00,13,ord('o')); frame('error-skip')
    press(ord('s')); frame('skipped-error')
    press(0xf101,0x3f00,13,ord('o'),27,13); frame('cancelled-error')
    # Completion includes directory separators and works in the path dialog.
    press(0x4300,ord('.'),21,*map(ord,r'D:\RU'),9,13); frame('completed-path')
    press(0x6e00); type_text(r'D:\BIG'); type_text('F1099.*'); press(13); frame('search-late-page')
    press(0x4400)
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys)+'\n')
    raw=BASE/(mode+'.FAT'); raw.unlink(missing_ok=True)
    sp.run(['mformat','-C','-i',str(raw),'-t','256','-h','16','-s','32','-r','64','-c','4','::'],check=True)
    sp.run(['mcopy','-s','-i',str(raw),
            *map(str,sorted(p for p in work.iterdir() if p.name!='BIG')),'::'],check=True)
    # mcopy -s follows the host's directory enumeration order. ext4 and NTFS
    # differ, so recursive copying can put F1099.TXT on any DOS page. Insert
    # BIG's children explicitly in filename order to exercise page 3 everywhere.
    sp.run(['mmd','-i',str(raw),'::BIG'],check=True)
    sp.run(['mcopy','-s','-i',str(raw),
            *map(str,sorted((work/'BIG').iterdir())),'::BIG'],check=True)
    payload=bytearray(raw.read_bytes()); struct.pack_into('<I',payload,28,63)
    prefix=bytearray(63*512)
    prefix[446:462]=struct.pack('<B3sB3sII',0,b'\x01\x20\x00',6,b'\xfe\xff\xff',63,len(payload)//512)
    prefix[510:512]=b'\x55\xaa'
    volume=BASE/(mode+'.IMG'); volume.write_bytes(prefix+payload); raw.unlink()
    args=['dosbox-x','-fastlaunch','-nogui','-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(BASE/'CPU.CONF')]
    mount=f'imgmount d "{volume}" -t hdd -fs fat -size 512,32,16,257 -o partidx=0'
    for command in [mount,'d:',f'UITEST /{mode} D:\\BIG D:\\DEST','exit']: args+=['-c',command]
    with (BASE/(mode+'.LOG')).open('w') as log:
        sp.run(args,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),stdout=log,stderr=log,check=True,timeout=420)
    image=str(volume)+'@@32256'
    out=BASE/(mode+'-captures'); out.mkdir(exist_ok=True)
    sp.run(['mcopy','-o','-i',image,'::F*.BIN',str(out)],check=True)
    assert len(list(out.glob('F*.BIN')))==len(keys),(mode,'script stopped early')
    for name,index in frames.items():
        if name!='launched': decode(out/f'F{index:03}.BIN',out/(name+'.png'))
    def contents(path): return sp.check_output(['mtype','-i',image,'::'+path])
    for name in ('OTHER.TXT','TARGET.TXT'):
        assert contents('DEST/'+name)==(name+' new contents\r\n').encode('ascii')
    assert contents('DEST/FILE.BIN')==bytes(range(256))*160
    assert contents('ERROR/FILE.BIN')==bytes(range(256))*160
    assert b'Launched selected BAT' in contents('LAUNCH.TXT')
    assert b'HISTORY' in contents('RUN/HIST.TXT')
    listing=sp.check_output(['mdir','-b','-i',image,'::RUN/*']).decode()
    assert 'ABC/' in listing or 'ABC' in listing
    attrs=sp.check_output(['mattrib','-i',image,'::BIG/SUB/TARGET.TXT']).decode()
    assert 'H' in attrs and 'R' not in attrs.split('::')[0],attrs
    if mode=='text':
        def screen(name): return (out/f'F{frames[name]:03}.BIN').read_bytes()[3:4003:2].decode('cp437')
        assert 'Free ' in screen('initial') and 'Selected ' in screen('initial')
        assert 'Page 2:' in screen('page2') and 'Page 3:' in screen('page3')
        assert 'D:\\BIG\\SUB' in screen('search-location') and 'TARGET.TXT' in screen('search-location')
        assert 'Readonly [X]' in screen('readonly')
        assert 'Source:' in screen('conflict') and 'Target:' in screen('conflict')
        assert 'overwrite all' in screen('conflict') and 'skip all' in screen('conflict')
        assert '2 completed, 0 skipped' in screen('overwrite-all')
        assert '0 completed, 2 skipped' in screen('skip-all')
        assert 'Transfer failed' in screen('error-retry') and 'R retry file' in screen('error-retry')
        assert 'echo HISTORY>HIST.TXT' in screen('command-history')
        assert 'D:\\RUN' in screen('completed-path')
        assert 'Page 3:' in screen('search-late-page') and 'F1099.TXT' in screen('search-late-page')
    names=sp.check_output(['mdir','-b','-i',image,'::DEST/*']).decode().upper().splitlines()
    assert not any(name.endswith(('.TMP','.NEW','.BAK')) for name in names),names
    print('PASS:',mode,'1100-entry paging, recursive search, properties, batch conflicts, program launch, input editing/completion, retry/skip/cancel',flush=True)
