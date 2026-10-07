#!/usr/bin/env python3
"""check_v243.py - v2.43 end-to-end checks after a run (in test/run):
  on:     sh ../run_e2e.sh script_bridge.txt 110 FAKE_RAYCAP=12 FAKE_BRIDGE=1 [KEEPMAP=1]       (up the ramp, over the deck)
  under:  sh ../run_e2e.sh script_bridge.txt 110 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_STARTX=12 [KEEPMAP=1]  (past the ramp, under the deck)
  pause:  sh ../run_e2e.sh script_pause.txt 110 FAKE_RAYCAP=12 FAKE_USERPAUSE="50 56" FAKE_REPAIR_AT=62
  static: after any run: tiles stay in BeamNG behind the car
Usage: python3 ../check_v243.py <on|under|pause|static> [keep]"""
import math, os, re, sys
mode = sys.argv[1]
keep = len(sys.argv) > 2 and sys.argv[2] == 'keep'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
log = open('GtaBeamD1.log', errors='replace').read()
mlog = open('gtabridge/gtabridge_log.txt', errors='replace').read() if os.path.exists('gtabridge/gtabridge_log.txt') else ''
ev = open('fake_beam_events.txt', errors='replace').read() if os.path.exists('fake_beam_events.txt') else ''
walls = open('fake_beam_walls.txt', errors='replace').read() if os.path.exists('fake_beam_walls.txt') else ''
host = open('host_out.txt', errors='replace').read() if os.path.exists('host_out.txt') else ''
check('Drive ON on GTA\'s ground' in log, 'drive mode started on GTA\'s ground')

def ground(x, y): return 0.03 * x + 1.5 * math.sin(y / 25.0)
def surfaces(x, y):
    off = abs(x + 0.364 * y)
    h = 0
    if 90 < y < 230:
        if 130 <= y < 190: h = 8 if off < 18 else 0
        elif off <= 5: h = 8 * (y - 90) / 40 if y < 130 else 8 * (230 - y) / 40
    g = 200 + ground(x, y)
    return [g] + ([g + h] if h > 0 else [])
def path():
    out = []
    for l in open('fake_beam_path.csv'):
        r = l.strip().split(',')
        st, x, y, z = map(float, r[:4])
        if z < 150 or st < 15 or (len(r) > 8 and r[8] != 'D'): continue   # (while GTA drives)
        out.append((st, x, y, z))
    return out

if mode in ('on', 'under'):
    P = path()
    dev, deck, under, ymax, bad = 0.0, 0, 0, 0.0, []
    for st, x, y, z in P:
        S = surfaces(x, y)
        s = min(S, key=lambda v: abs(v - z))
        d = abs(z - s)
        if d > dev: dev = d
        if d > 0.6 and len(bad) < 5: bad.append('t %.1f y %.0f z %.2f (surfaces %s)' % (st, y, z, ' '.join('%.2f' % v for v in S)))
        ymax = max(ymax, y)
        if len(S) == 2 and s == S[1] and S[1] - S[0] > 7: deck += 1
        if len(S) == 2 and s == S[0] and S[1] - S[0] > 7: under += 1
    falls = log.count('fell through GTA\'s ground (')
    check(falls == 0, 'nothing fell through (%d)' % falls)
    check(ymax > 200, 'the car got past the bridge (y %.0f)' % ymax)
    check(dev < 0.6, 'BeamNG\'s car stays on the surface it should be on: worst %.2f m off %s' % (dev, bad[:3]))
    m = re.search(r'WALLS (\d+) of (\d+) triangles in (\d+) tiles, (\d+) over 4 m', walls)
    if mode == 'on':
        check(deck > 100 and under == 0, 'v2.43: the car drove up the ramp and over the deck (%d frames on it, %d under it)' % (deck, under))
        check(m and int(m.group(4)) == 0, 'v2.43: no wall hanging from the deck down to the ground (%s)' % (m.group(0) if m else walls[:80]))
        lo = sum(int(x) for x in re.findall(r'(\d+) points left out under edges', log))
        check(lo > 500, 'v2.43: the ground under the deck\'s edges left out (%d points)' % lo)
        if keep:
            lm = sum(int(x) for x in re.findall(r'(\d+) wanted tiles known only at another level', log))
            check(lm > 0, 'v2.43: the saved road under the deck (last run) was not used on the deck: measured for the deck (%d)' % lm)
    else:
        check(under > 100 and deck == 0, 'v2.43: the car drove under the deck, not over it (%d frames under it, %d on it)' % (under, deck))
        if keep:
            fd = sum(int(x) for x in re.findall(r'\+(\d+) from the saved map', log))
            check(fd > 50, 'v2.43: the road under the deck came from the saved map (%d tiles)' % fd)
    check(m is not None, 'walls counted in BeamNG (%s)' % walls[:90])
if mode == 'static':
    rmd = sum(int(x) for x in re.findall(r'(\d+) far tiles taken out of BeamNG', log))
    st = re.findall(r'(\d+) tiles in BeamNG \((\d+) built', log)
    check(rmd == 0, 'v2.43: tiles behind the car stay in BeamNG (none taken out within the budget: %d)' % rmd)
    rm = [int(x) for x in re.findall(r'(\d+) removed;', log)]
    check(st and max(int(a) for a, b in st) > 150, 'v2.43: a static world: %s tiles in BeamNG at most' % (max(int(a) for a, b in st) if st else 0))
    sw = sum(int(x) for x in re.findall(r'(\d+) sent while standing', log))
    check(sw > 5, 'v2.43: standing still, measured ground nearby went to BeamNG (%d tiles)' % sw)
if mode == 'pause':
    check('BeamNG paused (simTimeAuthority.pause)' in mlog, 'GTA paused -> the mod paused BeamNG (simTimeAuthority.pause)')
    check('GTA runs again: BeamNG runs again' in mlog, 'GTA running again -> BeamNG runs again')
    check('BeamNG is paused (because GTA is)' in log and re.search(r'BeamNG runs again \(paused [\d.]+ s\)', log), 'GTA saw BeamNG paused and running again')
    check('no car data from BeamNG for 5 s' not in log and 'no car data from BeamNG for 1 s' not in log, 'no "no car data" while paused')
    check('you paused BeamNG: GTA\'s pause menu opened' in log and 'FAKE: pause menu opened (menu 0xD528C7E2)' in host, 'you paused BeamNG -> GTA\'s pause menu opened (FE_MENU_VERSION_SP_PAUSE)')
    check('BeamNG runs again: GTA\'s pause menu closed' in log and 'FAKE: frontend active 0' in host, 'BeamNG running again -> GTA\'s pause menu closed')
    check('BeamNG physics PAUSED (by the mod)' in ev and 'BeamNG physics RUNNING (by the mod)' in ev, 'the fake BeamNG\'s physics were paused and resumed by the mod')
    check('your recover_vehicle binding repaired the car where it is' in log and 'repair (recover_vehicle)' in mlog, 'your repair binding: repaired where the car is, GTA told')
    check('fell through' not in log, 'nothing fell through (pauses, repair)')
    pf = re.search(r'PAUSED FRAMES (\d+)', ev)
    # the drawn car: no jump when BeamNG runs again
    rows = []
    if os.path.exists('fake_puppet.csv'):
        for l in open('fake_puppet.csv'):
            r = l.strip().split(',')
            if len(r) > 4 and r[1] in ('coords', 'quat', 'rot'):
                try: rows.append((float(r[0]), float(r[2]), float(r[3]), float(r[4])))
                except ValueError: pass
    jumps = 0
    for a, b in zip(rows, rows[1:]):
        dt = b[0] - a[0]
        if dt <= 0: continue
        d = math.sqrt((b[1] - a[1]) ** 2 + (b[2] - a[2]) ** 2)
        if d > 3.0 and d / dt > 60: jumps += 1
    check(jumps == 0, 'the drawn car never jumps (pauses, clock found again after them): %d jumps' % jumps)
print()
print('ALL PASS' if not fails else '%d FAILED: %s' % (len(fails), fails))
sys.exit(1 if fails else 0)
