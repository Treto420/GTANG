#!/usr/bin/env python3
"""v2.7: the plugin's built-in JSON files (written to GTA's folder when missing) must be valid. Usage: check_json.py GtaBeamD1.cpp"""
import json, sys
s = open(sys.argv[1]).read()
for name in ('kWeaponsDefault', 'kTrafficDefault', 'kPropsDefault'):
    i = s.index(name + ' = R"JSON(') + len(name + ' = R"JSON(')
    json.loads(s[i:s.index(')JSON"', i)])
print('built-in JSON ok')
