# SPDX-License-Identifier: GPL-3.0-only
"""Verify release contents, byte integrity and failure on invalid build inputs."""
from pathlib import Path
import hashlib
import importlib.util
import tempfile
import unittest
import zipfile
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))

spec = importlib.util.spec_from_file_location('package', Path(__file__).resolve().parents[1] / 'tools/package.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class PackageTests(unittest.TestCase):
    def test_release_contents(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            exe = b'MZ' + bytes(range(256))
            license_data = b'BSD font license\n'
            (build / 'NIGHT.EXE').write_bytes(exe)
            (build / 'FONTLIC.TXT').write_bytes(license_data)
            (build / 'LICENSE.TXT').write_bytes(b'GNU GENERAL PUBLIC LICENSE version 3\n')
            (build / 'TCC.EXE').write_bytes(b'never redistribute compiler')
            output = build / 'dist'
            archive = module.package(build, output)
            first = archive.read_bytes()
            with zipfile.ZipFile(archive) as zipped:
                self.assertEqual(set(zipped.namelist()), {'NIGHT.EXE', 'FONTLIC.TXT', 'LICENSE.TXT', 'VERSION.TXT', 'README.TXT'})
                self.assertEqual(zipped.read('NIGHT.EXE'), exe)
                self.assertEqual(zipped.read('FONTLIC.TXT'), license_data)
                self.assertEqual(zipped.read('LICENSE.TXT'), (build / 'LICENSE.TXT').read_bytes())
                self.assertEqual(zipped.read('VERSION.TXT'), ('v' + module.read_version() + '\r\n').encode('ascii'))
                text = zipped.read('README.TXT')
                self.assertIn(b'NIGHT /text', text)
                self.assertIn(('Nightwatch v'+module.read_version()).encode('ascii'), text)
                self.assertIn(b'GNU GPL version 3', text)
                self.assertNotIn(b'\n', text.replace(b'\r\n', b''))
                self.assertIsNone(zipped.testzip())
            for line in (output / 'SHA256SUMS.txt').read_text().splitlines():
                digest, name = line.split('  ')
                self.assertEqual(digest, hashlib.sha256((output / name).read_bytes()).hexdigest())
            module.package(build, output)
            self.assertEqual(first, archive.read_bytes())

    def test_rejects_invalid_inputs(self):
        with tempfile.TemporaryDirectory() as directory:
            build = Path(directory)
            output = build / 'dist'
            (build / 'NIGHT.EXE').write_bytes(b'not a DOS executable')
            (build / 'FONTLIC.TXT').write_text('license')
            (build / 'LICENSE.TXT').write_text('GPL version 3')
            with self.assertRaises(ValueError):
                module.package(build, output)
            self.assertFalse(output.exists())
            (build / 'NIGHT.EXE').write_bytes(b'MZ' + bytes(100))
            (build / 'FONTLIC.TXT').write_bytes(b'')
            with self.assertRaises(ValueError):
                module.package(build, output)
            self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
