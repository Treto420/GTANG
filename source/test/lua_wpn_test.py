#!/usr/bin/env python3
"""lua_wpn_test.py - v2.6: the BeamNG mod's weapons code in the car's Lua (gtabWpn: wpn.VEH in gtaBridge.lua) on a box car with
BeamNG 0.39's interfaces as stubs (names from its lua/vehicle: beamstate.addPlanet / deflateTire / triggerDeformGroup /
deformGroupsTriggerBeam, fire.flammableNodes / hotNodes / igniteNode / explodeVehicle / igniteVehicle, powertrain
.getDevicesByCategory + the engine's applyDeformGroupDamage, wheels.wheels[id].isTireDeflated); and the export's deformGroup list.
Usage: python3 lua_wpn_test.py ../src/gtaBridge.lua"""
import json, math, re, sys
from lupa import luajit21 as lj

src = open(sys.argv[1] if len(sys.argv) > 1 else '../src/gtaBridge.lua').read()
i = src.index('wpn.VEH = [==['); j = src.index(']==]', i)
VEH = src[i + len('wpn.VEH = [==['):j]
fails = []
def check(c, what):
    print(('PASS ' if c else 'FAIL ') + what)
    if not c: fails.append(what)

L = lj.LuaRuntime(unpack_returned_tuples=True)
reports = []
L.globals().REPORT = lambda s: reports.append(s)
# a box car 1.8 wide (x), 4.4 long (y, forward +y), 1.3 tall, its position (refnode) at world (100, 200, 50); nodes relative to it
L.execute(r'''
  vec3 = function(x, y, z) return { x = x, y = y, z = z } end
  OPOS = { x = 100, y = 200, z = 50 }
  NODES = {}
  local function add(x, y, z) local c = #NODES + 1 NODES[c] = { x, y, z } return c end
  -- body corners: 8 nodes (cids 1..8), and a middle ring (9..12)
  local C = {}
  for _, z in ipairs({ 0.3, 1.3 }) do for _, y in ipairs({ -2.2, 2.2 }) do for _, x in ipairs({ -0.9, 0.9 }) do C[#C + 1] = add(x, y, z) end end end
  -- (1 (-x,-y,lo) 2 (+x,-y,lo) 3 (-x,+y,lo) 4 (+x,+y,lo) 5..8 the same at the top)
  -- wheels: hub nodes at the front left (13, 14), front right (15, 16)
  local w1a, w1b = add(-0.95, 1.4, 0.33), add(-0.75, 1.4, 0.33)
  local w2a, w2b = add(0.75, 1.4, 0.33), add(0.95, 1.4, 0.33)
  -- the left window frame: 4 nodes on the left side, high (17..20), and its diagonal glass beams
  local g1, g2, g3, g4 = add(-0.9, -0.5, 0.9), add(-0.9, 0.5, 0.9), add(-0.9, 0.5, 1.25), add(-0.9, -0.5, 1.25)
  -- engine block (21, 22) at the front middle; radiator nodes (23, 24) at the very front
  local e1, e2 = add(0, 1.6, 0.6), add(0, 1.9, 0.6)
  local r1, r2 = add(-0.3, 2.15, 0.5), add(0.3, 2.15, 0.5)
  -- a flammable fuel tank node at the back (25)
  local ft = add(0.3, -1.6, 0.35)
  FT = ft
  -- v2.61: a door panel node on the right side (26) where the pistol hits, with plain beams to the side's corners
  DOOR = add(0.9, -1.45, 0.65)
  v = { data = { nodes = {}, triangles = {}, beams = {}, wheels = {} } }
  for c, p in ipairs(NODES) do v.data.nodes[c] = { cid = c, name = 'n' .. c } end
  local function tri(a, b, c) v.data.triangles[#v.data.triangles + 1] = { id1 = a, id2 = b, id3 = c, cid = #v.data.triangles } end
  -- the box's faces (two triangles each)
  tri(1, 2, 6) tri(1, 6, 5) tri(3, 4, 8) tri(3, 8, 7) tri(1, 3, 7) tri(1, 7, 5) tri(2, 4, 8) tri(2, 8, 6) tri(5, 6, 8) tri(5, 8, 7) tri(1, 2, 4) tri(1, 4, 3)
  local function beam(a, b, extra) local t = extra or {} t.id1 = a t.id2 = b t.cid = #v.data.beams v.data.beams[#v.data.beams + 1] = t end
  beam(g1, g3, { deformSwitches = { [7] = { mesh = 'door_L_glass', deformGroup = 'sidewindow_L_break', dmgMat = 'invis' } } })
  beam(g2, g4, { deformSwitches = { [7] = { mesh = 'door_L_glass', deformGroup = 'sidewindow_L_break', dmgMat = 'invis' } } })
  beam(r1, r2, { deformGroup = 'radiator_damage' })
  beam(DOOR, 2) beam(DOOR, 6) beam(DOOR, 4) beam(2, 6)
  BEAMRL = {}
  v.data.wheels[0] = { name = 'FL', node1 = w1a, node2 = w1b, radius = 0.33, tireWidth = 0.25 }
  v.data.wheels[1] = { name = 'FR', node1 = w2a, node2 = w2b, radius = 0.33, tireWidth = 0.25 }
  obj = {}
  function obj:getID() return 42 end
  function obj:getPosition() return { x = OPOS.x, y = OPOS.y, z = OPOS.z } end
  function obj:getNodePosition(c) local p = NODES[c] return { x = p[1], y = p[2], z = p[3] } end
  function obj:getDirectionVectorUp() return { x = 0, y = 0, z = 1 } end
  function obj:getDirectionVector() return { x = 0, y = 1, z = 0 } end
  function obj:getBeamRestLength(c) local b = v.data.beams[c + 1] local p, q = NODES[b.id1], NODES[b.id2]
    return BEAMRL[c] or math.sqrt((p[1] - q[1]) ^ 2 + (p[2] - q[2]) ^ 2 + (p[3] - q[3]) ^ 2) end
  function obj:setBeamLength(c, l) BEAMRL[c] = l CALLS[#CALLS + 1] = string.format('restlen %d %.4f', c, l) end
  function obj:queueGameEngineLua(s)
    local a = s:match('gtaBridge%.wfx%(%d+, (.*)%) end$')
    if a then REPORT(loadstring('return ' .. a)()) end
  end
  CALLS = {}
  local function call(s) CALLS[#CALLS + 1] = s end
  beamstate = { deformGroupsTriggerBeam = {}, damage = 5000 }
  function beamstate.addPlanet(c, r, m, dt) call(string.format('planet %.3f %.3f %.3f %.3f %.6g %.3f', c.x, c.y, c.z, r, m, dt)) end
  function beamstate.deflateTire(id) call('deflate ' .. id) wheels.wheels[id].isTireDeflated = true end
  function beamstate.triggerDeformGroup(g) call('trigger ' .. g) beamstate.deformGroupsTriggerBeam[g] = 1 end
  wheels = { wheels = { [0] = {}, [1] = {} } }
  fire = { flammableNodes = { [ft] = {}, [e1] = {} }, hotNodes = {} }
  function fire.igniteNode(c) call('ignite ' .. c) end
  function fire.explodeVehicle() call('explodeVehicle') end
  function fire.igniteVehicle() call('igniteVehicle') end
  ENGINE = { engineBlockNodes = { e1, e2 }, deformGroups = { { groupType = 'radiator', groupNames = { 'radiator_damage' } } } }
  function ENGINE:applyDeformGroupDamage(a, t) call(string.format('engine %s %.4f', t, a)) end
  powertrain = { getDevicesByCategory = function(c) if c == 'engine' then return { ENGINE } end return {} end }
  math.randomseed(1)
''')
L.execute(VEH)
calls = lambda: [str(L.globals().CALLS[k]) for k in range(1, len(L.globals().CALLS) + 1)]
def clear(): L.execute('CALLS = {}'); reports.clear()
def run(name, *a):
    f = L.eval('function(n, ...) gtabWpn.run(n, ...) end')
    f(name, *a)
def last(kind):
    for s in reversed(reports):
        d = json.loads(s)
        if d.get('k') == kind: return d
    return None

run('tick', 0.25, 30, 0, L.table_from([500, 1.5, 0.06, 1, 10]))
info = last('info')
check(info and '12 collision triangles, 2 glass/light beams, 2 wheels, 2 engine block nodes, 1 radiator beams, 2 flammable nodes' in info['s'], 'init: the car read (%s)' % (info and info['s']))
# 1. a pistol bullet (1.5 cm dent) into the right door (from x +5 going -x) at y -1.5, z 0.6: on the side triangle at x 0.9
clear()
run('hit', 100 + 5, 200 - 1.5, 50 + 0.6, -1.0, 0.0, 0.0, 1.5, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
h = last('hit'); c = calls()
check(h and h['how'] == 'on a collision triangle' and h['tire'] == '' and h['glass'] == '' and h['en'] == 0 and h['ra'] == 0, 'door hit: a triangle, nothing else broken (%s)' % h)
# v2.61: the dent as BeamNG restores saved damage: the door node's 3 beams get the rest lengths of the shape 1.5 cm in (along -x)
import math
rl = {int(x.split()[1]): float(x.split()[2]) for x in c if x.startswith('restlen')}
DR = int(L.eval('DOOR'))
N = {k: L.eval('NODES')[k] for k in (2, 4, 6, DR)}
def dist(a, b): return math.sqrt(sum((a[i] - b[i]) ** 2 for i in (1, 2, 3)))
door1 = {1: N[DR][1] - 0.015, 2: N[DR][2], 3: N[DR][3]}
want = {3: dist(door1, N[2]), 4: dist(door1, N[6]), 5: dist(door1, N[4])}
check(sorted(rl) == [3, 4, 5] and all(abs(rl[k] - want[k]) < 1e-3 for k in want), 'v2.61: the 3 door beams get the dented rest lengths, nothing else (%s vs %s)' % (rl, want))
check(not any(x.startswith('planet') for x in c), 'v2.61: no push on the car for it')
# the rest lengths did nothing here (this car has no physics): after 3 frames, pushes at the door node, 3 cm outside it
clear()
for k in range(4): run('step')
pl = [x for x in calls() if x.startswith('planet')]
check(len(pl) == 1, 'v2.61: rest lengths showed nothing after 3 frames: one push (%s)' % pl)
if pl:
    p = [float(v) for v in pl[0].split()[1:]]
    acc = -p[4] * 6.674e-11 / 0.03 ** 2
    check(abs(p[0] - (100 + 0.9 + 0.03)) < 1e-3 and abs(p[1] - (200 - 1.45)) < 1e-3 and abs(p[2] - 50.65) < 1e-3 and abs(acc - 3000) < 1, 'v2.61: 3 cm outside the door node, 3000 m/s2 there (%s, %.0f)' % (pl[0], acc))
clear()
run('step'); run('step')
pl = [x for x in calls() if x.startswith('planet')]
check(len(pl) == 1 and abs(-float(pl[0].split()[5]) * 6.674e-11 / 0.03 ** 2 - 6000) < 1, 'v2.61: not dented yet: the next push twice as hard (%s)' % pl)
# the door gives: 1.6 cm in -> done, reported
L.execute('NODES[DOOR][1] = NODES[DOOR][1] - 0.016')
clear()
run('step'); run('step')
d = last('dent')
check(d and 'dent 1.6 cm deep (wanted 1.5) by 2 pushes' in d['s'] and 'rest lengths alone 0.0 cm' in d['s'] and not any(x.startswith('planet') for x in calls()), 'v2.61: dented: reported, no more pushes (%s)' % d)
L.execute('NODES[DOOR][1] = NODES[DOOR][1] + 0.016')
# 2. into the front left tyre (from x -5 at the hub, z 0.2: below the hub, on the tread)
clear()
run('hit', 100 - 5, 200 + 1.4, 50 + 0.15, 1.0, 0.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
check('deflate 0' in calls() and last('hit')['tire'] == 'FL', 'tyre hit: FL popped (%s, %s)' % (calls(), last('hit')))
clear()
run('hit', 100 - 5, 200 + 1.4, 50 + 0.15, 1.0, 0.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
check('deflate 0' not in calls(), 'a popped tyre is not popped again')
# 3. into the left window (z 1.05, y 0)
clear()
run('hit', 100 - 5, 200 + 0.0, 50 + 1.05, 1.0, 0.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_RIFLE')
check('trigger sidewindow_L_break' in calls() and last('hit')['glass'] == 'sidewindow_L_break', 'window hit: the left side window breaks (%s)' % calls())
run('tick', 0.25, 30, 0, L.table_from([500, 1.5, 0.06, 1, 10]))
dg = last('dg')
check(dg and dg['g'] == 'sidewindow_L_break', 'tick: GTA told the broken group (%s)' % dg)
# 4. into the engine bay from above (x 0, y 1.75), 12 hits -> the engine catches fire
clear()
for k in range(12):
    run('hit', 100, 200 + 1.75, 50 + 5, 0.0, 0.0, -1.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
c = calls()
check(c.count('engine main 0.0020') == 12, 'engine hits: 12 x applyDeformGroupDamage main (%d)' % c.count('engine main 0.0020'))
check(any(x.startswith('ignite ') for x in c) and last('ignite') is not None, 'after 12 engine hits the engine catches fire (%s)' % [x for x in c if 'ignite' in x])
# 5. the radiator (front, y 2.15... from the front going -y at z 0.5)
clear()
run('hit', 100, 200 + 6, 50 + 0.5, 0.0, -1.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
check('engine radiator 0.0100' in calls(), 'radiator hit: radiator damage (%s)' % calls())
# 6. a miss (passes over the car)
clear()
run('hit', 100 + 5, 200, 50 + 3, -1.0, 0.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
check(last('hit') and last('hit')['how'].startswith('missed') and not calls(), 'a bullet over the car: missed, nothing done (%s)' % last('hit'))
# 7. a blast 2 m behind it on the ground, ignite 1, glass within 4 m
clear()
L.execute('beamstate.deformGroupsTriggerBeam = {}')
run('blast', 100, 200 - 4.2, 50, 700.0, 1.2, 0.05, 1.0, 4.0, 'a grenade')
c = calls(); b = last('blast')
check(any(x.startswith('planet 100.000 195.800 50.000 1.200') for x in c), 'blast: a planet where it went off (%s)' % c)
check('ignite %d' % L.eval('FT') in c, 'blast: it set the fuel tank on fire (%s)' % c)
check(b and 'pushed with 700' in b['s'], 'blast reported (%s)' % b)
# 8. fire: the tank burning -> fire reports, after 30 s the car blows up; CarExplosion=beamng -> explodeVehicle too
clear()
L.execute('fire.hotNodes[FT] = { intensity = 0.4 }')
for k in range(130):
    run('tick', 0.25, 30, 1, L.table_from([500, 1.5, 0.06, 1, 10]))
fr = [json.loads(s) for s in reports if json.loads(s).get('k') == 'fire']
boom = last('boom'); c = calls()
check(fr and fr[0]['on'] == 1 and abs(fr[0]['p'][0] - 100.3) < 1e-3 and abs(fr[0]['p'][1] - 198.4) < 1e-3, 'fire: GTA told where it burns (%s)' % (fr[:1],))
check(boom and boom['why'].startswith('it burned 30') and abs(boom['p'][2] - 50) < 1e-3, 'after burning 30 s it blows up (%s)' % boom)
check('explodeVehicle' in c and any(x.startswith('planet 100.000 200.000 49.400 1.500') for x in c), 'CarExplosion=beamng: BeamNG\'s explodeVehicle + the car\'s own blast from under it (%s)' % c[:4])
check(sum(1 for s in reports if json.loads(s).get('k') == 'boom') == 1, 'it blows up once')
# v2.7: the wreck: tyres burst, deep dents, the report
wr = [json.loads(x) for x in reports if json.loads(x).get('k') == 'info' and 'wreck (v2.7)' in json.loads(x).get('s', '')]
check(wr and sum(1 for x in c if x.startswith('deflate')) >= 2 and 'deep dents' in wr[0]['s'], 'v2.7 wreck: every tyre burst, dents, reported (%s)' % (wr[:1],))
# v2.7: a GTA car's contact: a dent at the contact (rest lengths change)
clear()
run('contact', 100 + 0.95, 200 - 1.5, 50 + 0.6, -1.0, 0.0, 0.0, 0.05)
check(any(x.startswith('restlen') for x in calls()) and not any('error' in json.loads(x).get('s', '') for x in reports), 'v2.7 contact: dented (%s)' % calls()[:3])
# a fireball (fuel vapour) blows it up at once
L.execute('gtabWpn.boomed = false gtabWpn.burnT = 0 fire.hotNodes[FT] = { intensity = 12 }')
clear()
run('tick', 0.25, 30, 0, L.table_from([500, 1.5, 0.06, 1, 10]))
check(last('boom') and 'fireball' in last('boom')['why'] and 'explodeVehicle' not in calls(), 'a fireball blows it up at once (no explodeVehicle on top) (%s)' % last('boom'))
# the fire out: one "off"; repaired (damage 0): timers start over
L.execute('fire.hotNodes = {} beamstate.damage = 0')
clear()
run('tick', 0.25, 30, 0, L.table_from([500, 1.5, 0.06, 1, 10]))
check(last('fire') and last('fire')['on'] == 0 and L.eval('gtabWpn.boomed') is False, 'fire out told; repaired: it can blow up again (%s)' % last('fire'))
# 9. errors are reported once, not thrown
clear()
L.execute('beamstate.deflateTire = nil wheels.wheels[1].isTireDeflated = nil')
run('hit', 100 + 5, 200 + 1.4, 50 + 0.15, -1.0, 0.0, 0.0, 10.0, 1.0, 1.0, 0.002, 0.01, 12, 'GROUP_PISTOL')
check(last('info') and 'error in the car (hit)' in last('info')['s'], 'an error in the car\'s Lua is reported (%s)' % last('info'))
# v2.7: the wreck breaks the doors' / hood's / bumpers' break groups (BeamNG's breakBreakGroup, else each beam) and nothing else
clear()
L.execute('''for i, b in ipairs(v.data.beams) do b.breakGroup = nil end
  v.data.beams[1].breakGroup = 'hood_hinge'; v.data.beams[2].breakGroup = { 'door_RL_hinge', 'x' }; v.data.beams[3].breakGroup = 'wheel_FR'
  function obj:breakBeam(c) CALLS[#CALLS + 1] = 'break ' .. c end
  math.randomseed(3)''')
for _ in range(20): run('wreck')
c = calls()
br = set(x for x in c if x.startswith('break '))
check(br and br <= {'break %d' % int(L.eval('v.data.beams[1].cid')), 'break %d' % int(L.eval('v.data.beams[2].cid'))}, 'v2.7 wreck: only door / hood / bumper groups break (%s)' % sorted(br))
L.execute('beamstate.breakBreakGroup = function(g) CALLS[#CALLS + 1] = "bbg " .. g end')
clear()
for _ in range(20): run('wreck')
check(any(x == 'bbg hood_hinge' for x in calls()) and not any(x.startswith('break ') for x in calls()), 'v2.7 wreck: BeamNG\'s breakBreakGroup used when it is there')
# the export's deformGroups (wpn.exportDg)
i = src.index('function wpn.exportDg(vdata)'); j = src.index('\nend\n', i)
G = lj.LuaRuntime(unpack_returned_tuples=True)
G.execute('wpn = {}\n' + src[i:j + 5])
dg, n = G.eval('''wpn.exportDg({ flexbodies = { { mesh = 'windshield', deformGroup = 'windshield_break', deformMaterialDamaged = 'windshield_dmg' }, { mesh = 'body' } },
  beams = { { deformSwitches = { [3] = { mesh = 'sidewindow_L', deformGroup = 'sidewindow_L_break', dmgMat = 'invis' } } }, { deformSwitches = { [4] = { mesh = 'windshield', deformGroup = 'other', dmgMat = 'x' } } } } })''')
check(n == 2 and dg['windshield'][1] == 'windshield_break' and dg['windshield'][2] == 'windshield_dmg' and dg['sidewindow_L'][2] == 'invis' and dg['body'] is None,
      'export: each mesh\'s deformGroup and broken material (flexbodies first, then the beams\' deformSwitches)')
print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
