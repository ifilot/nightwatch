# SPDX-License-Identifier: GPL-3.0-only
"""Generated masks must reproduce every source pixel, including native CGA."""
from pathlib import Path
import re
import unittest
from PIL import Image

root = Path(__file__).resolve().parents[1]


class IconTests(unittest.TestCase):
    def test_far_modal_sprites_match_approved_pixels(self):
        assembly = (root/'src/FONT.ASM').read_text()
        names = ('help', 'about', 'search', 'properties', 'copy', 'move',
                 'delete', 'warning', 'storage-test', 'display-settings')
        self.assertEqual(len(re.findall(r'^modal_icon_\d+ label byte$', assembly, re.M)), len(names))
        for index, name in enumerate(names):
            block = re.search(r'modal_icon_' + str(index) + r' label byte\n((?:    db [^\n]+\n)+)', assembly)
            self.assertIsNotNone(block)
            data = bytes(int(n, 16) for n in re.findall(r'\b0([0-9a-f]{2})h\b', block[1]))
            self.assertEqual(len(data), 160)
            with Image.open(root/'assets/modal-icon-studies'/(name+'.png')) as stored:
                authored = stored.convert('RGBA')
            self.assertEqual(authored.size, (16, 16))
            for y in range(16):
                for x in range(16):
                    at, bit = y*2+x//8, 128 >> (x%8)
                    expected = authored.getpixel((x, y))
                    self.assertEqual(bool(data[at]&bit), bool(expected[3]), (name, x, y))
                    if expected[3]:
                        color = sum(1 << p for p in range(4) if data[32+p*32+at]&bit)
                        colors = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
                                  (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
                        self.assertEqual(colors[color], expected[:3], (name, x, y))

    def test_vga_planes_match_authored_pixels(self):
        source = (root/'src/ASSETS.H').read_text()
        tables = {name: bytes(int(v,16) for v in re.findall(r'0x([0-9a-f]{2})',data))
                  for name,data in re.findall(r'static const unsigned char (vga_\w+)\[\d+\] = \{(.*?)\};',source,re.S)}
        self.assertEqual(len(tables),8)
        colors = [(0,0,0),(0,0,170),(0,170,0),(0,170,170),(170,0,0),(170,0,170),(170,85,0),(170,170,170),
                  (85,85,85),(85,85,255),(85,255,85),(85,255,255),(255,85,85),(255,85,255),(255,255,85),(255,255,255)]
        for name,data in tables.items():
            self.assertEqual(len(data),160)
            authored = Image.open(root/'assets/icons-v130'/('quickmenu-'+name[4:]+'.png')).convert('RGBA')
            for y in range(16):
                for x in range(16):
                    at,bit = y*2+x//8,128>>(x%8)
                    opaque = bool(data[at]&bit)
                    color = sum((1<<p) for p in range(4) if data[32+p*32+at]&bit)
                    expected = authored.getpixel((x,y))
                    self.assertEqual(opaque,bool(expected[3]),(name,x,y))
                    if opaque: self.assertEqual(colors[color],expected[:3],(name,x,y))

    def test_exact_native_masks(self):
        source = (root/'src/ASSETS.H').read_text()
        tables = {name: bytes(int(v, 16) for v in re.findall(r'0x([0-9a-f]{2})', data))
                  for name, data in re.findall(r'static const unsigned char (\w+)\[\d+\] = \{(.*?)\};', source, re.S)}
        names = re.findall(r'#define ICON_(\w+) \d+', source)
        self.assertEqual(len(names), 8)
        for name in names:
            for prefix, folder, height, channels in (
                    ('icon_', 'assets/icons', 16, 'X'),
                    ('cga_', 'assets/icons/cga', 8, 'Xo+')):
                with self.subTest(name=name, mode=prefix):
                    rows = (root/folder/(name.lower()+'.txt')).read_text().splitlines()
                    self.assertEqual(len(rows), height)
                    self.assertTrue(all(len(row) == 16 for row in rows))
                    self.assertTrue(all(c in channels+'.' for row in rows for c in row))
                    expected = bytearray()
                    for channel in channels:
                        for row in rows:
                            bits = int(''.join('1' if c == channel else '0' for c in row), 2)
                            expected.extend((bits >> 8, bits & 255))
                    self.assertEqual(tables[prefix+name.lower()], expected)


if __name__ == '__main__':
    unittest.main()
