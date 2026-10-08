# SPDX-License-Identifier: GPL-3.0-only
"""Compile a test-only scripted navigator and capture real emulator VRAM.

No X server required. Production key handling is replaced only in staged source.
Requires DOSBox, Turbo C and Pillow. Never writes outside build/.
"""
from pathlib import Path
import os
import shutil
import subprocess as sp
import hashlib
import sys
from PIL import Image
from dosbuild import compile_dos
root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tools'))
from vram import decode, palette
base = root / 'build/video'
base.mkdir(exist_ok=True)
(base / 'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
toolchain = Path(os.environ.get('DOS_TOOLCHAIN', str(root / 'buildenv')))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')

def dos(directory, commands, machine='svga_s3'):
    args = ['dosbox', '-conf', str(root / 'tools/dosbox.conf'), '-conf', str(base / 'FAST.CONF'), '-machine', machine]
    for cmd in [f'mount c "{toolchain}"', f'mount d "{directory}"', 'd:'] + commands + ['exit']:
        args += ['-c', cmd]
    with (directory / 'DOSBOX.LOG').open('w') as f:
        sp.run(args, env=env, stdout=f, stderr=f, check=True, timeout=45)

for src in (root / 'src').iterdir():
    if src.suffix in {'.C', '.H', '.ASM'}:
        text = src.read_text()
        if src.name == 'MAIN.C':
            begin = text.index('static int key_read(void)')
            end = text.index('static int pane_rows', begin)
            text = text[:begin] + '#include "UITEST.H"\nstatic int key_read(void) { return test_key_read(); }\n' + text[end:]
            # Capture copy dialogs separately so ordinary scripted-key frame
            # numbers remain stable. Production source has no capture calls.
            text = text.replace('        paint_tick = now;',
                                '        test_progress_capture(done, total); paint_tick = now;')
        if src.name == 'CORE.C':
            # Test-only pacing makes BIOS-timed copy snapshots deterministic,
            # even when the mounted host filesystem transfers a MB instantly.
            text = '#include <dos.h>\n' + text
            text = text.replace('        operation_current_bytes = copied;',
                                '        delay(5); operation_current_bytes = copied;')
        (base / src.name).write_bytes(text.replace('\n', '\r\n').encode('ascii'))
(base / 'UITEST.H').write_bytes((root / 'tests/UITEST.H').read_text().replace('\n', '\r\n').encode('ascii'))
compile_dos(base, dos, 'UIBUILD',
    [r'set PATH=C:\TC;C:\TASM', 'tasm /mx FONT.ASM > ASSEMBLE.LOG',
     r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
     r'tcc -DNW_DIAGNOSTICS -1- -ms -O -Z -eUITEST.EXE MAIN.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > COMPILE.LOG'],
    ['UITEST.EXE','FONT.OBJ'], ['ASSEMBLE.LOG','COMPILE.LOG'])

def baseline_compare(work):
    if not os.environ.get('NW_COMPARE_OLD_VRAM'): return
    before = root/'build/speed-before/video'/work.name
    if not before.exists() or (before/'KEYS.TXT').read_text() != (work/'KEYS.TXT').read_text(): return
    frames = sorted(before.glob('F*.BIN'))
    for frame in frames:
        assert (work/frame.name).read_bytes() == frame.read_bytes(), (work.name,frame.name,'baseline VRAM differs')
    print('PASS:',work.name,len(frames),'byte-exact before/after VRAM comparisons',flush=True)

for machine, mode in [('cga','text'), ('cga','cga'), ('ega','ega'), ('vgaonly','vga'), ('hercules','text')]:
    work = base / (machine + '-' + mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir()
    shutil.copy2(base / 'UITEST.EXE', work / 'UITEST.EXE')
    (work / 'LEFT').mkdir(); (work / 'RIGHT').mkdir(); (work / 'LEFT/SUB').mkdir()
    content = b'Hello from Nightwatch.\r\n' * 70
    (work / 'LEFT/ALPHA.TXT').write_bytes(content)
    (work / 'LEFT/BETA.BIN').write_bytes(bytes(range(256)) * 300 + b'ABC')
    (work / 'LEFT/ZERO.BIN').write_bytes(b'')
    for name in ('DEMO.EXE', 'IMAGE.PCX', 'PACK.ZIP', 'README.TXT'):
        (work / 'LEFT' / name).write_bytes(b'Icon sample\r\n')
    # Each key is consumed after a VRAM capture; named indices identify hex pages.
    keys = [0x3b00, 27, 9, 9, 0x4700, 0x5000, 0x5000,
            0x3d00, 0x5100, 0x4900, 0x3e00]
    checks = {'toggle_hex': len(keys)}
    keys += [0x4f00, 0x4700, 0x3e00, 27, 0x5000, 0x5600]
    checks['binary_start'] = len(keys)
    keys += [0x5100]
    checks['binary_next'] = len(keys)
    keys += [0x3e00]
    checks['binary_position_text'] = len(keys)
    keys += [0x3e00]
    checks['binary_position_hex'] = len(keys)
    keys += [0x4f00]
    checks['binary_end'] = len(keys)
    keys += [0x4800, 0x5000, 0x4700, 0x3e00]
    checks['binary_text'] = len(keys)
    keys += [27, 0x4f00, 0x5600]
    checks['empty'] = len(keys)
    keys += [0x5100, 0x4f00, 0x4700, 0x3e00, 27,
             0x4700, 0x5000, 0x5000, 0x3f00, 13,
             0x4100, ord('N'), ord('E'), ord('W'), 13,
             9, 0x4f00, 0x4200, ord('y'), 9]
    if machine == 'vgaonly':
        keys += [0x3c00, ord('c'), 0x3c00, ord('e'), 0x3c00, ord('v')]
    else:
        keys += [0x3c00, ord('c') if machine == 'hercules' else ord('v'), 27]
    keys += [0x3c00, ord('t'), 0x4400]
    (work / 'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    dos(work, [r'UITEST /' + mode + r' D:\LEFT D:\RIGHT', 'echo RETURNED>EXIT.OK'], machine)
    assert (work / 'EXIT.OK').exists()
    assert (work / 'LEFT/BETA.BIN').read_bytes() == bytes(range(256))*300+b'ABC'
    assert (work / 'LEFT/ZERO.BIN').stat().st_size == 0
    assert (work / 'LEFT/NEW').is_dir(), (machine, 'mkdir failed')
    assert not (work / 'RIGHT/ALPHA.TXT').exists(), (machine, 'delete failed')
    frames = sorted(work.glob('F*.BIN'))
    assert len(frames) >= 20, (machine, 'script stopped early', len(frames))
    images = [decode(f) for f in frames]
    if mode == 'vga': images[checks['binary_start']].save(base/'hex.png')
    if mode == 'text':
        cells = frames[0].read_bytes()[3:4003]
        for x, y, code in [(0,1,201),(39,1,187),(40,1,201),(79,1,187),(0,21,200),(79,21,188)]:
            assert cells[(y*80+x)*2] == code, (machine, 'CP437 frame', x, y)
        assert cells[(24*80+1)*2+1] != cells[(24*80+2)*2+1], 'Footer key colors must differ'
    else:
        # Folder glyphs must remain recognizable outlines in every mode.
        image = images[0]
        y = {'cga': 29, 'ega': 49, 'vga': 53}[mode]
        icon = image.crop((22,y,38,y+{'cga': 8, 'ega': 16, 'vga': 16}[mode]))
        colors = icon.getcolors(256)
        assert len(colors) >= 2, (mode, 'icon layers')
        if mode == 'cga':
            black = sum(n for n, color in colors if color == (0,0,0))
            assert 8 < black < 100, 'CGA folder must not be a solid block'

    for index in [0, 1, 8]:
        colors = images[index].getcolors(640 * 480)
        assert len(colors) >= 2, (machine, index, 'blank')
        assert sum(n for n,c in colors if max(c) > 140) > 1000, (machine, index, 'missing glyphs')
    if mode == 'text':
        help_text = frames[1].read_bytes()[3:4003:2].decode('cp437')
        commit = (root/'src/BUILD.H').read_text().split('NW_BUILD_COMMIT "')[1].split('"')[0]
        for caption in ('Nightwatch v'+(root/'VERSION').read_text().strip(),
                        'github.com/ifilot/nightwatch', commit):
            assert caption in help_text, ('help metadata', caption)
    else:
        # Native Help contains a document viewport and fixed modal chrome.
        assert images[1].tobytes() != images[0].tobytes()
    # Full/partial binary rows and empty files, using actual text VRAM.
    page_bytes = (27 if mode == 'vga' else 22) * 16
    last_offset = (76803 - 1) // page_bytes * page_bytes
    for name in checks:
        assert sum(n for n,c in images[checks[name]].getcolors(640*480) if max(c)>140) > 1000
    assert images[checks['binary_start']].tobytes() != images[checks['binary_next']].tobytes()
    assert images[checks['binary_start']].tobytes() != images[checks['binary_end']].tobytes()
    if mode == 'text':
        def row_at(name, y):
            cells = frames[checks[name]].read_bytes()[3:4003]
            return cells[y*160:(y+1)*160:2].decode('ascii', errors='replace')
        assert row_at('toggle_hex', 0).startswith(' Hex:')
        assert row_at('toggle_hex', 2)[10:15] == '48 65'
        assert row_at('binary_start', 2).startswith('00000000  00 01 02')
        assert row_at('binary_start', 2)[61:79] == '|................|'
        assert row_at('binary_next', 2).startswith(f'{page_bytes:08X}')
        assert row_at('binary_position_text', 0).startswith(' View:')
        assert row_at('binary_position_hex', 2).startswith(f'{page_bytes:08X}')
        assert row_at('binary_end', 2).startswith(f'{last_offset:08X}')
        tail_row = 2 + (76800-last_offset)//16
        assert row_at('binary_end', tail_row).startswith('00012C00  41 42 43 ')
        assert row_at('binary_end', tail_row)[61:79] == '|ABC             |'
        assert 'EOF' in row_at('binary_end', 24)
        assert row_at('binary_text', 0).startswith(' View:')
        assert row_at('empty', 2).strip() == ''
        assert '00000000/00000000' in row_at('empty', 24) and 'EOF' in row_at('empty', 24)
    # Selection motion must change the actual rendered pixels.
    assert images[5].tobytes() != images[6].tobytes()
    assert (work / 'F000.BIN').read_bytes()[0] == ['text','cga','ega','vga'].index(mode)
    assert frames[-1].read_bytes()[0] == 0, (machine, 'F2 did not return to text')
    actual_modes = {f.read_bytes()[0] for f in frames}
    expected_modes = {'cga-text': {0}, 'cga-cga': {0,1}, 'ega-ega': {0,2}, 'vgaonly-vga': {0,1,2,3}, 'hercules-text': {0}}
    assert actual_modes == expected_modes[machine + '-' + mode], (machine, actual_modes)
    baseline_compare(work)
    print('PASS:', machine, mode, len(frames), 'VRAM captures; help, text/hex viewers, paging, empty/tail rows, copy, mkdir, delete, F2, exit', flush=True)
print('PASS: all four renderers and monochrome text with the same backend/UI')

# Dense directories: assert the scroll boundary and inspect every actual rendered row.
# Expected capacities are product requirements, not values obtained from graph_rows.
for machine, mode, capacity in [('cga','text',17), ('cga','cga',18), ('ega','ega',16), ('vgaonly','vga',24)]:
    work = base / ('density-' + mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE', work/'UITEST.EXE')
    for i in range(40): (work/'LEFT'/f'FILE{i:02d}.TXT').write_bytes(b'density sample')
    # Move to the last initial row, mark it, then cross the scroll boundary.
    keys = [0x5000]*(capacity-1) + [0x5200, 9, 9, 0x5100, 0x4900,
                                   0x4f00, 0x4700] + [ord('W')]*100 + [27, 0x4400]
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    dos(work, [r'UITEST /'+mode+r' D:\LEFT D:\RIGHT', 'echo RETURNED>EXIT.OK'], machine)
    assert (work/'EXIT.OK').exists()
    def position(index): return list(map(int,(work/f'F{index:03d}.POS').read_text().split()))
    assert position(0)[1] == capacity
    assert position(capacity-1)[3:5] == [0,capacity-1], (mode,'last visible row scrolled early')
    assert position(capacity)[3:5] == [1,capacity], (mode,'scroll boundary')
    assert position(capacity+2)[3:5] == [1,capacity], (mode,'pane switching lost selection')
    first = decode(work/'F000.BIN')
    boundary = decode(work/f'F{capacity:03d}.BIN')
    end_index = capacity+5
    assert position(end_index)[3:5] == [41-capacity,40], (mode,'End')
    last = decode(work/f'F{end_index:03d}.BIN')
    assert position(capacity+6)[3:5] == [0,0], (mode,'Home')
    if mode != 'text':
        start, pitch, font_h = {'cga':(21,8,8), 'ega':(33,16,12), 'vga':(37,16,16)}[mode]
        # Each unselected row contains a distinct filename and no overlapping neighbor.
        for row in range(1,capacity):
            text_y = start+row*pitch+(pitch-font_h)//2
            crop = first.crop((40,text_y,150,text_y+font_h))
            assert any(min(pixel)<100 for pixel in crop.getdata()), (mode,'missing row',row)
        y = start+(capacity-1)*pitch+(pitch-font_h)//2
        selected = boundary.crop((40,y,150,y+font_h))
        bg = (0,0,0) if mode=='cga' else palette[1]
        assert sum(pixel==bg for pixel in selected.getdata()) > 200, (mode,'last row clipped')
        mark_y = start+(capacity-2)*pitch+pitch//2
        assert boundary.getpixel((12,mark_y)) == ((0,0,0) if mode=='cga' else palette[14]), (mode,'mark disappeared')
        # A real proportional scrollbar reaches the bottom with End.
        track = capacity*pitch
        thumb = max(8,capacity*track//41)
        x = 6+310-6
        thumb_color = (0,0,0) if mode=='cga' else palette[8]
        assert first.getpixel((x,start)) == thumb_color
        assert last.getpixel((x,start+track-1)) == thumb_color
        assert first.getpixel((x,start+track-1)) != thumb_color
        # Every footer label is a single line at the bottom, including the tenth key.
        footer_y = {'cga':190,'ega':336,'vga':462}[mode]
        assert len(first.crop((0,footer_y,640,first.height)).getcolors(640*20)) >= 2
        # Long command lines stay inside their field and preserve the footer.
        long_frame = decode(work/f'F{capacity+106:03d}.BIN')
        assert long_frame.crop((0,footer_y,640,first.height)).tobytes() == first.crop((0,footer_y,640,first.height)).tobytes()
    baseline_compare(work)
    print('PASS:',mode,capacity,'visible rows; scrolling, marking, pane focus, Home/End, scrollbar and command clipping',flush=True)

# Compare viewport copies with an independent forced-redraw executable, both panes.
compile_dos(base, dos, 'REFBUILD',
    [r'set PATH=C:\TC;C:\TASM',
     r'tcc -DNW_DIAGNOSTICS -DNW_NO_SCROLL -1- -ms -IC:\TC\INCLUDE -c VIDEO.C > REFCOMP.LOG',
     r'tcc -ms -LC:\TC\LIB -eUIREF.EXE MAIN.OBJ CORE.OBJ FSDOS.OBJ VIDEO.OBJ GRAPH.OBJ HEX.OBJ VIEW.OBJ FONT.OBJ > REFLINK.LOG'],
    ['UIREF.EXE','VIDEO.OBJ'], ['REFCOMP.LOG','REFLINK.LOG'])
for machine,mode,capacity in [('cga','cga',18),('ega','ega',16),('vgaonly','vga',24)]:
    work=base/('scroll-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE'); shutil.copy2(base/'UIREF.EXE',work/'UIREF.EXE')
    for side in ('LEFT','RIGHT'):
        for i in range(40): (work/side/f'FILE{i:02d}.TXT').write_bytes(b'scroll sample')
    moves=[0x4700]+[0x5000]*(capacity+4)+[0x4800]*8+[0x4f00,0x4700]
    keys=moves+[9]+moves+[0x5200,ord('+'),ord('-'),9,0x4400]
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    dos(work,[r'UITEST /'+mode+r' D:\LEFT D:\RIGHT','echo RETURNED>EXIT.OK'],machine)
    assert (work/'EXIT.OK').exists()
    captures={f.name:hashlib.sha256(f.read_bytes()).digest() for f in work.glob('F*.BIN')}
    (work/'EXIT.OK').unlink()
    dos(work,[r'UIREF /'+mode+r' D:\LEFT D:\RIGHT','echo RETURNED>EXIT.OK'],machine)
    assert (work/'EXIT.OK').exists()
    assert captures=={f.name:hashlib.sha256(f.read_bytes()).digest() for f in work.glob('F*.BIN')},(mode,'viewport copy differs from redraw')
    print('PASS:',mode,len(captures),'copy/redraw comparisons: both panes, up/down, Home/End, focus and marks',flush=True)

# Viewer no-op keys must neither reread nor touch VRAM, including EOF/empty pages.
for machine, mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga')]:
    work = base/('noop-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    (work/'LEFT/SHORT.TXT').write_bytes(b'A\tB\r\n'+b'W'*90+b'\n'+bytes([0,128,255]))
    (work/'LEFT/ZERO.BIN').write_bytes(b'')
    keys = [0x5000,0x3d00,ord('?'),0x4900,0x4700,0x5100,0x3e00,
            ord('?'),0x4900,0x4800,0x4700,0x4f00,0x5100,27,
            0x4f00,0x5600,0x5100,0x4900,0x4700,0x4f00,ord('?'),0x3e00,ord('?'),27,0x4400]
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    dos(work,[r'UITEST /'+mode+r' D:\LEFT D:\RIGHT','echo RETURNED>EXIT.OK'],machine)
    assert (work/'EXIT.OK').exists()
    def info(index): return list(map(int,(work/f'F{index:03d}.POS').read_text().split()))
    for first,last in [(2,6),(7,13),(16,21),(22,23)]:
        initial = (work/f'F{first:03d}.BIN').read_bytes()
        for index in range(first+1,last+1):
            assert (work/f'F{index:03d}.BIN').read_bytes() == initial,(mode,'no-op changed screen',index)
            assert info(index)[7:9] == info(first)[7:9],(mode,'no-op wrote VRAM or reread',index)
    assert info(7)[8] == info(2)[8]+1 and info(22)[8] == info(16)[8]+1
    print('PASS:',mode,'viewer no-op keys, EOF, empty file, tabs/wrapping, text/hex toggles: zero rereads and VRAM writes',flush=True)

# Matching paths share enumeration, and mkdir/delete reload affected panes only.
for same in (False,True):
    work = base/('refresh-same' if same else 'refresh-distinct')
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    for side in ('LEFT','RIGHT'):
        (work/side/'A.TXT').write_bytes(b'a'); (work/side/'B.TXT').write_bytes(b'b')
    keys = [0x5000,0x5200,ord('+'),ord('-'),0x5200,9,0x5000,0x5200,9,
            0x4100,ord('N'),13,0x4700,0x5000,0x4200,ord('y'),18,0x4400]
    (work/'KEYS.TXT').write_text('\n'.join(f'{k:x}' for k in keys))
    right = 'LEFT' if same else 'RIGHT'
    dos(work,[r'UITEST /text D:\LEFT '+'D:'+chr(92)+right,'echo RETURNED>EXIT.OK'])
    assert (work/'EXIT.OK').exists() and not (work/'LEFT/N').exists()
    states=[list(map(int,f.read_text().split())) for f in sorted(work.glob('F*.POS'))]
    initial = 1 if same else 2
    assert states[0][9] == initial
    assert states[3][10:12] == [2,0] and states[4][10:12] == [0,0]
    assert states[9][10:12] == [1,1],('independent pane marks',same)
    assert states[12][9] == initial+1 and states[12][10:12] == [0,0]
    assert states[16][9] == initial+(2 if same else 3) and states[17][9] == initial+(3 if same else 5)
    print('PASS: shared-directory snapshot' if same else 'PASS: affected-directory refresh', 'independent marks, mark-all/clear, refresh clears marks',flush=True)

# About must restore the desktop and show the same dialog via both entry paths.
version = (root/'VERSION').read_text().strip()
for machine, mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga'),('hercules','text')]:
    work=base/('about-'+machine+'-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    (work/'KEYS.TXT').write_text('6800\n1b\n3b00\n41\nd\n4400\n')
    dos(work,[r'UITEST /'+mode+r' D:\LEFT D:\RIGHT'],machine)
    frames=[(work/f'F{i:03d}.BIN').read_bytes() for i in range(6)]
    assert frames[0]==frames[2]==frames[5],(mode,'About changed desktop')
    assert frames[1]==frames[4] and frames[1]!=frames[0],(mode,'About entry paths differ')
    if mode=='text':
        cells=frames[1][3:4003:2].decode('cp437')
        for caption in ('Nightwatch v'+version,'GPLv3','LICENSE.TXT','Spleen','BSD-2-Clause','FONTLIC.TXT','16pxls','Paul Mackenzie','ICONLIC.TXT'):
            assert caption in cells,(machine,caption)
    else:
        image=decode(work/'F001.BIN')
        assert image.getbbox(),mode
    print('PASS:',machine,mode,'About via Alt-F1 and Help/A; version/licenses and exact desktop restoration',flush=True)

# Query version from the actual executable without opening the UI.
version_work=base/'version-production'
if version_work.exists(): shutil.rmtree(version_work)
version_work.mkdir(); shutil.copy2(root/'build/NIGHT.EXE',version_work/'NIGHT.EXE')
dos(version_work,['NIGHT /version > VERSION.LOG','NIGHT --version > LONGVER.LOG'])
for name in ('VERSION.LOG','LONGVER.LOG'):
    assert (version_work/name).read_text().strip()=='Nightwatch v'+version
print('PASS: production /version and --version agree with VERSION',flush=True)

# Compile a launcher that fills the real BIOS keyboard queue, then starts NIGHT.
# This uses the production executable, without replacing key_read.
(base / 'KEYTEST.C').write_bytes((root / 'tests/KEYTEST.C').read_text().replace('\n', '\r\n').encode('ascii'))
compile_dos(base, dos, 'KEYBUILD', [r'set PATH=C:\TC;C:\TASM', r'tcc -1- -ms -IC:\TC\INCLUDE -LC:\TC\LIB -eKEYTEST.EXE KEYTEST.C > KEYCOMP.LOG'], ['KEYTEST.EXE'], ['KEYCOMP.LOG'])
production = base / 'production'
if production.exists(): shutil.rmtree(production)
production.mkdir(); (production / 'LEFT').mkdir(); (production / 'RIGHT').mkdir(); (production / 'LEFT/SUB').mkdir()
content = b'BIOS keyboard test\r\n'
(production / 'LEFT/ALPHA.TXT').write_bytes(content)
shutil.copy2(base / 'KEYTEST.EXE', production / 'KEYTEST.EXE')
shutil.copy2(root / 'build/NIGHT.EXE', production / 'NIGHT.EXE')
dos(production, ['KEYTEST > RESULT.LOG'])
assert (production / 'RIGHT/ALPHA.TXT').read_bytes() == content
assert 'returned 0' in (production / 'RESULT.LOG').read_text()
print('PASS: production NIGHT.EXE with real BIOS keyboard events; About, F2, Tab, selection, Shift-F3, viewer F4, F5 and F10')

# Check the 8086 instruction target using the real executable and BIOS input.
# DOSBox-X has an explicit 8086 CPU core; ordinary DOSBox does not.
(production / 'RIGHT/ALPHA.TXT').unlink()
(base / 'XT.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
args = ['dosbox-x', '-fastlaunch', '-nogui', '-conf', str(root / 'tools/dosbox.conf'),
        '-conf', str(base / 'XT.CONF'), '-machine', 'cga']
for cmd in [f'mount d "{production}"', 'd:', 'KEYTEST /cga > XTRESULT.LOG', 'exit']:
    args += ['-c', cmd]
with (production / 'XT.LOG').open('w') as log:
    sp.run(args, env=env, stdout=log, stderr=log, check=True, timeout=45)
assert (production / 'RIGHT/ALPHA.TXT').read_bytes() == content
assert 'returned 0' in (production / 'XTRESULT.LOG').read_text()
print('PASS: production NIGHT.EXE on DOSBox-X 8086 CPU + CGA, real BIOS input and file copy')

# Real BIOS input injected during a write: previous commits survive cancellation.
for name in ('KEYCAN.C','CANCEL.ASM'):
    (base/name).write_bytes((root/'tests'/name).read_text().replace('\n','\r\n').encode('ascii'))
compile_dos(base,dos,'CANBUILD',
    [r'set PATH=C:\TC;C:\TASM','tasm /mx CANCEL.ASM > CANASM.LOG',
     r'tcc -1- -ms -IC:\TC\INCLUDE -LC:\TC\LIB -eKEYCAN.EXE KEYCAN.C CANCEL.OBJ > CANCOMP.LOG'],
    ['KEYCAN.EXE','CANCEL.OBJ'],['CANASM.LOG','CANCOMP.LOG'])
cancel_work=base/'cancel-production'
if cancel_work.exists(): shutil.rmtree(cancel_work)
cancel_work.mkdir(); (cancel_work/'LEFT').mkdir(); (cancel_work/'RIGHT').mkdir()
first=b'committed before cancellation\r\n'; second=bytes(range(256))*100
(cancel_work/'LEFT/FIRST.TXT').write_bytes(first)
(cancel_work/'LEFT/SECOND.BIN').write_bytes(second)
for name in ('NIGHT.EXE','KEYCAN.EXE'):
    shutil.copy2(root/'build/NIGHT.EXE' if name=='NIGHT.EXE' else base/name,cancel_work/name)
args=['dosbox-x','-fastlaunch','-nogui','-conf',str(root/'tools/dosbox.conf'),
      '-conf',str(base/'XT.CONF'),'-machine','cga']
for cmd in [f'mount d "{cancel_work}"','d:','KEYCAN /cga > RESULT.LOG','exit']: args+=['-c',cmd]
with (cancel_work/'DOSBOX.LOG').open('w') as log:
    sp.run(args,env=env,stdout=log,stderr=log,check=True,timeout=45)
assert 'returned 0; injected 1' in (cancel_work/'RESULT.LOG').read_text()
assert (cancel_work/'RIGHT/FIRST.TXT').read_bytes()==first
assert (cancel_work/'LEFT/SECOND.BIN').read_bytes()==second
assert not (cancel_work/'RIGHT/SECOND.BIN').exists()
assert not list((cancel_work/'RIGHT').glob('*.TMP'))
print('PASS: production 8086 BIOS Escape behind another key during transfer; current temporary removed, prior commit preserved',flush=True)
