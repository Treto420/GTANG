#!/usr/bin/env python3
"""check_v25.py - v2.5 end-to-end checks after a run (in test/run):
  walls:  sh ../run_e2e.sh script_walls.txt 90 FAKE_RAYCAP=12 FAKE_WALLS=1 [FAKE_TWOSIDED=1]   (barrier, 45-degree wall, building)
  deck2:  sh ../run_e2e.sh script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=2                  (up onto a 14 m deck, under a 22 m one)
  padcombo: sh ../run_e2e.sh script_padcombo.txt 80 FAKE_RAYCAP=12                           (RB + LB + D-pad down, also held across a switch)
  extras: sh ../run_e2e.sh script_extras.txt 70 FAKE_RAYCAP=12                               (gravity x1.1 set in the ini (v2.51 default 1.0), drive modes on the gauge)
  props:  sh ../run_e2e.sh script_props_fixed.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1             (v2.51, PropPhysics=0: a bin, a breakaway lamp post on the road, a traffic light arm over it, a container)
  physprops: sh ../run_e2e.sh script_props.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1                (v2.52: the same as physics props)
  slowmo: sh ../run_e2e.sh script_slowmo.txt 90 FAKE_RAYCAP=12 FAKE_VMAX=15                              (GTA at 0.2 x speed for a while: radio wheel)
  jump:   sh ../run_e2e.sh script_jump.txt 120 FAKE_RAYCAP=12 FAKE_VMAX=15 FAKE_JUMP=1 FAKE_SYNCUS=10       (v2.51: off a 10 m drop at 80 m)
  pits:   sh ../run_e2e.sh script_walls.txt 90 FAKE_RAYCAP=12 FAKE_PITS=1                    (v2.55: narrow pits in the road filled)
  crest:  sh ../run_e2e.sh script_walls.txt 90 FAKE_RAYCAP=12 FAKE_CREST=1                   (v2.54: a crest with a low joint: no wall)
  dents:  sh ../run_e2e.sh script_dents.txt 200 FAKE_DENT_AFTER=15 FAKE_REPAIR_FILE=do_repair.txt "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"
          (BeamNG's car bent at the front left corner, then repaired; screenshots before / after / repaired)
Usage: python3 ../check_v25.py <mode>"""
import os, math, os, re, sys
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

if mode == 'crest':
    # v2.54: over a 1.2 m crest with a 0.2 m joint across the road at its top: no wall box across the road there (2.53 made one
    # where the rays ran just over the road: an invisible wall driving onto a bridge)
    P = path()
    smax = max((x * FX + y * FY) for st, x, y, z in P) if P else 0
    check(smax > 73, 'the car got past the crest (s %.0f)' % smax)
    across = [b for b in boxes if any(in_box(b, FX * s + RX * o, FY * s + RY * o, 0.3) for s in (69.0, 70.0, 71.0) for o in (-3.0, -1.5, 0.0, 1.5, 3.0))]
    check(not across, 'no wall box across the road at the crest (%d: %s)' % (len(across), across[:2]))
    low = sum(int(x) for x in re.findall(r'(\d+) faces met too low over their foot', log))
    check(low > 0, 'v2.54: the joint was met and left out as too low (%d faces)' % low)

if mode == 'pits':
    # v2.55: 0.8 m deep holes, 0.6 m wide, in the road: lifted to the road around them (not holes in BeamNG's road)
    lifted = sum(int(x) for x in re.findall(r'(\d+) points lifted out of narrow pits', log))
    check(lifted > 20, 'v2.55: narrow pits filled (%d points)' % lifted)
    lowered = sum(int(x) for x in re.findall(r'(\d+) lowered from narrow spikes', log))
    check(lowered > 20, 'v2.55: narrow spikes (beams over the road) taken down (%d points)' % lowered)
    P0 = path()
    smax = max((x * FX + y * FY) for st, x, y, z in P0) if P0 else 0
    check(smax > 60, 'BeamNG\'s car drove on past the beams (s %.0f)' % smax)
    P = path()
    # (v2.7: from when it first moves: the sandbox's speed moves the start; standing at the spawn height before isn't on the road yet)
    mv = [l.strip().split(',') for l in open('fake_beam_path.csv')]
    t0 = next((float(r[0]) for r in mv if len(r) > 5 and float(r[0]) >= 15 and float(r[5]) > 0.1), 0.0) + 1.0
    P = [q for q in P if q[0] >= t0]
    worst = max((abs(z - 200 - ground(x, y)) for st, x, y, z in P), default=99)
    check(P and worst < 0.6, 'BeamNG\'s car stays on the road (worst %.2f m off)' % worst)

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
    check('9015' not in pushed and 'model 0xA1110005' not in ''.join(l for l in log.splitlines() if 'hit a' in l or 'broke a' in l),
          'v2.51: the traffic light whose arm is over the road: no hit when the car passes under its arm')
    bumps = re.findall(r'BUMP ([\d.]+) -> ([\d.]+) m/s', ev)
    check(len(bumps) >= 1 and all(float(a) > float(b) for a, b in bumps), 'the bin (soft): BeamNG\'s car lost a little speed (%s)' % bumps)
    # v2.51: the lamp post on the road is a breakaway box in BeamNG: the car hits it for real, loses speed, the box breaks, GTA knocks it over
    check('PROPHIT gtap_9011' in ev, 'v2.51: BeamNG\'s car hit the lamp post\'s box (PROPHIT)')
    br = re.findall(r'props: the car broke prop gtap_9011: it lost ([\d.]+) km/h against it \(breaks at ([\d.]+)\)', mlog)
    check(br and float(br[0][0]) >= float(br[0][1]), 'v2.51: it broke after the car lost its break speed against it (%s)' % br)
    check('BeamNG\'s car broke a breakaway prop (model 0xA1110001' in log, 'v2.51: GTA knocked it over when BeamNG said')
    tm = re.findall(r'props: model 0xA1110005 .*?what a car hits at 0\.6 m: (\d+) part\(s\), the biggest ([\d.]+) x ([\d.]+) m', log)
    check(tm and int(tm[0][0]) == 1 and float(tm[0][1]) < 0.8 and float(tm[0][2]) < 0.8, 'v2.51: the traffic light measured: only its pole at bumper height (%s)' % tm)
    pbx = [l.split() for l in walls.splitlines() if l.startswith('PROPBOX gtap_9015')]
    if pbx:
        cx, cy = float(pbx[-1][2]), float(pbx[-1][3])
        off = cx * RX + cy * RY
        check(abs(off - 4.5) < 0.6, 'v2.51: its box in BeamNG is at the pole, 4.5 m right of the road (%.2f m)' % off)
    pb = re.findall(r'props: solid prop gtap_9012 is 1 box\(es\) here \(the first ([\d.]+) x ([\d.]+) x ([\d.]+) m\)', mlog)
    check(len(pb) >= 1 and abs(float(pb[0][0]) - 6.0) < 0.5 and abs(float(pb[0][1]) - 2.4) < 0.5, 'the container is a solid box in BeamNG (%s)' % pb)
    seen = rd('GtaBeamD1_props_seen.txt')
    check('0xA1110003' in seen and 'solid' in seen and '0xA1110004' in seen and 'tiny' in seen and 'breakaway' in seen, 'GtaBeamD1_props_seen.txt lists the models met and why')
    check('props: BeamNG\'s car hit a soft prop' in log, 'the soft hits are in GtaBeamD1.log')

if mode == 'physprops':
    # v2.52: physics props (the default): the lamp post on the road and the bin are BeamNG boxes that tip over; GTA's copy them
    host = rd('host_out.txt')
    check('props: BeamNG spawns physics props' in log, 'v2.52: BeamNG confirmed it spawns physics props')
    sp = re.findall(r'PROPSPAWN gtab_prop_(\d+) anchored (\w+) ([\d.]+) x ([\d.]+) x ([\d.]+) m (\d+) kg', ev)
    d = {a: (b, float(c), float(e), float(f), int(g)) for a, b, c, e, f, g in sp}
    check('9011' in d and d['9011'][0].lower() == 'true' and d['9011'][3] > 3.0, 'v2.52: the lamp post on the road: a box tied to the ground, its trunk up to 6 m (%s)' % (d.get('9011'),))
    check('9010' in d and d['9010'][0].lower() == 'false', 'v2.52: the bin: a loose box (%s)' % (d.get('9010'),))
    check('9012' not in d and '9013' not in d and '9014' not in d, 'v2.52: not the container (solid), the can (tiny) or the attached bin (%s)' % sorted(d))
    check('PHYSHIT gtab_prop_9011' in ev and 'PHYSHIT gtab_prop_9010' in ev, 'v2.52: BeamNG\'s car hit both boxes')
    kn = re.findall(r"BeamNG's car knocked a (\w+) prop \(model 0x(\w+)", log)
    check(('tied', 'A1110001') in kn and ('loose', 'A1110002') in kn, 'v2.52: GTA told they were knocked (%s)' % kn)
    posed = re.findall(r'FAKE: prop (\d+) posed ([-\d.]+) ([-\d.]+) ([-\d.]+) \((\d+), frozen (\d)\)', host)
    p11 = [p for p in posed if p[0] == '9011']
    check(p11 and p11[-1][5] == '1', 'v2.52: GTA\'s lamp post copies the box, GTA\'s own physics off it (%s)' % (p11[-1:],))
    if p11:
        x, y = float(p11[-1][1]), float(p11[-1][2])
        s_al, off = x * FX + y * FY, x * RX + y * RY
        check(abs(off) < 1.5 and 80.4 < s_al < 82.5, 'v2.52: it went along the car\'s way (s %.2f, %.2f m off the line; it stood at s 80)' % (s_al, off))
    up = re.findall(r'FAKE: prop 9011 turned: up ([-\d.]+) ([-\d.]+) ([-\d.]+)', host)
    check(up and float(up[-1][2]) < 0.4, 'v2.52: and it is lying over in GTA (up %s)' % (up[-1:],))
    check(not any(p[0] < '9010' for p in posed), 'v2.52: the lamp posts beside the road stay (%s)' % sorted(set(p[0] for p in posed)))
    pushed = re.findall(r'FAKE: prop (\d+) pushed', host)
    check('9010' not in pushed and '9011' not in pushed, 'v2.52: GTA\'s own push not used on them (%s)' % pushed)
    check(re.search(r'props: solid prop gtap_9012 is 1 box', mlog) is not None, 'v2.52: the container is still a fixed solid box')

if mode == 'jump':
    P = path()
    tps = log.count('put its car on GTA\'s ground (fell)')
    check(tps == 0, 'v2.51: no reset after the drop (%d put back)' % tps)
    nf = sum(int(x) for x in re.findall(r'(\d+) times under the ground it left while still over GTA', log))
    check(nf > 0 or 'ground: landed after' in log, 'v2.51: under the ground it left, still over GTA\'s: not taken for a fall (%d), or the landing ground was there in time' % nf)
    check('ground: the car is in the air' in log, 'v2.51: GTA saw the car in the air and where it lands')
    smax = max((x * FX + y * FY) for st, x, y, z in P) if P else 0
    check(smax > 125, 'the car drove on after the drop (s %.0f)' % smax)
    low = [(st, x, y, z) for st, x, y, z in P if x * FX + y * FY > 110]
    dev = max((abs(z - 200 - (ground(x, y) - 10.0)) for st, x, y, z in low), default=99)
    check(low and dev < 0.6, 'BeamNG\'s car on the lower ground after landing (worst %.2f m off)' % dev)

if mode == 'dents':
    m = re.search(r'car dents \(v2\.5\d*\): (\d+) nodes .*?; (\d+) vertices follow them', log)
    check(m and int(m.group(1)) == 48 and int(m.group(2)) > 0, 'the car file\'s nodes were read and the body follows them (%s)' % (m.group(0) if m else 'no line'))
    check('dents: on' in mlog and 'dents: the car\'s reference taken' in mlog, 'BeamNG mod: dents on, the car\'s reference taken')
    check('dents: in the car' not in mlog and 'nothing from the car' not in mlog, 'no errors in the car\'s Lua, no restarts')
    check('dents: the first ones from BeamNG' in log, 'GTA got the dents')
    st = re.findall(r'dents: \d+ messages from BeamNG \((\d+) node moves, (\d+) unknown nodes, (\d+) bad, (\d+) for another car, \d+ lost\), (\d+) of 48 nodes moved now \(the most ([\d.]+) cm\), \d+ updates, (\d+) copies', log)
    now = re.findall(r'dents: (?:now (\d+) of 48 nodes moved, the most ([\d.]+) cm|(none) now)', log)
    far = os.environ.get('FAKE_DENT_FAR') == '1'
    if far:
        check(any(a == '5' and float(b) > 1600.0 for a, b, c in now), 'v2.54: GTA had the 5 moved nodes, one 16 m away: %s' % now[-3:])
    else:
        check(any(a == '4' and abs(float(b) - 43.0) < 1.0 for a, b, c in now), 'GTA had the 4 bent nodes, the most 43 cm (as BeamNG\'s): %s' % now)
    check(now and now[-1][2] == 'none', 'after the repair: no node moved any more')
    check(all(int(x[1]) == 0 and int(x[2]) == 0 and int(x[3]) == 0 for x in st), 'no unknown nodes, bad entries or messages for another car')
    check(st and int(st[-1][6]) >= 3, 'node data copied to the video card (%s times)' % (st[-1][6] if st else 0))
    # v2.53: the body sits in triangles of its own flexbody's nodes; v2.54: the best-placed ones (inside them)
    check(re.search(r'car dents \(v2\.55\): .*vertices follow them \(0 only move with one node; 0 carried rigidly \(props, far from their nodes\); \d+ at the same spot as another; outside their triangle 1\.00 on average, 1 = inside\); '
                    r'parts: 1 on their own flexbody\'s nodes, 0 props on 3 nodes, 0 ride with the wheels \(drawn as they are\), 0 on all of the body', log) is not None,
          'v2.54: the body on its flexbody\'s own nodes, every vertex inside its triangle')
    check('renderer: input layout FAILED' not in log and 'dents vertex buffer FAILED' not in log, 'v2.53: the 20-byte dents vertex layout is accepted')
    try:
        from PIL import Image
        import numpy as np
        im = {n: np.asarray(Image.open(n + '.ppm')).astype(int) for n in ('dent_before', 'dent_after', 'dent_repaired')}
        def changed(a, b):
            d = np.abs(im[a] - im[b]).sum(2)[200:560, 380:800] > 60   # (the car's part of the picture)
            return int(d.sum())
        ba, br = changed('dent_before', 'dent_after'), changed('dent_before', 'dent_repaired')
        check(ba > 3000, 'the drawn car changed shape after the dent (%d pixels)' % ba)
        if far:
            # v2.54: the triangles stretched between the car and the far node are not drawn (2.53 drew a sheet across the left
            # of the picture)
            a = im['dent_after'][:, :380]
            red = int(((a[:, :, 0] > 120) & (a[:, :, 0] > a[:, :, 1] + 60)).sum())
            check(red < 300, 'v2.54: no stretched sheet to the far node (%d car-red pixels left of the car)' % red)
        check(br < ba / 4, 'repaired: drawn as before again (%d pixels differ, dent %d)' % (br, ba))
    except Exception as e:
        check(False, 'screenshots: %s' % e)

print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
