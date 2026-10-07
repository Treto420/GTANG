#!/usr/bin/env python3
"""lua_world_test.py - v2.4 BeamNG mod test for T3 (GTA's ground) and the sound session mode, against BeamNG API stubs
written in Lua (LuaJIT via lupa, like BeamNG's GE Lua).
Checks: map (fixed mapping, epochs), the sub-mesh collision self-check (and the "split" fallback), "hf" height tiles
(vertices, heights, one sub-mesh per ground model, groundType materials, unknown ground models falling back), "rm",
the teleport ("tp": waits for the tiles, finds the rotation convention by reading the car back, corrects a position
offset and remembers both), old-epoch cleanup after a fresh start, the parking pad at drive end, "ws" stats, the
simulation clock in poses, damage + ground ids in "vs", surfaceTest(), and the sound session mode (switched once,
volume down while not driving, remembered across BeamNG starts, everything put back at unload).
Usage: python3 lua_world_test.py ../src/gtaBridge.lua"""
import json, math, sys
from lupa import luajit21 as lj

SRC = sys.argv[1] if len(sys.argv) > 1 else '../src/gtaBridge.lua'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)

class Clock: t = 1000.0
clock = Clock()

STUBS = r'''
OBJS, RELOADS, SUBMESH_FIRST_ONLY = {}, 0, false
local Obj = {}
Obj.__index = Obj
function createObject(cls) return setmetatable({ cls = cls, fields = {}, pos = { x = 0, y = 0, z = 0 }, alive = true }, Obj) end
function Obj:setPosition(p) self.pos = { x = p.x, y = p.y, z = p.z } end
function Obj:registerObject(name)
  if OBJS[name] and OBJS[name].alive then error('name in use: ' .. name) end
  self.name = name; OBJS[name] = self
end
function Obj:createMesh(t) self.meshes = t[1] end
function Obj:delete() self.alive = false; if self.name and OBJS[self.name] == self then OBJS[self.name] = nil end end
function Obj:setField(k, i, v) self.fields[k] = v end
function Obj:getField(k, i) return self.fields[k] or '' end
scenetree = { findObject = function(n) return OBJS[n] end, MissionGroup = { addObject = function(self, o) end } }
be = { reloadStaticCollision = function(self) RELOADS = RELOADS + 1 end }
vec3 = function(x, y, z) if type(x) == 'table' then return { x = x.x, y = x.y, z = x.z } end return { x = x, y = y, z = z } end
quat = function(x, y, z, w) return { x = x, y = y, z = z, w = w } end
-- straight-down rays against the live procedural meshes (the sub-mesh check, groundBelow)
function castRayStatic(p, d, maxd)
  local best = maxd
  for _, o in pairs(OBJS) do
    if o.cls == 'ProceduralMesh' and o.alive and o.meshes then
      for mi, mesh in ipairs(o.meshes) do
        if mi == 1 or not SUBMESH_FIRST_ONLY then
          local f = mesh.faces
          for k = 1, #f, 3 do
            local A, B, C = mesh.verts[f[k].v + 1], mesh.verts[f[k + 1].v + 1], mesh.verts[f[k + 2].v + 1]
            local px, py = p.x - o.pos.x, p.y - o.pos.y
            local den = (B.y - C.y) * (A.x - C.x) + (C.x - B.x) * (A.y - C.y)
            if math.abs(den) > 1e-12 then
              local a = ((B.y - C.y) * (px - C.x) + (C.x - B.x) * (py - C.y)) / den
              local b = ((C.y - A.y) * (px - C.x) + (A.x - C.x) * (py - C.y)) / den
              local c = 1 - a - b
              if a >= -1e-6 and b >= -1e-6 and c >= -1e-6 then
                local z = a * A.z + b * B.z + c * C.z + o.pos.z
                local dist = p.z - z
                if dist >= 0 and dist < best then best = dist end
              end
            end
          end
        end
      end
    end
  end
  return best
end
-- quaternions (x, y, z, w); Torque-like quatFromDir: the rotation taking +Y to dir, +Z towards up
local function cross(a, b) return { x = a.y * b.z - a.z * b.y, y = a.z * b.x - a.x * b.z, z = a.x * b.y - a.y * b.x } end
local function nrm(a) local l = math.sqrt(a.x * a.x + a.y * a.y + a.z * a.z) return { x = a.x / l, y = a.y / l, z = a.z / l } end
function QROT(q, v)
  local t = cross(q, v); t = { x = 2 * t.x, y = 2 * t.y, z = 2 * t.z }
  local c = cross(q, t)
  return { x = v.x + q.w * t.x + c.x, y = v.y + q.w * t.y + c.y, z = v.z + q.w * t.z + c.z }
end
function quatFromDir(dir, up)
  local f = nrm(dir); local r = nrm(cross(f, up)); local u = cross(r, f)
  local m00, m01, m02, m10, m11, m12, m20, m21, m22 = r.x, f.x, u.x, r.y, f.y, u.y, r.z, f.z, u.z
  local tr = m00 + m11 + m22
  local q = {}
  if tr > 0 then local s = math.sqrt(tr + 1) * 2; q.w = 0.25 * s; q.x = (m21 - m12) / s; q.y = (m02 - m20) / s; q.z = (m10 - m01) / s
  elseif m00 > m11 and m00 > m22 then local s = math.sqrt(1 + m00 - m11 - m22) * 2; q.w = (m21 - m12) / s; q.x = 0.25 * s; q.y = (m01 + m10) / s; q.z = (m02 + m20) / s
  elseif m11 > m22 then local s = math.sqrt(1 + m11 - m00 - m22) * 2; q.w = (m02 - m20) / s; q.x = (m01 + m10) / s; q.y = 0.25 * s; q.z = (m12 + m21) / s
  else local s = math.sqrt(1 + m22 - m00 - m11) * 2; q.w = (m10 - m01) / s; q.x = (m02 + m20) / s; q.y = (m12 + m21) / s; q.z = 0.25 * s end
  return q
end
-- the car: BeamNG vehicles face -Y in their own frame; the teleport places the object origin, getPosition reads the
-- ref node 0.3 m right / 0.1 m ahead of it (so the mod must correct)
REF_OFF = { x = 0.3, y = -0.1, z = 0 }
VEH = { org = { x = 5, y = 7, z = 0.5 }, q = { x = 0, y = 0, z = 0, w = 1 }, tps = 0 }
function VEH:getID() return 42 end
function VEH:getJBeamFilename() return 'sdd_s650' end
function VEH:getPosition() local o = QROT(self.q, REF_OFF) return { x = self.org.x + o.x, y = self.org.y + o.y, z = self.org.z + o.z } end
function VEH:getDirectionVector() return QROT(self.q, { x = 0, y = -1, z = 0 }) end
function VEH:getDirectionVectorUp() return QROT(self.q, { x = 0, y = 0, z = 1 }) end
function VEH:getVelocity() return { x = 0, y = 0, z = 0 } end
function VEH:setPositionRotation(x, y, z, qx, qy, qz, qw) self.org = { x = x, y = y, z = z }; self.q = { x = qx, y = qy, z = qz, w = qw }; self.tps = self.tps + 1 end
function VEH:getNodePosition(c) local p = self:getPosition() return { x = p.x + (c % 3) * 0.1, y = p.y, z = p.z } end
function VEH:queueLuaCommand(code) VEH_CMDS[#VEH_CMDS + 1] = code end
VEH_CMDS = {}
getPlayerVehicle = function() return VEH end
core_vehicle_manager = { getPlayerVehicleData = function() return { vdata = { wheels = {}, nodes = {} } } end }
guihooks = { message = function(m) end }
FS = {}
function FS:findFiles(dir, pat) if FILES_IN then return FILES_IN(dir) end return {} end
function FS:fileExists(p) if FILES_HAS then return FILES_HAS(p) end return false end
function FS:getFileRealPath(p) return "C:/user" .. p end
GROUNDMODELS = { ASPHALT = {}, ASPHALT_WET = {}, DIRT = {}, DIRT_DUSTY = {}, GRAVEL = {}, GRASS = {}, SAND = {}, MUD = {}, ROCK = {}, METAL = {}, WOOD = {}, ICE = {}, SNOW = {} }
jsonReadFile = function(p)
  if p == 'art/groundmodels.json' then return GROUNDMODELS end
  if FILES_JSON then return FILES_JSON(p) end
  return nil
end
SETTINGS_VALUES = { AudioMuteOnWindowLoseFocus = true, AudioMasterVol = 0.8, AudioMusicVol = 0.5, fpsLimitBackgroundEnabled = true, fpsLimitBackground = 90 }
settings = { setValue = function(k, v) SETTINGS_VALUES[k] = v end, getValue = function(k) return SETTINGS_VALUES[k] end,
             impl = { defaultValues = { AudioMuteOnWindowLoseFocus = true, AudioMasterVol = 1, AudioMusicVol = 1, fpsLimitBackground = 30 } } }
extensions = {}
ActionMap = {}
function ActionMap:getList() return {} end
'''

def make_world(files=None, submesh_first_only=False):
    ge = lj.LuaRuntime(unpack_returned_tuples=True)
    logs, sent, inbox = [], [], []
    files = {} if files is None else files
    g = ge.globals()
    g.log = lambda lvl, tag, msg: logs.append(f'{lvl} {msg}')
    ge.execute(STUBS)
    if submesh_first_only: ge.execute('SUBMESH_FIRST_ONLY = true')
    def to_lua(o):
        if isinstance(o, dict): return ge.table_from({k: to_lua(v) for k, v in o.items()})
        if isinstance(o, list): return ge.table_from([to_lua(v) for v in o])
        return o
    def from_lua(t):
        if lj.lua_type(t) == 'table':
            keys = list(t.keys())
            if keys and all(isinstance(k, int) for k in keys) and sorted(keys) == list(range(1, len(keys) + 1)):
                return [from_lua(t[k]) for k in sorted(keys)]
            return {str(k): from_lua(t[k]) for k in keys}
        return t
    g.jsonDecode = lambda s: to_lua(json.loads(s))
    g.jsonEncode = lambda t: json.dumps(from_lua(t))
    g.readFile = lambda p: files.get(p)
    g.writeFile = lambda p, d: files.__setitem__(p, d) or True
    g.FILES_JSON = lambda p: to_lua(json.loads(files[p])) if files.get(p) else None
    g.FILES_HAS = lambda p: p in files
    g.FILES_IN = lambda d: ge.table_from([k for k in sorted(files) if k.startswith(d) and k.endswith('.json')])
    class Udp:
        def setsockname(self, ip, port): return 1
        def settimeout(self, t): return None
        def receivefrom(self, n):
            if inbox: return inbox.pop(0), '127.0.0.1', 55555
            return None, 'timeout', None
        def sendto(self, s, ip, port): sent.append(s); return len(s)
        def close(self): pass
    udp = Udp()
    g.socket = ge.table_from({})
    g.socket.udp = lambda: udp
    g.socket.gettime = lambda: clock.t
    M = ge.execute(open(SRC).read())
    g.gtaBridge = M
    M.onExtensionLoaded()
    return dict(ge=ge, M=M, logs=logs, sent=sent, inbox=inbox, files=files, g=g)

def frame(w, n=1):
    for _ in range(n):
        clock.t += 1 / 60
        w['M'].onUpdate(1 / 60, 1 / 60, 1 / 60)
        q = w['g'].VEH_CMDS
        w['ge'].execute('VEH_CMDS = {}')

def msgs(w, t):
    out = []
    for s in w['sent']:
        try: m = json.loads(s)
        except Exception: continue
        if m.get('t') == t: out.append(m)
    return out

def ev(w, code): return w['ge'].eval(code)

def tile_msg(tid, x0, y0, z0, n=17, st=0.5, slope=0.1, s=None, cells=None):
    z = []
    for j in range(n):
        for i in range(n):
            z.append(int(round((i * st * slope) * 100)))
    if cells is None: cells = '0' * ((n - 1) ** 2)
    return json.dumps({'t': 'hf', 'id': tid, 'x0': x0, 'y0': y0, 'z0': z0, 'st': st, 'n': n, 'z': z, 's': s or ['ASPHALT'], 'm': cells, 'e': 1}, separators=(',', ':'))

# ======================================================================== map, sub-mesh check, tiles
w = make_world()
w['inbox'].append('{"t":"map","ox":-500,"oy":1000,"zl":200,"fresh":1,"tile":8}')
frame(w)
mo = msgs(w, 'map_ok')
check(len(mo) == 1 and mo[0]['epoch'] == 1 and mo[0]['mode'] == 'submesh', 'map -> map_ok, epoch 1, sub-mesh check passes (%s)' % (mo and mo[0]))
check(any('ground: BeamNG has 13 ground models' in l for l in w['logs']), 'BeamNG\'s ground model list is read')
check(ev(w, 'OBJS.gtab_submesh_check') is None, 'the check quads are deleted again')
w2 = make_world(submesh_first_only=True)
w2['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1}')
frame(w2)
check(msgs(w2, 'map_ok')[0]['mode'] == 'split', 'only the first sub-mesh solid -> split mode')
# a tile with three surfaces: ASPHALT, DIRT_DUSTY|DIRT (exists), CONCRETE|ASPHALT (CONCRETE missing -> ASPHALT, merged)
n = 17
cells = ''.join('0' if i < 6 else ('1' if i < 12 else '2') for j in range(n - 1) for i in range(n - 1))
w['inbox'].append(tile_msg('g_10_20', 80.0, 160.0, 230.0, s=['ASPHALT', 'DIRT_DUSTY|DIRT', 'CONCRETE|ASPHALT'], cells=cells))
frame(w)
ok = msgs(w, 'hf_ok')
check(len(ok) == 1 and ok[0]['id'] == 'g_10_20', 'hf -> hf_ok (%s)' % ok)
obj = ev(w, 'OBJS.gtab_g_10_20')
check(obj is not None, 'tile object registered as gtab_g_10_20')
nsub = ev(w, '#OBJS.gtab_g_10_20.meshes')
mats = sorted(ev(w, 'OBJS.gtab_g_10_20.meshes[%d].material' % i) for i in range(1, nsub + 1))
check(nsub == 3 and mats == ['gtab_gm_ASPHALT', 'gtab_gm_CONCRETE_ASPHALT', 'gtab_gm_DIRT_DUSTY'], 'sub-meshes per ground model (v2.41: CONCRETE drives as ASPHALT but is its own grey sub-mesh): %d %s' % (nsub, mats))
check(ev(w, 'OBJS.gtab_gm_CONCRETE_ASPHALT.fields.groundType') == 'ASPHALT' and ev(w, 'OBJS.gtab_gm_CONCRETE_ASPHALT.fields.diffuseColor') == '0.55 0.55 0.52 1',
      'concrete without a CONCRETE ground model: grey, groundType ASPHALT')
tris = sum(ev(w, '#OBJS.gtab_g_10_20.meshes[%d].faces' % i) for i in range(1, nsub + 1)) // 3
check(tris == 2 * 16 * 16, '512 triangles (%d)' % tris)
check(ev(w, 'OBJS.gtab_gm_DIRT_DUSTY.fields.groundType') == 'DIRT_DUSTY' and ev(w, 'OBJS.gtab_gm_ASPHALT.fields.groundType') == 'ASPHALT',
      'surface materials carry groundType')
check(any('none of "CONCRETE|ASPHALT"' in l for l in w['logs']) is False and any('ground: material gtab_gm_DIRT_DUSTY made with groundType DIRT_DUSTY' in l for l in w['logs']),
      'material creation logged with its read-back')
h = ev(w, 'castRayStatic({x = 84.0, y = 162.0, z = 240.0}, {x=0,y=0,z=-1}, 50)')
check(abs((240.0 - h) - (230.0 + 4.0 * 0.1)) < 0.02, 'tile height at a point = z0 + slope (got %.3f)' % (240.0 - h))
# winding: BeamNG's front faces are clockwise: the first face (after the flip) seen from above must be clockwise
f = [ev(w, 'OBJS.gtab_g_10_20.meshes[1].faces[%d].v' % k) for k in (1, 2, 3)]
vx = [(ev(w, 'OBJS.gtab_g_10_20.meshes[1].verts[%d].x' % (v + 1)), ev(w, 'OBJS.gtab_g_10_20.meshes[1].verts[%d].y' % (v + 1))) for v in f]
area = (vx[1][0] - vx[0][0]) * (vx[2][1] - vx[0][1]) - (vx[1][1] - vx[0][1]) * (vx[2][0] - vx[0][0])
check(area < 0, 'triangles flipped to clockwise for BeamNG (signed area %.3f)' % area)
check(ev(w, 'OBJS.gtab_g_10_20.meshes[1].normals[1].z') > 0.9, 'normals point up')
# a broken tile -> hf_err, no crash
w['inbox'].append('{"t":"hf","id":"bad","n":17,"st":0.5,"x0":0,"y0":0,"z0":0,"z":[1,2,3],"s":["ASPHALT"],"m":""}')
frame(w)
check(len(msgs(w, 'hf_err')) == 1, 'a tile with missing heights -> hf_err')
# v2.4: a tile with points GTA has no ground for (-999999): the cells around them are left out
zp = []
for j in range(9):
    for i in range(9):
        zp.append(-999999 if (j >= 6) else 10)
w['inbox'].append(json.dumps({'t': 'hf', 'id': 'g_partial', 'x0': 200.0, 'y0': 200.0, 'z0': 230.0, 'st': 1.0, 'n': 9, 'z': zp, 's': ['ASPHALT'], 'm': '0' * 64}, separators=(',', ':')))
frame(w)
pt = sum(ev(w, '#OBJS.gtab_g_partial.meshes[%d].faces' % i) for i in range(1, ev(w, '#OBJS.gtab_g_partial.meshes') + 1)) // 3
check(pt == 2 * 8 * 5, 'partial tile: only the cells with ground on all 4 corners are built (%d triangles of 128)' % pt)
hp = ev(w, 'castRayStatic({x = 204.0, y = 202.0, z = 240.0}, {x=0,y=0,z=-1}, 50)')
hn = ev(w, 'castRayStatic({x = 204.0, y = 207.5, z = 240.0}, {x=0,y=0,z=-1}, 50)')
check(abs(240.0 - hp - 230.1) < 0.02 and hn >= 49.9, 'ground where GTA had it, nothing where it had none')
w['inbox'].append(json.dumps({'t': 'hf', 'id': 'g_empty', 'x0': 300.0, 'y0': 300.0, 'z0': 230.0, 'st': 1.0, 'n': 3, 'z': [-999999] * 9, 's': ['ASPHALT'], 'm': '0000'}, separators=(',', ':')))
frame(w)
check(any(m['id'] == 'g_empty' for m in msgs(w, 'hf_err')), 'a tile with no ground at all -> hf_err')
# rm: quiet removal
w['inbox'].append(tile_msg('g_11_20', 88.0, 160.0, 230.0))
frame(w)
nb = len(w['sent'])
w['inbox'].append('{"t":"rm","ids":["g_11_20"]}')
frame(w)
check(ev(w, 'OBJS.gtab_g_11_20') is None and not msgs(w, 'remove_ok'), 'rm removes a tile without answering')

# ======================================================================== teleport
# target: the middle of tile g_10_20, facing +x (east), level
w['inbox'].append(json.dumps({'t': 'tp', 'p': [84.0, 164.0, 230.4], 'f': [1, 0, 0], 'u': [0, 0, 1], 'o': [0, 0.2, -0.45], 'lift': 0.2, 'why': 'start'}))
frame(w, 40)
tp = msgs(w, 'tp_ok')
check(len(tp) == 1, 'tp -> tp_ok (%s)' % (tp or msgs(w, 'tp_err')))
fwd = ev(w, 'VEH:getDirectionVector()')
check(fwd and fwd['x'] > 0.999, 'the car faces the GTA car\'s way (forward %.3f %.3f %.3f)' % (fwd['x'], fwd['y'], fwd['z']))
check(tp and tp[0]['conv'] == 'quatFromDir(-forward)', 'convention found by reading back: %s' % (tp and tp[0]['conv']))
pos = ev(w, 'VEH:getPosition()')
# where the car-space origin X ends up: P + r*o + f*o + u*o - u*lift  (r = (0,-1,0) for forward +x)
X = (pos['x'] + 0 * 0, pos['y'] + 0, pos['z'] - 0.45 - 0.2)
check(tp and tp[0]['eh'] < 0.05, 'position offset corrected (%.3f m off)' % (tp and tp[0]['eh'] or -1))
check(abs(pos['x'] - (84.0 - 0.2)) < 0.05 and abs(pos['y'] - 164.0) < 0.05 and abs(pos['z'] - (230.4 + 0.45 + 0.2)) < 0.05,
      'ref node at target - origin offset + lift (%.2f %.2f %.2f)' % (pos['x'], pos['y'], pos['z']))
check(any('rotation convention that works' in l for l in w['logs']), 'the convention is logged')
tps_before = ev(w, 'VEH.tps')
w['inbox'].append(json.dumps({'t': 'tp', 'p': [83.0, 165.0, 230.3], 'f': [0, 1, 0], 'u': [0, 0, 1], 'o': [0, 0.2, -0.45], 'lift': 0.2, 'why': 'fell'}))
frame(w, 20)
tp2 = msgs(w, 'tp_ok')
check(len(tp2) == 2 and ev(w, 'VEH.tps') - tps_before == 1, 'second teleport: convention and offset remembered: 1 try (%d)' % (ev(w, 'VEH.tps') - tps_before))
fwd = ev(w, 'VEH:getDirectionVector()')
check(fwd['y'] > 0.999, 'second teleport faces north')
# a teleport waits for its tile: a request over nothing waits, then goes once the tile is built
w['inbox'].append(json.dumps({'t': 'tp', 'p': [500.0, 500.0, 230.0], 'f': [1, 0, 0], 'u': [0, 0, 1], 'o': [0, 0, 0], 'lift': 0.2, 'why': 'wait'}))
frame(w, 30)
check(len(msgs(w, 'tp_ok')) == 2, 'teleport waits while there is no tile under the target')
w['inbox'].append(tile_msg('g_62_62', 496.0, 496.0, 229.0))
frame(w, 10)
check(len(msgs(w, 'tp_ok')) == 3, 'teleport goes once the tile is there')

# ======================================================================== fresh start: old tiles go after the teleport
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":0}')   # new origin = fresh
frame(w)
mo = msgs(w, 'map_ok')[-1]
check(mo['epoch'] == 2, 'a new origin starts epoch 2 (%s)' % mo['epoch'])
check(ev(w, 'OBJS.gtab_g_10_20') is not None, 'old tiles stay until the car is on new ground')
w['inbox'].append(tile_msg('g_0_0', 0.0, 0.0, 230.0))
w['inbox'].append(json.dumps({'t': 'tp', 'p': [4.0, 4.0, 230.2], 'f': [1, 0, 0], 'u': [0, 0, 1], 'o': [0, 0, 0], 'lift': 0.2, 'why': 'start'}))
frame(w, 30)
check(len(msgs(w, 'tp_ok')) == 4, 'teleport on the new ground')
frame(w, 3)
check(ev(w, 'OBJS.gtab_g_10_20') is None and ev(w, 'OBJS.gtab_g_0_0') is not None, 'old-epoch tiles removed after the teleport, new ones stay')

# ======================================================================== drive: ws stats, ss, vs, parking pad
w['inbox'].append('{"t":"drive_start","v":"2.4","snd":0}')
frame(w)
check(len(msgs(w, 'drive_ok')) == 1, 'drive_ok')
for i in range(80):
    w['inbox'].append('{"t":"drive","q":%d,"thr":0,"brk":0,"str":0,"hb":0,"flt":1,"pad":"","keys":"","m":0,"p":0}' % i)
    frame(w)
ws = msgs(w, 'ws')
check(len(ws) >= 1 and 'fp95' in ws[-1] and ws[-1]['n'] >= 1 and ws[-1]['mode'] == 'submesh', 'ws stats once a second while driving (%s)' % (ws[-1] if ws else None))
poses = [json.loads(s) for s in w['sent'] if s.startswith('{"t":"pose"')]
check(poses and 'ss' in poses[-1] and abs(poses[-1]['ss'] - poses[-1]['st']) < 0.05, 'poses carry the simulation clock (ss %.3f, st %.3f)' % (poses[-1]['ss'], poses[-1]['st']))
w['M'].vstate(42, '0.000|0.000|0.000|0.000|0.000|0.000|0.000|1|900|0|2|true|arcade|0|7000|0|0|0|0|0|0|1234|10,10,12,12')
vs = msgs(w, 'vs')
check(vs and vs[-1]['dmg'] == 1234 and vs[-1]['gm'] == '10,10,12,12', 'vs carries damage and ground ids (%s)' % (vs[-1] if vs else None))
check(any('ground ids under the wheels now 10,10,12,12' in l for l in w['logs']), 'ground id change logged')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(ev(w, 'OBJS.gtab_surf_gtab_pad') is not None, 'drive end: parking pad built under the car')
pz = ev(w, 'OBJS.gtab_surf_gtab_pad.pos.z')
cx = ev(w, 'VEH:getPosition()')['x']
check(pz is not None and abs(pz - (230.0 + 0.1 * cx - 0.03)) < 0.02, 'pad 3 cm under the ground the car stands on (%.3f, ground %.3f)' % (pz or -1, 230.0 + 0.1 * cx))
w['inbox'].append(json.dumps({'t': 'tp', 'p': [4.0, 4.0, 230.2], 'f': [1, 0, 0], 'u': [0, 0, 1], 'o': [0, 0, 0], 'lift': 0.2, 'why': 'start'}))
frame(w, 30)
check(ev(w, 'OBJS.gtab_surf_gtab_pad') is None, 'the next teleport removes the pad')

# ======================================================================== surface test
w['M'].surfaceTest()
st = ev(w, 'OBJS.gtab_surface_test')
check(st is not None and ev(w, '#OBJS.gtab_surface_test.meshes') == 4, 'surfaceTest: 4 strips as 4 ground models')

# ======================================================================== sound session mode
files = {}
w = make_world(files=files)
w['inbox'].append('{"t":"drive_start","v":"2.4","snd":2,"sq":0}')
frame(w)
ok = msgs(w, 'drive_ok')[-1]
check(ok.get('sw') == 1, 'session asked while BeamNG mutes in the background: drive_ok asks GTA for one wake (sw)')
check(ev(w, 'SETTINGS_VALUES.AudioMuteOnWindowLoseFocus') is False and ev(w, 'SETTINGS_VALUES.AudioMasterVol') == 0.8, 'session on: option off, volume as it was while driving')
check(json.loads(files.get('/gtabridge/config.json', '{}')).get('soundSession') is True, 'session mode remembered in gtabridge/config.json')
check('AudioMuteOnWindowLoseFocus=boolean:true' in files.get('/gtabridge/session_restore.txt', ''), 'original value written before the change')
check(any('AudioMasterVol=0.8' in l for l in w['logs']), 'audio settings listed in the log (volume name check)')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(ev(w, 'SETTINGS_VALUES.AudioMuteOnWindowLoseFocus') is False and ev(w, 'SETTINGS_VALUES.AudioMasterVol') == 0, 'drive end: option stays off, BeamNG quiet')
w['inbox'].append('{"t":"drive_start","v":"2.4","snd":2,"sq":0}')
frame(w)
check('sw' not in msgs(w, 'drive_ok')[-1] and ev(w, 'SETTINGS_VALUES.AudioMasterVol') == 0.8, 'next drive: no wake needed, volume back to 0.8')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
w['M'].onExtensionUnloaded()
check(ev(w, 'SETTINGS_VALUES.AudioMuteOnWindowLoseFocus') is True and ev(w, 'SETTINGS_VALUES.AudioMasterVol') == 0.8, 'unload: both put back')
# the next BeamNG start (config remembered): session on at load, quiet until GTA drives
w3 = make_world(files=files)
check(ev(w3, 'SETTINGS_VALUES.AudioMuteOnWindowLoseFocus') is False and ev(w3, 'SETTINGS_VALUES.AudioMasterVol') == 0, 'BeamNG start: session mode applied at load, quiet')
# a crash with the session on: the start after puts the originals back first, then applies the session again
files2 = dict(files)
w4 = make_world(files=files2)
check(any('sound session: put back what an earlier run changed' in l for l in w4['logs']) or files2.get('/gtabridge/session_restore.txt'), 'crash restore path runs at load')
# switching back to bounce turns the session off and puts things back
w3['inbox'].append('{"t":"drive_start","v":"2.4","snd":1}')
frame(w3)
check(json.loads(files.get('/gtabridge/config.json', '{}')).get('soundSession') is False, 'snd 1 (bounce) switches the session mode off (remembered)')
check(ev(w3, 'SETTINGS_VALUES.AudioMasterVol') == 0.8, 'volume back after leaving session mode')

# ======================================================================== v2.41: hold (GTA paused / alt-tabbed)
w = make_world()
w['inbox'].append('{"t":"drive_start","v":"2.41","snd":0}')
frame(w)
check(len(msgs(w, 'drive_ok')) == 1, 'hold: drive_ok')
w['inbox'].append('{"t":"drive","q":1,"thr":0.5,"brk":0,"str":0,"hb":0,"flt":1,"pad":"","keys":"","m":0,"p":0}')
frame(w)
for i in range(12 * 60):   # 12 s of GTA paused: only "hold" twice a second
    if i % 30 == 0: w['inbox'].append('{"t":"hold","ms":%d}' % (500 + i * 16))
    frame(w)
check(not any('drive: ended' in l for l in w['logs']) and not msgs(w, 'drive_err'), 'hold: 12 s without controls, the drive stays (v2.4 ended it after 5 s)')
check(any('GTA is paused or alt-tabbed (hold)' in l for l in w['logs']), 'hold: logged once, the car brakes')
w['inbox'].append('{"t":"drive","q":2,"thr":0,"brk":0,"str":0,"hb":0,"flt":1,"pad":"","keys":"","m":0,"p":0}')
frame(w)
check(any('GTA runs again (24 hold messages' in l for l in w['logs']), 'hold: GTA back, controls again (%s)' % [l for l in w['logs'] if 'runs again' in l])
frame(w, 6 * 60)
check(any('no controls from GTA for' in l and 'ended' in l for l in w['logs']), 'without holds or controls the drive still ends (GTA closed)')

# ======================================================================== v2.41: CONCRETE from your ASPHALT
files = {}
w = make_world(files=files)
w['ge'].execute("GROUNDMODELS.ASPHALT = { staticFrictionCoefficient = 1.0, slidingFrictionCoefficient = 0.8, collisiontype = 'ASPHALT', aliases = { 'road' } }")
w['ge'].execute("getMissionFilename = function() return 'levels/smallgrid/main.level.json' end")
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
frame(w)
cf = json.loads(files.get('/levels/smallgrid/groundModels/gtabridge.json', '{}')).get('CONCRETE')
check(cf and abs(cf['staticFrictionCoefficient'] - 0.85) < 1e-6 and abs(cf['slidingFrictionCoefficient'] - 0.68) < 1e-6 and cf['collisiontype'] == 'ASPHALT' and 'aliases' not in cf,
      'CONCRETE written to the map\'s groundModels folder from ASPHALT x 0.85 (v2.42) (%s)' % cf)
check(any('load the map again once' in l for l in w['logs']), 'CONCRETE: you are told to load the map again once')
w['inbox'].append(tile_msg('g_1_1', 8.0, 8.0, 230.0, s=['CONCRETE|ASPHALT']))
frame(w)
check(ev(w, 'OBJS.gtab_g_1_1.meshes[1].material') == 'gtab_gm_CONCRETE_ASPHALT', 'until the map is loaded again: grey, drives as ASPHALT')
w['M'].onClientPostStartMission()
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":0,"tile":8}')
w['inbox'].append(tile_msg('g_1_2', 8.0, 16.0, 230.0, s=['CONCRETE|ASPHALT']))
frame(w, 2)
check(ev(w, 'OBJS.gtab_g_1_2.meshes[1].material') == 'gtab_gm_CONCRETE' and ev(w, 'OBJS.gtab_gm_CONCRETE.fields.groundType') == 'CONCRETE',
      'after the map is loaded again: CONCRETE is a ground model of its own (%s)' % ev(w, 'OBJS.gtab_g_1_2.meshes[1].material'))
check(any('BeamNG has it' in str(x) for x in [ev(w, 'gtaBridge.concreteNote()')]), 'concrete note: BeamNG has it (%s)' % ev(w, 'gtaBridge.concreteNote()'))
check(msgs(w, 'map_ok')[-1]['n'] == 0, 'after a map load the mod has no tiles of the old map: GTA sends them again (map_ok n %s)' % msgs(w, 'map_ok')[-1]['n'])
lines = w['M'].world()
check(any('concrete (GTA sidewalks): BeamNG has it' in l for l in lines.values()), 'gtaBridge.world() shows the concrete line')

# v2.42: a CONCRETE written by 2.41 (grip 0.97) is written again with 0.85; the old one is used until the map loads again
files2 = {'/levels/smallgrid/groundModels/gtabridge.json': json.dumps({'CONCRETE': {'staticFrictionCoefficient': 0.97, 'slidingFrictionCoefficient': 0.776, 'collisiontype': 'ASPHALT'}}),
          '/gtabridge/config.json': json.dumps({'concreteGrip': 0.97, 'soundSession': False})}
w = make_world(files=files2)
w['ge'].execute("GROUNDMODELS.ASPHALT = { staticFrictionCoefficient = 1.0, slidingFrictionCoefficient = 0.8, collisiontype = 'ASPHALT' }")
w['ge'].execute("getMissionFilename = function() return 'levels/smallgrid/main.level.json' end")
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
w['inbox'].append(tile_msg('g_1_1', 8.0, 8.0, 230.0, s=['CONCRETE|ASPHALT']))
frame(w, 2)
cf = json.loads(files2['/levels/smallgrid/groundModels/gtabridge.json'])['CONCRETE']
check(abs(cf['staticFrictionCoefficient'] - 0.85) < 1e-6 and 'old one is used until then' in str(ev(w, 'gtaBridge.concreteNote()')),
      'a 2.41 CONCRETE (0.97) is written again with 0.85, the old one used until the map loads again (%s)' % ev(w, 'gtaBridge.concreteNote()'))
check(ev(w, 'OBJS.gtab_g_1_1.meshes[1].material') == 'gtab_gm_CONCRETE', 'meanwhile concrete still drives as CONCRETE (%s)' % ev(w, 'OBJS.gtab_g_1_1.meshes[1].material'))

# ======================================================================== v2.42: flat cells drawn as one ("lf")
w = make_world()
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
frame(w)
n = 17
lf = []
for j in range(n - 1):
    for i in range(n - 1):
        lf.append('4' if (i < 8 and j < 8) else ('2' if j < 8 else '1'))   # 4 cells of 2 m, 8 of 1 m, the rest 0.5 m
msg = json.loads(tile_msg('g_5_5', 40.0, 40.0, 230.0))
msg['lf'] = ''.join(lf)
w['inbox'].append(json.dumps(msg, separators=(',', ':')))
frame(w)
nsub = ev(w, '#OBJS.gtab_g_5_5.meshes')
tris = sum(ev(w, '#OBJS.gtab_g_5_5.meshes[%d].faces' % i) for i in range(1, nsub + 1)) // 3
check(tris == 4 * 2 + 16 * 2 + 128 * 2, 'lf: 4 cells of 2 m and 16 of 1 m as 2 triangles each, the rest as before: %d triangles (512 without)' % tris)
for (x, y) in ((41.3, 41.7), (43.9, 45.1), (46.2, 41.2), (44.4, 47.9)):
    h = ev(w, 'castRayStatic({x = %f, y = %f, z = 240.0}, {x=0,y=0,z=-1}, 50)' % (x, y))
    check(abs((240.0 - h) - (230.0 + (x - 40.0) * 0.1)) < 0.01, 'lf: merged cells keep the height (%.2f, %.2f): %.3f' % (x, y, 240.0 - h))

# ======================================================================== v2.41: teleport without repair
SPAWN_SRC = 'local function safeTeleport(veh, pos, rot, checkOnlyStatics_, visibilityPoint_, removeTraffic_, centeredPosition, resetVehicle)\nend\n'
def tp_world(rotates=True, keepsSpeed=False):
    files = {'lua/ge/spawn.lua': SPAWN_SRC}
    w = make_world(files=files)
    w['ge'].execute('''
      VEH.dmg, VEH.keepTps, VEH.vel = 500, 0, { x = 0, y = 0, z = 0 }
      function VEH:getVelocity() return self.vel end
      local spr = VEH.setPositionRotation
      function VEH:setPositionRotation(...) self.dmg = 0; self.vel = { x = 0, y = 0, z = 0 }; return spr(self, ...) end   -- BeamNG's reset repairs
      spawn = { safeTeleport = function(veh, pos, rot, a, b, c, d, reset)
        SAFE_RESET = reset
        if reset ~= false then return veh:setPositionRotation(pos.x, pos.y, pos.z, rot.x, rot.y, rot.z, rot.w) end
        veh.org = { x = pos.x, y = pos.y, z = pos.z }
        if ROTATES then veh.q = { x = rot.x, y = rot.y, z = rot.z, w = rot.w } end
        if KEEPS_SPEED then veh.vel = { x = 0, y = 0, z = -7 } end
        veh.keepTps = veh.keepTps + 1
      end }
    ''')
    w['ge'].execute('ROTATES = %s KEEPS_SPEED = %s' % ('true' if rotates else 'false', 'true' if keepsSpeed else 'false'))
    w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
    w['inbox'].append(tile_msg('g_10_20', 80.0, 160.0, 230.0))
    frame(w, 3)
    w['inbox'].append(json.dumps({'t': 'tp', 'p': [84.0, 164.0, 230.4], 'f': [1, 0, 0], 'u': [0, 0, 1], 'o': [0, 0.2, -0.45], 'lift': 0.2, 'why': 'start'}))
    frame(w, 60)
    return w
w = tp_world()
tp = msgs(w, 'tp_ok')
check(tp and tp[0]['api'] == 'safeTeleport (no repair)' and tp[0]['keep'] == 1 and ev(w, 'VEH.dmg') == 500 and ev(w, 'SAFE_RESET') is False,
      'no-repair teleport: spawn.safeTeleport with its reset switch (8th parameter) = false, damage kept (%s, dmg %s)' % (tp and tp[0].get('api'), ev(w, 'VEH.dmg')))
check(any('resetVehicle = false teleports the car as it is' in l for l in w['logs']), 'the switch is found in your BeamNG\'s spawn.lua and logged')
fwd = ev(w, 'VEH:getDirectionVector()')
check(fwd['x'] > 0.999, 'no-repair teleport faces the GTA car\'s way')
w = tp_world(rotates=False)
tp = msgs(w, 'tp_ok')
fwd = ev(w, 'VEH:getDirectionVector()')
check(tp and tp[0]['api'] == 'setPositionRotation' and fwd['x'] > 0.999 and any('did not turn the car' in l for l in w['logs']),
      'a no-repair teleport that doesn\'t turn the car -> setPositionRotation (logged) (%s)' % (tp and tp[0].get('api')))
w = tp_world(keepsSpeed=True)
tp = msgs(w, 'tp_ok')
check(tp and tp[0]['api'] == 'setPositionRotation' and tp[0]['v'] < 0.5 and any('kept 7.0 m/s' in l for l in w['logs']),
      'a no-repair teleport that keeps falling speed -> setPositionRotation (%s, %.1f m/s)' % (tp and tp[0].get('api'), tp[0]['v'] if tp else -1))

# ======================================================================== v2.43: pause sync
def pworld(api='sta', works=True):
    w = make_world()
    if api == 'sta':
        w['ge'].execute('SIM_PAUSED = false PAUSE_CALLS = 0 WORKS = %s simTimeAuthority = { pause = function(on) PAUSE_CALLS = PAUSE_CALLS + 1 if WORKS then SIM_PAUSED = on end end, getPause = function() return SIM_PAUSED end }' % ('true' if works else 'false'))
    elif api == 'bt':
        w['ge'].execute('SIM_PAUSED = false PAUSE_CALLS = 0 bullettime = { pause = function(on) PAUSE_CALLS = PAUSE_CALLS + 1 SIM_PAUSED = on end }')
    else:
        w['ge'].execute('SIM_PAUSED = false PAUSE_CALLS = 0')
    w['inbox'].append('{"t":"drive_start","v":"2.43","snd":0,"ps":1}')
    pframe(w, 3)
    return w
def dmsg(gp=0, p=None):
    return '{"t":"drive","q":1,"thr":0,"brk":0,"str":0,"hb":0,"flt":1,"pad":"","keys":"","m":0,"p":%d,"gp":%d}' % (gp if p is None else p, gp)
def pframe(w, n=1, gp=None, user=None):
    for _ in range(n):
        if gp is not None: w['inbox'].append(dmsg(gp))
        clock.t += 1 / 60
        if user is not None: w['ge'].execute('SIM_PAUSED = %s' % ('true' if user else 'false'))
        paused = w['ge'].eval('SIM_PAUSED')
        w['M'].onUpdate(1 / 60, 0 if paused else 1 / 60, 1 / 60)
        w['ge'].execute('VEH_CMDS = {}')
def count(w, t):
    return len(msgs(w, t))
w = pworld()
check(len(msgs(w, 'drive_ok')) == 1, 'pause: drive_ok')
pframe(w, 30, gp=0)
np0 = count(w, 'pose')
check(np0 >= 25 and not ev(w, 'SIM_PAUSED'), 'pause: GTA running: poses, BeamNG runs (%d poses)' % np0)
pframe(w, 60, gp=1)
check(ev(w, 'SIM_PAUSED') and ev(w, 'PAUSE_CALLS') == 1, 'pause: GTA paused ("gp":1) -> BeamNG paused with simTimeAuthority.pause (once)')
check(any('BeamNG paused (simTimeAuthority.pause)' in l for l in w['logs']), 'pause: logged with the function used')
np1 = count(w, 'pose')
pzs = msgs(w, 'pz')
check(np1 - np0 <= 3 and len(pzs) >= 4 and all(m['by'] == 'gta' for m in pzs), 'pause: no poses while paused, "pz" by gta ~5 a second (%d poses, %d pz)' % (np1 - np0, len(pzs)))
pframe(w, 30, gp=0)
check(not ev(w, 'SIM_PAUSED') and any('GTA runs again: BeamNG runs again' in l for l in w['logs']) and count(w, 'pose') - np1 >= 25,
      'pause: GTA runs again -> BeamNG runs again, poses again')
# you pause BeamNG yourself: "pz" by user; GTA pausing (its menu opens) doesn't take it over; GTA running again doesn't unpause it
n0 = len(msgs(w, 'pz'))
pframe(w, 30, gp=0, user=True)
pz2 = msgs(w, 'pz')[n0:]
check(len(pz2) >= 2 and all(m['by'] == 'user' for m in pz2), 'pause: you pause BeamNG -> "pz" by user (GTA opens its pause menu) (%d in 0.5 s)' % len(pz2))
calls0 = ev(w, 'PAUSE_CALLS')
pframe(w, 30, gp=1)
pframe(w, 30, gp=0)
check(ev(w, 'SIM_PAUSED') and ev(w, 'PAUSE_CALLS') == calls0, 'pause: your pause stays yours (GTA paused and running again don\'t touch it)')
pframe(w, 30, gp=0, user=False)
check(not ev(w, 'SIM_PAUSED') and any('BeamNG runs again' in l for l in w['logs']), 'pause: you unpause BeamNG -> it runs')
# GTA paused, then you unpause BeamNG in BeamNG: it runs until GTA runs and pauses again
pframe(w, 20, gp=1)
check(ev(w, 'SIM_PAUSED'), 'pause: GTA paused again -> paused')
pframe(w, 30, gp=1, user=False)
check(not ev(w, 'SIM_PAUSED') and any('you unpaused BeamNG while GTA is paused' in l for l in w['logs']), 'pause: you unpause BeamNG while GTA is paused: it runs (logged)')
pframe(w, 30, gp=1)
check(not ev(w, 'SIM_PAUSED'), 'pause: ... and is not paused again while GTA stays paused')
pframe(w, 10, gp=0)
pframe(w, 10, gp=1)
check(ev(w, 'SIM_PAUSED'), 'pause: GTA ran and paused again -> paused again')
# alt-tab: GTA's script stops, "hold" twice a second
pframe(w, 10, gp=0)
check(not ev(w, 'SIM_PAUSED'), 'pause: running')
for i in range(60):
    if i % 30 == 0: w['inbox'].append('{"t":"hold","ms":600}')
    pframe(w)
check(ev(w, 'SIM_PAUSED') and any('hold' in l and 'BeamNG paused' in l for l in w['logs']), 'pause: alt-tabbed ("hold") -> BeamNG paused')
pframe(w, 10, gp=0)
check(not ev(w, 'SIM_PAUSED'), 'pause: GTA back from alt-tab -> BeamNG runs')
# the drive ends while the mod has BeamNG paused: it runs again
pframe(w, 10, gp=1)
w['inbox'].append('{"t":"drive_stop"}')
pframe(w, 3)
check(not ev(w, 'SIM_PAUSED') and any('the drive ended: BeamNG runs again' in l for l in w['logs']), 'pause: drive ends while paused -> BeamNG runs again')
# older BeamNG: bullettime.pause
w = pworld('bt')
pframe(w, 20, gp=1)
check(ev(w, 'SIM_PAUSED') and any('bullettime.pause' in l for l in w['logs']), 'pause: older BeamNG -> bullettime.pause')
# no pause function: nothing breaks, the car brakes as before
w = pworld('none')
pframe(w, 20, gp=1)
check(any('none found' in l for l in w['logs']) and count(w, 'pose') > 15, 'pause: no pause function -> logged, poses go on (the car brakes)')
# a pause function that doesn't stop the physics: found out, pause sync off
w = pworld('sta', works=False)
pframe(w, 120, gp=1)
check(any("didn't stop BeamNG's physics" in l for l in w['logs']), 'pause: a pause that does nothing is found out and switched off')
# an older GTA plugin (no "ps"): no pausing
w = make_world()
w['ge'].execute('SIM_PAUSED = false PAUSE_CALLS = 0 simTimeAuthority = { pause = function(on) PAUSE_CALLS = PAUSE_CALLS + 1 SIM_PAUSED = on end }')
w['inbox'].append('{"t":"drive_start","v":"2.42","snd":0}')
pframe(w, 3)
pframe(w, 30, gp=1)
check(ev(w, 'PAUSE_CALLS') == 0, 'pause: an older GTA plugin (no "ps") -> no pausing')

# ======================================================================== v2.43: repair where the car is
w = make_world()
w['ge'].execute("VEH.dmg = 700 local spr = VEH.setPositionRotation function VEH:setPositionRotation(...) self.dmg = 0 return spr(self, ...) end")
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
w['inbox'].append(tile_msg('g_10_20', 80.0, 160.0, 230.0))
frame(w, 3)
w['inbox'].append('{"t":"drive_start","v":"2.43","snd":0,"ps":1}')
frame(w, 3)
# the car on its side at (84, 164, 231), facing roughly east
w['ge'].execute('VEH.org = { x = 84, y = 164, z = 231 } VEH.q = quatFromDir({ x = 1, y = 0.2, z = 0 }, { x = 0, y = 1, z = 0 })')
p0 = ev(w, 'VEH:getPosition()')
f0 = ev(w, 'VEH:getDirectionVector()')
f0h = (f0['x'] / math.hypot(f0['x'], f0['y']), f0['y'] / math.hypot(f0['x'], f0['y']))
w['M'].repairHere('recover_vehicle')
frame(w, 20)
up = ev(w, 'VEH:getDirectionVectorUp()')
fw = ev(w, 'VEH:getDirectionVector()')
p1 = ev(w, 'VEH:getPosition()')
check(ev(w, 'VEH.dmg') == 0 and up['z'] > 0.99, 'repair: repaired (BeamNG\'s reset) and upright (up z %.3f)' % up['z'])
check(fw['x'] * f0h[0] + fw['y'] * f0h[1] > 0.999 and abs(fw['z']) < 0.01, 'repair: facing the same way, level (%.2f %.2f %.2f; before %.2f %.2f)' % (fw['x'], fw['y'], fw['z'], f0h[0], f0h[1]))
dxy = math.hypot(p1['x'] - p0['x'], p1['y'] - p0['y'])
check(dxy < 0.2 and 0.3 < p1['z'] - p0['z'] < 0.7, 'repair: where it was, 0.5 m up (moved %.2f m, up %.2f m)' % (dxy, p1['z'] - p0['z']))
rp = msgs(w, 'rep')
check(len(rp) == 1 and rp[0]['a'] == 'recover_vehicle', 'repair: GTA told ("rep") (%s)' % rp)
w['M'].repairHere('recover_vehicle')
frame(w, 5)
check(len(msgs(w, 'rep')) == 1, 'repair: a second press within 1 s is ignored')

# ======================================================================== v2.43: changing the map from inside a map
w = make_world()
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
w['inbox'].append(tile_msg('g_10_20', 80.0, 160.0, 230.0))
frame(w, 3)
w['inbox'].append('{"t":"drive_start","v":"2.43","snd":0,"ps":1}')
frame(w, 3)
rl0 = ev(w, 'RELOADS')
w['M'].onClientEndMission()
check(any('a map is being left' in l for l in w['logs']) and msgs(w, 'drive_err'), 'map change: logged, the drive ends (GTA told)')
check(ev(w, 'OBJS.gtab_g_10_20.alive') is True, 'map change: the mod doesn\'t delete its tiles itself (the map takes them with it)')
w['inbox'].append(tile_msg('g_11_20', 88.0, 160.0, 230.0))
nok = len(msgs(w, 'hf_ok'))
frame(w, 5)
check(len(msgs(w, 'hf_ok')) == nok and ev(w, 'OBJS.gtab_g_11_20') is None and ev(w, 'RELOADS') == rl0, 'map change: nothing built or reloaded while the map changes')
w['M'].onClientPreStartMission()
w['M'].onClientPostStartMission()
w['inbox'].append('{"t":"map","ox":0,"oy":0,"zl":200,"fresh":1,"tile":8}')
w['inbox'].append(tile_msg('g_12_20', 96.0, 160.0, 230.0))
frame(w, 3)
check(any(m['id'] == 'g_12_20' for m in msgs(w, 'hf_ok')), 'map change: the new map started -> tiles built again')
w['M'].onClientEndMission()
w['M'].onExtensionUnloaded()
check(True, 'unload during a map change: no error')

print()
print('ALL PASS' if not fails else '%d FAILED: %s' % (len(fails), fails))
sys.exit(1 if fails else 0)
