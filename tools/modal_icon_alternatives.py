# SPDX-License-Identifier: GPL-3.0-only
"""Compare the current Properties/Display proposals with two new options each."""
from PIL import Image, ImageDraw, ImageFont
from modal_icon_studies import ROOT, OUT, artwork
from vram import palette


def alternative(name):
    im = Image.new('RGBA', (16,16))
    d = ImageDraw.Draw(im)
    black, white, gray, dark = (palette[i] for i in (0,15,7,8))
    if name == 'properties-sliders':
        d.rectangle((1,1,14,14), fill=gray, outline=black)
        d.line((2,2,13,2), fill=white)
        for y, x in ((4,5),(8,10),(12,7)):
            d.line((3,y,12,y), fill=black)
            d.rectangle((x-1,y-1,x+1,y+1), fill=white, outline=black)
            d.point((x,y), fill=palette[11])
    elif name == 'properties-checklist':
        d.rectangle((2,1,13,14), fill=gray, outline=black)
        d.rectangle((4,0,10,2), fill=dark, outline=black)
        for y, on in ((4,True),(8,False),(12,True)):
            d.rectangle((4,y,6,y+1), fill=white)
            d.line((8,y,11,y), fill=black)
            d.point((10 if on else 8,y), fill=palette[10] if on else palette[12])
            if on:
                d.line([(4,y),(5,y+1),(6,y-1)], fill=palette[2])
    elif name == 'display-plain':
        d.rectangle((1,1,14,11), fill=gray, outline=black)
        d.rectangle((3,3,12,9), fill=black)
        d.rectangle((4,4,11,8), fill=palette[11])
        d.line((4,4,10,4), fill=white)
        d.line((6,12,9,12), fill=dark)
        d.rectangle((4,13,11,14), fill=gray, outline=black)
    elif name == 'properties-wrench':
        return artwork('Properties')
    elif name == 'display-color':
        return artwork('Display settings')
    elif name == 'display-resolution':
        d.rectangle((1,1,14,11), fill=gray, outline=black)
        d.rectangle((3,3,12,9), fill=black)
        # Opposed arrowheads indicate adjustable screen dimensions.
        d.line((4,6,11,6), fill=palette[11])
        d.line([(6,4),(4,6),(6,8)], fill=white)
        d.line([(9,4),(11,6),(9,8)], fill=white)
        d.line((6,12,9,12), fill=dark)
        d.rectangle((4,13,11,14), fill=gray, outline=black)
    else:
        raise ValueError(name)
    assert all(p[3] in (0,255) and (not p[3] or p[:3] in palette) for p in im.getdata())
    return im


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    sheet = Image.new('RGB',(1500,840),'#edf1f6')
    d = ImageDraw.Draw(sheet)
    fonts = {n:ImageFont.truetype('DejaVuSans.ttf',n) for n in (16,18,23,32)}

    def text(x,y,value,size=18,color='#23334b'):
        d.text((x,y),value,font=fonts[size],fill=color)

    d.rectangle((0,0,1500,120),fill='#172435')
    text(30,20,'Nightwatch · Properties and Display alternatives',32,'white')
    text(32,75,'Selected: P3 Properties and D2 Display · actual 16×16 VGA artwork · enlarged 6×',18,'#d8e4f2')
    groups = [
        ('Properties', [('P1','Checklist and switches','properties-checklist'),
                        ('P2','Attribute sliders','properties-sliders'),
                        ('P3','Document and wrench','properties-wrench')]),
        ('Display settings', [('D1','Plain CRT monitor','display-plain'),
                             ('D2','Color bars','display-color'),
                             ('D3','Screen size arrows','display-resolution')]),
    ]
    for row,(group,variants) in enumerate(groups):
        y=140+row*325
        text(30,y,group,23)
        for col,(code,title,key) in enumerate(variants):
            x=24+col*492
            top=y+42
            d.rounded_rectangle((x,top,x+476,top+260),radius=12,fill='white')
            text(x+18,top+14,f'{code} · {title}',23)
            text(x+18,top+49,'Selected' if code in ('P3','D2') else 'Alternative',16,'#536276')
            sprite=artwork(group) if key is None else alternative(key)
            if key:
                sprite.save(OUT/(key+'.png'))
            for offset,bg,label in ((20,1,'Blue · 6×'),(148,15,'White · 6×')):
                tile=Image.new('RGBA',(16,16),(*palette[bg],255))
                tile.alpha_composite(sprite)
                sheet.paste(tile.convert('RGB').resize((96,96),Image.Resampling.NEAREST),(x+offset,top+84))
                text(x+offset,top+190,label,16,'#536276')
            d.rectangle((x+268,top+100,x+456,top+134),fill=palette[1])
            sheet.paste(sprite,(x+277,top+109),sprite)
            text(x+302,top+108,'Properties' if row==0 else 'Display',16,'white')
            text(x+268,top+150,'Native 1×',16,'#536276')
    text(30,802,'Original artwork · GPL-3.0-only · Selected: P3 and D2; embedded in VGA title bars.',16,'#536276')
    output=ROOT/'docs/modal-icon-alternatives.png'
    sheet.save(output,optimize=True)
    print(output)


if __name__ == '__main__':
    main()
