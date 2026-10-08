Fonts: Spleen 2.2.0 by Frederic Cambus, BSD-2-Clause. Vendored BDF sources
come from https://github.com/fcambus/spleen/tree/2.2.0; see fonts/LICENSE.

VGA and EGA share the same monochrome 16x16 icons from [16pxls](https://16pxls.com/)
v1.0.1 by Paul Mackenzie, licensed CC-BY-SA-4.0. Selected original PNGs and
provenance are in [16pxls/](16pxls/README.md); attribution and the full license
are in [ICON-LICENSE.txt](ICON-LICENSE.txt). The DOS package includes these
notices as ICONLIC.TXT, and About credits the author.

| Nightwatch icon | Original 16pxls symbol |
| --- | --- |
| Folder | Folder |
| Parent directory | File-Upload |
| Document | File-Text |
| Binary | Menu-Four |
| Program | Browser |
| Archive | Archive |
| Image | PhotoSet |
| Drive | Server |

Run `python3 tools/import_16pxls.py` to convert the selected PNGs. Conversion
thresholds their original alpha at 128, without resizing or redrawing. The
native monochrome masks in icons/*.txt use `X` for ink and `.` for transparency.
Each VGA/EGA icon compiles to a single 32-byte mask. Outlines use black normally and white
when selected; the surrounding application theme retains its existing colors.

CGA retains its original GPL-3.0-only artwork at 16x8, including the P5 parent
badge. All eight definitions are preserved independently in icons/cga/*.txt,
using `X` for outline, `o` for body, `+` for detail and `.` for transparency,
so changing VGA/EGA artwork cannot change CGA. The old 12x12 drawings in
icons/compact/ are historical assets; they are no longer embedded or rendered.
The original alternatives remain in [the candidate sheet](../docs/icon-options.png).

Run `make assets` after editing a mask or upgrading a vendored font.
No download or external font/icon file is needed to build or run NW.EXE.
The generator emits src/ASSETS.H (icons), src/FONT.ASM (fonts and Help), and
src/HELP.H. Fonts map DOS CP437 to Unicode; small fonts substitute light
borders where upstream lacks double borders. Missing glyphs show a question mark.

See [the icon overview](../docs/icon-overview.png) for every current icon in
VGA/EGA/CGA, normal and selected states, Help's drive and selection marks.
Run `make icons-overview` to refresh it from the generated masks.

`help.txt` contains the embedded Help document. Lines are limited to 66
characters; `#` marks a regular-weight heading. Its text and offset table live
in the immutable font segment outside the small-model data segment.

Nightwatch code and the original CGA icons are GPL-3.0-only (see ../LICENSE).
The 16pxls artwork and bitmap adaptations retain CC-BY-SA-4.0. Spleen fonts
and derived glyph data retain BSD-2-Clause.
