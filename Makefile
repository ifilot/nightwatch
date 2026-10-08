# SPDX-License-Identifier: GPL-3.0-only
# Host-side entry points. DOS compilation uses src/MAKEFILE inside DOSBox.
# Keep bare make on the build even though generated-header rules appear first.
.DEFAULT_GOAL := all
MODE ?= text
DOSBOX ?= dosbox
DOS_TOOLCHAIN ?= $(CURDIR)/buildenv
export DOS_TOOLCHAIN
.PHONY: all build run test test-dos test-video assets icons-overview benchmark benchmark-speed benchmark-copy package screenshots clean
all: build
.PHONY: build-metadata
build-metadata:
src/BUILD.H: build-metadata tools/build_metadata.py
	python3 tools/build_metadata.py
src/VERSION.H: VERSION tools/version.py
	python3 tools/version.py --write
build: build/NIGHT.EXE
build/NIGHT.EXE: src/BUILD.H $(wildcard src/*.C src/*.H src/*.ASM) src/MAKEFILE VERSION LICENSE assets/ICON-LICENSE.txt build.sh tools/version.py tools/build_metadata.py tools/stage.py tools/check_memory.py tools/dosbox.conf
	bash ./build.sh
run: build
	$(DOSBOX) -conf tools/dosbox.conf -c 'mount c "$(CURDIR)/build"' -c 'c:' -c 'NIGHT /$(MODE)' -c exit
test:
	bash ./tests/run.sh
test-dos:
	bash ./tests/dos.sh
test-video: build
	python3 tests/progress.py
	python3 tests/video.py
	python3 tests/help.py
	python3 tests/features.py
screenshots: test-video
	python3 tools/screenshots.py
assets:
	python3 tools/assets.py
icons-overview: assets
	python3 tools/icon_sheet.py
benchmark: build
	python3 tests/benchmark.py new
benchmark-speed: build
	python3 tests/speed.py
benchmark-copy:
	python3 tests/copy_speed.py
package: build
	python3 tools/package.py
clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
