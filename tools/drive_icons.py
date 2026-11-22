# SPDX-License-Identifier: GPL-3.0-only
"""Original drive-icon study matching Nightwatch's 16x16 VGA artwork.

The asset compiler embeds these sprites in the VGA F9 drive picker and pane headers.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from vram import palette

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT/'assets/drive-icons'
DRIVES = (
    ('floppy-35', '3.5" floppy', 'Metal shutter + clipped corner'),
    ('floppy-525', '5.25" floppy', 'Dark jacket + exposed centre hub'),
    ('hard-disk', 'Hard drive', 'Metal case + platter and head'),
    ('sd-card', 'SD card', 'Cut corner + gold contacts'),
)


def drive_icon(kind):
    im = Image.new('RGBA', (16,16))
    d = ImageDraw.Draw(im)
    black, white, gray, dark = [palette[n] for n in (0,15,7,8)]
    if kind == 'floppy-35':
        d.polygon([(2,1),(11,1),(14,4),(14,14),(1,14),(1,2)], fill=palette[9], outline=black)
        d.line((2,2,2,13), fill=palette[11])
        d.rectangle((4,2,10,6), fill=gray, outline=black)
        d.line((5,2,9,2), fill=white)
        d.rectangle((8,3,9,5), fill=dark)
        d.rectangle((4,9,11,13), fill=white, outline=black)
        d.line((5,11,9,11), fill=gray)
        d.point((12,13), fill=black)
    elif kind == 'floppy-525':
        d.rectangle((1,1,14,14), fill=dark, outline=black)
        d.line((2,2,13,2), fill=gray)
        d.line((2,2,2,13), fill=gray)
        d.rectangle((4,2,11,5), fill=white, outline=black)
        d.line((5,3,9,3), fill=gray)
        d.ellipse((5,7,10,12), fill=gray, outline=black)
        d.rectangle((7,9,8,10), fill=black)
        d.rectangle((7,12,8,13), fill=black)
        d.rectangle((13,5,14,7), fill=black)
    elif kind == 'hard-disk':
        d.polygon([(3,1),(12,1),(14,3),(14,14),(1,14),(1,3)], fill=gray, outline=black)
        d.line((3,2,11,2), fill=white)
        d.line((2,4,2,11), fill=white)
        d.rectangle((2,12,13,13), fill=dark)
        d.line((4,13,10,13), fill=black)
        d.point((12,13), fill=palette[10])
        d.ellipse((4,3,11,10), fill=white, outline=dark)
        d.ellipse((6,5,9,8), fill=gray)
        d.rectangle((7,6,8,7), fill=black)
        d.line([(12,4),(11,5),(9,8),(8,8)], fill=black)
        d.point((12,4), fill=white)
        d.point((3,11), fill=black)
        d.point((12,11), fill=black)
    elif kind == 'sd-card':
        d.polygon([(4,1),(10,1),(14,5),(14,14),(3,14),(3,2)], fill=dark, outline=black)
        d.line((4,3,4,13), fill=gray)
        for x in (5,7,9):
            d.line((x,2,x,5), fill=palette[14])
            d.point((x,5), fill=palette[6])
        d.rectangle((5,8,12,12), fill=white)
        d.line((5,8,12,8), fill=palette[11])
        # Tiny authored S/D letterforms, with no font rasterisation.
        for dx, rows in ((5,('111','100','111','001','111')),
                         (9,('110','101','101','101','110'))):
            for y, row in enumerate(rows):
                for x, pixel in enumerate(row):
                    if pixel == '1': d.point((dx+x,8+y), fill=black)
    else:
        raise ValueError(kind)
    return im


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    fonts = {n:ImageFont.truetype('DejaVuSans.ttf',n) for n in (12,14,16,23,30)}
    sheet = Image.new('RGB', (1160,650), '#ededed')
    d = ImageDraw.Draw(sheet)
    d.rectangle((0,0,1159,99), fill=palette[1])
    d.text((28,16), 'Nightwatch / Drive icons', font=fonts[30], fill='white')
    d.text((30,62), '16 x 16 pixels  /  VGA 16-color palette  /  native size and 6x enlargement', font=fonts[14], fill=palette[11])
    left, width = 164, 244
    for column, (kind,title,detail) in enumerate(DRIVES):
        sprite = drive_icon(kind)
        assert sprite.size == (16,16)
        assert all(p[3] in (0,255) and (not p[3] or p[:3] in palette) for p in sprite.getdata())
        sprite.save(OUT/f'{kind}.png')
        x = left+column*width
        d.text((x+12,119), title, font=fonts[23], fill='#111111')
        d.text((x+12,154), detail, font=fonts[12], fill='#555555')
        for row, bg in enumerate((15,1,8)):
            y = 190+row*137
            tile = Image.new('RGBA',(16,16),(*palette[bg],255))
            tile.alpha_composite(sprite)
            d.rectangle((x+2,y,x+width-10,y+124), fill=palette[bg])
            sheet.paste(tile.convert('RGB').resize((96,96),Image.Resampling.NEAREST),(x+23,y+14))
            sheet.paste(tile.convert('RGB'),(x+168,y+46))
    for row, (label,description) in enumerate((('File list','White'),('Selected','Blue'),('Pane header','Gray'))):
        y = 221+row*137
        d.text((25,y),label,font=fonts[16],fill='#111111')
        d.text((25,y+25),description,font=fonts[14],fill='#555555')
    d.text((28,618),'Original pixel artwork used by the VGA F9 drive picker and pane headers.',font=fonts[14],fill='#555555')
    sheet.save(OUT/'preview.png')
    print(OUT/'preview.png')


if __name__ == '__main__':
    main()
