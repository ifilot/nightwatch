# SPDX-License-Identifier: GPL-3.0-only
"""Reuse checked DOS objects only for identical sources, flags and toolchains."""
from pathlib import Path
import hashlib
import json
import os
import shutil
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def compiler_files():
    base = Path(os.environ.get('DOS_TOOLCHAIN', str(ROOT/'buildenv')))
    files = [base/'TC/TCC.EXE', base/'TC/TLINK.EXE', base/'TASM/TASM.EXE']
    files += sorted(p for name in ('TC', 'TASM') for p in (base/name).iterdir()
                    if p.is_file() and p.suffix.lower() == '.cfg')
    files += sorted(p for name in ('TC/INCLUDE', 'TC/LIB')
                    for p in (base/name).rglob('*') if p.is_file())
    return [('toolchain/'+p.relative_to(base).as_posix(), p) for p in files]


def signature(files, flags, compiler=None):
    digest = hashlib.sha256(json.dumps(flags, sort_keys=True).encode())
    entries = [(p.relative_to(ROOT).as_posix(), p) for p in files]
    entries += compiler_files() if compiler is None else compiler
    for name, path in sorted(entries):
        digest.update(name.encode()+b'\0')
        digest.update(hashlib.sha256(path.read_bytes()).digest())
    return digest.hexdigest()


class ObjectCache:
    def __init__(self, directory, key):
        self.directory = Path(directory)/key
        self.key = key

    def restore(self, work, names):
        try:
            manifest = json.loads((self.directory/'manifest.json').read_text())
            if manifest['key'] != self.key or set(manifest['files']) != set(names):
                return False
            data = {name: (self.directory/name).read_bytes() for name in names}
            if any(not value or hashlib.sha256(value).hexdigest() != manifest['files'][name]
                   for name, value in data.items()):
                return False
        except (OSError, ValueError, KeyError, TypeError):
            return False
        for name, value in data.items():
            (Path(work)/name).write_bytes(value)
        return True

    def save(self, work, names):
        self.directory.parent.mkdir(parents=True, exist_ok=True)
        temp = Path(tempfile.mkdtemp(prefix='.objects-', dir=self.directory.parent))
        try:
            files = {}
            for name in names:
                value = (Path(work)/name).read_bytes()
                if not value:
                    raise ValueError('Cannot cache an empty DOS object: '+name)
                (temp/name).write_bytes(value)
                files[name] = hashlib.sha256(value).hexdigest()
            (temp/'manifest.json').write_text(json.dumps({'key': self.key, 'files': files}))
            # Publish only after a successful compiler gate and complete manifest.
            shutil.rmtree(self.directory, ignore_errors=True)
            temp.rename(self.directory)
        finally:
            if temp.exists(): shutil.rmtree(temp)
