#!/usr/bin/env python3
"""check_v277.py - v2.77 PuppetSolid end-to-end checks (run in test/run after run_e2e.sh). These test the plugin's code paths
against the FAKE GTA only (fakeshv.cpp): they say nothing about GTA's real physics. Only Treto's in-game test can.
  ps      : PuppetSolid=1, script_world.txt FAKE_TRAFFIC=1 FAKE_PSKICK=1: engaged; the fake "GTA crash" at the parked car found as
            a GTA car and sent to BeamNG ("tc"), BeamNG's mod took it; none of the scripted reactions (despawn, push, twins); it
            follows BeamNG's car; no snaps; off at F10 off; crash damage off then on again.
  frame W : PuppetSolid=1, FAKE_WOBBLE=1 FAKE_ANGLOCAL=0|1: the turn-frame check finds the fake's frame (W = world | car).
  calm    : PuppetSolid=1, a plain drive (FAKE_ROLL=1: hills): engaged, follows BeamNG's car, no snaps, no crash seen.
  off     : PuppetSolid=0, FAKE_TRAFFIC=1 (TrafficMode=despawn): v2.76's way: never engaged, the touched car taken away."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'ps'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host = rd('GtaBeamD1.log'), rd('host_out.txt')
mlog = rd('gtabridge/gtabridge_log.txt')
tens = re.findall(r'PuppetSolid \(v2\.77\): 10 s: the hidden car vs BeamNG\'s car: ([\d.]+) m off on average, ([\d.]+) m max.*?; (\d+) snaps', log)

if mode == 'ps':
    check('PuppetSolid (v2.77): ENGAGED (drive mode' in log, 'engaged at the drive start')
    check(re.search(r"PuppetSolid: GTA's crash: a GTA car \(9001.*SENT to BeamNG", log) is not None, "the fake GTA crash: found as a GTA car, sent to BeamNG")
    check("a GTA car (GTA's crash) hit the car" in mlog, "BeamNG's mod took the tc")
    check('the hidden car touches a GTA car (9001)' in log, 'the touch logged')
    check('TrafficMode=despawn: GTA car' not in log, 'no despawn')
    check('traffic twins: asked' not in log, 'no twins asked')
    check("traffic: BeamNG's car and a GTA car" not in log, "no v2.7 push")
    check(len(tens) >= 1, '10 s lines (%d)' % len(tens))
    check(all(float(a) < 0.2 for a, b, s in tens), 'follows BeamNG\'s car: averages %s m' % [a for a, b, s in tens])
    check(all(int(s) == 0 for a, b, s in tens), 'no snaps: %s' % [s for a, b, s in tens])
    check(re.search(r'PuppetSolid \(v2\.77\): off \(', log) is not None, 'off at F10 off')
    check('vehicle 100 strong 1' in host and 'vehicle 100 strong 0' in host, 'crash damage off while solid, back on after')
    check("didn't take the rotation" not in log and "doesn't take the rotation" not in log, 'no rotation check fired (PuppetSolid sets none)')
    check('GtaBeamD1.asi' not in host or 'exception' not in host.lower(), 'no exception')
elif mode == 'frame':
    want = sys.argv[2] if len(sys.argv) > 2 else 'world'
    m = re.search(r"SET_ENTITY_ANGULAR_VELOCITY is in the (CAR'S FRAME|WORLD frame)", log)
    got = None if not m else ('car' if 'CAR' in m.group(1) else 'world')
    check(got == want, 'turn frame found: %s (the fake: %s)' % (got, want))
    check(all(int(s) == 0 for a, b, s in tens), 'no snaps: %s' % [s for a, b, s in tens])
elif mode == 'calm':
    check('PuppetSolid (v2.77): ENGAGED (drive mode' in log, 'engaged at the drive start')
    check(len(tens) >= 1, '10 s lines (%d)' % len(tens))
    check(all(float(a) < 0.2 for a, b, s in tens), "follows BeamNG's car: averages %s m" % [a for a, b, s in tens])
    check(all(int(s) == 0 for a, b, s in tens), 'no snaps: %s' % [s for a, b, s in tens])
    check("GTA's crash:" not in log and 'GTA changed the hidden car' not in log, 'no crash seen in a plain drive')
elif mode == 'off':
    check('PuppetSolid (v2.77): ENGAGED' not in log, 'never engaged')
    check('settings: v2.77: PuppetSolid 0' in log, 'the setting read as 0')
    check('TrafficMode=despawn: GTA car 9001 touched' in log, "v2.76's despawn")
print('ALL PASS' if not fails else '%d FAIL' % len(fails))
sys.exit(1 if fails else 0)
