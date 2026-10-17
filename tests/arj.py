# SPDX-License-Identifier: GPL-3.0-only
"""Independent ARJ archives plus malformed streams, staging and fault checks."""
from pathlib import Path
import hashlib, json, os, random, shutil, struct, subprocess as sp, tempfile, unittest, zlib

ROOT = Path(__file__).resolve().parents[1]
FIX = ROOT/'tests/fixtures/arj'
BIN = ROOT/'build/test_arj'
ENV = dict(os.environ, ASAN_OPTIONS='detect_leaks=0', TZ='UTC')


def header(name=b'HELLO.TXT', data=b'', *, method=0, packed=None, kind=0, flags=0, host=0,
           size=None, crc=None, version=3, fixed=30, extra=b'', chapters=0):
    if packed is None: packed = data
    h = bytearray(fixed)
    h[:8] = bytes((fixed,3,version,host,flags,method,kind,0))
    struct.pack_into('<IIIIHHBB',h,8,0x28210000,len(packed),len(data) if size is None else size,
                     zlib.crc32(data) if crc is None else crc,0,32,chapters,0)
    body = h+name+b'\0\0'
    return b'\x60\xea'+struct.pack('<H',len(body))+body+struct.pack('<I',zlib.crc32(body))+extra+b'\0\0'+packed


def archive(*members):
    return header(b'TEST.ARJ',kind=2)+b''.join(members)+b'\x60\xea\0\0'


def bits(*fields):
    text = ''.join(f'{value:0{width}b}' for value,width in fields)
    text += '0'*(-len(text)%8)
    return bytes(int(text[i:i+8],2) for i in range(0,len(text),8))


def records(blob):
    at = 0
    while True:
        n = struct.unpack_from('<H',blob,at+2)[0]
        if not n: return
        start = at+4
        payload = start+n+6
        packed = 0 if at == 0 else struct.unpack_from('<I',blob,start+12)[0]
        yield at,start,n,payload,packed
        at = payload+packed


def mutate_header(blob, entry, offset, value):
    blob = bytearray(blob)
    at,start,n,_,_ = list(records(blob))[entry]
    blob[start+offset:start+offset+len(value)] = value
    struct.pack_into('<I',blob,start+n,zlib.crc32(blob[start:start+n]))
    return bytes(blob)


def compile_host():
    sp.run(['cc','-x','c','-std=c89','-Wall','-Wextra','-Werror','-pedantic',
            '-DNW_HOST','-Dz_off_t=long','-Isrc','-Ivendor/zlib','-fsyntax-only',
            'src/ARJ.C','src/ARJDEC.C','src/ARCHIVE.C'],cwd=ROOT,check=True)
    sp.run(['cc','-x','c','-std=c89','-DNW_HOST','-DNW_ARJ_TEST','-DZ_SOLO','-DNO_GZIP',
            '-Isrc','-Itests','-Ivendor/zlib','-fsanitize=address,undefined',
            '-fno-omit-frame-pointer','-no-pie','-g','src/CORE.C','src/ARCHIVE.C','src/ARJ.C','src/ARJDEC.C',
            'tests/fs_host.c','tests/ZIPTEST.C',*map(str,sorted((ROOT/'vendor/zlib').glob('*.c'))),
            '-o',str(BIN)],cwd=ROOT,check=True)


class ArjTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): compile_host()
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='nwarj-',dir='/tmp')
        self.work = Path(self.temp.name); self.out = self.work/'OUT'; self.out.mkdir()
    def tearDown(self): self.temp.cleanup()
    def clean(self): shutil.rmtree(self.out); self.out.mkdir()
    def run_arj(self, blob, mode='overwrite', success=True):
        if isinstance(blob,bytes):
            source = self.work/'IN.ARJ'; source.write_bytes(blob)
        else: source = blob
        before = source.read_bytes()
        p = sp.run([str(BIN),str(source),str(self.out),mode],env=ENV,capture_output=True,text=True,timeout=45)
        self.assertEqual(p.returncode,0 if success else 1,p.stdout+p.stderr)
        self.assertEqual(p.stderr,'')
        self.assertEqual(source.read_bytes(),before)
        return p.stdout
    def assert_clean(self):
        self.assertFalse(list(self.out.rglob('*.TMP')))
        self.assertFalse(list(self.out.rglob('*.BAK')))
    def test_independent_methods(self):
        manifest = json.loads((FIX/'manifest.json').read_text())
        hashes = json.loads((FIX/'archive-sha256.json').read_text())
        for method in range(5):
            source = FIX/f'METHOD{method}.ARJ'
            self.assertEqual(hashlib.sha256(source.read_bytes()).hexdigest(),hashes[source.name])
            methods = [source.read_bytes()[start+5] for _,start,_,_,_ in records(source.read_bytes())][1:]
            self.assertIn(method,methods)
            self.run_arj(source)
            actual = {p.relative_to(self.out).as_posix():p for p in self.out.rglob('*') if p.is_file()}
            self.assertEqual(set(actual),set(manifest))
            for name, p in actual.items():
                self.assertEqual(hashlib.sha256(p.read_bytes()).hexdigest(),manifest[name]['sha256'])
                self.assertEqual(p.stat().st_mtime,946684800)
            self.assertTrue((self.out/'SUB/EMPTY').is_dir())
            self.assert_clean(); self.clean()
    def test_long_distance_history(self):
        self.run_arj(FIX/'WINDOW.ARJ')
        self.assertEqual((self.out/'HELLO.TXT').read_bytes(),random.Random(26624).randbytes(26000)*3)
    def test_empty_and_stored(self):
        self.run_arj(archive())
        self.run_arj(archive(header(data=bytes(range(256))*300)))
        self.assertEqual((self.out/'HELLO.TXT').read_bytes(),bytes(range(256))*300)
    def test_singleton_huffman_and_method4_literals(self):
        payload = bits((1000,16),(0,5),(0,5),(0,9),(65,9),(0,5),(0,5))
        self.run_arj(archive(header(data=b'A'*1000,packed=payload,method=1)))
        self.assertEqual((self.out/'HELLO.TXT').read_bytes(),b'A'*1000)
        self.clean()
        payload = bits(*[(v,w) for byte in b'ARJ4' for v,w in ((0,1),(byte,8))])
        self.run_arj(archive(header(data=b'ARJ4',packed=payload,method=4)))
        self.assertEqual((self.out/'HELLO.TXT').read_bytes(),b'ARJ4')
    def test_preflight_rejects_paths_conflicts_and_features(self):
        for name in (b'../BAD.TXT',b'/BAD.TXT',b'C:BAD.TXT',b'LONGFILENAME.TXT',b'CON',b'A//B',b'A/../B',b'A\\..\\B',b'A\xff.TXT'):
            self.run_arj(archive(header(b'FIRST.TXT',b'ok'),header(name,b'bad')),success=False)
            self.assertEqual(list(self.out.iterdir()),[])
        for members in ((header(b'A',b'1'),header(b'a',b'2')),
                        (header(b'A',b'1'),header(b'A/B',b'2')),
                        (header(b'A/B',b'1'),header(b'A',b'2'))):
            self.run_arj(archive(*members),success=False)
            self.assertEqual(list(self.out.iterdir()),[])
        for kwargs in ({'flags':1},{'flags':4},{'flags':8},{'flags':128},
                       {'method':8},{'kind':1},{'kind':4},{'chapters':1},{'version':9},
                       {'kind':3,'data':b'x'},{'extra':b'\1\0x1234'},
                       {'host':2,'data':b'x'}):
            if kwargs.get('host') == 2:
                blob = mutate_header(archive(header(**kwargs)),1,26,b'\0\x40')
            else: blob = archive(header(**kwargs))
            self.run_arj(blob,success=False)
            self.assertEqual(list(self.out.iterdir()),[])
    def test_header_corruption_and_truncation(self):
        blob = archive(header(data=b'hello'))
        for end in range(len(blob)):
            self.run_arj(blob[:end],success=False)
            self.assertEqual(list(self.out.iterdir()),[])
        for i in (0,2,8,30,35):
            bad = bytearray(blob); bad[i] ^= 0x80
            self.run_arj(bytes(bad),success=False)
        self.run_arj(blob+b'junk',success=False)
        self.run_arj(b'MZ'+blob,success=False)
    def test_corrupt_streams_preserve_existing_files(self):
        old = self.out/'HELLO.TXT'; old.write_bytes(b'original')
        streams = [header(data=b'new',crc=123),header(data=b'new',size=4),
                   header(data=b'new',packed=b'',method=1),
                   header(data=b'new',packed=b'\0\0',method=1)]
        for lengths in ((1,1,1),(2,2,2)):
            payload = bits((1,16),(3,5),*[(v,3) for v in lengths],(0,2))
            streams.append(header(data=b'new',packed=payload,method=1))
        # A match before any literal may not read uninitialized history.
        payload = bits((1,16),(0,5),(0,5),(0,9),(256,9),(0,5),(0,5))
        streams.append(header(data=b'abc',packed=payload,method=1))
        for entry in streams:
            self.run_arj(archive(entry),success=False)
            self.assertEqual(old.read_bytes(),b'original'); self.assert_clean()
    def test_overwrite_skip_cancel_short_write_and_faults(self):
        blob = archive(header(data=b'new'*3000))
        old = self.out/'HELLO.TXT'
        old.write_bytes(b'original')
        for mode in ('skip','refuse','cancel','write','read','seek','stamp','close','attr','commit'):
            self.run_arj(blob,mode,success=mode=='skip')
            self.assertEqual(old.read_bytes(),b'original'); self.assert_clean()
        self.run_arj(blob,'short'); self.assertEqual(old.read_bytes(),b'new'*3000)
        self.run_arj(FIX/'METHOD1.ARJ','alloc1',success=False); self.assert_clean()
    def test_source_and_destination_aliases(self):
        outside = self.work/'REAL'; outside.mkdir()
        (self.out/'SUB').symlink_to(outside,target_is_directory=True)
        self.run_arj(archive(header(b'SUB/FILE.TXT',b'bad')),success=False)
        self.assertEqual(list(outside.iterdir()),[])
        self.run_arj(archive(header(b'IN.ARJ',b'bad')),success=True)
        # Point the target at the archive's own directory.
        self.out = self.work
        self.run_arj(archive(header(b'IN.ARJ',b'bad')),success=False)
    def test_more_than_pane_cache_and_payload_mutations(self):
        self.run_arj(archive(*(header(f'F{i:07d}'.encode(),b'x') for i in range(520))))
        self.assertEqual(len(list(self.out.iterdir())),520); self.clean()
        original = (FIX/'METHOD1.ARJ').read_bytes()
        _,_,_,payload,length = list(records(original))[1]
        for i in random.Random(130).sample(range(length),min(length,40)):
            blob = bytearray(original); blob[payload+i] ^= 0x55
            self.run_arj(bytes(blob),success=False); self.assert_clean(); self.clean()

if __name__ == '__main__': unittest.main()
