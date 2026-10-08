Fonts: Spleen 2.2.0 by Frederic Cambus, BSD-2-Clause. Vendored BDF sources
come from https://github.com/fcambus/spleen/tree/2.2.0; see fonts/LICENSE.

The icons are original, editable 16x16 pixel drawings. `X` is outline, `o` is
body, `+` is highlight, `.` is transparent. Their authoritative definitions
are in assets/icons/*.txt. Original 12x12 variants for compact EGA rows
are in assets/icons/compact/*.txt. CGA compresses the 16x16 masks vertically
to 16x8 during asset generation; VGA uses them at their original size.
The generated CGA masks match the original runtime conversion exactly, including
outline/body/highlight overlap precedence. tools/assets.py reads them and emits src/ASSETS.H
(icons) plus src/FONT.ASM (fonts).
Generated font tables map DOS CP437 to Unicode glyphs. Small fonts substitute
light borders where the upstream font lacks double borders. Missing glyphs
are explicitly represented by a question mark, rather than masking bit 7.

Run `make assets` after editing a bitmap or upgrading a vendored font.
No download or external font file is needed to build or run NIGHT.EXE.

`help.txt` contains the embedded Help document. Lines are limited to 66
characters so they fit every graphical viewport; `#` marks a regular-weight
section heading. `make assets` generates `src/HELP.H` and appends the document
and its line-offset table to the immutable font segment in `src/FONT.ASM`.
This keeps the documentation outside the small-model data segment.

Nightwatch code and the original icons are GPL-3.0-only (see ../LICENSE).
The vendored Spleen fonts and derived glyph data retain BSD-2-Clause licensing.

The panel-path drive icon is an original perspective hard-drive drawing inspired
by early Windows desktop icons, with a beveled case, front slot and activity
light. Its full, compact and monochrome variants use the same mask system.
