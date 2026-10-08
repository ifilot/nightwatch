# CI and releases

[build.yml](../.github/workflows/build.yml) runs on every branch/tag push and can
be started manually. All jobs use GitHub-hosted Ubuntu 24.04 machines.
Pull requests run host regression tests. Pushes and manual runs also compile and
test DOS in the standalone [build environment](../buildenv/README.md).

## Build environment

`buildenv/TC/` and `buildenv/TASM/` contain the supplied Turbo C 2.0 and Turbo
Assembler 2.0.1 tools, headers and libraries. The Docker image uses Debian
Trixie and installs DOSBox and DOSBox-X as binary packages from Debian, alongside
the host build and test tools. The GitHub-hosted runner remains Ubuntu 24.04.
Compiler inputs come from this checkout; no emulator is compiled during image
creation. Git generates the build commit in `src/BUILD.H`, which is refreshed at
build time and excluded from version control.
No self-hosted runner, compiler secret or Actions variable is required.

To reproduce the CI environment locally:

```sh
docker build -t nightwatch-build buildenv
docker run --rm --user "$(id -u):$(id -g)" \
  --volume "$PWD:/workspace" nightwatch-build \
  bash -c 'make clean && make build && make test-dos && make test-video && make benchmark && make package'
```

The image build needs Debian's base image and package mirrors. Original Borland
notices remain in `buildenv/`; Debian's DOSBox-X license notices are installed
under `/usr/share/doc/dosbox-x/` in the image. The old vendored DOSBox-X sources
remain available for manual source builds and earlier benchmark reproduction.

The workflow removes old output, compiles `NW.EXE`, checks the small-model
memory limit, then runs DOS/FAT12 filesystem tests, display/keyboard workflows,
production BIOS input/cancellation on an emulated 8086, and the rendering
benchmark. Host tests enforce C89 warnings and address/undefined sanitizers.
Failures stop packaging and publishing; DOS diagnostic logs are retained.
`make benchmark-speed` needs a local before-source snapshot and is not a CI check.

## Tag releases

The project version lives in `VERSION`. Run `python3 tools/version.py --set v1.0.1`
to update it, the committed DOS header and README badge together. Commit those
changes, then push the matching tag. A tag must match the packaged version;
otherwise publishing fails before a release is created.

```sh
git tag v1.0.0
git push origin v1.0.0
```

The release waits for both test jobs and downloads that run's DOS artifact.
It verifies SHA-256 hashes before publishing:

- `NW.EXE`: one 8088 executable containing every display mode.
- `LICENSE.TXT`: GNU GPL version 3 for Nightwatch.
- `VERSION.TXT`: the built version, such as `v1.0.0`.
- `FONTLIC.TXT`: the embedded font license.
- `ICONLIC.TXT`: 16pxls icon attribution and CC-BY-SA-4.0 license.
- `NIGHTWATCH-DOS.zip`: executable, all licenses, version and a DOS README.
- `SHA256SUMS.txt`: hashes for the executable, licenses, version and ZIP.

Only release publishing receives `contents: write`; build jobs receive read
access. Rerunning a successful tag workflow replaces its release assets.
Branch builds expose the same package through workflow artifacts. Compiler files
and the build environment are not included in the downloadable DOS package.
