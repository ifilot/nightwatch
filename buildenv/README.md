# Standalone DOS build environment

This directory contains the Turbo C 2.0 and Turbo Assembler 2.0.1 environment
copied from the supplied local build environment, plus the Debian Trixie Docker
recipe used by GitHub Actions. DOSBox and DOSBox-X are installed from Debian's
package repositories; the image does not compile an emulator. No sibling project,
submodule, compiler download, Actions secret or self-hosted machine is required.
The Borland files retain their original notices; see [BORLAND-NOTICE.md](BORLAND-NOTICE.md).
They are proprietary tools, separate from Nightwatch's GPLv3 code.

From the repository root:

```sh
docker build -t nightwatch-build buildenv
docker run --rm --user "$(id -u):$(id -g)" \
  --volume "$PWD:/workspace" nightwatch-build make build
docker run --rm --user "$(id -u):$(id -g)" \
  --volume "$PWD:/workspace" nightwatch-build \
  bash -c 'make test && make test-dos && make test-video && make benchmark && make package'
```

Outputs appear in the checkout's `build/` directory. The image contains the
compiler at `/opt/dos-toolchain` and runs emulators headlessly. Native Linux
builds use this directory as their default `DOS_TOOLCHAIN`; that variable can
still select an alternative toolchain.

Building the image requires network access to Debian's base image and package
mirrors. Compiler inputs come from this checkout; emulator binaries and their
runtime dependencies come from Debian Trixie. Debian installs the DOSBox-X
license notices under `/usr/share/doc/dosbox-x/`. The image needs no GUI service.

The Debian migration was validated with DOSBox-X `2025.02.01+dfsg-3`: a fresh
Turbo C/TASM build, host and DOS regression tests, all display modes, production
8086 BIOS input and cancellation, rendering benchmarks, and release packaging.

The older DOSBox-X source archive and `build-dosbox-x.sh` remain available for
manual source builds and reproducing earlier measurements. They are not used by
the Docker image. See [vendor provenance](vendor/README.md).
