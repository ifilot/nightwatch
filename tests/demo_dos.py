# SPDX-License-Identifier: GPL-3.0-only
"""Exercise demo drive identities, VGA header pixels and persistent DOS media."""
from pathlib import Path
import os
import shutil
import subprocess as sp
import sys

from ui_build import require_prebuilt, build_ui

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
from prepare_demo import prepare_media, build_launcher
from vram import decode
from drive_icons import drive_icon
from vram import palette

try:
    require_prebuilt()
except (AssertionError, FileNotFoundError):
    build_ui()
work = ROOT / 'build/demo-test'
if work.exists():
    shutil.rmtree(work)
launcher = work / 'launcher'
# Every run exercises the same cold start as a fresh Actions matrix worker.
build_launcher(launcher)
media = work / 'demo'
prepare_media(media)
shutil.copy2(launcher / 'DEMODRV.EXE', media / 'DEMODRV.EXE')
shutil.copy2(ROOT / 'build/video/UITEST.EXE', work / 'UITEST.EXE')
(work / 'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
keys = [0x4300, 27]
frames = {'initial': 0, 'picker': 1}
for drive in 'abs':
    keys += [0x4300, ord(drive), 13]
    frames[drive] = len(keys)
# Revisit A: after B: and S:; the first panel must reuse its earlier sample.
keys += [0x4300, ord('a'), 13, 0x4300, ord('s'), 13]
# Visit S: from the second panel as well, then return it to E:.
keys += [9]
frames['inactive-s'] = len(keys)
keys += [0x4300, ord('s'), 13, 0x4300, ord('e'), 13]
frames['revisited'] = len(keys)
keys += [18]
frames['space'] = len(keys)
keys += [0x4400]
(work / 'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys) + '\n')

(work / 'WRITES.BAT').write_bytes(b'@echo off\r\necho Persistent floppy A>A:\\KEEP.TXT\r\necho Persistent floppy B>B:\\KEEP.TXT\r\necho Persistent SD>S:\\KEEP.TXT\r\n')

for mode in ('text', 'vga'):
    for path in work.glob('F*.*'):
        if path.suffix in ('.BIN', '.POS'):
            path.unlink()
    args = ['dosbox', '-conf', str(ROOT / 'tools/dosbox.conf'),
            '-conf', str(work / 'FAST.CONF')]
    commands = [f'mount c "{work}"',
                f'imgmount a "{media / "A.IMG"}" -t floppy -fs fat',
                f'imgmount b "{media / "B.IMG"}" -t floppy -fs fat',
                f'mount d "{media / "D"}"', f'mount e "{media / "E"}"',
                f'mount s "{media / "S"}"', 'c:',
                f'DEMO\\DEMODRV UITEST.EXE /{mode} D:\\ E:\\',
                'call WRITES.BAT', 'exit']
    for command in commands:
        args += ['-c', command]
    with (work / 'DOSBOX.LOG').open('w') as log:
        sp.run(args, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
               stdout=log, stderr=log, check=True, timeout=90)
    assert len(list(work.glob('F*.BIN'))) == len(keys), 'Demo launcher or UI stopped early'
    states = [list(map(int, path.read_text().split())) for path in sorted(work.glob('F*.POS'))]
    assert states[0][12] == 2, 'Startup samples D: and E: once each'
    assert [states[frames[d]][12] for d in 'abs'] == [3,4,5], 'Each new drive gets one sample'
    assert states[frames['inactive-s']][12] == states[frames['revisited']][12] == 5, 'Returning to drives or using the other panel reuses samples'
    assert states[-1][12] == 7, 'Ctrl-R resamples S: and E: once each'
    if mode == 'text':
        def screen(name):
            return (work / f'F{frames[name]:03}.BIN').read_bytes()[3:4003:2].decode('cp437')
        picker = screen('picker')
        assert '3.5-inch floppy' in picker and '5.25-inch floppy' in picker, picker
        assert 'Slot-otter SD' in picker, picker
        for drive in 'abs':
            assert drive.upper() + ':\\' in screen(drive)[160:200], screen(drive)
            assert 'README.TXT' in screen(drive), screen(drive)
    for name, index in frames.items():
        image = decode(work / f'F{index:03}.BIN', work / f'{mode}-{name}.png')
        kinds = {'initial': 'hard-disk', 'a': 'floppy-35', 'b': 'floppy-525',
                 's': 'sd-card', 'inactive-s': 'sd-card'}
        if mode == 'vga' and name in kinds:
            left, right = (8, 1) if name == 'inactive-s' else (1, 8)
            for x, sprite, background in [(8, drive_icon(kinds[name]), left),
                                           (332, drive_icon('hard-disk'), right)]:
                for sy in range(16):
                    for sx in range(16):
                        pixel = sprite.getpixel((sx, sy))
                        expected = pixel[:3] if pixel[3] else palette[background]
                        assert image.getpixel((x+sx, 2+sy)) == expected, (name, x, sx, sy)

before = {drive: (media / (drive + '.IMG')).read_bytes() for drive in 'AB'}
(media / 'S/README.TXT').write_bytes(b'Edited SD readme\r\n')
prepare_media(media)
for drive in 'AB':
    assert (media / (drive + '.IMG')).read_bytes() == before[drive], 'Floppy overwritten'
    saved = sp.check_output(['mtype', '-i', str(media / (drive + '.IMG')), '::KEEP.TXT'])
    assert b'Persistent floppy' in saved, saved
assert (media / 'S/README.TXT').read_bytes() == b'Edited SD readme\r\n'
assert b'Persistent SD' in (media / 'S/KEEP.TXT').read_bytes()
print('PASS: demo drives, text/VGA browsing, active/inactive header sprites and persistent writes')
