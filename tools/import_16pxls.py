# SPDX-License-Identifier: GPL-3.0-only
"""Convert selected upstream 16pxls PNGs into the shared VGA/EGA masks."""
from pathlib import Path
import json
from PIL import Image

root = Path(__file__).resolve().parents[1]
source = root/'assets/16pxls'
for name, symbol in json.loads((source/'mapping.json').read_text()).items():
    image = Image.open(source/(symbol+'.png')).convert('RGBA')
    assert image.size == (16,16), symbol
    rows = [''.join('X' if image.getpixel((x,y))[3] >= 128 else '.'
                    for x in range(16)) for y in range(16)]
    (root/'assets/icons'/(name+'.txt')).write_text('\n'.join(rows)+'\n',encoding='ascii')
print('Imported eight original 16pxls symbols for VGA and EGA; CGA unchanged')
