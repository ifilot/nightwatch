# SPDX-License-Identifier: GPL-3.0-only
"""Check version grammar and agreement between committed application metadata."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import version


class VersionTests(unittest.TestCase):
    def test_valid_and_invalid_versions(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'VERSION'
            for text in ('1.0.0','0.0.1','12.34.56'):
                path.write_text(text+'\n')
                self.assertEqual(version.read_version(path),text)
            for text in ('v1.0.0','01.0.0','1.0','1.0.0junk','1.0.0\n2.0.0','-1.0.0'):
                path.write_text(text)
                with self.assertRaises(ValueError):version.read_version(path)

    def test_generated_header_and_badge(self):
        current=version.read_version()
        version.synchronize(current,check=True)
        self.assertIn('#define NW_VERSION "'+current+'"',version.header(current))
        self.assertIn('#define NW_VERSION_TAG "v" NW_VERSION',version.header(current))

    def test_bump_and_detect_stale_metadata(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            readme = root / 'README.md'
            readme.write_text('Badge: https://img.shields.io/badge/version-v1.0.0-blue\n')
            with patch.object(version, 'ROOT', root):
                with self.assertRaises(ValueError):
                    version.synchronize('1.0.1', check=True)
                version.synchronize('1.0.1')
                version.synchronize('1.0.1', check=True)
                self.assertIn('version-v1.0.1-blue', readme.read_text())
                self.assertEqual((root / 'src/VERSION.H').read_text(), version.header('1.0.1'))
                with self.assertRaises(ValueError):
                    version.synchronize('1.0.2', check=True)


if __name__ == '__main__':
    unittest.main()
