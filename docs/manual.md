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

**Select the mode** at startup with `NW /text`, `NW /cga`, `NW /ega`,
or `NW /vga`. During use, press **F2**, then **T**, **C**, **E**, or **V**.
Escape cancels. Adapter detection rejects unsupported modes. Text mode defaults
to 80-column color text, or monochrome text when launched from mode 07h.

## Build and run

Turbo C 2.0 and its DOS MAKE run inside DOSBox. The code uses C89, the medium
memory model, and `-1-` to explicitly disable 80186/286 instructions. A small 8086 assembly module embeds font
resources outside the near-data segment; no Borland BGI driver is used.
Turbo Assembler 2.0 is also required.

Host prerequisites: GNU Make, Bash, Python 3 and DOSBox. Provide Turbo C 2.0 in
an external directory with `TC/TCC.EXE`, `TC/MAKE.EXE`, `TC/INCLUDE`, `TC/LIB`,
and `TASM/TASM.EXE`. The toolchain is proprietary and its binaries are not
copied into this repository. Set `DOS_TOOLCHAIN` to their installation directory
(or place them in `toolchain/`):

```sh
export DOS_TOOLCHAIN=/path/to/toolchain
make
make run                 # VGA with sample hard disks, floppies and an SD card
make run MODE=text
make run MODE=cga
make run MODE=ega
make run MODE=vga
```

Override the location or emulator when needed:

```sh
make DOS_TOOLCHAIN=/path/to/buildenv
make run MODE=vga DOSBOX=dosbox-x
```

The executable is **`build/NW.EXE`**. Compile logs and a linker map also
appear in `build/`. Sources are staged there with DOS line endings and DOS
filenames. The source directory and toolchain are not modified by
compilation. `make clean` removes generated build artifacts.

`make run` mounts `build/` as DOS C: and starts with D: (work) and E:
(backup/transfer) in the two panes. Press F9 to choose another drive:

| Drive | Demo media | Persistent storage |
| --- | --- | --- |
| A: | 3.5-inch floppy, 1.44 MB | `build/demo/A.IMG` |
| B: | 5.25-inch floppy, 360 KB | `build/demo/B.IMG` |
| D: | Work disk | `build/demo/D/` |
| E: | Backup disk | `build/demo/E/` |
| S: | Simulated SD card | `build/demo/S/` |

The floppies are FAT12 images with their corresponding capacities. A demo-only
launcher supplies floppy metadata and simulates Slot-otter discovery for S:,
so the picker shows the matching icons. It runs the unchanged `NW.EXE` and
requires no SD hardware or custom driver.

The samples include documents, project folders, images, ZIP and ARJ archives.
Existing files are preserved on later launches. Missing directory samples are
recreated; existing floppy images are left intact. To reset a floppy, delete its
`.IMG` file before the next launch. `build/demo/A/` and `B/` contain only the
initial seed files; edit floppy contents through DOSBox or mtools.
`make demo` prepares the drives and launcher without opening an interactive
session. It needs DOSBox and mtools (`mformat` and `mcopy`). Everything is
generated under `build/` and removed by `make clean`.

To work with other files, mount their directory in DOSBox and invoke the
executable manually. For example:

```dos
mount c /path/to/nightwatch/build
mount d /path/to/files
c:
NW /vga D:\SOURCE D:\DEST
```

Use `NW /?` for startup syntax. Optional arguments give the left and right
pane directories. Without them both panes start in the current directory. On DOS
hardware, copy `NW.EXE`, `FONTLIC.TXT` and `ICONLIC.TXT` to a disk and run it directly. The
license file accompanies redistributed font data; the executable needs no
external font or icon files.

## Drive picker

Press **F9** to list configured drives. The current drive is highlighted. Use
Up/Down or a drive letter to select one and press Enter to open its root in the
active pane. Left/Right move between columns when many drives are present;
Home/End select the first entry or the final **Enter a path...** option. Escape
leaves both panes and marks unchanged. Choose **Enter a path...**, or press **.**,
to retain direct directory-path entry, including when leaving an archive.

The list uses DOS metadata rather than opening every drive. Type information is
used when DOS and its driver support it; unsupported queries retain a generic
type/icon. VGA uses distinct 16x16 icons for 3.5-inch and 5.25-inch floppy drives,
fixed disks and Slot-otter SD. Other modes show the same textual descriptions.
A configured drive can be empty or unavailable; filesystem access happens on
selection, with DOS critical errors returned to Nightwatch rather than the DOS
Abort/Retry prompt. An unsuccessful canonicalization leaves the pane unchanged.

OTTERSD is recognised before the generic remote-drive fallback, using its private
INT 2Fh AX=D74Fh discovery signature and returned drive number. The picker does
not assume an S: drive. Discovery runs once per opening; OTTERSD checks its media
presence signal and may take a removed card offline. Nightwatch does not issue
mount/unmount controls. An offline resident is labelled accordingly.

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
| F9 | Pick a drive for the active pane; choose "Enter a path..." or press `.` to type a folder path |
| F10 | Quit and restore the original video mode, page and cursor |
| Ctrl-R | Refresh both panes |
| Ctrl-F | Find the next filename matching a pattern (`*`, `?`), wrapping within the pane |
| Ctrl-P / Ctrl-N | Mark / unmark entries matching a filename pattern |
| Ctrl-S | Sort active pane: name, size, date/time, extension, or reverse |
| Ctrl-T | Storage integrity tests: quick / long / verify / cleanup |
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
containing directories. Copies verify the transferred length against the
source's size before committing; unexpected EOF or a source-size change leaves
the source and any existing destination intact. Replacement files are fully
prepared before the original is moved to a temporary backup; a failed commit attempts to restore the
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
During batches of small files, ordinary progress updates are coalesced to one
paint per four BIOS ticks. First/final updates and restoration after a prompt
are immediate. Keys typed during copying are deferred for pane navigation and
cannot answer a later overwrite prompt.
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

In graphics modes, the text viewer, hex viewer and About screen use the same
window style as Help: a blue title bar, gray frame and white document area.
The viewer keeps the file path and navigation controls visible while paging.
Text wraps to the document width; hex view retains 16 bytes per row, grouped
in pairs. Line counts and byte offsets appear below the document.

Both viewers offer **F7** search, **F8** next match and **Ctrl-G** byte-offset
jump. Search accepts case-insensitive literal ASCII or exact binary patterns
such as `hex:DE AD 00 BE EF` (up to 63 bytes). Jumps accept decimal or `0x`
hexadecimal. Matches appear at the top of the view. Text supports Home/End and
backward paging beyond its 128 cached offsets by rescanning earlier content when
necessary.

The text viewer shows **Line X / Y**, where X is the first displayed
file line and Y is the total. Wrapped rows still belong to the same file line;
an empty file shows 0 / 0. The initial count scans the file, so opening large
files on slow disks can take time. Hex mode shows the byte offset and file size
in its footer.

Press **F2**, then **R**, to enable or disable remembering the display mode
(default: off). The F2 title shows the current setting. Enabling this option
writes the current mode to `NIGHT.CFG`; subsequent successful mode selections
are saved immediately, including text mode. Pane paths and sorting are saved
only on **Ctrl-W**. Disabling the option keeps the last saved startup mode.
Subsequent launches from the same startup directory load `NIGHT.CFG`; explicit
mode and pane arguments override saved values. When remembering is enabled,
an explicit mode becomes the new saved mode. Existing configuration files
remain compatible. Save failures are reported and leave existing settings intact
when rollback succeeds. Missing saved directories fall back to the startup
directory, and an unavailable saved display mode falls back to text. Corrupt, oversized or
incomplete settings records are ignored as a whole.

The shell and editor run in the active pane's directory, with the original text
mode restored. After a command, press a key to return. An interactive shell
returns on `EXIT`. The editor executable must be on DOS's PATH; `EDITOR` may
contain a command with options. A DOS `COMMAND.COM` must be available through
`COMSPEC`.

Storage integrity testing is available through **Ctrl-T**. Quick and long
profiles write deterministic data in an exclusive test folder, exercise file
and directory changes, and verify every byte. Local manifests and logs must be
on another drive. See the [storage test guide](storage-test.md) for profiles,
verification after reboot, cleanup rules and the OTTERSD zero-gap requirement.

### Everyday file tools

Enter runs a selected EXE, COM or BAT through DOS in the active pane's directory.
The original display and working directories are restored on return; both panes
are refreshed. Press any key after the child finishes. Files in an archive must
be extracted before execution. F3 still views executable bytes without running
anything.

The idle status shows free space on both pane drives (K means KiB) and selected
file bytes. A shared drive is shown once.
Folder contents are not scanned on every cursor movement. Ctrl-L calculates a
recursive selection total, with progress and Escape cancellation; the cached
result is invalidated by selection changes and directory refreshes. Unknown
free space is explicitly shown as unknown.

Ctrl-PgDn and Ctrl-PgUp page physical directory listings. Pages use DOS enumeration
order and hold up to 512 entries including the parent. The chosen sort is applied
within each page. [MORE] indicates a following page. Marks belong to the page;
changing pages with marks asks before clearing them. Refresh retains the page
offset; folder navigation starts at page one. Archive listings keep their existing
512-entry cap.

Alt-F7 searches beneath an existing directory or drive root using DOS filename
patterns. It streams enumeration beyond the pane cache, bounded by the same
32-level nesting and 127-byte path limits as recursive file operations. Matches
appear one at a time: N continues, Enter opens the containing directory and
selects the entry (including on a later directory page), and Escape stops.

Ctrl-A, or Alt-Enter when the BIOS supplies it, opens file properties. R/H/S/A
toggle Read-only, Hidden, System and Archive. Edits remain local until Enter;
Escape cancels. Existing directory and other attribute bits are retained.
Attribute editing is unavailable inside archives.

Path/dialog input and the command line support insertion, Left/Right, Home/End,
Delete, Backspace and Ctrl-Left/Right word movement. Tab completes local names;
a unique directory gains its separator, while ambiguous names extend to their
common prefix. Dialog Up/Down recalls recent entries. Command Ctrl-E/Ctrl-X
recalls previous/next commands; Up/Down also works while composing a command.
There are four recent entries per history, stored only for the current session.
Ctrl-U clears a dialog field; Escape clears the command line.

Conflict prompts show source and destination bytes and packed DOS date/time.
O overwrites once, A overwrites all remaining conflicts, S skips once and K skips
all remaining conflicts. The choice resets for each command and also applies to
archive extraction. Transfer errors offer retry, skip or cancel. File retry starts
again after partial staging cleanup; completed files remain committed. A skipped
move retains its source. Recovery files named by an error are retained when
rollback or cleanup fails. DOS critical errors during interactive operations
return to Nightwatch rather than opening an Abort/Retry prompt; the original
handler is restored while external programs run and on exit.

### Current boundaries

- DOS 3.0 or later; DOS 8.3 names and conventional memory only.
- A cache of 512 entries per physical-directory page, including `..`; directories
  sort before files within a page. `[MORE]` reports another page. Archive panes
  retain their 512-entry `[LIMIT]` boundary. Hidden and system files are
  included.
- Recursive directory copy/move/delete; same-drive moves prefer DOS rename.
  Existing directories merge, and files require an overwrite/skip decision.
- Keyboard operation; no mouse or network protocols. Filename
  patterns, sorting and viewer search are available through keyboard commands.
- Native text retains the DOS character map. Graphics fonts map CP437 bytes to
  embedded Spleen glyphs, with explicit fallback for glyphs missing in small
  sizes. High bytes in the text viewer still display as dots.
- The text viewer caches 128 page offsets and rescans for older pages. Both
  viewers stream files; text End and uncached backward paging may take time on
  large files or slow disks.
- Hex view shows eight-digit byte offsets, 16 hexadecimal bytes per row
  (grouped in pairs in graphics modes), and printable ASCII (other bytes appear as dots). PgDn/Space/Enter and
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
VGA uses original Quick Menu inspired 16 x 16 color icons. EGA retains
monochrome 16 x 16 icons from the 16pxls set. The existing
font sizes remain readable. A single white scanline separates VGA/EGA file
entries, including the selected entry.

| Mode | Entries per pane | Row pitch | Function-key strip height |
| --- | ---: | ---: | ---: |
| Text | 17 | One character row | One character row |
| CGA | 18 | 8 pixels | 10 pixels |
| EGA | 15 | 17 pixels | 14 pixels |
| VGA | 22 | 17 pixels | 18 pixels |

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
memory. There is no RAM framebuffer. Dialogs use a cell overlay renderer that redraws
changed spans. Graphical Help, About and viewers use native document windows
and return to the pixel desktop on close. Viewer pages repaint only when the
file position or display mode changes.
Labels rasterize in a 512-byte scratch buffer and batch color-plane writes
across scanlines. Graphical viewers use the same opaque rendering path.
Scrolling copies unchanged pixels using CGA bank-aware transfers or EGA/VGA
hardware latches. Pane summaries repaint only changed character spans; marks and
content revisions avoid directory scans and entry comparisons on ordinary input.
Text mode retains its cell buffer and updates affected rows instead of
rebuilding the desktop.

Two 512-entry pane caches occupy about 25 KB. The copy fallback buffer occupies
2 KB outside DGROUP, cell buffers total 9,600 bytes, and the runtime stack is 8 KB. Font/help
and fixed Huffman tables live in immutable segments outside DGROUP; there is
no duplicate font cache. The medium memory model permits separate code
segments while retaining near pointers for ordinary data. The ZIP-enabled
executable is about 161 KB. **Allow 256 KB free conventional memory**, including
roughly 40 KB of far allocations during inflation. Shells/editors need
additional memory. The build checks that near data plus the actual stack leave
at least 2 KB of the 64 KB data segment for stdio and other heap use.
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

## Browsing ZIP and ARJ archives

Press **Enter** on a `.ZIP` or `.ARJ` file to open its contents in the active
pane. The path caption includes the archive filename and current member folder.
Archive browsing uses the same DOS 8.3 and format limits as extraction. Listing
validates archive headers and names without decompressing payloads or creating
files. Implicit directories are included, and the normal 512-entry pane limit,
sorting, marking, scrolling and filename search apply.

**Enter** opens a member folder. **Backspace** or **Enter** on `..` goes up;
at the archive root it returns to the containing disk directory and reselects
the archive. F9 can switch directly to an existing disk directory. Ctrl-R
refreshes the archive listing; Ctrl-W saves the containing disk paths.

**F3 / Shift-F3** views the cursor member as text/hex. Only that member is
streamed into an exclusively created temporary file in the launch directory,
with size/CRC checks before opening the viewer; the temporary is removed when
the viewer closes. The launch directory must therefore be writable for viewing.
The viewer caption retains the virtual archive/member path.

**F5** (or Ctrl-U inside an archive) extracts the cursor entry or marked entries
into an existing disk directory, defaulting to the inactive pane. A selected
folder includes all its descendants, including entries beyond the pane cache.
The existing overwrite/skip/cancel controls and staged replacement apply.

Archive panes are read-only: editor, move, mkdir, delete, shell and storage-test
commands are blocked there, as are transfers into a virtual archive pane. Source
archives are never rewritten. Extract a nested archive with F5 before opening it;
nested archive browsing and writing archive contents are not supported.

## ZIP and ARJ extraction

Select a ZIP or ARJ file and press **Ctrl-U**. The destination defaults to the inactive
pane and must be an existing directory. The operation validates the complete
archive structure and paths before creating files. Existing files offer
**O** overwrite, **S** skip or **Esc** cancel. The transfer dialog shows the
current uncompressed bytes, current-file and overall bars, speed and time left.
A corrupt or cancelled entry cannot replace its destination; earlier completed
entries and created directories remain.

For unattended extraction, use `NW /unzip D:\PACK.ZIP D:\OUT`. This command
refuses existing destination files and returns a nonzero status on failure.
Relative archive filenames are also accepted. Standard stored/DEFLATE entries
work at all compression levels, with ASCII DOS 8.3 names. Long/Unicode names,
ZIP64, encryption, split archives, Deflate64 and filesystem links are rejected.
Paths must fit the existing 127-byte program limit. Total uncompressed size and
archive offsets are limited to 2,147,483,647 bytes.

For ARJ, use `NW /unarj D:\PACK.ARJ D:\OUT` for unattended extraction.
Built-in decoding supports stored files and methods 1–4, directories and empty
files. ARJ text-mode entries, encryption, split volumes, chapters, extended
headers and self-extracting executables are rejected. Unix producer timestamps
are converted to DOS UTC. See [ARJ support](arj-support.md) for details.

## Validation

ZIP extraction is tested with retained archives actually produced by Windows
PowerShell/.NET and Linux Info-ZIP, plus generated compression and malformed
archive cases. See [ZIP support](zip-support.md) for formats, memory use,
compression details, fixture provenance and recovery behavior.

```sh
make test         # strict C89 host backend tests with address/undefined sanitizers
make test-browse-dos # archive browser and navigator integration on DOS/8086
make test-arj-dos # ARJ methods 0–4 on the 8086 and FAT12
make test-zip-dos # ZIP extraction on the 8086 and FAT12, Windows/Linux archives
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
Additional tests cover premature EOF with both transfer buffers, source growth,
copy/move source retention, exact VGA/EGA/CGA icon masks, and the number of
metadata lookups performed during recursive counting.
LeakSanitizer is disabled because the sandbox runs processes under tracing;
address and undefined-behavior instrumentation remain enabled.

DOS integration tests exercise byte-exact copying, moving, timestamps,
attributes, root-directory lookup, overwrite recovery, cancellation, recursive
operations, cross-drive moves and traversal at the 32-level stack bound. They
use DOSBox-X because DOSBox 0.74 does not implement attribute setting
faithfully. Deterministic adapter unit tests also inject DOS listing errors, distinguishing
normal exhaustion and the pane limit from partial failures. A scan that cannot
start preserves the valid cached listing; a failed partial scan clears its
entries, marks and selection and invalidates rendering.

Video tests include file-type icon fixtures and compile a separate test
executable with scripted key input; the production executable still reads BIOS
keyboard events. They run the actual UI and backend against each emulated
adapter, capture real video memory, decode it to PNG, and check visible output,
selection movement, viewer paging, copy, mkdir, delete, mode selection and exit.
Captures appear in `build/video/`. These tests run without an X server. Each
display mode also exercises overwrite input isolation,
timed progress throttling, forced completion and Escape polling between paints.
Text captures preserve and verify actual CP437 cells; their image previews substitute
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

Nightwatch is licensed under GPL-3.0-only; see [LICENSE](../LICENSE).
Spleen font data remains BSD-2-Clause. The About window (Alt-F1 or F1 then A)
shows the code, font and 16pxls icon credits. `NW /version` or `NW --version` prints the version.
[VERSION](../VERSION) is authoritative; `tools/version.py` updates its DOS
header and README badge, and `--check` catches inconsistencies in CI. Release
ZIPs include the GPL text as LICENSE.TXT, font license as FONTLIC.TXT, icon attribution/license as ICONLIC.TXT, the zlib license as ZLIBLIC.TXT, and
VERSION.TXT.
