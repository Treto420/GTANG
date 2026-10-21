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
  startpad: FAKE_HOLE=1 FAKE_STARTY=75 (v2.72): F10 over a hole: a pad there, placed once, no loop.
  seatadj : script_seat.txt (v2.72): Ctrl+Shift+F10 seat adjust, saved per car.
  door    : (after the foot run) v2.73: GTA's door ratio turns the drawn front doors.
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
    if 'traffic person 9002 stands' in host:   # v2.72 (FAKE_PED=1): a person 12 m ahead
        import math
        ph = re.search(r'traffic: BeamNG\'s car hit a person at (\d+) km/h', log)
        pv = re.findall(r'person 9002 velocity ([-\d.]+) ([-\d.]+) ([-\d.]+)', host)
        check(ph is not None and len(pv) >= 2, 'v2.72: the person was hit and got a speed on 2 ticks (%s, %s)' % (ph and ph.group(0), pv[:2]))
        if ph and pv:
            kmh, (vx, vy, vz) = float(ph.group(1)), map(float, pv[0])
            hz = math.hypot(vx, vy) * 3.6
            check(vz >= 3.0 or hz > kmh, 'v2.72: out of the car\'s way: thrown up (%.1f m/s) or shoved ahead faster than it (%.0f vs %.0f km/h)' % (vz, hz, kmh))
        check(re.search(r'traffic: the person hit at \d+ km/h, 0\.5 s later: exists', log) is not None, 'v2.72: the hit person is watched (0.5 s log)')
elif mode == 'twins':
    # v2.71 (FAKE_TRAFFIC=1, TrafficTwins=4): the parked GTA car gets a BeamNG twin before the car meets it; the crash is the
    # fake BeamNG's (the twin pushed and dented); GTA's car copies the twin and gets GTA damage at its dents; back to GTA once still
    check('traffic twins: asked BeamNG for 4 twins' in log, 'v2.71: GTA asked for the pool')
    i_ask, i_on = log.find('traffic twins: asked BeamNG'), log.find('Drive ON on GTA')
    check(0 <= i_ask < i_on, 'v2.72: asked when the drive starts, before the car is put on GTA\'s ground')
    check(len(re.findall(r'traffic twins: twin \d ready', log)) == 4, 'v2.71: 4 twins spawned, measured, parked (%d)' % len(re.findall(r'traffic twins: twin \d ready', log)))
    check(ev.count('active false') >= 4, 'v2.71: parked (setActive 0) in BeamNG')
    check(re.search(r'GTA car 9001 .*meets the car in [\d.]+ s: twin \d', log) is not None, 'v2.71: the GTA car got a twin before the meeting')
    check('TWINHIT' in ev, 'v2.71: BeamNG\'s car hit the twin in BeamNG')
    check('FAKE: traffic car posed' in host and re.search(r'traffic car velocity set -?[1-9]', host) is not None, 'v2.71: GTA\'s car copied its twin (pushed along)')
    dm = re.findall(r'GTA car 9001 dented at ([-\d.]+) ([-\d.]+) ([-\d.]+): wanted (\d+) mm, GTA shows (\d+) mm after (\d+) steps', log)
    check(len(dm) >= 1 and all(int(x[4]) >= int(x[3]) and int(x[5]) > 1 for x in dm), 'v2.72: GTA dents where the twin was dented, stepped until GTA shows them (%s)' % dm[:3])
    rad = re.findall(r'FAKE: vehicle 9001 damaged at [-\d.]+ [-\d.]+ [-\d.]+ by (\d+) radius (\d+)', host)
    check(rad and all(x[0] == x[1] for x in rad), 'v2.72: damage = radius (%s)' % rad[:3])
    check(re.search(r'GTA car 9001 back to GTA \(standing still', log) is not None and 'vehicle 9001 no longer needed' in host, 'v2.71: back to GTA once it stood still')
    check('traffic: BeamNG\'s car and a GTA car' not in log, 'v2.71: no v2.7 push on top')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'startpad':
    # v2.72 (FAKE_HOLE=1 FAKE_STARTY=75: F10 over the hole): BeamNG lays a pad there once and puts the car on it; no loop
    check('a 12 m pad at' in mlog, 'v2.72: BeamNG laid a pad under the start spot (%s)' % re.findall(r'a 12 m pad at [^\n]*', mlog)[:1])
    m = re.search(r"BeamNG's car is on GTA's ground \(start\).*?; ([\d.]+) s after asking", log)
    check(m is not None and float(m.group(1)) < 12.0, 'v2.72: placed once, not after 21 s of asks restarting the wait (%s s)' % (m and m.group(1)))
    check(log.count("asking BeamNG to put its car on GTA's ground (fell)") <= 1, 'v2.72: no fall loop')
elif mode == 'seatadj':
    # v2.72 (script_seat.txt): Ctrl+Shift+F10, up arrow held, Page Up, Ctrl+Shift+F10: saved for this car, the car moved under you
    m = re.search(r'seat \(v2\.72\): saved for (\S+): ([-\d.]+) right, ([-\d.]+) forward, ([-\d.]+) up', log)
    check(m is not None and float(m.group(3)) > 0.05 and float(m.group(4)) > 0.02 and abs(float(m.group(2))) < 0.01, 'v2.72: seat adjust saved (%s)' % (m and m.groups(),))
    js = rd('GtaBeamD1_seats.json')
    check(m is not None and ('"%s"' % m.group(1)) in js, 'v2.72: GtaBeamD1_seats.json has it')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'door':
    # v2.73 (after the foot run, the car file with a right door: car_glass): getting out and back in: GTA's door ratio turns the
    # drawn front doors (no BeamNG door latch any more)
    check(re.search(r"door \(v2\.73\): GTA's (left|right) front door opens", log) is not None and re.search(r"door \(v2\.73\): GTA's (left|right) front door shut", log) is not None,
          'v2.73: GTA\'s doors seen open and shut (%s)' % re.findall(r'door \(v2\.73\)[^\n]*', log)[:4])
    check("right front door opens (0.80): the drawn car's opens with it" in log, 'v2.73: the drawn right door (the car file\'s glass_door) turns with it')
    check('toggle door' not in ev, 'v2.73: no BeamNG door latch toggled')
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
    # v2.72: ground found above the window is kept (a roof beside the road becomes a block where the building is)
    kept = sum(int(x) for x in re.findall(r'found ground \((\d+) of them above it: kept, v2\.72\)', log))
    check(kept > 20, 'v2.72: the roof found from above is kept, a block where the building is (%d points)' % kept)
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
    kept = sum(int(x) for x in re.findall(r'found ground \((\d+) of them above it: kept, v2\.72\)', log))
    check(kept > 100, 'v2.72: the hillside over the window found from above and kept, all of it (%d points)' % kept)
    check('fell through' not in log, 'nothing fell through')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
