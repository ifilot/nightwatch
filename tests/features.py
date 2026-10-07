# SPDX-License-Identifier: GPL-3.0-only
"""Exercise new keyboard workflows against the DOS renderer test executable."""
from pathlib import Path
import os
import shutil
import struct
import subprocess as sp
from dosbuild import compile_dos
root = Path(__file__).resolve().parents[1]
base = root / 'build/video'
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
toolchain = Path(os.environ.get('DOS_TOOLCHAIN', str(root / 'buildenv')))
def dos(work, command, machine, before=(), xt=False):
    # Every launch must produce its own evidence, including saved-mode reloads.
    for pattern in ('F*.BIN','F*.POS'):
        for capture in work.glob(pattern): capture.unlink()
    fat = xt or bool(os.environ.get('NW_FEATURE_8086'))
    if fat:
        (base/'CPUFEAT.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
        args = ['dosbox-x','-fastlaunch','-nogui','-conf',str(root/'tools/dosbox.conf'),
                '-conf',str(base/'CPUFEAT.CONF'),'-machine',machine]
    else:
        args = ['dosbox', '-conf', str(root/'tools/dosbox.conf'), '-conf', str(base/'FAST.CONF'), '-machine', machine]
    # DOSBox-X local-drive directory caching can reject a recursive rmdir.
    # Use an actual DOS FAT volume for the 8086 integration run.
    if fat:
        volume=base/(work.name+'.IMG')
        raw=base/(work.name+'.FAT')
        raw.unlink(missing_ok=True)
        sp.run(['mformat','-C','-i',str(raw),'-t','128','-h','16','-s','32','-r','64','-c','4','::'],check=True)
        entries=list(work.iterdir())
        if entries: sp.run(['mcopy','-s','-i',str(raw),*[str(p) for p in entries],'::'],check=True)
        # Standard MBR with one FAT16 partition at sector 63.
        payload=bytearray(raw.read_bytes()); struct.pack_into('<I',payload,28,63)
        prefix=bytearray(63*512)
        prefix[446:462]=struct.pack('<B3sB3sII',0,b'\x01\x20\x00',6,b'\xfe\xff\xff',63,len(payload)//512)
        prefix[510:512]=b'\x55\xaa'
        volume.write_bytes(prefix+payload); raw.unlink()
        mount=f'imgmount d "{volume}" -t hdd -fs fat -size 512,32,16,129 -o partidx=0'
    else: mount=f'mount d "{work}"'
    for cmd in [f'mount c "{toolchain}"', mount, 'd:', *before, command, 'exit']:
        args += ['-c', cmd]
    with (work/'DOSBOX.LOG').open('w') as f:
        sp.run(args, env=env, stdout=f, stderr=f, check=True, timeout=240 if fat else 60)
    if fat:
        transcript=(work/'DOSBOX.LOG').read_bytes()
        for entry in work.iterdir():
            if entry.is_dir(): shutil.rmtree(entry)
            else: entry.unlink()
        sp.run(['mcopy','-s','-i',str(volume)+'@@32256','::*',str(work)],check=True)
        (work/'DOSBOX.LOG').write_bytes(transcript)
def text(value): return [21] + list(map(ord, value)) + [13]
def script(work, keys): (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys))
for machine, mode in [('cga','text'), ('cga','cga'), ('ega','ega'), ('vgaonly','vga')]:
    work = base/('features-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT/TREE/NEST').mkdir(parents=True); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE', work/'UITEST.EXE')
    content = bytes(range(256))*300+b'ABC'
    (work/'LEFT/BETA.BIN').write_bytes(content)
    (work/'LEFT/ALPHA.TXT').write_bytes(b'alpha\r\n')
    (work/'LEFT/TREE/NEST/INNER.TXT').write_bytes(b'nested\r\n')
    keys = [6] + text('BETA.*') + [0x5600, 7] + text('0x10000')
    jumped = len(keys)
    keys += [0x4100] + text('hex:41 42 43')
    found = len(keys)
    keys += [27, 19, ord('s'), 16] + text('*.TXT')
    marked = len(keys)
    keys += [14] + text('*.TXT')
    cleared = len(keys)
    keys += [16] + text('*.TXT') + [0x3f00, 13, 0x3f00, 13, 0x3f00, 13, ord('s')]
    keys += [6] + text('TREE') + [13,0x5000,13,8]
    parent = len(keys)
    keys += [8,0x3f00,13,0x3f00,13,ord('o'),9,6] + text('TREE') + [0x4200,ord('y'),23,0x4400]
    script(work, keys)
    dos(work, r'UITEST /'+mode+r' D:\LEFT D:\RIGHT', machine)
    assert (work/'RIGHT/BETA.BIN').read_bytes() == content
    assert (work/'RIGHT/ALPHA.TXT').read_bytes() == b'alpha\r\n'
    assert not (work/'RIGHT/TREE').exists()
    assert (work/'LEFT/TREE/NEST/INNER.TXT').read_bytes() == b'nested\r\n'
    def state(index): return list(map(int, (work/f'F{index:03d}.POS').read_text().split()))
    assert state(marked)[10] == 1 and state(cleared)[10] == 0
    assert state(parent)[4] == 1, 'Parent must reselect NEST'
    if mode == 'text':
        def row(index): return (work/f'F{index:03d}.BIN').read_bytes()[3+320:3+480:2].decode('ascii')
        assert row(jumped).startswith('00010000')
        assert row(found).startswith('00010041  41 42 43')
    cfg = (work/'NIGHT.CFG').read_text().splitlines()
    assert cfg[0] == 'NIGHTWATCH1' and int(cfg[1].split()[0]) == ['text','cga','ega','vga'].index(mode)
    assert cfg[1].split()[1] == '1' and cfg[2:] == [r'D:\LEFT', r'D:\RIGHT']
    assert not (work/'NIGHT.NEW').exists() and not (work/'NIGHT.BAK').exists()
    # No command arguments: saved mode/paths/sorting load; a second save replaces safely.
    script(work, [19, ord('r'), 23, 0x4400])
    dos(work, 'UITEST', machine)
    assert state(0)[0] == ['text','cga','ega','vga'].index(mode)
    assert (work/'NIGHT.CFG').read_text().splitlines()[1].split()[2] == '1'
    assert not (work/'NIGHT.NEW').exists() and not (work/'NIGHT.BAK').exists()
    # Explicit arguments override both saved paths and mode.
    script(work, [0x4400]); dos(work, r'UITEST /text D:\RIGHT D:\LEFT', machine)
    assert state(0)[0] == 0
    cells = (work/'F000.BIN').read_bytes()[3:4003]
    paths = cells[320:480:2].decode('cp437')
    assert r'D:\RIGHT' in paths[:40] and r'D:\LEFT' in paths[40:]
    if machine != 'vgaonly':
        changed = (work/'NIGHT.CFG').read_text().splitlines()
        fields = changed[1].split(); fields[0] = '3'; changed[1] = ' '.join(fields)
        (work/'NIGHT.CFG').write_text('\n'.join(changed)+'\n')
        script(work, [0x4400]); dos(work, 'UITEST', machine)
        assert state(0)[0] == 0, 'Unavailable saved VGA mode must fall back to text'
    print('PASS:',mode,'find, mark/unmark patterns, sort, offset, binary search, parent selection, recursive copy/delete, overwrite/skip, settings save/reload',flush=True)

# Cross the old 128-page boundary in the actual text viewer, then search and End.
work = base/'features-history'
if work.exists(): shutil.rmtree(work)
work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
(work/'LEFT/LONG.TXT').write_bytes(b''.join(f'L{i:05d}\r\n'.encode('ascii') for i in range(6000)))
keys = [0x5000,0x3d00] + [0x5100]*150 + [0x4900]*131
previous = len(keys)
keys += [0x4f00]; last = len(keys)
keys += [0x4700,0x4100] + text('l05999'); found = len(keys)
keys += [27,0x4400]; script(work,keys)
dos(work,r'UITEST /text D:\LEFT D:\RIGHT','cga')
def first_line(index): return (work/f'F{index:03d}.BIN').read_bytes()[163:323:2].decode('ascii')
assert first_line(previous).startswith('L00437')
assert first_line(last).startswith('L05980')
assert first_line(found).startswith('L05999')
print('PASS: actual DOS text viewer paging backward beyond 128 cached pages, End and case-insensitive search',flush=True)

# Full 63-byte patterns, no-match retention and invalid/cancelled byte jumps.
work = base/'features-search'
if work.exists(): shutil.rmtree(work)
work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
(work/'LEFT/PATTERN.BIN').write_bytes(b'A'*31+b'!'+b'A'*63)
keys = [0x5000,0x5600,0x4100]+text('hex:'+'41 '*62+'41')
full = len(keys)
keys += [0x4200,27]; no_match = len(keys)
keys += [7]+text(' -4294967295')+[27]; invalid = len(keys)
keys += [7,27]; cancelled = len(keys)
keys += [27,0x4400]; script(work,keys)
dos(work,r'UITEST /text D:\LEFT D:\RIGHT','cga')
for index in (full,no_match,invalid,cancelled):
    row=(work/f'F{index:03d}.BIN').read_bytes()[323:483:2].decode('ascii')
    assert row.startswith('00000020'),('search/jump changed position',index,row)
print('PASS: 63-byte binary search, F8 no-match retention and invalid/cancelled offset jumps',flush=True)

# Exercise the app path bound directly: DOS shells impose shorter cwd limits.
for source in base.iterdir():
    if source.suffix in {'.C','.H','.OBJ'}: shutil.copy2(source,work/source.name)
long_source = r'''
#define main navigator_main
#include "MAIN.C"
#undef main
int main(void) {
    int mode=2, i;
    strcpy(startup,"D:");
    for(i=0;i<13;++i) strcat(startup,"\\LONGDIR0");
    assert(strlen(startup)==119);
    settings_load(&mode); assert(mode==2);
    video_init(); assert(video_set(VIDEO_TEXT));
    settings_save(); assert(!strcmp(nw_error,"Path is too long"));
    video_restore(); puts("PASS: checked settings paths"); return 0;
}
'''
(work/'LONG.C').write_bytes(long_source.replace('\n','\r\n').encode('ascii'))
compile_dos(work,lambda directory,cmds: dos(directory,cmds[0],'cga'),'LNBUILD',
    [r'set PATH=C:\TC;C:\TASM',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
     'tcc -DNW_DIAGNOSTICS -1- -ms -O -Z -eLONG.EXE LONG.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > LONG.LOG'],
    ['LONG.EXE'],['LONG.LOG'])
script(work,[27]); dos(work,'LONG > LONGRUN.LOG','cga')
assert 'PASS: checked settings paths' in (work/'LONGRUN.LOG').read_text()
notice=(work/'F000.BIN').read_bytes()[3+12*160:3+13*160:2].decode('cp437')
assert 'Path is too long' in notice and not (work/'NIGHT.CFG').exists()
print('PASS: oversized startup path skips settings load and reports save failure',flush=True)

# Corrupt and incomplete records are ignored as a whole; missing paths fall back.
cases = [
    ("NIGHTWATCH1\n0 3 1 2 1"+120*" "+"\nD:\\LEFT\nD:\\RIGHT\n", "0 0 0 0 0", ["D:"+chr(92),"D:"+chr(92)]),
    ('BROKEN\n0 3 1 2 1\nD:\\LEFT\nD:\\RIGHT\n', '0 0 0 0 0', ['D:'+chr(92),'D:'+chr(92)]),
    ('NIGHTWATCH1\n65536 3 1 2 1\nD:\\LEFT\nD:\\RIGHT\n', '0 0 0 0 0', ['D:'+chr(92),'D:'+chr(92)]),
    ('NIGHTWATCH1\n0 3 1 2 1 junk\nD:\\LEFT\nD:\\RIGHT\n', '0 0 0 0 0', ['D:'+chr(92),'D:'+chr(92)]),
    ('NIGHTWATCH1\n0 3 1 2 1\nD:\\LEFT\n', '0 0 0 0 0', ['D:'+chr(92),'D:'+chr(92)]),
    ('NIGHTWATCH1\n0 1 0 3 1\nD:\\MISSING\nD:\\RIGHT\n', '0 1 0 3 1', ['D:'+chr(92),r'D:\RIGHT']),
]
for content, fields, paths in cases:
    (work/'NIGHT.CFG').write_text(content)
    script(work,[23,0x4400]); dos(work,'UITEST','cga')
    loaded=(work/'NIGHT.CFG').read_text().splitlines()
    assert loaded[1]==fields and loaded[2:]==paths,('malformed settings',content,loaded)
print('PASS: malformed/overflowed/truncated settings ignored atomically and missing paths fall back',flush=True)

# A failed DOS launch must not leave a previous frame available to assertions.
(work/'F000.POS').write_text('0 stale')
(work/'F000.BIN').write_bytes(b'stale')
dos(work,'MISSING','cga')
assert not (work/'F000.POS').exists() and not (work/'F000.BIN').exists()
print('PASS: failed launch cannot reuse a previous capture',flush=True)
