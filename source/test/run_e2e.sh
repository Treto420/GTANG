#!/bin/sh
# v2.41/2.42 end-to-end run in test/run: the real plugin (host.exe + fake GTA) and the real BeamNG mod (fake_beam_lua.py).
# Usage: sh ../run_e2e.sh <script.txt> <seconds> [ENV=value ...]   (env: FAKE_RAYCAP, FAKE_HOLE, FAKE_VMAX, FAKE_RADIO,...)
W=${W:-/home/claude/w}
S=$1; T=$2; shift 2
cd $W/test/run
pgrep -x Xvfb >/dev/null || (Xvfb :99 -screen 0 1280x720x24 >/dev/null 2>&1 &) ; sleep 0.5
rm -f GtaBeamD1_poses.csv GtaBeamD1.log fake_beam_events.txt fake_beam_path.csv fake_beam_tiles.txt fake_puppet.csv fake_hud.txt gtabridge/gtabridge_log.txt GtaBeamD1_materials.txt
for kv in "$@"; do export "$kv"; done
[ -z "$KEEPMAP" ] && rm -rf GtaBeamD1_map   # v2.42: a fresh saved map unless KEEPMAP=1
python3 ../fake_beam_lua.py $T $W/src/gtaBridge.lua > fake_beam_out.txt 2>&1 &
BP=$!
sleep 1
DISPLAY=:99 WINEDLLOVERRIDES=xinput1_4=n timeout $((T + 30)) wine host.exe $S > host_out.txt 2>&1
kill $BP 2>/dev/null; wait $BP 2>/dev/null
grep -c . GtaBeamD1.log
