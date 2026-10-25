#!/usr/bin/env python3
"""check_v275.py - v2.75 end-to-end checks (run in test/run after run_e2e.sh):
  twinstale : script_world.txt FAKE_TRAFFIC=1 FAKE_TWIN_STALE=3: BeamNG says the twin's old spot (150 m up) for 3 frames after
              its teleport (his v2.74 log: 17 of 18 twins "thrown" after 1 pose): those poses are skipped, GTA's car is never moved
              there, the twin is not released as thrown, the crash happens.
  pedout    : script_world.txt FAKE_TRAFFIC=1 FAKE_PED=1 FAKE_PEDNEAR=1: a person standing inside the standing car is moved out
              beside it (his "people despawn when I hit them slow").
  propread  : script_props_fixed.txt FAKE_PROPS=1 FAKE_PROPSTUCK=1 FAKE_PROPREAD=1: GTA gives back the speed a copy was set but
              never moves it (his v2.74 "moved 0.04 m, upright 1.00"): the plugin knocks it over itself.
  push      : script_world.txt FAKE_TRAFFIC=1 with TrafficMode=push (the old "gta" mode's place): v2.7's push, no twins, the hidden
              car never made solid."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'twinstale'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host, ev = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt')
mlog = rd('gtabridge/gtabridge_log.txt')

if mode == 'twinstale':
    sk = [int(x) for x in re.findall(r"twin \d's first pose taken [\d.]+ m from GTA car 9001 \((\d+) old poses skipped", log)]
    check(sk and sk[0] >= 1, 'v2.75: the stale poses skipped before the first one copied (%s)' % sk)
    check("its twin was thrown in BeamNG" not in log, 'v2.75: not released as thrown')
    park = re.search(r'traffic car 9001 parked at ([-\d.]+) ([-\d.]+) ([-\d.]+)', host)
    ps = [tuple(map(float, x)) for x in re.findall(r'traffic car posed ([-\d.]+) ([-\d.]+) ([-\d.]+)', host)]
    far = max((sum((a - float(b)) ** 2 for a, b in zip(q, park.groups())) ** 0.5 for q in ps), default=None) if park else None
    check(far is not None and far < 15, "v2.75: GTA's car never moved to the twin's old spot (farthest from where it was parked: %s m)" % (far and round(far, 1)))
    check('TWINHIT' in ev, 'the crash happened in BeamNG')
elif mode == 'pedout':
    check('traffic person 9002 stands' in host, 'the fake put a person inside the car')
    check('a person inside the drawn car' in log and 'person 9002 moved to' in host, 'v2.75: moved out beside the car')
elif mode == 'propread':
    check(re.search(r"copy of prop \d+ velocity set: GTA doesn't move this model", host) is not None, "the fake GTA didn't move the copies")
    fl = [int(x) for x in re.findall(r'(\d+) knocked over by the plugin', log)]
    check(fl and max(fl) > 0, 'v2.75: knocked over by the plugin though the speed read back (%s)' % fl)
    check("(GTA's physics), upright 1.00" not in log, 'v2.75: no copy left standing as "moved by GTA"')
elif mode == 'push':
    check('traffic twins: asked BeamNG' not in log, 'v2.75: TrafficMode=push: no twins')
    check('mode push' in log, 'v2.75: the log says push')
    check(re.search(r"traffic: BeamNG's car and a GTA car \(class \d+", log) is not None and 'traffic car velocity set' in host, "v2.7's push")
    check('times the hidden car was solid' not in log, 'v2.75: the hidden car never made solid')
    check('fell through' not in log, 'nothing fell through')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
sys.exit(1 if fails else 0)
