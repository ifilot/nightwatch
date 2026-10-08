# SPDX-License-Identifier: GPL-3.0-only
"""Compile the same vendored inflate subset and medium-model ABI as production."""
from pathlib import Path
import re
from dosbuild import compile_dos
ROOT = Path(__file__).resolve().parents[1]
MODULES = ('INFLATE', 'INFFAST', 'INFTREES', 'ZUTIL', 'ADLER32', 'CRC32', 'ZIP')

def prepare_zip(work, run, flags=""):
    work = Path(work)
    for old in work.glob('*.OBJ'):
        old.unlink()
    for src in [*(ROOT/'vendor/zlib').iterdir(), ROOT/'src/ZIP.C', ROOT/'src/ZIP.H', ROOT/'src/FIXED.ASM', ROOT/'src/NW.H', ROOT/'src/VERSION.H']:
        if src.suffix.lower() in ('.c', '.h', '.asm'):
            (work/src.name.upper()).write_bytes(src.read_text(encoding='ascii').replace('\n', '\r\n').encode('ascii'))
    commands = [r'set PATH=C:\TC;C:\TASM', r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB']
    commands += ['tasm /mx FIXED.ASM > ZFIXED.LOG']
    commands += [f'tcc -1- -mm -O -Z -DZ_SOLO -DNO_GZIP {flags} -c {m}.C > Z{m}.LOG' for m in MODULES]
    compile_dos(work, run, 'ZBUILD', commands, ['FIXED.OBJ']+[m+'.OBJ' for m in MODULES], ['ZFIXED.LOG']+['Z'+m+'.LOG' for m in MODULES])

def compile_app(work, run, name, commands, outputs, logs):
    work = Path(work)
    if '"ZIP.H"' in (work/'MAIN.C').read_text():
        prepare_zip(work, run)
    else:
        # Historical renderer baselines have no archive dependency.
        for old in work.glob('*.OBJ'): old.unlink()
    font = work/'FONT.ASM'
    if not font.exists():
        font.write_bytes((ROOT/'src/FONT.ASM').read_bytes())
    text = font.read_text().replace('_asset_font proc near', '_asset_font proc far').replace('_asset_help proc near', '_asset_help proc far').replace('[bp+4]', '[bp+6]')
    font.write_bytes(text.replace('\n','\r\n').encode('ascii'))
    changed = []
    logs = list(logs)
    if not any('tasm' in c and 'FONT.ASM' in c for c in commands):
        changed += [r'set PATH=C:\TC;C:\TASM', 'tasm /mx FONT.ASM > ZFONT.LOG']
        logs.append('ZFONT.LOG')
    for command in commands:
        exe = re.search(r'-e(\w+\.EXE)', command)
        if exe and command.startswith('tcc '):
            # Stage commands compile each caller's modified entry point, then
            # link all freshly compiled objects, including the ZIP library.
            if '.C' in command:
                compile_command = command.replace(exe[0], '-c')
                compile_command = re.sub(r'\b\w+\.OBJ\s*', '', compile_command)
                changed.append(compile_command)
            log = name[:6]+'LK.LOG'
            changed.append(f'tcc -1- -mm -e{exe[1]} *.OBJ > {log}')
            logs.append(log)
        else:
            changed.append(command)
    return compile_dos(work, run, name, changed, outputs, logs)
