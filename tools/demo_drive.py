# SPDX-License-Identifier: GPL-3.0-only
"""Populate fictional DOS drives for make run, preserving existing demo files."""
from pathlib import Path
import argparse
import calendar
from io import BytesIO
import os
import random
import zipfile
from PIL import Image, ImageDraw
from vram import palette

ROOT = Path(__file__).resolve().parents[1]
STAMP = calendar.timegm((1993,7,16,12,30,0))


def text(value):
    return value.replace('\r\n','\n').replace('\n','\r\n').encode('ascii')


def populate(destination):
    def put(drive, name, content):
        path = destination/drive/name
        path.parent.mkdir(parents=True,exist_ok=True)
        # A later launch must retain edits, copied files and extraction results.
        try:
            with path.open('xb') as output:
                output.write(content)
        except FileExistsError:
            return
        os.utime(path,(STAMP,STAMP))

    directories = {
        'D': ('APPS','ARCHIVES','BACKUP','DATA','DOCS','DOS','GAMES','IMAGES',
              'MUSIC','PROJECTS','TEMP','TOOLS'),
        'E': ('ARCHIVES','DOCS','RESTORE','TRANSFER'),
    }
    for drive,names in directories.items():
        for name in names:
            (destination/drive/name).mkdir(parents=True,exist_ok=True)

    readme = text('''NIGHTWATCH DEMONSTRATION DRIVE

This fictional DOS workstation contains sample files for exploring Nightwatch.
D: is the work drive; E: is a backup and transfer drive.

Try F3 for text, Shift-F3 for hex, and F5 to copy into the other pane.
Select PACK.ZIP or PACK.ARJ and press Ctrl-U to unpack into E:.
F2 changes display mode. F10 exits.

All files under these demo drives are generated samples. Existing demo files
are preserved on subsequent launches. Missing sample files are recreated.
''')
    plan = text('''FIELD OFFICE WORKSTATION - JULY 1993

[ ] Copy the monthly report to the backup drive.
[ ] Review the equipment inventory in DATA.
[ ] Unpack the incoming archive into TRANSFER.
[ ] File the landscape illustration under IMAGES.
''')
    files = {
        'README.TXT': readme,
        'TODO.TXT': plan,
        'REPORT.TXT': text(('NORTH RIDGE FIELD OFFICE\nMonthly equipment and supplies report\n\n'
                            'Desktops: 12   Printers: 3   Modems: 2\n\n')*35),
        'CONFIG.SYS': text('REM Fictional configuration sample; not used to boot DOS.\nFILES=30\nBUFFERS=20\n'),
        'VIEW.BAT': text('@echo off\necho Nightwatch demonstration workstation\ntype D:\\README.TXT\npause\n'),
        'SAMPLE.BIN': random.Random(130).randbytes(12288),
        'DOCS/MANUAL.TXT': text(('OFFICE PROCEDURES\n\nKeep work on D: and copies on E:.\n'
                                 'Use dated folders for reports.\n\n')*50),
        'DOCS/LETTER.TXT': text('Dear colleague,\n\nThe July delivery is ready for collection.\n\nNorth Ridge Office\n'),
        'DATA/SALES.CSV': text('MONTH,ORDERS,TOTAL\nAPR,23,1840\nMAY,31,2480\nJUN,28,2240\n'),
        'DATA/STOCK.DAT': bytes(range(256))*256,
        'DATA/EMPTY.DAT': b'',
        'DOS/STARTUP.BAT': text('@echo off\necho Fictional DOS startup example.\n'),
        'APPS/EDITOR/README.TXT': text('Fictional text editor installation folder.\n'),
        'APPS/SPREAD/README.TXT': text('Fictional spreadsheet installation folder.\n'),
        'TOOLS/CHECK.BAT': text('@echo off\nver\n'),
        'GAMES/MAZE/README.TXT': text('Fictional maze game installation folder.\n'),
        'GAMES/CHESS/README.TXT': text('Fictional chess game installation folder.\n'),
        'MUSIC/TRACKS.TXT': text('1. Evening Shift\n2. Neon Terminal\n3. Last Train\n'),
        'PROJECTS/TERMINAL/MAIN.C': text('/* Fictional terminal project. */\n#include <stdio.h>\nint main(void) { puts("Terminal ready"); return 0; }\n'),
        'PROJECTS/TERMINAL/MAKEFILE': text('TERMINAL.EXE: MAIN.C\n\ttcc MAIN.C\n'),
        'PROJECTS/INVENT/NOTES.TXT': plan,
        'BACKUP/README.TXT': text('Local backup staging folder. The second drive is E:.\n'),
    }
    for name,content in files.items():
        put('D',name,content)

    # Real, viewable images give file types meaningful content in the hex viewer.
    picture = Image.new('P',(320,200),11)
    picture.putpalette([channel for rgb in palette for channel in rgb]+[0]*(768-48))
    draw = ImageDraw.Draw(picture)
    draw.ellipse((244,20,275,51),fill=14)
    draw.polygon([(0,140),(85,55),(167,135),(242,80),(319,140),(319,199),(0,199)],fill=2)
    draw.polygon([(0,160),(105,125),(205,164),(319,131),(319,199),(0,199)],fill=10)
    draw.rectangle((195,132,258,185),fill=12)
    draw.polygon([(185,133),(226,102),(268,133)],fill=4)
    draw.rectangle((220,156,234,185),fill=6)
    for fmt,name in (('PCX','SCENERY.PCX'),('BMP','IMAGES/SCENERY.BMP')):
        output = BytesIO(); picture.save(output,format=fmt)
        put('D',name,output.getvalue())
    packet = BytesIO()
    with zipfile.ZipFile(packet,'w',compression=zipfile.ZIP_DEFLATED) as archive:
        for name,content in (('README.TXT',readme),('DOCS/PLAN.TXT',plan),('DATA/BYTES.BIN',bytes(range(256))*64)):
            info = zipfile.ZipInfo(name,(1993,7,16,12,30,0))
            info.compress_type = zipfile.ZIP_DEFLATED
            archive.writestr(info,content)
    put('D','PACK.ZIP',packet.getvalue())
    put('D','PACK.ARJ',(ROOT/'tests/fixtures/arj/METHOD1.ARJ').read_bytes())
    put('D','ARCHIVES/JULY.ZIP',packet.getvalue())
    put('E','README.TXT',text('BACKUP AND TRANSFER DRIVE\n\nUse this drive as the destination for copies and archive extraction.\n'))
    put('E','DOCS/JUNE.TXT',text('JUNE ARCHIVE\n\nPrevious month: 28 orders, total 2240.\n'))
    put('E','ARCHIVES/JUNE.ZIP',packet.getvalue())
    put('E','RESTORE/NOTES.TXT',text('Restore staging area.\n'))
    put('E','TRANSFER/INBOX.TXT',text('Incoming files can be unpacked into this folder.\n'))
    print(f'Demo drives ready: {destination / "D"} and {destination / "E"}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT/'build/demo')
    populate(parser.parse_args().root)
