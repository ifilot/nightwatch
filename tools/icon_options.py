# SPDX-License-Identifier: GPL-3.0-only
"""Generate original, editable pixel-icon candidates without changing the app."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
from vram import palette

root = Path(__file__).resolve().parents[1]
destination = root/'assets/icon-options'
destination.mkdir(exist_ok=True)


def canvas():
    bitmap = Image.new('L',(16,16),0)
    return bitmap,ImageDraw.Draw(bitmap)


def page(draw, folded=True):
    if folded:
        draw.polygon([(3,1),(9,1),(12,4),(12,14),(3,14)],fill=2,outline=1)
        draw.line([(9,1),(9,4),(12,4)],fill=1)
    else:
        draw.rectangle((3,1,12,14),fill=2,outline=1)


def folder(draw):
    draw.polygon([(1,6),(3,6),(3,4),(7,4),(9,6),(14,6),(14,13),(1,13)],fill=2,outline=1)
    draw.line((2,7,13,7),fill=3)


def glyph(draw,x,y,rows,color=3):
    for dy,row in enumerate(rows):
        for dx,value in enumerate(row):
            if value=='1': draw.point((x+dx,y+dy),fill=color)


zero=('111','101','101','101','111')
one=('010','110','010','010','111')
entries=[]
for index,title in enumerate(('Folded classic','Fine rules','Double sheet','Side margin','Flat page'),1):
    bitmap,draw=canvas()
    if index==3:
        draw.rectangle((1,3,10,15),fill=2,outline=1)
        draw.rectangle((4,0,13,12),fill=2,outline=1)
        for y,end in ((4,11),(6,11),(8,9)): draw.line((6,y,end,y),fill=3)
    else:
        page(draw,folded=index!=5)
        if index==1:
            for y,end in ((6,10),(8,10),(10,8)): draw.rectangle((5,y,end,y+1),fill=3)
        elif index==2:
            for y,end in ((6,10),(8,9),(10,10),(12,7)): draw.line((5,y,end,y),fill=3)
        elif index==4:
            draw.line((5,5,5,12),fill=3)
            for y,end in ((6,10),(9,10),(12,9)): draw.line((7,y,end,y),fill=1)
        else:
            draw.rectangle((5,3,10,4),fill=3)
            for y,end in ((7,10),(9,10),(11,8)): draw.line((5,y,end,y),fill=1)
    entries.append((f'D{index}','Document',title,bitmap))

for index,title in enumerate(('Byte grid','Large 01','Hex byte','Chip on paper','Binary ticks'),1):
    bitmap,draw=canvas(); page(draw)
    if index==1:
        for x,y,w in ((5,6,2),(9,6,1),(5,10,1),(8,10,3)):
            draw.rectangle((x,y,x+w-1,y+1),fill=3)
    elif index==2:
        glyph(draw,4,7,zero); glyph(draw,8,7,one)
    elif index==3:
        glyph(draw,4,7,('010','101','111','101','101'))
        glyph(draw,8,7,('111','100','111','001','111'))
    elif index==4:
        draw.rectangle((6,7,9,11),fill=2,outline=3)
        for y in (8,10):
            draw.point((5,y),fill=3); draw.point((10,y),fill=3)
        draw.point((7,6),fill=3); draw.point((8,12),fill=3)
    else:
        for x,top,bottom in ((5,6,8),(7,7,8),(9,6,8),(5,11,12),(7,10,12),(10,10,11)):
            draw.line((x,top,x,bottom),fill=3)
    entries.append((f'B{index}','Binary',title,bitmap))

for index,title in enumerate(('Slim arrow','Broad arrow','Return arrow','Raised arrow','DOS dot badge'),1):
    bitmap,draw=canvas(); folder(draw)
    if index==1:
        draw.polygon([(7,1),(3,5),(6,5),(6,10),(8,10),(8,5),(11,5)],fill=3,outline=1)
    elif index==2:
        draw.polygon([(7,6),(3,10),(6,10),(6,12),(9,12),(9,10),(12,10)],fill=3)
    elif index==3:
        draw.line([(11,11),(6,11),(6,7)],fill=3,width=2)
        draw.polygon([(6,5),(3,8),(9,8)],fill=3)
    elif index==4:
        draw.polygon([(11,0),(7,4),(9,4),(9,9),(12,9),(12,4),(15,4)],fill=3,outline=1)
    else:
        draw.rectangle((5,10,6,11),fill=1); draw.rectangle((9,10,10,11),fill=1)
    entries.append((f'P{index}','Parent',title,bitmap))


def render(mask,family,selected=False,mono=False,transparent=False):
    bg=0 if mono and selected else 1 if selected else 15
    if mono:
        output=Image.new('RGB',(16,8),palette[bg])
        for y in range(8):
            for x in range(16):
                values=(mask.getpixel((x,2*y)),mask.getpixel((x,2*y+1)))
                value=next((layer for layer in (1,2,3) if layer in values),0)
                if value: output.putpixel((x,y),palette[bg if value==2 else 15 if selected else 0])
        return output,bg
    colors=(palette[0],palette[6 if family=='Parent' else 7 if selected else 8],
            palette[14 if family=='Parent' else 15],palette[15 if family=='Parent' else 11 if selected else 1])
    output=Image.new('RGBA' if transparent else 'RGB',(16,16),(0,0,0,0) if transparent else palette[bg])
    for y in range(16):
        for x in range(16):
            value=mask.getpixel((x,y))
            if value: output.putpixel((x,y),colors[value]+(255,) if transparent else colors[value])
    return output,bg


sheet=Image.new('RGB',(1440,1310),'#eef1f5')
draw=ImageDraw.Draw(sheet)
fonts={size:ImageFont.truetype('DejaVuSans.ttf',size) for size in (14,16,18,22,34)}


def label(x,y,value,size=16,color='#172435'):
    draw.text((x,y),value,font=fonts[size],fill=color)


def center(x,y,value,size=14,color='#536276'):
    box=draw.textbbox((0,0),value,font=fonts[size]); label(x-(box[2]-box[0])/2,y,value,size,color)


draw.rectangle((0,0,1440,138),fill='#172435')
label(32,22,'Nightwatch · Icon alternatives',34,'white')
label(32,75,'Five choices each for Document, Binary and Parent. White paper for the file icons.',18,'#dce6f1')
label(32,108,'Use D1–D5, B1–B5 and P1–P5 to choose. Current program icons are unchanged.',16,'#bdcddd')
for group,family in enumerate(('Document','Binary','Parent')):
    y=164+group*368
    label(32,y,family,22)
    for column,(identifier,_,title,mask) in enumerate(item for item in entries if item[1]==family):
        x=32+column*278
        draw.rounded_rectangle((x,y+38,x+264,y+350),radius=10,fill='white')
        label(x+12,y+49,f'{identifier}  {title}',18)
        for selected in (False,True):
            bx=x+12+int(selected)*126
            bitmap,bg=render(mask,family,selected)
            draw.rectangle((bx,y+82,bx+114,y+205),fill=palette[bg],outline='#d9e0e8')
            large=bitmap.resize((96,96),Image.Resampling.NEAREST)
            sheet.paste(large,(bx+9,y+88))
            sheet.paste(bitmap,(bx+12,y+187))
            center(bx+72,y+186,'1× / 6×',14,'white' if selected else '#536276')
            center(bx+57,y+210,'Selected' if selected else 'Normal')
            small,bg=render(mask,family,selected,mono=True)
            draw.rectangle((bx,y+239,bx+114,y+287),fill=palette[bg],outline='#d9e0e8')
            sheet.paste(small.resize((64,32),Image.Resampling.NEAREST),(bx+25,y+247))
        center(x+138,y+299,'CGA conversion · 4×')
label(32,1280,'Editable 16×16 masks and transparent PNGs: assets/icon-options/ · CGA shown in raw pixel proportions.',14,'#536276')
for identifier,family,title,mask in entries:
    rows=[''.join('.Xo+'[mask.getpixel((x,y))] for x in range(16)) for y in range(16)]
    (destination/(identifier+'.txt')).write_text('\n'.join(rows)+'\n',encoding='ascii')
    render(mask,family,transparent=True)[0].save(destination/(identifier+'.png'))
(destination/'README.md').write_text(
    '# Icon candidates\n\nThese are review alternatives; they are not used by NW.EXE.\n'
    'Each numbered candidate has an editable 16×16 mask and a transparent PNG.\n'
    '`X` is outline, `o` is body, `+` is detail/highlight, `.` is transparent.\n'
    'File bodies use white; parent folders use yellow. The comparison sheet\n'
    'shows normal/selected colors and the existing CGA conversion rule.\n'
    'Compact EGA variants can be drawn after a choice is made.\n\n'
    'All candidate artwork is original and GPL-3.0-only.\n\n'
    'Regenerate with `python3 tools/icon_options.py`.\n\n'
    + '\n'.join(f'- **{identifier}**: {family} — {title}' for identifier,family,title,_ in entries)+'\n')
output=root/'docs/icon-options.png'
sheet.save(output,optimize=True)
print(output)
