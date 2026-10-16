#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Five native pixel-art directions for the Quick Menu launcher icon."""
from pathlib import Path
from zipfile import ZipFile
from PIL import Image, ImageDraw, ImageFont
from quickmenu_icon import EGA, CLEAR, ICC_INDEX, HEADER, plane_block, unpack

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'assets/quickmenu/options'


def canvas():
    im = Image.new('P',(32,32),CLEAR)
    im.putpalette([v for rgb in EGA for v in rgb])
    im.info['transparency'] = bytes(0 if i==CLEAR else 255 for i in range(16))
    return im, ImageDraw.Draw(im)


def folder(d,x,y,color,highlight):
    d.polygon([(x,y+3),(x,y),(x+7,y),(x+10,y+3),(x+20,y+3),
               (x+20,y+17),(x,y+17)],fill=0)
    d.polygon([(x+1,y+4),(x+1,y+1),(x+6,y+1),(x+9,y+4),
               (x+19,y+4),(x+19,y+16),(x+1,y+16)],fill=color)
    d.line((x+2,y+5,x+18,y+5),fill=highlight)


def commander():
    im,d = canvas()
    d.rectangle((2,4,31,29),fill=0)
    d.rectangle((0,2,29,27),fill=15)
    d.line((0,27,29,27,29,2),fill=8)
    d.rectangle((1,3,28,26),fill=1)
    d.line((14,4,14,25),fill=7)
    d.line((2,6,27,6),fill=7)
    d.rectangle((2,9,12,12),fill=3)
    for left in (3,17):
        for y,length in [(10,7),(15,9),(19,6),(23,8)]:
            d.line((left,y,left+length-1,y),fill=15 if y==10 else 7)
    d.point((4,4),fill=14);d.point((18,4),fill=14)
    return im


def folders():
    im,d=canvas()
    folder(d,2,3,1,11)
    folder(d,9,12,6,14)
    # A broad yellow face reads as one front folder, rather than file rows.
    d.rectangle((10,18,28,27),fill=14)
    d.line((10,28,28,28),fill=6)
    return im


def monogram():
    im,d=canvas()
    d.polygon([(4,1),(27,1),(30,4),(30,27),(27,30),(4,30),(1,27),(1,4)],fill=0)
    d.polygon([(5,2),(26,2),(29,5),(29,26),(26,29),(5,29),(2,26),(2,5)],fill=1)
    d.line((5,2,26,2,29,5),fill=11)
    d.line((2,5,2,26),fill=11)
    glyphs=[(5,15,['10001','11001','11001','10101','10011','10011','10001']),
            (17,11,['10001','10001','10001','10101','10101','10101','01010'])]
    for ox,color,rows in glyphs:
        for y,row in enumerate(rows):
            for x,bit in enumerate(row):
                if bit=='1':d.rectangle((ox+x*2,9+y*2,ox+x*2+1,10+y*2),fill=color)
    return im


def transfer():
    im,d=canvas()
    for x,y in [(2,3),(19,12)]:
        d.polygon([(x,y),(x+7,y),(x+10,y+3),(x+10,y+15),(x,y+15)],fill=0)
        d.polygon([(x+1,y+1),(x+6,y+1),(x+9,y+4),(x+9,y+14),(x+1,y+14)],fill=15)
        d.polygon([(x+6,y+1),(x+6,y+4),(x+9,y+4)],fill=7)
    # One broad rightward arrow is the only detail between the two pages.
    d.polygon([(10,8),(19,8),(19,4),(27,11),(19,18),(19,14),(10,14)],fill=0)
    d.polygon([(11,9),(20,9),(20,7),(25,11),(20,15),(20,13),(11,13)],fill=14)
    return im


def foldermark():
    im,d=canvas()
    folder(d,5,7,6,14)
    d.rectangle((6,13,24,23),fill=14)
    # A single broad N rather than small text or miniature file glyphs.
    d.rectangle((10,13,12,22),fill=1)
    d.rectangle((18,13,20,22),fill=1)
    d.polygon([(12,13),(18,19),(18,22),(12,16)],fill=1)
    return im


DESIGNS=[('Commander',commander),('Folder pair',folders),('NW badge',monogram),
         ('File transfer',transfer),('Folder monogram',foldermark)]


def font(size):
    path=Path('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf')
    return ImageFont.truetype(str(path),size) if path.exists() else ImageFont.load_default()


def main():
    OUT.mkdir(parents=True,exist_ok=True)
    sheet=Image.new('RGB',(1200,410),'#101722');d=ImageDraw.Draw(sheet)
    d.text((22,13),'Nightwatch / Quick Menu icon studies',font=font(24),fill='white')
    d.text((22,49),'32 x 32 pixels  /  fixed 16-color EGA palette  /  enlarged 6x and native 1x',font=font(15),fill='#b0bdce')
    for i,(name,draw) in enumerate(DESIGNS,1):
        im=draw();pixels=list(im.getdata())
        colors=[0 if p==CLEAR else ICC_INDEX[p] for p in pixels]
        mask=[0 if p==CLEAR else 15 for p in pixels]
        data=plane_block(colors)+plane_block(mask)
        assert len(data)==1032 and data[:4]==HEADER and data[516:520]==HEADER
        assert unpack(data[:516])==colors and unpack(data[516:])==mask
        stem=f'NIGHT{i}'
        im.save(OUT/(stem+'.PNG'),bits=4)
        (OUT/(stem+'.ICC')).write_bytes(data)
        png=(OUT/(stem+'.PNG')).read_bytes();assert png[24:26]==bytes([4,3])
        saved=Image.open(OUT/(stem+'.PNG'));assert saved.size==(32,32)
        assert list(saved.getdata())==pixels
        assert set(saved.convert('RGBA').getdata())<={(*rgb,0 if n==CLEAR else 255) for n,rgb in enumerate(EGA)}
        x=(i-1)*240
        d.text((x+16,86),f'{i}. {name}',font=font(18),fill='white')
        bg=Image.new('RGBA',(32,32),(*EGA[8],255));bg.alpha_composite(im.convert('RGBA'))
        sheet.paste(bg.convert('RGB').resize((192,192),Image.Resampling.NEAREST),(x+16,119))
        for j,index in enumerate((1,7,0)):
            bg=Image.new('RGBA',(32,32),(*EGA[index],255));bg.alpha_composite(im.convert('RGBA'))
            sheet.paste(bg.convert('RGB'),(x+20+j*65,335))
        d.text((x+16,380),'Blue / Gray / Black at 1x',font=font(13),fill='#b0bdce')
        print(f'PASS {stem}: {name}, {len(set(pixels)-{CLEAR})} colors; PNG and ICC verified')
    sheet.save(OUT/'comparison.png')
    with ZipFile(OUT/'ICONS.ZIP','w') as z:
        for i in range(1,6):
            for ext in ('ICC','PNG'):
                path=OUT/f'NIGHT{i}.{ext}';z.write(path,path.name)


if __name__=='__main__':main()
