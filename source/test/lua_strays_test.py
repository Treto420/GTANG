#!/usr/bin/env python3
"""lua_strays_test.py - v2.74.1: the export leaves out mesh nodes outside the glTF scene (another mod's objects, e.g. DARS's
driver pieces 100 m away), keeps everything in the scene, and does nothing if most meshes would go.
Runs dropStrays from gtaBridge.lua (cut out of the file) in Lua. Usage: python3 lua_strays_test.py <gtaBridge.lua>"""
import re, sys
from lupa import LuaRuntime

src = open(sys.argv[1]).read()
m = re.search(r'^local function dropStrays\(root\)\n.*?^end\n', src, re.S | re.M)
assert m, 'dropStrays not found'
lua = LuaRuntime()
f = lua.execute(m.group(0) + 'return dropStrays')
fails = []
def check(c, what):
    print(('PASS ' if c else 'FAIL ') + what)
    if not c: fails.append(what)

# the car: scene -> part 2 (children 0, 1), part 3 (no children), wheel 6; nodes 4, 5 = another mod's, outside the scene
root = lua.eval('''{ scene = 0, scenes = { { nodes = { 2, 3, 6 } } }, nodes = {
  { name = 'body', mesh = 0 }, { name = 'glass', mesh = 1 }, { name = 'chassis', children = { 0, 1 } }, { name = 'main' },
  { name = 'da_walkerdrv_Head_mesh', mesh = 2 }, { name = 'da_walkerdrv_LeftFoot_mesh', mesh = 3 }, { name = 'wheel', mesh = 4 } } }''')
n, names = f(root)
N = root.nodes
check(n == 2 and N[5].mesh is None and N[6].mesh is None, 'the two pieces outside the scene lose their mesh (%s: %s)' % (n, list(names.values())))
check(N[1].mesh == 0 and N[2].mesh == 1 and N[7].mesh == 4, 'the car\'s meshes in the scene stay')
# most meshes outside the scene: nothing changed
root2 = lua.eval('''{ scenes = { { nodes = { 0 } } }, nodes = { { name = 'a', mesh = 0 }, { name = 'b', mesh = 1 }, { name = 'c', mesh = 2 } } }''')
n2, names2 = f(root2)
check(n2 == 0 and root2.nodes[2].mesh == 1 and root2.nodes[3].mesh == 2, 'most of the car outside the scene: left as it is (%s)' % list(names2.values()))
# no scene: nothing changed
root3 = lua.eval('''{ nodes = { { name = 'a', mesh = 0 } } }''')
n3, _ = f(root3)
check(n3 == 0 and root3.nodes[1].mesh == 0, 'no scene: left as it is')
print('LUA STRAYS TEST:', 'ALL PASS' if not fails else '%d FAILED' % len(fails))
sys.exit(1 if fails else 0)
