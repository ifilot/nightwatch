# SPDX-License-Identifier: GPL-3.0-only
"""Refresh README images from successful make test-video emulator captures."""
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[1]
base = root / 'build/video'
destination = root / 'docs/screenshots'
sources = {
    'text': 'cga-text/F000.png',
    'cga': 'cga-cga/F000.png',
    'ega': 'ega-ega/F000.png',
    'vga': 'vgaonly-vga/F000.png',
    'hex': 'hex.png',
    'about': 'about-vgaonly-vga/F001.png',
    'help-cga': 'cga-cga/F001.png',
    'help-ega': 'ega-ega/F001.png',
    'help-vga': 'vgaonly-vga/F001.png',
    'copy-cga': 'copy-cga.png',
    'copy-ega': 'copy-ega.png',
    'copy-vga': 'copy-vga.png',
}
# Verify every input before changing any checked-in image.
for source in sources.values():
    if not (base / source).is_file():
        raise SystemExit(f'Missing {base / source}; run make test-video first')
for name, source in sources.items():
    shutil.copyfile(base / source, destination / (name + '.png'))
print(f'Updated {len(sources)} README screenshots from emulator VRAM')
