#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
python3 tools/version.py --check
# Host builds enforce the DOS language subset and exercise memory/error paths
# under sanitizers. Hardware-specific VIDEO.C/MAIN.C compile in the DOS suite.
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -Isrc -fsyntax-only src/GRAPH.C
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -Isrc \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    src/GRAPH.C tests/test_progress.c -o build/test_progress
build/test_progress
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -Isrc \
    -ffunction-sections -fdata-sections -Wl,--gc-sections \
    src/GRAPH.C tests/test_help.c -o build/test_help
build/test_help
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -DNW_HOST -Isrc \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/CORE.C tests/fs_host.c tests/test_core.c -o build/test_core
ASAN_OPTIONS=detect_leaks=0 build/test_core
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -Isrc \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/HEX.C tests/test_hex.c -o build/test_hex
ASAN_OPTIONS=detect_leaks=0 build/test_hex
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -Isrc \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/VIEW.C tests/test_view.c -o build/test_view
ASAN_OPTIONS=detect_leaks=0 build/test_view
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -DNW_HOST -Isrc -Itests \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/CORE.C tests/fs_host.c tests/test_operations.c -o build/test_operations
ASAN_OPTIONS=detect_leaks=0 build/test_operations
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -DNW_HOST -Isrc -Itests \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/CORE.C tests/fs_host.c tests/test_copy_integrity.c -o build/test_copy_integrity
ASAN_OPTIONS=detect_leaks=0 build/test_copy_integrity
cc -x c -std=c89 -Wall -Wextra -Werror -pedantic -DNW_HOST -Isrc -Itests \
    -fsanitize=address,undefined -fno-omit-frame-pointer -g \
    src/CORE.C tests/fs_host.c tests/test_navigation.c -o build/test_navigation
ASAN_OPTIONS=detect_leaks=0 build/test_navigation
python3 tests/test_dosbuild.py
python3 tests/test_package.py
python3 tests/test_version.py
python3 tests/test_build_metadata.py
python3 tests/test_assets.py
python3 tests/zip.py
