# SPDX-License-Identifier: GPL-3.0-only
"""Keep Turbo C near data, runtime stack and stdio heap within 64 KB."""
from pathlib import Path
import re
import argparse
root=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('map_path', nargs='?', type=Path, default=root/'build/NW.MAP')
parser.add_argument('--stack-source', type=Path, default=root/'src/MAIN.C',
                    help='source that defines the linked executable stack size')
args=parser.parse_args()
map_path=args.map_path
# Turbo Link reports byte-addressed segment starts/stops/lengths in hex.
# Use the data-group span to include intervening alignment/BSS, then round
# the total to a paragraph for the startup memory calculation.
segments={}
for line in map_path.read_text().splitlines():
    match=re.match(r'\s*([0-9A-F]+)H\s+([0-9A-F]+)H\s+([0-9A-F]+)H\s+(\S+)',line)
    if match: segments[match[4]]=tuple(int(match[i],16) for i in (1,2,3))
stack_match=re.search(r'unsigned _stklen\s*=\s*(\d+)\s*;', args.stack_source.read_text())
assert stack_match,'Cannot verify runtime stack size'
stack=int(stack_match[1])
# The map's tiny _STACK segment is expanded by Turbo C startup using _stklen.
static=segments['_BSSEND'][0]-segments['_DATA'][0]
static=(static+15)&~15
# The font segment is outside DGROUP. Keep at least 2 KB for stdio/runtime
# allocations after startup expands the stack, rather than merely fitting data.
heap=65536-static-stack
assert heap>=2048,f'Near-data overflow: {static} static + {stack} stack leaves only {heap} heap bytes'
print(f'PASS: near data: {static} static + {stack} stack; {heap} bytes available for near heap')
