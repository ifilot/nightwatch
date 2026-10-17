# Nightwatch 1.3.0 icon studies

Two original 16×16 directions for all eight file-browser icon categories:
`windows-*.png` and `quickmenu-*.png`. The Quick Menu set is now embedded for
VGA; the Windows set remains a comparison study.
VGA is the primary design target; EGA and CGA adaptations are secondary.
The confirmed VGA size is **16×16**, preserving the current file-list density.
The Quick Menu direction has been redrawn against the extracted references;
see [its focused preview](quickmenu-preview.png) at native size and 4×.
See [the comparison](comparison.png) and [development scope](../../docs/v1.3.0.md).

Regenerate with `python3 tools/icons_v130.py` (Python 3 and Pillow).
Pixels use the standard EGA palette with binary transparency and no antialiasing.
All artwork and the generator are GPL-3.0-only, with no copied historical pixels.

## Quick Menu source references

The local `../quickmenu-icon-creator` repository contains extracted
`QM_COLOR.DEF` and `QM_MONO.DEF` libraries, with 100 icons each. Their native
size is 32×32. The source manifest records library and individual record hashes;
the color PNGs are in `extracted/color/` and the overview is
`extracted/color-contact-sheet.png`.

Useful references for the VGA redesign:

| Nightwatch category | Quick Menu references | Visual cues |
| --- | --- | --- |
| Folder | COLOR035 | Yellow body, black contour, restrained depth |
| Parent | COLOR066 | Clear upward arrow; adapt the symbol to a folder |
| Document | COLOR001, COLOR002 | White paper, fine gray rules, offset sheet or curl |
| Binary | COLOR028, COLOR061 | Disk/media silhouette and sparse identifying detail |
| Program | COLOR042, COLOR043, COLOR072 | Window frame or monitor, distinct working area |
| Archive | COLOR031, COLOR032 | Filing cabinet as a container metaphor |
| Image | COLOR015 | Readable landscape with a few strong color regions |
| Drive | COLOR063, COLOR064 | Shallow hardware perspective, gray bevels, small indicator |

These references suggest black one-pixel contours, large clear shapes, flat
color regions and selective white/gray highlights. Cyan belongs to particular
objects such as application windows, rather than every icon's details. The
Quick Menu study now follows these cues; the Windows study is retained for
comparison. The drawings are authored directly at 16×16, without resizing or
copying the extracted source bitmaps.

The VGA redesign keeps the 16×16 cell and current row count. The previews show
white and blue backgrounds. VGA now draws the Quick Menu artwork in full color,
with the same colors on selected rows and transparent pixels showing the row
background. EGA and CGA retain their existing artwork.
