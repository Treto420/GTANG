#!/usr/bin/env python3
"""check_v273.py - v2.73 end-to-end checks (run in test/run after run_e2e.sh):
  propstuck: script_props_fixed.txt FAKE_PROPS=1 FAKE_PROPSTUCK=1: GTA's physics don't move the knocked copies (his v2.72 log):
             the plugin knocks them over itself; the map's own props are never deleted or made mission entities (the crash).
  gap      : script_hole.txt FAKE_GAP=1: a 10 m strip across the road with no ground and no water (missing collision): filled
             in as asphalt, the car drives over it (no fall). (FAKE_HOLE's strip is water: left empty, check_world.py hole.)
  burnout  : script_world.txt FAKE_BURNOUT=1: the wheels spin 8 m/s faster than the car goes: GTA's tyre smoke at the wheels,
             stopped when the drive ends.
  ped      : (after the traffic run, FAKE_PED=1) the hit person is held as a mission entity, then given back to GTA.
  dents    : (after the twins run) GTA's dents capped: at most 10 steps, damage at most 110 + 300 x the depth wanted."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'propstuck'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host, ev = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt')

if mode == 'propstuck':
    check(re.search(r"copy of prop \d+ velocity set: GTA doesn't move this model", host) is not None, 'the fake GTA ignored the copies\' velocity (his posts and bins)')
    m = re.findall(r'the knocked prop 0x(\w+) 1 s later: its copy moved ([\d.]+) m \(the plugin moved it[^)]*\), upright ([-\d.]+)', log)
    check(len(m) >= 1 and all(float(x[1]) > 0.3 or float(x[2]) < 0.5 for x in m), 'v2.73: the plugin moved the copies (slid away or tipped over: %s)' % m)
    check(any(float(x[2]) < 0.3 for x in m), 'v2.73: a tall one lies on its side 1 s later (upright %s)' % [x[2] for x in m])
    check(re.search(r'knocked over by the plugin \(GTA\'s physics didn\'t move them\)', log) is not None and
          any(int(x) > 0 for x in re.findall(r'v2\.73: (\d+) knocked over by the plugin', log)), 'v2.73: counted in the 10 s line')
    check('map prop' not in host or 'deleted' not in ''.join(l for l in host.splitlines() if 'map prop' in l), 'v2.73: no map prop deleted (the v2.72 crash)')
    mission = re.findall(r'entity (\d+) is a mission entity', host)
    check(not [h for h in mission if 9000 <= int(h) < 9100], 'v2.73: no map prop made a mission entity (%s)' % mission[:5])
    check("the map's own there: still there, VISIBLE" not in log, 'v2.73: the map\'s own props hidden')
elif mode == 'gap':
    f = [(int(a), int(b)) for a, b in re.findall(r'ground: v2\.73: (\d+) points filled in as holes on (\d+) tiles', log)]
    check(sum(a for a, b in f) > 100, 'v2.73: the strip\'s points filled in (%s)' % f)
    check("BeamNG's car is on GTA's ground (fell)" not in log and 'fell through' not in log, 'v2.73: the car drove over it, nothing fell through')
elif mode == 'burnout':
    st = re.findall(r'tyre smoke \(v2\.73\): wheel (\d+) slips ([\d.]+) m/s: GTA\'s core/wheel_burnout started', log)
    check(len(st) >= 2, 'v2.73: smoke started at the spinning wheels (%s)' % st[:4])
    check(re.search(r'particle fx wheel_burnout on entity 100 at [-\d.]+ [-\d.]+ [-\d.]+ scale [\d.]+: started', host) is not None, 'v2.73: on the hidden GTA car')
    off = re.findall(r'particle fx wheel_burnout on entity 100 at ([-\d.]+) ([-\d.]+) ([-\d.]+)', host)
    check(off and all(float(z) < 0.6 for x, y, z in off), 'v2.73: at the tyres\' feet (%s)' % off[:4])
    check(len(re.findall(r'particle fx \d+ stopped', host)) >= len(st), 'v2.73: stopped again (%d stops, %d starts)' % (len(re.findall(r'particle fx \d+ stopped', host)), len(st)))
elif mode == 'ped':
    check('entity 9002 is a mission entity' in host, 'v2.73: the hit person held (mission entity)')
    check('ped 9002 no longer needed' in host, 'v2.73: and given back to GTA')
elif mode == 'dents':
    dm = re.findall(r'GTA car (\d+) dented at [-\d.]+ [-\d.]+ [-\d.]+: wanted (\d+) mm, GTA shows (\d+) mm after (\d+) steps \(damage (\d+)\)', log)
    check(dm and all(int(s) <= 10 and int(d) <= 110 + 0.3 * int(w) + 15 for c, w, g, s, d in dm), 'v2.73: capped (%s)' % dm[:4])
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
sys.exit(1 if fails else 0)
