#!/usr/bin/env python3
"""check_world.py - v2.4 end-to-end check of T3 (GTA's ground in BeamNG) after a run of host.exe script_world.txt with
fake_beam_lua.py (run it in test/run). The fake GTA ground (fakeshv.cpp FakeGround) is measured by the plugin's rays,
built as tiles by the real BeamNG mod in the fake BeamNG, the fake BeamNG car stands on them, and its poses come back
to GTA's puppet car. PASS = the puppet's tyres follow GTA's ground, BeamNG got tiles with several ground models, the
teleport found its convention, and nothing fell through.
Usage: python3 ../check_world.py [FAKE_HOLE]"""
import math, os, re, statistics, sys

hole = len(sys.argv) > 1 and sys.argv[1] in ('1', 'hole')
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)

rough = os.environ.get('FAKE_ROUGH') == '1'
def ground(x, y):
    if hole and 70 < y < 80: return None
    z = 0.03 * x + 1.5 * math.sin(y / 25.0)
    if 3.5 < x < 4.0: z += 0.15
    if rough:
        ax = abs(x)
        if ax < 6: z -= 0.02 * ax
        else: z -= 0.12 + 0.04 * math.sin(x * 2.1) * math.sin(y * 1.7) + 0.02 * math.sin(x * 5.3 + y * 3.1)
        ym = math.fmod(abs(y), 40.0)
        if 20.0 < ym < 21.5: z -= 0.06 * math.sin((ym - 20.0) / 1.5 * math.pi)
    return z

log = open('GtaBeamD1.log', errors='replace').read()
mlog = open('gtabridge/gtabridge_log.txt', errors='replace').read() if os.path.exists('gtabridge/gtabridge_log.txt') else ''
ev = open('fake_beam_events.txt', errors='replace').read() if os.path.exists('fake_beam_events.txt') else ''

check('Drive ON on GTA\'s ground' in log, 'drive mode started on GTA\'s ground (teleport answered)')
tl = list(re.finditer(r'TELEPORT to ([-\d.]+) ([-\d.]+) ([-\d.]+) heading ([-\d.]+) deg', ev))
print('   teleports: %s' % ', '.join(t.group(4) + ' deg' for t in tl))
m = tl[1] if len(tl) > 1 and abs(float(tl[0].group(4)) - 110.0) > 1 else (tl[0] if tl else None)   # the start teleport (its first try may use the wrong convention: read back and corrected)
check(m is not None and abs(float(m.group(4)) - 110.0) < 1.0, 'BeamNG\'s car teleported facing the GTA car\'s heading (110 deg): %s' % (m and m.group(4)))
check(m is not None and abs(float(m.group(3)) - 200.0) < 1.0, 'BeamNG\'s car put ~200 m up (GTA ground 0 + WorldZLift): z %s' % (m and m.group(3)))
check('rotation convention that works: quatFromDir(-forward)' in mlog, 'the mod found the rotation convention by reading the car back')
tiles = re.findall(r'(\d+) tiles in BeamNG \((\d+) built', log)
check(tiles and max(int(t[1]) for t in tiles) >= 20, 'tiles sent and built in BeamNG (%s)' % (tiles[:3]))
gm = re.findall(r'triangles per ground model: ([A-Z_=\d ]+)', log)
check(gm and all(k in gm[-1] for k in ('ASPHALT', 'DIRT_DUSTY', 'GRASS')), 'GTA Tarmac/DirtTrack/Grass became ASPHALT/DIRT_DUSTY/GRASS sub-meshes (%s)' % (gm[-1] if gm else None))
ref = re.findall(r'(\d+) refused by GTA \(limit now (\d+)', log)
check(bool(ref), 'ray stats logged (refused/limit: %s)' % ref[:3])
check('materials: ' in log and 'Tarmac' in log, 'GTA materials counted (Tarmac seen)')
mat = open('GtaBeamD1_materials.txt', errors='replace').read() if os.path.exists('GtaBeamD1_materials.txt') else ''
check('0x10DD5498  Tarmac' in mat and '-> ASPHALT' in mat, 'GtaBeamD1_materials.txt written with names and BeamNG ground models')
falls = log.count('BeamNG\'s car fell through GTA\'s ground (')
if not hole:
    check(falls == 0, 'nothing fell through (%d)' % falls)
else:
    check(falls >= 1 and 'BeamNG\'s car is on GTA\'s ground (fell)' in log, 'the hole: the car fell through and was put back (%d)' % falls)
    check('sent without those cells' in log, 'the hole: tiles at its edge sent with only the empty cells left out')
    # v2.41: no loop: at most 3 falls, then drive mode ends and GTA's car lands on the last good spot
    check(falls <= 3 and (falls < 3 or '3 times in 30 s' in log), 'the hole: no reset loop (%d falls; 3 = drive OFF)' % falls)
    check('BeamNG couldn\'t build tile' not in log, 'the hole: no tile BeamNG can\'t build is sent (no cells with ground)')
    fl = re.findall(r'BeamNG\'s car is on GTA\'s ground \(fell\).*?; ([\d.]+) s after asking', log)
    check(fl and max(float(x) for x in fl) < 6.0, 'the hole: put back within 6 s (%s)' % fl)
# v2.41: adaptive tiles: far fewer rays per tile than 289
rpt = [float(x) for x in re.findall(r'finished \([\d.]+ a second, ([\d.]+) rays each', log)]
rpt = [x for x in rpt if x > 0]
check(rpt and min(rpt) < (180 if rough else 100), 'adaptive tiles: rays per tile %s (v2.4: 289)' % rpt)
check('GTA\'s car is back on GTA\'s ground' in log, 'F10 off: GTA\'s car landed on GTA\'s ground (collision and ray checked)')

# the puppet (GTA's hidden car) during drive mode: its drawn origin = puppet - 0.6 x up must sit on GTA's ground
rows = []
for line in open('fake_puppet.csv'):
    p = line.strip().split(',')
    if len(p) < 14 or p[1] != 'coords': continue
    rows.append([float(x) for x in (p[2], p[3], p[4], p[8], p[9], p[10])])   # position, up (the rotation of that moment)
dz, xs, ys, deep = [], [], [], []
for x, y, z, ux, uy, uz in rows:
    ox, oy, oz = x - 0.6 * ux, y - 0.6 * uy, z - 0.6 * uz
    g = ground(ox, oy)
    if g is None and hole and 70 <= oy <= 80:
        deep.append(ground(ox, 69.9) - oz)   # over the hole: how far under the ground at its edge
        continue
    if g is None or (3.0 < ox < 4.5): continue   # the curb itself is a ramp one spacing wide
    if hole and oy > 55:
        deep.append(g - oz)   # around the hole the car falls in and is put back: how deep it got
        continue
    dz.append(oz - g)
    xs.append(ox); ys.append(oy)
moved = math.hypot(xs[-1] - xs[0], ys[-1] - ys[0]) if xs else 0
check(len(dz) > 100, 'puppet placements in drive mode: %d' % len(dz))
check(moved > 40, 'the car drove %.0f m on GTA\'s ground' % moved)
if dz:
    a = sorted(abs(v) for v in dz)
    p95 = a[int(len(a) * 0.95) - 1]
    print('   drawn tyres vs GTA ground: median %+.3f m, p95 |dz| %.3f m, max %.3f m' % (statistics.median(dz), p95, a[-1]))
    check(p95 < 0.08, 'the drawn car\'s tyres follow GTA\'s ground (p95 %.1f cm)' % (p95 * 100))
if hole:
    dmax = max(deep) if deep else 0
    check(dmax < 5.0, 'the hole: the drawn car never got more than 5 m under the ground before being put back (%.1f m)' % dmax)
print()
print('WORLD E2E: ALL PASS' if not fails else 'WORLD E2E: %d FAILED: %s' % (len(fails), fails))
sys.exit(1 if fails else 0)
