#!/bin/sh
# v2.85: PropProxies scenarios against the fake GTA and the fake BeamNG (about 5 minutes). Run from test/: sh run_v285.sh
# Code paths only (the pole boxes asked for, the hang test's result, which props get a box, the contact log, the cut when GTA's
# pole breaks, the put-backs, PropProxies=0 = v2.84); not either game's physics.
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^PuppetSolid=.*/PuppetSolid=1/; s/^PuppetNoMap=.*/PuppetNoMap=1/; s/^TrafficProxies=.*/TrafficProxies=4/; s/^TruckProxies=.*/TruckProxies=0/; s/^PropProxies=.*/PropProxies=3/' GtaBeamD1.ini
echo "== pole";   r script_world.txt 90 FAKE_RAYCAP=12 FAKE_PROPS=1 FAKE_POLEFALL=1; python3 ../check_v285.py pole
echo "== noties"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_PROPS=1 FAKE_POLEFALL=1 FAKE_NOTIES=1; python3 ../check_v285.py noties | tail -8
sed -i 's/^PropProxies=.*/PropProxies=0/' GtaBeamD1.ini
echo "== off";    r script_world.txt 90 FAKE_RAYCAP=12 FAKE_PROPS=1 FAKE_POLEFALL=1; python3 ../check_v285.py off | tail -6
cp ini_saved.txt GtaBeamD1.ini
