# SPDX-License-Identifier: GPL-3.0-only
"""Reject a stale or corrupted shared navigator before testing a release."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import hashlib
import json
import unittest
import ui_build


class SharedNavigatorTests(unittest.TestCase):
    def test_source_and_binary_must_match(self):
        with TemporaryDirectory() as directory:
            base = Path(directory)
            exe = base/'UITEST.EXE'; exe.write_bytes(b'MZ checked navigator')
            record = {'source':'matching inputs', 'exe':hashlib.sha256(exe.read_bytes()).hexdigest()}
            (base/'UIBUILD.json').write_text(json.dumps(record))
            with patch.object(ui_build,'base',base), patch.object(ui_build,'source_key',return_value='matching inputs'):
                ui_build.require_prebuilt()
                exe.write_bytes(b'corrupt navigator')
                with self.assertRaisesRegex(AssertionError,'damaged'): ui_build.require_prebuilt()
                exe.write_bytes(b'MZ checked navigator')
                record['source']='old source files'
                (base/'UIBUILD.json').write_text(json.dumps(record))
                with self.assertRaisesRegex(AssertionError,'inputs changed'): ui_build.require_prebuilt()


if __name__=='__main__':
    unittest.main()
