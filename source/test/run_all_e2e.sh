#!/bin/sh
# v2.43: every end-to-end scenario, one after another (about 25 minutes). Run from test/: sh run_all_e2e.sh
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" >/dev/null 2>&1; }
r script_world.txt 90 FAKE_RAYCAP=12;                  python3 ../check_world.py | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_ROUGH=1;     FAKE_ROUGH=1 python3 ../check_world.py | tail -1
python3 -c "import re;print(sum(int(x) for x in re.findall(r'rays: (\d+) started', open('GtaBeamD1.log').read())))" > run1_rays.txt
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_ROUGH=1 KEEPMAP=1; python3 ../check_v241.py disk | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_HOLE=1;      python3 ../check_world.py hole | tail -1
r script_alttab.txt 90 FAKE_RAYCAP=12 FAKE_COLLWAIT=20; python3 ../check_v241.py alttab | tail -1
r script_fast.txt 100 FAKE_RAYCAP=12 FAKE_VMAX=30 FAKE_ACC=5 FAKE_ROUGH=1 FAKE_FRAMEJITTER=10; python3 ../check_v241.py fast | tail -1
r script_ring.txt 120 FAKE_RAYCAP=12 FAKE_ROUGH=1;     python3 ../check_v241.py ring | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_RADIO=1 FAKE_PHONESTARTS=1; python3 ../check_v241.py radio | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_RADIO=255 FAKE_PHONESTARTS=1; python3 ../check_v241.py radiooff | tail -1
# v2.43: bridges (past the ramp and under the deck, then up the ramp and over it with that saved map, then under again)
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_STARTX=12;            python3 ../check_v243.py under | tail -1; python3 ../check_v243.py static | tail -1
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 KEEPMAP=1;                 python3 ../check_v243.py on keep | tail -1
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_STARTX=12 KEEPMAP=1;  python3 ../check_v243.py under keep | tail -1
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_VMAX=25 FAKE_ACC=5;   python3 ../check_v243.py on | tail -1
r script_pause.txt 120 FAKE_RAYCAP=12 "FAKE_USERPAUSE=36 41" FAKE_REPAIR_AT=43; python3 ../check_v243.py pause | tail -1
