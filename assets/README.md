Fonts: Spleen 2.2.0 by Frederic Cambus, BSD-2-Clause. Vendored BDF sources
come from https://github.com/fcambus/spleen/tree/2.2.0; see fonts/LICENSE.

VGA uses original 16×16 Quick Menu inspired color artwork, authored in
`tools/icons_v130.py`; see [the source references and previews](icons-v130/README.md).
`make assets` compiles an opacity mask and four color planes for each sprite
into `src/ASSETS.H`. The VGA artwork is GPL-3.0-only.

The VGA F9 picker and pane headers use four original 16x16 drive icons from
`tools/drive_icons.py`: 3.5-inch floppy, 5.25-inch floppy, hard disk and SD card.
Their PNGs and [comparison preview](drive-icons/preview.png) are in
`drive-icons/`. `make assets` packs them into far FONTDATA in `src/FONT.ASM`,
preserving near-heap space. These icons are GPL-3.0-only.

EGA uses monochrome 16x16 icons from [16pxls](https://16pxls.com/)
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
Each EGA icon compiles to a single 32-byte mask. Outlines use black normally and white
when selected; the surrounding application theme retains its existing colors.

CGA retains its original GPL-3.0-only artwork at 16x8, including the P5 parent
badge. All eight definitions are preserved independently in icons/cga/*.txt,
using `X` for outline, `o` for body, `+` for detail and `.` for transparency,
so changing VGA or EGA artwork cannot change CGA. The old 12x12 drawings in
icons/compact/ are historical assets; they are no longer embedded or rendered.
The original alternatives remain in [the candidate sheet](../docs/icon-options.png).

Run `make assets` after editing a mask or upgrading a vendored font.
No download or external font/icon file is needed to build or run NW.EXE.
The generator emits src/ASSETS.H (icons), src/FONT.ASM (fonts and Help), and
src/HELP.H. Archive error messages in `archive-errors.txt` compile to
`src/ARCMSG.H` and `src/ARCMSG.ASM`, keeping their text outside DGROUP.
Fonts map DOS CP437 to Unicode; small fonts substitute light
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

## Quick Menu launcher icon

[quickmenu/NIGHT.ICC](quickmenu/NIGHT.ICC) is an original 32x32, 16-color EGA
launcher icon with a binary transparency mask. A 4-bit PNG and preview are
included beside it. See [the format and import notes](quickmenu/README.md).
Regenerate with `python3 tools/quickmenu_icon.py`. This icon is GPL-3.0-only.

## About artwork

EGA and VGA display a shared 144x176 image of the two foreground gentlemen
from *The Night Watch*, using the standard 16-color EGA palette. See
[the artwork and generation notes](about/README.md). `make assets` embeds
its four color planes in `src/FONT.ASM`, outside DGROUP.
