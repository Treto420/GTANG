#!/usr/bin/env python3
"""check_v274.py - v2.74 end-to-end checks (run in test/run after run_e2e.sh):
  show    : script_show.txt: F11, Ctrl+Shift+F11 parks the BeamNG car as a show car on GTA car 100 (still drawn with no car
            loaded), Ctrl+Shift+F11 again next to it takes it away (not drawn any more).
  gta     : script_world.txt FAKE_TRAFFIC=1 with TrafficMode=gta: the hidden car turns solid before the parked car is met, GTA's
            own crash is checked (the fake GTA doesn't push: the 2.7 push instead), the BeamNG car takes the hit.
  despawn : script_world.txt FAKE_TRAFFIC=1 with TrafficMode=despawn: the parked car is taken away when touched; no push.
  semi    : script_world.txt FAKE_TRAFFIC=1 FAKE_TRAFFIC_LONG=1 (twins mode): a 12.8 m vehicle gets no twin, GTA's own crash instead.
  smoke   : script_world.txt FAKE_BURNOUT=1: BeamNG sends each tyre's slip; smoke from it (the script burnout effect).
  calm    : script_world.txt (plain drive): slip stays low, no smoke.
  roll    : script_world.txt FAKE_ROLL=1 (hills): the level check kept tiles that meet the tile towards the car, nothing fell through."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'show'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host, ev = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt')
mlog = rd('gtabridge/gtabridge_log.txt')

def ppm(p):
    try:
        b = open(p, 'rb').read()
    except OSError:
        return None
    parts = b.split(b'\n', 3)
    w, h = map(int, parts[1].split())
    return w, h, parts[3]

if mode == 'show':
    check('show cars (v2.74): the BeamNG car parked on GTA car 100' in log, 'v2.74: Ctrl+Shift+F11 parked the car as a show car')
    check(re.search(r'show cars \(v2\.74\): show car 1 parked \(\d+ MB', log) is not None, 'v2.74: its GPU data moved into the show car (render thread)')
    check('show car 1 taken away' in log, 'v2.74: Ctrl+Shift+F11 next to it took it away')
    a, b = ppm('show_parked.ppm'), ppm('show_gone.ppm')
    if a and b and a[:2] == b[:2]:
        w, h = a[0], a[1]
        diff = 0
        for y in range(h // 4, 3 * h // 4, 4):
            for x in range(w // 4, 3 * w // 4, 4):
                i = (y * w + x) * 3
                if sum(abs(a[2][i + k] - b[2][i + k]) for k in range(3)) > 30: diff += 1
        check(diff > 200, 'v2.74: the show car was drawn while parked, gone after (%d sample pixels differ)' % diff)
    else:
        check(False, 'v2.74: screenshots show_parked.ppm / show_gone.ppm')
elif mode == 'gta':
    i_on = log.find('Drive ON on GTA')
    check('traffic twins: asked BeamNG' not in log, 'v2.74: TrafficMode=gta: no twins asked for')
    check(re.search(r'traffic \(v2\.74\): mode gta: [1-9]\d* times the hidden car was solid', log) is not None, 'v2.74: the hidden car turned solid before the meeting')
    check(re.search(r"GTA's own crash with GTA car 9001 at \d+ km/h", log) is not None, "v2.74: GTA's own crash checked 0.2 s after the touch")
    check('the 2.7 push and damage instead' in log and 'traffic car velocity set' in host, "v2.74: the fake GTA didn't push it: the 2.7 push instead")
    check('traffic: a GTA car hit the car' in mlog, 'the BeamNG car took the hit')
    check('fell through' not in log, 'nothing fell through')
elif mode == 'despawn':
    check('FAKE: vehicle 9001 deleted' in host and 'TrafficMode=despawn: GTA car 9001 touched: taken away' in log, 'v2.74: the touched car was taken away')
    check('traffic car velocity set' not in host, 'v2.74: no push')
    check('traffic: a GTA car hit the car' not in mlog, 'v2.74: BeamNG car not hit')
elif mode == 'semi':
    check(re.search(r'GTA car 9001 .*takes its place', log) is None, 'v2.74: no twin for the 12.8 m vehicle')
    check(re.search(r"GTA's own crash with GTA car 9001", log) is not None, "v2.74: GTA's own crash for it")
elif mode == 'smoke':
    st = re.findall(r"tyre smoke \(v2\.74\): wheel (\d+) slips ([\d.]+) m/s: GTA's (\S+) started", log)
    check(len(st) >= 2 and all(a.startswith('scr_recartheft/') for _, _, a in st), 'v2.74: smoke started from BeamNG\'s slip (%s)' % st[:4])
    sl = [float(x) for x in re.findall(r'the most a tyre slipped ([\d.]+) m/s \(BeamNG', log)]
    check(sl and 6.0 < max(sl) < 12.0, 'v2.74: BeamNG\'s slip ~8 m/s (the fake spins the wheels 8 m/s over the car\'s speed): %s' % sl)
elif mode == 'calm':
    # a plain drive (no burnout): BeamNG's slip stays low, no smoke (v2.73 saw 60 m/s and smoked all the time)
    sl = [float(x) for x in re.findall(r'the most a tyre slipped ([\d.]+) m/s \(BeamNG', log)]
    check(not sl or max(sl) < 3.0, 'v2.74: plain driving: slip under 3 m/s (%s)' % sl)
    check('tyre smoke (v2.74): wheel' not in log, 'v2.74: no smoke')
elif mode == 'roll':
    kept = [int(x) for x in re.findall(r'v2\.74: (\d+) checks kept a tile', log)]
    check(kept and max(kept) > 0, 'v2.74: tiles that meet the tile towards the car kept (%s)' % kept)
    check("fell through GTA's ground (" not in log, 'nothing fell through')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
sys.exit(1 if fails else 0)
