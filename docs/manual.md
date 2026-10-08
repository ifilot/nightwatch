# Nightwatch

An 8088-compatible, two-pane file navigator for MS-DOS, inspired by Midnight
Commander. One DOS filesystem backend and one keyboard/UI implementation serve
all four display modes. The program is standalone: fonts and icons are embedded;
no graphics drivers, mouse driver, extender, EMS or XMS are required.

| Mode | BIOS mode | Resolution | Interface | Adapter |
| --- | --- | --- | --- | --- |
| Text (default) | 03h / 07h | 80 x 25 characters | CP437 double-line frames, colored function keys | CGA, MDA/Hercules, EGA, VGA |
| CGA monochrome | 06h | 640 x 200, 1 bit | Pixel layout, compact icons, Spleen 5 x 8 | CGA or compatible |
| EGA | 10h | 640 x 350, 16 colors | Pixel layout, 16 x 16 icons, Spleen 6 x 12 | EGA with at least 128 KB video RAM, or VGA |
| VGA | 12h | 640 x 480, 16 colors | Pixel layout, 16 x 16 icons, Spleen 8 x 16 | VGA |

**Select the mode** at startup with `NIGHT /text`, `NIGHT /cga`, `NIGHT /ega`,
or `NIGHT /vga`. During use, press **F2**, then **T**, **C**, **E**, or **V**.
Escape cancels. Adapter detection rejects unsupported modes. Text mode defaults
to 80-column color text, or monochrome text when launched from mode 07h.

## Build and run

Turbo C 2.0 and its DOS MAKE run inside DOSBox. The code uses C89, the small
memory model, and `-1-` to explicitly disable 80186/286 instructions. A small 8086 assembly module embeds font
resources outside the small-model data segment; no Borland BGI driver is used.
Turbo Assembler 2.0 is also required.

Host prerequisites: GNU Make, Bash, Python 3 and DOSBox. Provide Turbo C 2.0 in
an external directory with `TC/TCC.EXE`, `TC/MAKE.EXE`, `TC/INCLUDE`, `TC/LIB`,
and `TASM/TASM.EXE`. The toolchain is proprietary and its binaries are not
copied into this repository. Set `DOS_TOOLCHAIN` to their installation directory
(or place them in `toolchain/`):

```sh
export DOS_TOOLCHAIN=/path/to/toolchain
make
make run                 # launches a DOSBox window in text mode
make run MODE=cga
make run MODE=ega
make run MODE=vga
```

Override the location or emulator when needed:

```sh
make DOS_TOOLCHAIN=/path/to/buildenv
make run MODE=vga DOSBOX=dosbox-x
```

The executable is **`build/NIGHT.EXE`**. Compile logs and a linker map also
appear in `build/`. Sources are staged there with DOS line endings and DOS
filenames. The source directory and toolchain are not modified by
compilation. `make clean` removes generated build artifacts.

`make run` mounts only `build/` as the DOS C: drive; to work with other files,
mount their directory in DOSBox and invoke the executable manually. For example:

```dos
mount c /path/to/nightwatch/build
mount d /path/to/files
c:
NIGHT /vga D:\SOURCE D:\DEST
```

Use `NIGHT /?` for startup syntax. Optional arguments give the left and right
pane directories. Without them both panes start in the current directory. On DOS
hardware, copy `NIGHT.EXE`, `FONTLIC.TXT` and `ICONLIC.TXT` to a disk and run it directly. The
license file accompanies redistributed font data; the executable needs no
external font or icon files.

## Controls and behavior

| Key | Action |
| --- | --- |
| Tab | Switch pane |
| Up / Down, Home / End | Move selection |
| PgUp / PgDn | Scroll selection by a page |
| Enter | Enter a directory; view a file; run a typed command |
| Backspace | Parent directory when command line is empty; otherwise erase a character |
| Insert / Space | Mark a file or directory and advance |
| + / - | Mark all entries except `..` / clear marks, when command line is empty |
| F1 / Alt-F1 | Help (A opens About) / About with version and licenses |
| F2 | Display mode selector |
| F3 / Shift+F3 | Built-in text / hex viewer; F4 inside either viewer switches text/hex, Esc returns |
| F4 | External editor (`EDITOR` environment variable, default `EDIT`) |
| F5 | Copy marked files, or selected file, to the other pane or a specified path |
| F6 | Move/rename marked files or selected entry |
| F7 | Create an 8.3 directory |
| F8 | Confirm recursive deletion of files or directories |
| F9 | Change the active pane's directory/drive (e.g. `A:\`) |
| F10 | Quit and restore the original video mode, page and cursor |
| Ctrl-R | Refresh both panes |
| Ctrl-F | Find the next filename matching a pattern (`*`, `?`), wrapping within the pane |
| Ctrl-P / Ctrl-N | Mark / unmark entries matching a filename pattern |
| Ctrl-S | Sort active pane: name, size, date/time, extension, or reverse |
| Ctrl-W | Save display mode, pane paths and sorting in the startup directory’s `NIGHT.CFG` |
| Ctrl-O | Open DOS shell; `EXIT` returns to the navigator |
| Esc | Clear command line / cancel dialog |

Help presents scrollable documentation with regular-weight headings. Up/Down
scroll one line, PgUp/PgDn scroll a page, and Home/End jump to the beginning/end.
Esc, F1 or Q closes Help; A opens About. The graphical modal keeps its title,
repository, version, build commit and controls fixed around the document.
Text mode offers the same documentation and controls. A `-dirty` suffix means
the build includes uncommitted changes; source exports without Git show
`unknown`. The build refreshes this information automatically.
The document is embedded with the fonts outside the 64 KiB data segment;
no external help file is required. Edit `assets/help.txt` and run `make assets`
to regenerate it. Authored lines are checked against the narrowest viewport.

Prompts accept Enter, Escape and Ctrl-U (clear). Copy/move targets must be
absolute DOS paths; multiple marked entries require an existing destination
directory. Existing files prompt for **O** overwrite, **S** skip, or Escape to
cancel. Existing directories merge recursively; conflicting files still prompt.
Batch operations stop at the first error and retain completed work. Copy uses an
exclusive temporary file in the destination directory, closes it, then renames
it into place. DOS timestamps and readonly/hidden/system/archive attributes are
preserved. Cross-drive moves copy first and remove each source only after
successful copying. A skipped member remains at its source, together with its
containing directories. Replacement files are fully prepared before the original
is moved to a temporary backup; a failed commit attempts to restore the
original. Any retained backup is reported.

Before copying, Nightwatch scans every selected file and subfolder to count
files and sum their sizes. "Counting files" can be cancelled with Escape; no
files are copied until the scan succeeds. Directories do not count as files.
Totals are limited to 4,294,967,295 bytes; larger selections report an error.
The copy dialog shows completed/total files, skipped files, current-file bytes,
and separate file and overall progress bars in every display mode. Graphics
modes use solid pixel bars with inset tracks; text mode uses character bars.
Graphical updates paint only the added fill, keeping the existing fill and
frame in place. Tracks are rebuilt when the dialog opens or resumes after an
overwrite prompt; the current-file fill resets when the next file starts. Overall
progress includes skipped bytes, so all resolved files advance the bar.
For selections containing only empty files it advances by resolved file count.
A smoothed recent transfer rate appears in KiB/s with estimated time remaining
for the current file. Both show `--` until a sample is available; overwrite
prompt time is excluded from the rate. Move/delete retain their existing byte
and completed/skipped displays (a whole-directory rename counts as one completed
operation; recursive transfers count committed files). Escape cancels between
transfer blocks or directory entries. A partially copied current file is
removed; previously completed files remain. Recursive operations enumerate every
child independently of the 512-entry pane cache. Traversal is bounded to 32
levels and 127-byte paths, with an explicit error. Read-only files are preserved
and are not automatically made writable for deletion. File timestamps and
file/directory DOS attributes are preserved; directory timestamps are not
copied. Returning to a parent reselects the directory just left.

Copying uses a 16 KiB buffer allocated in conventional far memory, outside the
64 KiB data segment. If allocation fails, it uses the existing 2 KiB buffer.
Escape is checked after each transferred block; progress remains based on bytes
successfully written. No extended or expanded memory is required.

`make benchmark-copy` compares 2, 8 and 16 KiB buffers on DOSBox-X's 8086 core
at 3,000 cycles, checking copied bytes, transfer-block counts and cancellation
cleanup. An initial run copying three 1 MiB files took 19, 15 and 14 BIOS ticks
respectively, with 1,536, 384 and 192 blocks. These are coarse emulator timings
on a cached host drive; they measure CPU/DOS overhead, not physical disk seeks
or expected throughput on real hardware. Cancellation is checked after at most
one additional block, whose I/O duration depends on the drive.

Both viewers offer **F7** search, **F8** next match and **Ctrl-G** byte-offset
jump. Search accepts case-insensitive literal ASCII or exact binary patterns
such as `hex:DE AD 00 BE EF` (up to 63 bytes). Jumps accept decimal or `0x`
hexadecimal. Matches appear at the top of the view. Text supports Home/End and
backward paging beyond its 128 cached offsets by rescanning earlier content when
necessary.

Settings are saved only on Ctrl-W. Subsequent launches from the same startup
directory load `NIGHT.CFG`; explicit mode and pane arguments override saved
values. Missing saved directories fall back to the startup directory, and an
unavailable saved display mode falls back to text. Corrupt, oversized or
incomplete settings records are ignored as a whole.

The shell and editor run in the active pane's directory, with the original text
mode restored. After a command, press a key to return. An interactive shell
returns on `EXIT`. The editor executable must be on DOS's PATH; `EDITOR` may
contain a command with options. A DOS `COMMAND.COM` must be available through
`COMSPEC`.

### Current boundaries

- DOS 3.0 or later; DOS 8.3 names and conventional memory only.
- A cache of 512 entries per pane, including `..`; directories sort before
  files. `[LIMIT]` explicitly reports truncation. Hidden and system files are
  included.
- Recursive directory copy/move/delete; same-drive moves prefer DOS rename.
  Existing directories merge, and files require an overwrite/skip decision.
- Keyboard operation; no mouse, archives or network protocols. Filename
  patterns, sorting and viewer search are available through keyboard commands.
- Native text retains the DOS character map. Graphics fonts map CP437 bytes to
  embedded Spleen glyphs, with explicit fallback for glyphs missing in small
  sizes. High bytes in the text viewer still display as dots.
- The text viewer caches 128 page offsets and rescans for older pages. Both
  viewers stream files; text End and uncached backward paging may take time on
  large files or slow disks.
- Hex view shows eight-digit byte offsets, 16 hexadecimal bytes per row in two
  groups, and printable ASCII (other bytes appear as dots). PgDn/Space/Enter and
  PgUp page, Up/Down scroll one row, Home/End jump to the first/last page. F4
  switches near the current byte position; returning to text resets its page
  history. Hex paging has no history limit. Files use DOS signed 32-bit seek
  offsets (up to 2 GB). Viewing never modifies file contents.
- The original video mode is restored, but original screen contents are cleared.
- A 64 KB EGA card cannot provide all 16 colors at 640 x 350; use text or CGA.
  The EGA BIOS memory report is checked; monitor wiring is not detected.

## Display design, rendering and memory

The text interface uses DOS CP437 double-line borders. Directory names, marked
files and the numbered function-key strip have separate attributes.

Graphics use an independent pixel-coordinate desktop in `src/GRAPH.C`: compact
path headers, file-type icons, white list surfaces, column headings, selection
bars and proportional scroll indicators. A single-line numbered function-key
strip replaces large buttons. The command field also shows operation status when
empty; the permanent title and instructional strips have been removed. CGA uses
high-contrast monochrome and 16 x 8 icons adjusted for its pixel aspect ratio.
EGA and VGA share monochrome 16 x 16 icons from the 16pxls set. The existing
font sizes remain readable, with less surrounding padding.

| Mode | Entries per pane | Row pitch | Function-key strip height |
| --- | ---: | ---: | ---: |
| Text | 17 | One character row | One character row |
| CGA | 18 | 8 pixels | 10 pixels |
| EGA | 16 | 16 pixels | 14 pixels |
| VGA | 24 | 16 pixels | 18 pixels |

Paging and scrolling use the actual visible capacity. Long command input scrolls
horizontally within its field, preserving the footer and pane borders.

Fonts are the vendored, BSD-licensed [Spleen
2.2.0](https://github.com/fcambus/spleen/tree/2.2.0) bitmap family. Icon sources, attribution and generated tables are documented in
[assets/README.md](../assets/README.md). `make assets` regenerates the embedded
font and icon resources offline. `build/FONTLIC.TXT` contains the font license;
`build/ICONLIC.TXT` credits Paul Mackenzie and includes the 16pxls CC-BY-SA-4.0 license.

`src/CORE.C` owns sorting, pane navigation, path bounds and file transfers.
`src/FSDOS.C` implements DOS directory and file calls. `src/MAIN.C` owns input
and operations, with separate text and graphics views of the same backend.
`src/VIDEO.C` provides cell overlays and pixel drawing primitives.

Text uses B800/B000 memory; original CGA text writes synchronize with blanking
rather than introducing snow. CGA graphics use the interleaved B800 layout.
EGA/VGA fills broadcast a CPU write to all four A000 bitplanes using set/reset.
Opaque text groups planes by foreground/background bits and streams scanline
bytes with `REP MOVS`; transparent icon masks use hardware latches. Cached glyph
and scanline offsets avoid multiplication in the inner rendering loops. Register
writes are cached. This follows the [FreeVGA graphics register
reference](https://www.osdever.net/FreeVGA/vga/graphreg.htm) and [sequencer
reference](https://www.osdever.net/FreeVGA/vga/seqreg.htm).

The desktop keeps only visible-entry snapshots. Moving the cursor repaints the
two affected entries and the position indicator; idle redraws write no video
memory. There is no RAM framebuffer. Dialogs and the viewer share a cell overlay
renderer, which redraws changed spans and returns to the pixel desktop on close.
Labels rasterize in a 512-byte scratch buffer and batch color-plane writes
across scanlines. Graphical viewers use the same opaque rendering path.
Scrolling copies unchanged pixels using CGA bank-aware transfers or EGA/VGA
hardware latches. Pane summaries repaint only changed character spans; marks and
content revisions avoid directory scans and entry comparisons on ordinary input.
Text mode retains its cell buffer and updates affected rows instead of
rebuilding the desktop.

Two 512-entry pane caches occupy about 25 KB. The transfer buffer is 2 KB; cell
buffers total 9,600 bytes, the font cache is 4 KB, and the stack is 8 KB.
Embedded fonts live in their own segment outside DGROUP. The executable is about
60 KB; **allow 128 KB free conventional memory**. 256 KB or more installed RAM
remains a practical DOS system target. Shells/editors need additional memory.
The build checks that static near data plus the actual 8 KB runtime stack leave
at least 2 KB of the 64 KB small-model segment for stdio and other heap use.
Detailed VRAM/cell/refresh counters are enabled only in diagnostic test builds.
The instruction target is 8086/8088; physical 4.77 MHz IBM 5150 performance and
CGA snow behavior still need hardware testing.

### Rendering timings

`make benchmark` measures initial drawing, ten cursor moves and ten unchanged
redraws with a fixed file fixture, DOSBox-X's 8086 core, and 3,000 cycles.
Results are BIOS ticks (about 55 ms each); small results have one-tick
quantization. The original renderer measured 6/39/53 ticks for the initial
CGA/EGA/VGA screens and 23/49/55 ticks for ten moves. See
[docs/rendering.md](rendering.md) for current measurements and method.
These compare the old and new interfaces, rather than claiming physical hardware
timing.

## Validation

```sh
make test         # strict C89 host backend tests with address/undefined sanitizers
make test-dos     # real DOS calls on a FAT12 image (DOSBox-X + mtools required)
make test-video   # adapter + BIOS input + 8086 checks; Pillow and DOSBox-X required
```

Host tests cover multi-buffer binary copies, short writes, read/write/metadata
failures, collision protection, temporary-file cleanup, sorting, paths, filename
validation, scrolling, mark counts, independent directory snapshots, failed
loads and directory-cache bounds. Dedicated fault tests use different original
and replacement contents to verify commit/rollback failures, retained backups,
cancellation, skipped cross-drive moves, overlapping trees and temporary-name
collisions. Navigation tests assert every sort order, reversal and tie handling.
LeakSanitizer is disabled because the sandbox runs processes under tracing;
address and undefined-behavior instrumentation remain enabled.

DOS integration tests exercise byte-exact copying, moving, timestamps,
attributes, root-directory lookup, overwrite recovery, cancellation, recursive
operations, cross-drive moves and traversal at the 32-level stack bound. They
use DOSBox-X because DOSBox 0.74 does not implement attribute setting
faithfully.

Video tests include file-type icon fixtures and compile a separate test
executable with scripted key input; the production executable still reads BIOS
keyboard events. They run the actual UI and backend against each emulated
adapter, capture real video memory, decode it to PNG, and check visible output,
selection movement, viewer paging, copy, mkdir, delete, mode selection and exit.
Captures appear in `build/video/`. These tests run without an X server. Text
captures preserve and verify actual CP437 cells; their image previews substitute
Spleen glyphs for the upper half of the ROM font. The production executable is
also launched with real BIOS keyboard events, including a CGA graphics run on
the DOSBox-X 8086 CPU core. A production cancellation test injects a key
followed by Escape during a DOS write, checking that the current temporary is
removed and earlier work remains. Scripted workflows also cover recursive
operations, pattern selection, saved settings and viewer search/jump boundaries.
Set `NW_FEATURE_8086=1` to run these workflows on the 8086 core with a FAT16
volume (`mtools` required) ([CPU
documentation](https://dosbox-x.com/wiki/Guide%3ACPU-settings-in-DOSBox%E2%80%90X)).
`tests/gui.py` is an optional window/keyboard smoke test for environments with
Xvfb, xdotool, ImageMagick and Pillow.

Internal operations reload only directories they can affect. Refresh still
clears marks in both panes. Matching pane paths share one directory enumeration
and sort while retaining independent cursor and mark state. Ctrl-R and return
from an external command rescan both panes to pick up unrelated external
changes.

Additional regression checks cover viewer keys that leave the position unchanged
(zero page reads and VRAM writes), empty files, shorter final pages, independent
marks in matching directories, and operation-specific refresh counts. When a
`build/speed-before/video/` snapshot is present with matching key scripts, video
tests can require byte-for-byte equality with its saved VRAM captures by setting
`NW_COMPARE_OLD_VRAM=1`.

`make benchmark-speed` compares 512-entry synthetic panes, cursor movement,
viewport scrolling, typing, hex viewing and unchanged draws on the fixed-cycle
8086 core. It also measures viewport copying against forced redrawing. For
before/after comparisons, preserve the earlier sources in
`build/speed-before/src/` before editing. The baseline is a local artifact.

## Version and license

Nightwatch v1.0.0 is licensed under GPL-3.0-only; see [LICENSE](../LICENSE).
Spleen font data remains BSD-2-Clause. The About window (Alt-F1 or F1 then A)
shows the code, font and 16pxls icon credits. `NIGHT /version` or `NIGHT --version` prints the version.
[VERSION](../VERSION) is authoritative; `tools/version.py` updates its DOS
header and README badge, and `--check` catches inconsistencies in CI. Release
ZIPs include the GPL text as LICENSE.TXT, font license as FONTLIC.TXT, icon attribution/license as ICONLIC.TXT, and
VERSION.TXT.
