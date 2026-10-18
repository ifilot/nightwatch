# SPDX-License-Identifier: GPL-3.0-only
"""Check common native title strips, icons and repeated modal restoration."""
import shutil
import sys
from ui_build import root, base, dos, require_prebuilt
sys.path.insert(0,str(root/'tools'))
from vram import decode

require_prebuilt()
for machine,mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga')]:
    work=base/('modals-'+mode)
    if work.exists(): shutil.rmtree(work)
    (work/'LEFT').mkdir(parents=True); (work/'RIGHT').mkdir()
    (work/'LEFT/ALPHA.TXT').write_bytes(b'Hello\r\n')
    (work/'RIGHT/ALPHA.TXT').write_bytes(b'Keep\r\n')
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    keys=[0x5000]; frames={}
    cases=[('mode',0x3c00,27),('sort',19,27),('find',6,27),('mark',16,27),
           ('unmark',14,27),('copy',0x3f00,27),('move',0x4000,27),
           ('mkdir',0x4100,27),('delete',0x4200,ord('n')),('drive',0x4300,27),
           ('properties',1,27),('storage',20,27),('recursive',0x6e00,27)]
    for name,key,close in cases:
        frames[name+'-before']=len(keys); keys.append(key)
        frames[name]=len(keys); keys.append(close)
        keys.append(key); frames[name+'-again']=len(keys); keys.append(close)
        frames[name+'-after']=len(keys)
    keys.append(0x3f00); keys.append(13)
    frames['conflict']=len(keys); keys.extend([27,27])
    keys.append(0x4400)
    (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys))
    dos(work,[r'UITEST /'+mode+r' D:\LEFT D:\RIGHT'],machine)
    raw={name:(work/f'F{index:03d}.BIN').read_bytes() for name,index in frames.items()}
    for name,_,_ in cases:
        assert raw[name]==raw[name+'-again'],(mode,name,'repeated modal differs')
        assert raw[name+'-before']==raw[name+'-after'],(mode,name,'desktop differs')
    assert (work/'RIGHT/ALPHA.TXT').read_bytes()==b'Keep\r\n'
    if mode=='text':
        assert 'Display mode' in raw['mode'][3:4003:2].decode('cp437')
        print('PASS: text modal controls, repeat entry and desktop restoration',flush=True)
        continue
    height={'cga':200,'ega':350,'vga':480}[mode]
    rows=30 if mode=='vga' else 25
    cell=height//rows; font={'cga':8,'ega':12,'vga':16}[mode]
    def pixel(data,x,y):
        at=3+y*80+x//8; bit=128>>(x%8)
        color=sum(1<<p for p in range(1 if mode=='cga' else 4) if data[at+p*height*80]&bit)
        return (15 if color else 0) if mode=='cga' else color
    for name in [c[0] for c in cases]+['conflict']:
        data=raw[name]; bar=0 if mode=='cga' else 1
        if name=='drive':
            top=next(y for y in range(height-20)
                     if all(pixel(data,x,y)==0 for x in range(24,616))
                     and pixel(data,25,y+1)==15 and pixel(data,600,y+2)==bar)
        else: top=(rows//2-(4 if name=='conflict' else 3))*cell
        assert all(pixel(data,x,top)==0 for x in range(24,616)),(mode,name,'outer border')
        assert all(pixel(data,x,top+2)==bar for x in range(26,614)),(mode,name,'title strip')
        icon={pixel(data,x,y) for y in range(top+3,top+3+(8 if mode=='cga' else 16))
              for x in range(34,50)}
        assert len(icon)>1,(mode,name,'missing title icon')
        assert any(pixel(data,x,y)==15 for y in range(top+4,top+4+font)
                   for x in range(56,200)),(mode,name,'missing white title')
    for name in ('mode','drive','properties'):
        decode(work/f'F{frames[name]:03d}.BIN',work/(name+'.png'))
    print('PASS:',mode,'native title bars/icons for prompts, menus, properties, drive picker and conflicts',flush=True)
