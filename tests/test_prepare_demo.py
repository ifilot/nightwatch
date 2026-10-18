# SPDX-License-Identifier: GPL-3.0-only
"""Demo launcher preparation must work without pre-existing local demo media."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT/'tools'))
import prepare_demo


class DemoLauncherTests(unittest.TestCase):
    def setUp(self):
        self.temp = TemporaryDirectory(prefix='nw-demo-build-')
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        for name in ('tools/DEMODRV.C', 'tools/DRVMETA.ASM', 'tools/dosbox.conf',
                     'buildenv/TC/TCC.EXE', 'buildenv/TASM/TASM.EXE'):
            path = self.root/name
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(b'fixture\n')
        self.directory = self.root/'build/demo'
        self.enterContext(patch.object(prepare_demo, 'ROOT', self.root))
        self.enterContext(patch.dict(prepare_demo.os.environ,
                                    {'DOS_TOOLCHAIN': str(self.root/'buildenv')}))

    def compile_success(self, *args, **kwargs):
        (self.directory/'DEMODRV.EXE').write_bytes(b'MZ fresh launcher')
        (self.directory/'DEMO.OK').write_text('OK')

    def test_cold_build_without_media_then_cached_build(self):
        self.assertFalse(self.directory.exists())
        with patch.object(prepare_demo.sp, 'run', side_effect=self.compile_success) as run:
            prepare_demo.build_launcher()
            self.assertEqual(run.call_count, 1)
            prepare_demo.build_launcher()
            self.assertEqual(run.call_count, 1)
        self.assertFalse((self.directory/'A.IMG').exists())

    def test_failed_rebuild_cannot_accept_old_launcher(self):
        with patch.object(prepare_demo.sp, 'run', side_effect=self.compile_success):
            prepare_demo.build_launcher()
        (self.root/'tools/DEMODRV.C').write_text('changed source')
        with patch.object(prepare_demo.sp, 'run'):
            with self.assertRaises(RuntimeError):
                prepare_demo.build_launcher()
        self.assertFalse((self.directory/'DEMODRV.EXE').exists())
        self.assertFalse((self.directory/'launcher.sha256').exists())

    def test_isolated_suite_directory(self):
        default = self.directory
        self.directory = self.root/'build/demo-test/launcher'
        with patch.object(prepare_demo.sp, 'run', side_effect=self.compile_success):
            prepare_demo.build_launcher(self.directory)
        self.assertTrue((self.directory/'DEMODRV.EXE').is_file())
        self.assertFalse(default.exists())

    def test_empty_compiler_output_is_rejected(self):
        def empty(*args, **kwargs):
            self.compile_success()
            (self.directory/'DEMODRV.EXE').write_bytes(b'')
        with patch.object(prepare_demo.sp, 'run', side_effect=empty):
            with self.assertRaises(RuntimeError):
                prepare_demo.build_launcher()
        self.assertFalse((self.directory/'launcher.sha256').exists())


if __name__ == '__main__':
    unittest.main()
