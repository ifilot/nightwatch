# SPDX-License-Identifier: GPL-3.0-only
"""Virtual archive listings, navigation, selected extraction and integrity."""
from pathlib import Path
import hashlib, os, shutil, subprocess as sp, tempfile, unittest, zipfile
from arj import archive, header
ROOT=Path(__file__).resolve().parents[1]
BIN=ROOT/'build/test_browse'
ENV=dict(os.environ,ASAN_OPTIONS='detect_leaks=0')

def compile_host():
    modules=['src/CORE.C','src/ARCHIVE.C','src/ZIP.C','src/ARJ.C','src/ARJDEC.C','src/BROWSE.C']
    sp.run(['cc','-x','c','-std=c89','-Wall','-Wextra','-Werror','-pedantic','-DNW_HOST',
            '-Dz_off_t=long','-Isrc','-Ivendor/zlib','-fsyntax-only',*modules],cwd=ROOT,check=True)
    sp.run(['cc','-x','c','-std=c89','-DNW_HOST','-DZ_SOLO','-DNO_GZIP','-Isrc','-Itests','-Ivendor/zlib',
            '-fsanitize=address,undefined','-fno-omit-frame-pointer','-no-pie','-g',*modules,
            'tests/fs_host.c','tests/BROWTEST.C',*map(str,(ROOT/'vendor/zlib').glob('*.c')),
            '-o',str(BIN)],cwd=ROOT,check=True)

class BrowseTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls): compile_host()
    def setUp(self):
        self.tmp=tempfile.TemporaryDirectory(prefix='nwbrowse-',dir='/tmp')
        self.work=Path(self.tmp.name); self.out=self.work/'OUT'; self.out.mkdir()
    def tearDown(self): self.tmp.cleanup()
    def run_browser(self,source,*commands,target=None,success=True):
        before=source.read_bytes()
        r=sp.run([str(BIN),str(source),str(target or self.out),*commands],text=True,capture_output=True,env=ENV,timeout=45)
        self.assertEqual(r.returncode,0 if success else 1,r.stdout+r.stderr)
        self.assertEqual(r.stderr,''); self.assertEqual(source.read_bytes(),before)
        return r.stdout
    def make_zip(self,entries):
        source=self.work/'TEST.ZIP'
        with zipfile.ZipFile(source,'w',compression=zipfile.ZIP_DEFLATED) as z:
            for name,data in entries: z.writestr(name,data)
        return source
    def make_arj(self,entries):
        source=self.work/'TEST.ARJ'
        source.write_bytes(archive(*(header(name.encode(),data) for name,data in entries)))
        return source
    def test_implicit_directories_navigation_refresh_and_exit(self):
        for make in (self.make_zip,self.make_arj):
            source=make([('SUB/DEEP/ONE.TXT',b'one'),('SUB/TWO.TXT',b'two'),('TOP.TXT',b'top')])
            listing=self.run_browser(source)
            self.assertIn('ENTRY SUB 16 0',listing); self.assertNotIn('ENTRY ONE.TXT',listing)
            self.assertEqual(list(self.out.iterdir()),[])
            listing=self.run_browser(source,'SUB','DEEP','..','refresh','home','second')
            self.assertIn(str(source)+'/SUB',listing); self.assertIn('SELECTED DEEP',listing)
            listing=self.run_browser(source,'SUB','..','..')
            self.assertIn('active=0',listing); self.assertIn('SELECTED '+source.name,listing)
            self.assertEqual(list(self.out.iterdir()),[])
    def test_selected_and_marked_subtree_extraction(self):
        for make in (self.make_zip,self.make_arj):
            source=make([('SUB/DEEP/ONE.TXT',b'one'),('SUB/TWO.TXT',b'two'),('TOP.TXT',b'top')])
            self.run_browser(source,'select=SUB','copy')
            self.assertEqual((self.out/'SUB/DEEP/ONE.TXT').read_bytes(),b'one')
            self.assertFalse((self.out/'TOP.TXT').exists())
            shutil.rmtree(self.out); self.out.mkdir()
            self.run_browser(source,'SUB','mark=*.TXT','copy')
            self.assertEqual((self.out/'TWO.TXT').read_bytes(),b'two')
            self.assertFalse((self.out/'DEEP').exists())
            shutil.rmtree(self.out); self.out.mkdir()
    def test_view_uses_cursor_and_does_not_replace_existing_temp(self):
        for make in (self.make_zip,self.make_arj):
            source=make([('A.TXT',b'aaa'),('B.TXT',b'bbb')]); temp=self.work/'VIEW.TMP'
            self.run_browser(source,'mark=*.TXT','select=B.TXT','view',target=temp)
            self.assertEqual(temp.read_bytes(),b'bbb')
            self.run_browser(source,'select=A.TXT','view',target=temp,success=False)
            self.assertEqual(temp.read_bytes(),b'bbb'); temp.unlink()
    def test_real_compressed_archives(self):
        for source in [ROOT/'tests/fixtures/zip/WINBEST.ZIP',ROOT/'tests/fixtures/arj/METHOD1.ARJ',ROOT/'tests/fixtures/arj/METHOD4.ARJ']:
            temp=self.work/'VIEW.TMP'
            name='BYTES.BIN' if source.suffix=='.ZIP' else 'DATA.BIN'
            self.run_browser(source,'SUB','select='+name,'view',target=temp)
            manifest=__import__('json').loads((source.parent/'manifest.json').read_text())
            self.assertEqual(hashlib.sha256(temp.read_bytes()).hexdigest(),manifest['SUB/'+name]['sha256'])
            temp.unlink()
    def test_truncation_limit_and_empty_archives(self):
        source=self.make_zip([(f'F{i:07d}.TXT',b'x') for i in range(520)])
        listing=self.run_browser(source)
        self.assertIn('count=512 truncated=1',listing)
        source=self.make_zip([])
        self.assertIn('count=1',self.run_browser(source))
        self.run_browser(source,'MISSING',success=False)
    def test_unsafe_or_conflicting_members_rejected_before_output(self):
        for entries in ([('../BAD.TXT',b'x')],[('DIR',b'x'),('DIR/FILE.TXT',b'y')]):
            for make in (self.make_zip,self.make_arj):
                source=make(entries)
                self.run_browser(source,success=False)
                self.assertEqual(list(self.out.iterdir()),[])
    def test_long_source_filename_is_safe_on_parent_exit(self):
        original=self.make_zip([('A.TXT',b'hello')])
        source=self.work/'A_LONG_ARCHIVE_FILENAME.ZIP'; original.rename(source)
        self.run_browser(source,'..')
    def test_cancel_and_io_failure_cleanup(self):
        for make in (self.make_zip,self.make_arj):
            source=make([('A.TXT',b'hello'*4000)])
            for mode in ('cancel','fault=write','fault=close','fault=read'):
                temp=self.work/'VIEW.TMP'
                self.run_browser(source,'select=A.TXT',mode,'view',target=temp,success=False)
                self.assertFalse(temp.exists(),mode)
            self.run_browser(source,'select=A.TXT','cancel','copy',success=False)
            self.assertEqual(list(self.out.iterdir()),[])
            self.run_browser(source,'nested',success=False)
    def test_cannot_extract_over_source_archive(self):
        for make,name in ((self.make_zip,'TEST.ZIP'),(self.make_arj,'TEST.ARJ')):
            source=make([(name,b'bad replacement')])
            self.run_browser(source,'select='+name,'copy',target=self.work,success=False)
            self.assertFalse(list(self.work.glob('*.TMP')))
    def test_payload_corruption_cleans_only_owned_temp(self):
        source=self.make_arj([('A.TXT',b'hello')]); blob=bytearray(source.read_bytes())
        at=blob.index(b'hello'); blob[at]^=1; source.write_bytes(blob)
        temp=self.work/'VIEW.TMP'
        self.run_browser(source,'select=A.TXT','view',target=temp,success=False)
        self.assertFalse(temp.exists())
        self.assertEqual(list(self.out.iterdir()),[])

if __name__=='__main__': unittest.main()
