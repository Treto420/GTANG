#!/usr/bin/env python3
# lua_input_test.py - v2.3 BeamNG mod test against API stubs (LuaJIT via lupa, like BeamNG's GE Lua).
# Checks: drive_start -> drive_ok with your bound pad/key controls; raw pad/keys from GTA run YOUR
# bindings' action code in the car's Lua (VALUE/FILTERTYPE filled in, negative values safe); modifier
# keys (Torque rule); BeamNG's own controls blocked while driving/parked and given back; failsafe release;
# the car's state (electrics) going back to GTA as "vs"; fallback when no bindings; PlayStation (DirectInput)
# bindings mapped from the XInput state; poses still sent.
# v2.2: BeamNG's real sound setting (AudioMuteOnWindowLoseFocus) switched and put back to what it WAS;
# background fps limit (bgf); restore after a crash; debug categories (vehicle_debug) and reload_vehicle never forwarded.
import json, sys, time
from lupa import luajit21 as lj

SRC = sys.argv[1] if len(sys.argv) > 1 else '../src/gtaBridge.lua'
fails = []
def check(cond, what):
    print(('PASS ' if cond else 'FAIL ') + what)
    if not cond: fails.append(what)

class Clock:
    t = 1000.0
clock = Clock()

def make_world(bindings_mode='normal', files=None, maps_off=(), sound_muted=True, pre=''):
    ge = lj.LuaRuntime(unpack_returned_tuples=True)
    veh_vm = lj.LuaRuntime(unpack_returned_tuples=True)
    logs, sent, inbox, ge_queue, veh_calls = [], [], [], [], []
    files = {} if files is None else files

    # ---- vehicle VM (the car's own Lua)
    veh_vm.execute('''
      electrics = { values = { throttle_input = 0, brake_input = 0, steering_input = 0, parkingbrake_input = 0, clutch_input = 0,
                               throttle = 0, brake = 0, gear = 'N', rpm = 800, wheelspeed = 0, ignitionLevel = 2, engineRunning = 1,
                               gearboxMode = 'realistic', parkingbrake = 0 } }
      calls = {}
      input = { event = function(name, v, flt, angle, lock, x, src)
          calls[#calls + 1] = string.format('event %s %.3f %s', name, v, tostring(flt))
          electrics.values[name .. '_input'] = v
        end }
      controller = { mainController = { shiftUp = function() calls[#calls + 1] = 'shiftUp' end,
                                        shiftDown = function() calls[#calls + 1] = 'shiftDown' end } }
      boom = function() error('broken action') end
    ''')
    vg = veh_vm.globals()
    vg.obj = veh_vm.table_from({})
    veh_vm.execute('obj.getID = function(self) return 42 end')
    vg.obj.queueGameEngineLua = lambda self, code: ge_queue.append(code)

    g = ge.globals()
    g.log = lambda lvl, tag, msg: logs.append(f'{lvl} {msg}')
    ge.execute('''
      guihooks = { message = function(m) end }
      vec3 = function(x, y, z) if type(x) == 'table' then return x end return { x = x, y = y, z = z } end
      jsonReadFile = function(p) return nil end
    ''')
    g.readFile = lambda p: files.get(p)
    g.writeFile = lambda p, d: files.__setitem__(p, d) or True
    # json via python
    def to_lua(o):
        if isinstance(o, dict): return ge.table_from({k: to_lua(v) for k, v in o.items()})
        if isinstance(o, list): return ge.table_from([to_lua(v) for v in o])
        return o
    def from_lua(t):
        if lj.lua_type(t) == 'table':
            keys = list(t.keys())
            if keys and all(isinstance(k, int) for k in keys) and sorted(keys) == list(range(1, len(keys) + 1)):
                return [from_lua(t[k]) for k in sorted(keys)]
            return {str(k): from_lua(t[k]) for k in keys}
        return t
    g.jsonDecode = lambda s: to_lua(json.loads(s))
    g.jsonEncode = lambda t: json.dumps(from_lua(t))
    ge.execute('FS = {}')
    ge.execute('function FS:findFiles() return {} end function FS:fileExists() return false end function FS:getFileRealPath(p) return "C:/user" .. p end')
    # socket
    class Udp:
        def setsockname(self, ip, port): return 1
        def settimeout(self, t): return None
        def receivefrom(self, n):
            if inbox: return inbox.pop(0), '127.0.0.1', 55555
            return None, 'timeout', None
        def sendto(self, s, ip, port): sent.append(s); return len(s)
        def close(self): pass
    udp = Udp()
    g.socket = ge.table_from({})
    g.socket.udp = lambda: udp
    g.socket.gettime = lambda: clock.t
    # the car
    wheels = {}
    nodes = {}
    pos = {'x': 10.0, 'y': 20.0, 'z': 0.5}
    hubs = {'FL': (-0.75, 1.4), 'FR': (0.75, 1.4), 'RL': (-0.8, -1.3), 'RR': (0.8, -1.3)}
    cid = 0
    for name, (x, y) in hubs.items():
        a, b, s = cid, cid + 1, cid + 2
        cid += 3
        nodes[a] = (x - 0.1, y, 0.33); nodes[b] = (x + 0.1, y, 0.33); nodes[s] = (x, y + 0.2, 0.33)
        wheels[name] = {'name': name, 'node1': a, 'node2': b, 'radius': 0.33, 'treadNodes': [s]}
    g.core_vehicle_manager = ge.table_from({})
    vdata = to_lua({'vdata': {'wheels': list(wheels.values()), 'nodes': {}}})
    g.core_vehicle_manager.getPlayerVehicleData = lambda: vdata
    veh = ge.table_from({})
    g.VEH = veh
    ge.execute('''
      function VEH:getID() return 42 end
      function VEH:getJBeamFilename() return 'sdd_s650' end
      function VEH:getDirectionVector() return { x = 0, y = 1, z = 0 } end
      function VEH:getDirectionVectorUp() return { x = 0, y = 0, z = 1 } end
      function VEH:getVelocity() return { x = 0, y = 0, z = 0 } end
    ''')
    veh.getPosition = lambda self: ge.table_from(pos)
    veh.getNodePosition = lambda self, c: ge.table_from({'x': nodes[c][0], 'y': nodes[c][1], 'z': nodes[c][2]})
    veh.queueLuaCommand = lambda self, code: (veh_calls.append(code), veh_vm.execute(code))
    ge.execute('getPlayerVehicle = function() return VEH end')
    # BeamNG's input system (stubs with the shapes the mod expects)
    maps = {n: to_lua({'enabled': n not in maps_off}) for n in ['GlobalActionMap', 'MenuActionMap', 'VehicleCommonActionMap', 'VehicleSpecificActionMap', 'NormalActionMap']}
    g.scenetree = ge.table_from({})
    g.scenetree.findObject = lambda name: maps.get(name)
    ge.execute('ActionMap = {} function ActionMap:getList() return { "GlobalActionMap", "NormalActionMap" } end')
    actions = {
        'accelerate': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('throttle', VALUE, FILTERTYPE)"},
        'brake': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('brake', VALUE, FILTERTYPE)"},
        'steering': {'cat': 'vehicle', 'ctx': 'vlua', 'isCentered': True, 'onChange': "input.event('steering', VALUE, FILTERTYPE, ANGLE, LOCKTYPE)"},
        'steer_left': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('steering', -VALUE, FILTERTYPE)"},
        'steer_right': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('steering', VALUE, FILTERTYPE)"},
        'clutch': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('clutch', VALUE, FILTERTYPE)"},
        'parkingbrake': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "input.event('parkingbrake', VALUE, FILTERTYPE)"},
        'shiftUp': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'controller.mainController.shiftUp()'},
        'shiftDown': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'controller.mainController.shiftDown()'},
        'toggleESCMode': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'esc'"},
        'toggleNitrousOxide': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'nitrous'"},
        'toggle_left_signal': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'leftsignal'"},
        'toggle_headlights': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'headlights'"},
        'toggle_hazard_signal': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'hazard'"},
        'activateStarterMotor': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'starter'"},
        'horn': {'cat': 'vehicle', 'ctx': 'vlua', 'onChange': "calls[#calls + 1] = 'horn'"},
        'customModifier1': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
        'customModifier2': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
        'customModifier3': {'cat': 'modifier', 'ctx': 'tlua', 'onDown': ''},
        'brokenThing': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'boom()'},
        'badSyntax': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': 'this is ( not lua'},
        'plainQ': {'cat': 'vehicle', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'plainq'"},
        'reset_physics': {'cat': 'gameplay', 'ctx': 'tlua', 'onDown': 'RESET_CALLED = true'},
        'recover_vehicle': {'cat': 'vehicle', 'ctx': 'tlua', 'onDown': 'RECOVER_CALLED = true'},
        'camera_next': {'cat': 'camera', 'ctx': 'tlua', 'onDown': 'CAM_CALLED = true'},
        # v2.2: his bindings.txt: vehicle_debug actions were forwarded by the substring match
        'reload_vehicle': {'cat': 'vehicle_debug', 'ctx': 'tlua', 'onDown': 'RELOAD_CALLED = true'},
        'reload_all_vehicles': {'cat': 'vehicle_debug', 'ctx': 'tlua', 'onDown': 'RELOAD_ALL_CALLED = true'},
        'debugmode_next': {'cat': 'vehicle_debug', 'ctx': 'tlua', 'onDown': 'DEBUG_CALLED = true'},
        'vsThing': {'cat': 'vehicle_specific', 'ctx': 'vlua', 'onDown': "calls[#calls + 1] = 'vehspecific'"},
    }
    kb = [
        {'control': 'up', 'action': 'accelerate'}, {'control': 'down', 'action': 'brake'},
        {'control': 'left', 'action': 'steer_left'}, {'control': 'right', 'action': 'steer_right'},
        {'control': 'lshift', 'action': 'clutch'}, {'control': 'x', 'action': 'shiftUp'}, {'control': 'z', 'action': 'shiftDown'},
        {'control': 'ctrl q', 'action': 'toggleESCMode'}, {'control': 'q', 'action': 'plainQ'}, {'control': 'r', 'action': 'reset_physics'}, {'control': 'c', 'action': 'camera_next'},
        {'control': 'ctrl r', 'action': 'reload_vehicle'}, {'control': 'ctrl shift r', 'action': 'reload_all_vehicles'}, {'control': 'ctrl b', 'action': 'debugmode_next'},
        {'control': 'v', 'action': 'vsThing'},
    ]
    pad = [
        {'control': 'triggerr', 'action': 'accelerate'}, {'control': 'triggerl', 'action': 'brake'},
        {'control': 'thumblx', 'action': 'steering', 'deadzoneResting': 0.1}, {'control': 'btn_l', 'action': 'clutch'},
        {'control': 'btn_a', 'action': 'shiftUp'}, {'control': 'btn_x', 'action': 'shiftDown'}, {'control': 'btn_b', 'action': 'parkingbrake'},
        {'control': 'btn_y', 'action': 'brokenThing'}, {'control': 'dpadl', 'action': 'camera_next'}, {'control': 'btn_rt', 'action': 'badSyntax'},
    ]
    ds = [{'control': 'button1', 'action': 'shiftUp'}, {'control': 'ryaxis', 'action': 'accelerate'}, {'control': 'xaxis', 'action': 'steering'}]
    devs = []
    if bindings_mode in ('normal', 'ds'):
        devs.append({'devname': 'keyboard0', 'contents': {'name': 'Keyboard', 'bindings': kb}})
    if bindings_mode == 'normal':
        devs.append({'devname': 'xinput0', 'contents': {'name': 'Xbox Controller', 'vidpid': 'xinput', 'bindings': pad}})
        devs.append({'devname': 'xinput1', 'contents': {'name': 'Xbox Controller', 'vidpid': 'xinput', 'bindings': pad}})  # a second list for the same pad
    if bindings_mode == 'ds':
        devs.append({'devname': 'joystick0', 'contents': {'name': 'Wireless Controller', 'vidpid': '0ce6054c', 'bindings': ds}})
    if bindings_mode == 'user':
        # v2.1: Treto's setup (his bindings.txt): DS4Windows' Xbox pad with BeamNG's defaults + his own DualSense list
        # (BeamNG's own PlayStation support, "sce_pad_standard"): Circle up, Square down, Cross handbrake, L1 clutch
        xi_def = [{'control': 'btn_a', 'action': 'shiftUp'}, {'control': 'btn_x', 'action': 'shiftDown'}, {'control': 'btn_b', 'action': 'parkingbrake'},
                  {'control': 'btn_l', 'action': 'clutch'}, {'control': 'triggerr', 'action': 'accelerate'}, {'control': 'triggerl', 'action': 'brake'},
                  {'control': 'thumblx', 'action': 'steering', 'deadzoneResting': 0.1}, {'control': 'btn_r', 'action': 'customModifier1'},
                  {'control': 'btn_l', 'action': 'customModifier2'}, {'control': 'modifier1 dpov', 'action': 'activateStarterMotor'}]
        dse = [{'control': 'button2', 'action': 'shiftUp'}, {'control': 'button0', 'action': 'shiftDown'}, {'control': 'button1', 'action': 'parkingbrake'},
               {'control': 'button4', 'action': 'clutch'}, {'control': 'button4', 'action': 'customModifier2'}, {'control': 'button5', 'action': 'customModifier1'},
               {'control': 'button13', 'action': 'customModifier3'}, {'control': 'ryaxis', 'action': 'accelerate'}, {'control': 'rxaxis', 'action': 'brake'},
               {'control': 'xaxis', 'action': 'steering'}, {'control': 'modifier1 dpov', 'action': 'toggleNitrousOxide'},
               {'control': 'modifier2 upov', 'action': 'toggleESCMode'}, {'control': 'modifier1-modifier2 button10', 'action': 'toggle_left_signal'},
               {'control': 'button10', 'action': 'toggle_headlights'}, {'control': 'modifier3 button1', 'action': 'toggle_hazard_signal'},
               {'control': 'upov', 'action': 'camera_next'}, {'control': '', 'action': 'horn'},
               {'control': 'rpov', 'action': 'reload_vehicle'}, {'control': 'lpov', 'action': 'debugmode_next'},   # v2.2: his D-pad right / left
               {'control': 'modifier1-modifier2 button0', 'action': 'recover_vehicle'}]   # v2.43: his repair: RB + LB + Square
        devs.append({'devname': 'xinput0', 'contents': {'name': 'XBox Controller 1', 'vidpid': 'XIDevice', 'bindings': xi_def}})
        devs.append({'devname': 'gamepad0', 'contents': {'name': 'DualSense wireless controller 0', 'vidpid': 'sce_pad_standard', 'bindings': dse}})
    if bindings_mode == 'ds2':
        # v2.01.1 (user's log): a DirectInput DualSense under a "gamepad" name, with BeamNG pad-modifier combinations
        ds2 = ds + [{'control': 'modifier1 upov', 'action': 'shiftDown'}, {'control': 'modifier2-modifier3 rpov', 'action': 'toggleESCMode'},
                    {'control': 'button11', 'action': 'clutch'}, {'control': 'upov', 'action': 'parkingbrake'}]
        devs.append({'devname': 'gamepad0', 'contents': {'name': 'DualSense Wireless Controller', 'vidpid': '0ce6054c', 'bindings': ds2}})
    g.core_input_bindings = to_lua({'bindings': devs})
    g.ACTS = to_lua(actions)
    ge.execute('core_input_actions = { getActiveActions = function() return ACTS end }')
    g.extensions = ge.table_from({})
    # v2.2: BeamNG 0.39's settings (names from his settings dump: settings.impl.defaultValues)
    ge.execute('''
      SETTINGS_VALUES = { AudioMuteOnWindowLoseFocus = %s, fpsLimitBackgroundEnabled = true, fpsLimitBackground = 30, unfocusedInput = false, AudioMasterVol = 0.8 }
      SET_CALLS = 0
      settings = { setValue = function(k, v) SET_CALLS = SET_CALLS + 1 SETTINGS_VALUES[k] = v end, getValue = function(k) return SETTINGS_VALUES[k] end }
    ''' % ('true' if sound_muted else 'false'))
    if pre: ge.execute(pre)
    M = ge.execute(open(SRC).read())
    g.gtaBridge = M
    M.onExtensionLoaded()
    w = dict(ge=ge, veh_vm=veh_vm, M=M, logs=logs, sent=sent, inbox=inbox, files=files, ge_queue=ge_queue, veh_calls=veh_calls, maps=maps, g=g, vg=vg)
    return w

def frame(w, n=1):
    for _ in range(n):
        clock.t += 1 / 60
        w['M'].onUpdate(1 / 60, 1 / 60, 1 / 60)
        while w['ge_queue']:
            w['ge'].execute(w['ge_queue'].pop(0))

def calls(w):
    c = w['vg'].calls
    out = [c[i] for i in range(1, len(c) + 1)]
    w['veh_vm'].execute('calls = {}')
    return out

def drive_msg(pad='', keys='', thr=0, brk=0, st=0, hb=0, flt=1, m=0):
    return '{"t":"drive","q":1,"thr":%.3f,"brk":%.3f,"str":%.3f,"hb":%d,"flt":%d,"pad":"%s","keys":"%s","m":%d}' % (thr, brk, st, hb, flt, pad, keys, m)

def pad(lx=0, ly=0, rx=0, ry=0, lt=0, rt=0, btn=0):
    return '%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d' % (lx, ly, rx, ry, lt, rt, btn)

def last_sent(w, t):
    for s in reversed(w['sent']):
        if s.startswith('{"t":"%s"' % t) or ('"t": "%s"' % t) in s or ('"t":"%s"' % t) in s: return s
    return None

def sv(w, k): return w['ge'].eval('SETTINGS_VALUES.' + k)

# ======================================================================== normal: keyboard + XInput pad
w = make_world('normal')
w['inbox'].append('{"t":"drive_start","v":"2.01","cats":"vehicle","deny":"reset_physics,recover_vehicle","also":""}')
frame(w)
ok = last_sent(w, 'drive_ok')
check(ok is not None, 'drive_ok sent')
okj = json.loads(ok) if ok else {}
check(okj.get('inp') == 'binds', 'input mode = your bindings (got %s)' % okj.get('inp'))
check(set(okj.get('padc', '').split(',')) == {'triggerr', 'triggerl', 'thumblx', 'btn_l', 'btn_a', 'btn_x', 'btn_b', 'btn_y', 'btn_rt'}, 'bound pad controls listed (camera on dpadl not): %s' % okj.get('padc'))
check('r' not in okj.get('keyc', '').split(',') and 'up' in okj.get('keyc', '').split(','), 'keys: reset (r) not forwarded, up is: %s' % okj.get('keyc'))
check('b' not in okj.get('keyc', '').split(','), 'v2.2: ctrl+b (debugmode_next, vehicle_debug) not forwarded: %s' % okj.get('keyc'))
check('v' in okj.get('keyc', '').split(','), 'v2.2: a vehicle_specific action is still forwarded (category starts with "vehicle_")')
check(w['maps']['VehicleCommonActionMap'].enabled is False and w['maps']['NormalActionMap'].enabled is False and w['maps']['GlobalActionMap'].enabled is True,
      "BeamNG's own vehicle controls blocked while GTA drives (global map untouched)")
rep = w['files'].get('/gtabridge/bindings.txt', '')
check('FORWARDED' in rep and 'skipped' in rep, 'bindings.txt written with forwarded/skipped lines')
check('reload_vehicle' in rep and 'never forwarded (reloads the BeamNG car)' in rep, 'v2.2: reload_vehicle listed as never forwarded even when GTA\'s deny list leaves it out')
start_calls = calls(w)
check(any('parkingbrake 0.000' in c for c in start_calls), 'drive start releases the parking brake: %s' % start_calls[:6])

# pad: right trigger 0.7, stick left -0.5 (deadzone 0.1 -> -0.444)
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7)))
frame(w)
c = calls(w)
check('event throttle 0.700 1' in c, 'RT 0.7 -> throttle 0.7 with the pad filter (%s)' % c)
check(any(x.startswith('event steering -0.444') for x in c), 'stick -0.5 with deadzone 0.1 -> steering -0.444 (negative value safe)')
# same state again: nothing re-sent
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7)))
frame(w)
check(calls(w) == [], 'unchanged controls send nothing')
# A pressed -> shiftUp once; held -> nothing; LB clutch analog 1
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100)))
frame(w)
c = calls(w)
check(c.count('shiftUp') == 1 and 'event clutch 1.000 1' in c, 'A -> shiftUp once, LB -> clutch 1 (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100)))
frame(w)
check(calls(w) == [], 'held A does not shift again')
# an action that doesn't even compile (RS click) pressed with X: X still shifts down, the error is reported
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100 | 0x0080 | 0x4000)))
frame(w, 2)
c = calls(w)
check('shiftDown' in c and any('badSyntax failed' in l for l in w['logs']), 'an action that does not compile: reported, the others still run (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100)))
frame(w)
calls(w)
# a tap shorter than a BeamNG frame: B pressed and let go in two GTA messages before the frame
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100 | 0x2000)))
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100)))
frame(w)
c = calls(w)
check('event parkingbrake 1.000 1' in c and 'event parkingbrake 0.000 1' in c, 'a tap between two BeamNG frames still presses and lets go (%s)' % c)
# Y -> broken action: error comes back to the log
w['inbox'].append(drive_msg(pad=pad(lx=-0.5, rt=0.7, btn=0x1000 | 0x0100 | 0x8000)))
frame(w, 2)
check(any('brokenThing failed' in l for l in w['logs']), 'an action error in the car is reported to the log')
# keyboard: ctrl+q -> ESC mode; plain x with lshift held (clutch) still shifts (no "shift x" binding)
w['inbox'].append(drive_msg(pad=pad(), keys='lcontrol q'))
frame(w)
c = calls(w)
check('esc' in c and 'plainq' not in c, 'ctrl+q -> toggleESCMode, not plain q (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(), keys='q'))
frame(w)
c = calls(w)
check('plainq' not in c, 'ctrl let go first, q still held: plain q not pressed (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(), keys=''))
frame(w)
w['inbox'].append(drive_msg(pad=pad(), keys='q'))
frame(w)
c = calls(w)
check('plainq' in c, 'q pressed on its own afterwards: plain q (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(), keys='lshift x'))
frame(w)
c = calls(w)
check('shiftUp' in c and 'event clutch 1.000 0' in c, 'lshift (clutch, key filter) + x -> shiftUp (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(), keys='r c'))
frame(w)
check(w['ge'].eval('RESET_CALLED') is None and w['ge'].eval('CAM_CALLED') is None, 'reset and camera keys are not run')
w['inbox'].append(drive_msg(pad=pad(), keys='lcontrol r'))
frame(w)
w['inbox'].append(drive_msg(pad=pad(), keys='lcontrol lshift r'))
frame(w)
w['inbox'].append(drive_msg(pad=pad(), keys='lcontrol b'))
frame(w)
w['inbox'].append(drive_msg(pad=pad(), keys='v'))
frame(w)
check(w['ge'].eval('RELOAD_CALLED') is None and w['ge'].eval('RELOAD_ALL_CALLED') is None and w['ge'].eval('DEBUG_CALLED') is None,
      'v2.2: ctrl+r / ctrl+shift+r / ctrl+b (reload, debug) never run')
check('vehspecific' in calls(w), 'v2.2: the vehicle_specific action runs')
# vehicle state back to GTA
frame(w, 20)
vs = last_sent(w, 'vs')
check(vs is not None and '"gear":"N"' in vs and '"gbm":"realistic"' in vs, 'car state goes back to GTA: %s' % (vs or '')[:160])
check(last_sent(w, 'pose') is not None, 'poses still sent')
# failsafe: GTA quiet for 0.6 s -> everything let go
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0100)))
frame(w)
calls(w)
frame(w, 40)
c = calls(w)
check('event throttle 0.000 1' in c and 'event clutch 0.000 1' in c and any('parkingbrake 1' in x for x in c), 'GTA quiet: throttle/clutch let go, parking brake on (%s)' % c[:8])
# BeamNG switches a map back on while driving: off again within a second
w['maps']['VehicleCommonActionMap'].enabled = True
w['inbox'].append(drive_msg(pad=pad()))
frame(w, 70)
check(w['maps']['VehicleCommonActionMap'].enabled is False, 'a map switched back on while driving is switched off again')
check(w['files'].get('/gtabridge/blocked_maps.txt', '').count('Map') == 3, 'blocked maps written down (for a Lua reload): %r' % w['files'].get('/gtabridge/blocked_maps.txt'))
# the Lua reloads while blocked: the new run switches them back on
w2 = make_world('normal', files=dict(w['files']), maps_off=('VehicleCommonActionMap', 'VehicleSpecificActionMap', 'NormalActionMap'))
check(w2['maps']['VehicleCommonActionMap'].enabled is True and w2['maps']['NormalActionMap'].enabled is True, 'after a Lua reload the maps an earlier run left off are back on')
check(w2['files'].get('/gtabridge/blocked_maps.txt') == '', 'and the note is cleared')
# stop -> maps back
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(w['maps']['VehicleCommonActionMap'].enabled is True and w['maps']['NormalActionMap'].enabled is True, "drive_stop gives BeamNG its own controls back")
w['inbox'].append('{"t":"drive_start","v":"2.01"}')
frame(w)
w['inbox'].append('{"t":"drive_stop","park":1}')
frame(w)
check(w['maps']['VehicleCommonActionMap'].enabled is False, 'drive_stop with park:1 (car stays parked in GTA): BeamNG controls stay off, no gap')
frame(w, 170)
check(w['maps']['VehicleCommonActionMap'].enabled is True, '... and come back 2.5 s after the last park word')
# park: blocked while GTA keeps saying so, back 2.5 s after it stops
w['inbox'].append('{"t":"park","blk":1}')
frame(w)
check(w['maps']['VehicleCommonActionMap'].enabled is False, 'GTA focused + parked: BeamNG controls blocked')
frame(w, 170)
check(w['maps']['VehicleCommonActionMap'].enabled is True, 'no park messages for 2.5 s: BeamNG controls back')
w['inbox'].append('{"t":"park","blk":1}')
frame(w)
w['M'].unblock()
w['inbox'].append('{"t":"park","blk":1}')
frame(w, 5)
check(w['maps']['VehicleCommonActionMap'].enabled is True, 'gtaBridge.unblock(): controls back and park messages no longer block')
w['inbox'].append('{"t":"drive_start","v":"2.01"}')
frame(w)
check(w['maps']['VehicleCommonActionMap'].enabled is False, 'the next drive start blocks again')

# ======================================================================== no bindings: v2.0 fallback
w = make_world('none')
w['inbox'].append('{"t":"drive_start","v":"2.01"}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(okj.get('inp') == 'fallback', 'no bindings -> fallback (got %s)' % okj.get('inp'))
calls(w)
w['inbox'].append(drive_msg(thr=0.8, st=-0.3, flt=1))
frame(w)
c = calls(w)
check('event throttle 0.800 1' in c and 'event steering -0.300 1' in c, 'fallback drives with input.event (%s)' % c)

# ======================================================================== PlayStation pad as DirectInput (no DS4Windows)
w = make_world('ds')
w['inbox'].append('{"t":"drive_start","v":"2.01"}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(set(okj.get('padc', '').split(',')) == {'btn_a', 'triggerr', 'thumblx'}, 'DualSense bindings mapped to XInput names: %s' % okj.get('padc'))
calls(w)
w['inbox'].append(drive_msg(pad=pad(lx=0.5, rt=1.0, btn=0x1000)))
frame(w)
c = calls(w)
check('shiftUp' in c and 'event throttle 1.000 1' in c and any(x.startswith('event steering 0.500') for x in c), 'cross -> shiftUp, R2 -> throttle, stick -> steering (%s)' % c)


# ======================================================================== v2.01.1: DualSense as DirectInput named "gamepad0", pad modifiers
w = make_world('ds2')
w['inbox'].append('{"t":"drive_start","v":"2.01.1"}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(set(okj.get('padc', '').split(',')) == {'btn_a', 'triggerr', 'thumblx', 'btn_rt', 'dpadu'}, 'a DirectInput DualSense named gamepad0 is read as one (got %s)' % okj.get('padc'))
rep2 = w['files'].get('/gtabridge/bindings.txt', '')
check('pad combinations not usable: 2' in rep2 and 'needs modifier1' in rep2 and 'not understood' not in rep2, 'combinations needing a modifier with no button: reported as not usable, nothing "not understood"')
calls(w)
w['inbox'].append(drive_msg(pad=pad(btn=0x0001 | 0x0080)))
frame(w)
c = calls(w)
check('event parkingbrake 1.000 1' in c and 'event clutch 1.000 1' in c, 'D-pad up and R3 work through the DualSense bindings (%s)' % c)

# ======================================================================== v2.1: the user's real setup (DS4W Xbox list + his DualSense list)
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.1"}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(okj.get('plist') == 'gamepad0', 'auto: the PlayStation list (his own bindings) is used, not DS4W\'s Xbox defaults (got %s)' % okj.get('plist'))
check(set(okj.get('padc', '').split(',')) == {'btn_b', 'btn_x', 'btn_a', 'btn_l', 'btn_r', 'triggerr', 'triggerl', 'thumblx', 'btn_lt'}, 'pad controls incl. modifier buttons RB/LB; v2.2: no D-pad right (reload_vehicle) or left (debug) (%s)' % okj.get('padc'))
check(set(okj.get('padm', '').split(',')) == {'dpadd', 'dpadu'} and set(okj.get('pmod', '').split(',')) == {'btn_r', 'btn_l'}, 'combination-only controls and modifier buttons reported (%s / %s)' % (okj.get('padm'), okj.get('pmod')))
rep3 = w['files'].get('/gtabridge/bindings.txt', '')
check('needs modifier3' in rep3 and 'modifier3 = none (button13: PS / touchpad button' in rep3, 'modifier3 (touchpad) combinations listed as not usable through XInput')
check('debugmode_next' in rep3 and 'is a debug one' in rep3, 'v2.2: vehicle_debug actions reported as skipped (debug category)')
calls(w)
w['inbox'].append(drive_msg(pad=pad(btn=0x0008)))
frame(w)
w['inbox'].append(drive_msg(pad=pad()))
frame(w)
check(w['ge'].eval('RELOAD_CALLED') is None and calls(w) == [], 'v2.2: D-pad right does not reload BeamNG\'s car')
w['inbox'].append(drive_msg(pad=pad(btn=0x2000)))
frame(w)
c = calls(w)
check(c == ['shiftUp'], 'B (Circle) -> shiftUp, like in his BeamNG (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(btn=0x4000)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(btn=0x1000)))
frame(w)
c = calls(w)
check(c == ['shiftDown', 'event parkingbrake 1.000 1'], 'X (Square) -> shiftDown, A (Cross) -> handbrake (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
c = calls(w)
check('event throttle 1.000 1' in c, 'RT full -> throttle 1 (BeamNG\'s PlayStation triggers are 0..1) (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0002)))
frame(w)
c = calls(w)
check(c == [], 'D-pad down alone: nothing (only combinations use it) (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200 | 0x0002)))
frame(w)
c = calls(w)
check(sorted(c) == ['nitrous', 'starter'], 'RB (modifier 1) + D-pad down -> nitrous; v2.52: + the Xbox list\'s starter (both lists run) (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0100)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0100 | 0x0001)))
frame(w)
c = calls(w)
check(c == ['event clutch 1.000 1', 'esc'], 'LB (clutch + modifier 2) + D-pad up -> clutch and ESC mode (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
calls(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200 | 0x0100)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200 | 0x0100 | 0x0040)))
frame(w)
c = calls(w)
check('leftsignal' in c and 'headlights' not in c, 'RB + LB + L3 -> left signal, not headlights (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
frame(w)
c = calls(w)
check('headlights' not in c, 'modifiers let go first, L3 still held: headlights not pressed (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
frame(w)
c = calls(w)
check('headlights' in c, 'L3 on its own -> headlights (%s)' % c)
check(okj.get('padmx') == 'dpadd=btn_r;dpadu=btn_l', 'modifier sets per combination-only control sent to GTA (%s)' % okj.get('padmx'))
check('empty control' in rep3, 'an empty control in the list is skipped, the rest still used')
# review: GTA sends 2-3 messages per BeamNG frame: the release frame of a combination must not tap the plain binding
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
calls(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200 | 0x0100)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0200 | 0x0100 | 0x0040)))
frame(w)
calls(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
c = calls(w)
check('headlights' not in c, 'combination released (modifiers first, then L3, 2 messages per frame): no headlights (%s)' % c)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0040)))
w['inbox'].append(drive_msg(pad=pad(rt=1.0)))
frame(w)
c = calls(w)
check(c.count('headlights') == 1, 'a fresh L3 tap within one frame -> headlights once (%s)' % c)

# ======================================================================== v2.2: BeamNG's real sound setting + background fps
# drive start with no snd/bgf fields: sound on (default), fps kept
check(sv(w, 'AudioMuteOnWindowLoseFocus') is False, 'v2.2 driving: AudioMuteOnWindowLoseFocus set to false (BeamNG plays in the background)')
check(sv(w, 'fpsLimitBackgroundEnabled') is True and sv(w, 'fpsLimitBackground') == 30, 'v2.2: background fps settings untouched by default (keep)')
check(any('settings: while GTA drives: found AudioMuteOnWindowLoseFocus=true, fpsLimitBackgroundEnabled=true, fpsLimitBackground=30' in l for l in w['logs']),
      'v2.2: the current sound + background fps values are logged at drive start')
check(any('AudioMuteOnWindowLoseFocus true -> false: ok, reads back false' in l for l in w['logs']), 'v2.2: the change and its read-back are logged')
check('AudioMuteOnWindowLoseFocus=boolean:true' in w['files'].get('/gtabridge/sound_restore.txt', ''), 'v2.2: the old value is written down before the change (crash restore)')
check(not any('muteOnWindowLoseFocus' in l and '$pref' in l for l in w['logs']), 'v2.2: the old Torque pref is gone')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(sv(w, 'AudioMuteOnWindowLoseFocus') is True, 'v2.2 drive stop: put back to true (what it was)')
check(w['files'].get('/gtabridge/sound_restore.txt') == '', 'v2.2: restore note cleared after putting it back')

# he turned it off himself: nothing changed, and it stays off afterwards
w = make_world('user', sound_muted=False)
w['inbox'].append('{"t":"drive_start","v":"2.2","snd":1,"bgf":-1}')
frame(w)
check(sv(w, 'AudioMuteOnWindowLoseFocus') is False and w['ge'].eval('SET_CALLS') == 0, 'v2.2: already off -> nothing set')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(sv(w, 'AudioMuteOnWindowLoseFocus') is False, 'v2.2: ...and still off after the drive (not forced back on)')

# BeamNGBackgroundFps=60: limit 60 while driving, 30 back after
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.2","snd":1,"bgf":60}')
frame(w)
check(sv(w, 'fpsLimitBackground') == 60 and sv(w, 'fpsLimitBackgroundEnabled') is True, 'v2.2: bgf 60 -> background limit 60 while driving')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(sv(w, 'fpsLimitBackground') == 30 and sv(w, 'AudioMuteOnWindowLoseFocus') is True, 'v2.2: limit 30 and mute put back after')
# bgf 0: no background limit
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.2","snd":1,"bgf":0}')
frame(w)
check(sv(w, 'fpsLimitBackgroundEnabled') is False, 'v2.2: bgf 0 -> background limit off while driving')
# a crash mid-drive: the next run puts everything back
files_after_crash = dict(w['files'])
w3 = make_world('user', files=files_after_crash, pre='SETTINGS_VALUES.AudioMuteOnWindowLoseFocus = false SETTINGS_VALUES.fpsLimitBackgroundEnabled = false')
check(sv(w3, 'AudioMuteOnWindowLoseFocus') is True and sv(w3, 'fpsLimitBackgroundEnabled') is True, 'v2.2: after a crash the next run puts both settings back')

# snd 0 + xinput list
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.1","pb":"xinput","snd":0}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(okj.get('plist') == 'xinput0' and sv(w, 'AudioMuteOnWindowLoseFocus') is True, 'BeamNGPadBindings=xinput and BeamNGSound=0 are followed')
w['inbox'].append(drive_msg(pad=pad(btn=0x1000)))
frame(w)
c = calls(w)
check('shiftUp' in c, 'xinput list: A -> shiftUp (BeamNG\'s Xbox defaults) (%s)' % c)

# ======================================================================== v2.3
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.3","snd":1,"bgf":0}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(okj.get('sw') == 1, 'v2.3: drive_ok says BeamNG was muted in the background (sw=1) so GTA wakes it')
calls(w)
w['inbox'].append('{"t":"act","a":"toggle_headlights"}')
frame(w)
c = calls(w)
check(c.count('headlights') == 1, 'v2.3: "act" runs your BeamNG action by name once (D-pad right in GTA = toggle_headlights) (%s)' % c)
w['inbox'].append('{"t":"act","a":"reload_vehicle"}')
w['inbox'].append('{"t":"act","a":"reset_physics"}')
frame(w)
check(w['ge'].eval('RELOAD_CALLED') is None and w['ge'].eval('RESET_CALLED') is None, 'v2.3: "act" never runs reload / reset actions')
w['veh_vm'].execute('electrics.values.lowbeam = 1 electrics.values.brakelights = 1')
logBefore = w['files'].get('/gtabridge/gtabridge_log.txt')
for _ in range(700):
    w['inbox'].append(drive_msg(pad=pad()))
    frame(w)
vs = json.loads(last_sent(w, 'vs') or '{}')
check(vs.get('li') == 5, 'v2.3: the car\'s lights go to GTA in "vs" (low beams + brake lights = 5, got %s)' % vs.get('li'))
check(w['files'].get('/gtabridge/gtabridge_log.txt') == logBefore, 'v2.3: no log file writes while GTA drives (11 s)')
w['inbox'].append('{"t":"drive_stop"}')
frame(w)
check(w['files'].get('/gtabridge/gtabridge_log.txt') != logBefore and 'GTA asked for action toggle_headlights: run' in w['files'].get('/gtabridge/gtabridge_log.txt', ''),
      'v2.3: the log is written when the drive ends')
w = make_world('user', sound_muted=False)
w['inbox'].append('{"t":"drive_start","v":"2.3","snd":1}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check('sw' not in okj, 'v2.3: BeamNG not muted (you turned the option off): no wake asked')

# ======================================================================== DriveInput=gta (in=0) and a GTA pause
w = make_world('normal')
w['inbox'].append('{"t":"drive_start","v":"2.01","in":0}')
frame(w)
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check(okj.get('inp') == 'fallback' and okj.get('fb') == 'brk,hb,str,thr', 'in=0 -> GTA controls only, fb lists them (%s / %s)' % (okj.get('inp'), okj.get('fb')))
w = make_world('normal')
w['inbox'].append('{"t":"drive_start","v":"2.01","in":1}')
frame(w)
calls(w)
w['inbox'].append(drive_msg(pad=pad(rt=1.0, btn=0x0100)))
frame(w)
calls(w)
w['inbox'].append(drive_msg(pad='', keys='', thr=0, brk=0.6, hb=0).replace('"p":0', '') .replace('}', ',"p":1}'))
frame(w)
c = calls(w)
check('event throttle 0.000 1' in c and 'event clutch 0.000 1' in c and any('parkingbrake 1' in x for x in c), 'GTA paused: everything let go, parking brake (%s)' % c[:8])
check(any('GTA is paused' in l for l in w['logs']), 'pause logged')
w['inbox'].append(drive_msg(pad=pad()))
frame(w)
c = calls(w)
check('event parkingbrake 0.000 2' in c and 'event brake 0.000 2' in c, 'after the pause: failsafe brake / parking brake released (%s)' % c)

# ======================================================================== v2.43: his repair binding (RB + LB + Square)
REP = 'reset_physics,reset_all_physics,recover_vehicle,recover_vehicle_alt,recover_to_last_road,loadHome'
w = make_world('user')
w['inbox'].append('{"t":"drive_start","v":"2.43","deny":"reset_physics,recover_vehicle,loadHome","rep":"%s","ps":1}' % REP)
frame(w)
w['ge'].execute("REPAIRS = {} gtaBridge = { repairHere = function(a) REPAIRS[#REPAIRS + 1] = a end }")
rep4 = w['files'].get('/gtabridge/bindings.txt', '')
line = [l for l in rep4.split('\n') if '-> recover_vehicle' in l]
check(line and 'REPAIR WHERE IT IS' in line[0], 'v2.43: RB+LB+Square -> recover_vehicle forwarded as a repair where the car is (%s)' % (line and line[0].strip()))
okj = json.loads(last_sent(w, 'drive_ok') or '{}')
check('btn_x' in okj.get('padc', '').split(','), 'X (Square) handed over (%s)' % okj.get('padc'))
calls(w)
w['inbox'].append(drive_msg(pad=pad(rt=0.0, btn=0x0200 | 0x0100)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=0.0, btn=0x0200 | 0x0100 | 0x4000)))
frame(w)
w['inbox'].append(drive_msg(pad=pad(rt=0.0, btn=0x0200 | 0x0100 | 0x4000)))
frame(w)
c = calls(w)
reps = w['ge'].eval('#REPAIRS')
check(reps == 1 and w['ge'].eval('REPAIRS[1]') == 'recover_vehicle' and w['ge'].eval('RECOVER_CALLED') is None,
      'v2.43: RB + LB + Square -> the repair (once, BeamNG\'s own recover not run) (%d, %s)' % (reps, c))
check('shiftDown' not in c, 'v2.43: Square alone (shift down) not run with RB + LB held (%s)' % c)
w2 = make_world('user')
w2['inbox'].append('{"t":"drive_start","v":"2.42","deny":"reset_physics,recover_vehicle,loadHome"}')
frame(w2)
rep5 = w2['files'].get('/gtabridge/bindings.txt', '')
line = [l for l in rep5.split('\n') if '-> recover_vehicle' in l]
check(line and 'skipped' in line[0], 'an older GTA plugin (no "rep"): recover_vehicle still skipped as before (%s)' % (line and line[0].strip()))

print()
print('ALL PASS' if not fails else '%d FAILED: %s' % (len(fails), fails))
sys.exit(1 if fails else 0)
