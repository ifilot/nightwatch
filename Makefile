# SPDX-License-Identifier: GPL-3.0-only
# Host-side entry points. DOS compilation uses src/MAKEFILE inside DOSBox.
# Keep bare make on the build even though generated-header rules appear first.
.DEFAULT_GOAL := all
MODE ?= text
DOSBOX ?= dosbox
DOS_TOOLCHAIN ?= $(CURDIR)/toolchain
export DOS_TOOLCHAIN
.PHONY: all build run test test-dos test-video assets benchmark benchmark-speed package clean
all: build
src/VERSION.H: VERSION tools/version.py
	python3 tools/version.py --write
build: build/NIGHT.EXE
build/NIGHT.EXE: $(wildcard src/*.C src/*.H src/*.ASM) src/MAKEFILE VERSION LICENSE build.sh tools/version.py tools/stage.py tools/check_memory.py tools/dosbox.conf
	bash ./build.sh
run: build
	$(DOSBOX) -conf tools/dosbox.conf -c 'mount c "$(CURDIR)/build"' -c 'c:' -c 'NIGHT /$(MODE)' -c exit
test:
	bash ./tests/run.sh
test-dos:
	bash ./tests/dos.sh
test-video: build
	python3 tests/video.py
	python3 tests/features.py
assets:
	python3 tools/assets.py
benchmark: build
	python3 tests/benchmark.py new
benchmark-speed: build
	python3 tests/speed.py
package: build
	python3 tools/package.py
clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
