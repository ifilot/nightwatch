#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
repo=$PWD
dosbox_test=${DOSBOX_TEST:-dosbox-x}
toolchain=${DOS_TOOLCHAIN:-$repo/toolchain}
mkdir -p build/dostest
printf '[cpu]\ncputype=8086\ncore=normal\ncycles=3000\n' > build/dostest/CPU.CONF
scratch=$(mktemp -d /tmp/nightwatch-dos-XXXXXX)
trap 'rm -rf "$scratch"' EXIT
# Use actual FAT12 volumes: local-drive caching can hide DOS mutation behavior.
# The second volume forces cross-drive moves; compiler/source stay elsewhere.
mformat -f 1440 -C -i "$scratch/test.img" ::
mformat -f 1440 -C -i "$scratch/other.img" ::
python3 - <<'PY'
from pathlib import Path
import shutil
shutil.rmtree("build/dostest/XDRIVE", ignore_errors=True)
for src in [Path('src/CORE.C'), Path('src/FSDOS.C'), Path('src/NW.H'), Path('src/VERSION.H'), Path('src/VIEW.C'), Path('src/VIEW.H'), Path('tests/DOSTEST.C')]:
    Path('build/dostest', src.name).write_bytes(src.read_text().replace('\n', '\r\n').encode('ascii'))
PY
rm -f build/dostest/DOSTEST.EXE build/dostest/RESULT.LOG
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$dosbox_test" -fastlaunch -nogui -conf tools/dosbox.conf -conf build/dostest/CPU.CONF \
    -c "mount c \"$toolchain\"" -c "mount d \"$repo/build/dostest\"" \
    -c "imgmount e \"$scratch/test.img\" -t floppy" -c "imgmount f \"$scratch/other.img\" -t floppy" -c "set PATH=C:\TC" -c "d:" \
    -c 'tcc -1- -ms -IC:\TC\INCLUDE -LC:\TC\LIB -eDOSTEST.EXE DOSTEST.C CORE.C FSDOS.C VIEW.C > COMPILE.LOG' \
    -c 'e:' -c 'D:\DOSTEST > RESULT.LOG' -c exit > build/dostest/dosbox.log 2>&1
mcopy -o -i "$scratch/test.img" ::RESULT.LOG build/dostest/RESULT.LOG
cat build/dostest/COMPILE.LOG build/dostest/RESULT.LOG
rg -q '^PASS:' build/dostest/RESULT.LOG
