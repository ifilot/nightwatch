#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
environment=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
vendor="$environment/vendor"
# The Docker image copies this script beside the archive instead.
if [[ ! -d "$vendor" ]]; then vendor="$environment"; fi
prefix=${1:?Usage: build-dosbox-x.sh INSTALL_PREFIX}
mkdir -p "$prefix"
prefix=$(cd -- "$prefix" && pwd)
(cd "$vendor" && sha256sum --check SHA256SUMS)
scratch=$(mktemp -d /tmp/nightwatch-dosbox-build-XXXXXX)
trap 'rm -rf "$scratch"' EXIT
tar -xzf "$vendor/dosbox-x-2024.03.01.tar.gz" -C "$scratch"
cd "$scratch/dosbox-x"
bash ./autogen.sh
./configure --prefix="$prefix" --enable-sdl2 --disable-debug \
    --disable-avcodec --disable-opengl --disable-libslirp --disable-libfluidsynth
# Desktop font resources are omitted from the headless source bundle.
make -j"${DOSBOX_BUILD_JOBS:-2}" res_DATA=
# CI uses surface output and built-in fonts, so no desktop resources are needed.
install -D -s -m 755 src/dosbox-x "$prefix/bin/dosbox-x"
install -D -m 644 COPYING "$prefix/share/licenses/dosbox-x/COPYING"
