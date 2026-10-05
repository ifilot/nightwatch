# CI and releases

[build.yml](../.github/workflows/build.yml) runs on every branch/tag push and can
be started manually. Host regression tests also run for pull requests. DOS
compilation and integration tests run on trusted pushes, using an installed
Turbo C 2.0/TASM 2.0 toolchain.

## Runner setup

1. Register a Linux x64 [self-hosted GitHub Actions runner](https://docs.github.com/en/actions/how-tos/manage-runners/self-hosted-runners/use-in-a-workflow)
   for the repository and give it the custom label `nightwatch-dos`. Use a current
   runner release (the checkout action uses Node 24).
2. Install Bash, GNU Make, a C compiler, Python 3, Pillow, DOSBox, DOSBox-X,
   mtools and ripgrep. For example, the usual Linux package names are
   `build-essential python3 python3-pil dosbox mtools ripgrep`; install DOSBox-X
   separately if the distribution does not supply it. Tests run headlessly.
3. Install your licensed DOS tools outside the checkout:

   ```text
   /opt/dos-toolchain/
   ├── TC/TCC.EXE
   ├── TC/MAKE.EXE
   ├── TC/INCLUDE/
   ├── TC/LIB/
   └── TASM/TASM.EXE
   ```

   The runner account needs read access. If installed elsewhere, set the
   repository **Settings → Secrets and variables → Actions → Variables →
   DOS_TOOLCHAIN** to that absolute path. No compiler binaries are uploaded.

The workflow removes old build output, compiles `NIGHT.EXE`, checks the small-model
memory limit, then runs DOS/FAT12 filesystem tests, all display/keyboard workflows,
production BIOS input/cancellation on an emulated 8086, and the rendering
benchmark. Host tests run with C89 warnings and address/undefined sanitizers.
Failures stop packaging and publishing; compile/runtime logs are retained.
`make benchmark-speed` needs a local before-source snapshot and is not a CI check.

## Tag releases

The project version lives in `VERSION`. Use `python3 tools/version.py --set v1.0.1`
to update it, the committed DOS header and README badge together. Commit those
changes, then push the matching tag. A tag must match the packaged version;
otherwise publishing fails before any release is created. For the first release:

```sh
git tag v1.0.0
git push origin v1.0.0
```

The release waits for **both** test jobs and downloads the exact DOS artifact
from that workflow run. It checks SHA-256 hashes before publishing:

- `NIGHT.EXE`: one 8088 executable containing every display mode.
- `LICENSE.TXT`: GNU GPL version 3 for Nightwatch.
- `VERSION.TXT`: the built version, such as `v1.0.0`.
- `FONTLIC.TXT`: the embedded font license.
- `NIGHTWATCH-DOS.zip`: executable, both licenses, version and a short DOS `README.TXT`.
- `SHA256SUMS.txt`: hashes for the executable, licenses, version and ZIP.

Release publishing alone receives `contents: write`; build jobs receive read
access. Rerunning a successful tag workflow replaces its release assets.
Branch builds expose the same package through the workflow's artifacts.

GitHub's [workflow syntax](https://docs.github.com/en/actions/reference/workflows-and-actions/workflow-syntax)
and [release CLI](https://cli.github.com/manual/gh_release_create) document the
push/tag triggers and publishing commands used here. Until the labelled runner
is registered and online, DOS jobs remain queued.
