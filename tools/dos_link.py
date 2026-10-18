# SPDX-License-Identifier: GPL-3.0-only
"""Pass complete object lists to Turbo Link without DOS command-tail expansion."""
from pathlib import Path
import argparse
import re

def link_response(objects, executable, model='m'):
    objects = list(objects)
    assert objects and len(objects) == len(set(name.upper() for name in objects))
    assert model in ('s', 'm', 'c', 'l', 'h')
    assert all(re.fullmatch(r'[A-Za-z0-9_]{1,8}\.OBJ', name, re.I) for name in objects)
    assert re.fullmatch(r'[A-Za-z0-9_]{1,8}\.EXE', executable, re.I)
    library = 'C:\\TC\\LIB\\'
    # Turbo C 2 does not accept @response files; invoke TLINK directly.
    # Include the same startup, emulator, math and runtime libraries as TCC.
    return (library+'C0'+model.upper()+'.OBJ+'+'+'.join(objects)+','+
            executable+','+str(Path(executable).with_suffix('.MAP'))+','+
            library+'EMU.LIB+'+library+'MATH'+model.upper()+'.LIB+'+
            library+'C'+model.upper()+'.LIB\r\n')

def make_objects(makefile):
    match = re.search(r'^OBJECTS=(.+)$', Path(makefile).read_text(), re.M)
    assert match, 'DOS Makefile must declare OBJECTS'
    return match[1].split()

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('makefile', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    content = link_response(make_objects(args.makefile), 'NW.EXE').encode('ascii')
    if not args.output.exists() or args.output.read_bytes() != content:
        args.output.write_bytes(content)
