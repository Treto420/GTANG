#!/usr/bin/env python3
"""check_v285.py - v2.85 PropProxies end-to-end checks (run in test/run after run_e2e.sh). FAKE GTA and FAKE BeamNG only: they test
the two sides' code paths. Whether a tied box holds in BeamNG, whether its ties come along with a teleport, how GTA breaks its
pole and whether BeamNG's car bends is Treto's in-game test.
  pole  : PuppetSolid=1, TrafficProxies=4, TruckProxies=0, PropProxies=3, FAKE_PROPS=1 FAKE_POLEFALL=1 (lamp posts by the road, one
          ON it; the fake GTA breaks one the car reaches and tips it over): 3 pole boxes asked for after the 4 car boxes, tied; the
          lamp on the road gets one; the contact is logged (BeamNG = damage, GTA = the pole); GTA's break cuts the ties; the box is
          put back on the fallen pole; it goes back to the pool.
  noties: the same with FAKE_NOTIES=1 (a tied box falls after a teleport): the log says the ties DID NOT HOLD and no pole gets a
          box (poles as v2.84).
  off   : PropProxies=0: 4 boxes asked for, no v2.85 line but the setting: exactly v2.84."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'pole'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, ev, blog = rd('GtaBeamD1.log'), rd('fake_beam_events.txt'), rd('gtabridge/gtabridge_log.txt')
check('unloaded' in log or 'drive mode' in log, 'the plugin ran (%d log lines)' % log.count('\n'))
check(len(re.findall(r'proxy \(v2\.83\): box \d ready: a box:', log)) == 4, 'the 4 car boxes are ready as before')
asked = re.findall(r'asked BeamNG for 4 boxes \((.*)\)', log)
v285 = [l for l in log.splitlines() if 'v2.85' in l and 'settings:' not in l]
if mode == 'off':
    check('settings: v2.85: PropProxies 0' in log, 'PropProxies 0 in the settings')
    check(asked and 'pole:' not in asked[0], 'no pole box asked for (%s)' % (asked[:1],))
    check(not v285, 'no v2.85 line (%s)' % v285[:2])
    check('is TIED' not in ev, 'no tied box spawned in BeamNG')
else:
    check(asked and asked[0].count('pole:') == 3, '3 pole boxes asked for after the car boxes (%s)' % (asked[:1],))
    check('PropProxies ON: 3 pole boxes asked for too (boxes 5 to 7)' in log, 'the log says PropProxies is on (boxes 5 to 7)')
    check(ev.count('is TIED') == 3, 'BeamNG spawned 3 tied boxes (%d)' % ev.count('is TIED'))
    hang = re.findall(r'pole boxes \(v2\.85\): box (\d) is tied to the ground: .*?: (its ties \w+ \w+)', blog)
    rdy = re.findall(r'pole boxes \(v2\.85\): box (\d) ready: a (pole:[\d.:]+) \(([\d.]+) x ([\d.]+) x ([\d.]+) m, (\d+) kg in BeamNG\); its ties after a teleport: (\w+)', log)
    check(1 <= len(hang) <= 3 and len(rdy) == 3, 'the hang test ran (until its result is known) and GTA was told for all 3 boxes (%s; %s)' % (hang, [r[6] for r in rdy]))
    want = 'DID' if mode == 'noties' else 'HOLD'
    check(rdy and all(r[6] == want for r in rdy), 'ties after a teleport: %s (%s)' % (want, [r[6] for r in rdy]))
    check(rdy and all(abs(float(r[2]) - 0.3) < 0.05 and float(r[4]) in (9.5, 6.0) for r in rdy), 'the boxes measure 0.3 m thick, 9.5 or 6 m tall (%s)' % [(r[2], r[4]) for r in rdy])
    got = re.findall(r"pole boxes \(v2\.85\): GTA's pole (\d+) \(model 0x(\w+) .*?trunk ([\d.]+) x ([\d.]+) m.*?gets box (\d) \(a pole:[\d.:]+, (\w+)\)", log)
    if mode == 'noties':
        check(not got and 'contact (v2.85)' not in log and 'TIECUT' not in ev, 'no pole got a box, nothing cut (%s)' % got[:2])
    else:
        check(any(g[0] == '9011' for g in got), 'the lamp post on the road (9011) got a box (%s)' % got[:6])
        check(all(g[1].upper() in ('A1110001', 'A1110005') for g in got), 'only lamp posts and the traffic light got boxes, never the bin or the container (%s)' % sorted(set(g[1] for g in got)))
        check(all(float(g[2]) <= 0.8 for g in got), 'their trunks measured under 0.8 m')
        check(got and all(g[5] == 'tied' for g in got), 'the boxes are tied')
        con = re.findall(r"contact \(v2\.85\): a pole \((\d+), model 0x\w+, .*?your car's damage by BeamNG \(its box (\d).*?the pole's reaction by GTA.*?the hidden car ([\d.]+) m", log)
        check(any(c[0] == '9011' for c in con), 'the contact with it is logged: damage by BeamNG, the pole by GTA (%s)' % con[:3])
        brk = re.findall(r"pole \(v2\.85\): GTA's pole (\d+) BROKE .*?BeamNG told to cut box (\d)'s ties", log)
        check(any(b[0] == '9011' for b in brk), "GTA's pole broke: BeamNG told to cut the box's ties (%s)" % brk[:3])
        check('TIECUT' in ev and 'its ties were cut (12 of 12' in blog, 'BeamNG cut them and said so (%d)' % ev.count('TIECUT'))
        put = re.findall(r"pole \(v2\.85\): box (\d) put back on GTA's falling pole (\d+) \(they were ([\d.]+) m apart", log)
        check(len(put) >= 1, 'the loose box was put back on the fallen pole (%s)' % put[:3])
        check(not re.search(r'put back on GTA.s falling pole \d+ \(they were [\d.]+ m apart; the pole: [\d.]+ m/s, upright [-\d.]+, [012]\.\d m from', log), 'never while the pole was within 3 m of the car')
        check("back from GTA's pole 9011" in log, 'the box went back to the pool')
        check(re.search(r'pole \(v2\.85\): 10 s: PropProxies ON', log) is not None, 'the 10 s line is there')
        check("GTA's crash: a breakable prop (9011" not in log, "GTA's push from the boxed pole was not sent to BeamNG")
check('ERROR' not in blog.upper().replace('ERRORS', '') or 'pole' not in ''.join(l for l in blog.splitlines() if ' E ' in l), 'no pole error in the BeamNG log')
print('v2.85 %s: %s' % (mode, 'ALL PASS' if not fails else '%d FAILED' % len(fails)))
sys.exit(1 if fails else 0)
