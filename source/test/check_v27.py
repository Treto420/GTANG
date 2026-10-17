#!/usr/bin/env python3
"""check_v27.py - v2.7 end-to-end checks (run in test/run after run_e2e.sh):
  traffic : FAKE_TRAFFIC=1, script_world.txt: a parked GTA car 25 m ahead. PASS = the plugin sees the contact, pushes and damages
            GTA's car, BeamNG's car is pushed back (the fake's BUMP), the BeamNG mod logs it.
  twins   : FAKE_TRAFFIC=1 with TrafficTwins=4 (v2.71): the parked car gets a BeamNG twin; the crash is BeamNG's; GTA copies it.
  drop    : FAKE_DROP=1, script_world.txt: the ground right of the car's line from 8.5 m off it is 16 m lower (out of the normal 12 m reach). PASS = those
            empty tiles near the car are measured again, the second time with the tall window, and kept (ground below the level);
            not endlessly; nothing fell through.
  hill    : FAKE_HILL=1, script_world.txt: a 50-degree hillside beside the road rising past the rays' reach. PASS = it is found
            from above and kept (it meets the road), nothing fell through.
  seat    : (after any drive run) v2.71: the driver's seat from BeamNG's driver camera; you visible in drive mode.
  crash   : the 2.63 log's lethal crash (a glance off a wall: 32 km/h lost, the velocity turned by 77 km/h) costs a little
            health now (unit check of the rule, no run)."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'traffic'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''

if mode == 'crash':
    # the rule from WeaponsCrash (crashMinDv 7, crashHpPerDv 6): dv = max(speed lost, half the change of the flat velocity)
    import math
    def hurt(v0, v1, minDv=7.0, hp=6.0):
        s0 = math.hypot(*v0)
        loss = s0 - math.hypot(*v1)
        turn = 0.0
        if s0 > 1.0:
            ux, uy = v0[0] / s0, v0[1] / s0
            ax, ay = v1[0] - v0[0], v1[1] - v0[1]
            al = ax * ux + ay * uy
            turn = math.sqrt(max(0.0, ax * ax + ay * ay - al * al))
        dv = max(loss, 0.5 * turn)
        return max(0.0, (dv - minDv) * hp)
    # 512.3 s in his log: 29.6 m/s, 9 m/s lost, the velocity changed by 21.4 m/s (77 km/h): 2.63 took 86 health (dead)
    v0 = (29.6, 0.0)
    sp1 = 20.6
    ang = math.acos(max(-1, min(1, (29.6 ** 2 + sp1 ** 2 - 21.4 ** 2) / (2 * 29.6 * sp1))))
    h = hurt(v0, (sp1 * math.cos(ang), sp1 * math.sin(ang)))
    check(5 < h < 40, 'his lethal 2.63 glance off a wall now costs %.0f health (was 86)' % h)
    check(hurt((30.0, 0.0), (0.0, 0.0)) > 100, 'a head-on stop from 108 km/h still kills')
    check(hurt((0.0, 0.0), (0.0, 0.0)) == 0 and hurt((5.0, 0.0), (25.0, 0.0)) == 0 and hurt((5.0, 0.0), (25.0, 3.0)) == 0, 'speeding up never hurts')
    check(hurt((20.0, 0.0), (20.0, 18.0)) > 0, 'hit hard from the side at speed still hurts')
    print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
    sys.exit(0)

log, host, ev = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt')
mlog = rd('gtabridge/gtabridge_log.txt')
check('Drive ON on GTA\'s ground' in log, 'drive mode started')
if mode == 'traffic':
    check('traffic car 9001 parked' in host, 'the fake parked a GTA car ahead')
    m = re.search(r'traffic: BeamNG\'s car and a GTA car \(class \d+, (\d+) kg\) met at (\d+) km/h closing: GTA\'s car pushed ([\d.]+) km/h, BeamNG\'s ([\d.]+) km/h, dent (\d+) mm', log)
    check(m is not None, 'the plugin saw the contact (%s)' % (m and m.group(0)))
    if m:
        vn, pg, pb = float(m.group(2)), float(m.group(3)), float(m.group(4))
        check(vn > 5 and pg > 0 and pb > 0, 'both cars pushed (closing %.0f km/h: GTA %.1f, BeamNG %.1f km/h)' % (vn, pg, pb))
    check('traffic car velocity set' in host, 'GTA\'s car got its push (SET_ENTITY_VELOCITY)')
    check('vehicle 9001 damaged' in host, 'GTA\'s car got its damage (SET_VEHICLE_DAMAGE)')
    check('traffic: a GTA car hit the car' in mlog, 'the BeamNG mod pushed its car (log)')
    check('BUMP' in ev, 'the fake BeamNG car\'s speed changed')
    check('traffic (v2.7): ' in log and 'GtaBeamD1_traffic.json' in log, 'GtaBeamD1_traffic.json read or written')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'twins':
    # v2.71 (FAKE_TRAFFIC=1, TrafficTwins=4): the parked GTA car gets a BeamNG twin before the car meets it; the crash is the
    # fake BeamNG's (the twin pushed and dented); GTA's car copies the twin and gets GTA damage at its dents; back to GTA once still
    check('traffic twins (v2.71): asked BeamNG for 4 twins' in log, 'v2.71: GTA asked for the pool')
    check(len(re.findall(r'traffic twins: twin \d ready', log)) == 4, 'v2.71: 4 twins spawned, measured, parked (%d)' % len(re.findall(r'traffic twins: twin \d ready', log)))
    check(ev.count('active false') >= 4, 'v2.71: parked (setActive 0) in BeamNG')
    check(re.search(r'GTA car 9001 .*meets the car in [\d.]+ s: twin \d', log) is not None, 'v2.71: the GTA car got a twin before the meeting')
    check('TWINHIT' in ev, 'v2.71: BeamNG\'s car hit the twin in BeamNG')
    check('FAKE: traffic car posed' in host and re.search(r'traffic car velocity set -?[1-9]', host) is not None, 'v2.71: GTA\'s car copied its twin (pushed along)')
    dm = re.findall(r'FAKE: vehicle 9001 damaged at ([-\d.]+) ([-\d.]+) ([-\d.]+) by (\d+)', host)
    check(len(dm) >= 1 and all(float(x[3]) > 100 for x in dm), 'v2.71: GTA damage where the twin was dented (%s)' % dm[:3])
    check(re.search(r'GTA car 9001 back to GTA \(standing still', log) is not None and 'vehicle 9001 no longer needed' in host, 'v2.71: back to GTA once it stood still')
    check('traffic: BeamNG\'s car and a GTA car' not in log, 'v2.71: no v2.7 push on top')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'drop':
    again = re.findall(r'ground: tile (-?\d+),(-?\d+) (?:is empty|has empty cells) near the car: measured again \(try (\d)(, tall window)?\)', log)
    kept = re.findall(r'ground: tile (-?\d+),(-?\d+): found with the tall window \(below the level', log)
    # v2.71: the deep search finds it on the first measure (each point with no ground looks 28 m further down)
    deep = sum(int(x) for x in re.findall(r'v2\.71: \d+ rays looked below / above points with no ground in their window, (\d+) found ground', log))
    check(deep > 100 or (any(a[3] for a in again) and len(kept) > 0),
          'the ground 16 m down found: v2.71 deep search %d points, or measured again with the tall window (%d tries) and kept (%d tiles)' % (deep, len(again), len(kept)))
    check(len(again) < 200, 'not endlessly (%d tries in all)' % len(again))
    check('fell through' not in log, 'the car on the road didn\'t fall')
elif mode == 'roof':
    m = re.findall(r'found above and kept \(meets the ground next to it\) (\d+), dropped \(a roof, an overpass\) (\d+)', log)
    dropped, kept = sum(int(x[1]) for x in m), sum(int(x[0]) for x in m)
    check(dropped > 20 and kept < dropped / 4, 'v2.71: the roof found from above was dropped (%d points dropped, %d kept)' % (dropped, kept))
    check('fell through' not in log, 'nothing fell through')
elif mode == 'seat':
    # v2.71 (any drive run): the fake's driver camera: the eye at (-0.37, 0.3, 1.0) of the car file; your head in GTA's car at
    # (-0.4, 0, 0.45) (+ 9 cm ahead, 7 cm up): GTA's car offset (-0.03, -0.21, -0.48); you hidden with the car, then shown again
    m = re.search(r"drive: driver's eye ([-\d.]+) ([-\d.]+) ([-\d.]+) m", mlog)
    check(m is not None and abs(float(m.group(1)) + 0.37) < 0.02 and abs(float(m.group(2)) - 0.5) < 0.02 and abs(float(m.group(3)) - 0.55) < 0.02,
          'v2.71: the BeamNG mod read the driver camera into the car\'s frame (%s)' % (m and m.groups(),))
    m = re.search(r"driver's seat: BeamNG's eye at ([-\d.]+) ([-\d.]+) ([-\d.]+) m .*?-> ([-\d.]+) ([-\d.]+) ([-\d.]+)\)", log)
    want = (-0.03, -0.21, -0.48)
    check(m is not None and all(abs(float(m.group(4 + k)) - want[k]) < 0.03 for k in range(3)), 'v2.71: GTA\'s car put so your head is at that eye (%s)' % (m and m.groups(),))
    i = host.find('FAKE: player hidden with the car')
    check(i >= 0 and 'FAKE: player visible 1' in host[i:], 'v2.71: hidden with the car at F10, then shown again')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'hill':
    m = re.findall(r'v2\.71: (\d+) rays looked below / above points with no ground in their window, (\d+) found ground; found above and kept \(meets the ground next to it\) (\d+), dropped \(a roof, an overpass\) (\d+)', log)
    kept = sum(int(x[2]) for x in m)
    dropped = sum(int(x[3]) for x in m)
    check(kept > 100, 'v2.71: the hillside over the window found from above and kept (%d points; %d dropped)' % (kept, dropped))
    check(dropped < kept, 'v2.71: most of it kept (it meets the road: %d kept, %d dropped)' % (kept, dropped))
    check('fell through' not in log, 'nothing fell through')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
