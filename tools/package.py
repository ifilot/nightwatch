# SPDX-License-Identifier: GPL-3.0-only
"""Package the DOS executable, version and required licenses."""
from pathlib import Path
import argparse
import hashlib
import zipfile
from version import read_version

ROOT = Path(__file__).resolve().parents[1]
DOS_README = """Nightwatch {version} - two-pane file navigator for MS-DOS 3.0+, 8088/8086.

Run NIGHT /text, /cga, /ega or /vga.
Optional pane paths: NIGHT /vga C:\\FILES D:\\BACKUP
F1: help (A: About). Alt-F1: About. NIGHT /version prints the version.
F2: display mode. F3: text viewer. Shift-F3: hex viewer.
F5: copy. F6: move. F7: mkdir. F8: delete. F10: quit.
Ctrl-W saves mode, pane paths and sorting in NIGHT.CFG.

Allow 128 KB free conventional RAM. EGA needs 128 KB video RAM.
All fonts/icons are embedded. Keep FONTLIC.TXT and ICONLIC.TXT with the
executable. Spleen fonts use BSD-2-Clause. The VGA/EGA icons are 16pxls by
Paul Mackenzie (https://16pxls.com/), adapted under CC-BY-SA-4.0.

Nightwatch is free software under GNU GPL version 3, with no warranty.
See LICENSE.TXT for the full license. Corresponding source is available
in the project repository at the matching release tag.
"""


# Package only the explicitly listed runtime assets, never the mounted compiler
# or logs. Stable ZIP timestamps/member order make repeat packaging reproducible.
# Checksums cover the exact standalone assets and the complete DOS archive.
# Output writes are not transactional. Publish only after successful completion;
# an I/O failure can leave partial output that must be regenerated.
def package(build, output):
    executable = (build / 'NIGHT.EXE').read_bytes()
    license_data = (build / 'FONTLIC.TXT').read_bytes()
    icon_license = (build / 'ICONLIC.TXT').read_bytes()
    gpl = (build / 'LICENSE.TXT').read_bytes()
    version = 'v' + read_version()
    if len(executable) < 64 or executable[:2] != b'MZ':
        raise ValueError('NIGHT.EXE is not a DOS MZ executable')
    if not license_data.strip() or not icon_license.strip() or not gpl.strip():
        raise ValueError('A required license file is empty')
    output.mkdir(parents=True, exist_ok=True)
    members = {'NIGHT.EXE': executable, 'FONTLIC.TXT': license_data, 'ICONLIC.TXT': icon_license,
               'LICENSE.TXT': gpl, 'VERSION.TXT': (version + '\r\n').encode('ascii'),
               'README.TXT': DOS_README.format(version=version).replace('\n', '\r\n').encode('ascii')}
    for name in ('NIGHT.EXE', 'FONTLIC.TXT', 'ICONLIC.TXT', 'LICENSE.TXT', 'VERSION.TXT'):
        (output / name).write_bytes(members[name])
    archive = output / 'NIGHTWATCH-DOS.zip'
    with zipfile.ZipFile(archive, 'w') as zipped:
        for name, data in members.items():
            entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            zipped.writestr(entry, data)
    names = ('NIGHT.EXE', 'FONTLIC.TXT', 'ICONLIC.TXT', 'LICENSE.TXT', 'VERSION.TXT', archive.name)
    (output / 'SHA256SUMS.txt').write_text(''.join(
        hashlib.sha256((output / name).read_bytes()).hexdigest() + '  ' + name + '\n'
        for name in names), encoding='ascii')
    return archive


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=ROOT / 'build')
    parser.add_argument('--output-dir', type=Path, default=ROOT / 'build/dist')
    args = parser.parse_args()
    print(package(args.build_dir, args.output_dir))
