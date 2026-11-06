#!/bin/sh
# v2.77: PuppetSolid scenarios against the fake GTA (about 6 minutes). Run from test/: sh run_v277.sh
# They test code paths only (engage, the crash read-back, the tc to BeamNG, the turn-frame check, PuppetSolid=0 = v2.76),
# not GTA's physics: the fake's "physics" is ours. Only the in-game test says whether GTA's world reacts the vanilla way.
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" HOST_FRAME_MS=${HOST_FRAME_MS:-55} >/dev/null 2>&1; }
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^PuppetSolid=.*/PuppetSolid=1/' GtaBeamD1.ini; grep -q '^PuppetSolid=' GtaBeamD1.ini || echo 'PuppetSolid=1' >> GtaBeamD1.ini
echo "== ps";          r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1 FAKE_PSKICK=1; python3 ../check_v277.py ps
echo "== frame world"; r script_world.txt 90 FAKE_RAYCAP=12 FAKE_WOBBLE=1 FAKE_ANGLOCAL=0; python3 ../check_v277.py frame world | tail -2
echo "== frame car";   r script_world.txt 90 FAKE_RAYCAP=12 FAKE_WOBBLE=1 FAKE_ANGLOCAL=1; python3 ../check_v277.py frame car | tail -2
echo "== ps hills";    r script_world.txt 90 FAKE_RAYCAP=12 FAKE_ROLL=1; python3 ../check_v277.py calm | tail -1; python3 ../check_v274.py roll | tail -1
cp ini_saved.txt GtaBeamD1.ini
sed -i 's/^PuppetSolid=.*/PuppetSolid=0/; s/^TrafficMode=.*/TrafficMode=despawn/' GtaBeamD1.ini
echo "== off";         r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1; python3 ../check_v277.py off | tail -2; python3 ../check_v274.py despawn | tail -1
cp ini_saved.txt GtaBeamD1.ini
