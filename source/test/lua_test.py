"""lua_test.py - runs gtaBridge.lua (BeamNG mod 2.0) against stubs of the BeamNG Lua API it uses.
Checks: export with layered materials (normal/metallic/roughness maps, blend op, cubemap), driven
wheels from a powertrain, matCheck, and that the written glTF parses and carries the new fields."""
import json, os, sys
import lupa
from lupa import LuaRuntime

root = '/root/v200/test/luafs'
os.makedirs(root + '/vehicles/common', exist_ok=True)
os.makedirs(root + '/gtabridge/tex', exist_ok=True)
import shutil
shutil.rmtree(root + '/gtabridge', ignore_errors=True)
os.makedirs(root + '/gtabridge/tex', exist_ok=True)
for f in ['vehicles/common/body_n.dds', 'vehicles/common/body_o.dds', 'vehicles/common/glass_b.color.png']:
    open(root + '/' + f, 'wb').write(b'DDS ' + bytes(140) if f.endswith('.dds') else b'x')
open(root + '/vehicles/common/body_m.dds', 'wb').write(b'x')   # not a real DDS: falls back to PNG conversion

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

def json_encode(v): return json.dumps(to_py(v))
def json_decode(s): return lua.table_from(json.loads(s), recursive=True)
def vpath(p): return root + ('' if p.startswith('/') else '/') + p
written = {}
def write_file(p, data):
    os.makedirs(os.path.dirname(vpath(p)), exist_ok=True)
    open(vpath(p), 'wb').write(data.encode('latin-1') if isinstance(data, str) else data)
    written[p] = True
    return True
def read_file(p):
    try: return open(vpath(p), 'rb').read().decode('latin-1')
    except Exception: return None
G.readFile = read_file
G.jsonEncode = json_encode
G.jsonDecode = json_decode
G.writeFile = write_file
msgs = []
lua.execute('''
log = function(l, tag, m) if l == 'E' then print('LOG E ' .. tostring(m)) end end
guihooks = { message = function(m) table.insert(MSGS, m) end }
MSGS = {}
vec3 = function(x, y, z) if type(x) == 'table' then return { x = x.x, y = x.y, z = x.z } end return { x = x, y = y, z = z } end
''')
G.FS_exists = lambda p: os.path.exists(vpath(p))
G.FS_real = lambda p: 'C:\\\\Users\\\\Treto' + p.replace('/', '\\\\')
G.conv = lambda src, dst: write_file(dst, 'PNG') if os.path.exists(vpath(src)) else False
lua.execute('''
FS = { fileExists = function(self, p) return FS_exists(p) end, getFileRealPath = function(self, p) return FS_real(p) end,
       findFiles = function() return {} end, getUserPath = function() return 'C:/u/' end }
convertDDSToPNG = function(s, d) return conv(s, d) end
-- materials as BeamNG would answer getField(name, layer)
local MATS = {
  body = { version = '1.5', activeLayers = '2', translucent = '0', alphaTest = '0', alphaRef = '1', translucentBlendOp = 'LerpAlpha', cubemap = '',
           L = { { baseColorFactor = 'White', normalMap = 'vehicles/common/body_n', metallicMap = '/vehicles/common/body_m.dds', metallicFactor = '1', roughnessFactor = '1' },
                 { colorPaletteMap = '/vehicles/common/nullcolormaskR', instanceDiffuse = '1', opacityMap = '/vehicles/common/body_o.dds', opacityFactor = '1', clearCoatFactor = '1', normalMapUseUV = '1' } } },
  glass = { version = '1.5', activeLayers = '1', translucent = '1', alphaTest = '0', alphaRef = '1', translucentBlendOp = 'PreMulAlpha', cubemap = 'BlackSkyCubemap',
            L = { { baseColorFactor = '0.5 0.5 0.5 1', baseColorMap = '/vehicles/common/glass_b.color.png', opacityFactor = '0.18', metallicFactor = '1', roughnessFactor = '0' } } },
  screen = { version = '1.5', activeLayers = '1', translucent = '0', alphaTest = '0', alphaRef = '1', translucentBlendOp = '', cubemap = '',
            L = { { baseColorFactor = 'White', baseColorMap = '@sdd_s650_screen', metallicFactor = '0', roughnessFactor = '0.2' } } },
}
local function matObj(name)
  local m = MATS[name]
  if not m then return nil end
  return { getClassName = function() return 'Material' end, activeLayers = m.activeLayers,
           getField = function(self, f, l) if l == 0 and m[f] then return m[f] end local L = m.L[l + 1]; return L and L[f] or '' end }
end
scenetree = { findObject = function(n) return matObj(n) end, MissionGroup = { addObject = function() end } }
local nodes = { [0] = { name = 'fl1', cid = 0 }, [1] = { name = 'fl2', cid = 1 }, [2] = { name = 'rl1', cid = 2 }, [3] = { name = 'rl2', cid = 3 } }
local POS = { [0] = { x = -0.9, y = -1.4, z = 0.3 }, [1] = { x = -0.6, y = -1.4, z = 0.3 }, [2] = { x = -0.9, y = 1.3, z = 0.3 }, [3] = { x = -0.6, y = 1.3, z = 0.3 } }
VDATA = { nodes = nodes,
  wheels = { [0] = { name = 'FL', node1 = 'fl1', node2 = 'fl2', radius = 0.33 }, [1] = { name = 'RL', node1 = 2, node2 = 3, radius = 0.34, tireWidth = 0.3 } },
  powertrain = { { type = 'combustionEngine', name = 'mainEngine', inputName = 'dummy' }, { type = 'manualGearbox', name = 'gearbox', inputName = 'clutch' },
                 { type = 'frictionClutch', name = 'clutch', inputName = 'mainEngine' }, { type = 'shaft', name = 'driveshaft', inputName = 'gearbox' },
                 { type = 'differential', name = 'differential_R', inputName = 'driveshaft' },
                 { type = 'shaft', name = 'wheelaxleRL', inputName = 'differential_R', connectedWheel = 'RL' } } }
PAINTS = { { baseColor = { 0.72, 0.16, 0.16, 1.2 }, metallic = 0.35, roughness = 0.42, clearcoat = 1, clearcoatRoughness = 0.05 },
           { baseColor = { 0, 0, 0, 1 }, metallic = 0, roughness = 0.6, clearcoat = 0.5, clearcoatRoughness = 0.1 } }
core_vehicle_manager = { getPlayerVehicleData = function() return { vdata = VDATA, config = { paints = PAINTS } } end }
VEH = { color = { x = 0.72, y = 0.16, z = 0.16, w = 1.2 }, colorPalette0 = { x = 0, y = 0, z = 0, w = 1 }, colorPalette1 = { x = 1, y = 1, z = 1, w = 1 },
        getNodePosition = function(self, cid) return POS[cid] end, getPosition = function() return { x = 10, y = 20, z = 0.5 } end,
        getJBeamFilename = function() return 'sdd_s650' end, getMaterialNames = function() return { 'body', 'glass', 'screen', 'missing' } end }
getPlayerVehicle = function() return VEH end
-- exporter stub: calls the handler with a root holding two materials
extensions = { load = function() end, unload = function() end,
  util_export = { export = function(h) h({ materials = { { name = 'body' }, { name = 'glass' }, { name = 'screen' } }, meshes = { { primitives = { { attributes = {} } } } },
                                             nodes = { { name = 'n' } }, scenes = { { nodes = { 0 } } }, scene = 0, images = {} }) end } }
''')
src = open('/root/v200/lua/gtaBridge.lua').read()
M = lua.execute(src)
G.M = M
lua.execute("M.onExtensionLoaded()")
lua.execute("M.exportCar()")
lua.execute("M.matCheck()")
gl = json.load(open(root + '/gtabridge/car.gltf'))
mats = {m['name']: m['extras']['gta'] for m in gl['materials']}
print('body blendOp', mats['body']['blendOp'], '| cubemap', repr(mats['body']['cubemap']))
print('body L1', {k: v for k, v in mats['body']['layers'][0].items()})
print('body L2', {k: v for k, v in mats['body']['layers'][1].items()})
print('glass', mats['glass']['blendOp'], mats['glass']['cubemap'], mats['glass']['layers'][0])
print('screen', mats['screen']['layers'][0])
print('wheels', gl['extras']['gtaBridge']['wheels'])
print('version', gl['extras']['gtaBridge']['version'])
print('paint', gl['extras']['gtaBridge']['paint'])
print('tex files', sorted(os.listdir(root + '/gtabridge/tex')))
print(open(root + '/gtabridge/materials.txt').read())
print('messages:', list(lua.globals().MSGS.values()))
ok = (mats['glass']['blendOp'] == 'PreMulAlpha' and mats['glass']['cubemap'] == 'BlackSkyCubemap' and
      mats['body']['layers'][0].get('normalMap') and mats['body']['layers'][0].get('metallicMap') and
      [w['driven'] for w in gl['extras']['gtaBridge']['wheels']] == [False, True] and
      mats['screen']['layers'][0].get('baseColorLive') is True and 'baseColorMap' not in mats['screen']['layers'][0] and
      gl['extras']['gtaBridge']['version'] == '2.0' and
      gl['extras']['gtaBridge']['paint'].get('pbr') == [[0.35, 0.42, 1, 0.05], [0, 0.6, 0.5, 0.1], [0.35, 0.42, 1, 0.05]] and
      gl['extras']['gtaBridge']['paint'].get('pbrSource') == 'config.paints' and
      mats['body']['layers'][0].get('normalMap') == 'tex/vehicles_common_body_n.dds' and
      mats['body']['layers'][1].get('opacityMap') == 'tex/vehicles_common_body_o.dds' and
      mats['body']['layers'][0].get('metallicMap') == 'tex/vehicles_common_body_m.dds.png' and
      open(root + '/gtabridge/tex/vehicles_common_body_n.dds', 'rb').read(4) == b'DDS ')
print('LUA TEST', 'PASS' if ok else 'FAIL')

# v1.9.6: no config paints -> the vehicle object's data (12 numbers), then nothing at all
lua.execute("PAINTS = nil; VEH.getMetallicPaintData = function(self) return { 0.1, 0.2, 1, 0.03, 0.4, 0.5, 0, 0.1, 0.6, 0.7, 1, 0.2 } end")
lua.execute("M.exportCar()")
p2 = json.load(open(root + '/gtabridge/car.gltf'))['extras']['gtaBridge']['paint']
print('paint (object data)', p2.get('pbr'), p2.get('pbrSource'))
lua.execute("VEH.getMetallicPaintData = nil")
lua.execute("M.exportCar()")
p3 = json.load(open(root + '/gtabridge/car.gltf'))['extras']['gtaBridge']['paint']
print('paint (none)', p3.get('pbr'), p3.get('pbrSource'))
ok2 = (p2.get('pbr') == [[0.1, 0.2, 1, 0.03], [0.4, 0.5, 0, 0.1], [0.6, 0.7, 1, 0.2]] and p2.get('pbrSource') == 'getMetallicPaintData()' and
       p3.get('pbr') is None and str(p3.get('pbrSource', '')).startswith('not found'))
print('LUA TEST 2', 'PASS' if ok2 else 'FAIL')
