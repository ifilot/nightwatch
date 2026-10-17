# SPDX-License-Identifier: GPL-3.0-only
"""Refresh all documentation images from current emulator captures.

Run make screenshots to regenerate the gallery and every required UI fixture.
"""
from pathlib import Path
import json
import os
import shutil
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(root / 'tests'))
from ui_build import require_prebuilt, source_key
from vram import decode

require_prebuilt()
manifest = json.loads((root / 'screenshots/manifest.json').read_text())
assert manifest['renderer_source_key'] == source_key(), 'Regenerate the VGA gallery first'
destination = root / 'docs/screenshots'
sources = {
    'text': 'build/video/cga-text/F000.png',
    'cga': 'build/video/cga-cga/F000.png',
    'ega': 'build/video/ega-ega/F000.png',
    'vga': 'screenshots/01-work-and-backup-drives.png',
    'text-viewer': 'screenshots/29-text-viewer.png',
    'hex': 'screenshots/34-binary-hex-viewer.png',
    'about': 'screenshots/24-about.png',
    'help-cga': 'build/video/cga-cga/F001.png',
    'help-ega': 'build/video/ega-ega/F001.png',
    'help-vga': 'screenshots/20-help-first-page.png',
    'copy-cga': 'build/doc-copy-cga/copy.png',
    'copy-ega': 'build/doc-copy-ega/copy.png',
    'copy-vga': 'screenshots/copy-progress.png',
    'drive-picker': 'build/drive-dos/vgaonly-vga/picker.png',
    'drive-headers': 'build/demo-test/vga-s.png',
    'archive-browser': 'build/browse-dos/vga-zip/root.png',
    'batch-conflict': 'build/workflows/vga-captures/conflict.png',
    'file-properties': 'build/workflows/vga-captures/readonly.png',
    'large-directory': 'build/workflows/vga-captures/page3.png',
    'recursive-search': 'build/workflows/vga-captures/match.png',
    'transfer-retry': 'build/workflows/vga-captures/error-retry.png',
}

# The gallery provides a real transfer at VGA resolution. Repeat its isolated
# scenario for CGA/EGA so those images also show actual speed and ETA values.
for mode, machine in [('cga', 'cga'), ('ega', 'ega')]:
    work = root / ('build/doc-copy-' + mode)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir()
    shutil.copytree(root / 'build/vga-screenshots/samples', work / 'drives')
    shutil.copy2(root / 'build/video/UITEST.EXE', work / 'UITEST.EXE')
    shutil.copy2(root / 'build/vga-screenshots/copy/KEYS.TXT', work / 'KEYS.TXT')
    args = ['dosbox', '-conf', str(root / 'tools/dosbox.conf'), '-machine', machine]
    for command in [f'mount c "{work}"', f'mount d "{work}/drives/D"',
                    f'mount e "{work}/drives/E"', 'c:',
                    f'UITEST /{mode} D:\\DATA E:\\', 'exit']:
        args += ['-c', command]
    with (work / 'DOSBOX.LOG').open('w') as log:
        subprocess.run(args, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
                       stdout=log, stderr=log, check=True, timeout=120)
    states = [(path, list(map(int, path.read_text().split()))) for path in work.glob('P*.TXT')]
    partial = [(path, state) for path, state in states
               if not state[0] and state[6] and 0 < state[5] < state[6] and state[9] > 0]
    assert partial, (mode, 'No in-flight copy with a measured transfer speed')
    capture, _ = min(partial, key=lambda pair: abs(pair[1][5] / pair[1][6] - 0.5))
    decode(capture.with_suffix('.BIN'), work / 'copy.png')

# Verify every input before replacing any checked-in documentation image.
for source in sources.values():
    if not (root / source).is_file():
        raise SystemExit(f'Missing {source}; run make screenshots first')
for name, source in sources.items():
    shutil.copyfile(root / source, destination / (name + '.png'))
print(f'Updated {len(sources)} documentation screenshots from emulator VRAM')
