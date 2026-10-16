# SPDX-License-Identifier: GPL-3.0-only
"""The DOS memory gate must use the stack of the executable being checked."""
from pathlib import Path
import subprocess as sp
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class MemoryTests(unittest.TestCase):
    def test_stack_source_and_heap_reserve(self):
        with tempfile.TemporaryDirectory() as directory:
            work = Path(directory)
            source = work/'PROBE.C'
            map_path = work/'PROBE.MAP'
            map_path.write_text(' 01000H 0100FH 00010H _DATA DATA\n'
                                ' 0F000H 0F000H 00000H _BSSEND STACK\n')
            # 57344 near bytes fit with the UI probe's 4 KB stack, but fail
            # with the application's 8 KB stack. A 6 KB stack leaves exactly
            # the same required 2 KB heap floor; anything larger must fail.
            for stack, success in [(4096, True), (6144, True), (6160, False), (8192, False)]:
                source.write_text(f'unsigned _stklen = {stack};\n')
                result = sp.run([sys.executable, str(ROOT/'tools/check_memory.py'),
                                 str(map_path), '--stack-source', str(source)],
                                text=True, capture_output=True)
                self.assertEqual(result.returncode == 0, success, result.stdout+result.stderr)
                if success:
                    self.assertIn(f'{stack} stack', result.stdout)
                else:
                    self.assertIn('Near-data overflow', result.stderr)


if __name__ == '__main__':
    unittest.main()
