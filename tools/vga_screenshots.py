# SPDX-License-Identifier: GPL-3.0-only
"""Capture a VGA gallery from actual DOSBox VRAM using isolated sample drives."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import random
import shutil
import subprocess
import sys
import zipfile
from PIL import Image, ImageDraw
from demo_drive import populate, text
from build_metadata import write as write_metadata
from vram import decode

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'tests'))
from ui_build import require_prebuilt, build_ui, source_key
EXIT, ESC, ENTER = 0x4400, 27, 13


class Script:
    def __init__(self): self.keys, self.frames = [], []
    def press(self,*keys): self.keys.extend(keys); return self
    def frame(self,name): self.frames.append((name,len(self.keys))); return self
    def enter(self,value): return self.press(21,*map(ord,value),ENTER)
    def find(self,value): return self.press(6).enter(value)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,default=ROOT/'screenshots')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True,exist_ok=True)
    previous = json.loads((out/'manifest.json').read_text()).get('screenshots',[]) if (out/'manifest.json').exists() else []
    base = ROOT/'build/vga-screenshots'; base.mkdir(parents=True,exist_ok=True)
    write_metadata()
    try: require_prebuilt()
    except (AssertionError,FileNotFoundError): build_ui()
    renderer_key = source_key()
    samples = base/'samples'
    populate(samples)
    # Extend only gallery samples, never the interactive make run demo drives.
    for month in range(1,41):
        p = samples/'D/DOCS/REPORTS'/f'REP{month:03}.TXT'
        p.parent.mkdir(parents=True,exist_ok=True)
        p.write_bytes(text(f'NORTH RIDGE - REPORT {month:03}\n\nEquipment inspection complete.\n'))
    (samples/'D/DATA/SURVEY.BIN').write_bytes(random.Random(480).randbytes(1024*1024))
    entries = []

    def export(name,raw,scenario,description=None):
        data = raw.read_bytes()
        assert data[:3] == b'\x03\xe0\x01', (name,'not 640x480 VGA')
        path = out/(name+'.png'); image = decode(raw,path)
        assert image.size == (640,480)
        entries.append({'file':path.name,'title':description or name.replace('-',' ').title(),
                        'scenario':scenario,'capture':str(raw.relative_to(ROOT)),
                        'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})

    def run(name,script,left='D:\\',right='E:\\',progress=False,setup=None):
        work = base/name
        if work.exists(): shutil.rmtree(work)
        work.mkdir(); shutil.copytree(samples,work/'drives')
        if setup: setup(work/'drives')
        shutil.copy2(ROOT/'build/video/UITEST.EXE',work/'UITEST.EXE')
        script.press(EXIT)
        (work/'KEYS.TXT').write_text('\n'.join(f'{key:x}' for key in script.keys)+'\n')
        (work/'CAPTURE.CONF').write_text('[cpu]\ncycles='+('3000' if name=='copy' else '100000')+'\n')
        command = ['dosbox','-conf',str(ROOT/'tools/dosbox.conf'),'-conf',str(work/'CAPTURE.CONF'),'-machine','vgaonly']
        for c in [f'mount c "{work}"',f'mount d "{work}/drives/D"',f'mount e "{work}/drives/E"',
                  'c:',f'UITEST /vga {left} {right}','exit']:
            command += ['-c',c]
        with (work/'DOSBOX.LOG').open('w') as log:
            subprocess.run(command,env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy'),
                           stdout=log,stderr=log,check=True,timeout=120)
        # Exhausted input captures one extra frame. A clean script ends on F10.
        captures = list(work.glob('F*.BIN'))
        assert len(captures)==len(script.keys),(name,'script did not complete',len(captures),len(script.keys))
        for title,frame in script.frames: export(title,work/f'F{frame:03}.BIN',name)
        if progress:
            states = [(p,list(map(int,p.read_text().split()))) for p in sorted(work.glob('P*.TXT'))]
            counting = [(p,s) for p,s in states if s[0]]
            partial = [(p,s) for p,s in states if not s[0] and s[6] and 0<s[5]<s[6]]
            completed = [(p,s) for p,s in states if not s[0] and s[1] and s[3]==s[1] and s[7]==s[2]]
            if name=='copy': assert partial, 'No in-flight copy captured'
            if counting: export(name+'-counting',counting[-1][0].with_suffix('.BIN'),name)
            if partial:
                p,s = min(partial,key=lambda pair:abs(pair[1][5]/pair[1][6]-0.5))
                export(name+'-progress',p.with_suffix('.BIN'),name)
            if completed: export(name+'-finished',completed[-1][0].with_suffix('.BIN'),name)
        print(f'Captured {name}: {len(script.frames)} selected frames',flush=True)

    s=Script().frame('01-work-and-backup-drives').press(9).frame('02-backup-pane-active').press(9)
    s.find('SCENERY.PCX').frame('03-image-file-selected')
    s.press(16).frame('04-mark-pattern-dialog').enter('*.TXT').frame('05-documents-marked')
    s.press(14).enter('*.TXT').press(ord('+')).frame('06-all-files-marked').press(ord('-'))
    s.press(*map(ord,'DIR *.TXT')).frame('07-command-line')
    run('desktop',s)

    s=Script().frame('08-documents-folder').find('REPORTS').press(ENTER).frame('09-reports-first-page')
    s.press(0x5100).frame('10-reports-scrolled').press(0x4f00).frame('11-reports-last-page')
    run('documents',s,left='D:\\DOCS')
    for name,left,right,title in [
        ('projects','D:\\PROJECTS\\TERMINAL','D:\\PROJECTS','12-project-source-tree'),
        ('applications','D:\\APPS','D:\\GAMES','13-applications-and-games'),
        ('images','D:\\IMAGES','D:\\DATA','14-images-and-data'),
        ('archives','D:\\ARCHIVES','E:\\ARCHIVES','15-archive-folders')]:
        run(name,Script().frame(title),left,right)

    s=Script().press(19).frame('16-sort-menu').press(ord('s')).frame('17-sorted-by-size')
    s.press(19,ord('e')).frame('18-sorted-by-extension').press(19,ord('r')).frame('19-reverse-sort')
    run('sorting',s)

    s=Script().press(0x3b00).frame('20-help-first-page').press(0x5100).frame('21-help-second-page')
    s.press(0x5100).frame('22-help-third-page').press(0x4f00).frame('23-help-last-page').press(ESC)
    s.press(0x6800).frame('24-about').press(ESC)
    s.press(0x3c00).frame('25-display-mode-dialog').press(ESC)
    s.press(20).frame('26-storage-test-menu').press(ESC)
    s.press(0x4300).frame('27-change-drive-dialog').press(ESC)
    s.press(6).frame('28-find-file-dialog').press(ESC)
    run('help-and-menus',s)

    s=Script().find('MANUAL.TXT').press(0x3d00).frame('29-text-viewer')
    s.press(0x5100).frame('30-text-viewer-next-page')
    s.press(0x4100).frame('31-viewer-search-dialog').enter('copies').frame('32-text-search-result')
    s.press(0x3e00).frame('33-text-as-hex').press(ESC)
    run('text-viewer',s,left='D:\\DOCS')

    s=Script().find('STOCK.DAT').press(0x5600).frame('34-binary-hex-viewer')
    s.press(0x5100).frame('35-hex-viewer-next-page').press(7).frame('36-offset-dialog')
    s.enter('0x4000').frame('37-hex-viewer-at-offset').press(ESC)
    run('hex-viewer',s,left='D:\\DATA')

    s=Script().find('SURVEY.BIN').press(0x3f00).frame('38-copy-destination-dialog').press(ENTER)
    s.frame('39-copy-completed')
    run('copy',s,left='D:\\DATA',progress=True)

    def existing(drives):
        shutil.copy2(drives/'D/DATA/SURVEY.BIN',drives/'E/SURVEY.BIN')
    s=Script().find('SURVEY.BIN').press(0x3f00,ENTER).frame('40-overwrite-or-skip-dialog')
    s.press(ord('s')).frame('41-existing-file-skipped')
    run('overwrite',s,left='D:\\DATA',setup=existing)

    s=Script().find('LETTER.TXT').press(0x4000).frame('42-move-destination-dialog').press(ESC)
    s.press(0x4200).frame('43-delete-confirmation').press(ord('n'))
    s.press(0x4100).frame('44-new-folder-dialog').enter('NEWFILES').frame('45-new-folder-created')
    s.press(0x4300).enter('D:\\MISSING').frame('46-invalid-path-notice').press(ENTER)
    run('file-dialogs',s,left='D:\\DOCS')

    s=Script().find('PACK.ZIP').press(21).frame('47-zip-destination-dialog').press(ENTER)
    s.frame('48-zip-existing-file-dialog').press(ord('s')).frame('49-zip-extracted')
    run('zip-extraction',s,progress=True)
    s=Script().find('PACK.ARJ').press(21).frame('50-arj-destination-dialog').press(ENTER).frame('51-arj-extracted')
    run('arj-extraction',s,progress=True)

    s=Script().press(20,ord('q')).frame('52-storage-test-target').press(ENTER)
    s.frame('53-storage-test-log-drive').enter('E:\\RESTORE')
    s.frame('54-storage-test-seed').enter('42').frame('55-storage-test-confirmation')
    s.press(ord('y')).frame('56-storage-test-result').press(ENTER)
    run('storage-test',s,left='D:\\TEMP')

    # Native captures remain untouched; only the overview uses resized thumbnails.
    columns,tw,th = 4,320,264
    sheet = Image.new('RGB',(columns*tw,((len(entries)+columns-1)//columns)*th),'#d8d8d8')
    draw = ImageDraw.Draw(sheet)
    for i,entry in enumerate(entries):
        x,y=(i%columns)*tw,(i//columns)*th
        with Image.open(out/entry['file']) as image:
            sheet.paste(image.resize((320,240),Image.Resampling.NEAREST),(x,y))
        draw.text((x+6,y+245),entry['file'].removesuffix('.png'),fill='black')
    sheet.save(out/'overview.png')
    assert source_key()==renderer_key, 'Sources changed during capture; regenerate the gallery'
    (out/'manifest.json').write_text(json.dumps({'mode':'VGA 640x480, 16 colors',
        'renderer_source_key':renderer_key,
        'method':'DOSBox VRAM captures; scripted input with the production renderer',
        'screenshots':entries},indent=2)+'\n')
    lines=['# Nightwatch VGA screenshots','',
           f'{len(entries)} native 640×480 screenshots captured from DOSBox video memory.',
           'The scripted navigator uses the application renderer and isolated fictional drives.',
           'The images contain no added labels or overlays. `overview.png` is a thumbnail contact sheet.',
           '', '[View the overview](overview.png)', '',
           'Regenerate from the repository root with `python3 tools/vga_screenshots.py`.',
           'Requires the DOS build tools, DOSBox and Pillow; the checked capture executable is reused or rebuilt.',
           'Gallery operations use disposable copies under `build/vga-screenshots/` and preserve `build/demo/`.',
           '', '| Screenshot | Scene |','| --- | --- |']
    lines += [f'| [{e["file"]}]({e["file"]}) | {e["title"]} |' for e in entries]
    (out/'README.md').write_text('\n'.join(lines)+'\n')
    # Remove only unchanged outputs recorded by the previous generated manifest.
    current = {entry['file'] for entry in entries}
    for entry in previous:
        path = out/entry['file']
        if path.parent==out and path.name not in current and path.suffix=='.png' and path.is_file():
            if hashlib.sha256(path.read_bytes()).hexdigest()==entry.get('sha256'): path.unlink()
    with zipfile.ZipFile(out/'vga-gallery.zip','w',compression=zipfile.ZIP_DEFLATED) as bundle:
        for filename in [*(entry['file'] for entry in entries),'overview.png','README.md','manifest.json']:
            bundle.write(out/filename,filename)
    print(f'Saved {len(entries)} VGA screenshots, overview, manifest and ZIP to {out}',flush=True)


if __name__ == '__main__': main()
