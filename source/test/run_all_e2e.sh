#!/bin/sh
# v2.43/2.5/2.51/2.6/2.62/2.63/2.7: every end-to-end scenario, one after another (about 60 minutes). Run from test/: sh run_all_e2e.sh
W=${W:-/home/claude/w}
cd $W/test/run
cp ../script_*.txt . 2>/dev/null
r() { script=$1; secs=$2; shift 2; sh ../run_e2e.sh $script $secs "$@" >/dev/null 2>&1; }
r script_world.txt 90 FAKE_RAYCAP=12;                  python3 ../check_world.py | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_ROUGH=1;     FAKE_ROUGH=1 python3 ../check_world.py | tail -1
python3 -c "import re;print(sum(int(x) for x in re.findall(r'rays: (\d+) started', open('GtaBeamD1.log').read())))" > run1_rays.txt
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_ROUGH=1 KEEPMAP=1; python3 ../check_v241.py disk | tail -1
r script_hole.txt 90 FAKE_RAYCAP=12 FAKE_HOLE=1;       python3 ../check_world.py hole | tail -1
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
# v2.5: walls as boxes, double decks, the pad's modifiers, gravity / drive modes, props, the radio wheel's slow motion, dents
r script_walls.txt 90 FAKE_RAYCAP=12 FAKE_WALLS=1;                python3 ../check_v25.py walls | tail -1
r script_walls.txt 90 FAKE_RAYCAP=12 FAKE_WALLS=1 FAKE_TWOSIDED=1; python3 ../check_v25.py walls | tail -1
r script_walls.txt 90 FAKE_RAYCAP=12 FAKE_CREST=1;               python3 ../check_v25.py crest | tail -1   # v2.54: no wall at a crest's low joint
r script_walls.txt 90 FAKE_RAYCAP=12 FAKE_PITS=1;                python3 ../check_v25.py pits | tail -1    # v2.55: narrow pits in the road filled
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=2;             python3 ../check_v25.py deck2 | tail -1
r script_padcombo.txt 130 FAKE_RAYCAP=12;                         python3 ../check_v25.py padcombo | tail -1
r script_extras.txt 70 FAKE_RAYCAP=12;                            python3 ../check_v25.py extras | tail -1
r script_props_fixed.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1;         python3 ../check_v25.py props | tail -1
r script_props.txt 150 FAKE_RAYCAP=12 FAKE_PROPS=1;               python3 ../check_v25.py physprops | tail -1
r script_slowmo.txt 90 FAKE_RAYCAP=12 FAKE_VMAX=15;               python3 ../check_v25.py slowmo | tail -1
r script_dents.txt 200 FAKE_DENT_AFTER=15 FAKE_REPAIR_FILE=do_repair.txt "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; python3 ../check_v25.py dents | tail -1
# v2.54: a node flung 16 m away (a part left on the road): no sheet stretched to it
r script_dents.txt 200 FAKE_DENT_AFTER=15 FAKE_DENT_FAR=1 FAKE_REPAIR_FILE=do_repair.txt "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; FAKE_DENT_FAR=1 python3 ../check_v25.py dents | tail -1
# v2.51: off a 10 m drop: the ground where it lands is built first, no reset
r script_jump.txt 120 FAKE_RAYCAP=12 FAKE_VMAX=15 FAKE_JUMP=1 FAKE_SYNCUS=10; python3 ../check_v25.py jump | tail -1
# v2.62: an overpass the rays meet first (WorldUpM=7: from 7 m up over the 6 m deck): the road under it, not the deck; a 1.1 m
# gap across a deck (an expansion joint) joined; a crash holding the car 4 m under GTA's ground for 1.5 s: not put back
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^WorldUpM=.*/WorldUpM=7/' GtaBeamD1.ini
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_STARTX=12;  python3 ../check_v243.py under overpass | tail -1
cp ini_saved.txt GtaBeamD1.ini
r script_bridge.txt 130 FAKE_RAYCAP=12 FAKE_BRIDGE=1 FAKE_JOINT=1;   python3 ../check_v243.py on joint | tail -1
r script_world.txt 90 FAKE_RAYCAP=12 "FAKE_DIP=3 1.5 4";             FAKE_DIP="3 1.5 4" python3 ../check_world.py | tail -1
# v2.6: weapons: the car with glass, the test weapons file (burns 6 s, the grenade always sets it on fire, a pistol bullet past your
# head takes 5 health); three shooters and a grenade (FAKE_WEAPONS=1)
python3 ../make_weapons_json.py
cp GtaBeamD1.ini ini_saved.txt
sed -i 's/^CarFile=.*/CarFile=car_glass\\car.gltf/' GtaBeamD1.ini
r script_weapons.txt 150 FAKE_WEAPONS=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; python3 ../check_v26.py weapons | tail -1
r script_foot.txt 120 FAKE_FOOT=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; python3 ../check_v26.py foot | tail -1   # v2.61: out of the car, drive mode on
# v2.63: your rocket on the car: it blows up at once; 5 shots into a headlight: cracked, then shattered out after 3 more
r script_foot.txt 120 FAKE_FOOT=1 FAKE_RPG=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; python3 ../check_v26.py rpg | tail -1
r script_foot.txt 120 FAKE_FOOT=1 FAKE_SHATTER=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"; python3 ../check_v26.py shatter | tail -1
cp ini_saved.txt GtaBeamD1.ini; rm -f GtaBeamD1_weapons.json
# v2.7: a parked GTA car on the way (both pushed, GTA's damaged, BeamNG's slowed); the ground 16 m below beside the road (empty tiles
# near the standing car measured again, the second time with the tall window, kept); the crash-hurt rule
r script_world.txt 90 FAKE_RAYCAP=12 FAKE_TRAFFIC=1; python3 ../check_v27.py traffic | tail -1
r script_ring.txt 120 FAKE_RAYCAP=12 FAKE_DROP=1;    python3 ../check_v27.py drop | tail -1
python3 ../check_v27.py crash | tail -1
