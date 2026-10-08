# SPDX-License-Identifier: GPL-3.0-only
"""Unit-test the actual DOS listing adapter with deterministic search faults."""
from pathlib import Path
import os
import subprocess as sp
from dosbuild import compile_dos

root = Path(__file__).resolve().parents[1]
work = root/'build/fsdos-unit'
work.mkdir(parents=True, exist_ok=True)
toolchain = os.environ.get('DOS_TOOLCHAIN', str(root/'buildenv'))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
for name in ('CORE.C', 'FSDOS.C', 'NW.H', 'VERSION.H'):
    (work/name).write_bytes((root/'src'/name).read_text().replace('\n', '\r\n').encode('ascii'))
(work/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
program = r'''
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <dir.h>
#include "NW.H"
static int entries, index, fail_at, fail_first;
static int fake_first(const char *, struct ffblk *, int);
static int fake_next(struct ffblk *);
#define findfirst fake_first
#define findnext fake_next
#include "FSDOS.C"
#undef findfirst
#undef findnext
#undef assert
#define assert(x) do { if (!(x)) { printf("FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
unsigned _stklen = 8192;
static void member(struct ffblk *f)
{
    memset(f, 0, sizeof(*f)); sprintf(f->ff_name, "F%03d.TXT", index);
    f->ff_fsize = index;
}
static int fake_first(const char *path, struct ffblk *f, int attr)
{
    (void)attr; memset(f, 0, sizeof(*f));
    if (!strstr(path, "*.*")) { f->ff_attrib = NW_DIR; return 0; }
    if (fail_first) { errno = EIO; return -1; }
    index = 0;
    if (!entries) { errno = ENOENT; return -1; }
    member(f); return 0;
}
static int fake_next(struct ffblk *f)
{
    ++index;
    if (index == fail_at) { errno = EIO; return -1; }
    if (index >= entries) { errno = ENOENT; return -1; }
    member(f); return 0;
}
int main(void)
{
    Panel *p = &panes[0];
    unsigned revision;
    strcpy(p->path, "D:\\LIST"); entries = 3; fail_at = -1;
    assert(panel_load(p) && p->count == 4 && !p->truncated);
    panel_mark(p, 2, 1); p->cursor = 2; p->top = 1; revision = p->revision;
    /* Initial failure preserves the old cache, marks and selection. */
    fail_first = 1;
    assert(!panel_load(p) && p->count == 4 && p->marks == 1 && p->cursor == 2);
    assert(p->top == 1 && p->revision == revision); fail_first = 0;
    fail_at = 2;
    assert(!panel_load(p) && strstr(nw_error, "Read directory"));
    assert(!p->count && !p->marks && !p->cursor && !p->top && !p->truncated);
    assert(p->revision != revision);
    fail_at = -1; entries = 0;
    assert(panel_load(p) && p->count == 1 && !strcmp(p->files[0].name, ".."));
    entries = 3; assert(panel_load(p) && p->count == 4);
    entries = 600; assert(panel_load(p) && p->count == NW_FILES && p->truncated);
    puts("PASS: DOS listing distinguishes exhaustion, failure and capacity; failed refresh clears stale state");
    return 0;
}
'''
(work/'FSUNIT.C').write_bytes(program.replace('\n', '\r\n').encode('ascii'))


def execute(directory, commands, cpu=False):
    args = ['dosbox-x' if cpu else 'dosbox', '-conf', str(root/'tools/dosbox.conf')]
    if cpu:
        args += ['-conf', str(work/'CPU.CONF'), '-fastlaunch', '-nogui']
    for command in [f'mount c "{toolchain}"', f'mount d "{directory}"', 'd:', *commands, 'exit']:
        args += ['-c', command]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args, env=env, stdout=log, stderr=log, check=True, timeout=60)


compile_dos(work, execute, 'FSBUILD',
            [r'set PATH=C:\TC', r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
             'tcc -1- -ms -O -Z -eFSUNIT.EXE FSUNIT.C CORE.C > COMPILE.LOG'],
            ['FSUNIT.EXE'], ['COMPILE.LOG'])
(work/'RESULT.LOG').unlink(missing_ok=True)
execute(work, ['FSUNIT > RESULT.LOG'], cpu=True)
result = (work/'RESULT.LOG').read_text()
assert 'PASS: DOS listing' in result and 'FAIL' not in result, result
print(result.strip(), flush=True)
