# Nightwatch icon for Quick Menu

Import **NIGHT.ICC** with Quick Menu's icon editor and assign it to the entry
that launches `NW.EXE`. This is a Quick Menu ICC icon, not a Windows ICO file.
No change to the executable is needed.

The release icon is the reference-informed two-pane design: bright cyan file
lists, strong black dividers, and a selected entry. It is drawn directly on the
final pixel grid. The DOS release archive includes `NIGHT.ICC` beside `NW.EXE`.
The original five options and the reference draft remain available for comparison.

- **32 x 32 pixels**, with no scaling or antialiasing.
- **16-entry standard EGA palette**, using six opaque colors.
- **Binary transparency**; no partially transparent pixels.
- **NIGHT.PNG** is an actual **4-bit indexed PNG** with palette transparency.
- **NIGHT.ICC** is exactly **1,032 bytes**: two 516-byte bitmap records.
  Each record starts with little-endian width-minus-one and height-minus-one
  (`1F 00 1F 00`), followed by 32 rows of four 4-byte color planes.
  The second record is the transparency mask: index 0 for transparent pixels
  and index 15 for opaque pixels. ICC color-plane order differs from ordinary
  EGA palette-index bit order.
- **preview.png** shows the same pixels at 6x and 1x on blue, gray, and black.
  It is a preview sheet, not an importable icon.

Format and plane ordering were checked against
[Quick Menu Icon Creator](https://github.com/ifilot/quickmenu-icon-creator),
including a byte-for-byte comparison with its converter after normalizing
invisible RGB pixels to black. Its display palette uses approximate RGB shades;
the PNG and preview here use standard EGA RGB values. ICC stores color indices,
not RGB values. Import into actual Quick Menu has not yet been tested.

Regenerate from the repository root:

```sh
python3 tools/quickmenu_icon.py
```

The generator checks dimensions, PNG bit depth, EGA colors, transparency,
ICC headers and length, and round-trips both ICC bitmaps.

This icon and its generator are original Nightwatch assets under GPL-3.0-only.
They do not incorporate the third-party 16pxls icons used inside Nightwatch.

Five alternative directions are available in [options/](options/README.md),
with a comparison sheet and individual PNG/ICC exports. Generate them with
`python3 tools/quickmenu_options.py`.
