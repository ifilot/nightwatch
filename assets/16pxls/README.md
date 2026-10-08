# 16pxls artwork

Selected original 16x16 PNGs from [16pxls](https://16pxls.com/) v1.0.1 by
Paul Mackenzie, source commit `dbf224f6f962ba59d6be8c429e53bb4518f057f3`.
The artwork and its bitmap adaptations use CC-BY-SA-4.0; see
[the attribution and license](../ICON-LICENSE.txt).

`mapping.json` maps Nightwatch icon names to the upstream symbols. Run
`python3 tools/import_16pxls.py` to convert these exact PNGs into native masks.
Conversion thresholds alpha at 128, without resizing, tracing or redrawing.
VGA and EGA use the same masks. CGA retains its previous native artwork.
