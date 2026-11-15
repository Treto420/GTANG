#!/bin/sh
# v2.84: TruckProxies scenarios against the fake GTA and the fake BeamNG (about 8 minutes). Run from test/: sh run_v284.sh
# Code paths only (the truck boxes asked for, their weight, the 35% length rule, the contact log, TruckProxies=0 = v2.83); not
# either game's physics.
# Needs $W/src/gtab_prop.jbeam (a copy of BeamNG/gtabridge/vehicles/gtab_prop/gtab_prop.jbeam): the fake BeamNG reads the box's
# weight limit from it.
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^PuppetSolid=.*/PuppetSolid=1/; s/^PuppetNoMap=.*/PuppetNoMap=1/; s/^TrafficProxies=.*/TrafficProxies=4/; s/^TruckProxies=.*/TruckProxies=2/' GtaBeamD1.ini
echo "== truck";    r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_TRAFFIC_LONG=1; python3 ../check_v284.py truck
echo "== car";      r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1; python3 ../check_v284.py car | tail -6
echo "== oldjbeam"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_TRAFFIC_LONG=1 FAKE_GM_MAX=5000; python3 ../check_v284.py oldjbeam | tail -4
sed -i 's/^TruckProxies=.*/TruckProxies=1/' GtaBeamD1.ini
echo "== none";     r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_TRAFFIC_LONG=1; python3 ../check_v284.py none | tail -6
sed -i 's/^TruckProxies=.*/TruckProxies=0/' GtaBeamD1.ini
echo "== off";      r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_TRAFFIC_LONG=1; python3 ../check_v284.py off | tail -6
cp ini_saved.txt GtaBeamD1.ini
