#!/usr/bin/env python3
"""locator_test.py - v2.53: the dents' triangles of nodes (carmodel.cpp) with the shader's math (SkinPos / SkinNrm in
GtaBeamD1.cpp, copied below). A test car with a bumper on its own 8 nodes, a trim piece no flexbody names and a mirror
prop on 3 nodes. Checks: who uses which nodes, nothing moves at rest, a whole-car turn moves every vertex rigidly, a
bumper that came off goes with its nodes as one piece while the body stays, a dent bends smoothly.
Usage: python3 locator_test.py   (needs ziglang; builds into ./loc)"""
import math, os, re, subprocess, sys
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, '..', 'src')
D = os.path.join(HERE, 'loc')
os.makedirs(D, exist_ok=True)
fails = []
def check(ok, what):
    print(('PASS ' if ok else 'FAIL ') + what)
    if not ok: fails.append(what)

subprocess.run([sys.executable, os.path.join(HERE, 'make_car_plain.py'), D], check=True, env=dict(os.environ, CAR_PARTS='1', CAR_GLASS='1'), stdout=subprocess.DEVNULL)
exe = os.path.join(D, 'locator_test')
subprocess.run([sys.executable, '-m', 'ziglang', 'c++', '-std=c++17', '-O1', '-I' + SRC, os.path.join(HERE, 'locator_test.cpp'), os.path.join(SRC, 'carmodel.cpp'), '-o', exe], check=True)
r = subprocess.run([exe, os.path.join(D, 'car.gltf'), os.path.join(D, 'dump.txt')], capture_output=True, text=True)
check(r.returncode == 0, 'car loaded (%s)' % r.stderr[-300:])
note, nodes, V, G, DR, MT = '', [], [], {}, [], {}
for line in open(os.path.join(D, 'dump.txt')):
    t = line.split()
    if t[0] == 'note': note = line[5:].strip()
    elif t[0] == 'n': nodes.append((int(t[1]), float(t[2]), float(t[3]), float(t[4]), int(t[5])))
    elif t[0] == 'g': G[t[2]] = (int(t[1]), int(t[3]))
    elif t[0] == 'd': DR.append(tuple(int(x) for x in t[1:5]))
    elif t[0] == 'm': MT[(t[1], int(t[2]))] = int(t[3])
    elif t[0] == 'v': V.append([float(x) for x in t[1:7]] + [int(x) for x in t[7:11]] + [float(x) for x in t[11:14]])
print('note:', note)
NP = np.array([n[1:4] for n in nodes])
cid = [n[0] for n in nodes]
check('parts: 5 on their own flexbody\'s nodes, 1 props on 3 nodes, 1 ride with the wheels (drawn as they are), 2 on all of the body: body_trim, paintx_panel' in note,
      'parts: body + bumper + 2 glass on their own nodes, mirror a prop, hub with the wheels, trim + panel on all of the body')
m = re.search(r'outside their triangle ([\d.]+) on average', note)
check(m and float(m.group(1)) < 1.6, 'v2.54: vertices sit in or near their triangles (%s)' % (m.group(1) if m else '?'))
# design positions -> car space (the loader centres the car)
shift = NP[cid.index(12)] - np.array((-0.95, -2.3, 0.4))
check(np.allclose(NP[cid.index(47)] - shift, (0.8, 0.6, 1.58), atol=1e-3), 'car space = the design frame + a shift')
P = np.array([v[0:3] for v in V]) - shift
Nn = np.array([v[3:6] for v in V])
SI = np.array([v[6:10] for v in V], dtype=int)
SC = np.array([v[10:13] for v in V])
bumper = P[:, 1] > 2.34
trim = (P[:, 1] < -2.29) & (P[:, 2] > 0.84) & (P[:, 2] < 0.96)
mirror = (P[:, 0] < -0.93) & (P[:, 1] > 0.66) & (P[:, 1] < 0.74)
hub = (np.abs(P[:, 0] - 0.8) < 0.06) & (np.abs(P[:, 1] - 1.45) < 0.06) & (np.abs(P[:, 2] - 0.8) < 0.13)
wheel = (SI[:, 3] == 0) & ~hub
body = ~(bumper | trim | mirror | wheel | hub)
check(bumper.sum() == 24 and trim.sum() == 24 and mirror.sum() == 24, 'test parts found (%d, %d, %d vertices)' % (bumper.sum(), trim.sum(), mirror.sum()))
check(hub.sum() == 24 and np.all(SI[hub, 3] == 0), 'v2.54: a strut (a flexbody on the wheel\'s nodes) stays as drawn (%d vertices)' % hub.sum())
check(np.all(SI[~(wheel | hub | mirror), 3] == 1), 'every body vertex is in a triangle (%d of %d)' % ((SI[~(wheel | hub | mirror), 3] == 1).sum(), (~(wheel | hub | mirror)).sum()))
check(np.all(SI[mirror, 3] == 3), 'v2.55: the mirror (a prop) is carried rigidly by its 3 nodes')
bn = set(range(cid.index(48), len(cid)))
check(all(set(SI[i, :3]) <= bn for i in np.where(bumper)[0]), 'the bumper uses only its own nodes')
check(all(not (set(SI[i, :3]) & bn) for i in np.where(body)[0]), 'the body never uses the bumper\'s nodes')
check(all(set(SI[i, :3]) == {cid.index(14), cid.index(15), cid.index(17)} for i in np.where(mirror)[0]), 'the mirror uses its prop\'s 3 nodes')
check(np.abs(SC).max() < 50, 'coordinates stay small (largest %.1f)' % np.abs(SC).max())

def safen(v):
    return v / np.sqrt(np.maximum((v * v).sum(-1, keepdims=True), 1e-12))
def skin(mv):
    """SkinPos / SkinNrm of the shader; mv: per node how far it moved (car space)"""
    k = SI[:, 3]
    r0, x0, y0 = NP[SI[:, 0]], NP[SI[:, 1]] - NP[SI[:, 0]], NP[SI[:, 2]] - NP[SI[:, 0]]
    m0 = mv[SI[:, 0]]
    dx, dy = mv[SI[:, 1]] - m0, mv[SI[:, 2]] - m0
    pos = P + shift + m0 + SC[:, :1] * dx + SC[:, 1:2] * dy + SC[:, 2:3] * (safen(np.cross(x0 + dx, y0 + dy)) - safen(np.cross(x0, y0)))
    x1, y1 = x0 + dx, y0 + dy
    e1, e3 = safen(x0), safen(np.cross(x0, y0)); e2 = np.cross(e3, e1)
    f1, f3 = safen(x1), safen(np.cross(x1, y1)); f2 = np.cross(f3, f1)
    turn = lambda v: f1 * (v * e1).sum(-1, keepdims=True) + f2 * (v * e2).sum(-1, keepdims=True) + f3 * (v * e3).sum(-1, keepdims=True)
    nrm = turn(Nn)
    rig = r0 + m0 + turn(P + shift - r0)   # v2.55: si.w 3, carried rigidly
    pos = np.where((k == 0)[:, None], P + shift, np.where((k == 2)[:, None], P + shift + m0, np.where((k == 3)[:, None], rig, pos)))
    nrm = np.where(((k == 1) | (k == 3))[:, None], nrm, Nn)
    return pos - shift, nrm
def tornv(mv):
    """TornV of the shader: an edge of the vertex's triangle of nodes over 2.5x as long"""
    x0, y0 = NP[SI[:, 1]] - NP[SI[:, 0]], NP[SI[:, 2]] - NP[SI[:, 0]]
    x1, y1 = x0 + mv[SI[:, 1]] - mv[SI[:, 0]], y0 + mv[SI[:, 2]] - mv[SI[:, 0]]
    r = lambda a, b: (b * b).sum(-1) / np.maximum((a * a).sum(-1), 1e-8)
    return (SI[:, 3] == 1) & (np.maximum(np.maximum(r(x0, x1), r(y0, y1)), r(x0 - y0, x1 - y1)) > 6.25)

def rot(axis, deg):
    a = np.array(axis, float); a /= np.linalg.norm(a); t = math.radians(deg)
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + math.sin(t) * K + (1 - math.cos(t)) * K @ K

zero = np.zeros_like(NP)
p0, n0 = skin(zero)
check(np.abs(p0 - P).max() < 1e-5 and np.abs(n0 - Nn).max() < 1e-5, 'nothing moved: the car as drawn (%.1e m)' % np.abs(p0 - P).max())
# the whole car turned 30 degrees about a tilted axis and moved: every vertex rigidly, normals turned
R = rot((0.3, 0.2, 1.0), 30); t = np.array((0.4, -1.2, 0.3))
NPd = NP - shift
mv = (NPd @ R.T + t) - NPd
p1, n1 = skin(mv)
want = P @ R.T + t
err = np.abs(p1 - want)[~(wheel | hub)].max()
check(err < 1e-4, 'whole car turned: every vertex rigidly with it (%.1e m)' % err)
check(np.abs(n1 - Nn @ R.T)[~(wheel | hub)].max() < 1e-4, 'whole car turned: normals turned with it')
# the bumper came off: its 8 nodes 3 m back and to the left, turned 70 degrees, lying on the road; the body stays
Rb = rot((0.2, 1.0, 0.1), 70); c = NPd[sorted(bn)].mean(0)
mv = zero.copy()
for i in bn: mv[i] = (NPd[i] - c) @ Rb.T + c + np.array((-1.5, -3.0, -0.4)) - NPd[i]
p2, n2 = skin(mv)
check(np.abs(p2 - ((P - c) @ Rb.T + c + np.array((-1.5, -3.0, -0.4))))[bumper].max() < 1e-4, 'bumper came off: it goes with its nodes as one piece')
check(np.abs(p2 - P)[body | mirror].max() < 1e-6, 'bumper came off: the body and the mirror stay (no stretched sheets)')
check(np.abs(p2 - P)[trim].max() < 1e-6, 'bumper came off: the trim (all of the body\'s nodes) stays')
# a dent: the front-left top node pushed back 10 cm and down 4 cm: vertices near it follow, nothing moves more than it
k = [i for i in range(len(cid)) if np.allclose(NPd[i], (-0.95, 2.3, 1.1), atol=1e-3)][0]
mv = zero.copy(); mv[k] = (0.0, -0.10, -0.04)
p3, _ = skin(mv)
d = np.linalg.norm(p3 - P, axis=1)
at = np.linalg.norm(P - NPd[k], axis=1) < 1e-3
check(at.any() and np.abs(d[at] - math.hypot(0.10, 0.04)).max() < 1e-4, 'dent: the vertex at the node moves exactly as it')
check(d.max() <= math.hypot(0.10, 0.04) * 1.6, 'dent: nothing flies off (most %.3f m)' % d.max())
check(np.abs(p3 - P)[np.linalg.norm(P - NPd[k], axis=1) > 1.8].max() < 1e-6, 'dent: far parts of the body stay')
# v2.54: 2 mm of wobble on every node (BeamNG's mm steps, its flex): the panels move about as much, not cm
rng = np.random.default_rng(1)
mv = rng.uniform(-0.002, 0.002, NP.shape)
p5, _ = skin(mv)
amp = np.linalg.norm(p5 - P, axis=1)[body | bumper].max() / 0.002 / math.sqrt(3)
check(amp < 2.5, 'v2.54: 2 mm node wobble moves vertices at most %.1fx as much' % amp)
# v2.55: the mirror's 3 nodes pushed 1 cm apart from each other (a bent door): it turns and moves with them but keeps its size
mv = zero.copy()
for q, d in zip((14, 15, 17), ((0.01, 0, 0), (0, 0.01, 0), (0, 0, -0.01))):
    mv[cid.index(q)] = d
p6, _ = skin(mv)
size0 = np.linalg.norm(P[mirror] - P[mirror].mean(0), axis=1)
size1 = np.linalg.norm(p6[mirror] - p6[mirror].mean(0), axis=1)
check(np.abs(size1 - size0).max() < 1e-4, 'v2.55: a prop keeps its size when its nodes shift (%.1e m)' % np.abs(size1 - size0).max())
# v2.55: a node flung 6 m away: the vertices whose triangle used it are flagged torn (their triangles aren't drawn), the rest not
k = cid.index(48)
mv = zero.copy(); mv[k] = (0.0, 6.0, 0.0)
tv = tornv(mv)
uses = np.any(SI[:, :3] == k, axis=1) & (SI[:, 3] == 1)
check(np.all(tv[uses]) and not np.any(tv[~uses]), 'v2.55: torn flags exactly on the vertices whose triangle used the flung node (%d)' % uses.sum())
check(not np.any(tornv(zero)), 'v2.55: nothing torn at rest')
# a triangle crushed flat (two nodes onto one): no NaN
mv = zero.copy(); mv[SI[0, 1]] = NPd[SI[0, 0]] - NPd[SI[0, 1]]
p4, n4 = skin(mv)
check(np.isfinite(p4).all() and np.isfinite(n4).all(), 'a crushed triangle: no NaN')
# v2.6: glass and lights break with their BeamNG deformGroup (the export's "dg"): the window is hidden, the lamp frosted
check(G.get('sidewindow_L_break', (0, -1))[1] == 1 and G.get('headlight_L_break', (0, -1))[1] == 0 and G.get('doorglass_R_break', (0, -1))[1] == 1 and len(G) == 3,
      'v2.61: 3 breakable groups: the windows gone when broken (one invisible in BeamNG, one by its name), the lamp cracked (%s)' % G)
withg = sorted(d[3] for d in DR if d[3] >= 0)
check(withg == sorted(g for g, h in G.values()), 'v2.6: exactly the 3 glass draws carry their group (%s)' % withg)
check('car glass/lights (v2.61): 3 breakable groups on 3 parts' in r.stderr and 'doorglass_R_break (gone)' in r.stderr and 'headlight_L_break (cracked)' in r.stderr, 'v2.61: the loader logs the groups')
# v2.6: a palette map with no paint flag (your Z06's paint) is painted; the normal paint layer still is
check(MT.get(('paintx', 1)) == 1 and MT.get(('body', 1)) == 1 and MT.get(('glass', 0)) == 0, 'v2.6: palette layer without the flag painted (%s)' % MT)
check("material 'paintx' layer 2 has a colour palette map but no paint flag: painted" in r.stderr, 'v2.6: and logged')
# v2.73: the front doors: a part named "door" on one side (the test car's right door glass) turns about its front outer edge
m = re.search(r'car doors \(v2.73\): right front door: 1 part\(s\) \(glass_door\), ([\d.]+) m long, hinge at ([-\d.]+) ([-\d.]+) m', r.stderr)
check(m and abs(float(m.group(1)) - 1.2) < 0.01 and float(m.group(2)) > 0.8 and 'left front door' not in r.stderr, 'v2.73: the right front door found, 1.2 m long, hinge on the outer edge (%s)' % (m.group(0) if m else r.stderr[-200:]))
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
