#!/usr/bin/env python3
"""check_v25.py - v2.5 end-to-end checks after a run (in test/run):
  walls:  sh ../run_e2e.sh script_walls.txt 90 FAKE_RAYCAP=12 FAKE_WALLS=1 [FAKE_TWOSIDED=1]   (barrier, 45-degree wall, building)
  deck2:  sh ../run_e2e.sh script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=2                  (up onto a 14 m deck, under a 22 m one)
  padcombo: sh ../run_e2e.sh script_padcombo.txt 80 FAKE_RAYCAP=12                           (RB + LB + D-pad down, also held across a switch)
  extras: sh ../run_e2e.sh script_extras.txt 70 FAKE_RAYCAP=12                               (gravity x1.1, drive modes on the gauge)
  props:  sh ../run_e2e.sh script_walls.txt 90 FAKE_RAYCAP=12 FAKE_PROPS=1                    (soft props on the road, a solid container)
  slowmo: sh ../run_e2e.sh script_slowmo.txt 90 FAKE_RAYCAP=12 FAKE_VMAX=15                              (GTA at 0.2 x speed for a while: radio wheel)
  dents:  sh ../run_e2e.sh script_dents.txt 200 FAKE_DENT_AFTER=15 FAKE_REPAIR_FILE=do_repair.txt "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"
          (BeamNG's car bent at the front left corner, then repaired; screenshots before / after / repaired)
Usage: python3 ../check_v25.py <mode>"""
import math, os, re, sys
mode = sys.argv[1]
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, mlog, ev, walls = rd('GtaBeamD1.log'), rd('gtabridge/gtabridge_log.txt'), rd('fake_beam_events.txt'), rd('fake_beam_walls.txt')
check('Drive ON on GTA\'s ground' in log, 'drive mode started on GTA\'s ground')
def ground(x, y): return 0.03 * x + 1.5 * math.sin(y / 25.0)
FX, FY, RX, RY = -0.34202, 0.93969, 0.93969, 0.34202
def path():
    out = []
    for l in open('fake_beam_path.csv'):
        r = l.strip().split(',')
        st, x, y, z = map(float, r[:4])
        if z < 150 or st < 15 or (len(r) > 8 and r[8] != 'D'): continue
        out.append((st, x, y, z))
    return out
boxes = []
for l in walls.splitlines():
    if l.startswith('BOX '):
        p = l.split()
        boxes.append(dict(cx=float(p[2]), cy=float(p[3]), z0=float(p[4]), z1=float(p[5]), len=float(p[6]), th=float(p[7]), ang=float(p[8]),
                          gs=float(p[9]) if len(p) > 9 else 0.0))
humps = [tuple(l.split()[1:]) for l in walls.splitlines() if l.startswith('HUMP ')]
high = [tuple(map(float, l.split()[1:])) for l in walls.splitlines() if l.startswith('HIGH ')]
falls = log.count('fell through GTA\'s ground (')
check(falls == 0, 'nothing fell through (%d)' % falls)

def in_box(b, x, y, m=0.05):
    a = math.radians(b['ang']); dx, dy = math.cos(a), math.sin(a)
    rx, ry = x - b['cx'], y - b['cy']
    al, ac = rx * dx + ry * dy, -rx * dy + ry * dx
    return abs(al) <= b['len'] / 2 + m and abs(ac) <= b['th'] / 2 + m

if mode == 'walls':
    P = path()
    smax = max((x * FX + y * FY) for st, x, y, z in P) if P else 0
    check(len(boxes) > 10, '%d wall boxes in BeamNG' % len(boxes))
    # the barrier: 6 m right of the car's line, 0.6 m wide, 0.9 m tall; covered every metre from s 25 to where the car got - 30
    miss, cov, bad = 0, 0, []
    for s10 in range(250, int((smax - 30) * 10), 10):
        s = s10 / 10.0
        x, y = FX * s + RX * 6.0, FY * s + RY * 6.0
        hit = [b for b in boxes if in_box(b, x, y)]
        cov += 1
        if not hit:
            miss += 1
            if len(bad) < 4: bad.append('s %.0f' % s)
            continue
        b = hit[0]
        a = math.radians(b['ang'])
        al = (x - b['cx']) * math.cos(a) + (y - b['cy']) * math.sin(a)
        top = b['z1'] + b['gs'] * al
        ok = abs(b['th'] - 0.6) < 0.2 and abs((top - 200 - ground(x, y)) - 0.9) < 0.2 and (abs(((b['ang'] - 110) + 90) % 180 - 90) < 6)
        if not ok and len(bad) < 4: bad.append('s %.0f box %s' % (s, b))
        miss += 0 if ok else 1
    check(cov > 20 and miss <= cov // 20, 'the barrier is a solid box along the road (0.6 m wide, 0.9 m tall): %d of %d metres right, car got to s %.0f %s' % (cov - miss, cov, smax, bad))
    # the 45-degree wall (2.5 m, 1 m thick): its middle third covered
    cx, cy = FX * 60 + RX * -12, FY * 60 + RY * -12
    a = math.atan2(FY, FX) + math.radians(45)
    pts = [(cx + math.cos(a) * t, cy + math.sin(a) * t) for t in [k * 0.5 for k in range(-8, 9)]]
    got = sum(1 for (x, y) in pts if any(in_box(b, x, y) and abs(b['th'] - 1.0) < 0.3 and abs(((b['ang'] - 155) + 90) % 180 - 90) < 8 for b in boxes))
    check(got >= len(pts) - 2, 'the wall at 45 degrees is a box along it (1 m thick): %d of %d points' % (got, len(pts)))
    # no humps: ground triangles more than 0.6 m high
    check(len(humps) == 0, 'no humps: ground triangles over 0.6 m high: %d %s' % (len(humps), humps[:4]))
    m = re.search(r'WALLS (\d+) of', walls)
    check(m and int(m.group(1)) == 0, 'no tall ground triangles (%s)' % (m.group(0) if m else '?'))
    # the car never climbed a wall: its path stays on the ground
    worst = max((abs(z - 200 - ground(x, y)) for st, x, y, z in P), default=99)
    check(worst < 0.6, 'BeamNG\'s car stays on the ground (worst %.2f m off)' % worst)
    sc = [int(x) for x in re.findall(r'walls \(v2.5\): (\d+) tiles scanned', log)]
    check(sum(sc) > 50, 'wall scans ran (%d tiles)' % sum(sc))

if mode == 'deck2':
    P = path()
    on8, dev, bad, ymax = 0, 0.0, [], 0.0
    for st, x, y, z in P:
        off = abs(x + 0.364 * y)
        h = 0
        if 90 < y < 230:
            if 130 <= y < 190: h = 14 if off < 18 else 0
            elif off <= 5: h = 14 * (y - 90) / 40 if y < 130 else 14 * (230 - y) / 40
        S = [200 + ground(x, y)] + ([200 + ground(x, y) + h] if h > 0 else [])
        s = min(S, key=lambda v: abs(v - z))
        d = abs(z - s)
        if d > dev: dev = d
        if d > 0.6 and len(bad) < 4: bad.append('t %.1f y %.0f z %.2f' % (st, y, z))
        if h == 14 and 150 < y < 162 and abs(z - S[1]) < 0.6: on8 += 1
        ymax = max(ymax, y)
    check(ymax > 200, 'the car got past the decks (y %.0f)' % ymax)
    check(on8 > 10, 'it drove on the 14 m deck under the 22 m one (%d frames)' % on8)
    check(dev < 0.6, 'BeamNG\'s car stays on the surface it should be on: worst %.2f m off %s' % (dev, bad))
    check(len(high) == 0, 'no ground at the 22 m deck\'s height in BeamNG (%d points %s)' % (len(high), high[:3]))
    m = re.search(r'WALLS (\d+) of (\d+) triangles in (\d+) tiles, (\d+) over 4 m', walls)
    check(m and int(m.group(4)) == 0, 'no big walls (%s)' % (m.group(0) if m else walls[:60]))

if mode == 'padcombo':
    sw = re.findall(r'drive: pad now in (GTA|BeamNG) mode', log)
    check(sw == ['GTA', 'BeamNG'], 'the D-pad switch: to GTA mode, then (RB + LB + D-pad down in GTA mode) back to BeamNG mode, no more (%s)' % sw)
    rep = len(re.findall(r'repair \(recover_vehicle\): the car repaired where it is', mlog))
    check(rep >= 2, 'RB + LB + D-pad down in BeamNG mode = your BeamNG repair, also with RB + LB held across the switch (%d repairs)' % rep)
    check('WRONG' not in ev, 'BeamNG\'s own reset never ran')

if mode == 'slowmo':
    resets = sum(int(x) for x in re.findall(r'frame clock resets (\d+)', log))
    slow = sum(float(x) for x in re.findall(r'GTA ran in slow motion for ([\d.]+) s', log))
    st = [float(x) for x in re.findall(r'the drawn car\'s speed from tick to tick vs BeamNG\'s: ([\d.]+) km/h off on average', log)]
    check(slow > 3, 'GTA\'s slow motion was seen (%.1f s)' % slow)
    check(resets <= 2, 'the car stays in step with BeamNG during GTA\'s slow motion: %d clock resets (v2.43: one every few frames)' % resets)
    check(st and max(st) < 3.5, 'steady: the drawn car\'s speed vs BeamNG\'s %s km/h off on average' % st)

if mode == 'extras':
    gl = re.findall(r'GRAVITY (-?[\d.]+)', ev)
    check(len(gl) >= 2 and abs(float(gl[0]) + 10.791) < 0.01 and abs(float(gl[-1]) + 9.81) < 0.01,
          'BeamNG\'s gravity x1.1 while GTA drives, back after (%s)' % gl)
    hud = rd('fake_hud.txt')
    seen = [m for m in ['COMFORT', 'SPORT', 'ESC & TC OFF'] if m in hud]
    check(len(seen) >= 2, 'the drive mode on GTA\'s gauge, switched with your binding (LB + D-pad up here): %s' % seen)
    check('drive mode now Sport' in mlog, 'the BeamNG mod logs the drive mode changes')

if mode == 'props':
    host = rd('host_out.txt')
    pushed = re.findall(r'FAKE: prop (\d+) pushed', host)
    check('9010' in pushed and '9011' in pushed, 'the bin and the lamp post on the road were knocked over in GTA (%s)' % pushed)
    check('9014' not in pushed and '9013' not in pushed, 'not the bin attached to something, not the can (%s)' % pushed)
    side = [p for p in pushed if int(p) < 9010]
    check(not side, 'the lamp posts beside the road were not touched (%s)' % side)
    bumps = re.findall(r'BUMP ([\d.]+) -> ([\d.]+) m/s', ev)
    check(len(bumps) >= 2 and all(float(a) > float(b) for a, b in bumps), 'BeamNG\'s car lost a little speed at each hit (%s)' % bumps)
    pb = re.findall(r'props: solid prop gtap_9012 is a box here \(([\d.]+) x ([\d.]+) x ([\d.]+) m\)', mlog)
    check(len(pb) >= 1 and abs(float(pb[0][0]) - 6.0) < 0.1 and abs(float(pb[0][1]) - 2.4) < 0.1, 'the container is a solid box in BeamNG (%s)' % pb)
    seen = rd('GtaBeamD1_props_seen.txt')
    check('0xA1110003' in seen and 'solid' in seen and '0xA1110004' in seen and 'tiny' in seen, 'GtaBeamD1_props_seen.txt lists the models met and why')
    check('props: BeamNG\'s car hit a soft prop' in log, 'the hits are in GtaBeamD1.log')

if mode == 'dents':
    m = re.search(r'car dents \(v2\.5\): (\d+) nodes .*?; (\d+) vertices follow them', log)
    check(m and int(m.group(1)) == 48 and int(m.group(2)) > 0, 'the car file\'s nodes were read and the body follows them (%s)' % (m.group(0) if m else 'no line'))
    check('dents: on' in mlog and 'dents: the car\'s reference taken' in mlog, 'BeamNG mod: dents on, the car\'s reference taken')
    check('dents: in the car' not in mlog and 'nothing from the car' not in mlog, 'no errors in the car\'s Lua, no restarts')
    check('dents: the first ones from BeamNG' in log, 'GTA got the dents')
    st = re.findall(r'dents: \d+ messages from BeamNG \((\d+) node moves, (\d+) unknown nodes, (\d+) bad, (\d+) for another car, \d+ lost\), (\d+) of 48 nodes moved now \(the most ([\d.]+) cm\), (\d+) turns fitted, \d+ updates, (\d+) copies', log)
    now = re.findall(r'dents: (?:now (\d+) of 48 nodes moved, the most ([\d.]+) cm|(none) now)', log)
    check(any(a == '4' and abs(float(b) - 43.0) < 1.0 for a, b, c in now), 'GTA had the 4 bent nodes, the most 43 cm (as BeamNG\'s): %s' % now)
    check(now and now[-1][2] == 'none', 'after the repair: no node moved any more')
    check(all(int(x[1]) == 0 and int(x[2]) == 0 and int(x[3]) == 0 for x in st), 'no unknown nodes, bad entries or messages for another car')
    check(any(int(x[6]) > 0 for x in st), 'turns fitted for the bent nodes (light on the bent panel)')
    check(st and int(st[-1][7]) >= 3, 'node data copied to the video card (%s times)' % (st[-1][7] if st else 0))
    try:
        from PIL import Image
        import numpy as np
        im = {n: np.asarray(Image.open(n + '.ppm')).astype(int) for n in ('dent_before', 'dent_after', 'dent_repaired')}
        def changed(a, b):
            d = np.abs(im[a] - im[b]).sum(2)[200:560, 380:800] > 60   # (the car's part of the picture)
            return int(d.sum())
        ba, br = changed('dent_before', 'dent_after'), changed('dent_before', 'dent_repaired')
        check(ba > 3000, 'the drawn car changed shape after the dent (%d pixels)' % ba)
        check(br < ba / 4, 'repaired: drawn as before again (%d pixels differ, dent %d)' % (br, ba))
    except Exception as e:
        check(False, 'screenshots: %s' % e)

print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
