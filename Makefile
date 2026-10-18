# SPDX-License-Identifier: GPL-3.0-only
# Host-side entry points. DOS compilation uses src/MAKEFILE inside DOSBox.
# Keep bare make on the build even though generated-header rules appear first.
.DEFAULT_GOAL := all
MODE ?= vga
DOSBOX ?= dosbox
DOS_TOOLCHAIN ?= $(CURDIR)/buildenv
export DOS_TOOLCHAIN
.PHONY: all build run demo site serve-site test-site test test-dos test-zip-dos test-arj-dos test-browse-dos test-video test-storage-dos assets icons-overview benchmark benchmark-speed benchmark-copy package screenshots clean
all: build
.PHONY: build-metadata
build-metadata:
src/BUILD.H: build-metadata tools/build_metadata.py
	python3 tools/build_metadata.py
src/VERSION.H: VERSION tools/version.py
	python3 tools/version.py --write
src/NW.RSP: src/MAKEFILE tools/dos_link.py
	python3 tools/dos_link.py src/MAKEFILE src/NW.RSP
build: build/NW.EXE
build/NW.EXE: src/BUILD.H src/NW.RSP $(wildcard src/*.C src/*.H src/*.ASM vendor/zlib/*) src/MAKEFILE VERSION LICENSE assets/ICON-LICENSE.txt build.sh tools/version.py tools/build_metadata.py tools/stage.py tools/dos_link.py tools/check_memory.py tools/dosbox.conf
	bash ./build.sh
demo:
	python3 tools/prepare_demo.py
run: build demo
	$(DOSBOX) -conf tools/dosbox.conf -c 'mount c "$(CURDIR)/build"' -c 'imgmount a "$(CURDIR)/build/demo/A.IMG" -t floppy -fs fat' -c 'imgmount b "$(CURDIR)/build/demo/B.IMG" -t floppy -fs fat' -c 'mount d "$(CURDIR)/build/demo/D"' -c 'mount e "$(CURDIR)/build/demo/E"' -c 'mount s "$(CURDIR)/build/demo/S"' -c 'c:' -c 'DEMO\DEMODRV NW.EXE /$(MODE) D:\ E:\' -c exit
test:
	bash ./tests/run.sh
test-dos:
	bash ./tests/dos.sh
test-zip-dos: build
	python3 tests/zip_dos.py
	python3 tests/zip_fat12.py
test-arj-dos: build
	python3 tests/arj_dos.py
test-browse-dos: build
	python3 tests/browse_dos.py
test-video: build
	python3 tests/progress.py
	python3 tests/video.py
	python3 tests/help.py
	python3 tests/mine.py
	python3 tests/features.py
	python3 tests/drive_dos.py
	python3 tests/demo_dos.py
	python3 tests/workflows_dos.py
	python3 tests/storage_ui.py
	python3 tests/storage_dos.py
test-storage-dos:
	python3 tests/storage_dos.py
screenshots: test-video
	python3 tests/browse_dos.py
	python3 tools/vga_screenshots.py
	python3 tools/screenshots.py
assets:
	python3 tools/assets.py
	python3 tools/zlib_tables.py
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
site: package
	python3 tools/build_site.py
serve-site:
	python3 -m http.server 8000 --bind 127.0.0.1 --directory build/site
test-site:
	node tests/check_site.cjs
clean:
	python3 -c 'import shutil; shutil.rmtree("build", ignore_errors=True)'
