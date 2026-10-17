# SPDX-License-Identifier: GPL-3.0-only
"""Static-site inputs, checked downloads and reproducible DOS bundle entries."""
from pathlib import Path
import hashlib
from io import BytesIO
import sys
import tempfile
import unittest
from unittest.mock import patch
from zipfile import ZipFile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import build_site


class SiteTests(unittest.TestCase):
    def test_gallery_includes_transfer_speed_captures(self):
        entries = build_site.gallery_entries()
        self.assertEqual(len(entries), 61)
        self.assertEqual(len({entry['file'] for entry in entries}), len(entries))
        for name in ('copy-progress.png', 'arj-extraction-progress.png'):
            self.assertEqual(next(e for e in entries if e['file'] == name)['category'], 'transfers')
        self.assertEqual(next(e for e in entries if e['file'] == '29-text-viewer.png')['category'], 'viewers')
        self.assertEqual({e['category'] for e in entries}, {'navigation', 'viewers', 'transfers', 'tools'})

    def test_cached_download_is_verified_without_network(self):
        with tempfile.TemporaryDirectory() as temporary:
            cache = Path(temporary)
            (cache/'asset.tgz').write_bytes(b'valid')
            digest = hashlib.sha256(b'valid').hexdigest()
            with patch.object(build_site.urllib.request, 'urlopen', side_effect=AssertionError('network')):
                self.assertEqual(build_site.fetch(cache, 'asset.tgz', 'https://invalid', digest).read_bytes(), b'valid')
                (cache/'asset.tgz').write_bytes(b'corrupt')
                with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
                    build_site.fetch(cache, 'asset.tgz', 'https://invalid', digest)

    def test_bad_download_is_not_cached(self):
        with tempfile.TemporaryDirectory() as temporary:
            with patch.object(build_site.urllib.request, 'urlopen', return_value=BytesIO(b'bad')):
                with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
                    build_site.fetch(Path(temporary), 'asset.tgz', 'https://invalid', '0'*64)
            self.assertFalse((Path(temporary)/'asset.tgz').exists())

    def test_bundle_entries_are_deterministic_and_include_empty_directories(self):
        outputs = []
        for unused in range(2):
            output = BytesIO()
            with ZipFile(output, 'w') as bundle:
                build_site.zip_write(bundle, 'WORK/', b'')
                build_site.zip_write(bundle, 'WORK/TEMP/', b'')
                build_site.zip_write(bundle, 'BACKUP/', b'')
                build_site.zip_write(bundle, 'BACKUP/README.TXT', b'backup\r\n')
            outputs.append(output.getvalue())
        self.assertEqual(*outputs)
        with ZipFile(BytesIO(outputs[0])) as bundle:
            self.assertTrue(bundle.getinfo('WORK/TEMP/').is_dir())
            self.assertEqual(bundle.namelist()[:2], ['WORK/', 'WORK/TEMP/'])
            self.assertTrue(bundle.getinfo('WORK/').external_attr & 0x10)
            self.assertEqual(bundle.read('BACKUP/README.TXT'), b'backup\r\n')

    def test_boot_mounts_only_packaged_demo_drives(self):
        config = build_site.DOSBOX_CONF
        self.assertIn('mount d WORK', config)
        self.assertIn('mount e BACKUP', config)
        self.assertIn('NW /vga D:\\ E:\\', config)
        self.assertNotIn('/mnt/', config)
        self.assertIn('NIGHTWATCH-BROWSER-SESSION-END', config)


if __name__ == '__main__':
    unittest.main()
