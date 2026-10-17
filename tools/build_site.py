#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build a self-contained Nightwatch/DOSBox site, following msdos-kakuro.

The pinned emulator binaries and corresponding sources are checked before use.
The resulting site makes no third-party runtime requests and works at subpaths.
"""
from pathlib import Path
import argparse
import hashlib
import html
import json
import re
import shutil
import subprocess
import tarfile
import tempfile
import urllib.request
from zipfile import ZipFile, ZipInfo, ZIP_DEFLATED
from demo_drive import populate

ROOT = Path(__file__).resolve().parents[1]
COMMIT = '6bbf0925d221a7304ea8e0dda1e89b8dc01569b2'
DOSBOX_COMMIT = '528578d5a6f76afd4d5ef4e722b5670826f1183a'
DOWNLOADS = (
    ('emulators-8.5.2.tgz', 'https://registry.npmjs.org/emulators/-/emulators-8.5.2.tgz',
     '99a3e6f257726749915f4dc6828d82cd9834969aa69cc2dcc5ed8babfb323cb4'),
    ('emulators-source.tgz', f'https://codeload.github.com/js-dos/emulators/tar.gz/{COMMIT}',
     '9b7354c2cc853681b99eb288bcdc7e5e0ff7fd2e821485c9b20c12472fbc822e'),
    ('dosbox-source.tgz', f'https://codeload.github.com/js-dos/dosbox/tar.gz/{DOSBOX_COMMIT}',
     '797efaf22246bbc5b2e6be0a6c791a58addd2bca330e0aa93232780a77e06019'),
)
DOSBOX_CONF = r'''[sdl]
autolock=false
[dosbox]
machine=svga_s3
memsize=8
[cpu]
core=normal
cputype=auto
cycles=fixed 15000
[mixer]
nosound=true
[sblaster]
sbtype=none
[speaker]
pcspeaker=false
tandy=off
disney=false
[midi]
mpu401=none
mididevice=none
[autoexec]
@echo off
mount c .
mount d WORK
mount e BACKUP
c:
NW /vga D:\ E:\
echo NIGHTWATCH-BROWSER-SESSION-END
'''


def fetch(cache, name, url, digest):
    path = cache / name
    if not path.exists():
        print(f'Downloading {name}', flush=True)
        request = urllib.request.Request(url, headers={'User-Agent': 'nightwatch-site'})
        with urllib.request.urlopen(request, timeout=120) as response:
            data = response.read()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError(f'Checksum mismatch for {name}')
        path.write_bytes(data)
    if hashlib.sha256(path.read_bytes()).hexdigest() != digest:
        raise ValueError(f'Checksum mismatch for cached {name}; remove it and rebuild')
    return path


def gallery_entries():
    names = re.findall(r'\[([^\]]+\.png)\]\(\1\)', (ROOT/'screenshots/README.md').read_text())
    entries = []
    for name in names:
        if name == 'overview.png':
            continue
        if Path(name).name != name or not (ROOT/'screenshots'/name).is_file():
            raise ValueError(f'Invalid gallery screenshot: {name}')
        number = int(name[:2]) if name[:2].isdigit() else 0
        category = ('transfers' if 38 <= number <= 51 or name.startswith(('copy-', 'arj-')) else
                    'viewers' if 29 <= number <= 37 else
                    'navigation' if 1 <= number <= 19 else 'tools')
        title = re.sub(r'^\d+-', '', Path(name).stem).replace('-', ' ').capitalize()
        for word in ('zip', 'arj'):
            title = re.sub(r'\b'+word+r'\b', word.upper(), title, flags=re.I)
        entries.append({'file': name, 'title': title, 'category': category})
    if not entries or len({entry['file'] for entry in entries}) != len(entries):
        raise ValueError('Screenshot manifest is empty or contains duplicates')
    return entries


def zip_write(archive, name, data):
    entry = ZipInfo(name, (1993, 7, 16, 12, 30, 0))
    entry.compress_type = ZIP_DEFLATED
    entry.external_attr = ((0o40755 << 16) | 0x10) if name.endswith('/') else 0o100644 << 16
    archive.writestr(entry, data)


def build(cache, release):
    if not release.is_file():
        raise ValueError('DOS archive missing. Run make package first.')
    cache.mkdir(parents=True, exist_ok=True)
    downloads = [fetch(cache, *entry) for entry in DOWNLOADS]
    # Disposable output and fresh demo drives, never the user's edited build/demo.
    with tempfile.TemporaryDirectory(prefix='site-staging-', dir=ROOT/'build') as temporary:
        workspace = Path(temporary)
        staging = workspace/'site'
        shutil.copytree(ROOT/'site', staging)
        emulator = staging/'emulator'
        emulator.mkdir()
        with tarfile.open(downloads[0], 'r:gz') as archive:
            for name in ('emulators.js', 'wdosbox.js', 'wdosbox.wasm', 'wlibzip.js', 'wlibzip.wasm'):
                (emulator/name).write_bytes(archive.extractfile('package/dist/'+name).read())
            (emulator/'LICENSE.txt').write_bytes(archive.extractfile('package/LICENSE').read())
        for path in downloads[1:]:
            shutil.copyfile(path, emulator/path.name)
        (emulator/'NOTICE.txt').write_text(
            'js-dos emulators 8.5.2 / DOSBox, GNU GPL version 2. See LICENSE.txt.\n'
            f'Emulators source ({COMMIT}): emulators-source.tgz\n'
            f'DOSBox source ({DOSBOX_COMMIT}): dosbox-source.tgz\n'
            'The corresponding source archives are served alongside this notice.\n'
            'Build instructions: emulators-source.tgz README.md and CMakeLists.txt.\n'
            f'Upstream tree: https://github.com/js-dos/emulators/tree/{COMMIT}\n'
            'GL4ES dependency (MIT):\n'
            'https://github.com/ptitSeb/gl4es/tree/a744af14d4afbda77bf472bc53f43b9ceba39cc0\n'
            'Nightwatch application and website source: https://github.com/ifilot/nightwatch\n', encoding='utf-8')
        demo = workspace/'demo'
        populate(demo)
        with ZipFile(release) as dos, ZipFile(staging/'NIGHTWATCH.jsdos', 'w') as bundle:
            executable = dos.read('NW.EXE')
            if executable[:2] != b'MZ':
                raise ValueError('DOS release has no valid NW.EXE')
            version = dos.read('VERSION.TXT').decode('ascii').strip()
            zip_write(bundle, 'NW.EXE', executable)
            for name in ('LICENSE.TXT', 'FONTLIC.TXT', 'ICONLIC.TXT', 'ZLIBLIC.TXT', 'README.TXT'):
                data = dos.read(name)
                (staging/name).write_bytes(data)
                zip_write(bundle, name, data)
            for source_drive, directory in [('D', 'WORK'), ('E', 'BACKUP')]:
                zip_write(bundle, directory+'/', b'')
                # Include empty folders as well as files.
                for path in sorted((demo/source_drive).rglob('*')):
                    name = directory+'/'+path.relative_to(demo/source_drive).as_posix()
                    zip_write(bundle, name+'/' if path.is_dir() else name,
                              b'' if path.is_dir() else path.read_bytes())
            zip_write(bundle, '.jsdos/dosbox.conf', DOSBOX_CONF.encode('ascii'))
            zip_write(bundle, '.jsdos/jsdos.json', json.dumps({'version':'8'}).encode('ascii'))
        shutil.copyfile(release, staging/'NIGHTWATCH-DOS.zip')
        entries = gallery_entries()
        (staging/'screenshots').mkdir()
        figures = []
        for index, entry in enumerate(entries, 1):
            name, title, category = entry['file'], html.escape(entry['title'], quote=True), entry['category']
            shutil.copyfile(ROOT/'screenshots'/name, staging/'screenshots'/name)
            figures.append(f'<figure class="capture" data-category="{category}" data-title="{title}">'
                           f'<a href="screenshots/{name}" aria-label="Enlarge: {title}">'
                           f'<img src="screenshots/{name}" width="640" height="480" loading="lazy" alt="Nightwatch VGA: {title.lower()}."></a>'
                           f'<figcaption><span class="capture-number">{index:02}</span><span>{title}</span></figcaption></figure>')
        template = (staging/'index.html').read_text()
        (staging/'index.html').write_text(template.replace('{{VERSION}}', html.escape(version))
                                        .replace('{{SCREENSHOT_COUNT}}', str(len(entries)))
                                        .replace('{{GALLERY}}', '\n'.join(figures)))
        commit = subprocess.check_output(['git','rev-parse','HEAD'], cwd=ROOT, text=True).strip()
        (staging/'build.json').write_text(json.dumps({
            'version':version, 'commit':commit, 'emulator':'8.5.2',
            'executable_sha256':hashlib.sha256(executable).hexdigest(), 'screenshots':len(entries),
        })+'\n')
        (staging/'.nojekyll').touch()
        output = ROOT/'build/site'
        if output.exists():
            shutil.rmtree(output)
        staging.rename(output)
    print(f'Built {output} ({sum(p.stat().st_size for p in output.rglob("*") if p.is_file()):,} bytes)')
    print('Preview: python3 -m http.server 8000 --directory build/site')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cache', type=Path, default=ROOT/'build/site-cache')
    parser.add_argument('--release', type=Path, default=ROOT/'build/dist/NIGHTWATCH-DOS.zip')
    args = parser.parse_args()
    build(args.cache, args.release)
