# SPDX-License-Identifier: GPL-3.0-only
"""DOS compiler gates: fresh outputs and an errorlevel-checked completion marker."""
from pathlib import Path
import re
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1]/'tools'))
from dos_link import link_response

# DOSBox may exit successfully after a failed compiler command. Delete prior
# outputs/markers, check errorlevel after each command, then verify fresh files.
# commands must fit the DOS 126-character command tail before CR/NUL.
def compile_dos(directory, run, name, commands, outputs, logs):
    directory = Path(directory)
    name = name.upper()
    assert len(name) <= 8
    for filename in [*outputs, name+'.OK', name+'.FAI']:
        (directory/filename).unlink(missing_ok=True)
    lines = ['@echo off']
    objects = {path.name.upper() for path in directory.iterdir()
               if path.is_file() and path.suffix.upper() == '.OBJ'}
    for command in commands:
        # Track planned outputs too: compilation has not run when the host
        # writes this batch. Never let TCC expand a long wildcard object list.
        if command.lower().startswith(('tcc ', 'tasm ')):
            for source in re.findall(r'\b([A-Za-z0-9_]{1,8})\.(?:C|ASM)\b', command, re.I):
                objects.add(source.upper()+'.OBJ')
        if command.lower().startswith('tcc ') and '*.OBJ' in command.upper():
            executable = re.search(r'-e([A-Za-z0-9_]{1,8}\.EXE)\b', command, re.I)
            model = re.search(r'-m([smclh])\b', command, re.I)
            assert executable, ('Missing link output name', command)
            rsp = str(Path(executable[1]).with_suffix('.RSP'))
            (directory/rsp).write_bytes(link_response(sorted(objects), executable[1],
                                       model[1].lower() if model else 's').encode('ascii'))
            redirect = command[command.index('>'):] if '>' in command else ''
            command = f'tlink /m /c @{rsp} {redirect}'.rstrip()
        assert len(command) <= 126, ('DOS command tail too long', command)
        lines += [command, 'if errorlevel 1 goto failed']
    lines += [f'echo OK>{name}.OK', 'goto done', ':failed', f'echo FAILED>{name}.FAI', ':done']
    (directory/(name+'.BAT')).write_bytes(('\r\n'.join(lines)+'\r\n').encode('ascii'))
    run(directory, ['call '+name+'.BAT'])
    transcript = '\n'.join((directory/filename).read_text(errors='replace')
                           for filename in logs if (directory/filename).exists())
    assert (directory/(name+'.OK')).exists() and not (directory/(name+'.FAI')).exists(), transcript
    assert all((directory/filename).is_file() and (directory/filename).stat().st_size
               for filename in outputs), transcript
    return transcript
