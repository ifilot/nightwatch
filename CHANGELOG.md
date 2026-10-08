# Changelog

Notable changes to Nightwatch, newest first. This history was reconstructed
from the repository's commits and release tags. Release dates follow the tags;
changes without a release tag are marked unreleased.

## v1.1.0 — Unreleased

### Added

- Scrollable embedded Help documentation with regular-weight headings, a
  graphical scrollbar, and fixed version/repository/commit details.
- Incremental graphical progress-bar updates that retain the existing fill and
  frame, avoiding the clear-and-repaint flash during copying.
- A 16 KiB conventional far-memory copy buffer, with automatic 2 KiB fallback
  when memory is scarce, retaining cancellation and safe replacement behavior.
- Solid graphical copy-progress bars with shaded tracks, grouped graphical Help
  with repository/version/build-commit details, and an original perspective
  hard-drive icon for panel paths.
- Reproducible README screenshots from emulator VRAM via `make screenshots`.

- Recursive counting of selected files and their total size before copying,
  including files in subfolders and beyond the 512-entry pane cache. The
  counting pass supports Escape cancellation and finishes before copying starts.
- Completed/total file counts and separate current-file and overall progress
  bars in text, CGA, EGA and VGA modes.
- Smoothed transfer speed in KiB/s and estimated time remaining for the current
  file. Time spent answering overwrite prompts is excluded from speed samples.
- Regression checks for recursive totals, counting failures and cancellation,
  skipped bytes, empty-file batches, progress near the 32-bit limit, BIOS clock
  rollover, transfer speed and time estimates.

### Changed

- The executable is now NW.EXE; startup commands, build outputs and release
  packages use the shorter name. Existing NIGHT.CFG settings remain compatible.

- Graphical cell-based dialogs draw crisp borders at the outside of their
  filled background, removing the rim left by centered text-mode strokes.
- VGA and EGA share monochrome 16x16 icons from Paul Mackenzie's 16pxls set.
  VGA/EGA use 17-pixel rows with a white separator, showing 22/15 entries
  per pane respectively. EGA has taller path captions;
  CGA retains its previous icons and layout. Icon attribution and license
  are included in About and the DOS package as ICONLIC.TXT.
- The CI build image uses Debian Trixie and its packaged DOSBox-X emulator,
  avoiding emulator compilation during image creation.
- Overall copy progress includes skipped files' sizes, so it reaches completion
  after every file has been handled. Batches containing only empty files use
  completed and skipped file counts for overall progress.
- Copy selections exceeding 4,294,967,295 bytes report an error during counting
  instead of overflowing the progress totals.
- The hex viewer's temporary page buffer uses stack space to preserve near-heap
  room for the expanded copy dialog on the DOS small memory model.
- Updated the manual and rendering notes to describe copy progress and memory
  use.

## v1.0.0 — 2026-10-07

Initial tagged release, including the application introduced on 2026-10-05
and the standalone build-environment changes merged before the release.

### Added

- Keyboard-driven, two-pane file navigation for DOS 3.0+ on 8088/8086 PCs,
  including the IBM 5150.
- A single executable supporting text, CGA, EGA and VGA displays, with embedded
  fonts and icons and display-mode switching during use.
- File marking, wildcard selection and filename searches, sorting, drive/path
  navigation and independent pane selections.
- Recursive copy, move and delete operations, overwrite/skip prompts, byte
  progress, completed/skipped counts and Escape cancellation. Transfers preserve
  DOS timestamps and attributes and use temporary files for copy commits and
  overwrite recovery.
- Built-in text and hex viewers with text/hex switching, ASCII and binary
  searches, byte-offset jumps and navigation beyond cached text pages.
- External editor and DOS shell access, directory creation, help and About
  dialogs, and saved display mode, pane paths and sorting in `NIGHT.CFG`.
- Host regression tests with sanitizers, DOS filesystem integration tests,
  display captures, production BIOS keyboard/cancellation checks on an emulated
  8086, and rendering benchmarks.
- Version synchronization, DOS release packaging and checksums, GPLv3 and font
  license notices, and automated builds, tests and tag-triggered publishing.
- A repository-contained Turbo C/TASM toolchain, DOSBox-X sources and Docker
  build environment. DOS CI runs on GitHub-hosted Ubuntu runners; native builds
  default to `buildenv/` and can use an alternative `DOS_TOOLCHAIN`.
