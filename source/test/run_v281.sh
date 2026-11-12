#!/bin/sh
# v2.81: PuppetNoMap scenarios against the fake GTA (about 5 minutes). Run from test/: sh run_v281.sh
# Code paths only (the native called every tick, gravity, the contact log, the F10-off check, PuppetNoMap=0 = v2.77); not GTA's physics.
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^PuppetSolid=.*/PuppetSolid=1/; s/^PuppetNoMap=.*/PuppetNoMap=1/' GtaBeamD1.ini
echo "== nomap"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_PSMAP=1 FAKE_STOP_AT=14 FAKE_DENT_AFTER=14; python3 ../check_v281.py nomap
echo "== gta";   r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1; python3 ../check_v281.py gta | tail -2
sed -i 's/^PuppetNoMap=.*/PuppetNoMap=0/' GtaBeamD1.ini
echo "== map77"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_PSMAP=1 FAKE_STOP_AT=14 FAKE_DENT_AFTER=14; python3 ../check_v281.py map77 | tail -2
sed -i 's/^PuppetNoMap=.*/PuppetNoMap=1/; s/^PuppetSlowMoFix=.*/PuppetSlowMoFix=1/' GtaBeamD1.ini
echo "== slowmo fix"; r script_slowmo.txt 90 FAKE_RAYCAP=12 FAKE_VMAX=15; python3 ../check_v281.py slowmo 1 | tail -2
sed -i 's/^PuppetSlowMoFix=.*/PuppetSlowMoFix=0/' GtaBeamD1.ini
echo "== slowmo v2.77"; r script_slowmo.txt 90 FAKE_RAYCAP=12 FAKE_VMAX=15; python3 ../check_v281.py slowmo 0 | tail -2
cp ini_saved.txt GtaBeamD1.ini
echo "== hurtkey"; r script_hurt.txt 40; python3 ../check_v281.py hurtkey | tail -2
