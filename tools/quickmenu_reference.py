#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Original two-pane draft informed by Quick Menu COLOR042/043's visual style."""
from pathlib import Path
from PIL import Image, ImageDraw
from quickmenu_icon import EGA, CLEAR, ICC_INDEX, plane_block, unpack

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'assets/quickmenu/reference'


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


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    im=artwork();pixels=list(im.getdata())
    colors=[0 if p==CLEAR else ICC_INDEX[p] for p in pixels]
    mask=[0 if p==CLEAR else 15 for p in pixels]
    data=plane_block(colors)+plane_block(mask)
    assert len(data)==1032
    assert unpack(data[:516])==colors and unpack(data[516:])==mask
    (OUT/'NIGHT.ICC').write_bytes(data)
    im.save(OUT/'NIGHT.PNG',bits=4)
    assert (OUT/'NIGHT.PNG').read_bytes()[24:26]==bytes([4,3])
    assert list(Image.open(OUT/'NIGHT.PNG').getdata())==pixels
    sheet=Image.new('RGB',(660,278),EGA[0]);d=ImageDraw.Draw(sheet)
    for col,(label,bg) in enumerate([('BLUE',1),('GRAY',7),('BLACK',0)]):
        x=col*220
        d.rectangle((x,0,x+219,239),fill=EGA[bg])
        tile=Image.new('RGBA',(32,32),(*EGA[bg],255));tile.alpha_composite(im.convert('RGBA'))
        sheet.paste(tile.convert('RGB').resize((192,192),Image.Resampling.NEAREST),(x+14,10))
        d.text((x+14,213),label+' / 6x',fill=EGA[15] if bg!=7 else EGA[0])
        sheet.paste(tile.convert('RGB'),(x+14,243))
        d.text((x+55,252),'32 x 32 / 1x',fill=EGA[15])
    sheet.save(OUT/'preview.png')
    print('PASS: native 32x32, 4-bit EGA PNG, 1032-byte ICC; color and mask round-trip')


if __name__=='__main__':main()
