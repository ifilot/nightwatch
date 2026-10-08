# SPDX-License-Identifier: GPL-3.0-only
"""Decode actual emulator VRAM captures into standalone PNG screenshots."""
from PIL import Image

palette = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
           (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
def decode(path, output=None):
    raw = path.read_bytes(); mode = raw[0]; h = int.from_bytes(raw[1:3], 'little'); data = raw[3:]
    im = Image.new('RGB', (640, h)); px = im.load()
    if mode == 0:
        assert len(data) == 6048
        for y in range(h):
            for x in range(640):
                at = ((y // 8) * 80 + x // 8) * 2
                ch, attr = data[at:at+2]
                bit = data[4000 + ch * 8 + y % 8] & (128 >> (x % 8))
                px[x,y] = palette[attr & 15 if bit else (attr >> 4) & 15]
    else:
        planes = 1 if mode == 1 else 4
        assert len(data) == h * 80 * planes
        for y in range(h):
            for x in range(640):
                index = y * 80 + x // 8; mask = 128 >> (x % 8)
                color = sum((1 << p) for p in range(planes) if data[p * h * 80 + index] & mask)
                px[x,y] = (255,255,255) if planes == 1 and color else palette[color]
    im.save(output if output is not None else path.with_suffix('.png'))
    return im

