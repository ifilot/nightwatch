# SPDX-License-Identifier: GPL-3.0-only
"""DOS compiler gates: fresh outputs and an errorlevel-checked completion marker."""
from pathlib import Path

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
    for command in commands:
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
