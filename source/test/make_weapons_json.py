#!/usr/bin/env python3
# v2.6: the test weapons file (run in test/run): the plugin's built-in one, burning 6 s, the grenade always igniting, a pistol
# bullet past your head taking 5 health
import re
s = open('../../src/GtaBeamD1.cpp').read()
js = re.search(r'kWeaponsDefault = R"JSON\((.*?)\)JSON"', s, re.S).group(1)
js = js.replace('"burnToExplodeS": 30', '"burnToExplodeS": 6')
js = js.replace('"name": "grenade", "accel": 700, "r": 1.2, "dt": 0.05, "ignite": 0.4', '"name": "grenade", "accel": 700, "r": 1.2, "dt": 0.05, "ignite": 1.0')
js = js.replace('"group": "GROUP_PISTOL", "dentCm": 0.75, "tire": 1, "glass": 1, "engine": 0.001, "radiator": 0.005, "driver": 15', '"group": "GROUP_PISTOL", "dentCm": 0.75, "tire": 1, "glass": 1, "engine": 0.001, "radiator": 0.005, "driver": 5')
assert '"driver": 5' in js and '"burnToExplodeS": 6' in js and '"ignite": 1.0, "glassR": 6' in js
open('GtaBeamD1_weapons.json', 'w').write(js)
