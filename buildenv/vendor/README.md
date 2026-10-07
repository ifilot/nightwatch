# DOSBox-X source

`dosbox-x-2024.03.01.tar.gz` contains the Linux build sources from DOSBox-X
2024.03.01, upstream commit `199aa35f34ea637cca901ada3a431939fcca13f2`:
https://github.com/joncampbell123/dosbox-x/tree/199aa35f34ea637cca901ada3a431939fcca13f2

The archive includes upstream top-level files, `src/`, `include/`,
`contrib/linux/`, the SDL compatibility sources in `vs/sdl/`, the save-state ZIP
sources in `vs/zlib/`, and `contrib/macos/dosbox-x.plist.in`. These files are
unmodified.
Ancillary Windows/macOS SDKs, media, website material and development experiments
are omitted. The SDL2 Linux build uses Ubuntu's library packages. No upstream
repository is fetched when building the container.

`SHA256SUMS` verifies the bundled archive. The upstream GPLv2 license is included
both in the archive and as `DOSBOX-X-COPYING`; component notices remain in their
source files. Nightwatch's GPLv3 license does not replace these notices.
