# SPDX-License-Identifier: GPL-3.0-only
"""Exercise new keyboard workflows against the DOS renderer test executable."""
from pathlib import Path
import os
import shutil
import struct
import sys
import subprocess as sp
from dosbuild import compile_dos
from doszip import compile_app
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from vram import decode
base = root / 'build/video'
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
toolchain = Path(os.environ.get('DOS_TOOLCHAIN', str(root / 'buildenv')))
def dos(work, command, machine, before=(), xt=False, cycles=None):
    # Every launch must produce its own evidence, including saved-mode reloads.
    for pattern in ('F*.BIN','F*.POS','P*.BIN','P*.TXT'):
        for capture in work.glob(pattern): capture.unlink()
    fat = xt or bool(os.environ.get('NW_FEATURE_8086'))
    if fat:
        (base/'CPUFEAT.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=100000\n')
        args = ['dosbox-x','-fastlaunch','-nogui','-conf',str(root/'tools/dosbox.conf'),
                '-conf',str(base/'CPUFEAT.CONF'),'-machine',machine]
    else:
        args = ['dosbox', '-conf', str(root/'tools/dosbox.conf'), '-conf', str(base/'FAST.CONF'), '-machine', machine]
    if cycles is not None:
        (work/'SLOW.CONF').write_text(f'[cpu]\ncycles={cycles}\n')
        args += ['-conf', str(work/'SLOW.CONF')]
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
    content = bytes(range(256))*4000+b'ABC'
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
    # Slow this copy fixture enough for BIOS-timed intermediate bar samples.
    dos(work, r'UITEST /'+mode+r' D:\LEFT D:\RIGHT', machine, cycles=3000)
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
    # Actual dialog captures prove both counting and transfer UI run in every
    # renderer. A skipped file advances overall bytes without counting writes.
    progress = [list(map(int, p.read_text().split())) for p in sorted(work.glob('P*.TXT'))]
    assert any(p[0] == 1 and p[1] >= 1 for p in progress), 'Missing counting dialog'
    assert any(p[0] == 0 and p[1] == 1 and p[2] == len(content) for p in progress)
    assert any(p[0] == 0 and p[4] == 1 and p[7] == p[2] for p in progress), 'Skip must resolve bytes'
    assert any(p[0] == 0 and p[3] == p[1] and p[7] == p[2] for p in progress), 'Missing completed totals'
    # Save an actual in-flight copy before later launches clear its captures.
    candidates = [(p, list(map(int, p.read_text().split()))) for p in sorted(work.glob('P*.TXT'))]
    partial = [(p, state) for p, state in candidates if not state[0] and 0 < state[5] < state[6]]
    assert partial, 'Expected a copy in progress'
    sampled = [(p, snapshot) for p, snapshot in partial if snapshot[9] > 0]
    capture, copy_state = min(sampled or partial, key=lambda item: abs(item[1][5] / item[1][6] - 0.5))
    screenshot = decode(capture.with_suffix('.BIN'), base/('copy-'+mode+'.png'))
    if mode != 'text':
        row_height = {'cga': 8, 'ega': 14, 'vga': 16}[mode]
        row = (30 if mode == 'vga' else 25)//2 - 5 + 4
        fill = 318 * (copy_state[5] * 100 // copy_state[6]) // 100
        assert fill > 0
        y = row * row_height + 3
        assert screenshot.getpixel((121, y)) == ((0,0,0) if mode == 'cga' else (0,0,170))
        assert screenshot.getpixel((121+fill, y)) == ((255,255,255) if mode == 'cga' else (170,170,170))
    if mode == 'text':
        captures = sorted(work.glob('P*.BIN'))
        decoded = [p.read_bytes()[3:4003:2].decode('cp437') for p in captures]
        assert any('Counting files' in frame for frame in decoded)
        assert any('File     [' in frame and 'Overall  [' in frame and
                   'KiB/s' in frame and 'Time left:' in frame for frame in decoded)
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
    if source.suffix in {'.C','.H','.OBJ','.ASM'}: shutil.copy2(source,work/source.name)
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
    assert(fraction(1,3,40)==13);
    assert(fraction(0xfffffffeUL,0xffffffffUL,100)==99);
    assert(fraction(0x80000000UL,0xffffffffUL,100)==50);
    assert(fraction(0,0,40)==40);
    assert(tick_elapsed(3,0x1800afUL)==4);
    begin_operation(); copy_progress=1; total_files=1; total_bytes=4096;
    assert(operation_ui("D:\\RATE.BIN",0,4096));
    sample_tick=(unsigned long)biostime(0,0L);
    sample_tick=sample_tick>=18 ? sample_tick-18 : 0x1800b0UL+sample_tick-18;
    operation_transferred=operation_current_bytes=2048; force_progress=1;
    assert(operation_ui("D:\\RATE.BIN",2048,4096));
    assert(transfer_rate>=1024 && transfer_rate<=2071);
    total_files=2; total_bytes=0; operation_files=1;
    operation_bytes=operation_current_bytes=0; force_progress=1;
    assert(operation_ui("D:\\EMPTY.TXT",0,0));
    copy_progress=0; end_operation();
    settings_save(); assert(!strcmp(nw_error,"Path is too long"));
    video_restore(); puts("PASS: checked settings paths"); return 0;
}
'''
(work/'LONG.C').write_bytes(long_source.replace('\n','\r\n').encode('ascii'))
compile_app(work,lambda directory,cmds: dos(directory,cmds[0],'cga'),'LNBUILD',
    [r'set PATH=C:\TC;C:\TASM',r'set INCLUDE=C:\TC\INCLUDE',r'set LIB=C:\TC\LIB',
     'tcc -DNW_DIAGNOSTICS -1- -mm -O -Z -eLONG.EXE LONG.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > LONG.LOG'],
    ['LONG.EXE'],['LONG.LOG'])
script(work,[27]); dos(work,'LONG > LONGRUN.LOG','cga')
assert 'PASS: checked settings paths' in (work/'LONGRUN.LOG').read_text()
rate_frame=(work/'P001.BIN').read_bytes()[3:4003:2].decode('cp437')
assert '50%' in rate_frame and 'KiB/s' in rate_frame and 'Time left: 0:' in rate_frame
assert '-- KiB/s' not in rate_frame
empty_frame=(work/'P002.BIN').read_bytes()[3:4003:2].decode('cp437')
assert '100%' in empty_frame and '50%' in empty_frame
print('PASS: DOS progress fractions near 4 GiB, midnight ticks, sampled speed, file ETA and empty-file batches',flush=True)
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
