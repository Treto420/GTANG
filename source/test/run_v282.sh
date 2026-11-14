#!/bin/sh
# v2.82: PuppetPassCar scenarios against the fake GTA (about 4 minutes). Run from test/: sh run_v282.sh
# Code paths only (the pair set not to collide at the teleport, the hit car's line, PuppetPassCar=0 = v2.81); not GTA's physics.
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^PuppetSolid=.*/PuppetSolid=1/; s/^PuppetNoMap=.*/PuppetNoMap=1/; s/^PuppetPassCar=.*/PuppetPassCar=1/; s/^PuppetSnapM=.*/PuppetSnapM=0.5/' GtaBeamD1.ini   # (0.5: the fake's crash leaves the car 0.85 m off)
echo "== pass"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_PSKICK_MS=14 FAKE_TCVANISH=1; python3 ../check_v282.py pass
sed -i 's/^PuppetPassCar=.*/PuppetPassCar=0/' GtaBeamD1.ini
echo "== v281"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1 FAKE_PSKICK_MS=14 FAKE_TCVANISH=1; python3 ../check_v282.py v281
cp ini_saved.txt GtaBeamD1.ini
