# Documentation

Use [the manual](manual.md) for the current interface, controls, build setup,
limits and validation commands.

| Reference | Contents |
| --- | --- |
| [CI and releases](ci.md) | Build environment, test jobs, packaging and publishing |
| [Modal inventory](modals.md) | Dialogs, prompts, progress windows and their icon assignments |
| [Modal icon overview](modal-icon-overview.png) | Current modal assignments with actual VGA icons; regenerate with `python3 tools/modal_icons.py` |
| [ZIP support](zip-support.md) | Supported ZIP formats, integrity checks and decoder integration |
| [ARJ support](arj-support.md) | Supported ARJ methods, metadata and recovery behavior |
| [Storage integrity tests](storage-test.md) | Test profiles, saved verification and cleanup |
| [Source comment style](commenting.md) | Contracts, old-tool compatibility and generated files |
| [Icon overview](icon-overview.png) | File-browser icons across VGA, EGA and CGA; regenerate with `make icons-overview` |

[Rendering measurements](rendering.md) records historical benchmarks, with
instructions for running fresh measurements. [The v1.3.0 design history](v1.3.0.md)
preserves earlier proposals and artwork provenance; it is not a current backlog.
The [older icon candidate sheet](icon-options.png) is a historical design study,
rather than the artwork embedded in the program.

Images in `screenshots/` are documentation fixtures. The complete VGA gallery
is in [the repository's screenshot directory](../screenshots/README.md).
Run `make screenshots` from the repository root to regenerate the gallery and
documentation images from real emulator captures. Regeneration requires the
DOS toolchain, DOSBox, DOSBox-X, mtools and Pillow and writes disposable test
workloads under `build/`.
