# SPDX-License-Identifier: GPL-3.0-only
"""File-integrity oracle, long profile, persistence, ownership and fault tests."""
from pathlib import Path
import hashlib
import os
import subprocess as sp
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT/'build/test_storage'
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')

def digest(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}

def oracle(seed, file_id, length):
    result = bytearray()
    for block in range((length+511)//512):
        state = (seed ^ (seed >> 16) ^ block ^ (block >> 16) ^ file_id) & 65535
        data = bytearray()
        for _ in range(512):
            state = (state*25173+13849) & 65535
            data.append(state >> 8)
        data[:12] = seed.to_bytes(4,'little') + file_id.to_bytes(4,'little') + block.to_bytes(4,'little')
        result += data
    return bytes(result[:length])

class StorageTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        sp.run(['cc','-x','c','-std=c89','-Wall','-Wextra','-Werror','-pedantic',
                '-DNW_HOST','-Isrc','-Itests','-fsanitize=address,undefined',
                '-fno-omit-frame-pointer','-no-pie','-g','src/CORE.C','src/STORTEST.C',
                'tests/fs_host.c','tests/STPROBE.C','-o',str(EXE)],cwd=ROOT,check=True)
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix='nwstore-', dir='/tmp')
        self.work = Path(self.tmp.name)
        self.target = self.work/'CARD'; self.target.mkdir()
        self.local = self.work/'LOCAL'; self.local.mkdir()
        (self.target/'KEEP.TXT').write_bytes(b'existing user file')
    def tearDown(self): self.tmp.cleanup()
    def run_engine(self, *args, success=True, fault=None):
        env = dict(ENV)
        if fault: env['NW_STORAGE_FAULT'] = fault
        p = sp.run([str(EXE),*map(str,args)],env=env,text=True,capture_output=True)
        self.assertEqual(p.returncode, 0 if success else 1, p.stdout+p.stderr)
        self.assertEqual((self.target/'KEEP.TXT').read_bytes(), b'existing user file')
        if success:
            mask,stages = (63,6) if args[0]=='start' else (7,3)
            self.assertIn(f'REPORT state=1 passed={mask} stages={stages}',p.stdout)
        elif fault in ('cancel','cancel-final','cancel-cleanup'):
            self.assertIn('REPORT state=3',p.stdout)
        else: self.assertIn('REPORT state=2',p.stdout)
        return p.stdout
    def start(self, profile=0, seed=4294967295, **kw):
        self.run_engine('start',self.target,self.local,profile,seed,**kw)
        self.manifest = next(self.local.glob('*.DAT'))
        self.root = self.target/self.manifest.stem
    def test_missing_directory_planning_and_creation(self):
        path = self.target/'PARENT/CHILD'
        def directory(command, path, success=True):
            p = sp.run([str(EXE),command,str(path)],env=ENV,text=True,capture_output=True)
            self.assertEqual(p.returncode, 0 if success else 1, p.stdout+p.stderr)
            return p.stdout
        self.assertIn('missing=1',directory('plan',path))
        self.assertFalse((self.target/'PARENT').exists())
        self.assertIn('missing=1',directory('mkdir',path))
        self.assertTrue(path.is_dir())
        self.assertIn('missing=0',directory('mkdir',path))
        directory('plan',self.target/'KEEP.TXT/CHILD',False)
        directory('mkdir',self.target/'NEW/BAD*NAME',False)
        self.assertFalse((self.target/'NEW').exists())
        directory('mkdir',self.target/'NEW/TOOLONGNAME',False)
        self.assertFalse((self.target/'NEW').exists())
        directory('mkdir',self.target/('A'*128),False)
        self.assertEqual((self.target/'KEEP.TXT').read_bytes(),b'existing user file')
    def test_quick_oracle_relaunch_verify_cleanup(self):
        output = self.run_engine('start',self.target,self.local,0,4294967295)
        self.manifest = next(self.local.glob('*.DAT')); self.root = self.target/self.manifest.stem
        self.assertIn('written=2675059 verified=5520668 files=131', output)
        self.assertEqual((self.root/'DATA/NEST/B13.BIN').read_bytes(),oracle(4294967295,13,65537))
        self.assertEqual((self.root/'DATA/NEST/B20.BIN').read_bytes(),bytes(513))
        self.assertEqual((self.root/'DATA/NEST/B21.BIN').read_bytes(),b'\xff'*65537)
        self.assertEqual(len(list((self.root/'GROW').iterdir())),40)
        self.assertEqual((self.root/'GROW/G0000.BIN').read_bytes(),oracle(4294967295,4000,17))
        self.assertEqual((self.root/'GROW/G0001.BIN').read_bytes(),oracle(4294967295,1001,17))
        self.assertEqual(list((self.root/'SCRATCH').iterdir()),[])
        before = digest(self.target)
        self.run_engine('verify',self.manifest)
        self.assertEqual(digest(self.target),before,'verification wrote to target')
        self.run_engine('cleanup',self.manifest)
        self.assertFalse(self.root.exists())
        self.assertTrue(self.manifest.exists())
        self.assertEqual(len(list(self.local.glob('*.L??'))),3)
    def test_long_profile(self):
        self.start(profile=1,seed=12345)
        self.assertEqual((self.root/'DATA/NEST/BIG.BIN').stat().st_size,16*1024*1024)
        self.assertEqual(len(list((self.root/'GROW').iterdir())),2200)
        self.run_engine('verify',self.manifest)
        self.run_engine('cleanup',self.manifest)
        self.assertFalse(self.root.exists())
    def test_mismatch_reports_exact_offset_and_preserves_evidence(self):
        self.start(seed=42)
        path = self.root/'DATA/NEST/B13.BIN'
        with path.open('r+b') as f:
            f.seek(65536); byte = f.read(1)[0]; f.seek(65536); f.write(bytes([byte^255]))
        before = digest(self.target)
        output = self.run_engine('verify',self.manifest,success=False)
        self.assertIn('byte 65536 expected',output)
        self.assertEqual(digest(self.target),before)
    def test_wrong_length_and_missing_file(self):
        self.start()
        path = self.root/'DATA/NEST/B00.BIN'; path.write_bytes(b'extra')
        self.assertIn('size/type mismatch',self.run_engine('verify',self.manifest,success=False))
        path.unlink()
        self.run_engine('verify',self.manifest,success=False)
    def test_unknown_entry_blocks_all_cleanup(self):
        self.start()
        (self.root/'DATA/NEST/MINE.TXT').write_bytes(b'preserve this too')
        before = digest(self.target)
        self.assertIn('Unrecorded entry',self.run_engine('cleanup',self.manifest,success=False))
        self.assertEqual(digest(self.target),before)
    def test_wrong_marker_blocks_cleanup_and_verify(self):
        self.start()
        (self.root/'OWNER.ID').write_bytes(b'bad owner')
        before = digest(self.target)
        self.run_engine('cleanup',self.manifest,success=False)
        self.run_engine('verify',self.manifest,success=False)
        self.assertEqual(digest(self.target),before)
    def test_cancel_and_incomplete_cleanup(self):
        self.start(success=False,fault='cancel')
        self.assertFalse((self.root/'DONE.ID').exists())
        self.run_engine('verify',self.manifest,success=False)
        self.run_engine('cleanup',self.manifest)
        self.assertFalse(self.root.exists())
    def test_cancel_during_final_audit_does_not_certify_completion(self):
        self.start(success=False,fault='cancel-final')
        self.assertFalse((self.root/'DONE.ID').exists())
        self.run_engine('verify',self.manifest,success=False)
        self.run_engine('cleanup',self.manifest)
    def test_cleanup_final_cancellation_and_rmdir_failure_are_retryable(self):
        for fault in ('cancel-cleanup','root-delete'):
            with self.subTest(fault=fault):
                self.start()
                self.run_engine('cleanup',self.manifest,success=False,fault=fault)
                self.assertTrue((self.root/'OWNER.ID').exists())
                self.run_engine('cleanup',self.manifest)
                self.assertFalse(self.root.exists())
                self.manifest.unlink()
    def test_create_and_resize_failures_preserve_partial_runs(self):
        for fault in ('create','resize','data-write','shortdata'):
            with self.subTest(fault=fault):
                self.start(success=False,fault=fault)
                self.assertTrue((self.root/'OWNER.ID').exists())
                self.assertFalse((self.root/'DONE.ID').exists())
                self.run_engine('cleanup',self.manifest)
                self.manifest.unlink()
    def test_local_failures_stop_before_target_writes(self):
        for fault in ('short','close'):
            self.run_engine('start',self.target,self.local,0,1,success=False,fault=fault)
            self.assertEqual([p.name for p in self.target.iterdir()],['KEEP.TXT'])
    def test_invalid_manifest_and_log_read_failures(self):
        manifest = self.local/'BAD.DAT'
        for content in (b'',b'NWSTEST1\n0 4294967296\n/tmp/foo\n',
                        b'NWSTEST1\n0 1\n/tmp/foo\nextra',b'NWSTEST1\n0 1\n/tmp/\0foo\n'):
            manifest.write_bytes(content)
            self.run_engine('cleanup',manifest,success=False)
        self.start()
        before = digest(self.target)
        self.run_engine('verify',self.manifest,success=False,fault='read')
        self.assertEqual(digest(self.target),before)
    def test_existing_run_names_are_not_overwritten(self):
        self.start(seed=1)
        first = self.root; before = digest(first)
        self.run_engine('start',self.target,self.local,0,2)
        self.assertTrue((self.target/'NWT0002/DONE.ID').exists())
        self.assertEqual(digest(first),before)

if __name__ == '__main__': unittest.main()
