#!/usr/bin/env python3
"""fake_beam_lua.py - v2.01 sandbox stand-in for BeamNG that runs the REAL BeamNG mod (gtaBridge.lua) in
LuaJIT (lupa), with a real UDP socket on 127.0.0.1:47011, BeamNG's input system as stubs (your bindings:
an Xbox pad + keyboard, as in lua_input_test.py) and a simple car whose physics read the car's own
electrics (throttle_input, brake_input, steering_input, clutch_input, parkingbrake_input) - so the car only
moves if the mod's bindings path really drives it.
Usage: python3 fake_beam_lua.py <seconds> <gtaBridge.lua>
Writes fake_beam_events.txt (what ran in the car), fake_beam_path.csv (st,x,y,z,psi,v,delta,gear) and
gtabridge/ (the mod's log and bindings.txt) in the current folder.
v2.4: BeamNG's scene objects (ProceduralMesh, Material, scenetree, static collision rays) as Lua stubs, the teleport
(setPositionRotation; this fake car faces -Y in its own frame like BeamNG's), and the car standing on the tiles the mod
builds (falling where there are none) - so GTA's fake ground -> rays -> tiles -> the car's height -> poses -> GTA's
puppet can be checked end to end. Tile builds go to fake_beam_tiles.txt."""
import json, math, os, socket, sys, time
from lupa import luajit21 as lj

SECS = float(sys.argv[1]) if len(sys.argv) > 1 else 60
VMAX = float(os.environ.get('FAKE_VMAX', '9'))      # v2.41: top speed (m/s) for the fast-drive test
ACC = float(os.environ.get('FAKE_ACC', '3'))
# v2.42: BeamNG's frame clock is a few ms off its physics (your v2.41 log): each frame's dtRaw = dt + e_now - e_before,
# e uniform within +-FAKE_FRAMEJITTER ms; the car moves on the physics clock (dtSim = dt)
JIT = float(os.environ.get('FAKE_FRAMEJITTER', '0')) / 1000.0
import random
random.seed(7)
jit_prev = 0.0
SRC = sys.argv[2] if len(sys.argv) > 2 else '../../src/gtaBridge.lua'
O_TRUE = (0.0, 0.2, -0.45)  # car-space origin in the vehicle frame
L = 2.85
T0 = time.perf_counter()

ge = lj.LuaRuntime(unpack_returned_tuples=True)
vm = lj.LuaRuntime(unpack_returned_tuples=True)
ge_queue = []
ev = open('fake_beam_events.txt', 'w')
path = open('fake_beam_path.csv', 'w')

# ---------------------------------------------------------------- the car's own Lua
vm.execute('''
  electrics = { values = { throttle_input = 0, brake_input = 0, steering_input = 0, parkingbrake_input = 0, clutch_input = 0,
                           throttle = 0, brake = 0, gear = 'N', rpm = 800, wheelspeed = 0, ignitionLevel = 2, engineRunning = 1,
                           gearboxMode = 'arcade', parkingbrake = 0, maxrpm = 7000 } }
  calls = {}
  GEARS = { 'R', 'N', 'D' }
  gi = 2
  input = { event = function(name, v, flt, angle, lock, x, src)
      calls[#calls + 1] = string.format('event %s %.3f %s', name, v, tostring(flt))
      electrics.values[name .. '_input'] = v
    end }
  controller = { mainController = {
      shiftUp = function() calls[#calls + 1] = 'shiftUp'; gi = math.min(3, gi + 1); electrics.values.gear = GEARS[gi] end,
      shiftDown = function() calls[#calls + 1] = 'shiftDown'; gi = math.max(1, gi - 1); electrics.values.gear = GEARS[gi] end } }
  boom = function() error('broken action (Y must never reach BeamNG)') end
''')
vg = vm.globals()
vg.obj = vm.table_from({})
vm.execute('obj.getID = function(self) return 42 end')
vg.obj.queueGameEngineLua = lambda self, code: ge_queue.append(code)

# ---------------------------------------------------------------- GE (BeamNG's game engine Lua)
g = ge.globals()
os.makedirs('gtabridge', exist_ok=True)
def wfile(p, d):
    p = str(p).lstrip('/')
    os.makedirs(os.path.dirname(p) or '.', exist_ok=True)
    open(p, 'w').write(d)
    return True
g.log = lambda lvl, tag, msg: None
g.writeFile = wfile
ge.execute('''
  guihooks = { message = function(m) end }
  vec3 = function(x, y, z) if type(x) == 'table' then return x end return { x = x, y = y, z = z } end
  quat = function(x, y, z, w) return { x = x, y = y, z = z, w = w } end
  GROUNDMODELS = { ASPHALT = {}, ASPHALT_WET = {}, ASPHALT_OLD = {}, DIRT = {}, DIRT_DUSTY = {}, GRAVEL = {}, GRASS = {}, SAND = {}, MUD = {}, ROCK = {}, METAL = {}, WOOD = {}, ICE = {}, SNOW = {}, RUMBLE_STRIP = {}, COBBLESTONE = {} }
  jsonReadFile = function(p) if p == 'art/groundmodels.json' then return GROUNDMODELS end return nil end
  readFile = function(p) return nil end
  -- v2.4: scene objects
  OBJS, RELOADS = {}, 0
  local Obj = {}
  Obj.__index = Obj
  function createObject(cls) return setmetatable({ cls = cls, fields = {}, pos = { x = 0, y = 0, z = 0 }, alive = true }, Obj) end
  function Obj:setPosition(p) self.pos = { x = p.x, y = p.y, z = p.z } end
  function Obj:registerObject(name) self.name = name; OBJS[name] = self end
  function Obj:createMesh(t)
    self.meshes = t[1]
    -- v2.43: the mesh's extent (castRayStatic below skips objects not under the ray: the static world has hundreds)
    local x0, y0, x1, y1 = 1e30, 1e30, -1e30, -1e30
    for _, mesh in ipairs(t[1]) do
      for _, v in ipairs(mesh.verts) do
        if v.x < x0 then x0 = v.x end; if v.x > x1 then x1 = v.x end
        if v.y < y0 then y0 = v.y end; if v.y > y1 then y1 = v.y end
      end
    end
    self.bb = { x0, y0, x1, y1 }
    if TILELOG then TILELOG(self.name, #t[1]) end
  end
  function Obj:delete() self.alive = false; if self.name and OBJS[self.name] == self then OBJS[self.name] = nil end end
  function Obj:setField(k, i, v) self.fields[k] = v end
  function Obj:getField(k, i) return self.fields[k] or '' end
  be = { reloadStaticCollision = function(self) RELOADS = RELOADS + 1 end }
  function castRayStatic(p, d, maxd)
    local best = maxd
    for _, o in pairs(OBJS) do
      local px, py = p.x - o.pos.x, p.y - o.pos.y
      if o.cls == 'ProceduralMesh' and o.alive and o.meshes and (not o.bb or (px >= o.bb[1] - 1e-6 and px <= o.bb[3] + 1e-6 and py >= o.bb[2] - 1e-6 and py <= o.bb[4] + 1e-6)) then
        for _, mesh in ipairs(o.meshes) do
          local f, V = mesh.faces, mesh.verts
          -- quick reject on the mesh's extent
          local v1 = V[1]
          for k = 1, #f, 3 do
            local A, B, C = V[f[k].v + 1], V[f[k + 1].v + 1], V[f[k + 2].v + 1]
            if px >= math.min(A.x, B.x, C.x) - 1e-6 and px <= math.max(A.x, B.x, C.x) + 1e-6 and py >= math.min(A.y, B.y, C.y) - 1e-6 and py <= math.max(A.y, B.y, C.y) + 1e-6 then
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
  -- v2.43: BeamNG's pause (current BeamNG: simTimeAuthority)
  SIM_PAUSED = false
  simTimeAuthority = { pause = function(on) SIM_PAUSED = on and true or false; if PAUSELOG then PAUSELOG(SIM_PAUSED) end end,
                       getPause = function() return SIM_PAUSED end }
  FS = {}
  function FS:findFiles() return {} end
  function FS:fileExists() return false end
  function FS:getFileRealPath(p) return "C:/user" .. p end
''')
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
g.jsonEncode = lambda t: json.dumps(from_lua(t), separators=(',', ':'))

class Udp:
    def __init__(self):
        self.s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    def setsockname(self, ip, port):
        self.s.bind((str(ip), int(port))); return 1
    def settimeout(self, t):
        self.s.setblocking(False)
    def receivefrom(self, n=None):
        try:
            d, a = self.s.recvfrom(65536)
            return d.decode('latin-1'), a[0], a[1]
        except BlockingIOError:
            return None, 'timeout', None
    def sendto(self, d, ip, port):
        b = d.encode('latin-1') if isinstance(d, str) else bytes(d)
        self.s.sendto(b, (str(ip), int(port))); return len(b)
    def close(self):
        self.s.close()
udp = Udp()
g.socket = ge.table_from({})
g.socket.udp = lambda: udp
g.socket.gettime = lambda: 1000.0 + time.perf_counter() - T0

# ---------------------------------------------------------------- the car (BeamNG world: x, y, z up)
car = dict(x=500.0, y=-300.0, z=12.0, psi=1.0, v=0.0, delta=0.0, spin=0.0)
WH = [('FR', 0.8, 1.45, True), ('FL', -0.8, 1.45, True), ('RR', 0.8, -1.4, False), ('RL', -0.8, -1.4, False)]
def basis():
    f = (math.cos(car['psi']), math.sin(car['psi']), 0.0)
    u = (0.0, 0.0, 1.0)
    r = (f[1], -f[0], 0.0)
    return r, f, u
def to_world(l):
    r, f, u = basis()
    return tuple(r[i] * l[0] + f[i] * l[1] + u[i] * l[2] for i in range(3))
def node_local(cid):
    w, k = divmod(cid, 3)
    name, x, y, front = WH[w]
    hub = (x + O_TRUE[0], y + O_TRUE[1], 0.33 + O_TRUE[2])
    d = car['delta'] if front else 0.0
    ax = (math.cos(d), math.sin(d), 0.0)
    if k < 2:
        s = -0.1 if k == 0 else 0.1
        return tuple(hub[i] + ax[i] * s for i in range(3))
    ef = (-math.sin(d), math.cos(d), 0.0)
    a = car['spin']
    return tuple(hub[i] + 0.2 * (ef[i] * math.cos(a) + (0, 0, 1)[i] * math.sin(a)) for i in range(3))
wheels = [{'name': n, 'node1': i * 3, 'node2': i * 3 + 1, 'radius': 0.33, 'treadNodes': [i * 3 + 2]} for i, (n, x, y, fr) in enumerate(WH)]
g.core_vehicle_manager = ge.table_from({})
vdata = to_lua({'vdata': {'wheels': wheels, 'nodes': {}}})
g.core_vehicle_manager.getPlayerVehicleData = lambda: vdata
veh = ge.table_from({})
g.VEH = veh
ge.execute('''
  function VEH:getID() return 42 end
  function VEH:getJBeamFilename() return 'testcar' end
  function VEH:getDirectionVectorUp() return { x = 0, y = 0, z = 1 } end
''')
def P():
    o = to_world(O_TRUE)
    return (car['x'] - o[0], car['y'] - o[1], car['z'] - o[2])
veh.getPosition = lambda self: to_lua(dict(zip('xyz', P())))
veh.getDirectionVector = lambda self: to_lua(dict(zip('xyz', basis()[1])))
veh.getVelocity = lambda self: to_lua(dict(zip('xyz', [c * car['v'] for c in basis()[1]])))
veh.getNodePosition = lambda self, c: to_lua(dict(zip('xyz', to_world(node_local(int(c))))))
def veh_cmd(self, code):
    vm.execute(code)
veh.queueLuaCommand = veh_cmd
world_car = {'on': False, 'vz': 0.0, 'tps': 0}
def set_pos_rot(self, x, y, z, qx, qy, qz, qw):
    # BeamNG-like: the vehicle faces -Y in its own frame; x, y, z = its ref node (what getPosition reads)
    f = ge.eval('QROT')(ge.table_from({'x': qx, 'y': qy, 'z': qz, 'w': qw}), ge.table_from({'x': 0, 'y': -1, 'z': 0}))
    car['psi'] = math.atan2(f['y'], f['x'])
    car['v'] = 0.0
    o = to_world(O_TRUE)
    car['x'], car['y'], car['z'] = x + o[0], y + o[1], z + o[2]
    world_car['on'] = True
    world_car['vz'] = 0.0
    world_car['tps'] += 1
    ev.write('%.3f TELEPORT to %.2f %.2f %.2f heading %.1f deg\n' % (st, x, y, z, math.degrees(car['psi']))); ev.flush()
veh.setPositionRotation = set_pos_rot
cast = None
ge.execute('getPlayerVehicle = function() return VEH end')

# ---------------------------------------------------------------- BeamNG's input system
maps = {n: to_lua({'enabled': True}) for n in ['GlobalActionMap', 'MenuActionMap', 'VehicleCommonActionMap', 'VehicleSpecificActionMap', 'NormalActionMap']}
g.MAPFIND = lambda name: maps.get(name)
ge.execute("scenetree = { findObject = function(n) return OBJS[n] or MAPFIND(n) end, MissionGroup = { addObject = function(self, o) end } }")
tiles_log = open('fake_beam_tiles.txt', 'w')
def tilelog(name, nsub):
    tiles_log.write('%.3f %s %d\n' % (time.perf_counter() - T0, name, nsub)); tiles_log.flush()
g.TILELOG = tilelog
ge.execute('ActionMap = {} function ActionMap:getList() return { "GlobalActionMap", "NormalActionMap" } end')
actions = {
    'accelerate': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('throttle', VALUE, FILTERTYPE)"},
    'brake': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('brake', VALUE, FILTERTYPE)"},
    'steering': {'cat': 'vehicle', 'ctx': 'vlua', 'isCentered': True, 'onChange': "input.event('steering', VALUE, FILTERTYPE, ANGLE, LOCKTYPE)"},
    'steer_left': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('steering', -VALUE, FILTERTYPE)"},
    'steer_right': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('steering', VALUE, FILTERTYPE)"},
    'clutch': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('clutch', VALUE, FILTERTYPE)"},
    'parkingbrake': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('parkingbrake', VALUE, FILTERTYPE)"},
    'shiftUp': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'controller.mainController.shiftUp()'},
    'shiftDown': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'controller.mainController.shiftDown()'},
    'toggleESCMode': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'esc'"},
    'brokenThing': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'boom()'},
    'toggleNitrousOxide': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'nitrous'"},
    'toggle_headlights': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'headlights'"},
    'customModifier1': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
    'customModifier2': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
    'customModifier3': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
    'reset_physics': {'cat': 'gameplay', 'ctx': 'tlua', 'onDown': 'RESET_CALLED = true'},
    'camera_next': {'cat': 'camera', 'ctx': 'tlua', 'onDown': 'CAM_CALLED = true'},
}
kb = [
    {'control': 'up', 'action': 'accelerate'}, {'control': 'down', 'action': 'brake'},
    {'control': 'left', 'action': 'steer_left'}, {'control': 'right', 'action': 'steer_right'},
    {'control': 'lshift', 'action': 'clutch'}, {'control': 'x', 'action': 'shiftUp'}, {'control': 'z', 'action': 'shiftDown'},
    {'control': 'ctrl q', 'action': 'toggleESCMode'}, {'control': 'r', 'action': 'reset_physics'}, {'control': 'c', 'action': 'camera_next'},
]
pad = [
    {'control': 'triggerr', 'action': 'accelerate'}, {'control': 'triggerl', 'action': 'brake'},
    {'control': 'thumblx', 'action': 'steering', 'deadzoneResting': 0.1}, {'control': 'btn_l', 'action': 'clutch'},
    {'control': 'btn_a', 'action': 'shiftUp'}, {'control': 'btn_x', 'action': 'shiftDown'}, {'control': 'btn_b', 'action': 'parkingbrake'},
    {'control': 'btn_r', 'action': 'toggleESCMode'}, {'control': 'btn_y', 'action': 'brokenThing'}, {'control': 'dpadl', 'action': 'camera_next'},
]
# v2.1: like the user's setup: DS4Windows' Xbox pad (BeamNG's defaults, above) + his own DualSense list (BeamNG's
# PlayStation support): Circle shift up, Square down, Cross handbrake, L1 clutch + modifier 2, R1 modifier 1
dse = [{'control': 'button2', 'action': 'shiftUp'}, {'control': 'button0', 'action': 'shiftDown'}, {'control': 'button1', 'action': 'parkingbrake'},
       {'control': 'button4', 'action': 'clutch'}, {'control': 'button4', 'action': 'customModifier2'}, {'control': 'button5', 'action': 'customModifier1'},
       {'control': 'button13', 'action': 'customModifier3'}, {'control': 'ryaxis', 'action': 'accelerate'}, {'control': 'rxaxis', 'action': 'brake'},
       {'control': 'xaxis', 'action': 'steering', 'deadzoneResting': 0.1}, {'control': 'modifier1 dpov', 'action': 'toggleNitrousOxide'},
       {'control': 'modifier2 upov', 'action': 'toggleESCMode'}, {'control': 'button10', 'action': 'toggle_headlights'},
       {'control': 'button3', 'action': 'brokenThing'}]
g.core_input_bindings = to_lua({'bindings': [
    {'devname': 'keyboard0', 'contents': {'name': 'Keyboard', 'bindings': kb}},
    {'devname': 'xinput0', 'contents': {'name': 'XBox Controller 1', 'vidpid': 'XIDevice', 'bindings': pad}},
    {'devname': 'gamepad0', 'contents': {'name': 'DualSense wireless controller 0', 'vidpid': 'sce_pad_standard', 'bindings': dse}}]})
ge.execute('''
  TSVARS = { ['$pref::SFX::muteOnWindowLoseFocus'] = '1' }
  TorqueScriptLua = { getVar = function(n) return TSVARS[n] or '' end, setVar = function(n, v) TSVARS[n] = tostring(v) end }
''')
g.ACTS = to_lua(actions)
ge.execute('core_input_actions = { getActiveActions = function() return ACTS end }')
g.extensions = ge.table_from({})
# v2.5: FAKE_PORT=<n> runs this fake BeamNG on another UDP port (several e2e runs side by side; run_e2e.sh puts the same
# port into the run folder's GtaBeamD1.ini as BeamNGPort)
LUA_SRC = open(SRC).read()
if os.environ.get('FAKE_PORT'):
    LUA_SRC = LUA_SRC.replace('listenPort = 47011', 'listenPort = %d' % int(os.environ['FAKE_PORT']), 1)
M = ge.execute(LUA_SRC)
g.gtaBridge = M
M.onExtensionLoaded()

# ---------------------------------------------------------------- loop
# v2.43: FAKE_USERPAUSE="a b": you pause BeamNG at a s and unpause it at b s; FAKE_REPAIR_AT=t: your repair binding at t s
UP = [float(x) for x in os.environ.get('FAKE_USERPAUSE', '').split()] if os.environ.get('FAKE_USERPAUSE') else None
REP_AT = float(os.environ.get('FAKE_REPAIR_AT', '0') or 0)
rep_done = False
def pauselog(on):
    ev.write('%.3f BeamNG physics %s (by the mod)\n' % (st, 'PAUSED' if on else 'RUNNING')); ev.flush()
g.PAUSELOG = pauselog
paused_frames = 0
# v2.43: walls in BeamNG's GTA ground: triangles more than 2.5 m high (a deck's edge drawn down to the road); written to
# fake_beam_walls.txt every 5 s (GTA quitting cuts the run)
def walls_dump():
    walls, big, ntri, ntile, where = 0, 0, 0, 0, []
    objs = ge.eval('OBJS')
    for name in list(objs.keys()):
        o = objs[name]
        if o.cls != 'ProceduralMesh' or not o.alive or not o.meshes or not str(name).startswith('gtab_g_'): continue
        ntile += 1
        for mi in range(1, len(o.meshes) + 1):
            mesh = o.meshes[mi]
            f, V = mesh.faces, mesh.verts
            for k in range(1, len(f) + 1, 3):
                zs = [V[f[k + q].v + 1].z for q in range(3)]
                ntri += 1
                if max(zs) - min(zs) > 4.0: big += 1
                if max(zs) - min(zs) > 2.5:
                    walls += 1
                    if len(where) < 20: where.append('%s(%.1f,%.1f)' % (name, o.pos.x + V[f[k].v + 1].x, o.pos.y + V[f[k].v + 1].y))
    open('fake_beam_walls.txt', 'w').write('%.1f WALLS %d of %d triangles in %d tiles, %d over 4 m high %s\n' % (st, walls, ntri, ntile, big, ' '.join(where)))
# (counted when the run is stopped: GTA quitting ends it with SIGTERM)
import signal
def on_term(sig, frm):
    try: walls_dump()
    finally: os._exit(0)
signal.signal(signal.SIGTERM, on_term)
DT = 1 / 60
dt = DT
last_frame = None
st = 0.0
nxt = time.perf_counter()
mapsWas = None
end = time.perf_counter() + SECS
while time.perf_counter() < end:
    now = time.perf_counter()
    if now < nxt:
        time.sleep(min(0.002, nxt - now))
        continue
    nxt += DT
    if nxt < now: nxt = now + DT
    # v2.43: like BeamNG's physics, the simulation catches up with real time when a frame took longer (it used to step
    # 1/60 s whatever the frame took: its clock fell behind under load)
    dt = min(0.1, max(DT, now - last_frame)) if last_frame else DT
    last_frame = now
    st += dt
    jit_now = random.uniform(-JIT, JIT) if JIT > 0 else 0.0
    dt_raw = max(0.001, dt + jit_now - jit_prev)
    jit_prev = jit_now
    if UP:
        if len(UP) == 2 and st >= UP[0]:
            ge.execute('SIM_PAUSED = true'); ev.write('%.3f BeamNG physics PAUSED (by you)\n' % st); ev.flush(); UP = [UP[1]]
        elif len(UP) == 1 and st >= UP[0]:
            ge.execute('SIM_PAUSED = false'); ev.write('%.3f BeamNG physics RUNNING (by you)\n' % st); ev.flush(); UP = None
    if REP_AT and not rep_done and st >= REP_AT:
        rep_done = True
        M.repairHere('recover_vehicle')
        ev.write('%.3f REPAIR binding pressed\n' % st); ev.flush()
    sim_paused = bool(ge.eval('SIM_PAUSED'))
    paused_frames += 1 if sim_paused else 0
    M.onUpdate(dt_raw, 0.0 if sim_paused else dt, dt_raw)
    while ge_queue:
        ge.execute(ge_queue.pop(0))
    c = vg.calls
    for i in range(1, len(c) + 1):
        ev.write('%.3f %s\n' % (st, c[i]))
    if len(c): ev.flush()
    vm.execute('calls = {}')
    if ge.eval('RESET_CALLED') or ge.eval('CAM_CALLED'):
        ev.write('%.3f WRONG: reset/camera action ran\n' % st); ev.flush()
        ge.execute('RESET_CALLED = nil CAM_CALLED = nil')
    snd = ge.eval("TSVARS['$pref::SFX::muteOnWindowLoseFocus']")
    if snd != globals().get('sndWas'):
        ev.write('%.3f BeamNG mute-in-background setting = %s\n' % (st, snd)); ev.flush()
        globals()['sndWas'] = snd
    mp = tuple(bool(maps[n].enabled) for n in ['VehicleCommonActionMap', 'NormalActionMap', 'GlobalActionMap'])
    if mp != mapsWas:
        ev.write('%.3f maps vehicle=%s normal=%s global=%s\n' % (st, *mp)); ev.flush()
        mapsWas = mp
    # physics from the car's electrics (arcade gearbox: throttle in N engages D)
    e = vg.electrics['values']
    if sim_paused:
        path.write('%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.4f,%s,P\n' % (st, car['x'], car['y'], car['z'], car['psi'], car['v'], car['delta'], e.gear))
        continue
    thr, brk, stv = float(e.throttle_input or 0), float(e.brake_input or 0), float(e.steering_input or 0)
    clu, pb = float(e.clutch_input or 0), float(e.parkingbrake_input or 0)
    if e.gear == 'N' and thr > 0.1:
        vm.execute("gi = 3 electrics.values.gear = 'D'")
    drive = 1.0 if e.gear == 'D' else (-1.0 if e.gear == 'R' else 0.0)
    acc = thr * ACC * (1 - clu) * abs(drive) - brk * 6.0 - pb * 4.0
    car['v'] = max(0.0, min(VMAX, car['v'] + acc * dt))
    want = -stv * 0.45
    car['delta'] += (want - car['delta']) * min(1.0, dt * 6.0)
    car['psi'] += car['v'] * math.tan(car['delta']) / L * dt
    f = basis()[1]
    car['x'] += f[0] * car['v'] * dt
    car['y'] += f[1] * car['v'] * dt
    car['spin'] -= car['v'] * dt / 0.33
    # v2.4: on GTA's ground (the mod's tiles): the car stands on them; where there are none it falls
    if world_car['on']:
        if cast is None:
            cast = ge.eval('castRayStatic')
        d = cast(ge.table_from({'x': car['x'], 'y': car['y'], 'z': car['z'] + 1.5}), ge.table_from({'x': 0, 'y': 0, 'z': -1}), 3.0)
        if d is not None and d < 2.99:
            car['z'] = car['z'] + 1.5 - d
            world_car['vz'] = 0.0
        else:
            world_car['vz'] -= 9.81 * dt
            car['z'] += world_car['vz'] * dt
    e.wheelspeed = car['v']
    e.rpm = 800 + car['v'] * 300
    e.throttle = thr
    e.brake = brk
    e.parkingbrake = pb
    path.write('%.4f,%.4f,%.4f,%.4f,%.4f,%.3f,%.4f,%s,%s\n' % (st, car['x'], car['y'], car['z'], car['psi'], car['v'], car['delta'], e.gear, 'D' if (not M.driveOn or M.driveOn()) else '-'))
walls_dump()
ev.write('PAUSED FRAMES %d\n' % paused_frames)
ev.write('done: speed %.2f, heading %.2f\n' % (car['v'], car['psi']))
ev.close(); path.close()
M.onExtensionUnloaded()
print('fake BeamNG done')
