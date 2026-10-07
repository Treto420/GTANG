"""lua_drive_test.py - BeamNG mod drive mode (v2.0 path, no BeamNG bindings -> fallback) against stubs:
drive_start -> drive_ok (wheel names, spin source), GTA controls -> input.event commands for the vehicle,
poses back every frame (vehicle-frame wheels, unwrapped turn angle at speed), failsafe after 0.5 s without
controls, drive_stop. v2.01: the mod path comes from argv; the failsafe is its own command.
Usage: python3 lua_drive_test.py ../src/gtaBridge.lua"""
import json, math, re, sys
import lupa
from lupa import LuaRuntime

SRC = sys.argv[1] if len(sys.argv) > 1 else '../src/gtaBridge.lua'
lua = LuaRuntime(unpack_returned_tuples=True)
G = lua.globals()

def to_py(v):
    if lupa.lua_type(v) == 'table':
        keys = list(v.keys())
        if keys and all(isinstance(k, int) for k in keys) and sorted(keys) == list(range(1, len(keys) + 1)):
            return [to_py(v[k]) for k in sorted(keys)]
        if not keys:
            return []
        return {str(k): to_py(v[k]) for k in keys}
    return v

G.jsonEncode = lambda v: json.dumps(to_py(v))
G.jsonDecode = lambda s: lua.table_from(json.loads(s), recursive=True)
G.writeFile = lambda p, d: True
G.readFile = lambda p: None
inbox, outbox = [], []
G.py_recv = lambda: (inbox.pop(0) if inbox else None)
G.py_send = lambda s, ip, port: outbox.append(s)
clock = [100.0]
G.py_time = lambda: clock[0]
lua.execute('''
log = function(l, tag, m) print('LOG ' .. l .. ' ' .. tostring(m)) end
guihooks = { message = function(m) end }
vec3 = function(x, y, z) if type(x) == 'table' then return { x = x.x, y = x.y, z = x.z } end return { x = x, y = y, z = z } end
socket = { gettime = function() return py_time() end,
  udp = function() return { setsockname = function() return true end, settimeout = function() end, close = function() end,
    receivefrom = function(self) local d = py_recv(); if d then return d, '127.0.0.1', 50123 end return nil end,
    sendto = function(self, s, ip, port) py_send(s, ip, port) end } end }
CMDS = {}
-- car at (10, 20, 0.5) heading +x (BeamNG world), driving forward at 10 m/s
POSV = { x = 10, y = 20, z = 0.5 }
DIR = { x = 1, y = 0, z = 0 }
local nodes = { [0] = { name = 'fl1', cid = 0 }, [1] = { name = 'fl2', cid = 1 }, [2] = { name = 'rl1', cid = 2 }, [3] = { name = 'rl2', cid = 3 },
                [4] = { name = 'flh', cid = 4 }, [5] = { name = 'rlh', cid = 5 } }
-- node positions relative to the car (world axes): heading +x, so right = -y
ANG = 0   -- wheel turn angle (rad, about the right-pointing axle)
local function hub(cx, cy, cz, a)
  return { x = cx + 0.25 * (-math.sin(a)), y = cy, z = cz + 0.25 * math.cos(a) }
end
function NODEPOS(cid)
  if cid == 0 then return { x = 1.4, y = 0.9, z = 0.3 } end   -- front left, outer
  if cid == 1 then return { x = 1.4, y = 0.6, z = 0.3 } end
  if cid == 2 then return { x = -1.3, y = 0.9, z = 0.3 } end
  if cid == 3 then return { x = -1.3, y = 0.6, z = 0.3 } end
  if cid == 4 then return hub(1.4, 0.75, 0.3, ANG) end
  if cid == 5 then return hub(-1.3, 0.75, 0.3, ANG) end
end
VDATA = { nodes = nodes,
  wheels = { [0] = { name = 'FL', node1 = 'fl1', node2 = 'fl2', radius = 0.33, hubNodes = { 'flh' } },
             [1] = { name = 'RL', node1 = 2, node2 = 3, radius = 0.33, nodes = { 5 } } } }
core_vehicle_manager = { getPlayerVehicleData = function() return { vdata = VDATA } end }
VEH = { getID = function() return 77 end, getJBeamFilename = function() return 'sdd_s650' end,
        getNodePosition = function(self, cid) return NODEPOS(cid) end,
        getPosition = function() return POSV end,
        getDirectionVector = function() return DIR end, getDirectionVectorUp = function() return { x = 0, y = 0, z = 1 } end,
        getVelocity = function() return { x = 10, y = 0, z = 0 } end,
        queueLuaCommand = function(self, s) table.insert(CMDS, s) end }
getPlayerVehicle = function() return VEH end
extensions = { load = function() end, unload = function() end }
''')
M = lua.execute(open(SRC).read())
G.M = M
lua.execute("M.onExtensionLoaded()")
fails = []
def check(name, cond, extra=''):
    print(('ok   ' if cond else 'FAIL ') + name, extra)
    if not cond: fails.append(name)

def frame(dt=1/60):
    clock[0] += dt
    lua.execute('M.onUpdate(%f, %f, %f)' % (dt, dt, dt))

# 1. drive_start -> drive_ok
inbox.append('{"t":"drive_start","v":"2.0"}')
frame()
oks = [json.loads(s) for s in outbox if '"drive_ok"' in s]
check('drive_ok sent', len(oks) == 1, oks)
ok = oks[0] if oks else {}
check('wheel names', sorted(ok.get('wheels', [])) == ['FL', 'RL'])
check('spin from wheel nodes', str(ok.get('spin', '')).startswith('wheel nodes'), ok.get('spin'))
check('version 2.x', str(ok.get('v', '')).startswith('2.'), ok.get('v'))
check('no bindings here: fallback', ok.get('inp') == 'fallback', ok.get('inp'))
# a repeated start keeps the session
inbox.append('{"t":"drive_start","v":"2.0"}')
outbox.clear(); frame()
check('repeat start answered, same session', sum('"drive_ok"' in s for s in outbox) == 1)

# 2. controls -> input.event
lua.execute('CMDS = {}')
inbox.append('{"t":"drive","q":1,"thr":0.800,"brk":0.000,"str":-1.000,"hb":0,"flt":0}')
frame()
cmds = list(lua.globals().CMDS.values())
check('input command queued', len(cmds) >= 1 and "input.event('throttle',0.800,0)" in cmds[-1] and "input.event('steering',-1.000,0)" in cmds[-1]
      and "input.event('parkingbrake',0,0)" in cmds[-1], cmds[-1:])
lua.execute('CMDS = {}')
for i in range(5):
    inbox.append('{"t":"drive","q":%d,"thr":0.800,"brk":0.000,"str":-1.000,"hb":0,"flt":0}' % (i + 2))
    frame()
check('unchanged controls not re-sent every frame', len([c for c in lua.globals().CMDS.values() if 'input.event' in c]) == 0)

# 3. poses: 2.5 rad per frame backwards = rolling forward fast (would alias without unwrapping)
outbox.clear()
for i in range(30):
    lua.execute('ANG = ANG - 2.5')
    inbox.append('{"t":"drive","q":%d,"thr":1.000,"brk":0.000,"str":0.000,"hb":0,"flt":0}' % (i + 10))
    frame()
poses = [json.loads(s) for s in outbox if s.startswith('{"t":"pose"')]
check('a pose every frame', len(poses) == 30, len(poses))
p = poses[-1]
check('pose fields', all(k in p for k in ('q', 'st', 'p', 'f', 'u', 'v', 'w', 'sp')) and len(p['w']) == 14)
w = p['w']
check('FL centre in the vehicle frame', abs(w[0] + 0.75) < 1e-3 and abs(w[1] - 1.4) < 1e-3 and abs(w[2] - 0.3) < 1e-3, w[:3])
check('axle points right', abs(w[3] - 1) < 1e-3, w[3:6])
d_ang = poses[-1]['w'][6] - poses[-2]['w'][6]
check('turn angle unwrapped (-2.5 rad per frame)', abs(d_ang + 2.5) < 1e-3, d_ang)
check('turn angle counts on', abs((poses[-1]['w'][6] - poses[0]['w'][6]) + 2.5 * 29) < 1e-2, poses[-1]['w'][6] - poses[0]['w'][6])
check('BeamNG time advances', abs((poses[-1]['st'] - poses[0]['st']) - 29 / 60) < 1e-3)
check('spin flag', p['sp'] == 1)

# 4. failsafe: no controls for 0.6 s
lua.execute('CMDS = {}')
for i in range(40):
    frame()
cmds = list(lua.globals().CMDS.values())
check('failsafe at speed: throttle off, brake', any("input.event('throttle',0.000,2)" in c and "input.event('brake',0.600,2)" in c and "input.event('parkingbrake',0,2)" in c for c in cmds), cmds[:1])
lua.execute('CMDS = {}; VEH.getVelocity = function() return { x = 0.5, y = 0, z = 0 } end')
for i in range(5):
    frame()
cmds = list(lua.globals().CMDS.values())
check('failsafe slow: parking brake', any("input.event('brake',0.000,2)" in c and "input.event('parkingbrake',1,2)" in c for c in cmds), cmds[:1])

# 4b. GTA silent for 5 s: the session ends by itself
outbox.clear()
for i in range(300):
    frame()
check('silent GTA ends the session (poses stop)', 200 < len(outbox) < 290, len(outbox))
inbox.append('{"t":"drive_start","v":"2.0"}')
frame()

# 5. drive_stop
lua.execute('CMDS = {}')
outbox.clear()
inbox.append('{"t":"drive_stop"}')
frame(); frame()
cmds = list(lua.globals().CMDS.values())
check('stop: parking brake', any("input.event('parkingbrake',1,2)" in c for c in cmds))
check('no poses after stop', not any(s.startswith('{"t":"pose"') for s in outbox))

# 6. car changed mid-drive -> drive_err to GTA
inbox.append('{"t":"drive_start","v":"2.0"}')
frame()
outbox.clear()
lua.execute('VEH.getID = function() return 78 end')
frame()
check('car change -> drive_err', any('"drive_err"' in s for s in outbox), outbox[:1])
print('LUA DRIVE TEST', 'PASS' if not fails else 'FAIL ' + ', '.join(fails))
sys.exit(1 if fails else 0)
