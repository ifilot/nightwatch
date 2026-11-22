# Changelog

Notable changes to Nightwatch, newest first. This history was reconstructed
from the repository's commits and release tags. Release dates follow the tags;
changes without a release tag are marked unreleased.

## v1.4.0 — Unreleased

### Added

- F9 drive picker for the active pane, with arrow-key or drive-letter selection,
  current-drive highlighting, and an "Enter a path..." option (also the `.` key).
  Enter opens the selected drive's root; Escape preserves the pane and marks.
- Original 16x16 VGA icons for 3.5-inch floppy, 5.25-inch floppy, fixed disk and
  SD card. Unsupported device queries fall back to generic icons; all display
  modes show drive-type descriptions.
- DOS-version-aware drive detection and Slot-otter identification through its
  existing OTTERSD discovery interface, including offline status. Discovery is
  performed once per picker opening and does not mount or unmount the card.
- Host classification tests and DOS picker coverage for drive types, OTTERSD,
  active-pane changes, cancellation, direct paths and critical-error handling.

- Enter launches selected EXE/COM/BAT programs in the active directory.
- On-demand free-space and selected-byte information, with Ctrl-L folder totals.
- Overwrite-all/skip-all choices and source/destination metadata in conflicts.
- Bounded directory pages beyond 512 entries, using Ctrl-PgUp/Ctrl-PgDn.
- Streaming recursive filename search through Alt-F7.
- Cursor editing, session histories and local filename completion for input.
- Ctrl-A properties with editable DOS Read-only/Hidden/System/Archive flags.
- Per-file retry/skip/cancel recovery for copy/move and archive extraction errors.

### Changed

- Graphics menus, prompts, conflicts, file properties, the drive picker and
  storage dashboard now share Help's native frame, title strip and icon.
  Text-mode dialogs retain their character layout and keyboard controls.

- Copy and archive-unpacking progress share Help's graphical window frame,
  blue title bar and white content area, with native labels, transfer speed,
  time remaining and an Escape hint. Incremental updates preserve responsiveness.

- Resolve Turbo C warnings with explicit constant widths, bounded ZIP/zlib
  indices and far-pointer offsets, a compatible opaque state declaration,
  and an explicit storage-manifest parser assignment.

- Free space is sampled once per drive on its first successful visit, using
  a session cache shared by both panels. Folder changes, return visits and
  automatic refreshes reuse the sample; Ctrl-R explicitly updates each drive.
- Release builds link after DOS Make exits, preserving conventional memory
  for the linker as the application grows.

- Graphics viewers and About now share Help's window frame, title bar and
  white document area. Viewers keep position information and controls visible;
  graphical hex rows group pairs while retaining 16 bytes and ASCII.
- Graphical panels reach the screen edges, with selection bars spanning each
  panel's interior.

- `make run` now includes persistent A: (3.5-inch, 1.44 MB), B: (5.25-inch,
  360 KB) and S: (simulated SD card) alongside D: and E:. A demo-only launcher
  supplies drive identities so F9 displays the corresponding icons.

- The F9 footer label is now Drive. Direct path entry remains available inside
  the picker, including when leaving an archive pane.
- VGA pane headers use the detected drive's icon, matching the F9 picker.
- Drive enumeration avoids opening drive roots. Picker filesystem access fails
  back to Nightwatch on DOS critical errors and restores the previous handler.

### Fixed

- Browser keyboard forwarding handles shifted punctuation and modifier release
  during path entry, including colons entered without separate Shift events.

## v1.3.0 — Unreleased

### Added

- Blue/cyan web interface running the real DOS executable through self-hosted
  DOSBox WebAssembly, with disposable D:/E: demo drives, keyboard/touch controls,
  pause, fullscreen and reset. The browser and downloadable DOS package share
  the same executable; reset or reload clears browser-session changes.
- Filterable gallery of all 61 native VGA screenshots, with enlarged viewing
  and keyboard navigation, including copy/extraction progress with kB/s markers.
- Text viewer header shows the current logical file line and total line count
  after paging, search and byte-offset jumps, including archive members.
- Enter opens ZIP/ARJ archives as read-only folders, with subfolder navigation,
  F3/Shift-F3 member viewing and F5 extraction of selected files/folder trees.
  Listings use validated headers without unpacking; source archives stay intact.
- Built-in ARJ extraction for stored files and methods 1–4 through Ctrl-U
  and `/unarj`, with bounded preflight, DOS 8.3 path checks, CRC validation,
  overwrite/skip/cancel and staged file replacement shared with ZIP.
- Independent method 0–4 ARJ fixtures, malformed-input and fault coverage,
  16-bit 8086/FAT12 checks and VGA navigator extraction coverage.
- Original Quick Menu inspired 16x16 color icons inside the VGA interface,
  retaining the existing row count and transparent selection backgrounds.
  All eight categories include reproducible artwork and previews, with an
  alternative Windows 3.x inspired study. EGA and CGA retain their artwork.
- Fictional D: work and E: backup drives for `make run`, containing representative
  documents, source trees, images and real ZIP/ARJ archives. Existing local demo
  files are preserved between launches.

### Changed

- `make site` builds a self-contained static website with pinned, checksum-checked
  emulator downloads, corresponding sources and license notices. The site works
  under URL subpaths and makes no third-party runtime requests.
- Pushes to `master` build and browser-test the website from the tested DOS
  package before deploying to GitHub Pages. Other branches, tags, pull requests
  and manual runs do not deploy; Pages must use the GitHub Actions source.
- DOS CI runs independent suites in parallel and reuses checked build inputs.
  Compiler/linker gates require fresh outputs and verify the DOS near-heap budget.

## v1.2.0 — Unreleased

### Added

- Original transparent 32x32 EGA Quick Menu launcher icon (`NIGHT.ICC`),
  included beside `NW.EXE` in the DOS package with import instructions.
- Consolidated the v1.1.1 display-mode persistence option: F2, R toggles
  automatic mode saving in `NIGHT.CFG`, while Ctrl-W saves pane paths and sorting.
- Ctrl-T storage integrity tests with quick/long profiles, recorded 32-bit seeds,
  exact file verification, directory growth, append/overwrite/truncate/zero-gap
  checks, moves and allocation reuse. Separate local manifests/logs support
  verification after reboot and cleanup of recorded files, including partial runs.
- Native storage-test dashboard with phase checkmarks, per-file and overall
  progress, live counters and a retained PASS/failure/cancellation summary.
- Storage test regression coverage for independent expected data, long directory
  enumeration, corruption, cancellation, failed writes and cleanup ownership.
- Built-in ZIP extraction using the vendored zlib 1.3.2 raw-inflate subset:
  Ctrl-U unpacks into the inactive pane; `/unzip` supports unattended extraction.
  Stored/DEFLATE entries, subfolders, data descriptors, overwrite/skip/cancel,
  size/CRC validation and staged recovery are supported with DOS 8.3 names.
- Windows PowerShell/.NET and Linux Info-ZIP fixtures, compression level/strategy
  coverage, malformed-archive and fault tests, and actual 8086/FAT12 checks.
- Retained the optional zlib Turbo C/8086 feasibility probe.
- Regression coverage for copy length, source preservation, DOS listing
  faults, overwrite input isolation, progress throttling and exact icon masks.

### Changed

- Medium-model far code removes the single 64 KiB code limit. Fonts are read
  directly from their immutable segment; fixed Huffman tables also move outside
  near data to retain the pane caches, copy buffer and runtime heap budget.
- Recursive counting reuses directory-entry metadata; tiny-file copy batches
  coalesce progress paints while retaining per-callback Escape polling.
- VGA/EGA icons store and draw one monochrome mask; removed the unused text
  renderer and reduced the graphical row cache to its 22-row maximum.

### Fixed

- Storage tests offer to create missing target/log folders and their parents
  after confirmation, instead of failing when a chosen folder is absent.
- Hardened copy/move staging against premature EOF and source-size changes,
  and isolated deferred pane keys from overwrite decisions.
- Directory refresh distinguishes DOS enumeration failures from normal
  completion and invalidates failed partial listings while preserving valid
  cached listings when a scan cannot start.

## v1.1.1 — Unreleased

### Added

- F2, R toggles remembering the last display mode in `NIGHT.CFG`. Successful
  mode selections are saved automatically, while Ctrl-W continues to save pane
  paths and sorting. Existing configuration files remain compatible; explicit
  startup mode arguments still take precedence.

## v1.1.0 — 2026-10-08

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
