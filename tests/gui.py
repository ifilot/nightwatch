# SPDX-License-Identifier: GPL-3.0-only
"""Real keyboard/display smoke test, run under xvfb-run."""
from pathlib import Path
import subprocess as sp
import time
import os
import shutil
from PIL import Image
root = Path(__file__).resolve().parents[1]
work = root / 'build' / 'gui'
work.mkdir(exist_ok=True)
shutil.copy2(root / 'build/NIGHT.EXE', work / 'NIGHT.EXE')
(work / 'LEFT').mkdir(exist_ok=True)
(work / 'RIGHT').mkdir(exist_ok=True)
(work / 'LEFT/ALPHA.TXT').write_text('Hello from Nightwatch.\r\n' * 70)
(work / 'LEFT/BETA.BIN').write_bytes(bytes(range(256)) * 80)
(work / 'LEFT/SUB').mkdir(exist_ok=True)

def key(window, *keys):
    sp.run(['xdotool', 'key', '--window', window, '--clearmodifiers', *keys], check=True)
    time.sleep(.5)

def shot(window, label):
    path = work / (label + '.png')
    sp.run(['import', '-window', window, str(path)], check=True)
    im = Image.open(path).convert('RGB')
    colors = im.getcolors(im.width * im.height)
    assert len(colors) > 1, (label, 'blank screen')
    assert sum(n for n, rgb in colors if max(rgb) > 140) > 1500, (label, 'no visible glyphs')
    print(label, im.size, len(colors), 'colors', flush=True)

for machine, mode in [('cga', 'text'), ('cga', 'cga'), ('ega', 'ega'), ('vgaonly', 'vga'), ('hercules', 'text')]:
    label = machine + '-' + mode
    marker = work / 'EXIT.OK'
    marker.unlink(missing_ok=True)
    log = (work / (label + '.log')).open('w')
    p = sp.Popen(['dosbox', '-conf', str(root / 'tools/dosbox.conf'), '-machine', machine,
                  '-c', f'mount d "{work}"', '-c', 'd:', '-c', f'NIGHT /{mode} D:\\LEFT D:\\RIGHT',
                  '-c', 'echo RETURNED>EXIT.OK', '-c', 'exit'], stdout=log, stderr=log, env=dict(os.environ, SDL_AUDIODRIVER='dummy'))
    try:
        time.sleep(3)
        window = sp.check_output(['xdotool', 'search', '--name', 'DOSBox']).decode().splitlines()[-1]
        time.sleep(2)
        shot(window, label)
        key(window, 'Tab', 'F1')
        shot(window, label + '-help')
        key(window, 'Escape', 'Tab', 'End', 'Home', 'Down', 'Down')
        key(window, 'F3')
        shot(window, label + '-view')
        key(window, 'Next', 'Prior', 'Escape', 'F10')
        p.wait(timeout=10)
        assert marker.exists(), (label, 'did not exit normally')
    finally:
        if p.poll() is None:
            p.kill(); p.wait(timeout=5)
        log.close()
print('PASS: CGA text and graphics, EGA, VGA, monochrome text; keyboard/view/help/exit')
