#!/usr/bin/env python3
"""check_natives.py - v2.01.1: every native hash in the GTA plugin must match its name in the native database
(alloc8or/gta5-nativedb-data, natives.json). A wrong hash makes Script Hook V close GTA ("FATAL: Can't find
native ..."), which is what v2.01's SET_TEXT_COLOUR did. Also writes native_hashes.txt (all known hashes) for
the fake Script Hook V, which ends the fake game on an unknown native the same way.
Usage: python3 check_natives.py ../src/GtaBeamD1.cpp [natives.json]
(natives.json: https://raw.githubusercontent.com/alloc8or/gta5-nativedb-data/master/natives.json)"""
import json, os, re, sys, urllib.request

src = sys.argv[1] if len(sys.argv) > 1 else '../src/GtaBeamD1.cpp'
db = sys.argv[2] if len(sys.argv) > 2 else 'natives.json'
if not os.path.exists(db):
    urllib.request.urlretrieve('https://raw.githubusercontent.com/alloc8or/gta5-nativedb-data/master/natives.json', db)
d = json.load(open(db))
byname, byhash = {}, {}
for ns, v in d.items():
    for h, n in v.items():
        H = h.upper()
        byhash[H] = n['name']
        for nm in [n['name']] + [o.lstrip('_') for o in (n.get('old_names') or [])]:
            byname.setdefault(nm, set()).add(H)
open('native_hashes.txt', 'w').write('\n'.join(sorted('0x' + h[2:] for h in byhash)) + '\n')
s = open(src).read()
found = re.findall(r'constexpr UINT64 (\w+) = (0x[0-9A-Fa-f]{16});', s)
bad = 0
for name, h in found:
    H = '0X' + h[2:].upper()
    if H not in byhash:
        print('UNKNOWN HASH (Script Hook V would close GTA): %s = %s; the database has %s' % (name, h, sorted(byname.get(name, [])) or 'no such name'))
        bad += 1
    elif byhash[H] != name and H not in byname.get(name, set()):
        print('NAME MISMATCH: %s = %s is %s in the database' % (name, h, byhash[H]))
        bad += 1
print('natives: %d checked, %d bad' % (len(found), bad))
sys.exit(1 if bad else 0)
