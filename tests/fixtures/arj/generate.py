# SPDX-License-Identifier: GPL-3.0-only
"""Retain archives from an independent ARJ producer, not the tested decoder."""
from pathlib import Path
import argparse, hashlib, json, os, random, subprocess, tempfile

ROOT = Path(__file__).resolve().parent

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--arj', default='arj')
    args = parser.parse_args()
    producer = str(Path(args.arj).resolve()) if '/' in args.arj else args.arj
    data = {
        'HELLO.TXT': b'Nightwatch ARJ extraction\r\n' * 4200,
        'SUB/DATA.BIN': random.Random(130).randbytes(28000) * 3,
        'SUB/EMPTY.DAT': b'',
        'SUB/SMALL.TXT': b'a\r\nb\r\n',
    }
    with tempfile.TemporaryDirectory() as directory:
        work = Path(directory)
        (work/'SUB/EMPTY').mkdir(parents=True)
        for name, content in data.items():
            p = work/name
            p.write_bytes(content)
            os.utime(p,(946684800,946684800))
        for p in (work/'SUB/EMPTY',work/'SUB'):
            os.utime(p,(946684800,946684800))
        for method in range(5):
            archive = ROOT/f'METHOD{method}.ARJ'
            archive.unlink(missing_ok=True)
            result = subprocess.run([producer,'a',f'-m{method}','-r','-a1','-t0',str(archive),'HELLO.TXT','SUB'],
                                    cwd=work,text=True,capture_output=True,check=True)
            (ROOT/f'METHOD{method}.log').write_text(result.stdout)
        # Near-window matches exercise long distances and ring wraparound.
        (work/'HELLO.TXT').write_bytes(random.Random(26624).randbytes(26000)*3)
        archive = ROOT/'WINDOW.ARJ'
        archive.unlink(missing_ok=True)
        result = subprocess.run([producer,'a','-m1','-t0',str(archive),'HELLO.TXT'],
                                cwd=work,text=True,capture_output=True,check=True)
        (ROOT/'WINDOW.log').write_text(result.stdout)
    manifest = {name: {'size':len(content),'sha256':hashlib.sha256(content).hexdigest()}
                for name, content in data.items()}
    (ROOT/'manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    hashes = {p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(ROOT.glob('*.ARJ'))}
    (ROOT/'archive-sha256.json').write_text(json.dumps(hashes,indent=2)+'\n')

if __name__ == '__main__':
    main()
