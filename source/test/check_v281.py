#!/usr/bin/env python3
"""check_v281.py - v2.81 PuppetNoMap end-to-end checks (run in test/run after run_e2e.sh). FAKE GTA only (fakeshv.cpp): they test
the plugin's code paths, not GTA's physics. Only Treto's in-game test says whether GTA really lets the hidden car through its map.
  nomap : PuppetNoMap=1, FAKE_PSMAP=1 FAKE_STOP_AT=14 FAKE_DENT_AFTER=14: map collision off every tick (the fake's kerbs never hit
          it), gravity off while solid and on after, BeamNG's stop logged as a contact resolved by BeamNG alone, then BENT, the
          F10-off check says GTA's map collision is back.
  map77 : PuppetNoMap=0, the same: v2.77's way: the fake's kerbs hit the hidden car (GTA's map lines), the native never called.
  gta   : PuppetNoMap=1, FAKE_TRAFFIC=1 FAKE_PSKICK=1: the parked car's contact logged as resolved by GTA, with the push sent.
  slowmo 1|0 : script_slowmo.txt: PuppetSlowMoFix=1 scales the hidden car's speed in GTA's slow motion; 0 doesn't (v2.77).
  hurtkey: script_hurt.txt: Ctrl+F9 twice: crash damage on, then off (CrashHurt=0 at the start)."""
import os, re, sys

mode = sys.argv[1] if len(sys.argv) > 1 else 'nomap'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)
def rd(p): return open(p, errors='replace').read() if os.path.exists(p) else ''
log, host, ev = rd('GtaBeamD1.log'), rd('host_out.txt'), rd('fake_beam_events.txt')
maplines = log.count("GTA's map (kerb, wall, ground): NOT sent")
kerbs = host.count("GTA's map (a kerb at")
driving = host[:host.rfind('FAKE: gravity 1')] if 'FAKE: gravity 1' in host else host   # (until F10 off: then it's a normal GTA car)
kerbsDriving = driving.count("GTA's map (a kerb at")

if mode == 'nomap':
    check('settings: v2.81: PuppetNoMap 1' in log, 'setting read')
    check('PuppetNoMap (v2.81): ON' in log, 'engaged line')
    check('map collision off this frame' in host, 'the native reached the fake GTA')
    check(kerbsDriving == 0 and maplines == 0, "the fake's kerbs never hit the hidden car while driving (%d kerbs, %d map lines)" % (kerbsDriving, maplines))
    check(re.search(r'PuppetSolid \(v2\.81\): 10 s: PuppetNoMap ON: GTA\'s map off [1-9]\d* ticks', log) is not None, '10 s line counts map-off ticks')
    g = re.findall(r'FAKE: gravity (\d)', host)
    check('0' in g and g[-1] == '1', 'gravity off while solid, on at the end (%s)' % ''.join(g))
    check('STOP the car hits a wall' in ev, 'the fake BeamNG car stopped')
    check(re.search(r'contact \(v2\.81\): .* resolved by BeamNG alone .*the hidden car [\d.]+ m from', log) is not None, "BeamNG's contact logged, resolved by BeamNG alone")
    check(re.search(r'contact \(v2\.81\): 1\.5 s after .*BENT', log) is not None, 'then BENT (the fake dent)')
    check("GTA's map collision is back on it" in log, 'F10 off: map collision back')
    check('FELL' not in log, 'no fall at F10 off')
elif mode == 'map77':
    check('settings: v2.81: PuppetNoMap 0' in log, 'setting read as 0')
    check('PuppetNoMap (v2.81): ON' not in log and 'map collision off this frame' not in host, 'never engaged, the native never called')
    check(kerbs > 0 and maplines > 0, "v2.77: the fake's kerbs hit the hidden car (%d kerbs, %d map lines)" % (kerbs, maplines))
    check('F10 off: 1.5 s after landing' not in log, 'no F10-off check')
    check(re.search(r'contact \(v2\.81\): .*resolved by BeamNG, and GTA\'s map on the hidden car too', log) is not None, "the contact says both (v2.77)")
elif mode == 'slowmo':
    # PuppetSolid=1, script_slowmo.txt (GTA at 0.2 x speed): PuppetSlowMoFix=1 scales the speed (argv[2] = 1), 0 doesn't (v2.77)
    want = (sys.argv[2] if len(sys.argv) > 2 else '1') == '1'
    n = sum(int(x) for x in re.findall(r'(\d+) slow-motion ticks scaled', log))
    check((n > 0) == want, 'slow-motion ticks scaled: %d (want %s)' % (n, 'some' if want else 'none'))
elif mode == 'hurtkey':
    # script_hurt.txt: Ctrl+F9 twice: crash damage on, then off (CrashHurt=0 at the start)
    check("CrashHurt 0 (crashes don't hurt you" in log, 'crash damage off at the start')
    k = re.findall(r'crash damage to you: (ON|OFF) \(Ctrl\+F9\)', log)
    check(k == ['ON', 'OFF'], 'Ctrl+F9 switches it on, then off (%s)' % k)
elif mode == 'gta':
    check('PuppetNoMap (v2.81): ON' in log, 'engaged line')
    check(re.search(r'contact \(v2\.81\): a parked GTA car \(9001.*resolved by GTA.*got a push of \d+ km/h', log) is not None, 'the parked car: resolved by GTA, push sent')
    check(re.search(r"PuppetSolid: GTA's crash: a GTA car \(9001.*SENT to BeamNG", log) is not None, "v2.77's crash line still there")
print('ALL PASS' if not fails else '%d FAIL' % len(fails))
sys.exit(1 if fails else 0)
