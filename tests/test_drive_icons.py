# SPDX-License-Identifier: GPL-3.0-only
"""Far-memory VGA sprites must reproduce every approved drive-icon pixel."""
from pathlib import Path
import re,sys,unittest
from PIL import Image
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tools'))
from drive_icons import drive_icon
from vram import palette

class DriveIcons(unittest.TestCase):
    def test_far_sprites_match_approved_art(self):
        assembly=(ROOT/'src/FONT.ASM').read_text()
        for index,kind in enumerate(('floppy-35','floppy-525','hard-disk','sd-card')):
            block=re.search(r'drive_icon_'+str(index)+r' label byte\n((?:    db [^\n]+\n)+)',assembly)
            self.assertIsNotNone(block)
            data=bytes(int(n,16) for n in re.findall(r'\b0([0-9a-f]{2})h\b',block[1]))
            self.assertEqual(len(data),160)
            authored=drive_icon(kind)
            with Image.open(ROOT/'assets/drive-icons'/f'{kind}.png') as stored:
                self.assertEqual(stored.convert('RGBA').tobytes(),authored.tobytes())
            for y in range(16):
                for x in range(16):
                    at,bit=y*2+x//8,128>>(x%8)
                    pixel=authored.getpixel((x,y))
                    self.assertEqual(bool(data[at]&bit),bool(pixel[3]),(kind,x,y))
                    if pixel[3]:
                        color=sum(1<<p for p in range(4) if data[32+p*32+at]&bit)
                        self.assertEqual(palette[color],pixel[:3],(kind,x,y))

if __name__=='__main__': unittest.main()
