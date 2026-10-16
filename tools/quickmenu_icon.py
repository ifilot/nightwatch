#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build the original Nightwatch 32x32 EGA Quick Menu icon and ICC mask."""
from pathlib import Path
import struct
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/quickmenu'
EGA = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),
       (170,0,0),(170,0,170),(170,85,0),(170,170,170),
       (85,85,85),(85,85,255),(85,255,85),(85,255,255),
       (255,85,85),(255,85,255),(255,255,85),(255,255,255)]
# ICC numbers the four color planes in reverse EGA index-bit order.
ICC_INDEX = [0,8,4,12,2,10,6,14,1,9,5,13,3,11,7,15]
CLEAR = 13  # Unused magenta palette entry carries PNG transparency.
SIZE = 32
HEADER = struct.pack('<HH', SIZE - 1, SIZE - 1)


def artwork():
    im=Image.new('P',(32,32),CLEAR)
    im.putpalette([v for color in EGA for v in color])
    im.info['transparency']=bytes(0 if i==CLEAR else 255 for i in range(16))
    d=ImageDraw.Draw(im)
    # Use the full canvas: one-pixel black structure, bright working areas.
    d.rectangle((0,0,30,30),fill=0)
    d.line((1,31,31,31,31,0),fill=7)
    d.rectangle((4,1,25,4),fill=9)
    for x in (1,27):
        d.rectangle((x,1,x+2,3),fill=15)
        d.line((x+2,1,x+2,3,x,3),fill=7)
        d.point((x+1,2),fill=0)
    # One flat toolbar: three sparse marks, not miniature widgets.
    d.rectangle((1,6,29,9),fill=7)
    for x,end in [(3,6),(10,15),(20,26)]:
        d.line((x,7,end,7),fill=0)
        d.line((x,8,end-1,8),fill=15)
    # Twin cyan panes with explicit divider and shallow path headers.
    for left,right in [(1,14),(16,29)]:
        d.rectangle((left,11,right,29),fill=11)
        d.rectangle((left,11,right,13),fill=15)
        d.line((left+2,12,left+6,12),fill=0)
    # Slightly irregular black marks suggest rows, rather than large glyphs.
    for x,y,w in [(3,16,3),(7,16,5),(3,19,5),(9,19,3),
                  (3,22,2),(6,22,4),(3,25,4),(8,25,3),(3,28,3),
                  (18,16,4),(24,16,3),(18,19,3),(22,19,3),
                  (18,22,5),(25,22,2),(18,25,3),(22,25,4),(18,28,4)]:
        d.line((x,y,x+w-1,y),fill=0)
    # One selected entry makes the active pane clear at native size.
    d.rectangle((2,18,13,20),fill=1)
    d.line((3,19,7,19),fill=15)
    d.line((9,19,11,19),fill=15)
    return im


def plane_block(values):
    data = bytearray(HEADER)
    for y in range(SIZE):
        for plane in range(4):
            for group in range(4):
                data.append(sum(((values[y*SIZE+group*8+x] >> plane) & 1)
                                << (7-x) for x in range(8)))
    return data


def unpack(block):
    assert block[:4] == HEADER
    pixels = []
    for y in range(SIZE):
        for x in range(SIZE):
            pixels.append(sum(((block[4+y*16+plane*4+x//8] >> (7-x%8)) & 1)
                              << plane for plane in range(4)))
    return pixels


def main():
    OUT.mkdir(exist_ok=True)
    im = artwork()
    pixels = list(im.getdata())
    colors = [0 if i == CLEAR else ICC_INDEX[i] for i in pixels]
    mask = [0 if i == CLEAR else 15 for i in pixels]
    data = plane_block(colors) + plane_block(mask)
    assert len(data) == 1032
    assert unpack(data[:516]) == colors
    assert unpack(data[516:]) == mask
    (OUT/'NIGHT.ICC').write_bytes(data)
    im.save(OUT/'NIGHT.PNG', bits=4)
    saved = Image.open(OUT/'NIGHT.PNG')
    assert saved.size == (32,32) and saved.mode == 'P'
    assert list(saved.getdata()) == pixels
    raw = (OUT/'NIGHT.PNG').read_bytes()
    assert raw[24:26] == bytes([4,3]), 'PNG must be 4-bit indexed color'
    assert set(saved.convert('RGBA').getdata()) <= {
        (*color, 0 if i == CLEAR else 255) for i,color in enumerate(EGA)}
    # Nearest-neighbor previews on representative Quick Menu backgrounds.
    preview = Image.new('RGB',(660,278),EGA[0])
    draw = ImageDraw.Draw(preview)
    for col, (label, bg) in enumerate([('BLUE',1),('GRAY',7),('BLACK',0)]):
        x = col*220
        draw.rectangle((x,0,x+219,239),fill=EGA[bg])
        bgim = Image.new('RGBA',(32,32),(*EGA[bg],255))
        bgim.alpha_composite(saved.convert('RGBA'))
        preview.paste(bgim.convert('RGB').resize((192,192),Image.Resampling.NEAREST),(x+14,10))
        draw.text((x+14,213),label+' / 6x',fill=EGA[15] if bg!=7 else EGA[0])
        preview.paste(bgim.convert('RGB'),(x+14,243))
        draw.text((x+55,252),'32 x 32 / 1x',fill=EGA[15])
    preview.save(OUT/'preview.png')
    print('PASS: 32x32; 4-bit EGA PNG; binary transparency; 1032-byte ICC; both planes round-trip')
    print('Opaque palette indices:',sorted(set(pixels)-{CLEAR}))


if __name__ == '__main__':
    main()
