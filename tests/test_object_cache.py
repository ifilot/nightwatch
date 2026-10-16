# SPDX-License-Identifier: GPL-3.0-only
"""Cached DOS objects require matching inputs and intact object bytes."""
from pathlib import Path
from tempfile import TemporaryDirectory
from unittest.mock import patch
import unittest
import object_cache
from object_cache import ObjectCache, signature


class CacheTests(unittest.TestCase):
    def test_inputs_flags_compiler_and_location(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            keys = []
            for folder in ('first', 'relocated'):
                base = root/folder; base.mkdir()
                source = base/'module.C'; source.write_bytes(b'int value;')
                compiler = base/'TCC.EXE'; compiler.write_bytes(b'compiler version 1')
                with patch.object(object_cache, 'ROOT', base):
                    def key(flags='-mm'):
                        return signature([source],flags,[('toolchain/TCC.EXE',compiler)])
                    keys.append(key())
                    self.assertNotEqual(key('-ms'),key())
                    source.write_bytes(b'long value;')
                    self.assertNotEqual(key(),keys[-1])
                    source.write_bytes(b'int value;')
                    compiler.write_bytes(b'compiler version 2')
                    self.assertNotEqual(key(),keys[-1])
            self.assertEqual(keys[0],keys[1], 'Artifact reuse must survive checkout relocation')

    def test_toolchain_headers_libraries_and_configuration(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            for name in ('TC/TCC.EXE','TC/TLINK.EXE','TASM/TASM.EXE',
                         'TC/INCLUDE/STDIO.H','TC/LIB/CM.LIB','TC/TURBOC.CFG'):
                path = root/name; path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(b'original compiler input')
            source = root/'TEST.C'; source.write_bytes(b'int value;')
            with patch.object(object_cache,'ROOT',root), patch.dict(object_cache.os.environ,{'DOS_TOOLCHAIN':str(root)}):
                before = signature([source],'-mm')
                for name in ('TC/INCLUDE/STDIO.H','TC/LIB/CM.LIB','TC/TURBOC.CFG'):
                    path = root/name; path.write_bytes(b'changed compiler input')
                    self.assertNotEqual(signature([source],'-mm'),before,name)
                    path.write_bytes(b'original compiler input')

    def test_roundtrip_and_corruption_never_replaces_working_objects(self):
        with TemporaryDirectory() as directory:
            root = Path(directory)
            work = root/'work'; work.mkdir()
            (work/'ONE.OBJ').write_bytes(b'fresh one')
            (work/'TWO.OBJ').write_bytes(b'fresh two')
            cache = ObjectCache(root/'cache','checked-inputs')
            names = ['ONE.OBJ','TWO.OBJ']
            self.assertFalse(cache.restore(work,names))
            cache.save(work,names)
            for name in names: (work/name).unlink()
            self.assertTrue(cache.restore(work,names))
            self.assertEqual((work/'ONE.OBJ').read_bytes(),b'fresh one')
            (cache.directory/'TWO.OBJ').write_bytes(b'corrupted')
            (work/'ONE.OBJ').write_bytes(b'keep working copy')
            self.assertFalse(cache.restore(work,names))
            self.assertEqual((work/'ONE.OBJ').read_bytes(),b'keep working copy')
            cache.save(work,names)
            self.assertFalse(cache.restore(work,['ONE.OBJ']), 'Object list must match exactly')
            (cache.directory/'manifest.json').write_text('broken json')
            self.assertFalse(cache.restore(work,names))
            (work/'TWO.OBJ').write_bytes(b'')
            with self.assertRaises(ValueError): cache.save(work,names)


if __name__ == '__main__':
    unittest.main()
