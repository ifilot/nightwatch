# SPDX-License-Identifier: GPL-3.0-only
"""Draw VGA modal icon proposals directly on the application's 16x16 grid."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from vram import palette

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/modal-icon-studies'
ITEMS = (
    ('Help', 'Book and question mark', 'Help reference'),
    ('About', 'Blue information badge', 'Version and credits'),
    ('Search', 'Magnifying glass', 'Filename and viewer searches'),
    ('Properties', 'Document and wrench', 'File and folder attributes'),
    ('Copy', 'Two overlapping pages', 'Copy destination and progress'),
    ('Move', 'Page with an arrow', 'Move destination and progress'),
    ('Delete', 'Paper feeding through a shredder', 'Delete confirmation and progress'),
    ('Warning', 'Yellow warning triangle', 'Conflicts and transfer failures'),
    ('Storage test', 'Disk with a checkmark', 'Storage test menu and dashboard'),
    ('Display settings', 'Color-bar monitor', 'Display mode selection'),
)


def artwork(name):
    im = Image.new('RGBA', (16, 16))
    d = ImageDraw.Draw(im)
    black, white, gray, dark = (palette[i] for i in (0, 15, 7, 8))

    def page(box):
        d.rectangle(box, fill=white, outline=black)
        x, y, right, bottom = box
        for at in range(y + 3, bottom - 1, 2):
            d.line((x + 2, at, right - 2, at), fill=dark)

    if name == 'Help':
        d.polygon([(1,2),(10,1),(13,3),(13,14),(3,14),(1,12)], fill=palette[4], outline=black)
        d.rectangle((3,2,12,12), fill=palette[12], outline=black)
        d.line((4,3,11,3), fill=white)
        d.line((3,13,11,13), fill=white)
        d.line([(6,5),(9,5),(10,6),(10,7),(8,8),(8,9)], fill=white)
        d.point((8,11), fill=white)
    elif name == 'About':
        d.ellipse((1,1,14,14), fill=gray, outline=black)
        d.ellipse((2,2,13,13), fill=palette[9])
        d.line([(3,5),(3,4),(5,2)], fill=white)
        d.rectangle((7,4,8,5), fill=white)
        d.line((6,7,8,7), fill=white)
        d.rectangle((7,7,8,10), fill=white)
        d.line((6,11,9,11), fill=white)
    elif name == 'Search':
        d.line((9,9,14,14), fill=black, width=4)
        d.line((10,10,13,13), fill=palette[6], width=2)
        d.ellipse((1,1,11,11), fill=gray, outline=black)
        d.ellipse((3,3,9,9), fill=palette[11], outline=dark)
        d.line([(4,6),(4,4),(6,4)], fill=white)
    elif name == 'Properties':
        d.polygon([(2,1),(9,1),(12,4),(12,14),(2,14)], fill=white, outline=black)
        d.line([(9,1),(9,4),(12,4)], fill=dark)
        d.line((4,4,6,4), fill=dark)
        d.line((4,6,7,6), fill=dark)
        # Open jaw at the upper right, handle crossing the page.
        d.line((6,13,12,7), fill=black, width=4)
        d.line((6,12,11,7), fill=gray, width=2)
        d.polygon([(10,3),(12,2),(11,5),(13,6),(15,3),(15,7),(12,9),(9,6)], fill=gray, outline=black)
        d.point((6,12), fill=black)
    elif name == 'Copy':
        page((5,1,14,11))
        page((1,4,10,14))
        d.line((3,6,8,6), fill=palette[1])
    elif name == 'Move':
        page((1,1,10,14))
        d.polygon([(8,6),(11,6),(11,3),(15,8),(11,12),(11,9),(8,9)], fill=palette[10], outline=black)
        d.line((9,7,12,7), fill=white)
    elif name == 'Delete':
        # Intact sheet above the slot, separate strips below the cutter.
        d.polygon([(4,0),(10,0),(12,2),(12,7),(4,7)], fill=white, outline=black)
        d.line([(10,0),(10,2),(12,2)], fill=dark)
        d.line((6,3,9,3), fill=dark)
        d.line((6,5,10,5), fill=dark)
        for x, end in ((4,14),(7,15),(10,13)):
            d.rectangle((x,11,x+2,end), fill=black)
            d.line((x+1,11,x+1,end), fill=white)
        d.rectangle((1,7,14,11), fill=gray, outline=black)
        d.line((2,8,13,8), fill=white)
        d.line((4,9,11,9), fill=black)
        d.point((13,10), fill=palette[12])
    elif name == 'Warning':
        d.polygon([(7,0),(15,14),(0,14)], fill=palette[14], outline=black)
        d.line((7,3,2,12), fill=white)
        d.rectangle((7,5,8,9), fill=black)
        d.rectangle((7,11,8,12), fill=black)
    elif name == 'Storage test':
        d.polygon([(3,2),(11,2),(14,5),(14,11),(1,11),(1,5)], fill=gray, outline=black)
        d.line((4,3,10,3), fill=white)
        d.line((2,6,13,6), fill=white)
        d.line((3,8,8,8), fill=black)
        d.polygon([(7,10),(9,12),(13,7),(15,9),(9,15),(5,12)], fill=palette[10], outline=black)
        d.line([(7,11),(9,13),(13,9)], fill=palette[10])
    elif name == 'Display settings':
        d.rectangle((1,1,14,11), fill=gray, outline=black)
        d.rectangle((3,3,12,9), fill=black)
        for x,color in ((4,12),(6,10),(8,9),(10,14)):
            d.rectangle((x,4,x+1,8), fill=palette[color])
        d.line((6,12,9,12), fill=dark)
        d.rectangle((4,13,11,14), fill=gray, outline=black)
    else:
        raise ValueError(name)
    assert all(p[3] in (0,255) and (not p[3] or p[:3] in palette) for p in im.getdata())
    return im


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new('RGB', (1280, 1240), '#edf1f6')
    d = ImageDraw.Draw(sheet)
    fonts = {n: ImageFont.truetype('DejaVuSans.ttf', n) for n in (15,17,20,24,34)}

    def text(x,y,value,size=17,color='#23334b'):
        d.text((x,y),value,font=fonts[size],fill=color)

    d.rectangle((0,0,1280,130), fill='#172435')
    text(30,22,'Nightwatch · VGA modal icons',34,'white')
    text(32,78,'16×16 originals · 16-color palette · transparent backgrounds · approved designs',20,'#d8e4f2')
    for i,(name,motif,usage) in enumerate(ITEMS):
        x,y = 24+(i%2)*628, 150+(i//2)*206
        d.rounded_rectangle((x,y,x+604,y+190),radius=12,fill='white')
        text(x+18,y+12,f'{i+1:02d}  {name}',24)
        text(x+18,y+49,motif,17,'#536276')
        sprite=artwork(name)
        sprite.save(OUT/(name.lower().replace(' ','-')+'.png'))
        for offset,bg,label in ((18,1,'Title bar · 5×'),(140,15,'White · 5×')):
            tile=Image.new('RGBA',(16,16),(*palette[bg],255))
            tile.alpha_composite(sprite)
            sheet.paste(tile.convert('RGB').resize((80,80),Image.Resampling.NEAREST),(x+offset,y+80))
            text(x+offset,y+166,label,15,'#536276')
        d.rectangle((x+270,y+83,x+582,y+117),fill=palette[1])
        sheet.paste(sprite,(x+279,y+92),sprite)
        text(x+305,y+90,name,17,'white')
        text(x+270,y+126,'Native 1× title-bar sample',15,'#536276')
        text(x+270,y+153,usage,15,'#536276')
    text(32,1190,'Original pixel artwork · GPL-3.0-only · Approved VGA modal artwork; embedded by tools/assets.py.',17,'#536276')
    output=ROOT/'docs/modal-icon-proposals.png'
    sheet.save(output,optimize=True)
    print(output)


if __name__ == '__main__':
    main()
