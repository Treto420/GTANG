#!/bin/sh
# v2.75: the traffic, people and prop scenarios (about 20 minutes). Run from test/: sh run_v275.sh
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
echo "== twinstale"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_TWIN_STALE=3; python3 ../check_v275.py twinstale; python3 ../check_v27.py twins | tail -3
echo "== twins";     r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1; python3 ../check_v27.py twins | tail -1; python3 ../check_v273.py dents | tail -1
echo "== semi";      r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_TRAFFIC_LONG=1; python3 ../check_v274.py semi
echo "== pedout";    r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PED=1 FAKE_PEDNEAR=1; python3 ../check_v275.py pedout
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^TrafficMode=.*/TrafficMode=push/' GtaBeamD1.ini
echo "== push";      r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PED=1; python3 ../check_v275.py push; python3 ../check_v27.py traffic | tail -1; python3 ../check_v273.py ped | tail -1
sed -i 's/^TrafficMode=.*/TrafficMode=despawn/' GtaBeamD1.ini
echo "== despawn";   r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1; python3 ../check_v274.py despawn | tail -1
cp ini_saved.txt GtaBeamD1.ini
echo "== props";     r script_props_fixed.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1; python3 ../check_v25.py props | tail -1
echo "== propstuck"; r script_props_fixed.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1 FAKE_PROPSTUCK=1; python3 ../check_v273.py propstuck | tail -1
echo "== propread";  r script_props_fixed.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1 FAKE_PROPSTUCK=1 FAKE_PROPREAD=1; python3 ../check_v275.py propread
echo "== world";     r script_world.txt 90 FAKE_RAYCAP=12; python3 ../check_world.py | tail -1; python3 ../check_v274.py calm | tail -1
