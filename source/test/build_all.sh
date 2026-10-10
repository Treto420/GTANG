#!/bin/sh
# v2.41/2.5 sandbox build + tests. Layout: $W/src (GtaBeamD1.cpp, carmodel.*, shv.h, cgltf.h, stb_image.h, bcdec.h,
# ScriptHookV.lib, gtaBridge.lua), $W/test (these files), $W/test/run (Wine run folder).
# Needs: pip install ziglang==0.14.1 lupa numpy; apt install wine64 xvfb.
set -e
W=${W:-/home/claude/w}
Z="python3 -m ziglang c++ -target x86_64-windows-gnu -std=c++17 -O2"
cd $W/test
# every native hash checked against the native database (a wrong one closes GTA) + the list for the fake
python3 check_natives.py $W/src/GtaBeamD1.cpp || exit 1
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
# the ini with CarFile pointing at the test car
# v2.5: light rendering (no GPU here: llvmpipe draws the car's shadows / ambient occlusion / learned reflections at ~3 fps on
# 2 cores; the scenario timings expect ~15 fps)
sed 's/^CarFile=$/CarFile=car\\car.gltf/; s/^SelfShadows=1/SelfShadows=0/; s/^AmbientOcclusion=1/AmbientOcclusion=0/; s/^WorldShadows=1/WorldShadows=0/; s/^EdgeSmoothing=1/EdgeSmoothing=0/; s/^ShadowMode=own/ShadowMode=off/; s/^Environment=world/Environment=sky/' $W/kit/GTA/GtaBeamD1.ini > run/GtaBeamD1.ini
python3 lua_input_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_drive_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_world_test.py $W/src/gtaBridge.lua | tail -1
python3 lua_dents_test.py $W/src/gtaBridge.lua | tail -1
cp script_*.txt run/ 2>/dev/null || true
echo "end to end (v2.5, ~40 min): sh run_all_e2e.sh  (one scenario: cd run; sh ../run_e2e.sh <script> <secs> [FAKE_...=v]; python3 ../check_world.py [hole] or ../check_v241.py <alttab|fast|radio|radiooff|disk|ring> or ../check_v243.py <on|under|pause|static>)"
