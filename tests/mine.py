# SPDX-License-Identifier: GPL-3.0-only
"""Keyboard-only Minefield in real CGA/EGA/VGA VRAM; text has no game."""
from pathlib import Path
import shutil
import sys
from ui_build import root, base, dos, require_prebuilt
sys.path.insert(0, str(root/'tools'))

require_prebuilt()

def neighbors(at):
    x,y=at%9,at//9
    return [ny*9+nx for ny in range(max(0,y-1),min(9,y+2))
            for nx in range(max(0,x-1),min(9,x+2)) if (nx,ny)!=(x,y)]

def board(seed):
    bombs=set()
    while len(bombs)<10:
        seed^=(seed<<7)&65535
        seed^=seed>>9
        seed^=(seed<<8)&65535
        at=seed%81
        if at!=40 and at not in neighbors(40): bombs.add(at)
    return bombs

for machine,mode in [('cga','text'),('cga','cga'),('ega','ega'),('vgaonly','vga')]:
    work=base/('mine-'+mode)
    if work.exists(): shutil.rmtree(work)
    work.mkdir(); (work/'LEFT').mkdir(); (work/'RIGHT').mkdir()
    (work/'LEFT/KEEP.TXT').write_bytes(b'unchanged\r\n')
    shutil.copy2(base/'UITEST.EXE',work/'UITEST.EXE')
    keys=[]; checks={}
    def shot(name,key):
        checks[name]=len(keys); keys.append(key)
    if mode=='text':
        shot('desktop',0x6800); shot('about',ord('g')); shot('returned',0x4400)
    else:
        shot('desktop',0x6800); shot('about',ord('g'))
        shot('ready',ord('f')); shot('flagged',32); shot('blocked',ord('f'))
        shot('unflagged',32); shot('revealed',ord('z')); shot('noop',ord('f'))
        keys.append(0x4d00)
        shot('open_right',0x4b00); shot('open_back',ord('z'))
        cursor=40
        def move(at):
            global cursor
            while cursor%9!=at%9:
                right=cursor%9<at%9
                keys.append(0x4d00 if right else 0x4b00); cursor+=1 if right else -1
            while cursor//9!=at//9:
                down=cursor//9<at//9
                keys.append(0x5000 if down else 0x4800); cursor+=9 if down else -9
        bombs=board(123)
        hit=min(bombs)
        move(hit); keys.append(32)
        shot('lost',ord('f')); shot('lost_flag',32); shot('lost_reveal',ord('n'))
        shot('restart',32)
        cursor=40; bombs=board((123*25173+13849)&65535); opened=set()
        def reveal(at):
            pending=[at]
            while pending:
                item=pending.pop()
                if item in opened: continue
                assert item not in bombs
                opened.add(item)
                if not any(n in bombs for n in neighbors(item)): pending+=neighbors(item)
        reveal(40)
        while len(opened)<71:
            candidates=set(range(81))-bombs-opened
            at=min(candidates,key=lambda c: (any(n in bombs for n in neighbors(c)),
                   abs(c%9-cursor%9)+abs(c//9-cursor//9),c))
            move(at); keys.append(32); reveal(at)
        shot('won',32); shot('won_reveal',ord('f')); shot('won_flag',ord('n'))
        shot('fresh',27); shot('back_about',ord('g')); shot('reopened',27)
        shot('about_again',27); shot('returned',0x4400)
    (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in keys))
    dos(work,[r'UITEST /'+mode+r' D:\LEFT D:\RIGHT'],machine)
    frames={name:(work/f'F{index:03d}.BIN').read_bytes() for name,index in checks.items()}
    assert frames['desktop']==frames['returned'], (mode,'desktop restoration')
    assert (work/'LEFT/KEEP.TXT').read_bytes()==b'unchanged\r\n' and not list((work/'RIGHT').iterdir())
    if mode=='text':
        assert 'OFF WORK' not in frames['about'][3:4003:2].decode('cp437')
        print('PASS: text About/G returns directly to the untouched desktop',flush=True)
        continue
    assert frames['ready']==frames['unflagged']==frames['restart']==frames['fresh']==frames['reopened']
    assert frames['flagged']==frames['blocked']!=frames['ready']
    assert frames['revealed']==frames['noop']!=frames['ready']
    assert frames['revealed']==frames['open_back']!=frames['open_right']
    # Inspect native planes directly; never generate a preview of the secret.
    tile_height, font_height, screen_height={'cga':(12,8,200),'ega':(20,12,350),'vga':(24,16,480)}[mode]
    board_y=(screen_height-(9*tile_height+3*font_height+54))//2+2*font_height+28
    def pixel(scene,x,y):
        raw=frames[scene]
        at=3+y*80+x//8; bit=128>>(x%8)
        planes=1 if mode=='cga' else 4
        color=sum(1<<p for p in range(planes) if raw[at+p*screen_height*80]&bit)
        return (15 if color else 0) if mode=='cga' else color
    for scene,cell in (('revealed',40),('open_right',41)):
        x,y=72+cell%9*24,board_y+cell//9*tile_height
        assert pixel(scene,x+2,y+2)==(0 if mode=='cga' else 7), (mode,'opened cursor fill')
        assert pixel(scene,x,y+3)==0, (mode,'cursor outer border')
        assert pixel(scene,x+1,y+3)==(15 if mode=='cga' else 1), (mode,'cursor inner border')
    assert pixel('open_right',72+4*24+2,board_y+4*tile_height+2)==15, (mode,'old cursor restored')
    assert frames['lost']==frames['lost_flag']==frames['lost_reveal']
    assert frames['won']==frames['won_reveal']==frames['won_flag']
    assert frames['about']==frames['back_about']==frames['about_again']
    for scene in ('ready','revealed','lost','won'):
        raw=frames[scene]
        expected_mode, height, planes={'cga':(1,200,1),'ega':(2,350,4),'vga':(3,480,4)}[mode]
        assert raw[0]==expected_mode and int.from_bytes(raw[1:3],'little')==height
        assert len(raw)==3+height*80*planes
    writes=[int((work/f'F{checks[name]:03d}.POS').read_text().split()[7])
            for name in ('revealed','noop','lost','lost_flag','lost_reveal','won','won_reveal','won_flag')]
    assert writes[0]==writes[1] and writes[2]==writes[3]==writes[4] and writes[5]==writes[6]==writes[7]
    # Keep diagnostics, but no game images or reusable VRAM previews.
    for capture in work.glob('F*.BIN'): capture.unlink()
    print('PASS:',mode,'flags, safe opening, loss, restart, solved win, no-op writes, repeated entry and desktop restoration',flush=True)
