#!/usr/bin/env python3
"""check_v27.py - v2.7 end-to-end checks (run in test/run after run_e2e.sh):
  traffic : FAKE_TRAFFIC=1, script_world.txt: a parked GTA car 25 m ahead. PASS = the plugin sees the contact, pushes and damages
            GTA's car, BeamNG's car is pushed back (the fake's BUMP), the BeamNG mod logs it.
  drop    : FAKE_DROP=1, script_world.txt: the ground right of the car's line from 8.5 m off it is 16 m lower (out of the normal 12 m reach). PASS = those
            empty tiles near the car are measured again, the second time with the tall window, and kept (ground below the level);
            not endlessly; nothing fell through.
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
elif mode == 'drop':
    again = re.findall(r'ground: tile (-?\d+),(-?\d+) (?:is empty|has empty cells) near the car: measured again \(try (\d)(, tall window)?\)', log)
    check(any(a[3] for a in again), 'empty tiles beside the road measured again with the tall window (%d tries)' % len(again))
    kept = re.findall(r'ground: tile (-?\d+),(-?\d+): found with the tall window \(below the level', log)
    check(len(kept) > 0, 'the ground 16 m down kept for them (%d tiles)' % len(kept))
    check(len(again) < 200, 'not endlessly (%d tries in all)' % len(again))
    check('fell through' not in log, 'the car on the road didn\'t fall')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
