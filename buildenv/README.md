# Standalone DOS build environment

This directory contains the Turbo C 2.0 and Turbo Assembler 2.0.1 environment
copied from the supplied local build environment, plus pinned DOSBox-X Linux
source and the Docker recipe used by GitHub Actions. No sibling project,
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

Building the image requires network access to Ubuntu's base image and package
mirrors. Project-specific compiler and emulator sources are all in this checkout.
DOSBox-X is compiled from the checked archive and installs its binary and license.
The image needs no GUI service. See [vendor provenance](vendor/README.md).
