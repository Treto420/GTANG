"""make_car_plain.py - a texture-free test car in the BeamNG mod's export format (v2.01 sandbox drive test).
Body + roof boxes, 4 tyres (FR FL RR RL, rear driven), the mod's extras (wheels, vehicle position, direction).
BeamNG world: x right, y forward, z up. glTF = (x, z, -y) as BeamNG's exporter writes it.
Usage: python3 make_car_plain.py <folder>   -> <folder>/car.gltf"""
import base64, json, math, os, sys
import numpy as np

out = sys.argv[1] if len(sys.argv) > 1 else 'car'
os.makedirs(out, exist_ok=True)

def g(p):
    return (p[0], p[2], -p[1])

parts = {}
def add(mat, verts, norms, uvs, tris):
    P = parts.setdefault(mat, [[], [], [], []])
    base = len(P[0])
    P[0] += verts; P[1] += norms; P[2] += uvs; P[3] += [base + t for t in tris]

def box(mat, c, s):
    cx, cy, cz = c; sx, sy, sz = s
    faces = [((1, 0, 0), (0, 1, 0), (0, 0, 1)), ((-1, 0, 0), (0, -1, 0), (0, 0, 1)), ((0, 1, 0), (-1, 0, 0), (0, 0, 1)),
             ((0, -1, 0), (1, 0, 0), (0, 0, 1)), ((0, 0, 1), (1, 0, 0), (0, 1, 0)), ((0, 0, -1), (1, 0, 0), (0, -1, 0))]
    for n, a, b in faces:
        ctr = [cx + n[0] * sx / 2, cy + n[1] * sy / 2, cz + n[2] * sz / 2]
        ha = abs(a[0]) * sx / 2 + abs(a[1]) * sy / 2 + abs(a[2]) * sz / 2
        hb = abs(b[0]) * sx / 2 + abs(b[1]) * sy / 2 + abs(b[2]) * sz / 2
        vs = [[ctr[k] + a[k] * ha * i + b[k] * hb * j for k in range(3)] for i, j in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        add(mat, vs, [list(n)] * 4, [(0, 0), (1, 0), (1, 1), (0, 1)], [0, 1, 2, 0, 2, 3])

def cyl(mat, c, r, w, seg=24):
    vs, ns, uv, tr = [], [], [], []
    for side in (-1, 1):
        base = len(vs)
        vs.append([c[0] + side * w / 2, c[1], c[2]]); ns.append([side, 0, 0]); uv.append((0.5, 0.5))
        for i in range(seg):
            a = 2 * math.pi * i / seg
            vs.append([c[0] + side * w / 2, c[1] + r * math.cos(a), c[2] + r * math.sin(a)]); ns.append([side, 0, 0]); uv.append((0, 0))
        for i in range(seg):
            tr += [base, base + 1 + i, base + 1 + (i + 1) % seg]
    base = len(vs)
    for i in range(seg + 1):
        a = 2 * math.pi * i / seg
        for side in (-1, 1):
            vs.append([c[0] + side * w / 2, c[1] + r * math.cos(a), c[2] + r * math.sin(a)]); ns.append([0, math.cos(a), math.sin(a)]); uv.append((0, 0))
    for i in range(seg):
        q = base + i * 2
        tr += [q, q + 1, q + 3, q, q + 3, q + 2]
    add(mat, vs, ns, uv, tr)

box('body', (0, 0, 0.75), (1.9, 4.6, 0.7))
box('body', (0, -0.3, 1.33), (1.6, 1.8, 0.5))
# v2.53 (locator_test.py): CAR_PARTS=1 adds a front bumper with its own 8 nodes (BeamNG: a flexbody whose group is the
# bumper's), a trim piece no flexbody names (all of the body's nodes) and a mirror (a prop on 3 nodes)
PARTS = os.environ.get('CAR_PARTS') == '1'
if PARTS:
    box('body_bumper', (0, 2.45, 0.55), (1.8, 0.2, 0.3))
    box('body_trim', (0, -2.35, 0.9), (1.2, 0.1, 0.1))
    box('body_mirror', (-1.0, 0.7, 1.05), (0.12, 0.06, 0.08))
    box('body_strut', (0.8, 1.45, 0.8), (0.1, 0.1, 0.24))   # v2.54: a flexbody on the wheel's nodes only: drawn as it is
    box('paintx_panel', (0.0, 0.0, 1.62), (0.4, 0.4, 0.02))   # v2.6: a palette map without the paint flag (your Z06)
# v2.6 (weapons, locator_test.py): CAR_GLASS=1 adds the left side window and a headlight lens, each breaking with its BeamNG
# deformGroup (the 2.6 mod's "dg": the window's broken material is invisible, the lamp's isn't)
GLASS = os.environ.get('CAR_GLASS') == '1'
if GLASS:
    box('glass_sidewindow', (-0.81, -0.3, 1.33), (0.02, 1.2, 0.36))
    box('glass_lamp', (-0.7, 2.31, 0.55), (0.4, 0.02, 0.15))   # v2.63: at the height of BeamNG's lamp beam (the shatter test)
    box('glass_door', (0.81, -0.3, 1.33), (0.02, 1.2, 0.36))   # v2.61: broken material not invisible, gone by its name
# v2.54 (load time): CAR_DENSE=N adds an N x N vertex sheet over the car on a 20 x 40 node grid of its own
DENSE = int(os.environ.get('CAR_DENSE', '0') or 0)
if DENSE:
    vs, tr = [], []
    for j in range(DENSE):
        for i in range(DENSE):
            x, y = -0.9 + 1.8 * i / (DENSE - 1), -2.2 + 4.4 * j / (DENSE - 1)
            vs.append([x, y, 1.15 + 0.05 * math.sin(x * 3) * math.cos(y * 2)])
    for j in range(DENSE - 1):
        for i in range(DENSE - 1):
            q = j * DENSE + i
            tr += [q, q + 1, q + DENSE + 1, q, q + DENSE + 1, q + DENSE]
    add('body_sheet', vs, [[0, 0, 1]] * len(vs), [(0, 0)] * len(vs), tr)
WHEELS = ((0.8, 1.45, 'FR'), (-0.8, 1.45, 'FL'), (0.8, -1.4, 'RR'), (-0.8, -1.4, 'RL'))
for x, y, name in WHEELS:
    cyl('tyre_' + name, (x, y, 0.33), 0.33, 0.26)

L = lambda **k: k
materials = [
    ('body', dict(v=1.5, n=2, translucent='0', alphaTest='0', blendOp='LerpAlpha', cubemap='', layers=[
        L(baseColorFactor=[1, 1, 1, 1], metallicFactor=1, roughnessFactor=1, clearCoatFactor=0, opacityFactor=1),
        L(baseColorFactor=[1, 1, 1, 1], instanceDiffuse=True, paletteConst='R', opacityFactor=1, metallicFactor=1, roughnessFactor=1, clearCoatFactor=1)])),
    ('tyre', dict(v=1.5, n=1, translucent='0', alphaTest='0', layers=[L(baseColorFactor=[0.05, 0.05, 0.05, 1], metallicFactor=0, roughnessFactor=0.9)])),
    ('glass', dict(v=1.5, n=1, translucent='1', alphaTest='0', blendOp='PreMulAlpha', cubemap='', layers=[L(baseColorFactor=[0.1, 0.12, 0.14, 0.3], metallicFactor=0, roughnessFactor=0.05, opacityFactor=1)])),
    ('paintx', dict(v=1.5, n=2, translucent='0', alphaTest='0', layers=[
        L(baseColorFactor=[1, 1, 1, 1], metallicFactor=1, roughnessFactor=1, opacityFactor=1),
        L(baseColorFactor=[1, 1, 1, 1], paletteConst='G', opacityFactor=1, metallicFactor=1, roughnessFactor=1, clearCoatFactor=1)])),
]
bin_ = bytearray(); bufviews = []; accessors = []; meshes = []; nodes = []; gmats = []
def view(data, target):
    off = len(bin_); bin_.extend(data); bin_.extend(b'\0' * ((4 - len(data) % 4) % 4))
    bufviews.append(dict(buffer=0, byteOffset=off, byteLength=len(data), target=target)); return len(bufviews) - 1
names = [m[0] for m in materials]
for name, ex in materials:
    gmats.append(dict(name=name, extras=dict(gta=ex)))
for pname, P in parts.items():
    mi = names.index(pname.split('_')[0])
    pos = np.array([g(p) for p in P[0]], np.float32); nrm = np.array([g(n) for n in P[1]], np.float32)
    uv = np.array(P[2], np.float32); idx = np.array(P[3], np.uint32)
    a0 = len(accessors)
    accessors.append(dict(bufferView=view(pos.tobytes(), 34962), componentType=5126, count=len(pos), type='VEC3', min=pos.min(0).tolist(), max=pos.max(0).tolist()))
    accessors.append(dict(bufferView=view(nrm.tobytes(), 34962), componentType=5126, count=len(nrm), type='VEC3'))
    accessors.append(dict(bufferView=view(uv.tobytes(), 34962), componentType=5126, count=len(uv), type='VEC2'))
    accessors.append(dict(bufferView=view(uv.tobytes(), 34962), componentType=5126, count=len(uv), type='VEC2'))
    accessors.append(dict(bufferView=view(idx.tobytes(), 34963), componentType=5125, count=len(idx), type='SCALAR'))
    meshes.append(dict(name=pname, primitives=[dict(attributes=dict(POSITION=a0, NORMAL=a0 + 1, TEXCOORD_0=a0 + 2, TEXCOORD_1=a0 + 3), indices=a0 + 4, material=mi)]))
    nodes.append(dict(name=pname, mesh=len(meshes) - 1))
nodes.append(dict(name='car', children=list(range(len(nodes))), extras=dict(BNG_Direction=dict(dir=list(g((0, 1, 0))), up=list(g((0, 0, 1)))))))
wheels = [dict(name=n, c1=list(g((x - 0.13, y, 0.33))), c2=list(g((x + 0.13, y, 0.33))), r=0.33, width=0.26, driven=n[0] == 'R') for x, y, n in WHEELS]
# v2.5 (dents): BeamNG's nodes as the 2.5 mod exports them: 3 per wheel (ids 0-11, as fake_beam_lua.py's car), then the body's
# (fake_beam_lua.py body_nodes()); node groups; the body mesh follows the 'body' group
def body_nodes():
    out = []
    for x in (-0.95, 0.0, 0.95):
        for y in (-2.3, -0.6, 0.0, 0.6, 2.3):
            for z in (0.4, 1.1):
                out.append((x, y, z))
    out += [(-0.95, 1.45, 1.1), (0.95, 1.45, 1.1), (-0.8, -1.2, 1.58), (0.8, -1.2, 1.58), (-0.8, 0.6, 1.58), (0.8, 0.6, 1.58)]
    return out
WH_FAKE = ((0.8, 1.45), (-0.8, 1.45), (0.8, -1.4), (-0.8, -1.4))   # fake_beam_lua.py's wheel order (FR FL RR RL)
npos = []
for x, y in WH_FAKE:
    npos += [(x - 0.1, y, 0.33), (x + 0.1, y, 0.33), (x, y + 0.2, 0.33)]
npos += body_nodes()
nbody = len(npos)
if DENSE:
    npos += [(-0.95 + 1.9 * i / 19, -2.3 + 4.6 * j / 39, 1.2) for j in range(40) for i in range(20)]
if PARTS:
    npos += [(x, y, z) for x in (-0.9, 0.9) for y in (2.35, 2.55) for z in (0.4, 0.7)]
nodes_ex = dict(c=list(range(len(npos))), p=[round(v, 3) for q in npos for v in g(q)], w=[1] * 12 + [0] * (len(npos) - 12))
# v2.53: each flexbody mesh -> its own nodes (the 2.53 BeamNG mod's "flex"), props -> their 3 nodes
flex = {'body': list(range(12, nbody))}
if DENSE:
    flex['body_sheet'] = list(range(len(npos) - 800, len(npos)))
props = {}
if PARTS:
    flex['body_bumper'] = list(range(nbody, len(npos)))
    flex['body_strut'] = [0, 1, 2]
    props['body_mirror'] = [12 + 2, 12 + 3, 12 + 5]   # nodes at x=-0.95: (y -0.6, z 0.4), (y -0.6, z 1.1), (y 0, z 1.1)
dg = {}
if GLASS:
    flex['glass_sidewindow'] = list(range(12, nbody))
    flex['glass_lamp'] = list(range(12, nbody))
    flex['glass_door'] = list(range(12, nbody))
    dg = {'glass_sidewindow': ['sidewindow_L_break', 'invis'], 'glass_lamp': ['headlight_L_break', 'lights_dmg'], 'glass_door': ['doorglass_R_break', 'z06_glass_dmg']}
NO_NODES = os.environ.get('CAR_NO_NODES') == '1'
gl = dict(asset=dict(version='2.0'), scene=0, scenes=[dict(nodes=[len(nodes) - 1])], nodes=nodes, meshes=meshes, materials=gmats,
          accessors=accessors, bufferViews=bufviews,
          buffers=[dict(byteLength=len(bin_), uri='data:application/octet-stream;base64,' + base64.b64encode(bytes(bin_)).decode())],
          extras=dict(gtaBridge=dict(version='2.5', vehicle='testcar', paint=dict(color=[0.72, 0.16, 0.16, 1.2]), wheels=wheels, vehPos=[0, 0, 0],
                                     **({} if NO_NODES else dict(nodes=nodes_ex, flex=flex, **({'props': props} if props else {}))),
                                     **({'dg': dg} if dg else {}))))
json.dump(gl, open(out + '/car.gltf', 'w'))
print('wrote', out + '/car.gltf', sum(len(p[3]) for p in parts.values()) // 3, 'triangles')
