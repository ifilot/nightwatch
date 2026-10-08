#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Create deterministic input data and genuine Linux Info-ZIP archives."""
from pathlib import Path
import subprocess, random, hashlib, json, shutil
ROOT = Path(__file__).resolve().parents[3]
HERE = Path(__file__).resolve().parent
DATA = ROOT/'build/zip-fixture-input'
if DATA.exists(): shutil.rmtree(DATA)
(DATA/'SUB/EMPTY').mkdir(parents=True)
r = random.Random(2026).randbytes(30000)
files = {'HELLO.TXT': b'Nightwatch cross-platform ZIP test.\r\n'*4000,
         'ZERO.BIN': b'', 'SUB/BYTES.BIN': bytes(range(256))*800,
         'SUB/WINDOW.BIN': r+r[:16000]+b'END', 'SUB/RANDOM.BIN': r}
for name, data in files.items():
    (DATA/name).write_bytes(data)
manifest = {n: {'size':len(d),'sha256':hashlib.sha256(d).hexdigest()} for n,d in files.items()}
(HERE/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
for name, level in [('LINUX0.ZIP','-0'),('LINUX9.ZIP','-9')]:
    archive = HERE/name
    archive.unlink(missing_ok=True)
    subprocess.run(['zip', level, '-r', str(archive), '.'],cwd=DATA,check=True,stdout=subprocess.DEVNULL)
print(subprocess.check_output(['zip','-v'],text=True).splitlines()[1])
print('Input ready for the Windows generator:', DATA)
