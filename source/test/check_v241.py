#!/usr/bin/env python3
"""check_v241.py - v2.41 end-to-end checks after a run (in test/run):
  alttab: sh ../run_e2e.sh script_alttab.txt 90 FAKE_RAYCAP=12 FAKE_COLLWAIT=20   (GTA paused 7 s while driving)
  fast:   sh ../run_e2e.sh script_fast.txt 100 FAKE_RAYCAP=12 FAKE_VMAX=30 FAKE_ACC=5   (108 km/h, ~10 rays a frame)
  radio:  sh ../run_e2e.sh script_world.txt 90 FAKE_RAYCAP=12 FAKE_RADIO=1 FAKE_PHONESTARTS=1
  radiooff: sh ../run_e2e.sh script_world.txt 90 FAKE_RAYCAP=12 FAKE_RADIO=255 FAKE_PHONESTARTS=1
  disk:   two runs of script_world.txt (FAKE_ROUGH=1): the second one reads the saved map
  ring:   sh ../run_e2e.sh script_ring.txt 90 FAKE_RAYCAP=12 FAKE_ROUGH=1   (stands 20 s, then drives)
Usage: python3 ../check_v241.py <alttab|fast|radio|radiooff|disk|ring>"""
import os, re, sys
mode = sys.argv[1]
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
log = open('GtaBeamD1.log', errors='replace').read()
mlog = open('gtabridge/gtabridge_log.txt', errors='replace').read() if os.path.exists('gtabridge/gtabridge_log.txt') else ''
host = open('host_out.txt', errors='replace').read()
check('Drive ON on GTA\'s ground' in log, 'drive mode started on GTA\'s ground')
if mode == 'alttab':
    m = re.search(r'GTA didn\'t run for ([\d.]+) s .*?(\d+) hold message', log)
    check(m and float(m.group(1)) > 6.5 and int(m.group(2)) >= 10, 'GTA paused 7 s: the plugin noticed, hold messages went out (%s)' % (m.groups(),) if m else 'not found')
    check('GTA is paused or alt-tabbed (hold)' in mlog and 'GTA runs again' in mlog, 'the BeamNG mod held the drive and got controls again')
    check('no controls from GTA for' not in mlog.split('GTA runs again')[0].split('hold')[-1] and 'drive: ended (no controls' not in mlog, 'the BeamNG mod did not end the drive')
    check('no car data from BeamNG for 5 s' not in log, 'the plugin did not end the drive either')
    check('drive: stopped (drive key)' in log, 'the drive ended with F10 only')
    check('the car waits (frozen)' in log and re.search(r'back on GTA\'s ground \(drive key\): collision loaded after 0\.[1-9]', log), 'F10 off: waited (frozen) for GTA\'s collision, then landed')
elif mode == 'fast':
    v = [float(x) for x in re.findall(r'^[\d.]+,[-\d.]+,[-\d.]+,[-\d.]+,[-\d.]+,([\d.]+),', open('fake_beam_path.csv').read(), re.M)]
    check(v and max(v) > 24, 'the car got fast (%.1f m/s)' % (max(v) if v else 0))
    a = re.findall(r'road ahead above 50 km/h: worst ([\d.]+) s, average ([\d.]+) s \((\d+) samples', log)
    a = [(float(x), float(y), int(n)) for x, y, n in a if int(n) > 0]
    check(a and min(x for x, y, n in a) > 0.8, 'road built ahead at speed: worst %s s (needs > 0.8)' % [x for x, y, n in a])
    check('fell through' not in log, 'nothing fell through at speed')
    t = [float(x) for x in re.findall(r'\+\d+ finished \(([\d.]+) a second', log)]
    check(t and max(t) > 12, 'tiles a second at speed: %s (v2.4 on your PC: ~5)' % t)
    d = [int(x) for x in re.findall(r'\+(\d+) drafts sent', log)]
    check(d and max(d) > 0, 'v2.42: drafts went to BeamNG ahead of the car at speed (%s)' % d)
    tr = [float(x) for x in re.findall(r'([\d.]+) triangles a tile', log)]
    tr = [x for x in tr if x > 0]
    check(tr and min(tr) < 300, 'v2.42: fewer triangles a tile (merged flat cells, drafts): %s (512 before)' % tr)
    wb = re.findall(r'speed from tick to tick vs BeamNG\'s: ([\d.]+) km/h off on average, 95% within ([\d.]+)', log)
    wb = [(float(a), float(b)) for a, b in wb if float(a) > 0 or float(b) > 0]
    check(wb and max(b for a, b in wb) < 3.5, 'v2.42: the drawn car moves steadily at speed (95%% within %s km/h of BeamNG\'s speed; v2.43: < 3.5 since the fake BeamNG steps its physics with its real frame time)' % [b for a, b in wb])
elif mode == 'disk':
    # the second run over the same road: tiles from the saved map, far fewer rays
    fd = sum(int(x) for x in re.findall(r'\+(\d+) from the saved map', log))
    rd = sum(int(x) for x in re.findall(r'saved map: (\d+) tiles read', log))
    check(fd > 20 and rd > 20, 'the saved map: %d tiles read from disk, %d sent to BeamNG from it' % (rd, fd))
    # v2.43: the ring grows further out while standing (the near ground is saved already), so rays go to new ground: the
    # check is that the road driven before comes from the saved map, at the same level
    check(fd > 100, 'v2.43: the road driven before came from the saved map: %d tiles' % fd)
    lm = sum(int(x) for x in re.findall(r'(\d+) wanted tiles known only at another level', log))
    check(lm == 0, 'v2.43: no saved tile refused as another level on flat ground (%d)' % lm)
elif mode == 'ring':
    r = sum(int(x) for x in re.findall(r'(\d+) ring tiles done', log))
    check(r > 20, 'the ring around the standing car was measured with spare rays: %d tiles' % r)
    fc = sum(int(x) for x in re.findall(r'\+(\d+) from the cache', log))
    sw = sum(int(x) for x in re.findall(r'(\d+) sent while standing', log))
    check(fc + sw > 5, 'standing, then driving off: ring tiles went to BeamNG without rays: %d from the cache, %d while standing (v2.43)' % (fc, sw))
elif mode in ('radio', 'radiooff'):
    if mode == 'radio':
        check('as at F10' in log and 'FAKE: phone radio 1' in host, 'radio on at F10: the phone radio plays your station')
        check(re.search(r'GTA\'s radio at F10 off: RADIO_01', log) and 'FAKE: car radio RADIO_01_CLASS_ROCK' in host, 'F10 off: the car radio gets the station')
    else:
        check('it stays off (no phone radio)' in log and 'FAKE: phone radio 1' not in host, 'radio off at F10: no phone radio at all')
        check('FAKE: car radio OFF' in host, 'F10 off: the car radio is set OFF')
print()
print('V2.41 %s: ALL PASS' % mode if not fails else 'V2.41 %s: %d FAILED: %s' % (mode, len(fails), fails))
sys.exit(1 if fails else 0)
