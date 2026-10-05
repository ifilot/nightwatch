# SPDX-License-Identifier: GPL-3.0-only
"""Manage the shared X.Y.Z version, DOS header and README version badge."""
from pathlib import Path
import argparse
import re

ROOT = Path(__file__).resolve().parents[1]
PATTERN = r'(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)'


# VERSION is the single source of truth. Strict decimal components prevent
# ambiguous tags and keep the emitted C string and badge URL safe.
def read_version(path=ROOT / 'VERSION'):
    version = path.read_text(encoding='ascii').strip()
    if not re.fullmatch(PATTERN, version):
        raise ValueError('VERSION must contain X.Y.Z, without a v prefix or leading zeros')
    return version


def header(version):
    return ('/* SPDX-License-Identifier: GPL-3.0-only */\n'
            '/* Generated from VERSION by tools/version.py; edit VERSION, not this file. */\n'
            '/* Keep the plain version for metadata and the v-prefixed tag for display. */\n'
            '#ifndef VERSION_H\n#define VERSION_H\n'
            f'#define NW_VERSION "{version}"\n'
            '#define NW_VERSION_TAG "v" NW_VERSION\n#endif\n')


# Check mode is read-only for CI; write mode touches only changed metadata.
# The badge count check prevents a silently missed or duplicated version badge.
def synchronize(version, check=False):
    target = ROOT / 'src/VERSION.H'
    readme = ROOT / 'README.md'
    badge = f'https://img.shields.io/badge/version-v{version}-blue'
    text = readme.read_text()
    updated, count = re.subn(r'https://img\.shields\.io/badge/version-v' + PATTERN + '-blue', badge, text)
    if count != 1:
        raise ValueError('README must contain exactly one version badge')
    generated = header(version)
    if check:
        if not target.exists() or target.read_text() != generated or updated != text:
            raise ValueError('Version header/badge out of sync; run python3 tools/version.py --write')
    else:
        if not target.exists() or target.read_text() != generated:
            target.write_text(generated, encoding='ascii')
        if updated != text:
            readme.write_text(updated)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_mutually_exclusive_group()
    commands.add_argument('--set', dest='new_version', help='bump to X.Y.Z or vX.Y.Z')
    commands.add_argument('--write', action='store_true', help='update header and README badge')
    commands.add_argument('--check', action='store_true', help='verify committed version files')
    args = parser.parse_args()
    # --set writes VERSION first; a later metadata error can leave it ahead of
    # the header/badge. Fix the error and use --write to synchronize again.
    if args.new_version:
        version = args.new_version.removeprefix('v')
        if not re.fullmatch(PATTERN, version):
            parser.error('version must be X.Y.Z or vX.Y.Z, without leading zeros')
        (ROOT / 'VERSION').write_text(version + '\n', encoding='ascii')
    version = read_version()
    if args.new_version or args.write or args.check:
        synchronize(version, check=args.check)
    print('v' + version)
