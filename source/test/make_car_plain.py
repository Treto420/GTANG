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
WHEELS = ((0.8, 1.45, 'FR'), (-0.8, 1.45, 'FL'), (0.8, -1.4, 'RR'), (-0.8, -1.4, 'RL'))
for x, y, name in WHEELS:
    cyl('tyre_' + name, (x, y, 0.33), 0.33, 0.26)

L = lambda **k: k
materials = [
    ('body', dict(v=1.5, n=2, translucent='0', alphaTest='0', blendOp='LerpAlpha', cubemap='', layers=[
        L(baseColorFactor=[1, 1, 1, 1], metallicFactor=1, roughnessFactor=1, clearCoatFactor=0, opacityFactor=1),
        L(baseColorFactor=[1, 1, 1, 1], instanceDiffuse=True, paletteConst='R', opacityFactor=1, metallicFactor=1, roughnessFactor=1, clearCoatFactor=1)])),
    ('tyre', dict(v=1.5, n=1, translucent='0', alphaTest='0', layers=[L(baseColorFactor=[0.05, 0.05, 0.05, 1], metallicFactor=0, roughnessFactor=0.9)])),
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
gl = dict(asset=dict(version='2.0'), scene=0, scenes=[dict(nodes=[len(nodes) - 1])], nodes=nodes, meshes=meshes, materials=gmats,
          accessors=accessors, bufferViews=bufviews,
          buffers=[dict(byteLength=len(bin_), uri='data:application/octet-stream;base64,' + base64.b64encode(bytes(bin_)).decode())],
          extras=dict(gtaBridge=dict(version='2.0', vehicle='testcar', paint=dict(color=[0.72, 0.16, 0.16, 1.2]), wheels=wheels, vehPos=[0, 0, 0])))
json.dump(gl, open(out + '/car.gltf', 'w'))
print('wrote', out + '/car.gltf', sum(len(p[3]) for p in parts.values()) // 3, 'triangles')
