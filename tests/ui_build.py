# SPDX-License-Identifier: GPL-3.0-only
"""Prepare one checked scripted navigator for all DOS UI suites."""
from pathlib import Path
import os
import subprocess as sp
import hashlib
import json
import argparse
from doszip import compile_app
from object_cache import signature

root = Path(__file__).resolve().parents[1]
base = root / 'build/video'
toolchain = Path(os.environ.get('DOS_TOOLCHAIN', str(root / 'buildenv')))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')

def dos(directory, commands, machine='svga_s3'):
    args = ['dosbox', '-conf', str(root / 'tools/dosbox.conf'), '-conf', str(base / 'FAST.CONF'), '-machine', machine]
    for cmd in [f'mount c "{toolchain}"', f'mount d "{directory}"', 'd:'] + commands + ['exit']:
        args += ['-c', cmd]
    with (directory / 'DOSBOX.LOG').open('w') as f:
        sp.run(args, env=env, stdout=f, stderr=f, check=True, timeout=45)



def source_key():
    inputs = [p for folder in (root/'src', root/'vendor/zlib') for p in folder.iterdir()
              if p.suffix.upper() in ('.C','.H','.ASM')]
    inputs += [root/'tests'/name for name in ('UITEST.H','ui_build.py','doszip.py','dosbuild.py','object_cache.py')]
    return signature(inputs, 'scripted navigator / medium model / 8086 / diagnostics')


def build_ui():
    base.mkdir(parents=True, exist_ok=True)
    (base / 'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
    for src in (root / 'src').iterdir():
        if src.suffix in {'.C', '.H', '.ASM'}:
            text = src.read_text()
            if src.name == 'MAIN.C':
                begin = text.index('static int key_read(void)')
                end = text.index('static int pane_rows', begin)
                text = text[:begin] + ('#include "UITEST.H"\n'
                                      'static int key_read(void) { return test_key_read(); }\n'
                                      'static int modal_key_read(void) { return test_key_read(); }\n') + text[end:]
                # Capture copy dialogs separately so ordinary scripted-key frame
                # numbers remain stable. Production source has no capture calls.
                text = text.replace('        paint_tick = now;',
                                    '        test_progress_capture(done, total); paint_tick = now;')
            if src.name == 'CORE.C':
                # Test-only pacing makes BIOS-timed copy snapshots deterministic,
                # even when the mounted host filesystem transfers a MB instantly.
                text = '#include <dos.h>\n' + text
                text = text.replace('        operation_current_bytes = copied;',
                                    '        delay(5); operation_current_bytes = copied;')
            (base / src.name).write_bytes(text.replace('\n', '\r\n').encode('ascii'))
    (base / 'UITEST.H').write_bytes((root / 'tests/UITEST.H').read_text().replace('\n', '\r\n').encode('ascii'))
    compile_app(base, dos, 'UIBUILD',
        [r'set PATH=C:\TC;C:\TASM', 'tasm /mx FONT.ASM > ASSEMBLE.LOG',
         r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
         r'tcc -DNW_DIAGNOSTICS -1- -mm -O -Z -eUITEST.EXE MAIN.C CORE.C FSDOS.C VIDEO.C GRAPH.C HEX.C VIEW.C FONT.OBJ > COMPILE.LOG'],
        ['UITEST.EXE','FONT.OBJ'], ['ASSEMBLE.LOG','COMPILE.LOG'])
    record = {'source': source_key(), 'exe': hashlib.sha256((base/'UITEST.EXE').read_bytes()).hexdigest()}
    (base/'UIBUILD.json').write_text(json.dumps(record))


def require_prebuilt():
    record = json.loads((base/'UIBUILD.json').read_text())
    assert record['source'] == source_key(), 'Scripted navigator inputs changed; rebuild UI tests'
    assert record['exe'] == hashlib.sha256((base/'UITEST.EXE').read_bytes()).hexdigest(), 'Scripted navigator is damaged'
    print('PASS: using checked prebuilt scripted navigator', flush=True)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    require_prebuilt() if args.verify else build_ui()
