# SPDX-License-Identifier: GPL-3.0-only
"""Prepare persistent local demo media and its DOS-only identity launcher."""
from pathlib import Path
import hashlib
import os
import shutil
import subprocess as sp

from demo_drive import ROOT, populate, text


def prepare_media(destination):
    populate(destination)
    for drive, description in [('A', '3.5-inch floppy, 1.44 MB'),
                               ('B', '5.25-inch floppy, 360 KB'),
                               ('S', 'SD card (simulated Slot-otter)')]:
        directory = destination / drive
        directory.mkdir(parents=True, exist_ok=True)
        samples = {'README.TXT': text(description + '\n\n'
                   'Use F9 to switch drives. Files here persist between launches.\n')}
        if drive == 'A':
            samples['LETTER.TXT'] = text('July field office correspondence.\n')
        elif drive == 'B':
            samples['CATALOG.TXT'] = text('Legacy floppy archive - equipment records, 1988.\n')
        else:
            samples['CARD/NOTES.TXT'] = text('Field recordings transferred on an SD card.\n')
            samples['BACKUP/PLAN.TXT'] = text('Copy reports from D: into this folder.\n')
        for name, content in samples.items():
            path = directory / name
            path.parent.mkdir(parents=True, exist_ok=True)
            if not path.exists():
                path.write_bytes(content)
    for drive, size in [('A', '1440'), ('B', '360')]:
        image = destination / (drive + '.IMG')
        if image.exists():
            continue  # The image, rather than the seed directory, owns later edits.
        temporary = destination / (drive + '.TMP')
        try:
            # Classic DOSBox rewrites FAT[0] as F0 even for 360 KB disks.
            # Start both BPB and FAT with F0 to keep the image consistent after
            # writes; geometry and capacity still describe a 360 KB floppy.
            sp.run(['mformat', '-C', '-i', str(temporary), '-f', size, '-m', '240',
                    '-v', 'NW_DEMO_' + drive, '::'], check=True)
            sp.run(['mcopy', '-s', '-i', str(temporary),
                    *map(str, sorted((destination / drive).iterdir())), '::'], check=True)
            temporary.replace(image)
        finally:
            temporary.unlink(missing_ok=True)


def build_launcher(directory=None):
    directory = Path(directory) if directory is not None else ROOT / 'build/demo'
    directory.mkdir(parents=True, exist_ok=True)
    toolchain = Path(os.environ.get('DOS_TOOLCHAIN', str(ROOT / 'buildenv'))).resolve()
    sources = [ROOT / 'tools' / name for name in ('DEMODRV.C', 'DRVMETA.ASM')]
    digest = hashlib.sha256(b''.join(p.read_bytes() for p in sources) +
                            (toolchain / 'TC/TCC.EXE').read_bytes() +
                            (toolchain / 'TASM/TASM.EXE').read_bytes()).hexdigest()
    stamp = directory / 'launcher.sha256'
    executable = directory / 'DEMODRV.EXE'
    if executable.is_file() and executable.stat().st_size and stamp.exists() and stamp.read_text() == digest:
        return
    # A failed rebuild must invalidate both the output and its old cache stamp.
    for name in ('DEMODRV.EXE', 'DRVMETA.OBJ', 'DEMO.OK', 'launcher.sha256',
                 'META.LOG', 'DEMO.LOG', 'DOSBOX.LOG'):
        (directory / name).unlink(missing_ok=True)
    for source in sources:
        (directory / source.name).write_bytes(source.read_text().replace('\n', '\r\n').encode('ascii'))
    commands = [r'set PATH=C:\TC;C:\TASM', r'set INCLUDE=C:\TC\INCLUDE',
                r'set LIB=C:\TC\LIB', 'tasm /mx DRVMETA.ASM > META.LOG',
                'tcc -1- -mm -O -Z -eDEMODRV.EXE DEMODRV.C DRVMETA.OBJ > DEMO.LOG']
    lines = ['@echo off']
    for command in commands:
        lines += [command, 'if errorlevel 1 goto done']
    lines += ['echo OK>DEMO.OK', ':done']
    (directory / 'DEMO.BAT').write_bytes(('\r\n'.join(lines) + '\r\n').encode('ascii'))
    (directory / 'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
    args = ['dosbox', '-conf', str(ROOT / 'tools/dosbox.conf'),
            '-conf', str(directory / 'FAST.CONF')]
    for command in [f'mount c "{toolchain}" -ro', f'mount d "{directory}"',
                    'd:', 'call DEMO.BAT', 'exit']:
        args += ['-c', command]
    with (directory / 'DOSBOX.LOG').open('w') as log:
        sp.run(args, env=dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy'),
               stdout=log, stderr=log, check=True, timeout=90)
    if not (directory / 'DEMO.OK').exists() or not executable.is_file() or not executable.stat().st_size:
        transcript = '\n'.join((directory / name).read_text(errors='replace')
                               for name in ('META.LOG', 'DEMO.LOG')
                               if (directory / name).exists())
        raise RuntimeError(f'Demo launcher compilation failed in {directory}\n{transcript}')
    stamp.write_text(digest)
    print('Built demo drive identity launcher')


if __name__ == '__main__':
    for command in ('mformat', 'mcopy', 'dosbox'):
        if not shutil.which(command):
            raise SystemExit(f'{command} is required for the local demo (install mtools and DOSBox).')
    prepare_media(ROOT / 'build/demo')
    build_launcher()
