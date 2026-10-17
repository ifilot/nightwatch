# SPDX-License-Identifier: GPL-3.0-only
"""Draw original 16x16 icon studies using the application's EGA palette."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from vram import palette

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'assets/icons-v130'
NAMES = ('Folder', 'Parent', 'Document', 'Binary', 'Program', 'Archive', 'Image', 'Drive')


def quickmenu_icon(name):
    """Redraw the reference motifs on the final grid, without scaling bitmaps."""
    im = Image.new('RGBA', (16, 16))
    d = ImageDraw.Draw(im)
    black, white, gray, dark = (palette[i] for i in (0,15,7,8))
    if name in ('Folder', 'Parent'):
        # COLOR035: broad yellow body, right-hand tab, shallow black contour.
        d.polygon([(1,5),(8,5),(8,3),(13,3),(14,4),(14,12),(1,12)],
                  fill=palette[14], outline=black)
        d.line((2,6,13,6), fill=white)
        d.line((2,13,14,13), fill=dark)
        if name == 'Parent':
            # COLOR066's upward arrow, adapted to the directory silhouette.
            d.polygon([(7,0),(3,4),(5,4),(5,9),(9,9),(9,4),(11,4)],
                      fill=white, outline=black)
            d.line((7,3,7,8), fill=palette[12])
            d.line((6,3,8,3), fill=palette[12])
    elif name == 'Document':
        # COLOR001: an offset sheet, gray text, and a small turned corner.
        d.rectangle((4,3,14,14), fill=gray, outline=black)
        d.polygon([(1,1),(11,1),(11,10),(8,13),(1,13)], fill=white, outline=black)
        d.line([(8,12),(8,10),(10,10)], fill=black)
        for y, end in ((3,9),(5,8),(7,9),(9,6),(11,5)):
            d.line((3,y,end,y), fill=dark)
    elif name == 'Binary':
        # COLOR028/061: recognizable disk media, with a clean white label.
        d.polygon([(2,1),(12,1),(14,3),(14,14),(2,14)], fill=gray, outline=black)
        d.rectangle((4,2,10,5), fill=dark, outline=black)
        d.line((8,3,8,4), fill=white)
        d.rectangle((4,8,12,13), fill=white, outline=black)
        d.line((6,10,10,10), fill=gray)
    elif name == 'Program':
        # COLOR042/043: a window with a bright work area and spare text marks.
        d.rectangle((0,1,14,14), fill=gray, outline=black)
        d.rectangle((1,2,13,4), fill=palette[1])
        d.point((2,3), fill=white)
        d.line((11,3,12,3), fill=white)
        d.line((1,5,13,5), fill=black)
        d.rectangle((1,6,13,13), fill=palette[11])
        d.rectangle((1,6,5,8), fill=white, outline=black)
        for y, end in ((10,9),(12,7)):
            d.line((3,y,end,y), fill=black)
    elif name == 'Archive':
        # COLOR031/032: two drawers distinguish the container from a drive.
        d.polygon([(3,0),(12,0),(14,3),(14,14),(1,14),(1,3)], fill=dark, outline=black)
        d.line((3,1,12,1), fill=white)
        for y in (4,9):
            d.rectangle((3,y,12,y+3), fill=gray, outline=black)
            d.line((6,y+1,9,y+1), fill=white)
            d.line((6,y+2,9,y+2), fill=black)
    elif name == 'Image':
        # COLOR015: sky, hills and a small red subject, rather than paper.
        d.rectangle((0,2,14,13), fill=palette[11], outline=black)
        d.line((2,4,5,4), fill=white)
        d.point((12,4), fill=palette[14])
        d.polygon([(1,10),(5,6),(9,10),(12,7),(13,9),(13,12),(1,12)], fill=palette[2])
        d.polygon([(1,11),(5,9),(8,11),(13,10),(13,12),(1,12)], fill=palette[10])
        d.rectangle((8,9,11,12), fill=palette[12])
        d.line([(7,9),(9,7),(12,9)], fill=palette[4])
        d.point((9,11), fill=black)
    else:
        # COLOR063/064: gray top plane and a dark front slot, with red activity.
        d.polygon([(3,5),(11,5),(14,8),(14,12),(0,12),(0,8)], fill=gray, outline=black)
        d.line((3,6,10,6), fill=white)
        d.line((1,8,13,8), fill=white)
        d.line((1,9,13,9), fill=dark)
        d.line((3,10,9,10), fill=black)
        d.point((12,10), fill=palette[12])
        d.line((1,13,14,13), fill=dark)
    return im


def icon(name, style):
    if style == 'quickmenu':
        return quickmenu_icon(name)
    im = Image.new('RGBA', (16, 16))
    d = ImageDraw.Draw(im)
    ink, white, gray = palette[0], palette[15], palette[7]
    accent = palette[1]
    if name in ('Folder', 'Parent'):
        d.polygon([(1,4),(2,3),(6,3),(8,5),(14,5),(14,13),(1,13)], fill=palette[6], outline=ink)
        d.polygon([(1,7),(14,7),(13,13),(0,13)], fill=palette[14], outline=ink)
        d.line((2,8,12,8), fill=white)
        if name == 'Parent':
            d.polygon([(8,5),(4,9),(6,9),(6,12),(10,12),(10,9),(12,9)], fill=accent, outline=ink)
    elif name in ('Document', 'Binary', 'Image'):
        d.polygon([(3,1),(9,1),(12,4),(12,14),(3,14)], fill=white, outline=ink)
        d.line([(9,1),(9,4),(12,4)], fill=ink)
        if name == 'Document':
            for y, end in ((6,10),(8,10),(10,10),(12,8)):
                d.line((5,y,end,y), fill=accent)
        elif name == 'Binary':
            for x in (4,8):
                d.rectangle((x,7,x+2,11), outline=ink)
            d.line((9,7,9,11), fill=white)
        else:
            d.rectangle((4,6,11,12), fill=accent, outline=ink)
            d.polygon([(5,11),(7,8),(9,10),(10,9),(10,11)], fill=palette[10])
            d.point((10,7), fill=palette[14])
    elif name == 'Program':
        d.rectangle((1,2,14,13), fill=gray, outline=ink)
        d.rectangle((2,3,13,5), fill=accent)
        d.rectangle((3,7,12,11), fill=white, outline=ink)
        d.line([(5,8),(6,9),(5,10)], fill=ink)
        d.line((8,10,10,10), fill=ink)
    elif name == 'Archive':
        d.rectangle((2,4,13,13), fill=palette[6], outline=ink)
        d.rectangle((1,2,14,5), fill=palette[14], outline=ink)
        d.line((3,3,12,3), fill=white)
        d.rectangle((6,6,9,11), fill=gray, outline=ink)
        d.line((7,8,8,8), fill=ink)
    else:
        d.polygon([(3,4),(12,4),(14,9),(14,13),(1,13),(1,9)], fill=gray, outline=ink)
        d.rectangle((2,9,13,12), fill=gray, outline=ink)
        d.line((4,10,9,10), fill=ink)
        d.point((12,11), fill=palette[10])
    return im


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    font = ImageFont.load_default()
    sheet = Image.new('RGB', (1040, 520), '#e7e7e7')
    d = ImageDraw.Draw(sheet)
    d.text((20,12), 'Nightwatch 1.3.0 - original 16x16 icon studies (enlarged 4x)', font=font, fill='black')
    for row, style in enumerate(('windows', 'quickmenu')):
        top = 48 + row * 232
        d.text((20,top), 'Windows 3.x inspired' if style == 'windows' else 'Quick Menu inspired', font=font, fill='black')
        for col, name in enumerate(NAMES):
            im = icon(name, style)
            assert im.size == (16,16)
            assert all(p[3] in (0,255) and (not p[3] or p[:3] in palette) for p in im.getdata())
            im.save(OUT / f'{style}-{name.lower()}.png')
            x = 20 + col * 128
            d.text((x,top+24), name, font=font, fill='black')
            for y, bg in ((top+44,15),(top+116,1)):
                tile = Image.new('RGBA',(16,16),(*palette[bg],255))
                tile.alpha_composite(im)
                sheet.paste(tile.convert('RGB').resize((64,64),Image.Resampling.NEAREST),(x,y))
                sheet.paste(tile.convert('RGB'),(x+76,y+24))
    d.text((20,506), 'White and blue backgrounds; 4x and native size. Preview only; application artwork unchanged.', font=font, fill='black')
    sheet.save(OUT / 'comparison.png')
    # Keep a focused sheet for reviewing the chosen size and reference style.
    focused = Image.new('RGB',(1040,300),'#e7e7e7')
    fd = ImageDraw.Draw(focused)
    fd.text((20,12),'VGA: Quick Menu references redrawn at 16x16; 4x and native size',font=font,fill='black')
    focused.paste(sheet.crop((0,280,1040,482)),(0,42))
    fd.text((20,268),'White and blue background studies. Original artwork; preview only.',font=font,fill='black')
    focused.save(OUT / 'quickmenu-preview.png')


if __name__ == '__main__':
    main()
