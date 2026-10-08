# Rendering measurements

Measured on 2026-10-05 with DOSBox-X 2024.03.01, its normal 8086 core,
3,000 fixed cycles, a 640-pixel display, and Turbo C 2.0 (`-1- -ms -O -Z`).
The same fixture contains a parent entry, one directory and twelve files in
both panes. Mode selection/font caching happens before the timed first draw.

Times are BIOS ticks (approximately 54.9 ms). One-tick differences in these
small samples are timer phase/quantization, not evidence of a stable speed change.
The table below records the first graphics redesign, before the compact layout.
That intermediate layout used fewer, more generously spaced rows than text.
These timings compare complete interfaces, not an equal-glyph microbenchmark.

| Operation | CGA old/new | EGA old/new | VGA old/new |
| --- | --- | --- | --- |
| Initial screen | 6 / 5 | 39 / 10 | 53 / 15 |
| Ten alternating selection moves | 23 / 4 | 49 / 8 | 55 / 10 |
| Ten unchanged redraws | 18 / 0–1 | 19 / 0–1 | 20 / 0–1 |

EGA/VGA initial drawing is roughly 3.5–4 times faster; selection movement is
roughly 5–6 times faster. CGA movement is roughly 6 times faster. All three
new modes perform **zero video-memory writes on unchanged redraws**.

`make benchmark` generates a DOS benchmark executable from the current sources,
runs these three modes, and reports elapsed ticks plus CPU video-memory stores.
It asserts that idle draws perform no stores and that ten selection moves write
less than one complete initial screen. It does not silently raise the cycle
count or switch to a faster CPU for the new renderer.

The old implementation was staged before editing for the baseline measurement.
Its source hashes are:

- `MAIN.C`: `d5dfe45d0e2a37f30763675b1702118b2f6060c3ca6a5808f341de3f39e094e0`
- `VIDEO.C`: `10adc2e7664131885d19c4864174950dda447559e9ecae0d3b7e4cf67e359b4a`

The original before-change source snapshot and DOS logs remain in
`build/bench-old/` for this workspace. They are generated artifacts and are not
part of the source tree. Current results are in `build/bench-new/TIME*.LOG`.

Graphics captures in `build/video/` come from the emulator's actual bitplanes,
not a host-rendered mockup. The text captures retain CP437 character and attribute
bytes; previews use the IBM ROM's lower half and Spleen substitutes for its upper
half. Tests explicitly verify CP437 corner codes and separate footer attributes.
These checks cover interface output and instruction compatibility, not physical
5150 bus timing or CRT behavior.

## Compact layout

The current default layout displays 18 CGA, 16 EGA and 24 VGA entries per pane,
versus 17 in text. Fonts retain their original sizes; EGA and VGA share 16x16
monochrome 16pxls icons. EGA rows are 16 pixels high, with taller path captions. The title and permanent help strips were removed, the footer uses one
line, and operation status shares the command field when no command is typed.

Before EGA adopted 16x16 icons, the compact layout measured on the same 8086 benchmark:

| Operation | CGA | EGA | VGA |
| --- | ---: | ---: | ---: |
| Initial screen | 6 | 11 | 15 |
| Ten selection moves | 5 | 8 | 9 |
| Ten unchanged redraws | 0 | 0 | 0 |

The fixture still contains fourteen entries per pane. It does not fill the new
lists, so these measurements should not be extrapolated to a fully populated
initial screen. Idle redraws still write zero bytes to video memory.

`make test-video` also exercises forty-file directories. It checks the exact
scroll boundary, marking, pane switching, Home/End, bottom-row selection,
proportional scrollbar endpoints, and long command clipping using DOS state
captures and actual VRAM screenshots.

## Speed audit implementation

The speed changes keep the compact layouts and fonts. Desktop labels batch color
planes across scanlines using a 512-byte raster scratch buffer. Viewers use the
same opaque renderer and suppress reads/redraws when a key changes nothing.
CGA icons are compiled to their final masks. Text keeps its cell buffer; pane
revision fields and incremental mark counts avoid rebuilding unchanged content.
Summary text updates only changed character spans. Viewport scrolling copies
existing pixels, with a forced-redraw diagnostic alternative for comparison.
Production builds omit diagnostic store and cell counters.

The original fourteen-entry fixture now measures:

| Operation | CGA before/after | EGA before/after | VGA before/after |
| --- | ---: | ---: | ---: |
| Initial screen | 6 / 6 | 11 / 9 | 15 / 12 |
| Ten cursor moves | 5 / 3 | 8 / 5 | 9 / 6 |
| Ten unchanged redraws | 0 / 0 | 0 / 0 | 0 / 0 |

A separate identical-workload benchmark fills both pane caches with 512 synthetic
entries. Rendering, fonts, CPU core and cycle count are the same for both builds;
directory enumeration is outside these timings. Values are BIOS ticks:

| Workload | Text before/after | CGA before/after | EGA before/after | VGA before/after |
| --- | ---: | ---: | ---: | ---: |
| 40 alternating cursor moves | 88 / 11 | 17 / 10 | 28 / 15 | 33 / 19 |
| 20 one-row viewport scrolls | 44 / 15 | 51 / 10 | 94 / 36 | 129 / 49 |
| 20 command characters | 44 / 4 | 4 / 2 | 5 / 2 | 5 / 3 |
| Open hex viewer, five no-op keys, close | 11 / 2 | 69 / 8 | 127 / 11 | 195 / 17 |
| 20 unchanged desktop draws | 44 / 4 | 2 / 0 | 2 / 0 | 2 / 0 |

The last row measures synthetic repeated calls: normal operation blocks in BIOS
keyboard input and does not busy-spin while idle. The viewer row includes the
initial render; regression checks separately prove zero extra reads and stores
for the five no-op keys. Diagnostic counters are enabled in both timing builds
(the baseline already counted stores), so these are not claims about exact
production or physical 5150 timings.

With the other optimizations held constant, 20 viewport scrolls measured
42/81/111 ticks for forced CGA/EGA/VGA redrawing versus 10/36/49 for copying.
The copying path was retained only after this comparison and pixel-equivalence
checks covering both panes, scroll directions, selection and marks.

`make benchmark-speed` reproduces these workloads with a preserved source
baseline in `build/speed-before/src/`. Result logs live in
`build/speed-before-bench/`, `build/speed-after-bench/` and
`build/speed-no-scroll-bench/`. One-tick differences remain clock-phase noise.

The build's memory guard checks static data and the real 8192-byte runtime stack
against the 64 KB small-model segment. The guard reserves at least 2048 bytes for
the near heap; fonts still live in their separate segment. Recursive operations
use small traversal frames with a 32-level bound; the transfer buffer is now
2048 bytes to accommodate the larger stack and additional commands. The hex
viewer keeps its 432-byte page buffer on the stack; viewing and recursive
transfers never nest. This leaves near-heap room for the v1.1.0 copy progress
state and text.

The byte-exact comparisons against the earlier speed-only build are optional
(`NW_COMPARE_OLD_VRAM=1`), because help, operation dialogs and viewer footers
now intentionally include new commands. Copy-versus-redraw comparisons and
no-op viewer checks remain mandatory. `tests/features.py` exercises the new
commands in text, CGA, EGA and VGA, including settings reload and recursive
copy/delete.
