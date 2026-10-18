# About artwork

`gentlemen.png` is the embedded 144x176, 4-bit indexed image for EGA and VGA.
It uses only the standard IBM EGA palette (at most 16 colors). Text and CGA
modes do not display it. The image sits in its own gray inset beside the
white credits panel, framed by a dark 1px inner ring and a white 1px outer
ring. The two foreground figures are adapted from the
user's reference crop of Rembrandt's *The Night Watch* (1642).

`gentlemen-source.png` preserves the imagegen output. Built-in image generation
was used with the reference image and this prompt:

> Use case: style-transfer. Asset type: small DOS About dialog pixel artwork.
> Reference/edit target: attached painting crop. Adapt the TWO prominent
> foreground gentlemen from Rembrandt's Night Watch into faithful classic EGA
> 16-color pixel art: left captain in black with red sash and black hat
> extending his hand, right lieutenant in cream/gold uniform with wide cream
> hat, holding polearm. Preserve their identities, poses, clothing, left/right
> placement and full bodies and feet. Remove all other people. Both figures
> together fill a portrait composition with small margins, simple flat dark
> brown background. No text, no frame. Crisp deliberate coarse pixel art with
> readable faces and silhouettes at 144 x 176 pixels. Palette strictly classic
> IBM EGA 16 colors (black, dark blue, dark green, dark cyan, dark red, dark
> magenta, brown #AA5500, light gray, dark gray, bright blue, bright green,
> bright cyan, bright red, bright magenta, yellow #FFFF55, white), emphasizing
> brown, gray, black, white, red, yellow. No gradients, no antialiasing. Return
> a single image.

The build source was resized to 144x176 and quantized to the exact EGA
palette with Floyd-Steinberg dithering. `make assets` compiles its pixels
into four native color planes in `src/FONT.ASM`, outside DGROUP. No image
file or decoder is needed at runtime. The original adaptation is GPL-3.0-only.

## Dark gray background variant

The active `gentlemen.png` uses solid EGA dark gray (`#555555`) behind the
figures. `gentlemen-brown.png` preserves the earlier brown version for
comparison; `gentlemen-gray-source.png` preserves the built-in imagegen edit.
The edited output was resized to 144x176 and quantized to the standard EGA
palette without dithering, retaining a flat background.

Edit prompt:

> Use case: precise-object-edit. Edit target: the attached 144x176 DOS EGA pixel image of two gentlemen. Change ONLY the mottled brown/black background behind and between the two figures to a completely flat solid EGA dark gray #555555. Preserve the two gentlemen pixel-for-pixel: identical faces, hats, silhouettes, clothing, red sash, black uniform, gold/white uniform, weapons, hands and boots. Preserve original image dimensions 144x176, exact placement, scale and deliberate pixel grid. Do not redraw, enhance, soften, add detail or antialias the figures. Background must be exactly RGB(85,85,85), with no noise, texture, dithering, shadow, gradient or remaining brown/black speckles. Retain the original 16-color IBM EGA palette for the figures. No border, no text. Return the edited image.
