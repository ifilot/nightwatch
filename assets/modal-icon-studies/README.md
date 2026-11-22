# VGA modal icons

Ten original 16×16 icons for Help, About, Search, Properties, Copy, Move,
Delete, Warning, Storage test and Display settings. The approved designs are
embedded in VGA title bars.
EGA and CGA retain their existing artwork.

See the [preview sheet](../../docs/modal-icon-proposals.png) for enlarged
blue/white background samples and native-size title-bar samples. About uses
a blue information badge; Delete shows paper feeding through a shredder.

Each PNG has binary transparency and uses the application's 16-color palette.
Artwork and generator are GPL-3.0-only. Regenerate with
`python3 tools/modal_icon_studies.py` (Python 3 and Pillow).

The [Properties and Display comparison](../../docs/modal-icon-alternatives.png)
records the selected designs, **P3 (Properties)** and **D2 (Display)**, alongside
the original checklist (P1), attribute sliders (P2), plain monitor (D1), and
screen size arrows (D3). Regenerate with `python3 tools/modal_icon_alternatives.py`.

The main preview and canonical `properties.png` / `display-settings.png` use
the selected document-and-wrench and color-bar monitor designs.
