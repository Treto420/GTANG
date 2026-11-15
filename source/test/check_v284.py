#!/usr/bin/env python3
"""check_v284.py - v2.84 TruckProxies end-to-end checks (run in test/run after run_e2e.sh). FAKE GTA and FAKE BeamNG only: they test
the two sides' code paths. Whether BeamNG's truck box holds BeamNG's car, bends it and keeps GTA's truck is Treto's in-game test.
  truck   : PuppetSolid=1, TrafficProxies=4, TruckProxies=2, FAKE_TRAFFIC_LONG=1 (a 12.8 m semi parked across the way): 6 boxes
            asked for, the 2 truck boxes at their weight; the semi gets the 10.5 m box (the 8.2 m one is over 35% off); the fake
            BeamNG car hits it; the contact is logged as BeamNG's, as a truck; no GTA crash, no teleport; back to GTA, still there.
  car     : the same without FAKE_TRAFFIC_LONG (a 5 m car): it gets a car's box (1 to 4), never a truck's; logged as v2.83 does.
  oldjbeam: truck with FAKE_GM_MAX=5000 (the 2.83 jbeam's 5 t limit): the truck boxes come out 5000 kg and the log says LIGHTER.
  none    : TruckProxies=1 (only the 8.2 m box): no box within 35% of the 12.8 m semi: said once, GTA's own crash (v2.82).
  off     : TruckProxies=0 with the semi: no truck boxes asked for, no v2.84 proxy lines, GTA's own crash: exactly v2.83."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'truck'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host, ev, blog = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt'), rd('gtabridge/gtabridge_log.txt')
gtaCrash = "GTA's crash: a GTA car (9001" in log
ready = len(re.findall(r'proxy \(v2\.83\): box \d ready: a box:', log))
tk = re.findall(r'proxy \(v2\.84\): truck box (\d) ready: ([\d.]+) x ([\d.]+) x ([\d.]+) m, (\d+) kg in BeamNG \((\d+) asked\)(.*)', log)
check('GtaBeamD1 2.84 ' in log, 'the plugin is 2.84')
if mode in ('truck', 'car', 'oldjbeam'):
    check('settings: v2.84: TruckProxies 2 (truck-size boxes in BeamNG' in log, 'setting read')
    check('proxy (v2.83): asked BeamNG for 4 boxes' in log and 'proxy (v2.84): TruckProxies ON: 2 truck-size boxes asked for too (boxes 5 to 6) for GTA vehicles up to 16.0 m long' in log, '4 + 2 boxes asked for')
    check(re.search(r'"m":\["box:[^\]]*","box:2\.7:8\.2:2\.6:9000","box:3\.0:10\.5:2\.6:12000"\]', log) is not None, 'the truck boxes are the last two of the pool')
    check(ready == 6 and len(tk) == 2, '6 boxes ready, 2 of them truck boxes (%d, %d)' % (ready, len(tk)))
    check(len(re.findall(r'TWIN gtab_twin_\d spawned: a gtab_prop', ev)) == 6, "spawned as the mod's gtab_prop")
if mode in ('truck', 'car'):
    check(sorted((int(k), int(kg), int(ask)) for k, w, l, h, kg, ask, tail in tk) == [(5, 9000, 9000), (6, 12000, 12000)] and all('LIGHTER' not in t[6] for t in tk),
          'the truck boxes weigh what was asked in BeamNG (%s)' % ', '.join('%s: %s kg' % (t[0], t[4]) for t in tk))
    check(len(re.findall(r'proxy boxes \(v2\.84\): box [56] weighs (9000|12000) kg in BeamNG', blog)) == 2, "BeamNG's log: the truck boxes' weight")
if mode == 'truck':
    m = re.search(r'proxy \(v2\.84\): GTA truck 9001 \(class [\w -]+, 2\.6 x 12\.8 x 4\.0 m, \d+ km/h, no trailer\) meets the car in [\d.]+ s: truck box (\d) \(box:3\.0:10\.5:2\.6:12000, 12000 kg in BeamNG\)', log)
    check(m is not None and m.group(1) == '6', 'the semi got the 10.5 m truck box (not the 8.2 m one: over 35% off)')
    check('TWINHIT' in ev, "BeamNG's car hit the box in BeamNG")
    c = re.search(r"contact \(v2\.84\): a GTA truck \(9001, 12\.8 m long\) at \d+ km/h: resolved by BeamNG \(its box 6, a box:3\.0:10\.5.*the hidden car [\d.]+ m from BeamNG's car", log)
    check(c is not None, "the contact: a truck, resolved by BeamNG, the hidden car's distance")
    check('contact (v2.83): a GTA car (9001' not in log, 'not logged as a car as well')
    check(re.search(r'no collision (100 with 9001|9001 with 100)', host) is not None and not gtaCrash, 'the hidden car set not to collide with it; no GTA crash with it')
    check('PuppetSolid: snap' not in log, 'no teleport')
    check(re.search(r"proxy \(v2\.83\): GTA car 9001 back to GTA \([^)]*\) after [\d.]+ s: its box 6 was hit by BeamNG's car.*exists 1", log) is not None, 'back to GTA, hit, still there')
    check(re.search(r'proxy \(v2\.84\): GTA truck 9001 back to GTA after [\d.]+ s: exists 1, no trailer or cab with it', log) is not None, "the truck's own back-to-GTA line")
    check(re.search(r'proxy \(v2\.84\): 10 s: TruckProxies ON: 1 trucks / buses got a box, 1 hit by BeamNG\'s car, 0 met the car with no box free', log) is not None, '10 s line counts it')
    check('CONFLICT' not in log and 'THROWN' not in log, 'no conflict, not thrown')
elif mode == 'car':
    m = re.search(r'proxy \(v2\.83\): GTA car 9001 .*meets the car in [\d.]+ s: box (\d) \(box:([\d.]+):([\d.]+)', log)
    check(m is not None and int(m.group(1)) <= 4 and float(m.group(3)) < 6, "the car got a car's box (%s)" % (m.group(0)[-40:] if m else '-'))
    check('GTA truck' not in log, 'nothing logged as a truck')
    check(re.search(r"contact \(v2\.83\): a GTA car \(9001, [\d.]+ m long\) at \d+ km/h: resolved by BeamNG", log) is not None and not gtaCrash, 'the contact as v2.83: resolved by BeamNG')
    check(re.search(r'proxy \(v2\.84\): 10 s: TruckProxies ON: 0 trucks / buses got a box, 0 hit', log) is not None, '10 s truck line: none')
elif mode == 'oldjbeam':
    check(len(tk) == 2 and all(int(t[4]) == 5000 and 'LIGHTER than asked' in t[6] for t in tk), 'both truck boxes 5000 kg, said LIGHTER (%s)' % ', '.join(t[4] for t in tk))
elif mode == 'none':
    check('proxy (v2.84): TruckProxies ON: 1 truck-size boxes asked for too (boxes 5 to 5)' in log and ready == 5, '4 + 1 boxes')
    n = len(re.findall(r'proxy \(v2\.84\): GTA truck 9001 \(class [\w -]+, 2\.6 x 12\.8 m\) meets the car in [\d.]+ s but gets NO box: none of the \d free boxes is within 35% of its length', log))
    check(n >= 1, 'no box within 35%% of its length: said (%d)' % n)
    check('takes its place' not in log and gtaCrash, "no box given; GTA's own crash")
elif mode == 'off':
    check("settings: v2.84: TruckProxies 0 (v2.83: trucks and buses get GTA's own crash)" in log, 'setting read: off')
    check('proxy (v2.83): asked BeamNG for 4 boxes' in log and ready == 4, '4 boxes, as v2.83')
    check(len(re.findall(r'proxy \(v2\.84\)', log)) == 0 and 'contact (v2.84)' not in log, 'no v2.84 proxy or contact lines')
    check('takes its place' not in log and gtaCrash, "the semi gets no box: GTA's own crash, as v2.83")
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
sys.exit(1 if fails else 0)
