# SPDX-License-Identifier: GPL-3.0-only
"""Actual Windows/Linux ZIP fixtures, DEFLATE matrix and extraction fault tests."""
from pathlib import Path
import hashlib, json, os, random, struct, subprocess as sp, tempfile, unittest, warnings, zipfile, zlib
ROOT = Path(__file__).resolve().parents[1]
FIXTURES = ROOT/'tests/fixtures/zip'
BINARY = ROOT/'build/test_zip'
MANIFEST = json.loads((FIXTURES/'manifest.json').read_text())
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0')
# LeakSanitizer cannot run under this workspace's ptrace isolation. The C driver
# asserts allocator balance on every path; ASan/UBSan remain enabled.
def compile_host():
    sp.run(['cc','-x','c','-std=c89','-Wall','-Wextra','-Werror','-pedantic',
            '-Dz_off_t=long','-DNW_HOST','-DNW_ZIP_TEST','-DZ_SOLO','-DNO_GZIP',
            '-Isrc','-Ivendor/zlib','-fsyntax-only','src/ZIP.C'],cwd=ROOT,check=True)
    sp.run(['cc','-x','c','-std=c89','-DNW_HOST','-DNW_ZIP_TEST','-DZ_SOLO','-DNO_GZIP',
            '-Isrc','-Itests','-Ivendor/zlib','-fsanitize=address,undefined',
            '-fno-omit-frame-pointer','-no-pie','-g','src/CORE.C','src/ZIP.C',
            'tests/fs_host.c','tests/ZIPTEST.C',
            *map(str, sorted((ROOT/'vendor/zlib').glob('*.c'))),'-o',str(BINARY)],cwd=ROOT,check=True)

def raw_zip(data=b'hello'*1000, *, level=6, strategy=zlib.Z_DEFAULT_STRATEGY,
            descriptor=0, name=b'HELLO.TXT', trailing=b'', comment=b'', extra=b'', flags=0):
    c=zlib.compressobj(level,zlib.DEFLATED,-15,8,strategy)
    packed=c.compress(data)+c.flush()+trailing
    crc=zlib.crc32(data); size=len(data); cs=len(packed)
    flags |= 8 if descriptor else 0
    local=struct.pack('<IHHHHHIIIHH',0x04034b50,20,flags,8,0,0,
                      0 if descriptor else crc,0 if descriptor else cs,0 if descriptor else size,len(name),len(extra))
    body=local+name+extra+packed
    if descriptor:
        body+=(struct.pack('<I',0x08074b50) if descriptor==16 else b'')+struct.pack('<III',crc,cs,size)
    cd=struct.pack('<IHHHHHHIIIHHHHHII',0x02014b50,20,20,flags,8,0,0,crc,cs,size,len(name),len(extra),0,0,0,32,0)+name+extra
    end=struct.pack('<IHHHHIIH',0x06054b50,0,0,1,1,len(cd),len(body),len(comment))
    return body+cd+end+comment

def central(blob): return blob.index(b'PK\x01\x02')
def put(blob, at, fmt, value):
    b=bytearray(blob); struct.pack_into(fmt,b,at,value); return bytes(b)

class ZipTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): compile_host()
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory(prefix='nwzip-',dir='/tmp')
        self.work=Path(self.temp.name); self.dest=self.work/'OUT'; self.dest.mkdir()
    def tearDown(self): self.temp.cleanup()
    def run_zip(self, archive, mode='overwrite', success=True):
        if isinstance(archive,bytes):
            source=self.work/'IN.ZIP'; source.write_bytes(archive)
        else: source=archive
        before=hashlib.sha256(source.read_bytes()).hexdigest()
        p=sp.run([str(BINARY),str(source),str(self.dest),mode],env=ENV,text=True,capture_output=True,timeout=30)
        self.assertEqual(p.returncode,0 if success else 1,p.stdout+p.stderr)
        self.assertNotIn('runtime error:',p.stderr); self.assertEqual(p.stderr,'')
        self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),before,'source archive changed')
        if not success: self.assertIn('ok=0',p.stdout)
        return p.stdout
    def clean(self):
        import shutil
        shutil.rmtree(self.dest); self.dest.mkdir()
    def check_manifest(self):
        actual={p.relative_to(self.dest).as_posix():p for p in self.dest.rglob('*') if p.is_file()}
        self.assertEqual(set(actual),set(MANIFEST))
        for name,p in actual.items():
            self.assertEqual(p.stat().st_size,MANIFEST[name]['size'])
            self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),MANIFEST[name]['sha256'])
        self.assertTrue((self.dest/'SUB/EMPTY').is_dir())
    def test_actual_windows_linux_archives(self):
        for archive in sorted(FIXTURES.glob('*.ZIP')):
            with self.subTest(archive=archive.name):
                result=self.run_zip(archive); self.assertIn('files=5 bytes=428803',result)
                self.check_manifest(); self.clean()
    def test_deflate_levels_and_strategies(self):
        data=bytes(range(256))*400+b'abcdef'*20000
        for level in (0,1,6,9):
            for strategy in (zlib.Z_DEFAULT_STRATEGY,zlib.Z_FIXED,zlib.Z_HUFFMAN_ONLY,zlib.Z_RLE,zlib.Z_FILTERED):
                with self.subTest(level=level,strategy=strategy):
                    self.run_zip(raw_zip(data,level=level,strategy=strategy))
                    self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),data); self.clean()
    def test_descriptors_comments_extra_and_utf8_ascii(self):
        for descriptor in (0,12,16):
            for data in (b'',b'abc'*20000):
                blob=raw_zip(data,descriptor=descriptor,comment=b'x'*65535,
                             extra=b'\xfe\xca\x03\x00abc',flags=2048)
                self.run_zip(blob); self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),data); self.clean()
        # False end-record signatures inside a comment must not shadow the real one.
        self.run_zip(raw_zip(comment=b'PK\x05\x06'+b'\0'*40))
    def test_long_distance_window(self):
        r=random.Random(2026).randbytes(30000); data=r+r[:16000]+b'END'
        self.run_zip(raw_zip(data,level=9)); self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),data)
    def test_more_entries_than_pane_cache(self):
        path=self.work/'MANY.ZIP'
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for i in range(520): z.writestr(f'F{i:07d}.TXT',str(i).encode())
        self.assertIn('files=520',self.run_zip(path)); self.assertEqual(len(list(self.dest.iterdir())),520)
    def test_empty_archive(self):
        path=self.work/'EMPTY.ZIP'
        with zipfile.ZipFile(path,'w'): pass
        self.assertIn('files=0 bytes=0',self.run_zip(path)); self.assertEqual(list(self.dest.iterdir()),[])
    def test_reject_unsafe_paths_and_special_entries_before_writing(self):
        names=['../BAD.TXT','/BAD.TXT','C:/BAD.TXT','A/../BAD.TXT','A//BAD.TXT',
               'CON.TXT','LPT1','LONGFILENAME.TXT','é.TXT','BAD .TXT','A/./BAD.TXT',
               '..\\BAD.TXT','\\BAD.TXT','A\\..\\BAD.TXT']
        for name in names:
            with self.subTest(name=name):
                path=self.work/'BAD.ZIP'
                with zipfile.ZipFile(path,'w') as z:
                    z.writestr('GOOD.TXT',b'valid first entry'); z.writestr(name,b'bad')
                self.run_zip(path,success=False); self.assertEqual(list(self.dest.iterdir()),[])
        for names in [('A.TXT','a.txt'),('SUB','SUB/F.TXT'),('SUB/F.TXT','SUB'),('SUB/','sub/'),('A/B.TXT','A\\B.TXT')]:
            with self.subTest(names=names):
                path=self.work/'BAD.ZIP'
                with zipfile.ZipFile(path,'w') as z:
                    for name in names: z.writestr(name,b'')
                self.run_zip(path,success=False); self.assertEqual(list(self.dest.iterdir()),[])
        path=self.work/'LINK.ZIP'
        with zipfile.ZipFile(path,'w') as z:
            i=zipfile.ZipInfo('LINK'); i.create_system=3; i.external_attr=0o120777<<16; z.writestr(i,b'outside')
        self.run_zip(path,success=False); self.assertEqual(list(self.dest.iterdir()),[])
    def test_existing_ancestor_file_and_symlink(self):
        (self.dest/'SUB').write_bytes(b'original')
        self.run_zip(FIXTURES/'WINBEST.ZIP',success=False)
        self.assertEqual(list(self.dest.iterdir()),[self.dest/'SUB'])
        self.assertEqual((self.dest/'SUB').read_bytes(),b'original'); self.clean()
        outside=self.work/'OUTSIDE'; outside.mkdir(); (self.dest/'SUB').symlink_to(outside,target_is_directory=True)
        self.run_zip(FIXTURES/'WINBEST.ZIP',success=False); self.assertEqual(list(outside.iterdir()),[])
    def test_unsupported_formats(self):
        blob=raw_zip(); cd=central(blob); end=len(blob)-22
        variants=[put(blob,6,'<H',1),put(blob,cd+8,'<H',1),
                  put(blob,cd+10,'<H',9),put(blob,end+4,'<H',1),
                  put(blob,end+10,'<H',65535),put(blob,cd+24,'<I',0xffffffff),
                  put(blob,cd+42,'<I',0xffffffff),raw_zip(extra=b'\x01\x00\0\0'),
                  put(blob,cd+6,'<H',45)]
        for b in variants:
            self.run_zip(b,success=False); self.assertEqual(list(self.dest.iterdir()),[])
    def test_corruption_and_truncation_preserve_original(self):
        blob=raw_zip(); cd=central(blob)
        crc=put(put(blob,14,'<I',1),cd+16,'<I',1)
        size=put(put(blob,22,'<I',1),cd+24,'<I',1)
        grow=put(put(blob,22,'<I',5001),cd+24,'<I',5001)
        corrupt=put(blob,39,'<B',7)  # compressed stream starts after 30+9 filename bytes
        variants=[b'not a zip',blob[:-1],blob[:100],crc,size,grow,corrupt,
                  raw_zip(trailing=b'junk'),put(blob,cd+28,'<H',127),
                  put(blob,30,'<B',ord('X')),put(blob,cd+20,'<I',999999),
                  raw_zip(descriptor=16)[:-23]+b'X',raw_zip(extra=b'\xfe\xca\x08\0x')]
        for b in variants:
            (self.dest/'HELLO.TXT').write_bytes(b'original')
            self.run_zip(b,success=False)
            self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'original')
            self.assertEqual(list(self.dest.iterdir()),[self.dest/'HELLO.TXT'])
    def test_header_overlap_and_count(self):
        path=self.work/'TWO.ZIP'
        with zipfile.ZipFile(path,'w') as z:
            z.writestr('A.TXT',b'a'); z.writestr('B.TXT',b'b')
        blob=path.read_bytes(); cd=central(blob); second=blob.index(b'PK\x01\x02',cd+4)
        for b in [put(blob,second+42,'<I',0),put(blob,len(blob)-12,'<H',1),
                  put(blob,len(blob)-10,'<I',1)]:
            self.run_zip(b,success=False); self.assertEqual(list(self.dest.iterdir()),[])
    def test_overwrite_skip_cancel_short_write_and_faults(self):
        blob=raw_zip(b'abc'*2000)
        for mode in ('cancel','alloc1','alloc2','write','read','seek','stamp','close','attr','commit','refuse'):
            with self.subTest(mode=mode):
                (self.dest/'HELLO.TXT').write_bytes(b'original')
                self.run_zip(blob,mode,success=False)
                self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'original')
                self.assertEqual(list(self.dest.iterdir()),[self.dest/'HELLO.TXT']); self.clean()
        (self.dest/'HELLO.TXT').write_bytes(b'original')
        result=self.run_zip(blob,'skip'); self.assertIn('completed=0 skipped=1',result)
        self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'original')
        self.run_zip(blob,'short'); self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'abc'*2000)
    def test_cleanup_failure_reports_retained_temp(self):
        (self.dest/'HELLO.TXT').write_bytes(b'original')
        result=self.run_zip(raw_zip(),'cleanup',success=False)
        self.assertIn('Temporary remains:',result); self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'original')
        self.assertEqual(len(list(self.dest.glob('*.TMP'))),1)
    def test_source_archive_cannot_be_overwritten(self):
        archive=self.dest/'SELF.ZIP'; archive.write_bytes(raw_zip(name=b'SELF.ZIP'))
        self.run_zip(archive,success=False)
    def test_temporary_names_cannot_alias_destination(self):
        self.run_zip(raw_zip(name=b'NW0000.TMP'))
        self.assertEqual((self.dest/'NW0000.TMP').read_bytes(),b'hello'*1000)
        self.clean(); (self.dest/'NW0000.TMP').write_bytes(b'occupied')
        self.run_zip(raw_zip()); self.assertEqual((self.dest/'NW0000.TMP').read_bytes(),b'occupied')
    def test_prior_commits_survive_cancel(self):
        path=self.work/'TWO.ZIP'
        with zipfile.ZipFile(path,'w',compression=zipfile.ZIP_DEFLATED) as z:
            z.writestr('FIRST.TXT',b'first'); z.writestr('SECOND.TXT',b'second'*1000)
        self.run_zip(path,'cancel2',success=False)
        self.assertEqual((self.dest/'FIRST.TXT').read_bytes(),b'first')
        self.assertEqual(list(self.dest.iterdir()),[self.dest/'FIRST.TXT'])
    def test_retained_backup_on_restore_or_cleanup_failure(self):
        for mode in ('restore','backup-delete'):
            (self.dest/'HELLO.TXT').write_bytes(b'original')
            result=self.run_zip(raw_zip(),mode,success=False)
            self.assertIn('Original backup:',result)
            backups=list(self.dest.glob('*.BAK')); self.assertEqual(len(backups),1)
            self.assertEqual(backups[0].read_bytes(),b'original')
            self.assertEqual(list(self.dest.glob('*.TMP')),[])
            if mode=='backup-delete': self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'hello'*1000)
            else: self.assertFalse((self.dest/'HELLO.TXT').exists())
            self.clean()
    def test_source_close_failure_retains_completed_output(self):
        result=self.run_zip(raw_zip(),'close-source',success=False)
        self.assertIn('Close ZIP',result); self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'hello'*1000)
    def test_dos_readonly_attribute(self):
        path=self.work/'ATTR.ZIP'
        with zipfile.ZipFile(path,'w') as z:
            i=zipfile.ZipInfo('HELLO.TXT'); i.create_system=0; i.external_attr=1
            z.writestr(i,b'attributes')
        self.run_zip(path)
        self.assertEqual((self.dest/'HELLO.TXT').stat().st_mode & 0o222,0)
        self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'attributes')
    def test_relative_archive_and_destination(self):
        (self.work/'IN.ZIP').write_bytes(raw_zip())
        p=sp.run([str(BINARY),'IN.ZIP','OUT'],cwd=self.work,env=ENV,text=True,capture_output=True,timeout=10)
        self.assertEqual(p.returncode,0,p.stdout+p.stderr)
        self.assertEqual((self.dest/'HELLO.TXT').read_bytes(),b'hello'*1000)
    def test_fixture_hashes_and_windows_timestamps(self):
        hashes=json.loads((FIXTURES/'archive-sha256.json').read_text())
        for name,sha in hashes.items(): self.assertEqual(hashlib.sha256((FIXTURES/name).read_bytes()).hexdigest(),sha)
        self.run_zip(FIXTURES/'WINBEST.ZIP')
        import datetime
        dt=datetime.datetime.fromtimestamp((self.dest/'HELLO.TXT').stat().st_mtime)
        self.assertEqual(dt,datetime.datetime(2026,10,8,12,34,56))
    def test_bounded_random_mutations(self):
        blob=raw_zip(); r=random.Random(28)
        for _ in range(80):
            b=bytearray(blob); at=r.randrange(len(b)); b[at]^=1<<r.randrange(8)
            source=self.work/'MUTATE.ZIP'; source.write_bytes(b)
            p=sp.run([str(BINARY),str(source),str(self.dest)],env=ENV,text=True,capture_output=True,timeout=10)
            self.assertIn(p.returncode,(0,1),p.stdout+p.stderr); self.assertEqual(p.stderr,'')
            self.clean()

if __name__=='__main__': unittest.main()
