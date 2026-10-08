# SPDX-License-Identifier: GPL-3.0-only
"""Render a discussion sheet from the exact generated DOS icon masks."""
from pathlib import Path
import json
import re
from PIL import Image, ImageDraw, ImageFont
from vram import palette

root = Path(__file__).resolve().parents[1]
source = (root/'src/ASSETS.H').read_text()
symbols = json.loads((root/'assets/16pxls/mapping.json').read_text())
names = [name.lower() for name, index in re.findall(r'#define ICON_(\w+) (\d+)', source)]
tables = {name: bytes(int(value, 16) for value in re.findall(r'0x([0-9a-f]{2})', data))
          for name, data in re.findall(r'static const unsigned char (\w+)\[\d+\] = \{(.*?)\};', source, re.S)}
descriptions = {
    'folder': ('Directory',), 'parent': ('Parent directory', '..'),
    'document': ('Other extensions', 'or no extension'),
    'binary': ('.BIN  .DAT  .SYS',), 'program': ('.EXE  .COM  .BAT',),
    'archive': ('.ZIP  .ARJ  .LZH',), 'image': ('.GIF  .PCX  .BMP',),
    'drive': ('Panel path caption', 'Inactive / active states'),
}


def icon(name, mode, selected=False, help_icon=False):
    prefix, width, height = {'VGA': ('icon_',16,16), 'EGA': ('icon_',16,16), 'CGA': ('cga_',16,8)}[mode]
    mask = tables[prefix+name]
    assert len(mask) == 3 * height * 2
    if mode == 'CGA':
        bg = 0 if selected or help_icon else 15
        body, edge, light = bg, 15 if bg == 0 else 0, 15 if bg == 0 else 0
    elif name == 'drive':
        bg = 1 if selected or help_icon else 8
        body, edge, light = bg, 15, 15
    else:
        bg = 1 if selected else 15
        body, edge, light = bg, 15 if selected else 0, 15 if selected else 0
    bitmap = Image.new('RGB',(width,height),palette[bg])
    # video_icon paints outline, body, then highlight; zero bits are transparent.
    for layer, color in enumerate((edge,body,light)):
        for y in range(height):
            at = layer * height * 2 + y * 2
            bits = (mask[at] << 8) | mask[at+1]
            for x in range(width):
                if bits & (0x8000 >> x): bitmap.putpixel((x,y),palette[color])
    return bitmap, bg


def mark(mono, selected):
    bg = (0 if selected else 15) if mono else (1 if selected else 15)
    bitmap = Image.new('RGB',(4,4) if mono else (6,6),palette[bg])
    painter = ImageDraw.Draw(bitmap)
    if mono:
        painter.rectangle((0,0,3,3),fill=palette[15 if selected else 0])
    else:
        painter.rectangle((0,0,5,5),outline=palette[15 if selected else 6])
        painter.rectangle((1,1,4,4),fill=palette[14])
    return bitmap,bg


width, height = 1640, 1560
sheet = Image.new('RGB',(width,height),'#eef1f5')
draw = ImageDraw.Draw(sheet)
fonts = {size:ImageFont.truetype('DejaVuSans.ttf',size) for size in (14,16,18,22,36)}
ink, muted, border = '#172435','#536276','#d9e0e8'


def text(x,y,value,size=16,color=ink):
    draw.text((x,y),value,font=fonts[size],fill=color)


def centered(cx,y,value,size=16,color=ink):
    bounds = draw.textbbox((0,0),value,font=fonts[size])
    text(cx-(bounds[2]-bounds[0])/2,y,value,size,color)


def preview(x,y,w,h,bitmap,bg,zoom=5):
    draw.rectangle((x,y,x+w-1,y+h-1),fill=palette[bg],outline=border)
    # Preserve the actual square pixel grid, including CGA's 16x8 shape.
    native_x, large_x = x+35, x+w-65
    cy = y+(h-30)//2
    sheet.paste(bitmap,(native_x-bitmap.width//2,cy-bitmap.height//2))
    large = bitmap.resize((bitmap.width*zoom,bitmap.height*zoom),Image.Resampling.NEAREST)
    sheet.paste(large,(large_x-large.width//2,cy-large.height//2))
    fg = '#ffffff' if bg in (0,1,8) else muted
    centered(native_x,y+h-26,'1×',14,fg)
    centered(large_x,y+h-26,f'{zoom}×',14,fg)


draw.rectangle((0,0,width,144),fill='#172435')
text(36,24,'Nightwatch · Icon overview',36,'#ffffff')
text(36,80,'All eight icons, using the exact masks and colors drawn by the program.',18,'#dce6f1')
text(36,109,'Native size (1×) beside crisp pixel enlargement (5×). Numbers identify icons for discussion.',16,'#bdcddd')
left, column_width, row_height, first_y = 288, 214, 136, 216
for group,(mode,dimensions) in enumerate((('VGA','16 × 16'),('EGA','16 × 16'),('CGA','16 × 8'))):
    cx = left+(group*2+1)*column_width
    centered(cx,160,f'{mode}  ·  {dimensions} pixels',22)
    centered(cx-column_width/2,192,'Normal / inactive',14,muted)
    centered(cx+column_width/2,192,'Selected / active',14,muted)
for row,name in enumerate(names):
    y = first_y+row*row_height
    draw.rounded_rectangle((24,y,width-24,y+row_height-10),radius=10,fill='white')
    text(40,y+19,f'{row+1:02d}  {name.title()}',22)
    for line,value in enumerate(descriptions[name]): text(80,y+57+line*23,value,16,muted)
    text(40,y+105,'VGA/EGA: '+symbols[name],14,muted)
    for group,mode in enumerate(('VGA','EGA','CGA')):
        for selected in (False,True):
            bitmap,bg = icon(name,mode,selected)
            x = left+(group*2+int(selected))*column_width+8
            preview(x,y+7,column_width-16,row_height-24,bitmap,bg)

footer_y = first_y+len(names)*row_height+14
text(36,footer_y,'Additional appearances and selection marks',22)
items = [(f'{mode} Help drive',*icon('drive',mode,help_icon=True)) for mode in ('VGA','EGA','CGA')]
items += [(label,*mark(mono,selected)) for label,mono,selected in (
    ('Color mark',False,False),('Selected color mark',False,True),
    ('CGA mark',True,False),('Selected CGA mark',True,True))]
for i,(label,bitmap,bg) in enumerate(items):
    x = 36+i*224
    text(x,footer_y+38,label,14,muted)
    preview(x,footer_y+65,208,102,bitmap,bg)
text(36,1515,'VGA/EGA: 16pxls by Paul Mackenzie · CC-BY-SA-4.0 · https://16pxls.com/ | CGA shown in raw pixel proportions.',14,muted)
output = root/'docs/icon-overview.png'
sheet.save(output,optimize=True)
print(output)
