# SPDX-License-Identifier: GPL-3.0-only
"""Compare copy buffers on a fixed DOSBox-X 8086 core; not a disk benchmark."""
from pathlib import Path
import os
import re
import subprocess as sp
from dosbuild import compile_dos

root = Path(__file__).resolve().parents[1]
base = root / 'build/copy-speed'
base.mkdir(parents=True, exist_ok=True)
toolchain = os.environ.get('DOS_TOOLCHAIN', str(root / 'buildenv'))
env = dict(os.environ, SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
(base/'FAST.CONF').write_text('[cpu]\ncycles=100000\n')
(base/'CPU.CONF').write_text('[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n')
fixture = bytes(range(256)) * 4096
program = r'''
#include <stdio.h>
#include <stdlib.h>
#include <dos.h>
#include "NW.H"
unsigned _stklen = 8192;
static unsigned long blocks, first_block;
static int progress(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)total;
    if (!blocks) first_block = done;
    ++blocks; return 1;
}
static int cancel(const char *path, unsigned long done, unsigned long total)
{
    (void)path; (void)total;
    first_block = done; return 0;
}
int main(void)
{
    unsigned long start, elapsed;
    Entry e;
    int i;
    operation_progress = progress;
    start = biostime(0, 0L);
    for (i = 0; i < 3; ++i) {
        if (!file_copy("D:\\SOURCE.BIN", "D:\\DEST.BIN")) {
            puts(nw_error); return 1;
        }
        if (i < 2 && !fs_delete("D:\\DEST.BIN", 0)) return 1;
    }
    elapsed = biostime(0, 0L) - start;
    printf("copy: bytes=3145728 ticks=%lu blocks=%lu chunk=%lu\n",
           elapsed, blocks, first_block);
    operation_progress = cancel; first_block = 0;
    if (file_copy("D:\\SOURCE.BIN", "D:\\CANCEL.BIN") ||
        fs_info("D:\\CANCEL.BIN", &e)) return 1;
    printf("cancel: bytes=%lu\n", first_block);
    return 0;
}
'''


def execute(work, commands, cpu=False):
    args = ['dosbox-x' if cpu else 'dosbox', '-conf', str(root/'tools/dosbox.conf'),
            '-conf', str(base/('CPU.CONF' if cpu else 'FAST.CONF'))]
    if cpu:
        args += ['-fastlaunch', '-nogui']
    for command in [f'mount c "{toolchain}"', f'mount d "{work}"', 'd:', *commands, 'exit']:
        args += ['-c', command]
    with (work/'DOSBOX.LOG').open('w') as log:
        sp.run(args, env=env, stdout=log, stderr=log, check=True, timeout=120)


for size in (2048, 8192, 16384):
    work = base/str(size)
    work.mkdir(exist_ok=True)
    for name in ('CORE.C', 'FSDOS.C', 'NW.H', 'VERSION.H'):
        source = (root/'src'/name).read_text()
        if name == 'FSDOS.C':
            # Alter only the staged adapter. The 2 KiB case exercises fallback;
            # the other cases retain identical far I/O and allocation overhead.
            if size == 2048:
                source = source.replace('farmalloc(16384UL)', 'NULL')
            else:
                source = source.replace('16384', str(size))
        (work/name).write_bytes(source.replace('\n', '\r\n').encode('ascii'))
    (work/'CPBENCH.C').write_bytes(program.replace('\n', '\r\n').encode('ascii'))
    compile_dos(work, lambda directory, commands: execute(directory, commands), 'CPBUILD',
                [r'set PATH=C:\TC', r'set INCLUDE=C:\TC\INCLUDE', r'set LIB=C:\TC\LIB',
                 'tcc -1- -ms -O -Z -eCPBENCH.EXE CPBENCH.C CORE.C FSDOS.C > COMPILE.LOG'],
                ['CPBENCH.EXE'], ['COMPILE.LOG'])
    (work/'SOURCE.BIN').write_bytes(fixture)
    for name in ('DEST.BIN', 'CANCEL.BIN', 'RESULT.LOG'):
        (work/name).unlink(missing_ok=True)
    execute(work, ['CPBENCH > RESULT.LOG'], cpu=True)
    result = (work/'RESULT.LOG').read_text()
    timing = re.search(r'copy: bytes=3145728 ticks=(\d+) blocks=(\d+) chunk=(\d+)', result)
    cancelled = re.search(r'cancel: bytes=(\d+)', result)
    assert timing and cancelled, result
    ticks, blocks, chunk = map(int, timing.groups())
    assert ticks > 0 and chunk == size and blocks == 3 * len(fixture)//size, result
    assert int(cancelled[1]) == size and not (work/'CANCEL.BIN').exists(), result
    assert (work/'DEST.BIN').read_bytes() == fixture
    assert not list(work.glob('*.TMP'))
    rate = 3072 * 18.2 / ticks
    print(f'{size//1024:2} KiB buffer: {ticks} ticks, {rate:.1f} KiB/s, '
          f'{blocks} blocks; cancellation after {size} bytes', flush=True)
print('PASS: byte-exact copies, expected block counts and cancellation cleanup; '
      'timings measure emulator/CPU overhead on a cached host drive', flush=True)
