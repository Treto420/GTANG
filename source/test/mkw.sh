#!/bin/sh
# v2.5: make or refresh a sandbox build workspace $W from a checkout (the repo or a worktree), for build_all.sh:
#   sh source/test/mkw.sh <checkout> <W> <deps dir with cgltf.h stb_image.h bcdec.h ScriptHookV.lib [natives.json]>
# $W/src gets the checkout's sources (copied fresh each time) + the third-party files, $W/test the test files (run/ kept),
# $W/kit/GTA the kit ini. Then: cd $W/test && W=$W sh build_all.sh
set -e
C=$(cd "$1" && pwd); W=$2; D=$3
mkdir -p "$W/src" "$W/test/run" "$W/kit/GTA"
cp "$C/source/GtaBeamD1.cpp" "$C/source/carmodel.cpp" "$C/source/carmodel.h" "$C/source/shv.h" "$C/BeamNG/gtaBridge.lua" "$W/src/"
for f in cgltf.h stb_image.h bcdec.h ScriptHookV.lib; do [ -f "$W/src/$f" ] || cp "$D/$f" "$W/src/"; done
for f in "$C"/source/test/*; do [ -f "$f" ] && cp "$f" "$W/test/"; done
[ -f "$W/test/natives.json" ] || { [ -f "$D/natives.json" ] && cp "$D/natives.json" "$W/test/"; } || true
cp "$C/GTA/GtaBeamD1.ini" "$W/kit/GTA/"
for f in "$C"/GTA/GtaBeamD1_*.json; do [ -f "$f" ] && cp "$f" "$W/test/run/"; done
echo "workspace $W ready from $C"
