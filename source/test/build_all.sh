#!/bin/sh
# v2.41/2.5/2.6 sandbox build + tests. Layout: $W/src (GtaBeamD1.cpp, carmodel.*, shv.h, cgltf.h, stb_image.h, bcdec.h,
# ScriptHookV.lib, gtaBridge.lua), $W/test (these files), $W/test/run (Wine run folder).
# Needs: pip install ziglang==0.14.1 lupa numpy; apt install wine64 xvfb.
set -e
W=${W:-/home/claude/w}
Z="python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2"
cd $W/test
# every native hash checked against the native database (a wrong one closes GTA) + the list for the fake
python3 check_natives.py $W/src/GtaBeamD1.cpp || exit 1
# v2.63: the built-in weapons file (written to GTA's folder when missing) must be valid JSON
python3 -c "import json,sys; s=open(sys.argv[1]).read(); i=s.index('kWeaponsDefault = R\"JSON(')+25; json.loads(s[i:s.index(')JSON\"',i)])" $W/src/GtaBeamD1.cpp || { echo "built-in weapons JSON broken"; exit 1; }
python3 check_json.py $W/src/GtaBeamD1.cpp || exit 1   # v2.7
# v2.72: every lock taken is let go (a lost unlock in the receiver froze GTA's script on the first repair)
python3 -c "import re,sys,collections as C; s=open(sys.argv[1]).read(); a=C.Counter(re.findall(r'AcquireSRWLock(\w+\(&[\w.]+\))',s)); r=C.Counter(re.findall(r'ReleaseSRWLock(\w+\(&[\w.]+\))',s)); sys.exit(0 if a==r else 'locks not paired: %s' % (a-r or r-a))" $W/src/GtaBeamD1.cpp || exit 1
cp native_hashes.txt run/ 2>/dev/null || true
cd $W/src
$Z -c -o carmodel.o carmodel.cpp 2>&1 | grep -E "error|warning" || true
$Z -c -o GtaBeamD1.o GtaBeamD1.cpp 2>&1 | grep -E "error|warning" || true
$Z -shared -o GtaBeamD1.asi GtaBeamD1.o carmodel.o ScriptHookV.lib -luser32 -lws2_32 2>&1 | grep -E "error|warning" || true
cd $W/test
mkdir -p run
cp native_hashes.txt run/
$Z -shared -o run/ScriptHookV.dll fakeshv.cpp
$Z -shared -o run/xinput1_4.dll fakexinput.cpp
$Z -o run/host.exe host.cpp -ld3d11 -ld3dcompiler_47 -ldxgi -luser32 -lgdi32
cp $W/src/GtaBeamD1.asi run/
python3 make_car_plain.py run/car
CAR_GLASS=1 python3 make_car_plain.py run/car_glass   # v2.6: the weapons scenario's car (a side window, a headlight)
# the ini with CarFile pointing at the test car
# v2.5: light rendering (no GPU here: llvmpipe draws the car's shadows / ambient occlusion / learned reflections at ~3 fps on
# 2 cores; the scenario timings expect ~15 fps)
sed 's/^CarFile=$/CarFile=car\\car.gltf/; s/^SelfShadows=1/SelfShadows=0/; s/^AmbientOcclusion=1/AmbientOcclusion=0/; s/^WorldShadows=1/WorldShadows=0/; s/^EdgeSmoothing=1/EdgeSmoothing=0/; s/^ShadowMode=own/ShadowMode=off/; s/^Environment=world/Environment=sky/' $W/src/GtaBeamD1.ini > run/GtaBeamD1.ini
# v2.74.1: compiled by LuaJIT 2.1 like BeamNG does (its 200-locals limit per function: the main chunk is close to it)
python3 -c "import sys; from lupa.luajit21 import LuaRuntime as L; ok, e = L().eval('function(s) local f, e = loadstring(s, \"gtaBridge.lua\"); return f ~= nil, e end')(open(sys.argv[1]).read()); sys.exit(0 if ok else 'LuaJIT: %s' % e)" $W/src/gtaBridge.lua || exit 1
python3 lua_input_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_drive_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_world_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_dents_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_wpn_test.py $W/src/gtaBridge.lua | tail -1   # v2.6: the car-side weapons code
python3 lua_strays_test.py $W/src/gtaBridge.lua | tail -1   # v2.74.1: other mods' objects left out of the export
python3 locator_test.py | tail -1   # v2.53: the dents' triangles of nodes (car loader + the shader's math)
cp script_*.txt run/ 2>/dev/null || true
echo "end to end (v2.5, ~40 min): sh run_all_e2e.sh  (one scenario: cd run; sh ../run_e2e.sh <script> <secs> [FAKE_...=v]; python3 ../check_world.py [hole] or ../check_v241.py <alttab|fast|radio|radiooff|disk|ring> or ../check_v243.py <on|under|pause|static>)"
