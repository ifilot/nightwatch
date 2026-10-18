# SPDX-License-Identifier: GPL-3.0-only
"""Render modal assignments from docs/modals.md using the embedded icon assets."""
from pathlib import Path
import re
from PIL import Image, ImageDraw, ImageFont
from vram import palette

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'src/ASSETS.H').read_text()
tables = {
    name: bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', data))
    for name, data in re.findall(
        r'static const unsigned char (\w+)\[\d+\] = \{(.*?)\};', source, re.S)
}
groups = {name: [] for name in ('Help', 'About', 'Search', 'Properties', 'Copy', 'Move', 'Delete', 'Warning', 'Storage', 'Display', 'Document', 'Drive', 'Folder', 'Image', 'Archive')}
for line in (ROOT / 'docs/modals.md').read_text().splitlines():
    cells = [cell.strip() for cell in line.split('|')]
    if len(cells) == 5:
        for name in cells[3].split(' / '):
            if name in groups:
                groups[name].append(cells[1])
groups['Image'].append('Minefield (G from About; additional window)')


def icon(name, mode):
    """Match video_icon's foreground, layers and title-strip background."""
    custom = {'help':'help', 'about':'about', 'search':'search', 'properties':'properties',
              'copy':'copy', 'move':'move', 'delete':'delete', 'warning':'warning',
              'storage':'storage-test', 'display':'display-settings'}
    if name in custom:
        with Image.open(ROOT/'assets/modal-icon-studies'/(custom[name]+'.png')) as stored:
            rgba = stored.convert('RGBA')
        bitmap = Image.new('RGBA', (16,16), (*palette[1],255))
        bitmap.alpha_composite(rgba)
        return bitmap.convert('RGB')
    height = 8 if mode == 'CGA' else 16
    bitmap = Image.new('RGB', (16, height), palette[0 if mode == 'CGA' else 1])
    if mode == 'VGA':
        sprite = tables['vga_' + name]
        for y in range(16):
            for x in range(16):
                at, bit = y * 2 + x // 8, 128 >> (x % 8)
                if sprite[at] & bit:
                    color = sum(1 << p for p in range(4) if sprite[32 + p * 32 + at] & bit)
                    bitmap.putpixel((x, y), palette[color])
    else:
        mask = tables[('cga_' if mode == 'CGA' else 'icon_') + name]
        for layer, color in enumerate((15, 0, 15) if mode == 'CGA' else (15,)):
            for y in range(height):
                bits = int.from_bytes(mask[layer * height * 2 + y * 2:][:2], 'big')
                for x in range(16):
                    if bits & (0x8000 >> x):
                        bitmap.putpixel((x, y), palette[color])
    return bitmap


fonts = {n: ImageFont.truetype('DejaVuSans.ttf', n) for n in (15, 17, 19, 24, 36)}
row_heights = {name: max(174, 82 + ((len(items) + 1) // 2) * 27)
               for name, items in groups.items()}
sheet = Image.new('RGB', (1600, 160 + sum(row_heights.values()) + 118), '#edf1f6')
draw = ImageDraw.Draw(sheet)


def text(x, y, value, size=19, fill='#1c2b40'):
    draw.text((x, y), value, font=fonts[size], fill=fill)


draw.rectangle((0, 0, 1600, 130), fill='#172435')
text(32, 22, 'Nightwatch · Modal icon assignments', 36, 'white')
text(34, 78, 'Current VGA title-bar artwork · native size and nearest-neighbor enlargement (4×)', 19, '#d8e4f2')
y = 150
for name, items in groups.items():
    height = row_heights[name]
    draw.rounded_rectangle((24, y, 1576, y + height - 14), radius=12, fill='white')
    text(44, y + 15, name, 24)
    text(438, y + 20, f'{len(items)} modal flow' + ('s' if len(items) != 1 else '') if name != 'Image' else 'Additional game window', 17, '#536276')
    sprite = icon(name.lower(), 'VGA')
    for x, scale in ((52, 1), (196, 4)):
        text(x, y + 55, f'VGA · {scale}×', 17, '#536276')
        draw.rectangle((x, y + 82, x + 92, y + 145), fill=palette[1])
        enlarged = sprite.resize((16*scale, 16*scale), Image.Resampling.NEAREST)
        sheet.paste(enlarged, (x + (92-enlarged.width)//2, y + 82 + (64-enlarged.height)//2))
    rows = (len(items) + 1) // 2
    for index, item in enumerate(items):
        column, row = divmod(index, rows)
        text(438 + column * 555, y + 58 + row * 27, item, 19)
    y += height
text(34, y + 6, 'EGA and CGA retain their existing icons. Text mode has no title-bar icons.', 17, '#536276')
text(34, y + 35, 'Original VGA artwork: GPL-3.0-only · see assets/modal-icon-studies/README.md', 15, '#536276')
text(34, y + 61, 'Source: MAIN.C, GRAPH.C, STORUI.C · Full modal list and shortcuts: docs/modals.md', 15, '#536276')
output = ROOT / 'docs/modal-icon-overview.png'
sheet.save(output, optimize=True)
print(output)
