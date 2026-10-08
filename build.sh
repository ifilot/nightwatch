#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
# Mount the repository's compiler and assembler read-only.
toolchain=${DOS_TOOLCHAIN:-$repo/buildenv}
if [[ ! -f "$toolchain/TC/TCC.EXE" || ! -f "$toolchain/TASM/TASM.EXE" ]]; then
    echo "Set DOS_TOOLCHAIN to a directory containing TC/TCC.EXE, TC/INCLUDE, TC/LIB and TASM/TASM.EXE." >&2
    exit 1
fi
mkdir -p "$repo/build"
python3 "$repo/tools/version.py" --write
python3 "$repo/tools/build_metadata.py"
cp "$repo/assets/FONT-LICENSE.txt" "$repo/build/FONTLIC.TXT"
cp "$repo/assets/ICON-LICENSE.txt" "$repo/build/ICONLIC.TXT"
cp "$repo/LICENSE" "$repo/build/LICENSE.TXT"
python3 "$repo/tools/stage.py" "$repo/src" "$repo/build"
# DOSBox process success alone does not prove compiler success. The batch file
# checks DOS errorlevel and emits a fresh marker only after NIGHT.EXE exists.
cat > "$repo/build/BUILD.BAT" <<'BAT'
@echo off
set PATH=C:\TC;C:\TASM
D:
make -fMAKEFILE > COMPILE.LOG
if errorlevel 1 goto failed
if not exist NIGHT.EXE goto failed
echo OK>BUILD.OK
goto done
:failed
echo FAILED>BUILD.ERR
:done
exit
BAT
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy dosbox -noconsole -conf "$repo/tools/dosbox.conf" \
    -c "mount c \"$toolchain\" -t dir -ro" \
    -c "mount d \"$repo/build\"" -c "d:" -c "call BUILD.BAT" > "$repo/build/dosbox.log" 2>&1
cat "$repo/build/COMPILE.LOG"
[[ -s "$repo/build/NIGHT.EXE" && -f "$repo/build/BUILD.OK" ]]
# Linking can succeed even when Turbo C startup leaves too little near heap.
# Check the map against the actual runtime stack before accepting the build.
python3 "$repo/tools/check_memory.py" "$repo/build/NIGHT.MAP"
printf 'Built %s (%s bytes)\n' "$repo/build/NIGHT.EXE" "$(stat -c %s "$repo/build/NIGHT.EXE")"
