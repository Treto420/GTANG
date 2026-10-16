#!/usr/bin/env python3
"""check_v26.py - v2.6 end-to-end checks after a run (in test/run):
  foot:    (run_all_e2e.sh: the glass car) sh ../run_e2e.sh script_foot.txt 120 FAKE_FOOT=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"   (v2.61)
  weapons: (run_all_e2e.sh: the car with glass, the test weapons file) sh ../run_e2e.sh script_weapons.txt 150 FAKE_WEAPONS=1 "FAKE_CAM=-4.5 5.5 3.5 -21.5 -140.7"
           three shooters fire at the car (through the cabin, at the front left tyre, into the front), a grenade goes off behind
           it: dents, a popped tyre, broken glass (hidden in GTA), engine and radiator damage, the car set on fire, it blows up
           after 6 s of burning (GTA's car explodes, not taken for a new blast), you get hurt
  rpg / shatter (v2.63): the foot run with FAKE_RPG=1 (a rocket on the car) / FAKE_SHATTER=1 (5 shots into a headlight)
Usage: python3 ../check_v26.py <mode>"""
import os, re, sys
mode = sys.argv[1]
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, mlog, ev, host = rd('GtaBeamD1.log'), rd('gtabridge/gtabridge_log.txt'), rd('fake_beam_events.txt'), rd('host_out.txt')
check('Drive ON on GTA\'s ground' in log, 'drive mode started on GTA\'s ground')

if mode == 'weapons':
    check(re.search(r'weapons: \d+ weapon groups, \d+ explosion types \([^)]*GtaBeamD1_weapons\.json\); the car burns 6 s', log) is not None, 'GTA read the test weapons file (burns 6 s)')
    check('weapons: on: the car burns 6 s' in mlog and 'its own blast 500 m/s2 at 1 m' in mlog, 'the BeamNG mod got the burn time and the car\'s own blast (500, not the blast named "car") from GTA')
    m = re.search(r'the car: (\d+) collision triangles, (\d+) glass/light beams, (\d+) wheels, (\d+) engine block nodes, (\d+) radiator beams, (\d+) flammable nodes', mlog)
    check(m and m.groups() == ('58', '2', '4', '2', '1', '15'), 'the car\'s Lua found its triangles, glass, wheels, engine, radiator, fire nodes (%s)' % ((m.groups() if m else None),))
    check('error in the car' not in mlog, 'no errors in the car\'s weapons code')
    check(re.search(r'proofs entity 100: bullet 1 fire 1 explosion 1', host) is not None and re.search(r'proofs entity 1: bullet 1 fire 1 explosion 1', host) is not None,
          'GTA\'s own damage off the hidden car and you while BeamNG drives')
    hits = re.findall(r'weapons: a bullet \((GROUP_PISTOL)\) hit the drawn car', log)
    check(len(hits) >= 12, 'GTA found the bullets through the drawn car (%d)' % len(hits))
    check('(guessed)' not in log, 'no guessed hits (every shot had its impact)')
    found = re.findall(r'weapons: BeamNG: a bullet hit its car \((on a collision triangle|next to node \d+|missed[^)]*)\)', log)
    check(len(found) >= 12 and sum(1 for f in found if f == 'on a collision triangle') >= 8 and not any(f.startswith('missed') for f in found),
          'BeamNG found them on its car (%d: %d on triangles, %d next to a node, %d missed)' % (len(found), found.count('on a collision triangle'),
          sum(1 for f in found if f.startswith('next')), sum(1 for f in found if f.startswith('missed'))))
    dents = re.findall(r'PLANET bullet: [\d.]+ m/s2 at 1 m for [\d.]+ s: (\d+) nodes dented', ev)
    check(len(dents) >= 8 and sum(1 for x in dents if int(x) > 0) >= 8, 'bullets dent BeamNG\'s car (%d pushes, %d dented nodes)' % (len(dents), sum(int(x) for x in dents)))
    dr = re.findall(r'a bullet dent ([\d.]+) cm deep \(wanted ([\d.]+)\) by (\d+) pushes', log)
    check(len(dr) >= 6 and all(float(a) >= float(b) for a, b, n in dr), 'v2.61: each dent pushed in until it was as deep as wanted (%d: %s)' % (len(dr), dr[:3]))
    now = [int(x) for x in re.findall(r'dents: now (\d+) of 48 nodes moved', log)]
    check(now and max(now) >= 2, 'GTA draws the dents (%s nodes moved)' % (max(now) if now else 0))
    check(re.search(r'TIRE deflated 1\b', ev) is not None and 'tyre popped: FL' in log, 'the front left tyre popped (BeamNG, told to GTA)')
    # (v2.7: the blow-up after the burn makes a wreck: every tyre bursts at once; before it only the shot one)
    defl = {}
    for t_, w_ in re.findall(r'^([\d.]+) TIRE deflated (\d+)', ev, re.M): defl.setdefault(t_, set()).add(w_)
    check(all(ws == {'1'} or ws == {'0', '1', '2', '3'} for ws in defl.values()), 'only that tyre (until the wreck: %s)' % sorted(defl.items()))
    check('wreck (v2.7)' in log and '4 tyres burst' in log, 'v2.7: the blow-up left a wreck (every tyre burst, dents)')
    check('GLASS broken sidewindow_L_break' in ev and 'GLASS broken headlight_L_break' in ev, 'the side window and the headlight broke in BeamNG')
    br = re.findall(r'weapons: BeamNG broke (\S+) \((\d+) of the drawn car\'s groups', log)
    check(any('sidewindow_L_break' in g and 'headlight_L_break' in g and n == '2' for g, n in br), 'GTA hides / frosts them on the drawn car (%s)' % br[-2:])
    check(re.search(r'ENGINE damage radiator', ev) is not None and re.search(r'ENGINE damage main', ev) is not None, 'shots into the front: radiator and engine damage')
    blasts = re.findall(r'weapons: an explosion \(([^,]*), type (-?\d+)\) ([\d.]+) m from the car', log)
    check(len(blasts) == 1 and blasts[0][1] == '0' and 2.0 < float(blasts[0][2]) < 4.5, 'the grenade: one explosion, type 0, about 3 m away (%s)' % blasts)
    pb = re.findall(r'PLANET blast ([\d.]+) m from the car: (\d+) m/s2 at 1 m for [\d.]+ s: pushed ([\d.]+) m/s', ev)
    check(len(pb) >= 1 and pb[0][1] == '700' and float(pb[0][2]) > 1.0, 'BeamNG\'s car pushed by it (%s)' % pb[:2])
    check(re.search(r'FIRE ignite node \d+', ev) is not None and 'BeamNG\'s car is on fire' in log, 'it set the car on fire (BeamNG, told to GTA)')
    fx = re.findall(r'FAKE: particle fx (\S+) on entity (\d+) at ([-\d.]+) ([-\d.]+) ([-\d.]+) scale ([\d.]+): (\w+)', host)
    fx = [(a, b, c, e, f, h) for a, b, c, e, f, g, h in fx if abs(float(g) - 0.35) < 0.01]   # v2.62: the flames at 0.35 of the effect's size
    st = [f for f in fx if f[5] == 'started']
    check(fx and fx[0][0] == 'fire_wrecked_plane_cockpit' and fx[0][5] == 'unknown' and st and st[0][0] == 'ent_ray_heli_aprtmnt_l_fire' and st[0][1] == '100',
          'v2.61: GTA\'s flames as a particle effect on the hidden car (the first name unknown here, the second taken) (%s)' % fx[:3])
    check(st and abs(float(st[0][2])) < 1.3 and abs(float(st[0][3])) < 2.8 and -1.0 < float(st[0][4]) < 1.6, 'v2.61: placed on the car where BeamNG\'s burns (offset %s)' % ((st[0][2:5] if st else None),))
    check("GTA's flames on the car: particle effect core/ent_ray_heli_aprtmnt_l_fire" in log and 'FAKE: script fire' not in host, 'v2.61: logged; no fire on the spot')
    holes = [int(x) for x in re.findall(r'(\d+) bullet holes on the drawn car', log)]
    check(holes and max(holes) >= 8, 'v2.62: bullet holes on the drawn car (%s)' % holes)
    boom = re.search(r'weapons: BeamNG\'s car blew up \(it burned (\d+) s\)', log)
    check(boom is not None and 5 <= int(boom.group(1)) <= 8, 'it blew up after about 6 s of burning (%s)' % (boom.group(0) if boom else None))
    check('FAKE: vehicle 100 exploded' in host and re.search(r'FAKE: particle fx 6\d\d\d stopped', host) is not None, 'GTA\'s car exploded with it, the flames stopped')
    check(len(pb) == 2 and pb[-1][1] == '500', 'its own blast pushed BeamNG\'s car; GTA\'s explosion of its car not sent back as a new one (%d blasts in BeamNG)' % len(pb))
    hurt = re.findall(r'weapons: you lost (\d+) health \(([^)]*)\)', log)
    kinds = set(h[1] for h in hurt)
    check('a bullet through the cabin' in kinds, 'a bullet through the cabin hurt you (%s)' % [h for h in hurt if 'cabin' in h[1]][:3])
    check('an explosion near the car' in kinds, 'the grenade hurt you')
    check('your car blew up with you in it' in kinds, 'the car blowing up with you in it hurt you')
    hp = [int(x) for x in re.findall(r'FAKE: player health (-?\d+)', host)]
    check(hp and hp[0] < 200 and hp[-1] == 0, 'your health went down, to 0 when the car blew up (%s)' % hp)
    st = re.findall(r'weapons: 10 s: (\d+) bullets seen landing near \((\d+) through the car\)', log)
    check(st and sum(int(a) for a, b in st) >= 12, 'the 10 s stats line (%s)' % st)

if mode == 'shatter':
    # v2.63: (the foot run with FAKE_SHATTER=1) 5 shots into the left headlight: the first cracks it, 3 more shatter it out
    check('GLASS broken headlight_L_break' in ev, 'the headlight broke in BeamNG')
    sh = re.search(r'the cracked glass / lamp group (\d+) shattered out after (\d+) more hits', log)
    check(sh is not None and sh.group(2) == '3', 'v2.63: GTA shatters the cracked lamp out after 3 more hits (%s)' % (sh.group(0) if sh else None))

if mode == 'rpg':
    # v2.63: (the foot run with FAKE_RPG=1) a rocket on the car: it blows up at once, not after burning 30 s
    check(re.search(r'weapons: an explosion \(rocket, type 4\) [\d.]+ m from the car\'s middle .*\(close enough: the car blows up\)', log) is not None,
          'v2.63: the rocket found (not taken for the "car" explosion next to it), close enough to blow the car up')
    boom = re.search(r'weapons: BeamNG\'s car blew up \(([^)]*)\)', log)
    check(boom is not None and 'right at it' in boom.group(1), 'v2.63: BeamNG\'s car blew up at once (%s)' % (boom.group(1) if boom else None))
    check('FAKE: vehicle 100 exploded' in host, 'GTA\'s car exploded with it')

if mode == 'foot':
    # v2.61: out of the car with drive mode on: the car stays BeamNG's (parked), solid in GTA, you visible, your bullets hit it
    check('FAKE: entity 100 is a mission entity' in host, 'v2.61: GTA won\'t delete the hidden car (a mission car)')
    check('drive: you got out: BeamNG keeps the car' in log and "Drive OFF (you left the driver's seat)" not in log, 'v2.61: you got out: drive mode stays on')
    out = host.split('FAKE: you get out of the car', 1)[1] if 'FAKE: you get out of the car' in host else ''
    back = out.split('FAKE: you get back in', 1)
    o1, o2 = back[0], back[1] if len(back) > 1 else ''
    check('FAKE: collision 1' in o1 and 'FAKE: frozen 1' in o1, 'v2.61: GTA\'s hidden car solid and standing where BeamNG\'s is while you\'re out')
    check('FAKE: player visible 1' in o1 and 'FAKE: player alpha reset' in o1, 'v2.61: you are shown (visible, alpha reset)')
    check('proofs entity 1: bullet 0 fire 0 explosion 0' in o1, 'v2.61: GTA\'s own damage on you again on foot')
    check('DISABLED (0):' in o1, 'v2.61: none of GTA\'s controls switched off while you walk')
    check('drive: you got out in GTA: the car parks' in mlog and 'drive: you are back in the car in GTA' in mlog, 'v2.61: the BeamNG mod parks the car while you\'re out')
    check(re.search(r'event parkingbrake 1\.000', ev) is not None, 'v2.61: BeamNG\'s parking brake on')
    hits = re.findall(r'weapons: a bullet \(GROUP_PISTOL\) hit the drawn car', log)
    check(len(hits) >= 3 and '(guessed)' not in log, 'v2.61: your own bullets on foot hit it (%d)' % len(hits))
    check('a bullet through the cabin' not in log, 'v2.61: nobody in the cabin to hurt')
    check(re.search(r'a bullet dent [\d.]+ cm deep', log) is not None, 'v2.61: they dent it')
    check(re.search(r'[1-9]\d* bullet holes on the drawn car', log) is not None, 'v2.62: and leave holes')
    check('drive: back in: BeamNG drives again' in log and 'FAKE: collision 0' in o2, 'v2.61: back in: drive on, the hidden car without collision again')
    check('drive: stopped (drive key)' in log, 'F10 ended it at the end')

print('ALL PASS' if not fails else 'FAILED: %d' % len(fails))
