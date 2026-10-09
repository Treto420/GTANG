#!/usr/bin/env python3
"""lua_dents_test.py - v2.5 (D2): the car-side dents code (dents.VEH in gtaBridge.lua) in LuaJIT with a fake car.
Checks: the reference, moves of the whole car cancel out, a dent is sent in mm in the car's frame, small moves (< 1 cm)
are not, the wheels' nodes never are, ranges of 256 ids, the refNodes fallback frame, one error report per kind.
Usage: python3 lua_dents_test.py <gtaBridge.lua>"""
import math, re, sys
from lupa import luajit21 as lj

src = open(sys.argv[1] if len(sys.argv) > 1 else '../src/gtaBridge.lua', encoding='utf-8').read()
m1 = re.search(r"dents\.WHEELS = \[==\[(.*?)\]==\]", src, re.S)
m2 = re.search(r"dents\.VEH = dents\.WHEELS \.\. \[==\[(.*?)\]==\]", src, re.S)
assert m1 and m2, 'dents code not found'
VEH = m1.group(1) + m2.group(1)

fails = []
def check(ok, what):
    print(('PASS ' if ok else 'FAIL ') + what)
    if not ok: fails.append(what)

# the car: body nodes on a grid (car frame: x right, y forward, z up), 4 wheels (node1, node2, 6 tyre nodes each)
BODY = {}
cid = 0
WHEELS = []
for wx, wy in ((0.8, 1.45), (-0.8, 1.45), (0.8, -1.4), (-0.8, -1.4)):
    w = {'node1': cid, 'node2': cid + 1, 'radius': 0.33, 'tread': list(range(cid + 2, cid + 8)), 'c': (wx, wy, 0.33)}
    WHEELS.append(w)
    cid += 8
first_body = cid
for x in (-0.95, 0.0, 0.95):
    for y in (-2.3, -0.6, 0.0, 0.6, 2.3):
        for z in (0.4, 1.1):
            BODY[cid] = (x, y, z)
            cid += 1
# a few nodes with ids past 256 (a second range)
for k in range(5):
    BODY[300 + k] = (0.5, -2.0 + k * 0.9, 1.4)
# a body node inside a wheel (a hub node not in the wheel's lists): left out by the geometric test
BODY[290] = (0.85, 1.45, 0.30)
NN = len(BODY) + 8 * 4
state = {'yaw': 0.3, 'pitch': 0.0, 'roll': 0.0, 'spin': 0.0, 'dent': {}, 'nodir': False, 'boom': False}

def basis():
    y, p, r = state['yaw'], state['pitch'], state['roll']
    # forward/up from yaw, pitch, roll; right = f x u
    f = (math.cos(p) * math.cos(y), math.cos(p) * math.sin(y), math.sin(p))
    u0 = (-math.sin(p) * math.cos(y), -math.sin(p) * math.sin(y), math.cos(p))
    rr = (f[1] * u0[2] - f[2] * u0[1], f[2] * u0[0] - f[0] * u0[2], f[0] * u0[1] - f[1] * u0[0])
    u = tuple(u0[i] * math.cos(r) + rr[i] * math.sin(r) for i in range(3))
    right = (f[1] * u[2] - f[2] * u[1], f[2] * u[0] - f[0] * u[2], f[0] * u[1] - f[1] * u[0])
    return right, f, u

def local(c):
    if c in BODY:
        p = BODY[c]
        d = state['dent'].get(c, (0, 0, 0))
        return tuple(p[i] + d[i] for i in range(3))
    w = WHEELS[c // 8]
    k = c % 8
    cx, cy, cz = w['c']
    if k == 0: return (cx - 0.1, cy, cz)
    if k == 1: return (cx + 0.1, cy, cz)
    a = state['spin'] + (k - 2) * math.pi / 3
    return (cx, cy + 0.3 * math.cos(a), cz + 0.3 * math.sin(a))

def world(c):
    r, f, u = basis()
    l = local(c)
    return tuple(r[i] * l[0] + f[i] * l[1] + u[i] * l[2] for i in range(3))

L = lj.LuaRuntime(unpack_returned_tuples=True)
G = L.globals()
out = []
L.execute('obj = {} v = { data = { nodes = {}, wheels = {}, refNodes = {} } }')
for c in list(BODY) + [w['node1'] for w in WHEELS] + [w['node2'] for w in WHEELS] + [t for w in WHEELS for t in w['tread']]:
    L.execute('v.data.nodes[%d] = { cid = %d, name = "n%d" }' % (c, c, c))
for i, w in enumerate(WHEELS):
    L.execute('v.data.wheels[%d] = { name = "w%d", node1 = %d, node2 = "n%d", radius = %.2f, treadNodes = { %s } }'
              % (i, i, w['node1'], w['node2'], w['radius'], ','.join(str(t) for t in w['tread'][:4])))   # 2 tyre nodes not listed
# refNodes: ref at the middle bottom, back behind it, up above it
ref, back, up = [c for c, p in BODY.items() if p == (0.0, 0.0, 0.4)][0], [c for c, p in BODY.items() if p == (0.0, -0.6, 0.4)][0], [c for c, p in BODY.items() if p == (0.0, 0.0, 1.1)][0]
L.execute('v.data.refNodes[0] = { ref = %d, back = %d, up = %d }' % (ref, back, up))
def gnp(self, c):
    if state['boom']: raise RuntimeError('boom')
    p = world(int(c))
    return L.table_from({'x': p[0], 'y': p[1], 'z': p[2]})
def gdv(self):
    if state['nodir']: raise RuntimeError('no getDirectionVector here')
    f = basis()[1]; return L.table_from({'x': f[0], 'y': f[1], 'z': f[2]})
def gdu(self):
    if state['nodir']: raise RuntimeError('no getDirectionVectorUp here')
    u = basis()[2]; return L.table_from({'x': u[0], 'y': u[1], 'z': u[2]})
G.obj.getNodePosition = gnp
G.obj.getDirectionVector = gdv
G.obj.getDirectionVectorUp = gdu
G.obj.getID = lambda self: 42
G.obj.queueGameEngineLua = lambda self, s: out.append(s)

def take():
    r = list(out); out.clear(); return r
def parse(msgs):
    """dents(...) calls -> list of (nn, a, b, {cid: (x,y,z)}); dentsInfo -> list"""
    dm, info = [], []
    for s in msgs:
        m = re.search(r"gtaBridge\.dents\((\d+),(-?\d+),(\d+),(\d+),'([^']*)'\)", s)
        if m:
            ent = {}
            for e in filter(None, m.group(5).split(';')):
                c, x, y, z = map(int, e.split(','))
                ent[c] = (x, y, z)
            dm.append((int(m.group(2)), int(m.group(3)), int(m.group(4)), ent))
            continue
        m = re.search(r"gtaBridge\.dentsInfo\((\d+), (-?\d+), (-?\d+), \"([^\"]*)\", \"([^\"]*)\"\)", s)
        if m: info.append((int(m.group(2)), int(m.group(3)), m.group(4), m.group(5)))
    return dm, info

L.execute(VEH)
L.execute("gtabDents.run('takeRef', false)")
dm, info = parse(take())
check(len(info) == 1 and info[0][2] == 'taken', 'reference taken (%s)' % (info,))
tracked = info[0][1] if info else -1
check(info and info[0][0] == NN, 'node count %d = %d' % (info[0][0] if info else -1, NN))
check(tracked == len(BODY) - 1, 'watched nodes %d = body nodes %d minus the one inside a wheel' % (tracked, len(BODY) - 1))
check('2 wheel nodes' not in (info[0][3] if info else ''), 'note: %s' % (info[0][3] if info else ''))
ranges = sorted((a, b) for nn, a, b, e in dm)
check(ranges == [(0, 256), (256, 512)], 'first send: every range once, empty: %s' % ranges)
check(all(not e for _, _, _, e in dm), 'nothing moved yet')

# the whole car moves and turns (and the wheels spin): nothing to send but the round-robin range
for step in range(5):
    state['yaw'] += 0.4; state['pitch'] = 0.1 * step; state['roll'] = -0.05 * step; state['spin'] += 1.3
    L.execute("gtabDents.run('tick')")
dm, info = parse(take())
check(len(dm) == 5 and all(not e for _, _, _, e in dm), 'whole car turned / wheels spun: %d messages, all empty' % len(dm))

# a dent: two nodes at the front left pushed in (car frame)
fl = [c for c, p in BODY.items() if p[0] == -0.95 and p[1] == 2.3]
state['dent'] = {fl[0]: (0.12, -0.15, 0.0), fl[1]: (0.06, -0.08, -0.02)}
small = [c for c, p in BODY.items() if p == (0.95, 0.0, 1.1)][0]
state['dent'][small] = (0.004, 0.0, 0.0)   # 4 mm: under 1 cm
L.execute("gtabDents.run('tick')")
dm, info = parse(take())
ent = {}
for _, a, b, e in dm: ent.update(e)
ok = fl[0] in ent and abs(ent[fl[0]][0] - 120) <= 1 and abs(ent[fl[0]][1] + 150) <= 1 and abs(ent[fl[0]][2]) <= 1
check(ok, 'dent 1 sent in mm in the car frame: %s' % (ent.get(fl[0]),))
ok = fl[1] in ent and abs(ent[fl[1]][0] - 60) <= 1 and abs(ent[fl[1]][1] + 80) <= 1 and abs(ent[fl[1]][2] + 20) <= 1
check(ok, 'dent 2: %s' % (ent.get(fl[1]),))
check(small not in ent, '4 mm move not sent')
check(all(c in BODY for c in ent), 'no wheel node sent')
check(all(a <= c < b for _, a, b, e in dm for c in e), 'nodes inside their ranges')

# same dent, car keeps turning: only the round-robin range (with the dent when it's that range)
for step in range(4):
    state['yaw'] -= 0.7; state['spin'] += 2.0
    L.execute("gtabDents.run('tick')")
dm, info = parse(take())
check(len(dm) == 4, 'steady dent: one range per tick (%d)' % len(dm))
seen = {}
for _, a, b, e in dm: seen.update(e)
check(all(abs(seen[c][0] - ent[c][0]) <= 3 for c in seen if c in ent), 'steady dent repeated the same')

# a node past 256 dented: its range goes
hi = 302
state['dent'][hi] = (0.0, 0.0, -0.05)
L.execute("gtabDents.run('tick')")
dm, info = parse(take())
got = [(a, b, e) for _, a, b, e in dm if hi in e]
check(len(got) == 1 and got[0][0] == 256 and abs(got[0][2][hi][2] + 50) <= 1, 'node %d in range 256-512: %s' % (hi, got))

# fallback frame from refNodes (no getDirectionVector in the car's Lua): a new dent reads the same
state['nodir'] = True
state['yaw'] += 1.0; state['roll'] = 0.2
nd = [c for c, p in BODY.items() if p == (0.95, -2.3, 1.1)][0]
state['dent'][nd] = (-0.07, 0.03, -0.04)
L.execute("gtabDents.run('tick')")
dm, info = parse(take())
check(not info, 'refNodes frame: no error (%s)' % (info,))
seen = {}
for _, a, b, e in dm: seen.update(e)
check(nd in seen and abs(seen[nd][0] + 70) <= 2 and abs(seen[nd][1] - 30) <= 2 and abs(seen[nd][2] + 40) <= 2, 'refNodes frame: new dent %s' % (seen.get(nd),))
state['nodir'] = False

# repaired: dents gone, the reference again: everything back
state['dent'] = {}
L.execute("gtabDents.run('takeRef', true)")
dm, info = parse(take())
check(info and info[0][2] == 'again', 'reference again (%s)' % (info,))
check(len(dm) == 2 and all(not e for _, _, _, e in dm), 'after the repair: all ranges, empty')
L.execute("gtabDents.run('takeRef', false)")
dm, info = parse(take())
check(info and info[0][2] == 'kept' and not dm, 'soft reference request keeps the old one')

# an error in the car's Lua: reported once
state['boom'] = True
for _ in range(3): L.execute("gtabDents.run('tick')")
dm, info = parse(take())
check(len(info) == 1 and info[0][2] == 'error' and 'boom' in info[0][3], 'error reported once: %s' % (info,))
state['boom'] = False

print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
