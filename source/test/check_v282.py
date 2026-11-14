#!/usr/bin/env python3
"""check_v282.py - v2.82 PuppetPassCar end-to-end checks (run in test/run after run_e2e.sh). FAKE GTA only (fakeshv.cpp): they
test the plugin's code paths. Why the real GTA removes a car the hidden car is teleported into is a guess here (FAKE_TCVANISH);
only Treto's in-game test says whether PuppetPassCar keeps the cars.
  pass : PuppetPassCar=1, FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_PSKICK_MS=14 FAKE_TCVANISH=1: PuppetSnapM=0.5: a crash followed by a teleport: the
         pair is set not to collide before the teleport and every tick after, the car's line says STILL THERE.
  v281 : PuppetPassCar=0, the same: the native is never called for the pair, the fake removes the car, the line says GONE."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'pass'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host = rd('GtaBeamD1.log'), rd('host_out.txt')
line = re.search(r'hit car \(v2\.82\): a GTA car \(9001\).*', log)
line = line.group(0) if line else ''
check("GTA's crash: a GTA car (9001" in log and 'PuppetSolid: snap' in log, "the fake's crash, then a teleport")
check('resolved by GTA' in line and re.search(r'the hidden car [\d.]+ m from BeamNG\'s car now', line) is not None, "the car's line: resolved by GTA, the hidden car's distance")
if mode == 'pass':
    check('settings: v2.82: PuppetPassCar 1' in log, 'setting read')
    on = re.search(r'PuppetPassCar \(v2\.82\): ON for GTA car 9001: the hidden car is teleported ([\d.]+) m', log)
    check(on is not None and float(on.group(1)) > 0.5, 'ON line at the teleport')
    check('not colliding with it: it stays' in host and 'solid: 9001 removed' not in host, 'the pair was set not to collide before the teleport')
    n = re.search(r'no collision with that car for ([\d.]+) s', line)
    check(n is not None and float(n.group(1)) >= 1.0, 'no collision for 1 s at least (%s)' % (n.group(1) if n else '-'))
    check('STILL THERE' in line and 'GONE' not in line, 'the line says STILL THERE')
    check(re.search(r'PuppetPassCar ON: this session 1 teleports passed .* 1 still there 2\.5 s later, 0 gone', log) is not None, '10 s line counts it')
else:
    check('settings: v2.82: PuppetPassCar 0' in log, 'setting read as 0')
    check('PuppetPassCar (v2.82): ON' not in log, 'never on')
    check(not re.search(r'no collision (100 with 9001|9001 with 100)', host) and 'solid: 9001 removed' in host, 'the native never called for the pair; the fake removed the car')
    check('solid (PuppetPassCar off: v2.81)' in line and 'GONE' in line and 'ms after the teleport' in line, 'the line says GONE after the teleport')
print('ALL PASS' if not fails else '%d FAIL' % len(fails))
sys.exit(1 if fails else 0)
