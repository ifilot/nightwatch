# Source comment style

Use the plain style supported by the original tools: `/* ... */` in Turbo C
2.0/C89 files and `;` in TASM assembly. Keep DOS source comments ASCII, with
lines around 78 columns. Use `#` for host scripts and makefiles, and docstrings
for Python module/function contracts. Modern annotation tags or a documentation
preprocessor are unnecessary for this project.

A module comment explains its responsibility and memory/hardware constraints.
A function comment explains purpose, inputs that need special care, outputs,
return conventions, side effects and failure behavior. Short helpers need only
a sentence; complex routines need their invariants explained where they matter.
Use ` * ` on continued C comment lines. Do not repeat a whole public contract
beside every implementation: headers describe what callers need; implementations
explain how and why. Document limits and partial failure accurately, rather than
promise rollback or cleanup that the code cannot guarantee.

Comments should explain decisions, not translate each statement into English.
Explain DOS packed timestamps, DTA search ownership, BIOS scan codes, register
settings, pixel/byte coordinates, segment ownership and cache invalidation.
Distinguish a pane's bounded directory cache from recursive enumeration. Mention
8088 performance tradeoffs where they determine an algorithm or buffering choice.
Avoid decorative banners, personal history and speculative assurances.

Generated `ASSETS.H`, `FONT.ASM` and `VERSION.H` take their comments from
`tools/assets.py` and `tools/version.py`. Edit those generators and regenerate;
never rely on comments added only to the generated files. Leave the vendored
font data and upstream license texts unchanged.

For documentation-only changes, verify C/assembly tokens and Python syntax trees
against the starting source, then build with Turbo C/TASM and run the host and
DOS integration tests. Comments can still break an old compiler, a source-staging
script or the test harness's source transformations.
