# SPDX-License-Identifier: GPL-3.0-only
"""Build provenance follows Git commits and identifies modified/source exports."""
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import build_metadata


class BuildMetadataTests(unittest.TestCase):
    def test_export_without_git(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            build_metadata.write(root)
            self.assertIn('"unknown"', (root / 'src/BUILD.H').read_text())

    def test_commit_dirty_and_unchanged_header(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / 'src').mkdir()
            source = root / 'src/SAMPLE.C'
            source.write_text('original\n')
            (root / '.gitignore').write_text('/src/BUILD.H\n')
            def git(*args):
                return subprocess.check_output(['git', *args], cwd=root, text=True).strip()
            git('init', '-q')
            git('add', 'src/SAMPLE.C', '.gitignore')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                'commit', '-qm', 'Initial')
            commit = git('rev-parse', '--short=7', 'HEAD')
            self.assertEqual(build_metadata.revision(root), commit)
            build_metadata.write(root)
            header = root / 'src/BUILD.H'
            timestamp = header.stat().st_mtime_ns
            build_metadata.write(root)
            self.assertEqual(header.stat().st_mtime_ns, timestamp)
            extra = root / 'src/NEW.C'
            extra.write_text('new source\n')
            self.assertEqual(build_metadata.revision(root), commit + '-dirty')
            extra.unlink()
            source.write_text('changed\n')
            self.assertEqual(build_metadata.revision(root), commit + '-dirty')
            build_metadata.write(root)
            self.assertIn(commit + '-dirty', header.read_text())
            git('add', 'src/SAMPLE.C')
            git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                'commit', '-qm', 'Update')
            build_metadata.write(root)
            self.assertIn(git('rev-parse', '--short=7', 'HEAD'), header.read_text())
            self.assertNotIn('-dirty', header.read_text())


if __name__ == '__main__':
    unittest.main()
