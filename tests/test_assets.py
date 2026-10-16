# SPDX-License-Identifier: GPL-3.0-only
"""Generated masks must reproduce every source pixel, including native CGA."""
from pathlib import Path
import re
import unittest

root = Path(__file__).resolve().parents[1]


class IconTests(unittest.TestCase):
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
