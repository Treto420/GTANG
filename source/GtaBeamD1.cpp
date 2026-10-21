// GtaBeamD1.cpp - GTA x Beam kit v2.4 (v2.4: GTA's ground in BeamNG (T3), pad/radio/sound fixes; see "v2.4"): D1 (car look) + D3 (drawing inside GTA's frame) + T2 (drive mode) + T3 (GTA's world in BeamNG).
//
// A Script Hook V plugin (.asi) for GTA V Legacy (DirectX 11). It draws the user's BeamNG car,
// exported by BeamNG itself (BeamNG mod -> util_export -> car.gltf), at the pose of a GTA car and
// hides the GTA car. GTA's depth buffer hides the drawn car behind GTA's geometry (found in D0).
// Everything it learns goes to GtaBeamD1.log next to GTA5.exe.
//
// Keys (default F11, only while GTA has focus):
//   F11              attach to your car / the nearest car -> GTA car shown too -> detach
//   Shift+F11        ask BeamNG to export the car it has now, then load it
//   Ctrl+F11         re-read the ini (placement, look, shadow, hide mode, debug view) - instant
//   Ctrl+Shift+F11   view: normal / drawn car always on top / depth view / part picker
//   Alt+F11          frame capture: GTA's render passes of one frame -> GtaBeamD1_frame.txt (for D3)
//   F10 (DriveKey)   v2.0 drive mode on/off: GTA's controls drive the BeamNG car (BeamNG's physics)
//   Shift+F10        v2.01: drive check text on/off (what goes to BeamNG, what the BeamNG car does)
//   D-pad down       v2.01 (PadToggle, while driving): pad buttons -> GTA (aim/shoot) or back -> BeamNG
//
// v1.7: BeamNG material layers + paint palette, wheels spin with the GTA car's speed, steer with input.
// v1.8: metal/clear coat/glass shading; car-shaped shadows (contact + GTA's or ours).
// v1.9: translucentBlendOp, black-cubemap glass, normal/metallic/roughness maps, part picker, debug
// views, burnout wheel spin, ShadowMode=own default.
// v1.9.1:
//  - wheels follow GTA's own wheels (spin incl. burnouts/lock-ups, steering angle, suspension travel),
//    read from the GTA car's wheel bones with standard natives; the old speed model stays as fallback;
//  - glass: metal reflections fade with the glass's opacity, nothing reflects the outside when seen
//    from inside the car; layers whose mask hides them everywhere are drawn (RRpbr trims); live UI
//    screens drawn as dark screens;
//  - lighting: reflections and ambient light come from an environment map learned from GTA's own
//    frames (what the camera has seen around the car), the car shadows itself (sun shadow map) and
//    has ambient occlusion (12 directional maps baked once per car), the sun is blocked by GTA's world
//    (raycasts towards the sun), energy-correct sun highlights;
//  - the car is drawn into its own layer, then put on GTA's picture with smoothed edges and without
//    covering the minimap;
//  - Alt+F11 frame capture to find the moment before GTA's HUD/post-processing (the real fix, D3).
// v1.9.3:
//  - D3 test: the car is drawn INSIDE GTA's frame, into its HDR scene picture right before GTA's
//    post-processing (exposure/tone mapping, bloom, anti-aliasing) and before the HUD. Where that is,
//    is learned from the render targets GTA binds each frame (OMSetRenderTargets, the same hook as D0).
//    Falls back to drawing at Present (v1.9.1) until it is found, or with DrawStage=present;
//  - textures straight from BeamNG's .dds files, kept block-compressed on the GPU (BC1-BC7);
//  - HideMode=ghost: the GTA car stays "visible" to GTA (so its wheels keep turning) at alpha 0 with its
//    shadow switched off; the speed model's burnout no longer flips direction at a standstill;
//  - sun-blocking raycasts are asynchronous (no stalls in GTA's script thread); timing stats in the log.
// v1.9.4/v1.9.5: normals fix (BeamNG exports them in its own axes), D3 locks by render-target signature,
//    per-stage learned surroundings, one-sided glass, frame pairing, ride-height check, light measurement,
//    Alt+F11 timing recording.
// v1.9.6: car/camera pairing (the stutter: GTA's camera reading lags the car by one game frame, so the
//    car of the tick before is drawn with this tick's camera; auto-checked), HDR sun/ambient set from the
//    light measurement (HdrSunToSky/HdrAmbient=auto).
// v1.9.7: reflections from GTA's own reflection map (dual paraboloid, read in place; its layout found by
//    comparing it with GTA's picture; brightness matched), tighter contact shadow.
// v2.0: drive mode (T2). GTA's controls go over UDP to the BeamNG mod (input.event in the BeamNG car), BeamNG
//    streams the car's pose (position, direction, velocity, wheel hubs) every frame; the plugin interpolates
//    it, moves the hidden GTA car there (coords/rotation/velocity natives, so GTA's camera follows) and draws
//    the car with BeamNG's wheels. Reflection brightness fixed at 1.0 by default.
// v2.01: YOUR BeamNG controls drive the car. The plugin reads the pad (XInput: Xbox pads, DS4Windows) and,
//    while GTA has focus, the keyboard itself and sends their raw state; the BeamNG mod runs them through
//    your own BeamNG bindings (clutch, shifting, drive modes...). GTA's own reactions to the controls
//    BeamNG uses are switched off while driving; D-pad down (PadToggle) hands the pad's buttons to GTA
//    (drive-by: aim/shoot/weapons) and back. Sticks and triggers stay BeamNG's. The BeamNG car's
//    throttle/brake/gear/rpm come back for an on-screen check (Shift+F10) and the log.
// v2.4 (T3: GTA's world -> BeamNG):
//  - a fixed mapping: BeamNG = GTA - origin (rounded to 500 m at F10) and WorldZLift (200 m) up, no rotation. F10 (at a
//    standstill) builds GTA's ground around the car in BeamNG, then BeamNG's car is put on it at the GTA car's spot;
//  - GTA's ground is measured with asynchronous downward rays (START_SHAPE_TEST_LOS_PROBE, map only) on 8 m tiles
//    (0.5 m apart near the car and ahead, 1 m further out), with GTA's ground material per point
//    (GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL); tiles stream ahead of the car (speed x WorldAheadS) and go behind it;
//  - GTA's ground materials map to BeamNG ground models (asphalt, dirt, grass, sand...) through GtaBeamD1_surfaces.json,
//    so BeamNG's tyres feel GTA's ground; every material seen goes to GtaBeamD1_materials.txt;
//  - fall-through safety: BeamNG's car more than 3 m under GTA's ground = put back on the last good spot;
//  - F10 off hands BeamNG's speed to GTA's car;
//  - pad: D-pad down switches the whole pad between GTA and BeamNG (the rest is each game's own buttons: GTA's radio
//    wheel works in GTA mode); the right stick and Y stay GTA's, the left stick and triggers BeamNG's;
//  - radio: stays off if it was off; GTA's own radio controls in GTA mode (the plugin's radio button is gone);
//  - sound: BeamNGSoundWake=session (BeamNG plays in the background all session, quiet while you walk), bounce logs
//    more, "message" removed (it failed 8 of 8);
//  - DriveCameraSlipFix=1 by default; poses carry BeamNG's simulation clock (measurement for the pose clock, P1).
//
// How it finds GTA's depth buffer: it watches which depth buffers GTA binds each frame
// (ID3D11DeviceContext::OMSetRenderTargets and ClearDepthStencilView, through the context's
// vtable - the same API-level technique overlays such as ReShade use) and picks the busiest one
// that matches the screen. No game code is read or patched.
//
// Parts adapted from SkyCraft (https://github.com/chasmlol/SkyCraft), MIT License,
// Copyright (c) 2026 chasmlol: saving/restoring the pipeline around a Present-time draw,
// the 16-point depth probe that detects the depth convention, and the depth linearisation.
// See LICENSE-SkyCraft.txt. glTF loading: cgltf (MIT); images: stb_image (public domain / MIT).

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <d3dcompiler.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include "carmodel.h"
#include "shv.h"

#define D1_VERSION "2.73"

namespace
{
	// ---------------------------------------------------------------------------------------
	// Constants
	// ---------------------------------------------------------------------------------------

	// Native hashes (checked against alloc8or's GTA V native database).
	namespace N
	{
		constexpr UINT64 PLAYER_PED_ID = 0xD80958FC74E988A6;
		constexpr UINT64 GET_ENTITY_COORDS = 0x3FEF770D40960D5A;
		constexpr UINT64 GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS = 0x1899F328B0E12848;
		constexpr UINT64 DOES_ENTITY_EXIST = 0x7239B21A38F536BA;
		constexpr UINT64 GET_ENTITY_MODEL = 0x9F47B058362C84B5;
		// v2.5: props
		constexpr UINT64 GET_ENTITY_MATRIX = 0xECB2FC7235A7D137;        // entity, &forward, &right, &up, &position
		constexpr UINT64 GET_IS_ENTITY_A_FRAG = 0x0C112765300C7E1E;     // object: breakable (lamp posts, signs, fences)
		constexpr UINT64 IS_ENTITY_ATTACHED = 0xB346476EF1A64897;
		constexpr UINT64 IS_ENTITY_VISIBLE = 0x47D6F43D77935C75;
		constexpr UINT64 IS_ENTITY_STATIC = 0x1218E6886D3D8327;
		constexpr UINT64 SET_ENTITY_DYNAMIC = 0x1718DE8E3F2823CA;       // entity, toggle: its physics wakes up (a fixed prop can move)
		constexpr UINT64 ACTIVATE_PHYSICS = 0x710311ADF0E20730;
		constexpr UINT64 APPLY_FORCE_TO_ENTITY = 0xC5F68BE9613E2D18;    // entity, type, x y z, offset x y z, bone, dirRel, ignoreUp, forceRel, p12, p13
		constexpr UINT64 GET_ENTITY_VELOCITY = 0x4805D2B1D8CF94A9;
		constexpr UINT64 SET_ENTITY_VISIBLE = 0xEA1C610A04DB6BBB;
		constexpr UINT64 SET_ENTITY_ALPHA = 0x44A0870B7E92D7C0;
		constexpr UINT64 RESET_ENTITY_ALPHA = 0x9B1E824FFBB7027A;
		constexpr UINT64 GET_ENTITY_SPEED_VECTOR = 0x9A8D700A51CB7B0D;
		constexpr UINT64 GET_ENTITY_BONE_INDEX_BY_NAME = 0xFB71170B7E76ACBA;
		constexpr UINT64 GET_ENTITY_BONE_ROTATION = 0xCE6294A232D03786;  // world rotation of a bone (build 791+)
		constexpr UINT64 GET_WORLD_POSITION_OF_ENTITY_BONE = 0x44A8FCB8ED227738;
		constexpr UINT64 GET_PED_IN_VEHICLE_SEAT = 0xBB40DD2270B65366;
		constexpr UINT64 GET_CONTROL_NORMAL = 0xEC3C9B8D5327B563;
		constexpr UINT64 IS_VEHICLE_IN_BURNOUT = 0x1297A88E081430EB;
		constexpr UINT64 GET_MODEL_DIMENSIONS = 0x03E8D3D5F549087A;
		constexpr UINT64 GET_VEHICLE_PED_IS_IN = 0x9A9112A0FE9A4713;
		constexpr UINT64 GET_PLAYERS_LAST_VEHICLE = 0xB6997A7EB3F5C8C0;
		constexpr UINT64 GET_CLOSEST_VEHICLE = 0xF73EB622C4F1689B;
		constexpr UINT64 GET_DISPLAY_NAME_FROM_VEHICLE_MODEL = 0xB215AAC32D25D019;
		constexpr UINT64 GET_CLOCK_HOURS = 0x25223CA6B4D20B7F;
		constexpr UINT64 GET_CLOCK_MINUTES = 0x13D2B8ADD79640F2;
		constexpr UINT64 GET_FINAL_RENDERED_CAM_COORD = 0xA200EB1EE790F448;
		constexpr UINT64 GET_FINAL_RENDERED_CAM_ROT = 0x5B4E4C817FCC2DFB;
		constexpr UINT64 GET_FINAL_RENDERED_CAM_FOV = 0x80EC114669DAEFF4;
		constexpr UINT64 GET_FINAL_RENDERED_CAM_NEAR_CLIP = 0xD0082607100D7193;
		constexpr UINT64 GET_FINAL_RENDERED_CAM_FAR_CLIP = 0xDFC8CBC606FDB0FC;
		constexpr UINT64 IS_PAUSE_MENU_ACTIVE = 0xB0034A223497FFCB;
		constexpr UINT64 ACTIVATE_FRONTEND_MENU = 0xEF01D36B9C9D0C7B;  // v2.43: GTA's pause menu when you pause BeamNG
		constexpr UINT64 SET_FRONTEND_ACTIVE = 0x745711A75AB09277;
		constexpr UINT64 GET_GAMEPLAY_CAM_COORD = 0x14D6F5678D8F1B37;  // v1.9.4
		constexpr UINT64 GET_GAMEPLAY_CAM_ROT = 0x837765A25378F0BB;
		constexpr UINT64 GET_FRAME_TIME = 0x15C40837039FFAF7;
		// v2.6: weapons and explosions on BeamNG's car
		constexpr UINT64 IS_EXPLOSION_IN_SPHERE = 0xAB0F816885B0E483;        // type (-1 any), x, y, z, radius
		constexpr UINT64 GET_PED_LAST_WEAPON_IMPACT_COORD = 0x6C4D0409BA1A2BC2;
		constexpr UINT64 IS_PED_SHOOTING = 0x34616828CD07F1A1;
		constexpr UINT64 GET_SELECTED_PED_WEAPON = 0x0A6DB4965674D243;
		constexpr UINT64 GET_WEAPONTYPE_GROUP = 0xC3287EE3050FB74C;
		constexpr UINT64 GET_PED_BONE_COORDS = 0x17C07FC640E86B4E;
		constexpr UINT64 GET_PED_ACCURACY = 0x37F4AD56ECBC0CD6;
		constexpr UINT64 IS_PED_IN_COMBAT = 0x4859F1FC66A6278E;           // ped, target
		constexpr UINT64 GET_WATER_HEIGHT = 0xF6829842C06AE524;              // v2.73: x, y, z, float* height (hole fill: not over water)
		constexpr UINT64 SET_ENTITY_AS_MISSION_ENTITY = 0xAD738C3085FE7E11;  // v2.61: entity, scriptHost, grab (GTA doesn't delete it)
		constexpr UINT64 START_PARTICLE_FX_LOOPED_ON_ENTITY = 0x1AE42C1660FD6517;  // v2.61: name, entity, offset xyz, rot xyz, scale, flip xyz
		constexpr UINT64 STOP_PARTICLE_FX_LOOPED = 0x8F75998877616996;
		constexpr UINT64 SET_PARTICLE_FX_LOOPED_OFFSETS = 0xF7DDEBEC43483C43;  // v2.73: handle, offset xyz, rot xyz (tyre smoke follows its wheel)
		constexpr UINT64 DOES_PARTICLE_FX_LOOPED_EXIST = 0x74AFEF0D2E1E409B;
		constexpr UINT64 USE_PARTICLE_FX_ASSET = 0x6C38AF3693A69A91;
		constexpr UINT64 REQUEST_NAMED_PTFX_ASSET = 0xB80D8756B4668AB6;
		constexpr UINT64 HAS_NAMED_PTFX_ASSET_LOADED = 0x8702416E512EC454;
		constexpr UINT64 GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS = 0x2274BC1C4885E333;
		constexpr UINT64 IS_PED_DEAD_OR_DYING = 0x3317DEDB88C95038;
		constexpr UINT64 EXPLODE_VEHICLE = 0xBA71116ADF5B514C;
		constexpr UINT64 START_SCRIPT_FIRE = 0x6B83617E04503888;
		constexpr UINT64 REMOVE_SCRIPT_FIRE = 0x7FF548385680673F;
		constexpr UINT64 SET_ENTITY_PROOFS = 0xFAEE099C6F890BB8;
		constexpr UINT64 GET_ENTITY_HEALTH = 0xEEF059FAD016D209;
		constexpr UINT64 SET_ENTITY_HEALTH = 0x6B76DC1F3AE6E6A3;
		constexpr UINT64 GET_FRAME_COUNT = 0xFC8202EFC642E6F2;  // v1.9.5 (timing recording)
		constexpr UINT64 GET_GAME_TIMER = 0x9CD27B0045628463;
		constexpr UINT64 START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE = 0x377906D8A31E5586;
		constexpr UINT64 START_SHAPE_TEST_LOS_PROBE = 0x7EE9F5D83DD4F90E;  // asynchronous: the result comes a frame or two later
		constexpr UINT64 SET_ENTITY_FLAG_SUPPRESS_SHADOW = 0xD05A3241B9A86F19;  // build 1180+
		constexpr UINT64 IS_PED_IN_VEHICLE = 0xA3EE4A07279BB9DB;
		constexpr UINT64 GET_SHAPE_TEST_RESULT = 0x3D87450E15D98694;
		constexpr UINT64 GET_SAFE_ZONE_SIZE = 0xBAF107B6BB2C97F0;
		constexpr UINT64 GET_ASPECT_RATIO = 0xF1307EF624A80D87;
		constexpr UINT64 IS_HUD_HIDDEN = 0xA86478C6958735C5;
		constexpr UINT64 IS_RADAR_HIDDEN = 0x157F93B036700462;
		constexpr UINT64 IS_RADAR_PREFERENCE_SWITCHED_ON = 0x9EB6522EA68F22FE;
		constexpr UINT64 IS_MINIMAP_RENDERING = 0xAF754F20EB5CD51A;
		constexpr UINT64 BEGIN_TEXT_COMMAND_THEFEED_POST = 0x202709F4C58A0424;
		constexpr UINT64 ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME = 0x6C188BE134E074AA;
		constexpr UINT64 END_TEXT_COMMAND_THEFEED_POST_TICKER = 0x2ED7843F8F801023;
		// v2.0 drive mode (alloc8or's native database)
		constexpr UINT64 SET_ENTITY_COORDS_NO_OFFSET = 0x239A3351AC1DA385;  // entity, x, y, z, keepTasks, keepIK, doWarp
		constexpr UINT64 SET_ENTITY_ROTATION = 0x8524A8B0171D5E07;          // entity, pitch, roll, yaw, rotationOrder, p5
		constexpr UINT64 SET_ENTITY_QUATERNION = 0x77B21BE7AC540F07;        // entity, x, y, z, w
		constexpr UINT64 SET_ENTITY_VELOCITY = 0x1C99BB7B6E96D16F;
		constexpr UINT64 GET_VEHICLE_CLASS = 0x29439776AAA00A62;          // v2.7
		constexpr UINT64 SET_PED_TO_RAGDOLL = 0xAE99FB955581844A;         // v2.7: ped, time1, time2, type, p4, p5, p6
		constexpr UINT64 KNOCK_PED_OFF_VEHICLE = 0x45BBCBA77C29A841;      // v2.7
		constexpr UINT64 SET_VEHICLE_DAMAGE = 0xA1DD317EA8FD4F29;         // v2.7: veh, x y z offset, damage, radius, focusOnModel
		constexpr UINT64 APPLY_DAMAGE_TO_PED = 0x697157CED63F18D4;        // v2.7: ped, amount, p2, p3, weapon
		constexpr UINT64 IS_PED_IN_ANY_VEHICLE = 0x997ABD671D25CA0B;      // v2.7
		constexpr UINT64 START_PARTICLE_FX_NON_LOOPED_AT_COORD = 0x25129531F77B9ED3;  // v2.7: name, x y z, rot x y z, scale, axes
		constexpr UINT64 START_PARTICLE_FX_LOOPED_AT_COORD = 0xE184F4F0DC5910E7;      // v2.7: name, x y z, rot x y z, scale, axes, p11
		constexpr UINT64 SET_ENTITY_COLLISION = 0x1A9205C1B9EE827F;         // entity, toggle, keepPhysics
		constexpr UINT64 SET_ENTITY_HAS_GRAVITY = 0x4A4722448F18EEF5;
		constexpr UINT64 FREEZE_ENTITY_POSITION = 0x428CA6DBD1094446;
		constexpr UINT64 DISABLE_CONTROL_ACTION = 0xFE99B66D079CF6BC;       // control, action, disableRelatedActions
		constexpr UINT64 GET_DISABLED_CONTROL_NORMAL = 0x11E65974A982637C;
		constexpr UINT64 IS_USING_KEYBOARD_AND_MOUSE = 0xA571D46727E2B718;
		constexpr UINT64 GET_GROUND_Z_FOR_3D_COORD = 0xC906A7DAB05C8D2B;
		// v2.01: on-screen drive check text (alloc8or's native database)
		constexpr UINT64 SET_TEXT_FONT = 0x66E0276CC5F6B9DA;
		constexpr UINT64 SET_TEXT_SCALE = 0x07C837F9A01C34C9;
		constexpr UINT64 SET_TEXT_COLOUR = 0xBE6B23FFA53FB442;  // v2.01.1: was a wrong hash (Script Hook V: "Can't find native")
		constexpr UINT64 SET_TEXT_OUTLINE = 0x2513DFB0FB8400FE;
		constexpr UINT64 BEGIN_TEXT_COMMAND_DISPLAY_TEXT = 0x25FBB336DF1804CB;
		constexpr UINT64 END_TEXT_COMMAND_DISPLAY_TEXT = 0xCD015E5BB0D96A57;  // x, y, p2
		// v2.1: the BeamNG gauge, and GTA's engine sound off while BeamNG's plays
		constexpr UINT64 DRAW_RECT = 0x3A618A217E5154F0;               // x, y, w, h, r, g, b, a, p8
		constexpr UINT64 SET_TEXT_JUSTIFICATION = 0x4E096588B13FFECA;  // 0 centre, 1 left, 2 right
		constexpr UINT64 SET_TEXT_WRAP = 0x63145D9C883A1A70;           // start, end (needed for right-justified text)
		constexpr UINT64 SET_VEHICLE_ENGINE_ON = 0x2497C4717C8B881E;   // vehicle, on, instantly, disableAutoStart
		// v2.3: GTA's radio while the hidden car's engine is off (the phone radio)
		constexpr UINT64 SET_AUDIO_FLAG = 0xB9EFD5C25018725A;
		constexpr UINT64 SET_MOBILE_RADIO_ENABLED_DURING_GAMEPLAY = 0x1098355A16064BB3;
		constexpr UINT64 SET_MOBILE_PHONE_RADIO_STATE = 0xBF286C554784F3DF;
		constexpr UINT64 GET_PLAYER_RADIO_STATION_INDEX = 0xE8AF77C4C06ADC93;
		constexpr UINT64 SET_RADIO_TO_STATION_NAME = 0xC69EDA28699D5107;
		constexpr UINT64 GET_PLAYER_RADIO_STATION_NAME = 0xF6D733C32076AD03;
		constexpr UINT64 GET_IS_VEHICLE_ENGINE_RUNNING = 0xAE31E7DF9B5B132E;
		// v2.4: GTA's ground for BeamNG (T3)
		constexpr UINT64 GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL = 0x65287525D951F6BE;  // handle, hit*, end*, normal*, material*, entity*
		constexpr UINT64 REQUEST_COLLISION_AT_COORD = 0x07503F7948F491A7;
		constexpr UINT64 GET_ENTITY_SPEED = 0xD5037BA82E12416F;
		// v2.41
		constexpr UINT64 HAS_COLLISION_LOADED_AROUND_ENTITY = 0xE9676F61BC0B3321;
		constexpr UINT64 SET_VEH_RADIO_STATION = 0x1B9C0099CB942AC6;          // vehicle, station name ("OFF" = off)
		constexpr UINT64 IS_DISABLED_CONTROL_JUST_PRESSED = 0x91AEF906BCA88877;
		// v2.51: did a knocked prop really go over in GTA (measured 1 s after)
		constexpr UINT64 HAS_OBJECT_BEEN_BROKEN = 0x8ABFB70C49CC43E2;
		constexpr UINT64 GET_ENTITY_UPRIGHT_VALUE = 0x95EED5A694951F9F;
		// v2.71: a knocked map prop is swapped for a copy the plugin owns (GTA's map props don't move for a script)
		constexpr UINT64 REQUEST_MODEL = 0x963D27A58DF860AC;
		constexpr UINT64 HAS_MODEL_LOADED = 0x98A4EB5D89A0C952;
		constexpr UINT64 SET_MODEL_AS_NO_LONGER_NEEDED = 0xE532F5D78798DAAB;
		constexpr UINT64 CREATE_OBJECT = 0x509D5878EB39E842;                 // model, x y z, isNetwork, scriptHostObj, dynamic
		constexpr UINT64 DELETE_OBJECT = 0x539E0AE3E6634B9F;                 // Object*
		constexpr UINT64 CREATE_MODEL_HIDE_EXCLUDING_SCRIPT_OBJECTS = 0x3A52AE588830BF7F;  // x y z, radius, model, p5
		constexpr UINT64 REMOVE_MODEL_HIDE = 0xD9E3006FB3CBD765;             // x y z, radius, model, p5 (false)
		// v2.71: the driver's seat and visibility
		constexpr UINT64 GET_ENTITY_ALPHA = 0x5A47B3B5E63E94C6;
		constexpr UINT64 SET_PED_AS_NO_LONGER_NEEDED = 0x2595DD4236549CE3;      // Ped* (v2.73: a knocked person back to GTA)
		constexpr UINT64 IS_ENTITY_A_MISSION_ENTITY = 0x0A7B270912999B3C;       // v2.73
		constexpr UINT64 GET_ENTITY_HEIGHT_ABOVE_GROUND = 0x1DD55701034110E5;   // v2.73 (the hit log)
		constexpr UINT64 SET_VEHICLE_AS_NO_LONGER_NEEDED = 0x629BFA74418D6239;  // Vehicle* (v2.71: a twinned GTA car back to GTA)
		constexpr UINT64 GET_VEHICLE_DEFORMATION_AT_POS = 0x4EC6CFBC7B2E9536;   // v2.72: veh, offset x y z -> Vector3
		constexpr UINT64 IS_PED_RAGDOLL = 0x47E4E977581C5B55;
		constexpr UINT64 SET_ENTITY_ANGULAR_VELOCITY = 0x8339643499D1222E;     // v2.72: entity, x y z (rad/s)
		constexpr UINT64 GET_VEHICLE_DOOR_ANGLE_RATIO = 0xFE3F9C29F7B32BD5;    // v2.72: veh, door (0 = driver's) -> 0..1
		constexpr UINT64 GET_CLOSEST_OBJECT_OF_TYPE = 0xE143FA2249364369;      // x y z, radius, model, isMission (true: not mission objects), p6, p7
	}

	// GTA control ids (input group 0)
	constexpr int kInputVehMoveLR = 59;
	constexpr int kInputVehAccelerate = 71;
	constexpr int kInputVehBrake = 72;
	constexpr int kInputVehHandbrake = 76;

	// ID3D11DeviceContext vtable slots (from the d3d11.h C interface layout).
	constexpr int kSlotDrawIndexed = 12;
	constexpr int kSlotDraw = 13;
	constexpr int kSlotDrawIndexedInstanced = 20;
	constexpr int kSlotDrawInstanced = 21;
	constexpr int kSlotOMSetRenderTargets = 33;
	constexpr int kSlotOMSetRenderTargetsAndUAVs = 34;
	constexpr int kSlotDispatch = 41;
	constexpr int kSlotClearRenderTargetView = 50;
	constexpr int kSlotClearDepthStencilView = 53;

	constexpr double kPi = 3.14159265358979323846;
	constexpr double kDeg = kPi / 180.0;
	constexpr int    kMaxDsvs = 64;
	constexpr int    kCamRing = 16;
	constexpr int    kPsSlots = 35;  // pixel shader texture slots we use (saved/restored around our drawing)
	constexpr int    kSamplers = 5;
	constexpr int    kViewModes = 4;
	constexpr int    kAoDirs = 18;  // v1.9.5: 12 above the car's horizon + 6 below (inside the car, below is the floor)
	constexpr int    kAoSize = 512;
	constexpr UINT   kLLW = 128, kLLH = 64;  // learned environment map (latitude/longitude)
	// v1.9.6: paint surface when the car file doesn't have the paint's own values (older BeamNG mod): a
	// typical glossy car paint (metallic, roughness, clear coat, clear coat roughness)
	constexpr float  kPaintDefault[4] = { 0.3f, 0.45f, 1.0f, 0.05f };
	constexpr int    kDrvWheels = 8;    // v2.0: wheels BeamNG can drive us (per car file)
	constexpr int    kDrvRing = 128;    // v2.0: poses kept by the receiver

	template <class T>
	void SafeRelease(T*& p)
	{
		if (p) {
			p->Release();
			p = nullptr;
		}
	}

	// ---------------------------------------------------------------------------------------
	// Log file (GtaBeamD1.log next to the .asi)
	// ---------------------------------------------------------------------------------------

	HMODULE          g_module = nullptr;
	CRITICAL_SECTION g_logCs;
	HANDLE           g_logFile = INVALID_HANDLE_VALUE;
	ULONGLONG        g_startMs = 0;

	void PathNextToModule(const char* ext, char* out, size_t outSize)
	{
		char path[MAX_PATH] = {};
		GetModuleFileNameA(g_module, path, MAX_PATH);
		char* dot = strrchr(path, '.');
		if (dot) {
			*dot = 0;
		}
		snprintf(out, outSize, "%s%s", path, ext);
	}

	void InitLog()
	{
		InitializeCriticalSection(&g_logCs);
		g_startMs = GetTickCount64();
		char path[MAX_PATH + 8];
		PathNextToModule(".log", path, sizeof(path));
		g_logFile = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	void Log(const char* fmt, ...)
	{
		if (g_logFile == INVALID_HANDLE_VALUE) {
			return;
		}
		char         buf[2048];
		const double t = double(GetTickCount64() - g_startMs) / 1000.0;
		int          n = snprintf(buf, sizeof(buf), "[%9.3f] ", t);
		va_list      ap;
		va_start(ap, fmt);
		int m = vsnprintf(buf + n, sizeof(buf) - n - 3, fmt, ap);
		va_end(ap);
		if (m < 0) {
			m = 0;
		}
		n = std::min<int>(n + m, int(sizeof(buf)) - 3);
		buf[n++] = '\r';
		buf[n++] = '\n';
		EnterCriticalSection(&g_logCs);
		DWORD written = 0;
		WriteFile(g_logFile, buf, DWORD(n), &written, nullptr);
		LeaveCriticalSection(&g_logCs);
	}

	// ---------------------------------------------------------------------------------------
	// Settings (GtaBeamD1.ini next to the .asi; every value has a default)
	// ---------------------------------------------------------------------------------------

	struct Config
	{
		int   frameDelay = 0;          // camera samples behind the newest (0 tested best in D0)
		bool  fovHorizontal = false;   // GTA's FOV is vertical (D0)
		float rollSign = 1.0f;
		float tolRel = 0.002f;         // drawn pixel hidden when it is this much (relative) behind GTA's surface
		float tolAbs = 0.05f;          // ... plus this many metres
		int   depthSource = 0;         // 0 auto, 1 live, 2 copy made before GTA clears the depth
		int   depthReversed = -1;      // -1 auto (probe), 1 reversed Z, 0 standard Z
		float nearScale = 1.0f;
		int   key = VK_F11;
		// D1
		char  carFile[MAX_PATH] = "";  // fixed car file (empty: the path BeamNG reports)
		int   maxTex = 2048;
		int   maxDataTex = 1024;       // normal/metallic/roughness/opacity maps
		int   meshAxes = -1;           // -1 auto, 0 glTF, 1 BeamNG
		int   meshOrient = -1;         // -1 auto, 0 export direction, 1 vehicle space
		float offX = 0, offY = 0, offZ = 0, yawDeg = 0;
		float paintGamma = 2.2f;       // BeamNG paint values -> linear
		float exposure = 1.0f;         // Brightness
		float ambient = 0.35f;         // light from the surroundings (v1.9.1 meaning)
		float specStrength = 1.0f;     // sun highlights
		float sunStrength = 1.0f;
		float alphaCut = 0.0f;
		float glassMinAlpha = 0.0f;
		float glassReflections = 1.0f; // reflections on see-through parts
		bool  maskCheck = true;        // ignore layer masks that hide a layer everywhere
		int   exportTimeout = 300;     // seconds
		int   port = 47011;
		bool  autoLoad = true;         // load CarFile (or the default export path) at start
		int   hideMode = -1;           // -1 auto (ShadowMode=gta: transparent, else ghost), 0 transparent (GTA keeps its shadow), 1 invisible, 2 ghost
		bool  showDriver = true;       // v1.9.3: the player stays visible in the (ghost) GTA car
		bool  driverSeat = true;       // v2.71: drive mode: GTA's car is placed so your head sits at BeamNG's driver camera
		float seatAdj[3] = {};         // v2.71: and moved this much from there (right, forward, up; m)
		int   drawStage = 1;           // v1.9.3: 1 auto (inside GTA's frame when found, else at Present), 0 present (v1.9.1)
		float hdrSunToSky = 6.0f;      // inside GTA's frame: sunlight vs. light from the sky
		float hdrAmbient = 1.0f;       // inside GTA's frame: light from the surroundings
		int   hdrTarget = 0;           // inside GTA's frame: 0 auto, 1 the HDR target GTA drew last (with its depth), 2 the one before it
		bool  texCompression = true;   // v1.9.3: keep .dds textures compressed (BC) on the GPU
		int   cameraSource = -1;       // v1.9.4: -1 auto, 0 GTA's final rendered camera, 1 GTA's gameplay camera
		bool  camSlipFix = true;       // v1.9.4: put a camera sample that is a game step out of step with the car back in step
		bool  framePairing = true;     // v1.9.5: each drawn frame takes the script sample at a fixed offset from its frame number
		bool  oneSidedGlass = true;    // v1.9.5: see-through parts BeamNG marks one-sided are drawn from their front only
		bool  rideCheck = true;        // v1.9.5: measure how far GTA's ground is under the drawn tyres (log)
		int   carCamPairing = -1;      // v1.9.6: -1 auto, 0 car and camera from the same tick, 1 car from the tick before (GTA's camera lags one game frame)
		bool  hdrSunAuto = true;       // v1.9.6: HdrSunToSky / HdrAmbient from the light measurement
		bool  hdrAmbAuto = true;
		// v1.9.6: paint surface (metallic, roughness, clear coat, clear coat roughness); < 0 = auto (from the
		// BeamNG export, else the defaults)
		float paintSurf[4] = { -1, -1, -1, -1 };
		// v1.9.7: reflections from GTA's own reflection map (inside GTA's frame)
		bool  gtaRefl = true;          // GtaReflections: auto (on once its orientation is found) / 0
		float gtaReflScale = -1.0f;    // GtaReflectionScale: < 0 = auto (matched to GTA's picture)
		int   gtaReflLayout[2] = { -1, -1 };  // GtaReflectionLayoutUp/Down: -1 auto, 0..15 fixed
		int   shadowMode = 1;          // 0 gta (GTA's shadow of the hidden car), 1 own (our sun shadow), 2 off
		float contactStrength = 0.6f;  // darkness right under the car
		float sunShadowStrength = 0.7f;   // v1.9.5: 0.55 -> 0.7 (GTA's own shadows are darker)
		float shadowSoftness = 1.0f;   // blur of our shadows (1 = normal)
		float sunAzimuth = 0.0f;       // degrees added to our sun's compass direction
		float sunElevation = 1.0f;     // multiplies our sun's height
		bool  selfShadows = true;      // the car shadows itself
		int   selfShadowSize = 1024;
		bool  ao = true;               // ambient occlusion maps
		float aoStrength = 1.0f;
		bool  worldShadows = true;     // GTA's world blocks the sun (raycasts)
		float reflections = 1.0f;      // reflections on paint, glass, metal
		int   environment = 1;         // 0 sky (made-up sky gradient), 1 world (learned from GTA's frames)
		float envBrightness = 1.0f;
		bool  normalMaps = true;
		float normalStrength = 1.0f;
		float normalGreen = -1.0f;     // -1 OpenGL style (BeamNG), +1 DirectX style
		bool  surfaceMaps = true;      // metallic/roughness maps
		bool  edgeSmoothing = true;
		bool  hudMask = true;          // keep the minimap on top
		float hudMargin = 6.0f;        // pixels around the minimap
		int   debugView = 0;           // 0 off, 1 colour, 2 layer weights, 3 normals, 4 metal/rough/coat, 5 see-through, 6 materials, 7 light
		bool  wheels = true;           // spin/steer wheels
		int   wheelSource = -1;        // -1 auto (GTA's wheels when readable, else speed), 0 speed, 1 GTA's wheels
		bool  suspension = true;       // wheels move up/down with GTA's suspension
		float maxSteer = 32.0f;        // degrees at full steering input (speed model)
		float steerSign = 1.0f;
		int   drivenWheels = -1;       // -1 auto (from the BeamNG export, else rear), 0 rear, 1 front, 2 all
		float burnoutSpeed = 22.0f;    // m/s at the tyre surface during a GTA burnout (speed model)
		bool  handbrakeLocks = true;   // handbrake stops the rear wheels (speed model)
		// v2.0 drive mode (T2)
		int   driveKey = VK_F10;       // DriveKey: start/stop drive mode
		float driveSteerSign = 0.0f;   // DriveSteerSign: 0 auto (learned from how the car turns), +1 / -1 fixed
		int   driveFilter = -1;        // DriveInputFilter: -1 auto (keyboard -> BeamNG's key filter, pad -> pad filter), 0 key, 1 pad, 2 direct
		float driveDelayMs = -1.0f;    // DriveSmoothingMs: < 0 auto (from how often BeamNG sends), else fixed
		bool  driveSetVelocity = true; // DriveSetVelocity: the GTA car moves with BeamNG's speed between updates (GTA's camera sees the speed)
		// v2.2: drive-mode stutter
		bool  driveSlipFix = false;    // DriveCameraSlipFix: the camera slip fix in drive mode (CameraSlipFix is for normal driving)
		int   driveDrawPose = 0;       // DriveDrawPose: 0 readback (GTA's car as GTA reports it), 1 set (the pose we set + its speed x GTA's step)
		int   beamBgFps = 0;           // BeamNGBackgroundFps: -1 keep BeamNG's setting, 0 no limit (v2.3 default), n = limit to n while driving
		// v2.01: your BeamNG controls
		int         driveInput = 1;        // DriveInput: 1 beamng (pad + keyboard read here, run through your BeamNG bindings), 0 gta (v2.0: GTA's controls)
		int         padToggle = 11;        // PadToggle: pad control that hands the buttons to GTA and back (-1 none); 11 = D-pad down
		uint32_t    padAlwaysGta = 0;      // PadAlwaysGta: bit per pad control (PadCtl)
		uint32_t    padAlwaysBeam = 0;     // PadAlwaysBeamNG
		bool        padStartGta = false;   // PadStartsInGtaMode
		int         padIndex = -1;         // PadIndex: XInput pad 0-3, -1 auto (the one in use)
		std::string keysAlwaysGta;         // KeysAlwaysGta: " f escape p tab " (BeamNG key names, spaced)
		std::string beamCats, beamNever, beamAlso;  // BeamNGActionCategories / BeamNGActionsNever / BeamNGActionsAlso
		bool        driveHud = false;      // DriveCheckText (v2.1: off by default, the gauge shows the car)
		bool        parkBlock = true;      // ParkBlocksBeamNG: GTA focused + car parked = BeamNG's own controls off
		// v2.1
		std::string padList = "auto";      // BeamNGPadBindings: auto / xinput / playstation (which of your BeamNG pad lists)
		bool        gauge = true;          // DriveGauge: speed / gear / rpm of the BeamNG car
		bool        gaugeKmh = false;      // DriveGaugeUnits: mph (default) or kmh
		bool        beamSound = true;      // BeamNGSound: BeamNG keeps its sound with its window in the background
		bool        gtaEngineSound = false;  // DriveGtaEngineSound: GTA's own engine sound of the hidden car while driving
		// v2.3
		bool        drivePoseLog = true;   // DrivePoseLog (v2.42): BeamNG's poses and the drawn car to GtaBeamD1_poses.csv (for the rubber band)
		int         drivePoseClock = 1;    // DrivePoseClock (v2.42): 1 sim (BeamNG's physics clock), 0 frame (v2.41 and before)
		int         driveMotion = 1;       // DriveMotion: 1 filter (BeamNG's speed carries the car, its position corrects gently), 0 direct (v2.2)
		float       driveFilterMs = 80.0f; // DriveFilterMs: how fast the filter pulls the car onto BeamNG's position
		int         soundWake = 1;         // BeamNGSoundWake: 1 bounce (BeamNG's window in front for a moment), 3 session (v2.4), 0 off (v2.4: 2 message removed)
		int         soundWakeMs = 120;     // BeamNGSoundWakeMs: how long BeamNG's window stays in front
		int         driveRadio = 1;        // DriveRadio: 1 phone (GTA's radio with the hidden car's engine off), 2 engine (GTA's engine stays on), 0 off
		int         padActN = 0;           // PadGtaActions: pad control -> BeamNG action while that control is GTA's (v2.4: none by default)
		int         padActCtl[8] = {};
		std::string padActName[8];
		// v2.4 (T3: GTA's ground in BeamNG)
		bool        world = true;             // World: GTA's ground is built in BeamNG while driving (0 = v2.3: BeamNG's own map, anchored at F10)
		float       worldTileM = 8.0f;        // WorldTileM: tile size (metres)
		float       worldStepNear = 0.5f;     // WorldStepM: point spacing (finest)
		float       worldCoarseM = 2.0f;      // WorldCoarseM (v2.41): first pass every this many metres; only cells that aren't flat get the fine spacing
		float       worldRefineCm = 1.5f;     // WorldRefineCm: a cell this far off flat (cm) gets the next finer points (v2.56: 1.5; v2.52: 2, was 4)
		float       worldRefineSoftCm = 8.0f; // WorldRefineSoftCm (v2.42): the same for soft ground (grass, dirt, sand...)
		float       worldDraftS = 2.0f;       // WorldDraftS (v2.42): at speed, road the car reaches within this many seconds goes to BeamNG as a 2 m draft first
		float       worldPrebuildM = 96.0f;   // WorldPrebuildM (v2.42): the ring around the car measured with spare rays (0 = off)
		bool        worldMap = true;          // WorldMap (v2.42): finished tiles saved in GtaBeamD1_map and never measured again
		float       worldMapReadM = 256.0f;   // WorldMapReadM (v2.42): saved regions read this far ahead
		float       worldSyncMs = 1.0f;       // WorldSyncMs (v2.41): extra immediate rays per frame for road the car reaches soon (ms of GTA's frame; 0 = off)
		float       worldUrgentS = 1.5f;      // WorldUrgentS (v2.41): "soon" = within this many seconds
		int         worldCacheTiles = 6000;   // WorldCacheTiles (v2.41): measured tiles remembered (sent again later without rays; ~2.5 KB each)
		float       worldRadius = 24.0f;      // WorldRadiusM: ground kept all around the car
		float       worldAheadS = 2.5f;       // WorldAheadS: seconds of road ahead
		float       worldWidth = 32.0f;       // WorldWidthM: width of the road ahead
		int         worldRays = 96;           // WorldRaysPerFrame: new rays started per script tick
		int         worldInFlight = 384;      // WorldRaysInFlight: most rays waiting for GTA at once (lowered by itself when GTA refuses one)
		int         worldTilesWork = 16;      // WorldTilesAtOnce: tiles measured at the same time
		float       worldZLift = 200.0f;      // WorldZLift: GTA's ground this far above BeamNG's own map
		float       worldRebaseM = 2000.0f;   // WorldRebaseM: a new origin at F10 when the car is this far from the old one
		float       worldFallM = 3.0f;        // WorldFallCheckM: BeamNG's car this far under GTA's ground = it fell through
		float       worldFallWaitS = 2.5f;    // WorldFallWaitS (v2.62): ... for this long (s) before it is put back
		float       driveStartMaxKmh = 5.0f;  // DriveStartMaxKmh: F10 on only below this speed (World=1)
		bool        exitKeepSpeed = true;     // DriveExitKeepSpeed: F10 off hands BeamNG's speed to GTA's car
		float       quietVolume = 0.0f;       // BeamNGQuietVolume: BeamNG's volume while not driving in session mode (0..1 of its own)
		// v2.43
		float       worldLevelM = 3.5f;       // WorldLevelM: ground this far above/below the level the car is on is another level (bridges)
		float       worldDropM = 2.5f;        // WorldDropM: a step down this high within 1 m is an edge (a bridge deck): the ground below it is left out
		int         worldKeepTris = 30000;    // WorldKeepTris: built ground stays in BeamNG (a static world) up to this many triangles (0 = v2.42)
		float       worldKeepM = 400.0f;      // WorldKeepM: ... and within this many metres
		float       worldFillPerS = 4.0f;     // WorldFillPerS: standing still, measured ground nearby goes to BeamNG at this many tiles a second
		bool        pauseSync = true;         // PauseSync: pausing either game pauses the other
		bool        worldDebug = false;       // WorldDebug: every tile's level decision in the log (big)
		std::string repairActs;               // BeamNGRepairActions: these BeamNG actions repair the car where it is (comma list)
		// v2.5
		float       worldUpM = 4.0f;          // WorldUpM: ground rays start this far over the car's level (never higher: no overpasses)
		float       worldDownM = 12.0f;       // WorldDownM: ... and look this far under it
		bool        worldWalls = true;        // WorldWalls: GTA's walls, barriers and pillars as solid boxes in BeamNG
		float       worldWallProbeM = 0.6f;   // WorldWallProbeM: the wall scan's height over the ground (lower = kerbs count as walls)
		float       worldWallStepM = 1.0f;    // WorldWallStepM: its lines this far apart
		float       worldWallMs = 1.0f;       // WorldWallMs: time a frame for wall scans (the road right ahead is always scanned)
		bool        props = true;             // Props: GTA's props (lamp posts, signs, bins...) react to BeamNG's car
		float       propRangeM = 40.0f;       // PropRangeM: props this close to the car are watched
		float       propPush = 1.0f;          // PropPush: how hard a hit prop is pushed in GTA (x)
		float       propBump = 1.0f;          // PropBump: how much speed BeamNG's car loses on a soft prop (x)
		bool        propSolid = true;         // PropSolid: big props (and the ones GtaBeamD1_props.json calls solid) are boxes in BeamNG
		bool        dents = true;             // Dents: BeamNG's dents and bent parts shown on the drawn car
		float       dentHz = 20.0f;           // DentsPerSecond: how often BeamNG sends its car's shape
		float       driveGravity = 1.1f;      // DriveGravity: BeamNG's gravity x this while GTA drives (1 = BeamNG's own; v2.62: 1.1 again, v2.51: 1.0)
		float       propBreak = 1.0f;         // PropBreak: how hard BeamNG's car must hit a breakaway prop before it breaks (x)
		bool        worldLanding = true;      // WorldLanding: v2.51: jumps: GTA's ground built where the car will land
		bool        propPhysics = true;       // PropPhysics: v2.52: props as weighted boxes BeamNG simulates (GTA's prop copies them)
		int         propPhysicsMax = 10;      // PropPhysicsMax: v2.52: that many at once (each one may hitch BeamNG for a moment when it comes)
		bool        trafficCollision = true;  // TrafficCollision: v2.7: GTA's cars and people and BeamNG's car push each other
		int         trafficTwins = 4;         // TrafficTwins: v2.71: BeamNG cars that take GTA's cars' place for a crash (0: v2.7's push only)
		bool        worldDeep = true;         // WorldDeep: v2.71: no ground in a point's window: looked for below it, then above it
		bool        worldFill = true;
		float       doorOpenDeg = 65.0f;      // DoorOpenDeg: v2.73: the drawn front door's angle when GTA's is fully open
		bool        tireSmoke = true;         // TireSmoke: v2.73: GTA's tyre smoke at BeamNG's wheels that spin or slide
		char        tireFx[2][48] = { "core", "wheel_burnout" };  // TireSmokeFx: v2.73: asset, effect
		float       tireSmokeMs = 3.0f;       // TireSmokeSlipMs: v2.73: how fast a tyre must slip (m/s) to smoke         // WorldFill: v2.73: holes with ground on two sides filled in between (Tarmac / ASPHALT)
		bool        weapons = true;           // Weapons: v2.6: GTA's bullets and explosions hit BeamNG's car; fire; the driver gets hurt
		int         carExplosion = 0;         // CarExplosion: v2.6: how the burning car blows up: 0 gta (GTA's explosion pushes BeamNG's car), 1 beamng (+ BeamNG's own)
	} g_cfg;

	const char* kSec = "D1";

	float IniFloat(const char* path, const char* name, float def)
	{
		char v[64];
		GetPrivateProfileStringA(kSec, name, "", v, sizeof(v), path);
		return v[0] ? float(atof(v)) : def;
	}

	int IniChoice(const char* path, const char* name, const char* a, const char* b)
	{
		char v[32];
		GetPrivateProfileStringA(kSec, name, "auto", v, sizeof(v), path);
		return _stricmp(v, a) == 0 ? 0 : _stricmp(v, b) == 0 ? 1 : -1;
	}

	const char* DrivenName(int d) { return d == 0 ? "rear" : d == 1 ? "front" : d == 2 ? "all" : "auto"; }
	const char* ShadowName(int s) { return s == 1 ? "own" : s == 2 ? "off" : "GTA's"; }
	const char* WheelSrcName(int s) { return s == 0 ? "speed" : s == 1 ? "gta" : "auto"; }
	const char* HideName(int h) { return h == 0 ? "transparent" : h == 1 ? "invisible" : "ghost"; }
	// how the GTA car is hidden: 0 transparent (GTA's shadow stays), 1 invisible, 2 ghost (transparent, no shadow)
	bool g_driveHide = false;  // v2.0: drive mode hides the GTA car completely (no shadow halo; its wheels aren't needed)
	int EffectiveHide()
	{
		if (g_driveHide) return 1;
		if (g_cfg.hideMode >= 0) return g_cfg.hideMode;
		return g_cfg.shadowMode == 0 ? 0 : 2;
	}

	// v2.01: pad controls (XInput layout; PlayStation names accepted in the ini)
	enum PadCtl
	{
		kPA, kPB, kPX, kPY, kPLB, kPRB, kPBack, kPStart, kPLS, kPRS, kPDU, kPDD, kPDL, kPDR, kPLT, kPRT, kPLStick, kPRStick, kPadCtls
	};
	const char* kPadName[kPadCtls] = { "A", "B", "X", "Y", "LB", "RB", "BACK", "START", "LS", "RS", "DPAD_UP", "DPAD_DOWN", "DPAD_LEFT", "DPAD_RIGHT", "LT", "RT", "LSTICK", "RSTICK" };
	const WORD  kPadBit[14] = { 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0x0020, 0x0010, 0x0040, 0x0080, 0x0001, 0x0002, 0x0004, 0x0008 };
	// BeamNG's (Torque3D's) names for the same controls: the BeamNG mod lists the ones your bindings use
	const char* kPadBeam[kPadCtls] = { "btn_a", "btn_b", "btn_x", "btn_y", "btn_l", "btn_r", "btn_back", "btn_start", "btn_lt", "btn_rt", "dpadu", "dpadd", "dpadl", "dpadr", "triggerl", "triggerr", "thumblx", "thumbrx" };

	int PadCtlByName(const char* s0)
	{
		char s[32] = {};
		int  n = 0;
		for (const char* p = s0; *p && n < 31; ++p)
			if (*p != ' ' && *p != '_' && *p != '-') s[n++] = char(toupper((unsigned char)*p));
		static const struct
		{
			const char* n;
			int         c;
		} names[] = { { "A", kPA }, { "B", kPB }, { "X", kPX }, { "Y", kPY }, { "LB", kPLB }, { "RB", kPRB }, { "BACK", kPBack }, { "START", kPStart }, { "LS", kPLS }, { "RS", kPRS },
			          { "DPADUP", kPDU }, { "DPADDOWN", kPDD }, { "DPADLEFT", kPDL }, { "DPADRIGHT", kPDR }, { "LT", kPLT }, { "RT", kPRT }, { "LSTICK", kPLStick }, { "RSTICK", kPRStick },
			          // PlayStation names
			          { "CROSS", kPA }, { "CIRCLE", kPB }, { "SQUARE", kPX }, { "TRIANGLE", kPY }, { "L1", kPLB }, { "R1", kPRB }, { "L2", kPLT }, { "R2", kPRT }, { "L3", kPLS }, { "R3", kPRS },
			          { "SHARE", kPBack }, { "CREATE", kPBack }, { "VIEW", kPBack }, { "OPTIONS", kPStart }, { "MENU", kPStart }, { "UP", kPDU }, { "DOWN", kPDD }, { "LEFT", kPDL }, { "RIGHT", kPDR },
			          { "LEFTSTICK", kPLStick }, { "RIGHTSTICK", kPRStick } };
		for (const auto& e : names)
			if (!strcmp(s, e.n)) return e.c;
		return -1;
	}

	// "Y, START, BACK" -> bits; unknown names logged
	uint32_t PadList(const char* path, const char* key, const char* def)
	{
		char v[512];
		GetPrivateProfileStringA(kSec, key, def, v, sizeof(v), path);
		uint32_t bits = 0;
		for (char* t = strtok(v, ",;"); t; t = strtok(nullptr, ",;")) {
			while (*t == ' ') ++t;
			if (!*t || !_stricmp(t, "none")) continue;
			const int c = PadCtlByName(t);
			if (c >= 0) bits |= 1u << c;
			else Log("settings: %s: pad control '%s' not known (use A B X Y LB RB LT RT LS RS BACK START DPAD_UP/DOWN/LEFT/RIGHT LSTICK RSTICK, or the PlayStation names)", key, t);
		}
		return bits;
	}

	// a comma list of plain names (BeamNG keys / actions): lower case, no quotes, spaced or comma-joined
	std::string NameList(const char* path, const char* key, const char* def, bool spaced)
	{
		char v[2048];
		GetPrivateProfileStringA(kSec, key, def, v, sizeof(v), path);
		std::string out = spaced ? " " : "";
		for (char* t = strtok(v, ", ;"); t; t = strtok(nullptr, ", ;")) {
			std::string w;
			for (char* p = t; *p; ++p)
				if (isalnum((unsigned char)*p) || *p == '_' || *p == '.') w += spaced ? char(tolower((unsigned char)*p)) : *p;
			if (w.empty()) continue;
			if (spaced) out += w + " ";
			else out += (out.empty() ? "" : ",") + w;
		}
		return out;
	}

	void LoadConfig()
	{
		char path[MAX_PATH + 8];
		PathNextToModule(".ini", path, sizeof(path));
		Config& c = g_cfg;
		c.frameDelay = std::clamp(int(GetPrivateProfileIntA(kSec, "FrameDelay", 0, path)), 0, 3);
		c.fovHorizontal = GetPrivateProfileIntA(kSec, "FovIsHorizontal", 0, path) != 0;
		c.rollSign = IniFloat(path, "RollSign", 1.0f) < 0.0f ? -1.0f : 1.0f;
		c.tolRel = IniFloat(path, "DepthTolerance", 0.002f);
		c.tolAbs = IniFloat(path, "DepthToleranceMeters", 0.05f);
		c.nearScale = IniFloat(path, "NearScale", 1.0f);
		char src[32];
		GetPrivateProfileStringA(kSec, "DepthSource", "auto", src, sizeof(src), path);
		c.depthSource = _stricmp(src, "live") == 0 ? 1 : _stricmp(src, "preclear") == 0 ? 2 : 0;
		char rev[16];
		GetPrivateProfileStringA(kSec, "DepthReversed", "auto", rev, sizeof(rev), path);
		c.depthReversed = rev[0] == '1' ? 1 : rev[0] == '0' ? 0 : -1;
		char key[32];
		GetPrivateProfileStringA(kSec, "Key", "", key, sizeof(key), path);
		if (key[0]) {
			const long k = strtol(key, nullptr, 0);
			if (k > 0 && k < 256) {
				c.key = int(k);
			}
		}
		GetPrivateProfileStringA(kSec, "CarFile", "", c.carFile, sizeof(c.carFile), path);
		c.maxTex = std::clamp(int(GetPrivateProfileIntA(kSec, "MaxTextureSize", 2048, path)), 16, 8192);
		c.texCompression = GetPrivateProfileIntA(kSec, "TextureCompression", 1, path) != 0;
		c.maxDataTex = std::clamp(int(GetPrivateProfileIntA(kSec, "MaxDataTextureSize", 1024, path)), 16, 8192);
		c.meshAxes = IniChoice(path, "MeshAxes", "gltf", "beamng");
		c.meshOrient = IniChoice(path, "MeshOrientation", "direction", "vehicle");
		c.offX = IniFloat(path, "OffsetRight", 0.0f);
		c.offY = IniFloat(path, "OffsetForward", 0.0f);
		c.offZ = IniFloat(path, "OffsetUp", 0.0f);
		c.yawDeg = IniFloat(path, "TurnDegrees", 0.0f);
		c.paintGamma = IniFloat(path, "PaintGamma", 2.2f);
		c.exposure = IniFloat(path, "Brightness", 1.0f);
		c.ambient = std::clamp(IniFloat(path, "Ambient", 0.35f), 0.0f, 3.0f);
		c.specStrength = std::clamp(IniFloat(path, "SpecularStrength", 1.0f), 0.0f, 4.0f);
		c.sunStrength = std::clamp(IniFloat(path, "SunStrength", 1.0f), 0.0f, 4.0f);
		c.alphaCut = IniFloat(path, "AlphaCutout", 0.0f);
		c.glassMinAlpha = IniFloat(path, "GlassMinAlpha", 0.0f);
		c.glassReflections = std::clamp(IniFloat(path, "GlassReflections", 1.0f), 0.0f, 3.0f);
		c.maskCheck = GetPrivateProfileIntA(kSec, "LayerMaskCheck", 1, path) != 0;
		c.exportTimeout = std::clamp(int(GetPrivateProfileIntA(kSec, "ExportTimeout", 300, path)), 10, 900);
		c.port = int(GetPrivateProfileIntA(kSec, "BeamNGPort", 47011, path));
		c.autoLoad = GetPrivateProfileIntA(kSec, "LoadAtStart", 1, path) != 0;
		char hm[32];
		GetPrivateProfileStringA(kSec, "HideMode", "auto", hm, sizeof(hm), path);
		c.hideMode = _stricmp(hm, "invisible") == 0 ? 1 : _stricmp(hm, "transparent") == 0 ? 0 : _stricmp(hm, "ghost") == 0 ? 2 : -1;
		c.showDriver = GetPrivateProfileIntA(kSec, "ShowDriver", 1, path) != 0;
		c.driverSeat = GetPrivateProfileIntA(kSec, "DriverSeat", 1, path) != 0;
		c.seatAdj[0] = IniFloat(path, "DriverSeatRightM", 0.0f);
		c.seatAdj[1] = IniFloat(path, "DriverSeatForwardM", 0.0f);
		c.seatAdj[2] = IniFloat(path, "DriverSeatUpM", 0.0f);
		char dsg[32];
		GetPrivateProfileStringA(kSec, "DrawStage", "auto", dsg, sizeof(dsg), path);
		c.drawStage = _stricmp(dsg, "present") == 0 ? 0 : 1;
		{
			char cs[32];
			GetPrivateProfileStringA(kSec, "CameraSource", "auto", cs, sizeof(cs), path);
			c.cameraSource = _stricmp(cs, "final") == 0 ? 0 : _stricmp(cs, "gameplay") == 0 ? 1 : -1;
		}
		c.camSlipFix = GetPrivateProfileIntA(kSec, "CameraSlipFix", 1, path) != 0;
		c.framePairing = GetPrivateProfileIntA(kSec, "FramePairing", 1, path) != 0;
		c.oneSidedGlass = GetPrivateProfileIntA(kSec, "OneSidedGlass", 1, path) != 0;
		c.rideCheck = GetPrivateProfileIntA(kSec, "RideHeightCheck", 1, path) != 0;
		{
			char cp[32];
			GetPrivateProfileStringA(kSec, "CarCameraPairing", "auto", cp, sizeof(cp), path);
			c.carCamPairing = _stricmp(cp, "same") == 0 ? 0 : _stricmp(cp, "previous") == 0 ? 1 : -1;
			char hs[32];
			GetPrivateProfileStringA(kSec, "HdrSunToSky", "auto", hs, sizeof(hs), path);
			c.hdrSunAuto = _stricmp(hs, "auto") == 0;
			GetPrivateProfileStringA(kSec, "HdrAmbient", "auto", hs, sizeof(hs), path);
			c.hdrAmbAuto = _stricmp(hs, "auto") == 0;
		}
		{
			// v1.9.6: "auto" (the default) = the paint's own values from the BeamNG export; a number = that value
			char gv[32];
			GetPrivateProfileStringA(kSec, "GtaReflections", "auto", gv, sizeof(gv), path);
			c.gtaRefl = !(gv[0] == '0' || _stricmp(gv, "off") == 0 || _stricmp(gv, "false") == 0 || _stricmp(gv, "no") == 0);
			// v2.0: default 1.0 (GTA's map as it is). v1.9.7's auto brightness swung between x0.6 and x2.6 and made
			// the paint too shiny; "auto" still measures it (log), a number fixes it.
			GetPrivateProfileStringA(kSec, "GtaReflectionScale", "1.0", gv, sizeof(gv), path);
			c.gtaReflScale = _stricmp(gv, "auto") == 0 ? -1.0f : (gv[0] && atof(gv) > 0.0) ? std::clamp(float(atof(gv)), 0.05f, 10.0f) : 1.0f;
			const char* lk[2] = { "GtaReflectionLayoutUp", "GtaReflectionLayoutDown" };
			for (int k = 0; k < 2; ++k) {
				GetPrivateProfileStringA(kSec, lk[k], "auto", gv, sizeof(gv), path);
				c.gtaReflLayout[k] = (gv[0] >= '0' && gv[0] <= '9') ? std::clamp(atoi(gv), 0, 31) : -1;
			}
			const char* keys[4] = { "PaintMetallic", "PaintRoughness", "PaintClearCoat", "PaintClearCoatRoughness" };
			for (int k = 0; k < 4; ++k) {
				char pv[32];
				GetPrivateProfileStringA(kSec, keys[k], "auto", pv, sizeof(pv), path);
				c.paintSurf[k] = (pv[0] && _stricmp(pv, "auto") != 0) ? std::clamp(float(atof(pv)), 0.0f, 1.0f) : -1.0f;
			}
		}
		{
			// v1.9.6: "auto" (the default) = from the light measurement; a number = fixed (also the fallback)
			char hv[32];
			GetPrivateProfileStringA(kSec, "HdrSunToSky", "", hv, sizeof(hv), path);
			c.hdrSunToSky = (hv[0] && _stricmp(hv, "auto") != 0) ? std::clamp(float(atof(hv)), 0.0f, 50.0f) : 6.0f;
			GetPrivateProfileStringA(kSec, "HdrAmbient", "", hv, sizeof(hv), path);
			c.hdrAmbient = (hv[0] && _stricmp(hv, "auto") != 0) ? std::clamp(float(atof(hv)), 0.0f, 5.0f) : 1.0f;
		}
		{
			char ht[16];
			GetPrivateProfileStringA(kSec, "HdrTarget", "auto", ht, sizeof(ht), path);
			c.hdrTarget = (ht[0] == '1' || ht[0] == '2') ? ht[0] - '0' : 0;
		}
		char sm[32];
		GetPrivateProfileStringA(kSec, "ShadowMode", "own", sm, sizeof(sm), path);
		c.shadowMode = _stricmp(sm, "gta") == 0 ? 0 : _stricmp(sm, "off") == 0 ? 2 : 1;
		c.contactStrength = std::clamp(IniFloat(path, "ContactShadow", 0.6f), 0.0f, 1.0f);
		c.sunShadowStrength = std::clamp(IniFloat(path, "SunShadowStrength", 0.7f), 0.0f, 1.0f);
		c.shadowSoftness = std::clamp(IniFloat(path, "ShadowSoftness", 1.0f), 0.0f, 4.0f);
		c.sunAzimuth = IniFloat(path, "SunAzimuthOffset", 0.0f);
		c.sunElevation = std::clamp(IniFloat(path, "SunElevationScale", 1.0f), 0.2f, 2.0f);
		c.selfShadows = GetPrivateProfileIntA(kSec, "SelfShadows", 1, path) != 0;
		c.selfShadowSize = std::clamp(int(GetPrivateProfileIntA(kSec, "SelfShadowSize", 1024, path)), 256, 4096);
		c.ao = GetPrivateProfileIntA(kSec, "AmbientOcclusion", 1, path) != 0;
		c.aoStrength = std::clamp(IniFloat(path, "AOStrength", 1.0f), 0.0f, 1.0f);
		c.worldShadows = GetPrivateProfileIntA(kSec, "WorldShadows", 1, path) != 0;
		c.reflections = std::clamp(IniFloat(path, "Reflections", 1.0f), 0.0f, 3.0f);
		char env[32];
		GetPrivateProfileStringA(kSec, "Environment", "world", env, sizeof(env), path);
		c.environment = _stricmp(env, "sky") == 0 ? 0 : 1;  // "screen" (v1.9) counts as world
		c.envBrightness = std::clamp(IniFloat(path, "EnvBrightness", 1.0f), 0.0f, 4.0f);
		c.normalMaps = GetPrivateProfileIntA(kSec, "NormalMaps", 1, path) != 0;
		c.normalStrength = std::clamp(IniFloat(path, "NormalStrength", 1.0f), 0.0f, 3.0f);
		char ng[32];
		GetPrivateProfileStringA(kSec, "NormalMapStyle", "opengl", ng, sizeof(ng), path);
		c.normalGreen = _stricmp(ng, "directx") == 0 ? 1.0f : -1.0f;
		c.surfaceMaps = GetPrivateProfileIntA(kSec, "SurfaceMaps", 1, path) != 0;
		c.edgeSmoothing = GetPrivateProfileIntA(kSec, "EdgeSmoothing", 1, path) != 0;
		c.hudMask = GetPrivateProfileIntA(kSec, "KeepMinimapOnTop", 1, path) != 0;
		c.hudMargin = std::clamp(IniFloat(path, "MinimapMargin", 6.0f), 0.0f, 100.0f);
		c.debugView = std::clamp(int(GetPrivateProfileIntA(kSec, "DebugView", 0, path)), 0, 11);
		c.wheels = GetPrivateProfileIntA(kSec, "Wheels", 1, path) != 0;
		char ws[32];
		GetPrivateProfileStringA(kSec, "WheelSource", "auto", ws, sizeof(ws), path);
		c.wheelSource = _stricmp(ws, "speed") == 0 ? 0 : _stricmp(ws, "gta") == 0 ? 1 : -1;
		c.suspension = GetPrivateProfileIntA(kSec, "SuspensionTravel", 1, path) != 0;
		c.maxSteer = IniFloat(path, "MaxSteerDegrees", 32.0f);
		c.steerSign = IniFloat(path, "SteerSign", 1.0f) < 0 ? -1.0f : 1.0f;
		char dw[32];
		GetPrivateProfileStringA(kSec, "DrivenWheels", "auto", dw, sizeof(dw), path);
		c.drivenWheels = _stricmp(dw, "rear") == 0 ? 0 : _stricmp(dw, "front") == 0 ? 1 : _stricmp(dw, "all") == 0 ? 2 : -1;
		c.burnoutSpeed = std::clamp(IniFloat(path, "BurnoutWheelSpeed", 22.0f), 0.0f, 120.0f);
		c.handbrakeLocks = GetPrivateProfileIntA(kSec, "HandbrakeLocksRear", 1, path) != 0;
		{
			// v2.0 drive mode
			char dv[32];
			GetPrivateProfileStringA(kSec, "DriveKey", "", dv, sizeof(dv), path);
			if (dv[0]) {
				const long k = strtol(dv, nullptr, 0);
				if (k > 0 && k < 256) c.driveKey = int(k);
			}
			GetPrivateProfileStringA(kSec, "DriveSteerSign", "auto", dv, sizeof(dv), path);
			c.driveSteerSign = (_stricmp(dv, "auto") == 0 || !dv[0] || atof(dv) == 0.0) ? 0.0f : atof(dv) < 0 ? -1.0f : 1.0f;
			GetPrivateProfileStringA(kSec, "DriveInputFilter", "auto", dv, sizeof(dv), path);
			c.driveFilter = (_stricmp(dv, "key") == 0 || _stricmp(dv, "keyboard") == 0 || !strcmp(dv, "0")) ? 0
			              : (_stricmp(dv, "pad") == 0 || _stricmp(dv, "gamepad") == 0 || !strcmp(dv, "1")) ? 1
			              : (_stricmp(dv, "direct") == 0 || !strcmp(dv, "2")) ? 2 : -1;
			GetPrivateProfileStringA(kSec, "DriveSmoothingMs", "auto", dv, sizeof(dv), path);
			c.driveDelayMs = (_stricmp(dv, "auto") == 0 || !dv[0]) ? -1.0f : std::clamp(float(atof(dv)), 0.0f, 200.0f);
			c.driveSetVelocity = GetPrivateProfileIntA(kSec, "DriveSetVelocity", 1, path) != 0;
			// v2.2
			c.driveSlipFix = GetPrivateProfileIntA(kSec, "DriveCameraSlipFix", 1, path) != 0;  // v2.4: on by default (your v2.3 run B)
			GetPrivateProfileStringA(kSec, "DriveDrawPose", "readback", dv, sizeof(dv), path);
			c.driveDrawPose = (!_stricmp(dv, "set") || !strcmp(dv, "1")) ? 1 : 0;
			GetPrivateProfileStringA(kSec, "BeamNGBackgroundFps", "off", dv, sizeof(dv), path);
			c.beamBgFps = (!dv[0] || !_stricmp(dv, "keep")) ? -1 : (!_stricmp(dv, "off") || !_stricmp(dv, "none")) ? 0 : std::clamp(atoi(dv), 0, 500);
			Log("settings: v2.2 drive: velocity set %d, camera slip fix in drive mode %d, drawn pose %s, BeamNG background fps %s (%d)", c.driveSetVelocity, c.driveSlipFix,
				c.driveDrawPose ? "set" : "readback", c.beamBgFps < 0 ? "keep" : c.beamBgFps == 0 ? "no limit" : "", c.beamBgFps > 0 ? c.beamBgFps : 0);
			// v2.01: your BeamNG controls
			GetPrivateProfileStringA(kSec, "DriveInput", "beamng", dv, sizeof(dv), path);
			c.driveInput = (_stricmp(dv, "gta") == 0 || !strcmp(dv, "0")) ? 0 : 1;
			GetPrivateProfileStringA(kSec, "PadToggle", "DPAD_DOWN", dv, sizeof(dv), path);
			c.padToggle = (!dv[0] || !_stricmp(dv, "none")) ? -1 : PadCtlByName(dv);
			if (dv[0] && _stricmp(dv, "none") && (c.padToggle < 0 || c.padToggle >= 14)) {
				Log("settings: PadToggle '%s' %s: D-pad down", dv, c.padToggle < 0 ? "not known" : "must be a button (not a trigger or stick)");
				c.padToggle = kPDD;
			}
			c.padAlwaysGta = PadList(path, "PadAlwaysGta", "Y, RSTICK");  // v2.4: START/BACK/RS follow the mode (your spec)
			c.padAlwaysBeam = PadList(path, "PadAlwaysBeamNG", "LSTICK, LT, RT");
			c.padAlwaysGta &= ~c.padAlwaysBeam;
			c.padStartGta = GetPrivateProfileIntA(kSec, "PadStartsInGtaMode", 0, path) != 0;
			GetPrivateProfileStringA(kSec, "PadIndex", "auto", dv, sizeof(dv), path);
			c.padIndex = (dv[0] >= '0' && dv[0] <= '3' && !dv[1]) ? dv[0] - '0' : -1;
			c.keysAlwaysGta = NameList(path, "KeysAlwaysGta", "f, escape, p, tab", true);
			c.beamCats = NameList(path, "BeamNGActionCategories", "vehicle", false);
			c.beamNever = NameList(path, "BeamNGActionsNever",
				"reset_physics, reset_all_physics, recover_vehicle, recover_vehicle_alt, recover_to_last_road, loadHome, saveHome, dropPlayerAtCamera, "
				"dropPlayerAtCameraNoReset, toggleWalkingMode, switch_next_vehicle, switch_previous_vehicle, vehicle_selector, parts_selector, reload_vehicle, reload_all_vehicles",
				false);
			c.beamAlso = NameList(path, "BeamNGActionsAlso", "", false);
			// v2.43: your repair binding (RB+LB+X) repairs the car where it is (these actions would put BeamNG's car somewhere else)
			c.repairActs = NameList(path, "BeamNGRepairActions", "reset_physics, reset_all_physics, recover_vehicle, recover_vehicle_alt, recover_to_last_road, loadHome, reload_vehicle, reload_all_vehicles", false);
			c.pauseSync = GetPrivateProfileIntA(kSec, "PauseSync", 1, path) != 0;
			c.driveHud = GetPrivateProfileIntA(kSec, "DriveCheckText", 0, path) != 0;
			GetPrivateProfileStringA(kSec, "BeamNGPadBindings", "auto", dv, sizeof(dv), path);
			c.padList = !_stricmp(dv, "xinput") || !_stricmp(dv, "xbox") ? "xinput" : (!_stricmp(dv, "playstation") || !_stricmp(dv, "ps")) ? "playstation" : "auto";
			c.gauge = GetPrivateProfileIntA(kSec, "DriveGauge", 1, path) != 0;
			GetPrivateProfileStringA(kSec, "DriveGaugeUnits", "mph", dv, sizeof(dv), path);
			c.gaugeKmh = !_stricmp(dv, "kmh") || !_stricmp(dv, "km/h") || !_stricmp(dv, "kph");
			c.beamSound = GetPrivateProfileIntA(kSec, "BeamNGSound", 1, path) != 0;
			c.gtaEngineSound = GetPrivateProfileIntA(kSec, "DriveGtaEngineSound", 0, path) != 0;
			c.parkBlock = GetPrivateProfileIntA(kSec, "ParkBlocksBeamNG", 1, path) != 0;
			// v2.3
			GetPrivateProfileStringA(kSec, "DrivePoseClock", "sim", dv, sizeof(dv), path);
			c.drivePoseClock = (!_stricmp(dv, "frame") || !strcmp(dv, "0")) ? 0 : 1;
			c.drivePoseLog = GetPrivateProfileIntA(kSec, "DrivePoseLog", 1, path) != 0;
			GetPrivateProfileStringA(kSec, "DriveMotion", "filter", dv, sizeof(dv), path);
			c.driveMotion = (!_stricmp(dv, "direct") || !strcmp(dv, "0")) ? 0 : 1;
			c.driveFilterMs = std::clamp(IniFloat(path, "DriveFilterMs", 80.0f), 10.0f, 500.0f);
			GetPrivateProfileStringA(kSec, "BeamNGSoundWake", "bounce", dv, sizeof(dv), path);
			c.soundWake = (!_stricmp(dv, "off") || !strcmp(dv, "0")) ? 0 : !_stricmp(dv, "session") ? 3 : 1;
			if (!_stricmp(dv, "message")) Log("settings: BeamNGSoundWake=message was removed in v2.4 (it failed 8 of 8 in your test): using bounce");
			c.quietVolume = std::clamp(IniFloat(path, "BeamNGQuietVolume", 0.0f), 0.0f, 1.0f);
			c.soundWakeMs = std::clamp(int(GetPrivateProfileIntA(kSec, "BeamNGSoundWakeMs", 120, path)), 30, 1000);
			GetPrivateProfileStringA(kSec, "DriveRadio", "phone", dv, sizeof(dv), path);
			c.driveRadio = (!_stricmp(dv, "off") || !strcmp(dv, "0")) ? 0 : !_stricmp(dv, "engine") ? 2 : 1;
			if (c.gtaEngineSound) c.driveRadio = c.driveRadio ? 2 : 0;  // GTA's engine stays on: its own radio works
			GetPrivateProfileStringA(kSec, "DriveRadioButton", "", dv, sizeof(dv), path);
			if (dv[0]) Log("settings: DriveRadioButton was removed in v2.4: GTA's own radio controls work in GTA mode (D-pad left hold = radio wheel)");
			{
				char pa[512];
				GetPrivateProfileStringA(kSec, "PadGtaActions", "", pa, sizeof(pa), path);
				c.padActN = 0;
				for (char* tok = strtok(pa, ","); tok && c.padActN < 8; tok = strtok(nullptr, ",")) {
					char* colon = strchr(tok, ':');
					if (!colon) continue;
					*colon = 0;
					auto trim = [](char* t) {
						while (*t == ' ' || *t == '\t') ++t;
						char* e = t + strlen(t);
						while (e > t && (e[-1] == ' ' || e[-1] == '\t' || e[-1] == '\r' || e[-1] == '\n')) *--e = 0;
						return t;
					};
					const char* cn = trim(tok);
					const char* an = trim(colon + 1);
					const int   ctl = PadCtlByName(cn);
					if (ctl < 0 || ctl >= 14 || !*an) {
						Log("settings: PadGtaActions entry '%s: %s' not understood (pad button name : BeamNG action name)", cn, an);
						continue;
					}
					c.padActCtl[c.padActN] = ctl;
					c.padActName[c.padActN] = an;
					++c.padActN;
				}
			}
			{
				std::string acts;
				for (int i = 0; i < c.padActN; ++i) acts += std::string(i ? ", " : "") + kPadName[c.padActCtl[i]] + " -> " + c.padActName[i];
				Log("settings: v2.42 drive: poses timed by BeamNG's %s clock", c.drivePoseClock ? "simulation" : "frame");
				Log("settings: v2.3/2.4 drive: motion %s (%.0f ms), BeamNG sound %s (%d ms, quiet volume %.2f), radio %s, pad buttons for BeamNG actions while GTA's: %s",
					c.driveMotion ? "filter" : "direct", c.driveFilterMs, c.soundWake == 1 ? "bounce" : c.soundWake == 3 ? "session" : "off", c.soundWakeMs, c.quietVolume,
					c.driveRadio == 1 ? "phone" : c.driveRadio == 2 ? "engine" : "off", acts.empty() ? "none" : acts.c_str());
			}
			// v2.4: GTA's ground in BeamNG (T3)
			c.world = GetPrivateProfileIntA(kSec, "World", 1, path) != 0;
			c.worldTileM = std::clamp(IniFloat(path, "WorldTileM", 8.0f), 4.0f, 32.0f);
			c.worldStepNear = std::clamp(IniFloat(path, "WorldStepM", IniFloat(path, "WorldStepNearM", 0.5f)), 0.25f, 4.0f);
			// a tile must hold a whole number of steps (at most 64 per side: one message)
			auto fitStep = [&](float st) {
				int n = std::max(1, int(c.worldTileM / st + 0.5f));
				n = std::min(n, 24);  // at most 25 x 25 points: one message stays well under 8 KB (BeamNG's UDP reader may cut at 8 KB)
				return c.worldTileM / float(n);
			};
			c.worldStepNear = fitStep(c.worldStepNear);
			c.worldCoarseM = std::clamp(IniFloat(path, "WorldCoarseM", 2.0f), 0.0f, 8.0f);
			c.worldRefineCm = std::clamp(IniFloat(path, "WorldRefineCm", 1.5f), 0.5f, 100.0f);
			c.worldRefineSoftCm = std::clamp(IniFloat(path, "WorldRefineSoftCm", 8.0f), 0.5f, 100.0f);
			c.worldDraftS = std::clamp(IniFloat(path, "WorldDraftS", 2.0f), 0.0f, 10.0f);
			c.worldPrebuildM = std::clamp(IniFloat(path, "WorldPrebuildM", 96.0f), 0.0f, 400.0f);
			c.worldMap = GetPrivateProfileIntA(kSec, "WorldMap", 1, path) != 0;
			c.worldMapReadM = std::clamp(IniFloat(path, "WorldMapReadM", 256.0f), 64.0f, 2000.0f);
			c.worldSyncMs = std::clamp(IniFloat(path, "WorldSyncMs", 1.0f), 0.0f, 8.0f);
			c.worldUrgentS = std::clamp(IniFloat(path, "WorldUrgentS", 1.5f), 0.0f, 10.0f);
			c.worldCacheTiles = std::clamp(int(GetPrivateProfileIntA(kSec, "WorldCacheTiles", 6000, path)), 0, 100000);
			c.worldRadius = std::clamp(IniFloat(path, "WorldRadiusM", 24.0f), 8.0f, 200.0f);
			c.worldAheadS = std::clamp(IniFloat(path, "WorldAheadS", 2.5f), 0.5f, 10.0f);
			c.worldWidth = std::clamp(IniFloat(path, "WorldWidthM", 32.0f), 8.0f, 200.0f);
			c.worldRays = std::clamp(int(GetPrivateProfileIntA(kSec, "WorldRaysPerFrame", 96, path)), 4, 2000);
			c.worldInFlight = std::clamp(int(GetPrivateProfileIntA(kSec, "WorldRaysInFlight", 384, path)), 16, 4000);
			c.worldTilesWork = std::clamp(int(GetPrivateProfileIntA(kSec, "WorldTilesAtOnce", 16, path)), 1, 64);
			c.worldZLift = std::clamp(IniFloat(path, "WorldZLift", 200.0f), 0.0f, 5000.0f);
			c.worldRebaseM = std::clamp(IniFloat(path, "WorldRebaseM", 2000.0f), 200.0f, 20000.0f);
			c.worldFallM = std::clamp(IniFloat(path, "WorldFallCheckM", 3.0f), 0.5f, 50.0f);
			c.worldFallWaitS = std::clamp(IniFloat(path, "WorldFallWaitS", 2.5f), 0.0f, 30.0f);
			c.driveStartMaxKmh = std::clamp(IniFloat(path, "DriveStartMaxKmh", 5.0f), 0.0f, 400.0f);
			c.exitKeepSpeed = GetPrivateProfileIntA(kSec, "DriveExitKeepSpeed", 1, path) != 0;
			// v2.43
			c.worldLevelM = std::clamp(IniFloat(path, "WorldLevelM", 3.5f), 1.0f, 20.0f);
			c.worldDropM = std::clamp(IniFloat(path, "WorldDropM", 2.5f), 0.0f, 20.0f);
			c.worldKeepTris = std::clamp(int(GetPrivateProfileIntA(kSec, "WorldKeepTris", 30000, path)), 0, 400000);
			c.worldKeepM = std::clamp(IniFloat(path, "WorldKeepM", 400.0f), 50.0f, 5000.0f);
			c.worldFillPerS = std::clamp(IniFloat(path, "WorldFillPerS", 4.0f), 0.0f, 60.0f);
			c.worldDebug = GetPrivateProfileIntA(kSec, "WorldDebug", 0, path) != 0;
			// v2.5
			c.worldUpM = std::clamp(IniFloat(path, "WorldUpM", 4.0f), 1.0f, 20.0f);
			c.worldDownM = std::clamp(IniFloat(path, "WorldDownM", 12.0f), 2.0f, 60.0f);
			c.worldWalls = GetPrivateProfileIntA(kSec, "WorldWalls", 1, path) != 0;
			c.worldWallProbeM = std::clamp(IniFloat(path, "WorldWallProbeM", 0.6f), 0.2f, 2.5f);
			c.worldWallStepM = std::clamp(IniFloat(path, "WorldWallStepM", 1.0f), 0.25f, 4.0f);
			c.worldWallMs = std::clamp(IniFloat(path, "WorldWallMs", 1.0f), 0.0f, 10.0f);
			c.props = GetPrivateProfileIntA(kSec, "Props", 1, path) != 0;
			c.propRangeM = std::clamp(IniFloat(path, "PropRangeM", 40.0f), 10.0f, 150.0f);
			c.propPush = std::clamp(IniFloat(path, "PropPush", 1.0f), 0.0f, 10.0f);
			c.propBump = std::clamp(IniFloat(path, "PropBump", 1.0f), 0.0f, 10.0f);
			c.propSolid = GetPrivateProfileIntA(kSec, "PropSolid", 1, path) != 0;
			c.dents = GetPrivateProfileIntA(kSec, "Dents", 1, path) != 0;
			c.dentHz = std::clamp(IniFloat(path, "DentsPerSecond", 20.0f), 1.0f, 60.0f);
			c.driveGravity = std::clamp(IniFloat(path, "DriveGravity", 1.1f), 0.5f, 3.0f);
			c.propBreak = std::clamp(IniFloat(path, "PropBreak", 1.0f), 0.1f, 10.0f);
			c.worldLanding = GetPrivateProfileIntA(kSec, "WorldLanding", 1, path) != 0;
			c.propPhysics = GetPrivateProfileIntA(kSec, "PropPhysics", 1, path) != 0;
			c.propPhysicsMax = std::clamp(int(GetPrivateProfileIntA(kSec, "PropPhysicsMax", 10, path)), 1, 40);
			Log("settings: v2.5: ground rays %.1f m up / %.1f m down from the level; walls %d (scan %.2f m up, lines %.2f m apart, %.2f ms a frame); props %d "
			    "(within %.0f m, push x%.2f, bump x%.2f, big ones solid %d); dents %d (%.0f a second); BeamNG gravity x%.2f while GTA drives",
				c.worldUpM, c.worldDownM, int(c.worldWalls), c.worldWallProbeM, c.worldWallStepM, c.worldWallMs, int(c.props), c.propRangeM, c.propPush, c.propBump,
				int(c.propSolid), int(c.dents), c.dentHz, c.driveGravity);
			Log("settings: v2.51: breakaway props break after BeamNG's car lost x%.2f of their speed against them; jumps: ground built where the car lands %d",
				c.propBreak, int(c.worldLanding));
			Log("settings: v2.52: physics props %d (%d at once); ground heights to BeamNG in mm", int(c.propPhysics), c.propPhysicsMax);
			c.weapons = GetPrivateProfileIntA(kSec, "Weapons", 1, path) != 0;
			c.trafficCollision = GetPrivateProfileIntA(kSec, "TrafficCollision", 1, path) != 0;
			c.trafficTwins = std::clamp(int(GetPrivateProfileIntA(kSec, "TrafficTwins", 4, path)), 0, 8);
			c.worldDeep = GetPrivateProfileIntA(kSec, "WorldDeep", 1, path) != 0;
			c.worldFill = GetPrivateProfileIntA(kSec, "WorldFill", 1, path) != 0;
			c.doorOpenDeg = std::clamp(IniFloat(path, "DoorOpenDeg", 65.0f), 0.0f, 120.0f);
			c.tireSmoke = GetPrivateProfileIntA(kSec, "TireSmoke", 1, path) != 0;
			c.tireSmokeMs = std::clamp(IniFloat(path, "TireSmokeSlipMs", 3.0f), 0.5f, 30.0f);
			{
				char fx[100] = "";
				GetPrivateProfileStringA(kSec, "TireSmokeFx", "core,wheel_burnout", fx, sizeof(fx), path);
				const char* cm = strchr(fx, ',');
				if (cm && cm > fx && cm[1]) {
					snprintf(c.tireFx[0], sizeof(c.tireFx[0]), "%.*s", int(cm - fx), fx);
					snprintf(c.tireFx[1], sizeof(c.tireFx[1]), "%s", cm + 1);
				}
			}
			{
				char ce[32] = "";
				GetPrivateProfileStringA(kSec, "CarExplosion", "gta", ce, sizeof(ce), path);
				c.carExplosion = (!_stricmp(ce, "beamng") || !strcmp(ce, "1")) ? 1 : 0;
			}
			Log("settings: v2.6: weapons and explosions on BeamNG's car %d (GtaBeamD1_weapons.json); the burning car blows up %s", int(c.weapons),
				c.carExplosion ? "BeamNG's way (its fuel explodes) + GTA's explosion" : "with GTA's explosion");
			Log("settings: v2.43 GTA ground: levels %.1f m apart (bridges), edges %.1f m high leave the ground below out, static world %d triangles within %.0f m, "
			    "filled at %.1f tiles a second when standing",
				c.worldLevelM, c.worldDropM, c.worldKeepTris, c.worldKeepM, c.worldFillPerS);
			Log("settings: v2.42 GTA ground: finer where a cell is %.1f cm off flat (soft ground %.1f cm), drafts %.1f s ahead at speed, ring %.0f m, saved map %s (read %.0f m ahead)",
				c.worldRefineCm, c.worldRefineSoftCm, c.worldDraftS, c.worldPrebuildM, c.worldMap ? "on" : "off", c.worldMapReadM);
			Log("settings: v2.41 GTA ground in BeamNG %s: tiles %.0f m, first pass every %.2f m, finest %.2f m where a cell is %.1f cm off flat, %.0f m around the car, %.1f s ahead x "
			    "%.0f m wide, %d rays per frame (%d waiting at most) + immediate rays for %.2f ms a frame within %.1f s, %d tiles at once, %d tiles remembered, %.0f m above "
			    "BeamNG's map, new origin past %.0f m, fall check %.1f m for %.1f s; F10 below %.0f km/h; F10 off keeps speed %d",
				c.world ? "ON" : "off", c.worldTileM, c.worldCoarseM, c.worldStepNear, c.worldRefineCm, c.worldRadius, c.worldAheadS, c.worldWidth, c.worldRays,
				c.worldInFlight, c.worldSyncMs, c.worldUrgentS, c.worldTilesWork, c.worldCacheTiles, c.worldZLift, c.worldRebaseM, c.worldFallM, c.worldFallWaitS, c.driveStartMaxKmh,
				int(c.exitKeepSpeed));
		}
		Log("settings: frame delay %d, fov %s, tolerance %.4f + %.3f m, depth source %s, depth Z %s, near scale %.3f, key 0x%02X", c.frameDelay,
			c.fovHorizontal ? "horizontal" : "vertical", c.tolRel, c.tolAbs, src, c.depthReversed < 0 ? "auto" : c.depthReversed ? "reversed" : "standard",
			c.nearScale, c.key);
		Log("settings: car file '%s', max texture %d (data maps %d), axes %d, orientation %d, offset %.2f/%.2f/%.2f m, turn %.1f deg, paint gamma %.2f, brightness %.2f, "
		    "ambient %.2f, specular %.2f, sun %.2f, alpha cutout %.2f, glass alpha %.2f, glass reflections %.2f, layer mask check %d, BeamNG port %d",
			c.carFile, c.maxTex, c.maxDataTex, c.meshAxes, c.meshOrient, c.offX, c.offY, c.offZ, c.yawDeg, c.paintGamma, c.exposure, c.ambient, c.specStrength,
			c.sunStrength, c.alphaCut, c.glassMinAlpha, c.glassReflections, c.maskCheck, c.port);
		Log("settings: shadow mode %s, hide mode %s, contact %.2f, sun shadow %.2f, softness %.2f, sun offset %.0f deg x%.2f, self shadows %d (%d), AO %d (x%.2f), "
		    "world shadows %d, reflections %.2f, environment %s x%.2f, normal maps %d (x%.2f, %s), surface maps %d, edge smoothing %d, minimap on top %d (+%.0f px), debug view %d",
			ShadowName(c.shadowMode), HideName(EffectiveHide()), c.contactStrength, c.sunShadowStrength, c.shadowSoftness,
			c.sunAzimuth, c.sunElevation, c.selfShadows, c.selfShadowSize, c.ao, c.aoStrength, c.worldShadows, c.reflections, c.environment ? "world" : "sky",
			c.envBrightness, c.normalMaps, c.normalStrength, c.normalGreen < 0 ? "opengl" : "directx", c.surfaceMaps, c.edgeSmoothing, c.hudMask, c.hudMargin,
			c.debugView);
		Log("settings: wheels %d, source %s, suspension %d, (speed model: max steer %.0f deg, sign %+.0f, driven %s, burnout %.0f m/s, handbrake locks rear %d)", c.wheels,
			WheelSrcName(c.wheelSource), c.suspension, c.maxSteer, c.steerSign, DrivenName(c.drivenWheels), c.burnoutSpeed, c.handbrakeLocks);
		Log("settings: draw stage %s, HDR target %s, HDR sun/sky %.1f, HDR ambient %.2f, show driver %d, texture compression %d, camera source %s, camera slip fix %d",
			c.drawStage ? "auto (inside GTA's frame when found)" : "present", c.hdrTarget == 0 ? "auto" : c.hdrTarget == 1 ? "1 (last)" : "2 (the one before)", c.hdrSunToSky,
			c.hdrAmbient, c.showDriver, c.texCompression, c.cameraSource < 0 ? "auto" : c.cameraSource == 0 ? "final" : "gameplay", c.camSlipFix);
		Log("settings: frame pairing %d, one-sided see-through parts %d, ride height check %d", c.framePairing, c.oneSidedGlass, c.rideCheck);
		Log("settings: car/camera pairing %s, HDR sun %s, HDR ambient %s, paint metallic/roughness/clear coat/clear coat roughness %s",
			c.carCamPairing < 0 ? "auto" : c.carCamPairing ? "previous" : "same", c.hdrSunAuto ? "auto" : "fixed", c.hdrAmbAuto ? "auto" : "fixed",
			(c.paintSurf[0] < 0 && c.paintSurf[1] < 0 && c.paintSurf[2] < 0 && c.paintSurf[3] < 0) ? "auto" : "partly set in the ini");
		Log("settings: GTA reflections %s, brightness %s (x%.2f), layout up %d / down %d (-1 = auto)", c.gtaRefl ? "auto" : "off",
			c.gtaReflScale < 0 ? "auto" : "fixed", c.gtaReflScale, c.gtaReflLayout[0], c.gtaReflLayout[1]);
		Log("settings: drive key 0x%02X, steering sign %s, input filter %s, smoothing %s ms, set velocity %d", c.driveKey,
			c.driveSteerSign == 0 ? "auto" : c.driveSteerSign > 0 ? "+1" : "-1", c.driveFilter < 0 ? "auto" : c.driveFilter == 0 ? "key" : c.driveFilter == 1 ? "pad" : "direct",
			c.driveDelayMs < 0 ? "auto" : std::to_string(int(c.driveDelayMs)).c_str(), c.driveSetVelocity);
		{
			std::string gta, beam;
			for (int k = 0; k < kPadCtls; ++k) {
				if (c.padAlwaysGta & (1u << k)) gta += std::string(gta.empty() ? "" : " ") + kPadName[k];
				if (c.padAlwaysBeam & (1u << k)) beam += std::string(beam.empty() ? "" : " ") + kPadName[k];
			}
			Log("settings: v2.1: BeamNG pad bindings %s, gauge %d (%s), BeamNG sound in the background %d, GTA engine sound while driving %d", c.padList.c_str(),
				c.gauge, c.gaugeKmh ? "km/h" : "mph", c.beamSound, c.gtaEngineSound);
			Log("settings: drive input %s, pad toggle %s, pad always GTA: %s, always BeamNG: %s, starts in %s mode, pad %s, keys always GTA:%s, "
			    "BeamNG actions: categories '%s', never '%s', also '%s', check text %d, parked car blocks BeamNG's controls %d",
				c.driveInput ? "beamng (your BeamNG bindings)" : "gta (v2.0: GTA's controls)", c.padToggle >= 0 ? kPadName[c.padToggle] : "none", gta.c_str(),
				beam.c_str(), c.padStartGta ? "GTA" : "BeamNG", c.padIndex < 0 ? "auto" : std::to_string(c.padIndex).c_str(), c.keysAlwaysGta.c_str(), c.beamCats.c_str(),
				c.beamNever.c_str(), c.beamAlso.c_str(), c.driveHud, c.parkBlock);
			Log("settings: v2.43: BeamNG actions that repair the car where it is: '%s'; pausing one game pauses the other %d", c.repairActs.c_str(), int(c.pauseSync));
		}
	}

	// ---------------------------------------------------------------------------------------
	// Sun model (GTA has no native for its sun direction): rises in the east at 6, due south at 13,
	// sets in the west at 20. SunAzimuthOffset / SunElevationScale tune it.
	// ---------------------------------------------------------------------------------------

	struct Sun
	{
		double S[3];  // world direction to the sun (x east, y north, z up)
		double dayF;  // 0 night .. 1 day
		double elev;  // radians
	};

	Sun SunModel(double hour)
	{
		Sun          s{};
		const double t = std::clamp((hour - 6.0) / 14.0, 0.0, 1.0);
		s.dayF = std::clamp((hour - 5.0) / 1.5, 0.0, 1.0) * std::clamp((21.0 - hour) / 1.5, 0.0, 1.0);
		s.elev = std::clamp(std::max(sin(t * kPi) * 60.0, 6.0) * g_cfg.sunElevation, 3.0, 89.0) * kDeg;
		const double el = s.dayF > 0.0 ? s.elev : 60.0 * kDeg;  // night: soft light from above
		const double phi = t * kPi + g_cfg.sunAzimuth * kDeg;    // 0 east, pi/2 south, pi west
		s.S[0] = cos(phi) * cos(el);
		s.S[1] = -sin(phi) * cos(el);
		s.S[2] = sin(el);
		return s;
	}

	// ---------------------------------------------------------------------------------------
	// State shared between the script thread (natives) and the render thread (Present)
	// ---------------------------------------------------------------------------------------

	struct CamSample
	{
		double    px, py, pz;
		float     pitch, roll, yaw;  // degrees, rotation order 2
		float     fov, nearClip, farClip;
		uint32_t  seq;
		ULONGLONG ms;
		// the car to draw, sampled in the same script tick as the camera
		bool   carOn;
		double ox, oy, oz;               // world position of the drawn car's origin (footprint centre, tyre bottom)
		float  R[3], F[3], U[3];         // world axes of the drawn car
		float  hour;                     // GTA clock, for the sun
		float  sunVis;                   // 0..1: GTA's world doesn't block the sun (raycasts)
		// wheels, speed model
		double distFront, distRear;      // metres the front/rear tyre surfaces have turned
		float  steerDeg;                 // front wheel steering, + = left
		// wheels from GTA's own wheel bones (0 lf, 1 rf, 2 lr, 3 rr)
		bool   gtaWheels;
		double gDist[4];                 // metres the tyre surface has turned
		float  gSteer[4];                // radians, + = left
		float  gTravel[4];               // suspension travel, metres (+ = up)
		// v2.0 drive mode: wheels from BeamNG, per car-file wheel (car space)
		bool   bngWheels;
		bool   bwOk[kDrvWheels];
		float  bwC[kDrvWheels][3];       // hub centre
		float  bwA[kDrvWheels][3];       // axle direction (pointing right)
		float  bwAng[kDrvWheels];        // turn about the axle (rad, right-hand rule: rolling forward = negative)
		// HUD
		bool   hudOn;
		float  hud[4];                   // minimap rectangle in screen 0..1: x0 y0 x1 y1
	};

	struct RayCheck
	{
		double    viewZ;
		ULONGLONG ms;
		bool      valid;
	};

	SRWLOCK   g_shared = SRWLOCK_INIT;
	CamSample g_camRing[kCamRing] = {};
	uint32_t  g_camSeq = 0;
	RayCheck  g_ray = {};
	bool      g_paused = false;
	char      g_notify[4][160] = {};  // a small queue, so quick messages don't overwrite each other
	int       g_notifyCount = 0;
	char      g_status[128] = "depth: not found yet";

	std::atomic<int>      g_viewMode{ 0 };  // 0 normal, 1 no occlusion, 2 depth view, 3 part picker
	std::atomic<uint32_t> g_ticks{ 0 };
	std::atomic<uint64_t> g_tickUsSum{ 0 }, g_tickUsMax{ 0 };  // v1.9.3: time our script tick takes (microseconds)
	std::atomic<int>      g_fileDriven{ -1 };  // driven axles from the export: bit 0 front, bit 1 rear, -1 unknown
	std::atomic<int>      g_capture{ 0 };      // frame capture: 0 off, 1 armed, 2 recording
	ID3D11Resource*       g_gbufSeen = nullptr;  // v1.9.5: GTA's G-buffer colour target (render thread; identity only)
	// v1.9.7: GTA's reflection map (dual paraboloid, R11G11B10 float, twice as wide as high, bound several
	// times early in the frame: once per mip level). Render thread; identity only.
	ID3D11Resource*       g_dpSeen = nullptr;  // referenced (AddRef) until the end of the frame
	ID3D11Resource*       g_dpCand = nullptr;
	int                   g_dpCandBinds = 0;
	UINT                  g_dpCandMaxMip = 0;    // highest mip level GTA wrote this frame
	UINT                  g_dpSeenMips = 0;      // = that + 1, for g_dpSeen

	const char* ViewModeName(int m)
	{
		return m == 1 ? "car always on top" : m == 2 ? "depth view" : m == 3 ? "part picker (aim the screen centre at a part)" : "normal";
	}

	void Notify(const char* fmt, ...)
	{
		char    buf[160];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		for (char* c = buf; *c; ++c) {
			if (*c == '~') {
				*c = '-';  // GTA text formatting character
			}
		}
		AcquireSRWLockExclusive(&g_shared);
		if (g_notifyCount == 4) {
			memmove(g_notify[0], g_notify[1], sizeof(g_notify[0]) * 3);
			g_notifyCount = 3;
		}
		memcpy(g_notify[g_notifyCount++], buf, sizeof(buf));
		ReleaseSRWLockExclusive(&g_shared);
		Log("notify: %s", buf);
	}

	void SetStatus(const char* fmt, ...)
	{
		char    buf[128];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		AcquireSRWLockExclusive(&g_shared);
		memcpy(g_status, buf, sizeof(g_status));
		ReleaseSRWLockExclusive(&g_shared);
	}

	// Camera basis from GTA's rotation (order 2). GTA world axes: x east, y north, z up.
	void CamBasis(double pitchDeg, double rollDeg, double yawDeg, double r[3], double f[3], double u[3])
	{
		const double cp = cos(pitchDeg * kDeg), sp = sin(pitchDeg * kDeg);
		const double cy = cos(yawDeg * kDeg), sy = sin(yawDeg * kDeg);
		const double cr = cos(rollDeg * kDeg), sr = sin(rollDeg * kDeg);
		r[0] = cy * cr - sy * sp * sr;
		r[1] = sy * cr + cy * sp * sr;
		r[2] = -cp * sr;
		f[0] = -sy * cp;
		f[1] = cy * cp;
		f[2] = sp;
		u[0] = cy * sr + sy * sp * cr;
		u[1] = sy * sr - cy * sp * cr;
		u[2] = cp * cr;
	}

	// Rotation matrix from GTA Euler angles (degrees: x pitch, y roll, z yaw) for one of the six orders
	// GTA knows, M = R(first letter) * R(second) * R(third). Order 2 (ZXY) is CamBasis above.
	const char* kEulerNames[6] = { "ZYX", "YZX", "ZXY", "XZY", "YXZ", "XYZ" };
	void EulerMatrix(int order, double xDeg, double yDeg, double zDeg, double M[3][3])
	{
		auto rot = [](char ax, double a, double m[3][3]) {
			const double c = cos(a), s = sin(a);
			const double X[3][3] = { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } };
			const double Y[3][3] = { { c, 0, s }, { 0, 1, 0 }, { -s, 0, c } };
			const double Z[3][3] = { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } };
			memcpy(m, ax == 'X' ? X : ax == 'Y' ? Y : Z, sizeof(double) * 9);
		};
		auto mul = [](const double a[3][3], const double b[3][3], double o[3][3]) {
			double t[3][3];
			for (int i = 0; i < 3; ++i)
				for (int j = 0; j < 3; ++j) t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
			memcpy(o, t, sizeof(t));
		};
		const char* s = kEulerNames[std::clamp(order, 0, 5)];
		auto        angle = [&](char ax) { return (ax == 'X' ? xDeg : ax == 'Y' ? yDeg : zDeg) * kDeg; };
		double      a[3][3], b[3][3], c[3][3];
		rot(s[0], angle(s[0]), a);
		rot(s[1], angle(s[1]), b);
		rot(s[2], angle(s[2]), c);
		mul(a, b, M);
		mul(M, c, M);
	}

	// Light-space basis for a direction L (car space): the same in C++ and HLSL (DirBasis).
	void DirBasis(const double L[3], double e1[3], double e2[3])
	{
		const double up[3] = { 0, fabs(L[2]) > 0.99 ? 1.0 : 0.0, fabs(L[2]) > 0.99 ? 0.0 : 1.0 };
		double       c[3] = { up[1] * L[2] - up[2] * L[1], up[2] * L[0] - up[0] * L[2], up[0] * L[1] - up[1] * L[0] };
		const double l = sqrt(c[0] * c[0] + c[1] * c[1] + c[2] * c[2]);
		for (int k = 0; k < 3; ++k) e1[k] = c[k] / std::max(l, 1e-9);
		e2[0] = L[1] * e1[2] - L[2] * e1[1];
		e2[1] = L[2] * e1[0] - L[0] * e1[2];
		e2[2] = L[0] * e1[1] - L[1] * e1[0];
	}

	// ---------------------------------------------------------------------------------------
	// Car model handoff: loader thread -> render thread
	// ---------------------------------------------------------------------------------------

	std::mutex                  g_modelMx;
	std::unique_ptr<car::Model> g_pendingModel;  // loaded, waiting for the render thread to upload
	std::atomic<bool>           g_workerBusy{ false };
	std::atomic<bool>           g_haveModel{ false };  // the render thread has a car to draw
	std::string                 g_lastCarPath;         // guarded by g_modelMx
	std::atomic<float>          g_modelSize[3] = {};   // width, length, height of the loaded car
	std::atomic<float>          g_doorAng[2] = {};     // v2.73: how far the drawn front doors are open (rad; left, right)
	std::atomic<int>            g_doorParts[2] = {};   // v2.73: the drawn car's front door parts (left, right)
	std::atomic<uint64_t>       g_dgBroken[4] = {};    // v2.6: the car's breakable groups (car::Model::dgNames) BeamNG broke (bit per group)
	// v2.62: bullet holes on the drawn car (car space x y z, radius), the newest over the oldest past kHoles
	constexpr int               kHoles = 128;  // v2.7: 64 -> 128 (your 2.63 test: a riddled side ran out)
	std::mutex                  g_holeMx;
	float                       g_holes[kHoles][8] = {};  // v2.7: centre, radius; the bullet's direction (car frame), 0
	int                         g_holeN = 0, g_holeNext = 0;
	float                       g_burnt = 0;  // v2.7: the drawn car charred (0..1) after it blew up; off when repaired
	std::atomic<uint32_t>       g_holeGen{ 0 };
	// v2.63: a cracked windscreen / lamp shatters out after more hits on it: each breakable group's box (car space, from the
	// drawn car's own vertices), hits on it since it cracked, and the shattered ones (bit per group, drawn like hidden glass)
	std::vector<std::array<float, 6>> g_dgBox;  // guarded by g_holeMx
	int                               g_dgHits[256] = {};
	std::atomic<uint64_t>             g_dgShatter[4] = {};
	void HolesClear(const char* why)
	{
		std::lock_guard<std::mutex> lk(g_holeMx);
		if (g_holeN) Log("weapons: %d bullet holes taken off the drawn car (%s)", g_holeN, why);
		g_holeN = g_holeNext = 0;
		g_burnt = 0;
		++g_holeGen;
		memset(g_dgHits, 0, sizeof(g_dgHits));
		for (auto& b : g_dgShatter) b.store(0);
	}

	// ---------------------------------------------------------------------------------------
	// Script thread: natives, keys, the target GTA car
	// ---------------------------------------------------------------------------------------

	struct NVec3
	{
		alignas(8) float x;
		alignas(8) float y;
		alignas(8) float z;
	};

	template <typename T>
	inline void Push(T v)
	{
		static_assert(sizeof(T) <= sizeof(UINT64), "native argument too large");
		UINT64 u = 0;
		memcpy(&u, &v, sizeof(T));
		nativePush64(u);
	}

	template <typename R, typename... A>
	inline R Call(UINT64 hash, A... args)
	{
		nativeInit(hash);
		(Push(args), ...);
		return *reinterpret_cast<R*>(nativeCall());
	}

	template <typename... A>
	inline void CallVoid(UINT64 hash, A... args)
	{
		nativeInit(hash);
		(Push(args), ...);
		nativeCall();
	}

	// v2.01: text longer than one piece (GTA cuts a piece at 99 characters) goes in pieces into "CELL_EMAIL_BCON"
	void PushLongText(const char* text)
	{
		const size_t len = strlen(text);
		char         piece[100];
		for (size_t i = 0; i < len; i += 99) {
			const size_t m = std::min<size_t>(99, len - i);
			memcpy(piece, text + i, m);
			piece[m] = 0;
			CallVoid(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, piece);
		}
	}

	void ShowFeed(const char* text)
	{
		const bool longText = strlen(text) > 99;
		CallVoid(N::BEGIN_TEXT_COMMAND_THEFEED_POST, longText ? "CELL_EMAIL_BCON" : "STRING");
		if (longText) PushLongText(text);
		else CallVoid(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
		Call<int>(N::END_TEXT_COMMAND_THEFEED_POST_TICKER, 0, 0);
	}

	bool GtaFocused()
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &pid);
		return pid == GetCurrentProcessId();
	}

	// The GTA car the BeamNG car is drawn on (script thread only).
	struct Target
	{
		int   veh = 0;
		int   mode = 0;  // 0 none, 1 drawn + GTA car hidden, 2 drawn + GTA car visible
		NVec3 dmin{}, dmax{};
		char  name[32] = "";
	} g_tgt;

	// Hide or show the GTA car. "transparent" (alpha 0) keeps GTA's own shadow of it (VERIFIED v1.7);
	// "invisible" removes it, but then GTA stops turning its wheels (VERIFIED v1.9.1: 0.0 rad);
	// "ghost" (v1.9.3) = transparent with GTA's shadow of it switched off, so its wheels keep turning.
	// Showing always undoes all of them.
	void SetVisible(int veh, bool on)
	{
		if (!veh || !Call<int>(N::DOES_ENTITY_EXIST, veh)) {
			return;
		}
		const int hide = EffectiveHide();
		if (on) {
			CallVoid(N::RESET_ENTITY_ALPHA, veh);
			CallVoid(N::SET_ENTITY_VISIBLE, veh, 1, 0);
			CallVoid(N::SET_ENTITY_FLAG_SUPPRESS_SHADOW, veh, 0);
		} else if (hide == 0 || hide == 2) {
			CallVoid(N::SET_ENTITY_VISIBLE, veh, 1, 0);
			CallVoid(N::SET_ENTITY_ALPHA, veh, 0, 0);
			CallVoid(N::SET_ENTITY_FLAG_SUPPRESS_SHADOW, veh, hide == 2 ? 1 : 0);
		} else {
			CallVoid(N::RESET_ENTITY_ALPHA, veh);
			CallVoid(N::SET_ENTITY_VISIBLE, veh, 0, 0);
		}
	}

	// ShowDriver=0: the player is hidden while he sits in the car the BeamNG car is drawn on.
	int g_hiddenPed = 0;
	void UpdateDriver(int veh, bool drawnHidden)
	{
		const int  ped = Call<int>(N::PLAYER_PED_ID);
		const bool inVeh = drawnHidden && veh && Call<int>(N::IS_PED_IN_VEHICLE, ped, veh, 0);
		const bool want = inVeh && !g_cfg.showDriver;
		// v2.71: shown: hiding the car hides you with it (your v2.7 note: gone from F10 on, back only after getting out and in)
		if (inVeh && g_cfg.showDriver && (!Call<int>(N::IS_ENTITY_VISIBLE, ped) || Call<int>(N::GET_ENTITY_ALPHA, ped) < 255)) {
			static int shown = 0;
			CallVoid(N::SET_ENTITY_VISIBLE, ped, 1, 0);
			CallVoid(N::RESET_ENTITY_ALPHA, ped);
			if (shown++ < 10) Log("driver (v2.71): you were hidden with the car: shown again (ShowDriver=1)");
		}
		if (want && g_hiddenPed != ped) {
			if (g_hiddenPed) CallVoid(N::SET_ENTITY_VISIBLE, g_hiddenPed, 1, 0);
			CallVoid(N::SET_ENTITY_VISIBLE, ped, 0, 0);
			g_hiddenPed = ped;
		} else if (!want && g_hiddenPed) {
			if (Call<int>(N::DOES_ENTITY_EXIST, g_hiddenPed)) {
				CallVoid(N::SET_ENTITY_VISIBLE, g_hiddenPed, 1, 0);
				CallVoid(N::RESET_ENTITY_ALPHA, g_hiddenPed);  // v2.61: (the hidden car's alpha 0 stayed on you as you got out)
			}
			g_hiddenPed = 0;
		}
	}

	// ---- wheels: speed model (v1.7-v1.9, the fallback)
	double    g_distFront = 0, g_distRear = 0;
	double    g_extraFront = 0, g_extraRear = 0;  // tyre surface speed beyond the ground speed (m/s): burnout spin, locked wheels
	float     g_steer = 0;
	double    g_wheelT = 0;  // seconds (QueryPerformanceCounter): GetTickCount64 steps in ~16 ms, too coarse for wheel speeds
	double    g_carVelW[3] = {};  // v1.9.4: the GTA car's world velocity (m/s), for the camera sync

	double NowSeconds()
	{
		static LARGE_INTEGER freq{};
		if (!freq.QuadPart) QueryPerformanceFrequency(&freq);
		LARGE_INTEGER t;
		QueryPerformanceCounter(&t);
		return double(t.QuadPart) / double(freq.QuadPart);
	}
	bool      g_wasBurnout = false;
	int       g_burnDir = 1;  // which way a burnout spins the wheels (+1 forwards), with hysteresis

	// ---- wheels: GTA's own wheels (v1.9.1). GTA has no standard native for a wheel's spin, but its wheel
	// bones turn with the wheels: GET_ENTITY_BONE_ROTATION gives each bone's world rotation as Euler
	// angles. Which Euler order and which bone axis is the axle are found by watching the bones while
	// driving (the axle stays level with the car; the other axes roll). Spin direction per wheel is
	// learned from the car's speed.
	const char* kWheelBones[4] = { "wheel_lf", "wheel_rf", "wheel_lr", "wheel_rr" };
	struct GtaWheel
	{
		int    bone = -1;
		bool   havePrev = false;
		double prevAng = 0;
		double dist = 0, steer = 0, travel = 0;
		double rest = 0;
		bool   haveRest = false;
		double radius = 0.34;
		int    sign = 1;
		double speed = 0;  // tyre surface speed (m/s), smoothed, for the log
		double rate = 0;   // tyre surface speed over the last tick (m/s)
		double steerAvg = 0;  // rear wheels: average |steer| (should stay ~0; if not, the reading is wrong)
	};
	struct WheelSample
	{
		float  rot[4][3];
		double R[3], F[3], U[3];
		double ground;
		double dt;
		bool   burnout;
	};
	struct WheelCal
	{
		bool                     done = false, failed = false;
		int                      order = 2, axis = 0;
		bool                     local = false;  // the bone rotations turned out to be relative to the car, not the world
		std::vector<WheelSample> samples;
		char                     why[160] = "";
		ULONGLONG                lastLogMs = 0;
	};
	GtaWheel g_gw[4];
	WheelCal g_wcal;
	bool     g_wheelMsgShown = false;
	int      g_wheelRelearns = 0;

	// steer (rad, + left) and roll angle (rad) of one GTA wheel bone, in the GTA car's frame
	void DecodeWheel(int order, int axis, const float rot[3], const double R[3], const double F[3], const double U[3], double& steer, double& ang, double* level = nullptr)
	{
		double M[3][3];
		EulerMatrix(order, rot[0], rot[1], rot[2], M);
		const double a[3] = { M[0][axis], M[1][axis], M[2][axis] };
		const int    bi = (axis + 1) % 3;
		const double b[3] = { M[0][bi], M[1][bi], M[2][bi] };
		auto         dot = [](const double* p, const double* q) { return p[0] * q[0] + p[1] * q[1] + p[2] * q[2]; };
		double       ax[3] = { dot(a, R), dot(a, F), dot(a, U) };
		if (level) {
			*level = fabs(ax[2]);
		}
		if (ax[0] < 0) {
			ax[0] = -ax[0];
			ax[1] = -ax[1];
		}
		steer = atan2(ax[1], ax[0]);
		const double bc[3] = { dot(b, R), dot(b, F), dot(b, U) };
		const double cs = cos(steer), sn = sin(steer);
		const double by = -bc[0] * sn + bc[1] * cs;
		ang = atan2(bc[2], by);
	}

	double WrapPi(double a)
	{
		while (a > kPi) a -= 2 * kPi;
		while (a < -kPi) a += 2 * kPi;
		return a;
	}

	void ResetGtaWheels(int veh)
	{
		g_wcal = WheelCal{};
		g_wheelMsgShown = false;
		g_wheelRelearns = 0;
		int found = 0;
		for (int i = 0; i < 4; ++i) {
			g_gw[i] = GtaWheel{};
			g_gw[i].bone = veh ? Call<int>(N::GET_ENTITY_BONE_INDEX_BY_NAME, veh, kWheelBones[i]) : -1;
			found += g_gw[i].bone >= 0 ? 1 : 0;
		}
		Log("wheels: GTA wheel bones found: %d of 4 (lf %d, rf %d, lr %d, rr %d)", found, g_gw[0].bone, g_gw[1].bone, g_gw[2].bone, g_gw[3].bone);
	}

	// Decide the Euler order / axle axis from the samples collected while driving.
	void DecideWheelCal()
	{
		WheelCal& w = g_wcal;
		// For each reading (world or car-relative rotations, 6 Euler orders, 3 bone axes): how level the
		// axle stays (0 = always level with the car) and how well it lines up with the car's right axis
		// (1 = always). The right reading has a level axle that points sideways.
		double score[2][6][3] = {}, align[2][6][3] = {};
		int    cnt = 0;
		for (const WheelSample& s : w.samples) {
			for (int g = 0; g < 4; ++g) {
				if (g_gw[g].bone < 0) continue;
				++cnt;
				for (int k = 0; k < 6; ++k) {
					double M[3][3];
					EulerMatrix(k, s.rot[g][0], s.rot[g][1], s.rot[g][2], M);
					for (int a = 0; a < 3; ++a) {
						const double ax[3] = { M[0][a], M[1][a], M[2][a] };
						score[0][k][a] += fabs(ax[0] * s.U[0] + ax[1] * s.U[1] + ax[2] * s.U[2]);
						align[0][k][a] += fabs(ax[0] * s.R[0] + ax[1] * s.R[1] + ax[2] * s.R[2]);
						score[1][k][a] += fabs(ax[2]);
						align[1][k][a] += fabs(ax[0]);
					}
				}
			}
		}
		if (!cnt) {
			w.failed = true;
			snprintf(w.why, sizeof(w.why), "the GTA car has no wheel bones");
			return;
		}
		int    bf = 0, bk = 2, ba = 0;
		double best = 1e9;
		for (int f = 0; f < 2; ++f)
			for (int k = 0; k < 6; ++k)
				for (int a = 0; a < 3; ++a) {
					const double m = score[f][k][a] / cnt + 0.5 * (1.0 - align[f][k][a] / cnt) + (f ? 0.01 : 0.0);  // ties go to world rotations (what the native is documented to give)
					if (m < best) {
						best = m;
						bf = f;
						bk = k;
						ba = a;
					}
				}
		auto frameOf = [&](const WheelSample& s, double R[3], double F[3], double U[3]) {
			for (int i = 0; i < 3; ++i) {
				R[i] = bf ? (i == 0 ? 1.0 : 0.0) : s.R[i];
				F[i] = bf ? (i == 1 ? 1.0 : 0.0) : s.F[i];
				U[i] = bf ? (i == 2 ? 1.0 : 0.0) : s.U[i];
			}
		};
		// how much the wheels rolled according to that reading, and which way is forward
		double rolled = 0, corr[4] = {};
		for (int g = 0; g < 4; ++g) {
			if (g_gw[g].bone < 0) continue;
			bool   have = false;
			double prev = 0;
			for (const WheelSample& s : w.samples) {
				double st, ang, R[3], F[3], U[3];
				frameOf(s, R, F, U);
				DecodeWheel(bk, ba, s.rot[g], R, F, U, st, ang);
				if (have) {
					const double d = WrapPi(ang - prev);
					if (g == 0) rolled += fabs(d);
					// only samples slow enough that one frame's turn can't be mistaken for the other way round
					if (!s.burnout && fabs(s.ground) * s.dt / g_gw[g].radius < 2.0) corr[g] += d * s.ground;
				}
				prev = ang;
				have = true;
			}
		}
		Log("wheels: bone reading (lower = axle stays level and sideways): best %s rotations, Euler %s, axis %c = %.3f (level %.3f, sideways %.3f; as %s rotations %.3f / %.3f); "
			"rolled %.1f rad over %zu samples",
			bf ? "car-relative" : "world", kEulerNames[bk], "XYZ"[ba], best, score[bf][bk][ba] / cnt, align[bf][bk][ba] / cnt, bf ? "world" : "car-relative", score[1 - bf][bk][ba] / cnt,
			align[1 - bf][bk][ba] / cnt, rolled, w.samples.size());
		if (rolled < 1.0) {
			w.failed = true;
			snprintf(w.why, sizeof(w.why), "GTA's wheel bones don't turn (%.1f rad while driving)%s", rolled,
				EffectiveHide() == 1 ? "; GTA doesn't turn the wheels of an invisible car, use HideMode=ghost" : "");
		} else if (best > 0.2) {
			w.failed = true;
			snprintf(w.why, sizeof(w.why), "couldn't read the bone rotations (best score %.2f)", best);
		} else {
			w.done = true;
			w.order = bk;
			w.axis = ba;
			w.local = bf != 0;
			for (int g = 0; g < 4; ++g) {
				g_gw[g].sign = corr[g] >= 0 ? 1 : -1;
				g_gw[g].havePrev = false;
			}
			Log("wheels: following GTA's wheels (%s rotations, Euler %s, axle = bone %c axis, spin signs %+d %+d %+d %+d, radii %.2f %.2f %.2f %.2f m)",
				w.local ? "car-relative" : "world", kEulerNames[bk], "XYZ"[ba], g_gw[0].sign, g_gw[1].sign, g_gw[2].sign, g_gw[3].sign, g_gw[0].radius, g_gw[1].radius,
				g_gw[2].radius, g_gw[3].radius);
		}
		std::vector<WheelSample>().swap(w.samples);
	}

	int FindTargetCar()
	{
		const int ped = Call<int>(N::PLAYER_PED_ID);
		int       veh = Call<int>(N::GET_VEHICLE_PED_IS_IN, ped, 0);
		if (veh) {
			return veh;
		}
		const NVec3 pp = Call<NVec3>(N::GET_ENTITY_COORDS, ped, 1);
		const int   last = Call<int>(N::GET_PLAYERS_LAST_VEHICLE);
		if (last && Call<int>(N::DOES_ENTITY_EXIST, last)) {
			const NVec3 lp = Call<NVec3>(N::GET_ENTITY_COORDS, last, 1);
			const float dx = lp.x - pp.x, dy = lp.y - pp.y, dz = lp.z - pp.z;
			if (dx * dx + dy * dy + dz * dz < 25.0f * 25.0f) {
				return last;
			}
		}
		return Call<int>(N::GET_CLOSEST_VEHICLE, pp.x, pp.y, pp.z, 15.0f, 0, 70);
	}

	std::atomic<bool> g_attachedNew{ false };  // tells the render thread to relearn the environment

	void Attach()
	{
		const int veh = FindTargetCar();
		if (!veh) {
			Notify("D1: no car found. Get in a car or stand next to one (15 m).");
			return;
		}
		const unsigned model = Call<unsigned>(N::GET_ENTITY_MODEL, veh);
		NVec3          mn{}, mx{};
		CallVoid(N::GET_MODEL_DIMENSIONS, model, &mn, &mx);
		const char* dn = Call<const char*>(N::GET_DISPLAY_NAME_FROM_VEHICLE_MODEL, model);
		g_tgt.veh = veh;
		g_tgt.mode = 1;
		g_tgt.dmin = mn;
		g_tgt.dmax = mx;
		snprintf(g_tgt.name, sizeof(g_tgt.name), "%s", dn ? dn : "?");
		SetVisible(veh, false);
		ResetGtaWheels(veh);
		g_attachedNew.store(true);
		const float bw = g_modelSize[0].load(), bl = g_modelSize[1].load(), bh = g_modelSize[2].load();
		Log("attached to GTA car %d (%s): GTA size %.2f wide x %.2f long x %.2f tall (model box min %.2f %.2f %.2f, max %.2f %.2f %.2f); BeamNG car %.2f x %.2f x %.2f",
			veh, g_tgt.name, mx.x - mn.x, mx.y - mn.y, mx.z - mn.z, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, bw, bl, bh);
		if (!g_haveModel.load()) {
			Notify("D1: attached to %s, but no BeamNG car loaded yet. Shift+F11 exports it from BeamNG.", g_tgt.name);
		} else {
			Notify("D1: BeamNG car on %s (GTA %.1f m long, BeamNG %.1f m). F11 again: show GTA car too.", g_tgt.name, mx.y - mn.y, bl);
		}
	}

	void Detach(const char* why)
	{
		SetVisible(g_tgt.veh, true);
		UpdateDriver(0, false);
		Log("detached (%s)", why);
		g_tgt = Target{};
	}

	// Which axles are driven: bit 0 front, bit 1 rear (ini, else the BeamNG export, else rear)
	int DrivenAxles()
	{
		if (g_cfg.drivenWheels == 0) return 2;
		if (g_cfg.drivenWheels == 1) return 1;
		if (g_cfg.drivenWheels == 2) return 3;
		const int f = g_fileDriven.load();
		return f > 0 ? f : 2;
	}

	// world-shadow raycasts towards the sun. v1.9.3: asynchronous probes (START_SHAPE_TEST_LOS_PROBE),
	// 150 m: v1.9.1 used 5 "expensive synchronous" 250 m probes every 3rd tick, which can stall GTA's
	// script thread (a suspect for the small skips at speed).
	bool     g_worldBusy = false;  // v2.41: GTA's ground is streaming (its rays come first: fewer sun rays, no ride check)
	float    g_sunVis = 1.0f;
	float    g_sunVisTarget = 1.0f;
	uint32_t g_rayTick = 0;
	int      g_rayHandles[5] = {};
	int      g_rayPending = 0;
	uint32_t g_rayStartTick = 0;

	void SunRays(const CamSample& c, const double R[3], const double F[3], const double U[3], double dt)
	{
		const Sun sun = SunModel(c.hour);
		++g_rayTick;
		if (!g_cfg.worldShadows || sun.dayF < 0.02) {
			g_sunVisTarget = 1.0f;
			g_rayPending = 0;
		} else if (g_rayPending) {
			// collect the results of the last batch
			int done = 0, hits = 0, failed = 0;
			for (int i = 0; i < 5; ++i) {
				if (!g_rayHandles[i]) {
					++done;
					continue;
				}
				int       hit = 0, entity = 0;
				NVec3     end{}, normal{};
				const int st = Call<int>(N::GET_SHAPE_TEST_RESULT, g_rayHandles[i], &hit, &end, &normal, &entity);
				if (st == 2) {
					hits += hit ? 1 : 0;
					g_rayHandles[i] = 0;
					++done;
					--g_rayPending;
				} else if (st == 0) {
					g_rayHandles[i] = 0;
					++failed;
					--g_rayPending;
				}
			}
			(void)done;
			if (g_rayPending <= 0) {
				g_rayPending = 0;
				const int answered = 5 - failed;
				if (answered > 0) g_sunVisTarget = 1.0f - float(hits) / float(answered);
			} else if (g_rayTick - g_rayStartTick > 20) {
				g_rayPending = 0;  // give up on this batch
				memset(g_rayHandles, 0, sizeof(g_rayHandles));
			}
		} else if (g_rayTick % (g_worldBusy ? 9 : 3) == 0) {
			const double w = std::max(1.0f, g_modelSize[0].load()) * 0.4, l = std::max(2.0f, g_modelSize[1].load()) * 0.4;
			const double h = std::max(0.8f, g_modelSize[2].load() * 0.8f);
			const double pts[5][3] = { { 0, 0, h }, { 0, l, h * 0.75 }, { 0, -l, h * 0.75 }, { w, 0, h * 0.7 }, { -w, 0, h * 0.7 } };
			g_rayPending = 0;
			for (int i = 0; i < 5; ++i) {
				const auto&  q = pts[i];
				const double sx = c.ox + R[0] * q[0] + F[0] * q[1] + U[0] * q[2];
				const double sy = c.oy + R[1] * q[0] + F[1] * q[1] + U[1] * q[2];
				const double sz = c.oz + R[2] * q[0] + F[2] * q[1] + U[2] * q[2];
				g_rayHandles[i] = Call<int>(N::START_SHAPE_TEST_LOS_PROBE, float(sx), float(sy), float(sz), float(sx + sun.S[0] * 150.0), float(sy + sun.S[1] * 150.0),
					float(sz + sun.S[2] * 150.0), 1 | 16 | 256, g_tgt.veh, 7);
				g_rayPending += g_rayHandles[i] ? 1 : 0;
			}
			g_rayStartTick = g_rayTick;
		}
		g_sunVis += (g_sunVisTarget - g_sunVis) * float(std::min(1.0, dt * 4.0));
	}

	// v1.9.5: how far GTA's ground is below the drawn tyres (the v1.9.4 screenshots looked like the car
	// floats a little: the shadow starts away from the tyres). Four rays straight down at the tyres,
	// asynchronous, every 15 ticks; logged every 10 s (median). Measurement only, nothing is moved yet.
	int                 g_rideHandles[4] = {};
	double              g_rideOrg[3] = {}, g_rideUp[3] = {};  // the car when the rays were cast
	int                 g_ridePending = 0, g_rideTick = 0;
	std::vector<double> g_rideGaps;
	ULONGLONG           g_rideLogMs = 0;
	void RideCheck(const CamSample& c, const double R[3], const double F[3], const double U[3])
	{
		if (!g_cfg.rideCheck) return;
		++g_rideTick;
		const double w = std::max(1.0f, g_modelSize[0].load()) * 0.38, l = std::max(2.0f, g_modelSize[1].load()) * 0.30;
		const double pts[4][2] = { { -w, l }, { w, l }, { -w, -l }, { w, -l } };
		if (g_ridePending) {
			for (int i = 0; i < 4; ++i) {
				if (!g_rideHandles[i]) continue;
				int       hit = 0, entity = 0;
				NVec3     end{}, normal{};
				const int st = Call<int>(N::GET_SHAPE_TEST_RESULT, g_rideHandles[i], &hit, &end, &normal, &entity);
				if (st == 2 || st == 0) {
					if (st == 2 && hit) {
						const double d[3] = { end.x - g_rideOrg[0], end.y - g_rideOrg[1], end.z - g_rideOrg[2] };
						const double up = d[0] * g_rideUp[0] + d[1] * g_rideUp[1] + d[2] * g_rideUp[2];  // ground height above the tyre bottoms
						if (g_rideGaps.size() < 4000) g_rideGaps.push_back(-up);
					}
					g_rideHandles[i] = 0;
					--g_ridePending;
				}
			}
			if (g_ridePending <= 0 || g_rideTick > 40) g_ridePending = 0;
		} else if (g_rideTick >= 15 && !g_worldBusy) {
			g_rideTick = 0;
			g_rideOrg[0] = c.ox;
			g_rideOrg[1] = c.oy;
			g_rideOrg[2] = c.oz;
			for (int k = 0; k < 3; ++k) g_rideUp[k] = U[k];
			for (int i = 0; i < 4; ++i) {
				const double sx = c.ox + R[0] * pts[i][0] + F[0] * pts[i][1], sy = c.oy + R[1] * pts[i][0] + F[1] * pts[i][1], sz = c.oz + R[2] * pts[i][0] + F[2] * pts[i][1];
				g_rideHandles[i] = Call<int>(N::START_SHAPE_TEST_LOS_PROBE, float(sx + U[0] * 1.0), float(sy + U[1] * 1.0), float(sz + U[2] * 1.0), float(sx - U[0] * 1.5),
					float(sy - U[1] * 1.5), float(sz - U[2] * 1.5), 1, g_tgt.veh, 7);
				g_ridePending += g_rideHandles[i] ? 1 : 0;
			}
		}
		const ULONGLONG now = GetTickCount64();
		if (now - g_rideLogMs >= 10000) {
			g_rideLogMs = now;
			if (g_rideGaps.size() >= 8) {
				std::sort(g_rideGaps.begin(), g_rideGaps.end());
				const double med = g_rideGaps[g_rideGaps.size() / 2], lo = g_rideGaps[g_rideGaps.size() / 10], hi = g_rideGaps[g_rideGaps.size() * 9 / 10];
				Log("ride height: GTA's ground is %.1f cm %s the drawn tyres (median of %zu rays; 10%%-90%%: %.1f to %.1f cm; + = the car floats above GTA's ground)",
					fabs(med) * 100.0, med >= 0 ? "below" : "above", g_rideGaps.size(), lo * 100.0, hi * 100.0);
			}
			g_rideGaps.clear();
		}
	}

	// ---------------------------------------------------------------------------------------
	// v2.0 drive mode (T2), state. GTA's controls go to the BeamNG car (input.event in its vehicle Lua),
	// BeamNG sends the car's pose every frame, the plugin moves the hidden GTA car there (so GTA's camera,
	// streaming and minimap follow it) and draws the BeamNG car with BeamNG's own wheels (steering,
	// suspension, spin). The body is the exported mesh: it moves and tilts, but doesn't bend yet (D2).
	// ---------------------------------------------------------------------------------------
	struct DrvModel  // what drive mode needs from the car file (written by LoadCar, guarded by g_modelMx)
	{
		bool        ok = false;
		bool        frameOk = false;  // car-space axes = BeamNG's vehicle axes
		double      origin[3] = {};   // car-space origin in the vehicle frame (from the file)
		std::string originHow;
		std::string vehicle;
		int         nw = 0;
		std::string name[kDrvWheels];
		double      c[kDrvWheels][3] = {};
		double      radius[kDrvWheels] = {};
		std::vector<std::string> dgNames;  // v2.6: the car's breakable groups (glass, lights), as the car file's draws know them
	} g_drvModel;
	std::atomic<uint32_t> g_modelGen{ 0 };  // counts loaded car files

	struct DrvPose
	{
		double   st = 0, rt = 0;  // BeamNG time of this pose (v2.42: its simulation clock, DrivePoseClock=sim), local arrival time (s)
		double   ss = 0;          // v2.4: BeamNG's simulation clock (sum of its physics steps' time)
		double   fst = 0;         // v2.42: BeamNG's frame clock (sum of its frame times; the pose time before v2.42)
		bool     hasSs = false;
		uint32_t seq = 0;
		double   P[3] = {}, f[3] = {}, u[3] = {}, v[3] = {};  // vehicle position, forward, up (orthonormal), velocity (BeamNG world)
		bool     hasV = false;
		int      nw = 0;
		bool     spin = false;                // ang[] is the wheel's turn angle
		float    w[kDrvWheels][6] = {};       // hub centre (vehicle frame from P), axle (pointing right)
		double   ang[kDrvWheels] = {};        // turn angle about the axle (rad, counted on: not wrapped)
	};

	// receiver thread <-> script thread
	struct DrvNet
	{
		SOCKET            s = INVALID_SOCKET;
		int               port = 0;
		HANDLE            thread = nullptr;
		std::atomic<bool> stop{ false };
		SRWLOCK           lock = SRWLOCK_INIT;
		DrvPose           ring[kDrvRing];
		uint32_t          n = 0;    // poses stored since the last reset (ring index = n % kDrvRing)
		uint32_t          gen = 0;  // bumped on every reset
		std::string       reply;    // the last drive_* message
		uint32_t          replyCount = 0;
		std::string       vs;       // v2.01: the last "vs" (what the BeamNG car does)
		uint32_t          vsCount = 0;
		uint32_t          bad = 0, dropped = 0;
		// v2.4 (T3): BeamNG's answers about GTA's ground
		std::string       tp;       // the last tp_ok / tp_err
		uint32_t          tpCount = 0;
		std::string       ws;       // the last "ws" (ground stats, once a second)
		uint32_t          wsCount = 0;
		std::string       map;      // the last map_ok
		uint32_t          mapCount = 0;
		std::vector<std::string> acks;  // tiles BeamNG built (hf_ok ids) or refused ("!" + id)
		// v2.43: BeamNG paused ("pz" 5 times a second while its physics stand still): when, and whether you paused it there
		double            pzT = 0;
		bool              pzUser = false;
		uint32_t          pzCount = 0;
		std::string       rep;      // the last "rep" (your repair binding used in BeamNG)
		uint32_t          repCount = 0;
		std::string       eye;      // v2.72: the driver's eye, read in the car's own Lua (after drive_ok)
		uint32_t          eyeCount = 0;
		std::vector<std::string> dmQ;  // v2.5: "dm" messages (BeamNG's nodes that moved: dents), oldest first
		uint32_t          dmCount = 0, dmLost = 0;
		std::vector<std::string> pbkQ;  // v2.51: "pbk" (BeamNG's car broke a prop's box: knock it over in GTA)
		std::vector<std::string> ppQ;   // v2.52: physics props: "pp" poses, "pk" knocked, "pqf" one failed, "pqs" they work or not
		std::vector<std::string> twQ;   // v2.71: traffic twins: "twi" ready, "twu" poses, "twd" dents, "twf" failed
		std::vector<std::string> fxQ;   // v2.6: "fx" from BeamNG's car: hits it took, its fire, its explosion, its broken glass
	} g_dnet;

	struct DriveState
	{
		int      state = 0;  // 0 off, 1 waiting for BeamNG's answer, 2 driving
		int      veh = 0;
		uint32_t modelGen = 0;
		DrvModel m;
		double   tStart = 0, tLastStart = 0, tOk = 0;
		uint32_t replySeen = 0;
		uint32_t sendSeq = 0;
		// poses (copied from the receiver, oldest first)
		std::vector<DrvPose> poses;
		uint32_t gen = 0, taken = 0;
		uint32_t newPoses = 0;
		double   lastArrival = 0;
		bool     stale = false;
		// v2.42: how steady the drawn car moves at speed (its speed tick to tick vs BeamNG's), and the pose log
		double   wobX[3] = {};
		bool     wobOk = false;
		double   wobSum = 0, wobMax = 0;
		uint32_t wobN = 0;
		std::vector<float> wobAll;
		FILE*    plog = nullptr;
		uint32_t plogRows = 0;
		std::string vehB;
		int      nwB = 0;
		std::string nameB[kDrvWheels];
		int      map[kDrvWheels] = { -1, -1, -1, -1, -1, -1, -1, -1 };  // BeamNG wheel -> car-file wheel
		int      mapped = 0;
		// clock: local time = BeamNG time + off; drawn delay behind the newest
		bool     haveOff = false;
		double   off = 0;
		double   delay = 0.03;
		// GTA's frame clock: advances by GTA's frame time each tick (the steps GTA's world moves in), pulled
		// gently towards the real clock
		bool     gameInit = false;
		double   gameT = 0;
		uint32_t gameResets = 0;
		// car-space origin in the vehicle frame
		double   o[3] = {};
		int      oN = 0;
		double   oAcc[3] = {}, oMin[3] = { 1e9, 1e9, 1e9 }, oMax[3] = { -1e9, -1e9, -1e9 };
		bool     oDone = false;
		// BeamNG world -> GTA world: X_gta = Ag + Rz(th) (X_beam - Ab)
		bool     anchored = false;
		double   Ag[3] = {}, Ab[3] = {}, th = 0, cth = 1, sth = 0;
		double   lx = 0, ly = 0, lz = 0;  // GTA car origin -> drawn car origin (its model's footprint centre, bottom)
		// v2.71: the seat: your head in GTA's car (its frame, at F10) and BeamNG's driver eye (its vehicle frame, from drive_ok)
		double   headG[3] = {}, eyeB[3] = {};
		bool     headOk = false, eyeOk = false, seatDone = false;
		uint32_t eyeSeen = 0;
		// steering sign
		float    sign = 1.0f;
		bool     signLocked = false;
		int      signN = 0, signWrong = 0;
		float    lastLr = 0;
		bool     lastHb = false;
		// the GTA car's rotation: does GTA take what we set?
		bool     setValid = false;
		double   setF[3] = {}, setU[3] = {};
		int      rotMode = 0;  // 0 SET_ENTITY_ROTATION (order 2), 1 SET_ENTITY_QUATERNION
		int      rotChecks = 0, rotBad = 0;
		double   rotErrMax = 0;
		bool     rotFailed = false;
		bool     frozen = false;
		bool     foot = false;  // v2.61: you got out with drive mode on: BeamNG keeps its car parked
		// wheels for drawing (from the last interpolated pose, car space)
		bool     wHave = false;
		bool     wOk[kDrvWheels] = {};
		float    wC[kDrvWheels][3] = {}, wA[kDrvWheels][3] = {}, wAng[kDrvWheels] = {};
		double   wAngU[kDrvWheels] = {};  // v2.73: BeamNG's turn angle as sent (counted on, not wrapped: the tyre smoke's slip)
		bool     ang0Set[kDrvWheels] = {};
		double   ang0[kDrvWheels] = {};
		double   roll[kDrvWheels] = {};  // turn from the speed (when BeamNG has no spin data)
		double   lastTickT = 0;
		// stats (10 s)
		ULONGLONG logMs = 0;
		uint32_t ticks = 0, interp = 0, extrap = 0, held = 0, early = 0, rx = 0;
		double   delaySum = 0, ivLast = 0, speed = 0;
		float    thr = 0, brk = 0, str = 0;
		int      hb = 0, flt = 0;
		// v2.01: your BeamNG controls
		std::string inpHow;            // from drive_ok: "binds", "binds+fallback", "fallback"
		std::string fb;                // the parts BeamNG takes as plain values (thr, str, hb) instead of your bindings
		bool     padKnown = false;     // drive_ok listed the pad controls your bindings use
		bool     padBound[kPadCtls] = {};
		std::string keyBound;          // " up down x " (BeamNG key names your bindings use)
		int      nBinds = 0;
		bool     padGta = false;       // the pad's buttons go to GTA (PadToggle)
		uint32_t padSupp = 0;          // held when the mode changed or driving began: ignored until let go
		bool     toggleWas = false;
		uint32_t owner = 0;            // pad controls going to BeamNG this tick (bits)
		// v2.1: BeamNG's pad modifiers (customModifier1-3): combination-only controls go to BeamNG while one is held
		bool     padCombo[kPadCtls] = {};  // used only in combinations (padm)
		uint32_t comboMask[kPadCtls][6] = {};  // the modifier button sets each of them is used with (padmx)
		uint8_t  comboN[kPadCtls] = {};
		uint32_t padModBits = 0;           // the modifier buttons (pmod)
		bool     modHeld = false;          // a BeamNG modifier button is held now (BeamNG mode)
		uint32_t modMask = 0;              // which (held, usable, BeamNG's) modifier buttons
		WORD     sentPm = 0;               // v2.5: the modifier buttons sent as held (XInput bits)
		double   lastDrawNow = 0, slowS = 0, slowRatio = 1.0;  // v2.5: GTA's slow motion (its frame time under 70 % of real time, smoothed): seconds of it
		double   carO[3] = {}, carR[3] = {}, carF[3] = {}, carU[3] = {}, carV[3] = {};  // v2.5: the drawn car this tick (GTA world)
		bool     carPoseOk = false;
		uint32_t slowTicks = 0;
		uint32_t ownerPrev = 0;
		std::string plist;                 // which of your BeamNG pad lists BeamNG uses
		bool     engineOff = false;        // GTA's engine (sound) switched off for drive mode
		bool     engineWasOn = true;       // ... and whether it ran before
		double   vMx = -1, rpmShow = 0, rpmMaxSeen = 0, gaugeT = 0, speedAbs = 0;
		// what the BeamNG car does ("vs", 5 a second)
		uint32_t vsSeen = 0;
		bool     vsHave = false;
		double   vsT = 0, laT = 0;
		double   vThr = -1, vBrk = -1, vStr = 0, vPb = -1, vClu = -1, vRpm = -1, vWs = 0, vLi = -1;  // v2.3 vLi: BeamNG's lights (bits)
		std::string vGear, vIgn, vEng, vGbm, vLa;
		std::string vEsc;                  // v2.5: the car's drive mode (ESC configuration / drive mode), "-" none
		// input stats (10 s) and what went out last
		float    mxRt = 0, mxLt = 0, mxLx = 0;
		uint32_t ctlSeen = 0;          // pad controls that moved (bits)
		std::string keysSeen, keysNow;
		uint32_t sentOwner = 0;
		WORD     sentBtn = 0;
		float    sentLx = 0, sentLt = 0, sentRt = 0;
		bool     kbUsed = false, padUsed = false;
		int      lastDev = -1;         // what you drove with last: 0 keyboard, 1 pad (BeamNG's input filter)
		bool     signSkip = false;     // steering comes from your bindings: nothing to learn
		int      blocked = 0;          // GTA controls switched off this tick
		// v2.3
		bool     sw = false;           // BeamNG switched its "mute when unfocused" off for this drive: wake its window
		bool     delayInit = false;
		double   fX[3] = {};           // the filtered car position (BeamNG world), DriveMotion=filter
		bool     fOk = false;
		uint32_t fSnaps = 0;
		double   fErrSum = 0, fErrMax = 0;
		uint32_t fErrN = 0;
		double   tnSum = 0, tnMax = 0; // BeamNG pose timing noise: position vs (previous + speed x time), metres
		uint32_t tnN = 0;
		uint32_t gaps = 0;             // poses that came more than 2.5 BeamNG frames after the one before
		double   gapMax = 0, delayLo = 1e9, delayHi = 0;
		bool     radioOn = false, radioBtnWas = false, radioHeldDone = false;
		double   radioDownT = 0;
		uint32_t actWas = 0;           // PadGtaActions buttons held last tick
		uint32_t handled = 0;          // pad controls the plugin handles itself this tick (GTA's reaction off)
		uint32_t actSent = 0;
		// v2.4
		bool     bindsMode = false;    // BeamNG runs your bindings (else GTA's basic values: the v2.0 way)
		bool     radioWasOff = false;  // GTA's radio was off at F10: it stays off
		int      radioWas = 255;
		double   tnSum2 = 0, tnMax2 = 0;  // BeamNG pose timing noise against its simulation clock (P1)
		uint32_t tnN2 = 0;
		double   poseMinSt = -1e30;    // poses older than this (BeamNG time) are from before the teleport
		bool     tpSent = false;       // state 3: the teleport request went out
		double   tpT = 0, tpFirstT = 0;
		uint32_t tpSeen = 0;
		const char* tpWhy = "start";
		bool     fixedMap = false;     // the fixed GTA <-> BeamNG mapping (World=1) instead of v2.3's anchor
		double   lastV[3] = {};        // the last speed handed to the GTA car (GTA world), for F10 off
		bool     lastVOk = false;
		double   goodT = 0;            // the last pose standing on GTA's ground (for the fall-through safety)
		bool     goodOk = false;
		double   goodX[3] = {}, goodF[3] = {}, goodU[3] = {};
		uint32_t falls = 0;
		double   fallT = 0;
		double   underT = 0;           // v2.62: since when BeamNG's car is under GTA's ground (0: it isn't)
		int      underLogs = 0;
		double   tpX[3] = {}, tpF[3] = {}, tpU[3] = {};  // where the last teleport put the car
		int      setupNote = 0;        // state 3 messages shown
		// v2.41
		bool     puppet = false;       // the GTA car is the hidden puppet (no collision, no gravity): F10 off lands it
		std::string radioName;         // GTA's radio station at F10
		std::string radioWant;         // the phone radio is kept on this station for a few seconds (it starts one by itself)
		double   radioEnfUntil = 0;
		uint32_t radioChk = 0, radioFixes = 0;
		bool     radioOffHeld = false; // the radio was off at F10: no phone radio until you press GTA's radio button
		double   tickT = 0;            // the last DriveTick (a gap = GTA was paused)
		uint32_t holdsSeen = 0;
		// v2.43: pause sync
		bool     beamPaused = false, beamPausedUser = false;  // BeamNG's physics stand still (you paused it there)
		bool     beamPauseSeen = false;  // this BeamNG pause already opened GTA's pause menu once
		bool     menuByBeam = false;     // GTA's pause menu was opened because you paused BeamNG
		double   gtaPausedT = -1e9;      // v2.5: last tick GTA was paused
		double   beamPauseT0 = 0;
		uint32_t clockJumps = 0, repSeen = 0;
		std::vector<double> fallLog;   // when the falls were (3 in 30 s end drive mode)
		double   fallGraceT = 0;       // no fall check until then (just put back)
	} g_drv;
	float g_drvLearnedSign = 0.0f;  // a steering sign learned earlier in this game session (0 = not yet)
	std::atomic<int> g_exclusiveFs{ -1 };  // v2.3: GTA in exclusive fullscreen (1), windowed/borderless (0), unknown (-1)

	// Fill the car part of a camera sample from the target GTA car.
	void SampleCar(CamSample& c)
	{
		c.carOn = false;
		if (!g_tgt.mode) {
			return;
		}
		if (!Call<int>(N::DOES_ENTITY_EXIST, g_tgt.veh)) {
			g_tgt = Target{};
			Notify("D1: the GTA car is gone; detached.");
			return;
		}
		const NVec3 p = Call<NVec3>(N::GET_ENTITY_COORDS, g_tgt.veh, 0);
		// axes from points 10 m along each axis (no memory reads; good precision far from the map centre)
		const NVec3 px = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS, g_tgt.veh, 10.0f, 0.0f, 0.0f);
		const NVec3 py = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS, g_tgt.veh, 0.0f, 10.0f, 0.0f);
		const NVec3 pz = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS, g_tgt.veh, 0.0f, 0.0f, 10.0f);
		double R[3] = { (px.x - p.x) * 0.1, (px.y - p.y) * 0.1, (px.z - p.z) * 0.1 };
		double F[3] = { (py.x - p.x) * 0.1, (py.y - p.y) * 0.1, (py.z - p.z) * 0.1 };
		double U[3] = { (pz.x - p.x) * 0.1, (pz.y - p.y) * 0.1, (pz.z - p.z) * 0.1 };
		// where the BeamNG car's origin goes: the GTA model's footprint centre at its lowest point
		// (v2.0 drive mode: the GTA car is put where BeamNG has the car, so no ini offsets/turn then)
		const bool   drv = g_drv.state == 2;
		// (v2.71: in drive mode the offset the car is put with: the seat moves it)
		const double lx = drv ? g_drv.lx : (g_tgt.dmin.x + g_tgt.dmax.x) * 0.5 + g_cfg.offX;
		const double ly = drv ? g_drv.ly : (g_tgt.dmin.y + g_tgt.dmax.y) * 0.5 + g_cfg.offY;
		const double lz = drv ? g_drv.lz : g_tgt.dmin.z + g_cfg.offZ;
		c.ox = p.x + R[0] * lx + F[0] * ly + U[0] * lz;
		c.oy = p.y + R[1] * lx + F[1] * ly + U[1] * lz;
		c.oz = p.z + R[2] * lx + F[2] * ly + U[2] * lz;
		// optional turn of the drawn car about its up axis
		const double yawT = drv ? 0.0 : g_cfg.yawDeg;
		const double cy = cos(yawT * kDeg), sy = sin(yawT * kDeg);
		for (int k = 0; k < 3; ++k) {
			c.R[k] = float(R[k] * cy + F[k] * sy);
			c.F[k] = float(-R[k] * sy + F[k] * cy);
			c.U[k] = float(U[k]);
		}
		const ULONGLONG nowMs = GetTickCount64();
		const double    nowT = NowSeconds();
		const double    dt = g_wheelT > 0 ? std::min(0.1, nowT - g_wheelT) : 0.0;
		g_wheelT = nowT;

		// the sun blocked by GTA's world?
		SunRays(c, R, F, U, dt);
		RideCheck(c, R, F, U);
		c.sunVis = g_sunVis;

		// ---- wheels, speed model (always kept up to date: the fallback)
		const NVec3  v = Call<NVec3>(N::GET_ENTITY_SPEED_VECTOR, g_tgt.veh, 1);
		for (int k = 0; k < 3; ++k) g_carVelW[k] = R[k] * v.x + F[k] * v.y + U[k] * v.z;  // v1.9.4: world velocity (camera sync)
		if (drv) {
			// v2.0 drive mode: the wheels come from BeamNG (DriveTick fills them in); GTA's hidden car's own
			// wheels don't turn and the speed model isn't needed
			c.gtaWheels = false;
			c.distFront = g_distFront;
			c.distRear = g_distRear;
			c.steerDeg = 0;
			c.carOn = true;
			return;
		}
		const double ground = -double(v.x) * sy + double(v.y) * cy;
		const int    ped = Call<int>(N::PLAYER_PED_ID);
		const bool   driver = Call<int>(N::GET_PED_IN_VEHICLE_SEAT, g_tgt.veh, -1, 0) == ped;
		const bool   burnout = Call<int>(N::IS_VEHICLE_IN_BURNOUT, g_tgt.veh) != 0;
		float        throttle = 0, handbrake = 0;
		if (driver) {
			const float input = Call<float>(N::GET_CONTROL_NORMAL, 0, kInputVehMoveLR);  // -1 left, +1 right
			const float target = -input * g_cfg.maxSteer * g_cfg.steerSign;
			g_steer += (target - g_steer) * float(std::min(1.0, dt * 8.0));
			throttle = Call<float>(N::GET_CONTROL_NORMAL, 0, kInputVehAccelerate);
			handbrake = Call<float>(N::GET_CONTROL_NORMAL, 0, kInputVehHandbrake);
		}
		if (burnout != g_wasBurnout) {
			g_wasBurnout = burnout;
			Log("wheels: burnout %s (speed %.1f m/s)", burnout ? "started" : "ended", ground);
		}
		if (ground > 1.0) g_burnDir = 1;
		else if (ground < -1.5) g_burnDir = -1;
		const int    axles = DrivenAxles();
		const double spinUp = std::min(1.0, dt * 3.0), spinDown = std::min(1.0, dt * 5.0);
		auto axle = [&](bool rear, double& extra, double& dist) {
			const bool driven = (axles & (rear ? 2 : 1)) != 0;
			double     want = 0.0;  // extra surface speed wanted
			if (burnout && driven) {
				// v1.9 bug: at speeds above BurnoutWheelSpeed the wheels turned slower than the car moved.
				// v1.9.1 bug: at a standstill the direction followed the car's tiny speed jitter (+/- 0.0x m/s),
				// so the wheels flipped forwards/backwards. A burnout spins forwards unless clearly reversing.
				const double surf = std::max(double(g_cfg.burnoutSpeed) * std::clamp(double(throttle), 0.6, 1.0), fabs(ground) + 4.0);
				want = (g_burnDir >= 0 ? surf : -surf) - ground;
			} else if (rear && driver && g_cfg.handbrakeLocks && handbrake > 0.5 && !burnout) {
				want = -ground;  // locked
			}
			const double k = fabs(want) > fabs(extra) ? spinUp : spinDown;
			if (rear && want == -ground && handbrake > 0.5) {
				extra = want;  // a handbrake locks the wheels at once
			} else {
				extra += (want - extra) * k;
			}
			dist += (ground + extra) * dt;
		};
		axle(false, g_extraFront, g_distFront);
		axle(true, g_extraRear, g_distRear);
		c.distFront = g_distFront;
		c.distRear = g_distRear;
		c.steerDeg = g_steer;

		// ---- wheels from GTA's own wheel bones
		c.gtaWheels = false;
		const bool wantGta = g_cfg.wheels && g_cfg.wheelSource != 0;
		bool       anyBone = false;
		float      rot[4][3] = {};
		NVec3      bpos[4] = {};
		if (wantGta) {
			for (int g = 0; g < 4; ++g) {
				if (g_gw[g].bone < 0) continue;
				anyBone = true;
				const NVec3 r = Call<NVec3>(N::GET_ENTITY_BONE_ROTATION, g_tgt.veh, g_gw[g].bone);
				rot[g][0] = r.x;
				rot[g][1] = r.y;
				rot[g][2] = r.z;
				bpos[g] = Call<NVec3>(N::GET_WORLD_POSITION_OF_ENTITY_BONE, g_tgt.veh, g_gw[g].bone);
				// height of the hub in the car's frame: suspension (vs. a slow average) and tyre radius
				const double h = (bpos[g].x - p.x) * U[0] + (bpos[g].y - p.y) * U[1] + (bpos[g].z - p.z) * U[2];
				GtaWheel&    w = g_gw[g];
				if (!w.haveRest) {
					w.rest = h;
					w.haveRest = true;
					w.radius = std::clamp(h - double(g_tgt.dmin.z), 0.2, 0.7);
				}
				w.rest += (h - w.rest) * std::min(1.0, dt * 0.5);
				w.radius += (std::clamp(w.rest - double(g_tgt.dmin.z), 0.2, 0.7) - w.radius) * std::min(1.0, dt * 0.2);
				w.travel = g_cfg.suspension ? std::clamp(h - w.rest, -0.15, 0.15) : 0.0;
			}
		}
		if (wantGta && anyBone && !g_wcal.done && !g_wcal.failed) {
			if (fabs(ground) > 1.5) {
				WheelSample s{};
				memcpy(s.rot, rot, sizeof(rot));
				memcpy(s.R, R, sizeof(R));
				memcpy(s.F, F, sizeof(F));
				memcpy(s.U, U, sizeof(U));
				s.ground = ground;
				s.dt = dt;
				s.burnout = burnout || handbrake > 0.5f;
				g_wcal.samples.push_back(s);
				if (g_wcal.samples.size() >= 90) {
					DecideWheelCal();
				}
			}
			if (!g_wheelMsgShown && g_wcal.samples.size() == 1) {
				Log("wheels: learning how GTA's wheel bones turn (drive a few metres)");
			}
		}
		if ((g_wcal.done || g_wcal.failed) && !g_wheelMsgShown) {
			g_wheelMsgShown = true;
			if (g_wcal.done) {
				Notify("D1 wheels: following GTA's own wheels (spin, steering, suspension).");
			} else {
				Notify("D1 wheels: using the speed model (%s).", g_wcal.why);
				Log("wheels: falling back to the speed model: %s", g_wcal.why);
			}
		}
		if (wantGta && g_wcal.done) {
			for (int g = 0; g < 4; ++g) {
				GtaWheel& w = g_gw[g];
				if (w.bone < 0) continue;
				double       steer, ang;
				const double LR[3] = { 1, 0, 0 }, LF[3] = { 0, 1, 0 }, LU[3] = { 0, 0, 1 };
				DecodeWheel(g_wcal.order, g_wcal.axis, rot[g], g_wcal.local ? LR : R, g_wcal.local ? LF : F, g_wcal.local ? LU : U, steer, ang);
				if (w.havePrev && dt > 0) {
					// The bone angle only says where the wheel is within one turn. At speed a wheel turns
					// more than half a turn between two script ticks (55 m/s = 2.7 rad per tick at 60 fps),
					// so pick the whole number of turns that best matches what we expect: rolling with the
					// ground, or (burnout, sliding) the tyre speed we saw last tick.
					const bool   slipping = burnout || handbrake > 0.5f;
					const double expect = (slipping ? w.rate : ground) * dt / w.radius;  // radians, forward = +
					double       d = WrapPi(ang - w.prevAng) * w.sign;
					d += 2 * kPi * std::round((expect - d) / (2 * kPi));
					w.dist += d * w.radius;
					w.rate = d * w.radius / dt;
					w.speed += (w.rate - w.speed) * std::min(1.0, dt * 8.0);
				} else if (!w.havePrev) {
					w.rate = w.speed = ground;
				}
				w.prevAng = ang;
				w.havePrev = true;
				w.steer = steer;
				if (g >= 2) {
					w.steerAvg += (fabs(steer) - w.steerAvg) * std::min(1.0, dt * 0.7);
				}
			}
			// Rear wheels don't steer. If they seem to, the reading was learned wrongly (e.g. while
			// heading straight north, where world and car-relative rotations look alike): learn again.
			const double rearSteer = std::max(g_gw[2].bone >= 0 ? g_gw[2].steerAvg : 0.0, g_gw[3].bone >= 0 ? g_gw[3].steerAvg : 0.0);
			if (rearSteer > 12.0 * kDeg && g_wheelRelearns < 3) {
				++g_wheelRelearns;
				Log("wheels: rear wheels read as steered %.0f deg; learning the wheel bones again (%d of 3)", rearSteer / kDeg, g_wheelRelearns);
				g_wcal = WheelCal{};
				for (GtaWheel& w : g_gw) {
					w.havePrev = false;
					w.steerAvg = 0;
				}
			}
		}
		if (wantGta && g_wcal.done) {
			c.gtaWheels = true;
			for (int g = 0; g < 4; ++g) {
				const GtaWheel& w = g_gw[g];
				c.gDist[g] = w.dist;
				c.gSteer[g] = float(w.steer);
				c.gTravel[g] = float(w.travel);
			}
			if (nowMs - g_wcal.lastLogMs > 10000) {
				g_wcal.lastLogMs = nowMs;
				Log("wheels: GTA tyre speeds lf %.1f rf %.1f lr %.1f rr %.1f m/s (car %.1f), steer %.1f %.1f deg, travel %.3f %.3f %.3f %.3f m", g_gw[0].speed, g_gw[1].speed,
					g_gw[2].speed, g_gw[3].speed, ground, g_gw[0].steer / kDeg, g_gw[1].steer / kDeg, g_gw[0].travel, g_gw[1].travel, g_gw[2].travel, g_gw[3].travel);
			}
		}
		c.carOn = true;
	}

	// ---------------------------------------------------------------------------------------
	// Worker: ask BeamNG for an export over UDP, load the file
	// ---------------------------------------------------------------------------------------

	// Minimal value lookup in a flat JSON reply: "key":"string" or "key":number.
	bool JsonStr(const std::string& js, const char* key, std::string& out)
	{
		const std::string k = std::string("\"") + key + "\"";
		size_t            p = js.find(k);
		if (p == std::string::npos) {
			return false;
		}
		p = js.find(':', p + k.size());
		if (p == std::string::npos) {
			return false;
		}
		++p;
		while (p < js.size() && (js[p] == ' ' || js[p] == '\t')) {
			++p;
		}
		if (p >= js.size() || js[p] != '"') {
			return false;
		}
		++p;
		out.clear();
		while (p < js.size() && js[p] != '"') {
			if (js[p] == '\\' && p + 1 < js.size()) {
				++p;
				const char e = js[p];
				out += e == 'n' ? '\n' : e == 't' ? '\t' : e == '/' ? '/' : e;
			} else {
				out += js[p];
			}
			++p;
		}
		return true;
	}

	double JsonNum(const std::string& js, const char* key, double def)
	{
		const std::string k = std::string("\"") + key + "\"";
		size_t            p = js.find(k);
		if (p == std::string::npos) {
			return def;
		}
		p = js.find(':', p + k.size());
		if (p == std::string::npos) {
			return def;
		}
		return atof(js.c_str() + p + 1);
	}

	bool FileExists(const char* path)
	{
		const DWORD a = GetFileAttributesA(path);
		return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
	}

	// CarFile from the ini, else where the BeamNG mod writes it (verified on the user's PC in D1)
	std::string DefaultCarPath()
	{
		if (g_cfg.carFile[0]) {
			return g_cfg.carFile;
		}
		char la[MAX_PATH] = {};
		if (GetEnvironmentVariableA("LOCALAPPDATA", la, sizeof(la)) == 0) {
			return std::string();
		}
		return std::string(la) + "\\BeamNG\\BeamNG.drive\\current\\gtabridge\\car.gltf";
	}

	// ---------------------------------------------------------------------------------------
	// v2.5 (D2): dents. BeamNG's car sends how far its nodes moved since the drive started, in the car's own frame
	// (driving and turning don't count, only bending): "dm" messages, each one for a range of node ids. The car
	// file says which triangle of nodes each vertex sits in (carmodel.cpp, v2.53), and the render thread copies the
	// moves to the video card (DentsUpload), where the shader puts every vertex back in its moved triangle.
	// ---------------------------------------------------------------------------------------

	struct DentModel
	{
		uint32_t                           gen = 0;   // g_modelGen of the car file
		std::vector<int>                   cid;       // BeamNG's node id per node
		std::vector<float>                 rest;      // xyz per node (car space, at export)
		std::vector<uint8_t>               wheel;     // a wheel's node (turns with the wheel: its moves are left out)
		std::vector<int>                   order;     // nodes sorted by cid
		std::vector<int>                   sortedCid;
		std::vector<int>                   idxOfCid;  // cid -> node (-1: none)
		int                                skinned = 0;
		std::string                        note;
	};
	std::shared_ptr<DentModel> g_dentModelNew;  // set by LoadCar (under g_modelMx), taken by DentsTick

	struct DentShared  // script thread -> render thread
	{
		SRWLOCK            lock = SRWLOCK_INIT;
		std::vector<float> data;  // per node: how far it moved (xyz, 0), where it is on the car (xyz, 0)
		uint32_t           gen = 0;
		uint64_t           seq = 0;
	} g_dentShared;
	std::atomic<uint32_t> g_dentUploads{ 0 };

	struct DentState
	{
		std::shared_ptr<DentModel> m;
		std::vector<float>         off, tgt, nv;  // drawn (eased), BeamNG's newest, a message's
		double                     easeT = 0;
		std::vector<uint32_t>      stamp;
		uint32_t                   stampNow = 0;
		bool                       any = false;      // a node is moved now (published)
		bool                       pending = false;  // changes not published yet
		double                     statT = 0;
		uint32_t                   msgs = 0, entries = 0, unknown = 0, bad = 0, pubs = 0, wrongCar = 0;
		uint32_t                   lostSeen = 0;
		bool                       warnedNodes = false, warnedNone = false, firstMove = false;
		int                        changeLogs = 0, movedLogged = 0, movedNow = 0;
		double                     mostLogged = 0, mostNow = 0, lastChangeT = 0;
		bool                       settleCheck = false;
	} g_dent;

	// the dents data of the car file (LoadCar, on the loader thread)
	std::shared_ptr<DentModel> DentModelOf(const car::Model& model)
	{
		auto         dm = std::make_shared<DentModel>();
		const size_t n = model.nodeCid.size();
		dm->note = model.skinNote;
		if (model.skinned <= 0 || n == 0 || model.nodePos.size() != n * 3) return dm;
		int mx = 0;
		for (int c : model.nodeCid) mx = std::max(mx, c);
		if (mx > 4000000) {
			dm->note = "node ids too large";
			return dm;
		}
		dm->cid = model.nodeCid;
		dm->rest = model.nodePos;
		dm->wheel = model.nodeWheel;
		dm->wheel.resize(n, 0);
		dm->idxOfCid.assign(size_t(mx) + 1, -1);
		for (size_t i = 0; i < n; ++i)
			if (dm->cid[i] >= 0) dm->idxOfCid[size_t(dm->cid[i])] = int(i);
		dm->order.resize(n);
		for (size_t i = 0; i < n; ++i) dm->order[i] = int(i);
		std::sort(dm->order.begin(), dm->order.end(), [&](int x, int y) { return dm->cid[x] < dm->cid[y]; });
		dm->sortedCid.resize(n);
		for (size_t k = 0; k < n; ++k) dm->sortedCid[k] = dm->cid[size_t(dm->order[k])];
		dm->skinned = model.skinned;
		return dm;
	}

	// script thread, every tick: BeamNG's "dm" messages -> node moves -> g_dentShared
	void DentsTick()
	{
		DentState& d = g_dent;
		{
			std::shared_ptr<DentModel> nm;
			{
				std::lock_guard<std::mutex> lk(g_modelMx);
				nm = std::move(g_dentModelNew);
			}
			if (nm) {
				// a newly loaded car: nothing moved yet
				d.m = std::move(nm);
				const size_t n = d.m->cid.size();
				d.off.assign(n * 3, 0.0f);
				d.tgt.assign(n * 3, 0.0f);
				d.nv.assign(n * 3, 0.0f);
				d.stamp.assign(n, 0);
				d.stampNow = 0;
				d.any = false;
				d.pending = true;
				d.warnedNodes = d.warnedNone = d.firstMove = false;
			}
		}
		std::vector<std::string> q;
		uint32_t                 lost = 0;
		AcquireSRWLockExclusive(&g_dnet.lock);
		q.swap(g_dnet.dmQ);
		lost = g_dnet.dmLost;
		ReleaseSRWLockExclusive(&g_dnet.lock);
		if (!d.m) return;
		DentModel&   m = *d.m;
		const size_t n = m.cid.size();
		if (n == 0) {
			if (!q.empty() && !d.warnedNone) {
				d.warnedNone = true;
				Log("dents: BeamNG sends the car's dents, but the loaded car file can't show them: %s", m.note.empty() ? "no node data" : m.note.c_str());
				Notify("D1: export the car again (Shift+F11) to see its dents in GTA");
			}
			return;
		}
		const double now = NowSeconds();
		bool         changed = false;
		if (!g_cfg.dents) {
			// switched off (ini): the car as drawn
			if (d.any) {
				std::fill(d.off.begin(), d.off.end(), 0.0f);
				std::fill(d.tgt.begin(), d.tgt.end(), 0.0f);
				d.any = false;
				d.pending = true;
			}
			q.clear();
		}
		for (const std::string& s : q) {
			++d.msgs;
			const double nn = JsonNum(s, "nn", -1), a = JsonNum(s, "a", -1), b = JsonNum(s, "b", -1);
			if (nn != double(n)) {
				++d.wrongCar;
				if (!d.warnedNodes) {
					d.warnedNodes = true;
					Log("dents: BeamNG's car has %.0f nodes, the car file %zu: it's another car or another setup of it. Export it again (Shift+F11) to see its dents",
						nn, n);
					Notify("D1: the BeamNG car isn't the exported one (export again: Shift+F11) - no dents shown");
				}
				continue;
			}
			if (!(a >= 0) || !(b > a)) {
				++d.bad;
				continue;
			}
			// the new moves of this range: listed nodes, all others in it back to where they were
			++d.stampNow;
			const size_t p0 = s.find("\"d\":\"");
			if (p0 != std::string::npos) {
				const char* p = s.c_str() + p0 + 5;
				while (*p && *p != '"') {
					char*      e = nullptr;
					const long c = strtol(p, &e, 10);
					if (e == p || *e != ',') break;
					p = e + 1;
					const long x = strtol(p, &e, 10);
					if (e == p || *e != ',') break;
					p = e + 1;
					const long y = strtol(p, &e, 10);
					if (e == p || *e != ',') break;
					p = e + 1;
					const long z = strtol(p, &e, 10);
					if (e == p) break;
					p = e;
					if (*p == ';') ++p;
					if (c < long(a) || c >= long(b)) {
						++d.bad;
						continue;
					}
					const int i = (c >= 0 && size_t(c) < m.idxOfCid.size()) ? m.idxOfCid[size_t(c)] : -1;
					if (i < 0) {
						++d.unknown;
						continue;
					}
					if (m.wheel[size_t(i)]) continue;  // (a wheel's node: the wheels are drawn on their own)
					d.nv[size_t(i) * 3] = float(x) * 0.001f;
					d.nv[size_t(i) * 3 + 1] = float(y) * 0.001f;
					d.nv[size_t(i) * 3 + 2] = float(z) * 0.001f;
					d.stamp[size_t(i)] = d.stampNow;
					++d.entries;
				}
			}
			const size_t k0 = size_t(std::lower_bound(m.sortedCid.begin(), m.sortedCid.end(), int(a)) - m.sortedCid.begin());
			const size_t k1 = size_t(std::lower_bound(m.sortedCid.begin(), m.sortedCid.end(), int(b)) - m.sortedCid.begin());
			for (size_t k = k0; k < k1; ++k) {
				const size_t i = size_t(m.order[k]);
				const bool   set = d.stamp[i] == d.stampNow;
				for (int c = 0; c < 3; ++c) {
					const float v = set ? d.nv[i * 3 + c] : 0.0f;
					if (fabsf(v - d.tgt[i * 3 + c]) > 0.0005f) {
						d.tgt[i * 3 + c] = v;
						changed = true;
					}
				}
			}
		}
		if (changed) {
			bool   any = false;
			int    movedNow = 0;
			double most = 0;
			for (size_t i = 0; i < n; ++i) {
				const float* o = &d.tgt[i * 3];
				const double l = sqrt(double(o[0]) * o[0] + double(o[1]) * o[1] + double(o[2]) * o[2]);
				if (l > 0) {
					any = true;
					++movedNow;
					most = std::max(most, l);
				}
			}
			if (any && !d.firstMove) {
				d.firstMove = true;
				Log("dents: the first ones from BeamNG: %d nodes moved, the most %.1f cm", movedNow, most * 100.0);
			}
			// (when it changes a lot: a crash, a repair; a few dozen lines at most)
			if ((any != d.any || movedNow > d.movedLogged * 2 + 10 || most > d.mostLogged + 0.1) && d.changeLogs < 40) {
				++d.changeLogs;
				if (any) Log("dents: now %d of %zu nodes moved, the most %.1f cm", movedNow, n, most * 100.0);
				else Log("dents: none now (the car was repaired)");
				d.movedLogged = movedNow;
				d.mostLogged = most;
			}
			d.lastChangeT = now;
			d.settleCheck = true;
			d.movedNow = movedNow;
			d.mostNow = most;
			d.any = any;
			d.pending = true;
		}
		if (d.settleCheck && now - d.lastChangeT > 1.0) {
			// settled (a second without changes): the final shape, when the last line said otherwise
			d.settleCheck = false;
			if (d.any && (d.movedNow != d.movedLogged || fabs(d.mostNow - d.mostLogged) > 0.01) && d.changeLogs < 60) {
				++d.changeLogs;
				Log("dents: now %d of %zu nodes moved, the most %.1f cm (settled)", d.movedNow, n, d.mostNow * 100.0);
				d.movedLogged = d.movedNow;
				d.mostLogged = d.mostNow;
			}
		}
		// v2.54: the drawn nodes ease onto BeamNG's every frame (BeamNG's DentsPerSecond came as steps: "the damaged parts
		// look lower fps than the rest", your 2.53 test)
		{
			const double dt = std::clamp(now - d.easeT, 0.0, 0.1);
			d.easeT = now;
			const float k = float(1.0 - exp(-dt / std::clamp(0.7 / double(g_cfg.dentHz), 0.015, 0.1)));
			bool        moving = false;
			for (size_t q = 0; q < n * 3; ++q) {
				const float e = d.tgt[q] - d.off[q];
				if (e == 0.0f) continue;
				if (fabsf(e) < 0.0003f || fabsf(e) > 3.0f) d.off[q] = d.tgt[q];  // (there; a part flung far: at once)
				else d.off[q] += e * k;
				moving = true;
			}
			if (moving) d.pending = true;
		}
		if (d.pending) {
			d.pending = false;
			AcquireSRWLockExclusive(&g_dentShared.lock);
			g_dentShared.data.assign(n * 8, 0.0f);
			float* o = g_dentShared.data.data();
			for (size_t i = 0; i < n; ++i) {
				for (int c = 0; c < 3; ++c) {
					o[i * 8 + c] = d.off[i * 3 + c];
					o[i * 8 + 4 + c] = m.rest[i * 3 + c];  // where it is on the car (the shader rebuilds each vertex's triangle)
				}
			}
			g_dentShared.gen = m.gen;
			++g_dentShared.seq;
			ReleaseSRWLockExclusive(&g_dentShared.lock);
			++d.pubs;
		}
		if (now - d.statT >= 10.0) {
			if (d.msgs > 0) {
				int    movedNow = 0;
				double most = 0;
				for (size_t i = 0; i < n; ++i) {
					const float* o = &d.tgt[i * 3];
					const double l = sqrt(double(o[0]) * o[0] + double(o[1]) * o[1] + double(o[2]) * o[2]);
					if (l > 0) ++movedNow, most = std::max(most, l);
				}
				Log("dents: %u messages from BeamNG (%u node moves, %u unknown nodes, %u bad, %u for another car, %u lost), %d of %zu nodes moved now (the most %.1f cm), "
				    "%u updates, %u copies to the video card",
					d.msgs, d.entries, d.unknown, d.bad, d.wrongCar, lost - d.lostSeen, movedNow, n, most * 100.0, d.pubs, g_dentUploads.load());
			}
			d.statT = now;
			d.msgs = d.entries = d.unknown = d.bad = d.pubs = d.wrongCar = 0;
			d.lostSeen = lost;
		}
	}

	void LoadCar(const std::string& path)
	{
		car::Options opt;
		opt.maxTextureSize = g_cfg.maxTex;
		opt.maxDataTextureSize = g_cfg.maxDataTex;
		opt.axes = g_cfg.meshAxes;
		opt.orient = g_cfg.meshOrient;
		opt.alphaCutDefault = g_cfg.alphaCut;
		opt.normalMaps = g_cfg.normalMaps;
		opt.surfaceMaps = g_cfg.surfaceMaps;
		opt.maskCheck = g_cfg.maskCheck;
		opt.compressed = g_cfg.texCompression;
		auto        model = std::make_unique<car::Model>();
		std::string err;
		Log("loading car file %s", path.c_str());
		const ULONGLONG t0 = GetTickCount64();
		const bool      ok = car::Load(path.c_str(), opt, *model, err, [](const char* s) { Log("%s", s); });
		if (!ok) {
			Notify("D1: could not load the car file: %s", err.c_str());
			Log("car load FAILED: %s", err.c_str());
			return;
		}
		const ULONGLONG ms = GetTickCount64() - t0;
		for (int k = 0; k < 3; ++k) {
			g_modelSize[k].store(model->size[k]);
		}
		// driven axles as the BeamNG mod found them (from the car's powertrain)
		int bits = 0, known = 0;
		for (const car::Wheel& w : model->wheels) {
			if (w.driven >= 0) {
				++known;
				if (w.driven) bits |= w.front ? 1 : 2;
			}
		}
		g_fileDriven.store(known ? bits : -1);
		Log("wheels: driven axles from the export: %s", !known ? "unknown (mod older than 1.9 or no powertrain data), using rear" : bits == 3 ? "front + rear" : bits == 1 ? "front" : bits == 2 ? "rear" : "none found");
		Notify("D1: car loaded (%.1f x %.1f m, %zu tris, %.0f MB textures, %.1f s). F11 puts it on a GTA car.", model->size[0], model->size[1],
			model->indices.size() / 3, model->textureBytes / 1048576.0, ms / 1000.0);
		if (!model->modVersion.empty() && model->modVersion < std::string("1.9.3")) {
			Notify("D1: car exported by BeamNG mod %s. Install mod 1.9.3 and press Shift+F11 (textures straight from BeamNG's files).", model->modVersion.c_str());
		}
		// v2.0: what drive mode needs from the file
		DrvModel dm;
		dm.ok = true;
		dm.frameOk = model->vehFrameOk;
		for (int k = 0; k < 3; ++k) dm.origin[k] = model->vehOrigin[k];
		dm.originHow = model->vehOriginHow;
		dm.vehicle = model->vehicle;
		dm.nw = std::min<int>(int(model->wheels.size()), kDrvWheels);
		for (int i = 0; i < dm.nw; ++i) {
			const car::Wheel& w = model->wheels[i];
			dm.name[i] = w.name;
			for (int k = 0; k < 3; ++k) dm.c[i][k] = w.c[k];
			dm.radius[i] = std::max(0.1f, w.radius);
		}
		if (model->wheels.size() > size_t(kDrvWheels)) Log("drive: the car has %zu wheels; drive mode uses the first %d", model->wheels.size(), kDrvWheels);
		dm.dgNames = model->dgNames;  // v2.6
		for (auto& b : g_dgBroken) b.store(0);
		std::shared_ptr<DentModel> dents = DentModelOf(*model);  // v2.5
		if (model->skinNote.find("export the car again") != std::string::npos) Notify("D1: export the car again (Shift+F11) for the 2.53 dents");
		std::lock_guard<std::mutex> lk(g_modelMx);
		g_lastCarPath = path;
		g_pendingModel = std::move(model);
		g_drvModel = dm;
		g_modelGen.fetch_add(1);
		dents->gen = g_modelGen.load();
		g_dentModelNew = std::move(dents);
	}

	void ExportAndLoad()
	{
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
			Notify("D1: network start failed");
			return;
		}
		SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET) {
			Notify("D1: could not open a UDP socket");
			WSACleanup();
			return;
		}
		sockaddr_in me{};
		me.sin_family = AF_INET;
		me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		me.sin_port = 0;
		bind(s, reinterpret_cast<sockaddr*>(&me), sizeof(me));
		DWORD tmo = 1000;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tmo), sizeof(tmo));
		sockaddr_in beam{};
		beam.sin_family = AF_INET;
		beam.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		beam.sin_port = htons(u_short(g_cfg.port));
		const char req[] = "{\"t\":\"export\"}";
		sendto(s, req, int(sizeof(req) - 1), 0, reinterpret_cast<sockaddr*>(&beam), sizeof(beam));
		Log("export: asked BeamNG on port %d", g_cfg.port);
		Notify("D1: asking BeamNG to export your car...");

		const ULONGLONG t0 = GetTickCount64();
		bool            started = false;
		std::string     reply;
		static char     buf[65536];
		while (GetTickCount64() - t0 < ULONGLONG(g_cfg.exportTimeout) * 1000) {
			const int n = recv(s, buf, sizeof(buf) - 1, 0);
			if (n <= 0) {
				if (!started && GetTickCount64() - t0 > 3000) {
					break;
				}
				continue;
			}
			buf[n] = 0;
			std::string msg(buf, n);
			std::string t;
			JsonStr(msg, "t", t);
			if (t == "export_started") {
				started = true;
				Notify("D1: BeamNG is exporting (the first time can take a while)...");
			} else if (t == "export_ok" || t == "export_err") {
				reply = msg;
				break;
			}
		}
		closesocket(s);
		WSACleanup();

		if (reply.empty()) {
			Notify(started ? "D1: BeamNG didn't finish the export in time." : "D1: no reply from BeamNG. Is it running with the mod, in a level?");
			Log("export: %s", started ? "timed out" : "no reply");
			return;
		}
		Log("export reply: %.900s", reply.c_str());
		std::string t;
		JsonStr(reply, "t", t);
		if (t == "export_err") {
			std::string e;
			JsonStr(reply, "err", e);
			Notify("D1: BeamNG export failed: %s", e.c_str());
			return;
		}
		std::string path;
		JsonStr(reply, "path", path);
		if (g_cfg.carFile[0]) {
			path = g_cfg.carFile;  // the ini wins
		}
		if (path.empty() || !FileExists(path.c_str())) {
			Notify("D1: export done but I can't find the file (%s). Set CarFile in GtaBeamD1.ini.", path.empty() ? "no path" : path.c_str());
			Log("export: file not found: '%s'", path.c_str());
			return;
		}
		Log("export: %.1f MB in %.1f s (BeamNG side %.0f ms)", JsonNum(reply, "bytes", 0) / 1048576.0, (GetTickCount64() - t0) / 1000.0,
			JsonNum(reply, "ms", 0));
		LoadCar(path);
	}

	enum class Job
	{
		Export,
		Reload
	};

	DWORD WINAPI WorkerMain(LPVOID p)
	{
		const Job job = static_cast<Job>(reinterpret_cast<uintptr_t>(p));
		if (job == Job::Export) {
			ExportAndLoad();
		} else {
			std::string path;
			{
				std::lock_guard<std::mutex> lk(g_modelMx);
				path = g_lastCarPath;
			}
			if (path.empty() || g_cfg.carFile[0]) {
				path = DefaultCarPath();
			}
			if (path.empty() || !FileExists(path.c_str())) {
				Notify("D1: no car file yet. Shift+F11 exports one from BeamNG.");
			} else {
				LoadCar(path);
			}
		}
		g_workerBusy.store(false);
		return 0;
	}

	void StartWorker(Job job)
	{
		bool expected = false;
		if (!g_workerBusy.compare_exchange_strong(expected, true)) {
			Notify("D1: busy with the last request, wait a moment.");
			return;
		}
		HANDLE h = CreateThread(nullptr, 0, WorkerMain, reinterpret_cast<LPVOID>(static_cast<uintptr_t>(job)), 0, nullptr);
		if (h) {
			CloseHandle(h);
		} else {
			g_workerBusy.store(false);
		}
	}

	void RunRayCheck(const CamSample& c)
	{
		double r[3], f[3], u[3];
		CamBasis(c.pitch, c.roll * g_cfg.rollSign, c.yaw, r, f, u);
		const float sx = float(c.px + f[0] * 0.5), sy = float(c.py + f[1] * 0.5), sz = float(c.pz + f[2] * 0.5);
		const float ex = float(c.px + f[0] * 400.0), ey = float(c.py + f[1] * 400.0), ez = float(c.pz + f[2] * 400.0);
		const int   ped = Call<int>(N::PLAYER_PED_ID);
		const int   ignore = g_tgt.mode == 1 ? g_tgt.veh : ped;  // the hidden car has collision but no pixels
		const int   handle = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, sx, sy, sz, ex, ey, ez, 1 | 2 | 16, ignore, 7);
		int         hit = 0, entity = 0;
		NVec3       end{}, normal{};
		const int   state = Call<int>(N::GET_SHAPE_TEST_RESULT, handle, &hit, &end, &normal, &entity);
		RayCheck    rc{};
		rc.ms = GetTickCount64();
		if (state == 2 && hit) {
			rc.viewZ = (end.x - c.px) * f[0] + (end.y - c.py) * f[1] + (end.z - c.pz) * f[2];
			rc.valid = rc.viewZ > 0.3;
		}
		AcquireSRWLockExclusive(&g_shared);
		g_ray = rc;
		ReleaseSRWLockExclusive(&g_shared);
	}

	// The minimap's place on screen (same formula as FiveM's GetMinimapAnchor, from the safe zone and
	// aspect ratio), so the car can be kept out of it.
	bool  g_hudOn = false;
	float g_hud[4] = {};

	void SampleHud()
	{
		const float sz = Call<float>(N::GET_SAFE_ZONE_SIZE);
		const float ar = std::max(0.5f, Call<float>(N::GET_ASPECT_RATIO, 0));
		const bool  on = !Call<int>(N::IS_HUD_HIDDEN) && !Call<int>(N::IS_RADAR_HIDDEN) && Call<int>(N::IS_RADAR_PREFERENCE_SWITCHED_ON);
		const float inset = 0.5f * fabsf(1.0f - sz);
		const float w = 1.0f / (4.0f * ar), h = 1.0f / 5.674f;
		const float r[4] = { inset, 1.0f - inset - h, inset + w, 1.0f - inset };
		static float lastLogged[4] = { -1, -1, -1, -1 };
		static int   lastOn = -1;
		if (int(on) != lastOn || fabsf(r[0] - lastLogged[0]) > 0.002f || fabsf(r[3] - lastLogged[3]) > 0.002f) {
			lastOn = on;
			memcpy(lastLogged, r, sizeof(r));
			Log("hud: minimap %s at %.3f %.3f - %.3f %.3f of the screen (safe zone %.3f, aspect %.3f, minimap rendering %d)", on ? "shown" : "hidden", r[0], r[1], r[2],
				r[3], sz, ar, Call<int>(N::IS_MINIMAP_RENDERING));
		}
		g_hudOn = on;
		memcpy(g_hud, r, sizeof(r));
	}

	// ---------------------------------------------------------------------------------------
	// v2.0 drive mode (T2): network, poses, the GTA car as a puppet
	// ---------------------------------------------------------------------------------------
#ifndef SIO_UDP_CONNRESET
#define SIO_UDP_CONNRESET _WSAIOW(IOC_VENDOR, 12)
#endif

	// where the value of "key" starts (after the colon and any spaces), or nullptr
	const char* JsonValue(const char* js, const char* key)
	{
		char k[48];
		snprintf(k, sizeof(k), "\"%s\"", key);
		const size_t kl = strlen(k);
		for (const char* p = strstr(js, k); p; p = strstr(p + 1, k)) {
			const char* q = p + kl;
			while (*q == ' ' || *q == '\t') ++q;
			if (*q != ':') continue;  // a string value that happens to read "key"
			++q;
			while (*q == ' ' || *q == '\t') ++q;
			return q;
		}
		return nullptr;
	}

	// "key":[n, n, ...] -> up to maxN numbers; -1 if the key is missing or a value isn't a number
	int JsonNums(const char* js, const char* key, double* out, int maxN)
	{
		const char* p = JsonValue(js, key);
		if (!p || *p != '[') return -1;
		++p;
		int n = 0;
		while (*p == ' ') ++p;
		while (*p && *p != ']') {
			char*        e = nullptr;
			const double v = strtod(p, &e);
			if (e == p || !std::isfinite(v)) return -1;
			if (n < maxN) out[n] = v;
			++n;
			p = e;
			while (*p == ' ') ++p;
			if (*p == ',') ++p;
			while (*p == ' ') ++p;
		}
		return *p == ']' ? std::min(n, maxN) : -1;
	}

	// "key":number
	bool JsonNumC(const char* js, const char* key, double& out)
	{
		const char* p = JsonValue(js, key);
		if (!p) return false;
		char*        e = nullptr;
		const double v = strtod(p, &e);
		if (e == p || !std::isfinite(v)) return false;
		out = v;
		return true;
	}

	// "key":["a","b",...]
	int JsonStrs(const std::string& js, const char* key, std::string* out, int maxN)
	{
		const char* v = JsonValue(js.c_str(), key);
		if (!v || *v != '[') return -1;
		size_t p = size_t(v - js.c_str()) + 1;
		int    n = 0;
		while (p < js.size() && js[p] != ']') {
			if (js[p] == '"') {
				std::string v;
				++p;
				while (p < js.size() && js[p] != '"') {
					if (js[p] == '\\' && p + 1 < js.size()) ++p;
					v += js[p++];
				}
				if (n < maxN) out[n] = v;
				++n;
			}
			++p;
		}
		return std::min(n, maxN);
	}

	void Norm3(double v[3])
	{
		const double l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
		if (l > 1e-9) {
			v[0] /= l;
			v[1] /= l;
			v[2] /= l;
		}
	}
	inline double Dot3(const double a[3], const double b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
	inline void   Cross3(const double a[3], const double b[3], double o[3])
	{
		const double t[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
		memcpy(o, t, sizeof(t));
	}
	// f normalised, u made perpendicular to it
	void Orthonormal(double f[3], double u[3])
	{
		Norm3(f);
		const double d = Dot3(f, u);
		for (int k = 0; k < 3; ++k) u[k] -= f[k] * d;
		Norm3(u);
	}

	bool ParsePose(const char* js, DrvPose& p)
	{
		double q = 0;
		if (!JsonNumC(js, "st", p.st)) return false;
		p.hasSs = JsonNumC(js, "ss", p.ss);  // v2.4
		// v2.42: the rubber band at speed. BeamNG's positions fit its simulation clock (your v2.41 log: 0.1 cm off on
		// average) but not its frame clock (6-10 cm, up to 64 cm at 80 mph): BeamNG's physics runs on its own, a few ms
		// off its frames. So the poses are timed by the simulation clock now.
		p.fst = p.st;
		if (p.hasSs && g_cfg.drivePoseClock == 1) p.st = p.ss;
		if (JsonNumC(js, "q", q)) p.seq = uint32_t(q);
		if (JsonNums(js, "p", p.P, 3) != 3 || JsonNums(js, "f", p.f, 3) != 3 || JsonNums(js, "u", p.u, 3) != 3) return false;
		p.hasV = JsonNums(js, "v", p.v, 3) == 3;
		if (!p.hasV) p.v[0] = p.v[1] = p.v[2] = 0;
		double sp = 0;
		p.spin = JsonNumC(js, "sp", sp) && sp > 0.5;
		double    w[kDrvWheels * 7];
		const int n = JsonNums(js, "w", w, kDrvWheels * 7);
		p.nw = n > 0 ? n / 7 : 0;
		for (int i = 0; i < p.nw; ++i) {
			for (int k = 0; k < 6; ++k) p.w[i][k] = float(w[i * 7 + k]);
			p.ang[i] = w[i * 7 + 6];
			double a[3] = { w[i * 7 + 3], w[i * 7 + 4], w[i * 7 + 5] };
			Norm3(a);
			for (int k = 0; k < 3; ++k) p.w[i][3 + k] = float(a[k]);
		}
		if (Dot3(p.f, p.f) < 0.25 || Dot3(p.u, p.u) < 0.25) return false;
		Orthonormal(p.f, p.u);
		return true;
	}

	// v2.41: alt-tab keeps the drive. GTA pauses when it loses focus (its script stops ticking); your v2.4 test: the BeamNG
	// mod heard nothing for 5 s and ended the drive. Now this thread (it runs while GTA is paused) tells the mod "hold"
	// twice a second while GTA's script is stopped: BeamNG's car brakes and waits, the drive stays.
	std::atomic<int>      g_holdOn{ 0 };    // drive mode on (any state)
	std::atomic<uint64_t> g_tickMs{ 0 };    // the last script tick (GetTickCount64)
	std::atomic<uint32_t> g_holdsSent{ 0 };

	DWORD WINAPI DrvRecvMain(LPVOID)
	{
		static char buf[65536];
		ULONGLONG   lastHold = 0;
		while (!g_dnet.stop.load()) {
			{
				const ULONGLONG nowMs = GetTickCount64();
				const uint64_t  tk = g_tickMs.load();
				if (g_holdOn.load() && tk && nowMs > tk + 400 && nowMs - lastHold >= 400) {
					lastHold = nowMs;
					char m[96];
					snprintf(m, sizeof(m), "{\"t\":\"hold\",\"ms\":%llu}", (unsigned long long)(nowMs - tk));
					send(g_dnet.s, m, int(strlen(m)), 0);
					g_holdsSent.fetch_add(1);
				}
			}
			const int n = recv(g_dnet.s, buf, int(sizeof(buf)) - 1, 0);
			if (n <= 0) {
				if (WSAGetLastError() != WSAETIMEDOUT) Sleep(20);
				continue;
			}
			const double t = NowSeconds();
			buf[n] = 0;
			if (!strncmp(buf, "{\"t\":\"pose\"", 11)) {
				DrvPose   p;
				const bool ok = ParsePose(buf, p);
				p.rt = t;
				AcquireSRWLockExclusive(&g_dnet.lock);
				if (!ok) {
					++g_dnet.bad;
				} else {
					const double last = g_dnet.n ? g_dnet.ring[(g_dnet.n - 1) % kDrvRing].st : -1e30;
					if (p.st < last - 1.0) {
						g_dnet.n = 0;  // BeamNG's clock started again (mod reloaded): a new stream
						++g_dnet.gen;
					}
					if (g_dnet.n && p.st <= g_dnet.ring[(g_dnet.n - 1) % kDrvRing].st) {
						++g_dnet.dropped;  // late or repeated
					} else {
						g_dnet.ring[g_dnet.n % kDrvRing] = p;
						++g_dnet.n;
					}
				}
				ReleaseSRWLockExclusive(&g_dnet.lock);
			} else {
				std::string s(buf, size_t(n)), ty;
				JsonStr(s, "t", ty);
				if (ty.compare(0, 6, "drive_") == 0) {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.reply = s;
					++g_dnet.replyCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "vs") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.vs = s;
					++g_dnet.vsCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "hf_ok" || ty == "hf_err") {
					// v2.4: a GTA ground tile built (or refused) in BeamNG
					std::string id;
					JsonStr(s, "id", id);
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.acks.size() < 4096) g_dnet.acks.push_back(ty == "hf_ok" ? id : "!" + id);
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "tp_ok" || ty == "tp_err") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.tp = s;
					++g_dnet.tpCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "ws") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.ws = s;
					++g_dnet.wsCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "pz") {
					// v2.43: BeamNG's physics are paused (by you there, or by the mod because GTA is)
					std::string by;
					JsonStr(s, "by", by);
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.pzT = NowSeconds();
					g_dnet.pzUser = by == "user";
					++g_dnet.pzCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "rep") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.rep = s;
					++g_dnet.repCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "eye") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.eye = s;
					++g_dnet.eyeCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "dm") {
					// v2.5: each one covers a range of node ids (all of them are kept: one car's dents come in several)
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.dmQ.size() >= 256) {
						g_dnet.dmQ.erase(g_dnet.dmQ.begin());
						++g_dnet.dmLost;
					}
					g_dnet.dmQ.push_back(std::move(s));
					++g_dnet.dmCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "pbk") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.pbkQ.size() < 256) g_dnet.pbkQ.push_back(std::move(s));
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "fx") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.fxQ.size() < 512) g_dnet.fxQ.push_back(std::move(s));
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "twu" || ty == "twd" || ty == "twi" || ty == "twf") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.twQ.size() < 1024) g_dnet.twQ.push_back(std::move(s));
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "pp" || ty == "pk" || ty == "pqf" || ty == "pqs") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					if (g_dnet.ppQ.size() < 1024) g_dnet.ppQ.push_back(std::move(s));
					ReleaseSRWLockExclusive(&g_dnet.lock);
				} else if (ty == "map_ok") {
					AcquireSRWLockExclusive(&g_dnet.lock);
					g_dnet.map = s;
					++g_dnet.mapCount;
					ReleaseSRWLockExclusive(&g_dnet.lock);
				}
			}
		}
		return 0;
	}

	bool DrvOpen()
	{
		if (g_dnet.s != INVALID_SOCKET) {
			if (g_dnet.port != g_cfg.port) {
				sockaddr_in beam{};
				beam.sin_family = AF_INET;
				beam.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
				beam.sin_port = htons(u_short(g_cfg.port));
				connect(g_dnet.s, reinterpret_cast<sockaddr*>(&beam), sizeof(beam));
				g_dnet.port = g_cfg.port;
			}
			return true;
		}
		WSADATA wsa;
		if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) return false;
		SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (s == INVALID_SOCKET) {
			WSACleanup();
			return false;
		}
		sockaddr_in me{};
		me.sin_family = AF_INET;
		me.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		me.sin_port = 0;
		bind(s, reinterpret_cast<sockaddr*>(&me), sizeof(me));
		DWORD tmo = 100;
		setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tmo), sizeof(tmo));
		int rcvBuf = 1 << 20;
		setsockopt(s, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&rcvBuf), sizeof(rcvBuf));
		BOOL  no = FALSE;
		DWORD got = 0;
		WSAIoctl(s, SIO_UDP_CONNRESET, &no, sizeof(no), nullptr, 0, &got, nullptr, nullptr);  // no "port unreachable" errors while BeamNG isn't there
		sockaddr_in beam{};
		beam.sin_family = AF_INET;
		beam.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
		beam.sin_port = htons(u_short(g_cfg.port));
		connect(s, reinterpret_cast<sockaddr*>(&beam), sizeof(beam));  // only BeamNG's answers arrive here
		g_dnet.s = s;
		g_dnet.port = g_cfg.port;
		g_dnet.thread = CreateThread(nullptr, 0, DrvRecvMain, nullptr, 0, nullptr);
		if (!g_dnet.thread) {
			closesocket(s);
			g_dnet.s = INVALID_SOCKET;
			return false;
		}
		return true;
	}

	void DrvSend(const char* msg) { send(g_dnet.s, msg, int(strlen(msg)), 0); }

	// ---------------------------------------------------------------------------------------
	// v2.01: your pad and keyboard, read here (not through GTA's controls), handed to the BeamNG mod in
	// BeamNG's own names; the mod runs them through your BeamNG bindings. The pad through XInput (Xbox pads,
	// DS4Windows' virtual pad), the keyboard only while GTA has focus. Both only while this script runs.
	// ---------------------------------------------------------------------------------------
	struct XiPadRaw
	{
		WORD  buttons;
		BYTE  lt, rt;
		SHORT lx, ly, rx, ry;
	};
	struct XiStateRaw
	{
		DWORD    packet;
		XiPadRaw pad;
	};
	typedef DWORD(WINAPI* XiGetStateFn)(DWORD, XiStateRaw*);
	struct PadNow
	{
		bool   ok = false;
		int    index = -1;
		float  lx = 0, ly = 0, rx = 0, ry = 0, lt = 0, rt = 0;  // sticks -1..1 (up = +), triggers 0..1
		WORD   btn = 0;
		double lastChange = -1e9;
	};
	struct InputDev
	{
		bool         tried = false;
		XiGetStateFn get = nullptr;
		const char*  dllName = "none";
		bool         conn[4] = {};
		DWORD        packet[4] = {};
		double       changed[4] = { -1e9, -1e9, -1e9, -1e9 };
		double       nextScan = 0;
		PadNow       pad;
		const char*  keyName[256] = {};
		double       keyT = -1e9;  // when a key was last held
	} g_in;

	void InputInit()
	{
		if (g_in.tried) return;
		g_in.tried = true;
		const char* dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
		for (const char* n : dlls) {
			HMODULE h = LoadLibraryA(n);
			if (!h) continue;
			auto f = reinterpret_cast<XiGetStateFn>(reinterpret_cast<void*>(GetProcAddress(h, "XInputGetState")));
			if (f) {
				g_in.get = f;
				g_in.dllName = n;
				break;
			}
			FreeLibrary(h);
		}
		Log("input: pad reading %s (%s)", g_in.get ? "ready" : "NOT available (no XInput DLL)", g_in.dllName);
		// BeamNG's key names (Torque3D's): letters, digits, F keys, numpad and the named ones
		static char letters[26][2], digits[10][2], fkeys[24][4], numpad[10][8];
		for (int i = 0; i < 26; ++i) {
			letters[i][0] = char('a' + i);
			g_in.keyName['A' + i] = letters[i];
		}
		for (int i = 0; i < 10; ++i) {
			digits[i][0] = char('0' + i);
			g_in.keyName['0' + i] = digits[i];
			snprintf(numpad[i], sizeof(numpad[i]), "numpad%d", i);
			g_in.keyName[VK_NUMPAD0 + i] = numpad[i];
		}
		for (int i = 0; i < 24; ++i) {
			snprintf(fkeys[i], sizeof(fkeys[i]), "f%d", i + 1);
			g_in.keyName[VK_F1 + i] = fkeys[i];
		}
		static const struct
		{
			int         vk;
			const char* n;
		} named[] = { { VK_SPACE, "space" }, { VK_RETURN, "return" }, { VK_TAB, "tab" }, { VK_BACK, "backspace" }, { VK_ESCAPE, "escape" }, { VK_UP, "up" },
			          { VK_DOWN, "down" }, { VK_LEFT, "left" }, { VK_RIGHT, "right" }, { VK_LSHIFT, "lshift" }, { VK_RSHIFT, "rshift" }, { VK_LCONTROL, "lcontrol" },
			          { VK_RCONTROL, "rcontrol" }, { VK_LMENU, "lalt" }, { VK_RMENU, "ralt" }, { VK_INSERT, "insert" }, { VK_DELETE, "delete" }, { VK_HOME, "home" },
			          { VK_END, "end" }, { VK_PRIOR, "pageup" }, { VK_NEXT, "pagedown" }, { VK_CAPITAL, "capslock" }, { VK_OEM_3, "tilde" }, { VK_OEM_MINUS, "minus" },
			          { VK_OEM_PLUS, "equals" }, { VK_OEM_4, "lbracket" }, { VK_OEM_6, "rbracket" }, { VK_OEM_5, "backslash" }, { VK_OEM_1, "semicolon" },
			          { VK_OEM_7, "apostrophe" }, { VK_OEM_COMMA, "comma" }, { VK_OEM_PERIOD, "period" }, { VK_OEM_2, "slash" }, { VK_OEM_102, "lessthan" },
			          { VK_MULTIPLY, "numpadmult" }, { VK_ADD, "numpadadd" }, { VK_SUBTRACT, "numpadminus" }, { VK_DECIMAL, "numpaddecimal" }, { VK_DIVIDE, "numpaddivide" },
			          { VK_PAUSE, "pause" }, { VK_NUMLOCK, "numlock" }, { VK_SCROLL, "scrolllock" } };
		for (const auto& k : named) g_in.keyName[k.vk] = k.n;
	}

	// the pad in use: the one that moved last (or PadIndex)
	void ReadPad(double now)
	{
		InputInit();
		PadNow& p = g_in.pad;
		p.ok = false;
		if (!g_in.get) return;
		const bool scan = now >= g_in.nextScan;
		if (scan) g_in.nextScan = now + 1.0;  // asking an empty slot is slow: once a second
		XiStateRaw st[4] = {};
		int        best = -1;
		for (int i = 0; i < 4; ++i) {
			if (!g_in.conn[i] && !scan) continue;
			const bool c = g_in.get(DWORD(i), &st[i]) == ERROR_SUCCESS;
			if (c != g_in.conn[i]) {
				g_in.conn[i] = c;
				Log("input: XInput pad %d %s", i, c ? "connected" : "gone");
			}
			if (!c) continue;
			if (st[i].packet != g_in.packet[i]) {
				g_in.packet[i] = st[i].packet;
				g_in.changed[i] = now;
			}
			if (g_cfg.padIndex >= 0 ? i == g_cfg.padIndex : (best < 0 || g_in.changed[i] > g_in.changed[best])) best = i;
		}
		if (best < 0) return;
		const XiPadRaw& r = st[best].pad;
		auto stick = [](SHORT v) { return std::clamp(float(v) / 32768.0f, -1.0f, 1.0f); };
		p.lx = stick(r.lx);
		p.ly = stick(r.ly);
		p.rx = stick(r.rx);
		p.ry = stick(r.ry);
		p.lt = float(r.lt) / 255.0f;
		p.rt = float(r.rt) / 255.0f;
		p.btn = r.buttons;
		p.ok = true;
		p.index = best;
		p.lastChange = g_in.changed[best];
	}

	// pad controls held now (bits): buttons, triggers past 0.15, sticks past 0.3
	uint32_t PadHeld(const PadNow& p)
	{
		if (!p.ok) return 0;
		uint32_t b = 0;
		for (int c = 0; c < 14; ++c)
			if (p.btn & kPadBit[c]) b |= 1u << c;
		if (p.lt > 0.15f) b |= 1u << kPLT;
		if (p.rt > 0.15f) b |= 1u << kPRT;
		if (fabsf(p.lx) > 0.3f || fabsf(p.ly) > 0.3f) b |= 1u << kPLStick;
		if (fabsf(p.rx) > 0.3f || fabsf(p.ry) > 0.3f) b |= 1u << kPRStick;
		return b;
	}

	// v2.72: your seat per BeamNG car (SeatsLoad below)
	struct SeatState
	{
		bool                                          loaded = false, on = false;
		std::map<std::string, std::array<double, 3>> cars;
		double                                        base[3] = {}, noteT = 0;
	} g_seat;

	// keys held now, BeamNG names, spaced (" up x "), without GTA's own (KeysAlwaysGta) and the plugin's keys
	std::string ReadKeys()
	{
		InputInit();
		std::string out = " ";
		// while the plugin's own keys are held (Shift+F10, Ctrl+F11...) their Shift/Ctrl/Alt are not BeamNG's
		if ((GetAsyncKeyState(g_cfg.key) & 0x8000) || (GetAsyncKeyState(g_cfg.driveKey) & 0x8000)) return out;
		for (int vk = 1; vk < 256; ++vk) {
			const char* n = g_in.keyName[vk];
			if (!n || vk == g_cfg.key || vk == g_cfg.driveKey) continue;
			if (g_seat.on && (vk == VK_UP || vk == VK_DOWN || vk == VK_LEFT || vk == VK_RIGHT || vk == VK_PRIOR || vk == VK_NEXT)) continue;  // (v2.72: seat adjust)
			if (!(GetAsyncKeyState(vk) & 0x8000)) continue;
			if (g_cfg.keysAlwaysGta.find(std::string(" ") + n + " ") != std::string::npos) continue;
			out += n;
			out += ' ';
		}
		return out;
	}

	// GTA's controls on each pad control / key (GTA's default layouts, from FiveM's control list). In drive
	// mode GTA's reaction is switched off where BeamNG gets that control, so pressing it doesn't do both.
	struct GtaCtl
	{
		int         id;
		signed char pad;     // PadCtl, -1 none
		const char* keys;    // BeamNG key names, spaced
		bool        always;  // GTA driving its own (hidden, puppet) car: always off in drive mode
	};
	const GtaCtl kGtaCtls[] = {
		{ 59, kPLStick, " a d ", true }, { 60, kPLStick, " lcontrol ", true }, { 61, kPLStick, " lshift ", true }, { 62, kPLStick, " lcontrol ", true },
		{ 63, kPLStick, " a ", true }, { 64, kPLStick, " d ", true }, { 71, kPRT, " w ", true }, { 72, kPLT, " s ", true }, { 76, kPRB, " space ", true },
		{ 77, kPLT, " w ", true }, { 78, kPRT, " s ", true }, { 87, kPRT, " w ", true }, { 88, kPLT, " s ", true }, { 89, kPLB, " a ", true }, { 90, kPRB, " d ", true },
		{ 102, kPRB, " space ", true }, { 107, kPLStick, " numpad6 ", true }, { 108, kPLStick, " numpad4 ", true }, { 109, kPLStick, " numpad6 ", true },
		{ 110, kPLStick, " numpad5 ", true }, { 337, kPA, " x ", true }, { 338, kPLStick, " a ", true }, { 339, kPLStick, " d ", true }, { 340, kPLStick, " lshift ", true },
		{ 341, kPLStick, " lcontrol ", true }, { 342, kPLStick, " d ", true }, { 343, kPLStick, " lcontrol ", true }, { 350, kPLS, " e ", true }, { 351, kPLS, " e ", true },
		{ 352, kPLS, " lshift ", true }, { 353, kPA, " space ", true }, { 354, kPA, " x ", true }, { 357, kPA, " x ", true },
		{ 0, kPBack, " v ", false },    // next camera view
		{ 14, kPDR, "", false },        // weapon wheel next (pad)
		{ 15, kPDL, "", false },        // weapon wheel previous (pad)
		{ 19, kPDD, " lalt ", false },  // character wheel
		{ 20, kPDD, " z ", false },
		{ 26, kPRS, " c ", false },
		{ 27, kPDU, " up ", false },    // phone
		{ 68, kPLB, "", false },        // drive-by aim (keyboard: right mouse button, stays GTA's)
		{ 69, kPRB, "", false },        // drive-by shoot (left mouse button stays GTA's)
		{ 70, kPA, "", false },
		{ 73, kPA, " x ", false },      // duck
		{ 74, kPDR, " h ", false },     // headlights
		{ 75, kPY, " f ", false },      // get out
		{ 79, kPRS, " c ", false },     // look behind
		{ 80, kPB, " r ", false },      // cinematic camera
		{ 81, -1, " period ", false }, { 82, -1, " comma ", false }, { 83, -1, " equals ", false }, { 84, -1, " minus ", false },
		{ 85, kPDL, " q ", false },     // radio wheel
		{ 86, kPLS, " e ", false },     // horn
		{ 93, kPRS, "", false },
		{ 99, kPX, "", false },         // next weapon (pad X; the mouse wheel stays GTA's)
		{ 100, -1, " lbracket ", false },
		{ 101, kPDR, " h ", false }, { 103, kPDR, " e ", false }, { 104, kPDR, " h ", false }, { 105, kPA, " x ", false }, { 114, kPA, "", false },
		{ 199, kPStart, " p ", false }, { 200, -1, " escape ", false }, { 244, kPBack, " m ", false }, { 344, kPDR, "", false }, { 345, kPA, " x ", false },
		{ 346, kPLB, "", false }, { 347, kPRB, "", false }, { 355, kPDR, " e ", false }, { 356, kPDR, " e ", false },
	};

	// who gets pad control c now: 1 BeamNG, 0 GTA, -1 nobody (the toggle itself)
	int PadOwner(const DriveState& d, int c)
	{
		// v2.1: while one of your BeamNG pad modifiers is held (BeamNG mode), the controls your BeamNG combinations
		// use are BeamNG's (RB + D-pad down = your BeamNG combination, not the GTA/BeamNG switch)
		bool combo = false;
		if (!d.padGta && d.modMask && d.padCombo[c]) {
			// exactly the modifiers one of your combinations on this control uses (BeamNG's rule)
			for (int i = 0; i < d.comboN[c]; ++i)
				if (d.comboMask[c][i] == d.modMask) combo = true;
			if (!d.comboN[c]) combo = true;  // a BeamNG mod without the sets (older): any modifier
		}
		if (c == g_cfg.padToggle) return combo ? 1 : -1;
		if (g_cfg.padAlwaysBeam & (1u << c)) return 1;
		if (g_cfg.padAlwaysGta & (1u << c)) return 0;
		if (d.padGta) return 0;
		// v2.4 (your spec): in BeamNG mode the whole pad is BeamNG's, raw: a button none of your BeamNG bindings use does
		// nothing (GTA's reaction stays off). Only without your bindings (GTA's basic values, the v2.0 way) do the
		// buttons BeamNG doesn't take stay GTA's.
		if (!d.bindsMode && d.padKnown && !d.padBound[c]) return 0;
		return 1;
	}

	bool KeyForBeam(const DriveState& d, const char* key)
	{
		const std::string w = std::string(" ") + key + " ";
		return d.keyBound.find(w) != std::string::npos && g_cfg.keysAlwaysGta.find(w) == std::string::npos;
	}

	// switch GTA's reactions off where BeamNG has the control (every tick: GTA only holds it for one frame)
	int BlockGtaControls(const DriveState& d, bool rawIn, bool padActive, bool kbActive)
	{
		int n = 0;
		for (const GtaCtl& c : kGtaCtls) {
			bool off = c.always;
			if (!off && rawIn && padActive && c.pad >= 0 && PadOwner(d, c.pad) != 0) off = true;
			if (!off && rawIn && c.pad >= 0 && (d.padSupp >> c.pad & 1u)) off = true;  // held across a switch: nobody's until let go
			if (!off && padActive && c.pad >= 0 && (d.handled >> c.pad & 1u)) off = true;  // v2.3: the plugin handles it (radio, BeamNG action)
			if (!off && rawIn && kbActive && c.keys[0]) {
				char k[24];
				for (const char* p = c.keys; *p && !off;) {
					while (*p == ' ') ++p;
					int n2 = 0;
					while (*p && *p != ' ' && n2 < 23) k[n2++] = *p++;
					k[n2] = 0;
					if (n2 && KeyForBeam(d, k)) off = true;
				}
			}
			if (off) {
				CallVoid(N::DISABLE_CONTROL_ACTION, 0, c.id, 0);
				++n;
			}
		}
		return n;
	}

	// one line of on-screen text (GTA's own text drawing; the drive check, Shift+F10)
	void DrawLine(float x, float y, const char* text, int r = 255, int g = 255, int b = 255)
	{
		CallVoid(N::SET_TEXT_FONT, 0);
		CallVoid(N::SET_TEXT_SCALE, 0.0f, 0.30f);
		CallVoid(N::SET_TEXT_COLOUR, r, g, b, 235);
		CallVoid(N::SET_TEXT_OUTLINE);
		const bool longText = strlen(text) > 99;
		CallVoid(N::BEGIN_TEXT_COMMAND_DISPLAY_TEXT, longText ? "CELL_EMAIL_BCON" : "STRING");
		if (longText) PushLongText(text);
		else CallVoid(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, text);
		CallVoid(N::END_TEXT_COMMAND_DISPLAY_TEXT, x, y, 0);
	}

	// new poses from the receiver
	void DrvPull()
	{
		DriveState& d = g_drv;
		AcquireSRWLockShared(&g_dnet.lock);
		if (g_dnet.gen != d.gen) {
			d.gen = g_dnet.gen;
			d.taken = 0;
			d.poses.clear();
		}
		const uint32_t n = g_dnet.n;
		const uint32_t from = std::max(d.taken, n > uint32_t(kDrvRing) ? n - uint32_t(kDrvRing) : 0u);
		for (uint32_t i = from; i < n; ++i) {
			const DrvPose& np = g_dnet.ring[i % kDrvRing];
			if (np.st < d.poseMinSt) continue;  // v2.4: from before BeamNG's car was put on GTA's ground
			// v2.43: BeamNG's clock stood still while ours went on because BeamNG was paused ("pz" came meanwhile; or more
			// than 3 s): the poses before go, the clock is found again from the new ones (a short hitch is left to the
			// clock's own slow adjustment, as before)
			if (!d.poses.empty()) {
				const double jump = (np.rt - np.st) - (d.poses.back().rt - d.poses.back().st);
				if (jump > 3.0 || (jump > 0.3 && g_dnet.pzT > d.poses.back().rt - 0.5)) {
					d.poses.clear();
					d.haveOff = false;
					++d.clockJumps;
				}
			}
			// v2.3: how well BeamNG's positions fit its own speeds and clock (timing noise), and late poses
			if (!d.poses.empty()) {
				const DrvPose& pp = d.poses.back();
				const double   h = np.fst - pp.fst;  // v2.42: the frame clock (for comparison)
				if (h > 0 && h < 0.2 && pp.hasV && np.hasV) {
					double e2 = 0;
					for (int k = 0; k < 3; ++k) {
						const double e = np.P[k] - (pp.P[k] + 0.5 * (pp.v[k] + np.v[k]) * h);
						e2 += e * e;
					}
					const double e = sqrt(e2);
					if (e < 5.0) {  // v2.4: not a teleport
						d.tnSum += e;
						d.tnMax = std::max(d.tnMax, e);
						++d.tnN;
					}
				}
				// v2.4 (P1, measurement): the same against BeamNG's simulation clock
				const double h2 = np.ss - pp.ss;
				if (np.hasSs && pp.hasSs && h2 > 0 && h2 < 0.2 && pp.hasV && np.hasV) {
					double e2 = 0;
					for (int k = 0; k < 3; ++k) {
						const double e = np.P[k] - (pp.P[k] + 0.5 * (pp.v[k] + np.v[k]) * h2);
						e2 += e * e;
					}
					const double e = sqrt(e2);
					if (e < 5.0) {
						d.tnSum2 += e;
						d.tnMax2 = std::max(d.tnMax2, e);
						++d.tnN2;
					}
				}
				const double gap = np.rt - pp.rt;
				if (d.ivLast > 0 && gap > 2.5 * d.ivLast) ++d.gaps;
				d.gapMax = std::max(d.gapMax, gap);
			}
			d.poses.push_back(np);
		}
		ReleaseSRWLockShared(&g_dnet.lock);
		d.newPoses = n > from ? n - from : 0;
		// v2.42: the pose log (DrivePoseLog=1): what arrived when (for the rubber band; GtaBeamD1_poses.csv)
		if (d.plog && d.plogRows < 150000) {
			const size_t k0 = d.poses.size() - std::min<size_t>(d.poses.size(), n > from ? n - from : 0);
			for (size_t k = k0; k < d.poses.size(); ++k) {
				const DrvPose& q = d.poses[k];
				fprintf(d.plog, "P,%.5f,%.5f,%.5f,%.5f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n", q.rt, q.fst, q.ss, q.st, q.P[0], q.P[1], q.P[2], q.v[0], q.v[1], q.v[2]);
				++d.plogRows;
			}
		}
		d.rx += d.newPoses;
		d.taken = n;
		if (d.newPoses && !d.poses.empty()) d.lastArrival = std::max(d.lastArrival, d.poses.back().rt);
		if (d.poses.size() > 96) d.poses.erase(d.poses.begin(), d.poses.end() - 96);  // ~1.5 s of poses
	}

	// BeamNG's pose at BeamNG time t: between the two poses around it (cubic with the speeds), a little
	// past the newest (moved on with its speed, at most 0.1 s), or the oldest.
	// kind: 0 between, 1 past the newest, 2 held (too far past), 3 before the oldest
	bool DrvSample(double t, DrvPose& out, int& kind)
	{
		const std::vector<DrvPose>& P = g_drv.poses;
		if (P.empty()) return false;
		if (t <= P.front().st) {
			out = P.front();
			kind = 3;
			return true;
		}
		if (t >= P.back().st) {
			out = P.back();
			const double dt = std::min(t - P.back().st, 0.1);
			for (int k = 0; k < 3; ++k) out.P[k] += out.v[k] * dt;
			kind = t - P.back().st > 0.1 ? 2 : 1;
			return true;
		}
		size_t i = P.size() - 1;
		while (i > 1 && P[i - 1].st > t) --i;
		const DrvPose& a = P[i - 1];
		const DrvPose& b = P[i];
		const double   h = std::max(1e-6, b.st - a.st);
		const double   s = std::clamp((t - a.st) / h, 0.0, 1.0);
		out = b;
		if (a.hasV && b.hasV) {
			// Hermite: positions and speeds at both ends (smooth curves even at 30 updates a second)
			const double s2 = s * s, s3 = s2 * s;
			const double h00 = 2 * s3 - 3 * s2 + 1, h10 = s3 - 2 * s2 + s, h01 = -2 * s3 + 3 * s2, h11 = s3 - s2;
			for (int k = 0; k < 3; ++k) out.P[k] = h00 * a.P[k] + h10 * h * a.v[k] + h01 * b.P[k] + h11 * h * b.v[k];
		} else {
			for (int k = 0; k < 3; ++k) out.P[k] = a.P[k] + (b.P[k] - a.P[k]) * s;
		}
		for (int k = 0; k < 3; ++k) {
			out.v[k] = a.v[k] + (b.v[k] - a.v[k]) * s;
			out.f[k] = a.f[k] + (b.f[k] - a.f[k]) * s;
			out.u[k] = a.u[k] + (b.u[k] - a.u[k]) * s;
		}
		Orthonormal(out.f, out.u);
		if (a.nw == b.nw) {
			for (int w = 0; w < b.nw; ++w) {
				for (int k = 0; k < 6; ++k) out.w[w][k] = float(a.w[w][k] + (b.w[w][k] - a.w[w][k]) * s);
				out.ang[w] = a.ang[w] + (b.ang[w] - a.ang[w]) * s;
			}
		}
		out.spin = a.spin && b.spin;
		kind = 0;
		return true;
	}

	void DrvFrame(const DrvPose& p, double r[3], double f[3], double u[3])
	{
		memcpy(f, p.f, sizeof(double) * 3);
		memcpy(u, p.u, sizeof(double) * 3);
		Orthonormal(f, u);
		Cross3(f, u, r);
	}

	void DrvMapPoint(const double X[3], double out[3])
	{
		const DriveState& d = g_drv;
		const double      dx = X[0] - d.Ab[0], dy = X[1] - d.Ab[1], dz = X[2] - d.Ab[2];
		out[0] = d.Ag[0] + d.cth * dx - d.sth * dy;
		out[1] = d.Ag[1] + d.sth * dx + d.cth * dy;
		out[2] = d.Ag[2] + dz;
	}
	void DrvMapDir(const double v[3], double out[3])
	{
		const DriveState& d = g_drv;
		const double      x = d.cth * v[0] - d.sth * v[1], y = d.sth * v[0] + d.cth * v[1];
		out[0] = x;
		out[1] = y;
		out[2] = v[2];
	}

	// Rotation (columns R, F, U = the car's right/forward/up in the world) -> quaternion x, y, z, w
	void BasisQuat(const double R[3], const double F[3], const double U[3], double q[4])
	{
		const double m00 = R[0], m01 = F[0], m02 = U[0];
		const double m10 = R[1], m11 = F[1], m12 = U[1];
		const double m20 = R[2], m21 = F[2], m22 = U[2];
		const double tr = m00 + m11 + m22;
		if (tr > 0) {
			const double s = sqrt(tr + 1.0) * 2;
			q[3] = 0.25 * s;
			q[0] = (m21 - m12) / s;
			q[1] = (m02 - m20) / s;
			q[2] = (m10 - m01) / s;
		} else if (m00 > m11 && m00 > m22) {
			const double s = sqrt(1.0 + m00 - m11 - m22) * 2;
			q[3] = (m21 - m12) / s;
			q[0] = 0.25 * s;
			q[1] = (m01 + m10) / s;
			q[2] = (m02 + m20) / s;
		} else if (m11 > m22) {
			const double s = sqrt(1.0 + m11 - m00 - m22) * 2;
			q[3] = (m02 - m20) / s;
			q[0] = (m01 + m10) / s;
			q[1] = 0.25 * s;
			q[2] = (m12 + m21) / s;
		} else {
			const double s = sqrt(1.0 + m22 - m00 - m11) * 2;
			q[3] = (m10 - m01) / s;
			q[0] = (m02 + m20) / s;
			q[1] = (m12 + m21) / s;
			q[2] = 0.25 * s;
		}
	}

	// Put the hidden GTA car where the drawn car's origin O (GTA world) is, turned to R/F/U, moving with V.
	// v2.2: the puppet pose set last tick, and how GTA moved it since (drift)
	struct DrvSetRec
	{
		bool     valid = false;
		double   O[3] = {}, R[3] = {}, F[3] = {}, U[3] = {}, V[3] = {};
		double   drift = 0, driftMax = 0, driftSum = 0;  // metres: read-back vs set + V x GTA's step
		uint32_t driftN = 0;
		bool     used = false;  // this tick's drawn car is the set pose (DriveDrawPose=set)
		double   beamSt = 0;    // BeamNG time drawn this tick
		int      slipApplied = 0;
		double   corrSum = 0, corrMax = 0;  // v2.3: how far each tick's placement is from GTA's own step (metres)
		uint32_t corrN = 0;
	} g_dset;

	// before DriveTick: compare GTA's read-back with what we set; DriveDrawPose=set draws the set pose instead
	void DrvDrawCheck(CamSample& c)
	{
		g_dset.used = false;
		g_dset.drift = -1;
		if (g_drv.state != 2 || !c.carOn || !g_dset.valid) {
			if (g_drv.state != 2) g_dset.valid = false;
			return;
		}
		const double ft = std::clamp(double(Call<float>(N::GET_FRAME_TIME)), 0.0, 0.1);
		double E[3];
		for (int k = 0; k < 3; ++k) E[k] = g_dset.O[k] + g_dset.V[k] * ft;
		const double dx = c.ox - E[0], dy = c.oy - E[1], dz = c.oz - E[2];
		const double dr = sqrt(dx * dx + dy * dy + dz * dz);
		if (dr < 5.0) {
			g_dset.drift = dr;
			g_dset.driftSum += dr;
			g_dset.driftMax = std::max(g_dset.driftMax, dr);
			++g_dset.driftN;
		}
		if (g_cfg.driveDrawPose == 1) {
			c.ox = E[0];
			c.oy = E[1];
			c.oz = E[2];
			for (int k = 0; k < 3; ++k) {
				c.R[k] = float(g_dset.R[k]);
				c.F[k] = float(g_dset.F[k]);
				c.U[k] = float(g_dset.U[k]);
			}
			g_dset.used = true;
		}
	}

	void DrvSetPuppet(const double O[3], const double R[3], const double F[3], const double U[3], const double V[3])
	{
		DriveState& d = g_drv;
		const int   veh = d.veh;
		const double e[3] = { O[0] - (R[0] * d.lx + F[0] * d.ly + U[0] * d.lz), O[1] - (R[1] * d.lx + F[1] * d.ly + U[1] * d.lz),
			                  O[2] - (R[2] * d.lx + F[2] * d.ly + U[2] * d.lz) };
		CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, veh, float(e[0]), float(e[1]), float(e[2]), 0, 0, 0);
		if (d.rotMode == 0) {
			// GTA rotation order 2: the inverse of CamBasis (verified for the camera in D0)
			const double pitch = asin(std::clamp(F[2], -1.0, 1.0)) / kDeg;
			const double yaw = atan2(-F[0], F[1]) / kDeg;
			const double roll = atan2(-R[2], U[2]) / kDeg;
			CallVoid(N::SET_ENTITY_ROTATION, veh, float(pitch), float(roll), float(yaw), 2, 1);
		} else {
			double q[4];
			BasisQuat(R, F, U, q);
			CallVoid(N::SET_ENTITY_QUATERNION, veh, float(q[0]), float(q[1]), float(q[2]), float(q[3]));
		}
		if (g_cfg.driveSetVelocity) {
			CallVoid(N::SET_ENTITY_VELOCITY, veh, float(V[0]), float(V[1]), float(V[2]));
		}
		d.setValid = true;
		// v2.3: how far the car is put from where GTA's own step took it (what GTA's camera sees as jitter)
		if (g_dset.valid) {
			const double ftn = std::clamp(double(Call<float>(N::GET_FRAME_TIME)), 0.0, 0.1);
			double       c2 = 0;
			for (int k = 0; k < 3; ++k) {
				const double c = O[k] - (g_dset.O[k] + g_dset.V[k] * ftn);
				c2 += c * c;
			}
			const double cl = sqrt(c2);
			if (cl < 2.0) {
				g_dset.corrSum += cl;
				g_dset.corrMax = std::max(g_dset.corrMax, cl);
				++g_dset.corrN;
			}
		}
		// v2.2: what was set (drawn-origin frame), for the drift check and DriveDrawPose=set
		g_dset.valid = true;
		memcpy(g_dset.O, O, sizeof(g_dset.O));
		memcpy(g_dset.R, R, sizeof(g_dset.R));
		memcpy(g_dset.F, F, sizeof(g_dset.F));
		memcpy(g_dset.U, U, sizeof(g_dset.U));
		for (int k = 0; k < 3; ++k) g_dset.V[k] = g_cfg.driveSetVelocity ? V[k] : 0.0;
		memcpy(d.setF, F, sizeof(d.setF));
		memcpy(d.setU, U, sizeof(d.setU));
	}

	const char* KeyName(int vk)
	{
		static char buf[16];
		if (vk >= VK_F1 && vk <= VK_F24) {
			snprintf(buf, sizeof(buf), "F%d", vk - VK_F1 + 1);
		} else {
			snprintf(buf, sizeof(buf), "key 0x%02X", vk);
		}
		return buf;
	}

	// drive_start, with what the BeamNG mod needs to pick your bindings (v2.01)
	std::string WeaponsStartJson();  // v2.6 (below)
	void DrvSendStart()
	{
		// v2.4: snd 0 no BeamNG sound, 1 switched for each drive (bounce), 2 session mode (sq: BeamNG's volume while not driving)
		const char* snd = !g_cfg.beamSound ? "0" : g_cfg.soundWake == 3 ? "2" : "1";
		char        sq[32];
		snprintf(sq, sizeof(sq), "%.2f", g_cfg.quietVolume);
		std::string m = std::string("{\"t\":\"drive_start\",\"v\":\"" D1_VERSION "\",\"in\":") + (g_cfg.driveInput ? "1" : "0") + ",\"cats\":\"" + g_cfg.beamCats +
		                "\",\"deny\":\"" + g_cfg.beamNever + "\",\"also\":\"" + g_cfg.beamAlso + "\",\"ds\":1,\"pb\":\"" + g_cfg.padList + "\",\"snd\":" + snd +
		                ",\"sq\":" + sq + ",\"bgf\":" + std::to_string(g_cfg.beamBgFps) + ",\"rep\":\"" + g_cfg.repairActs + "\",\"ps\":" + (g_cfg.pauseSync ? "1" : "0");
		// v2.5: gravity x DriveGravity while GTA drives; the car's shape (dents) this often a second (0 = off)
		char v25[96];
		snprintf(v25, sizeof(v25), ",\"grav\":%.3f,\"dz\":%.1f", g_cfg.driveGravity, g_cfg.dents ? g_cfg.dentHz : 0.0f);
		m += v25;
		m += WeaponsStartJson() + "}";  // v2.6
		DrvSend(m.c_str());
	}

	// ---------------------------------------------------------------------------------------
	// v2.4 T3: GTA's ground -> BeamNG. Asynchronous downward rays (map collision only) on 8 m tiles around the car and
	// along where it is going; each finished tile goes to the BeamNG mod as heights + one surface per cell ("hf") and
	// is built there as collision, WorldZLift (200 m) above BeamNG's own map. The mapping is fixed: BeamNG = GTA - origin
	// (origin = the car's spot at F10, rounded to 500 m) and + WorldZLift up. Only standard natives; GTA's world reaches
	// BeamNG as live ray results on your PC, nothing is saved.
	// ---------------------------------------------------------------------------------------

	// GTA ground material -> BeamNG ground model (GtaBeamD1_surfaces.json next to the .asi). Hashes and names: Script
	// Hook V .NET's MaterialHash list (zlib licence, crosire & kagikn), confirmed in game by its users.
	const char* kSurfacesDefault = R"JSON({
  "about": "GTA x Beam v2.41: which BeamNG ground model (art/groundmodels.json in BeamNG) each GTA ground material becomes. Edit 'beamng'; 'A|B' = A if BeamNG has it, else B. Materials not listed use 'default'. GtaBeamD1_materials.txt lists the materials your drives touched. Ctrl+F11 re-reads this file. CONCRETE: BeamNG has none of its own; the BeamNG mod (2.41) makes one from your ASPHALT the first time (load the map again once). GTA material hashes/names: Script Hook V .NET's MaterialHash list (zlib licence).",
  "default": "ASPHALT",
  "materials": [
    { "gta": "Default", "hash": "0x962C3F7B", "beamng": "ASPHALT" },
    { "gta": "Tarmac", "hash": "0x10DD5498", "beamng": "ASPHALT" },
    { "gta": "TarmacPainted", "hash": "0xB26EEFB0", "beamng": "ASPHALT" },
    { "gta": "TarmacPothole", "hash": "0x70726A55", "beamng": "ASPHALT_OLD|ASPHALT" },
    { "gta": "MetalSolidRoadSurface", "hash": "0xD48AA0F2", "beamng": "ASPHALT" },
    { "gta": "StuntRampSurface", "hash": "0x8388FA6C", "beamng": "ASPHALT" },
    { "gta": "RumbleStrip", "hash": "0xF116BC2D", "beamng": "RUMBLE_STRIP|ASPHALT" },
    { "gta": "Concrete", "hash": "0x46CA81E8", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "ConcretePothole", "hash": "0x1567BF52", "beamng": "CONCRETE|ASPHALT_OLD|ASPHALT" },
    { "gta": "ConcreteDusty", "hash": "0xBF59B491", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "ConcretePavement", "hash": "0x78239B1A", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "BrickPavement", "hash": "0xBB9CA6D8", "beamng": "COBBLESTONE|ASPHALT" },
    { "gta": "PavingSlab", "hash": "0x71AB3FEE", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "BreezeBlock", "hash": "0xC72165D6", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Brick", "hash": "0x61B1F936", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Marble", "hash": "0x73EF7697", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Ceramic", "hash": "0xB94A2EB5", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Stone", "hash": "0x2D9C1E0D", "beamng": "ROCK|ASPHALT" },
    { "gta": "Cobblestone", "hash": "0x2257A573", "beamng": "COBBLESTONE|ASPHALT" },
    { "gta": "Rock", "hash": "0xCDEB5023", "beamng": "ROCK|GRAVEL" },
    { "gta": "RockMossy", "hash": "0xF8902AC8", "beamng": "ROCK|GRAVEL" },
    { "gta": "RockNoinst", "hash": "0x079E4953", "beamng": "ROCK|GRAVEL" },
    { "gta": "SandstoneSolid", "hash": "0x23500534", "beamng": "ROCK|GRAVEL" },
    { "gta": "SandstoneBrittle", "hash": "0x7209440E", "beamng": "GRAVEL|DIRT" },
    { "gta": "SandLoose", "hash": "0xA0EBF7E4", "beamng": "SAND|DIRT" },
    { "gta": "SandCompact", "hash": "0x1E6D775E", "beamng": "SAND|DIRT" },
    { "gta": "SandWet", "hash": "0x363CBCD5", "beamng": "SAND|MUD" },
    { "gta": "SandTrack", "hash": "0x8E4D8AFF", "beamng": "SAND|DIRT" },
    { "gta": "SandUnderwater", "hash": "0xBC4922A4", "beamng": "SAND|MUD" },
    { "gta": "SandDryDeep", "hash": "0x1E5E7A48", "beamng": "SAND|DIRT" },
    { "gta": "SandWetDeep", "hash": "0x4CCC2AFF", "beamng": "SAND|MUD" },
    { "gta": "Ice", "hash": "0xD125AA55", "beamng": "ICE" },
    { "gta": "IceTarmac", "hash": "0x8CE6E7D9", "beamng": "ICE" },
    { "gta": "SnowLoose", "hash": "0x8C8308CA", "beamng": "SNOW|ICE" },
    { "gta": "SnowCompact", "hash": "0xCBA23987", "beamng": "SNOW|ICE" },
    { "gta": "SnowDeep", "hash": "0x608ABC80", "beamng": "SNOW|ICE" },
    { "gta": "SnowTarmac", "hash": "0x5C67C62A", "beamng": "SNOW|ICE" },
    { "gta": "GravelSmall", "hash": "0x38BBD00C", "beamng": "GRAVEL|DIRT" },
    { "gta": "GravelLarge", "hash": "0x7EDC5571", "beamng": "GRAVEL|DIRT" },
    { "gta": "GravelDeep", "hash": "0xEABD174E", "beamng": "GRAVEL|DIRT" },
    { "gta": "GravelTrainTrack", "hash": "0x72C668B6", "beamng": "GRAVEL|DIRT" },
    { "gta": "DirtTrack", "hash": "0x8F9CD58F", "beamng": "DIRT_DUSTY|DIRT" },
    { "gta": "MudHard", "hash": "0x8C31B7EA", "beamng": "DIRT" },
    { "gta": "MudPothole", "hash": "0x129ECA2A", "beamng": "MUD|DIRT" },
    { "gta": "MudSoft", "hash": "0x61826E7A", "beamng": "MUD|DIRT" },
    { "gta": "MudUnderwater", "hash": "0xEFB2DF09", "beamng": "MUD|DIRT" },
    { "gta": "MudDeep", "hash": "0x42251DC0", "beamng": "MUD|DIRT" },
    { "gta": "Marsh", "hash": "0x0D4C07E2", "beamng": "MUD|DIRT" },
    { "gta": "MarshDeep", "hash": "0x5E73A22E", "beamng": "MUD|DIRT" },
    { "gta": "Soil", "hash": "0xD63CCDDB", "beamng": "DIRT" },
    { "gta": "ClayHard", "hash": "0x4434DFE7", "beamng": "DIRT" },
    { "gta": "ClaySoft", "hash": "0x216FF3F0", "beamng": "DIRT|MUD" },
    { "gta": "GrassLong", "hash": "0xE47A3E41", "beamng": "GRASS|DIRT" },
    { "gta": "Grass", "hash": "0x4F747B87", "beamng": "GRASS|DIRT" },
    { "gta": "GrassShort", "hash": "0xB34E900D", "beamng": "GRASS|DIRT" },
    { "gta": "Hay", "hash": "0x92B69883", "beamng": "GRASS|DIRT" },
    { "gta": "Bushes", "hash": "0x22AD7B72", "beamng": "GRASS|DIRT" },
    { "gta": "BushesNoinst", "hash": "0x55E5AAEE", "beamng": "GRASS|DIRT" },
    { "gta": "Twigs", "hash": "0xC98F5B61", "beamng": "DIRT" },
    { "gta": "Leaves", "hash": "0x8653C6CD", "beamng": "GRASS|DIRT" },
    { "gta": "Woodchips", "hash": "0xED932E53", "beamng": "DIRT" },
    { "gta": "MetalSolidSmall", "hash": "0xA9BC4217", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalSolidMedium", "hash": "0xEA34E8F8", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalSolidLarge", "hash": "0x2CD49BD1", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalHollowSmall", "hash": "0x00F3B93B", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalHollowMedium", "hash": "0x6E3DBFB8", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalHollowLarge", "hash": "0xDD3CDCF9", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalCorrugatedIron", "hash": "0x31B80AD6", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalGrille", "hash": "0xE699F485", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalManhole", "hash": "0xD2FFA63D", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalGarageDoor", "hash": "0xF2373DE9", "beamng": "METAL|ASPHALT" },
    { "gta": "WoodSolidSmall", "hash": "0xE82A6F1C", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodSolidMedium", "hash": "0x2114B37D", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodSolidLarge", "hash": "0x309F8BB7", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodSolidPolished", "hash": "0x0789C7AB", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodFloorDusty", "hash": "0xD35443DE", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodHighDensity", "hash": "0x981E5200", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodHighFriction", "hash": "0x8070DCF9", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodOldCreaky", "hash": "0x2B13503D", "beamng": "WOOD|ASPHALT" },
    { "gta": "TreeBark", "hash": "0x8DD4EBB9", "beamng": "WOOD|DIRT" },
    { "gta": "Linoleum", "hash": "0x11436942", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Laminate", "hash": "0x6E02C9AA", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "Rubber", "hash": "0xF7503F13", "beamng": "ASPHALT" },
    { "gta": "Plastic", "hash": "0x846BC4FF", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PlasticHighDensity", "hash": "0x9F154729", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "Water", "hash": "0x19F81600", "beamng": "ASPHALT_WET|ASPHALT" },
    { "gta": "Puddle", "hash": "0x3B982E13", "beamng": "ASPHALT_WET|ASPHALT" },
    { "gta": "Oil", "hash": "0xDA2E9567", "beamng": "SLIPPERY|ASPHALT_WET|ASPHALT" },
    { "gta": "Petrol", "hash": "0x9E98536C", "beamng": "SLIPPERY|ASPHALT_WET|ASPHALT" },
    { "gta": "MetalChainLinkSmall", "hash": "0x2D6E26CD", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalChainLinkLarge", "hash": "0x0781FA34", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalRailing", "hash": "0x7D368D93", "beamng": "METAL|ASPHALT" },
    { "gta": "MetalDuct", "hash": "0x68FEB9FD", "beamng": "METAL|ASPHALT" },
    { "gta": "WoodHollowSmall", "hash": "0x76D9AC2F", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodHollowMedium", "hash": "0xEA3746BD", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodHollowLarge", "hash": "0xC8D738E7", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodChipboard", "hash": "0x461D0E9B", "beamng": "WOOD|ASPHALT" },
    { "gta": "WoodLattice", "hash": "0x77E08A22", "beamng": "WOOD|ASPHALT" },
    { "gta": "RoofTile", "hash": "0x689E0E75", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "RoofFelt", "hash": "0xAB87C845", "beamng": "ASPHALT" },
    { "gta": "Fiberglass", "hash": "0x50B728DB", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "FiberglassHollow", "hash": "0xD256ED46", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "Tarpaulin", "hash": "0xD9B1CDE0", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PlasticHollow", "hash": "0x25612338", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PlasticClear", "hash": "0x9126E8CB", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PlasticHollowClear", "hash": "0x2E0ECF63", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PlasticHighDensityClear", "hash": "0xB038852E", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "RubberHollow", "hash": "0xD1461B30", "beamng": "ASPHALT" },
    { "gta": "CarpetSolid", "hash": "0x27E49616", "beamng": "ASPHALT" },
    { "gta": "CarpetSolidDusty", "hash": "0x0973AE44", "beamng": "ASPHALT" },
    { "gta": "CarpetFloorboard", "hash": "0xACC354B1", "beamng": "WOOD|ASPHALT" },
    { "gta": "Cloth", "hash": "0x07519E5D", "beamng": "ASPHALT" },
    { "gta": "PlasterSolid", "hash": "0xDDC7963F", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "PlasterBrittle", "hash": "0xF0FC7AFE", "beamng": "CONCRETE|ASPHALT" },
    { "gta": "CardboardSheet", "hash": "0x0E18DFF5", "beamng": "ASPHALT" },
    { "gta": "CardboardBox", "hash": "0xAC038918", "beamng": "ASPHALT" },
    { "gta": "Paper", "hash": "0x1C42F3BC", "beamng": "ASPHALT" },
    { "gta": "Foam", "hash": "0x30341454", "beamng": "ASPHALT" },
    { "gta": "FeatherPillow", "hash": "0x4FFB413F", "beamng": "ASPHALT" },
    { "gta": "Polystyrene", "hash": "0x97476A9D", "beamng": "ASPHALT" },
    { "gta": "Leather", "hash": "0xDDFF4E0C", "beamng": "ASPHALT" },
    { "gta": "GlassShootThrough", "hash": "0x37E12A0B", "beamng": "ASPHALT" },
    { "gta": "GlassBulletproof", "hash": "0x0E931A0E", "beamng": "ASPHALT" },
    { "gta": "GlassOpaque", "hash": "0x596C55D1", "beamng": "ASPHALT" },
    { "gta": "Perspex", "hash": "0x9F73E76C", "beamng": "PLASTIC|ASPHALT" },
    { "gta": "PhysNoFriction", "hash": "0x63545F03", "beamng": "SLIPPERY|ICE|ASPHALT" },
    { "gta": "PhysElectricFence", "hash": "0xBA428CAB", "beamng": "METAL|ASPHALT" },
    { "gta": "PhysBarbedWire", "hash": "0xA402C0C0", "beamng": "METAL|ASPHALT" }
  ]
})JSON";

	struct SurfTable
	{
		std::map<uint32_t, int>         byHash;   // GTA material hash -> surface index
		std::map<uint32_t, std::string> gtaName;  // GTA material names (for the materials log)
		std::vector<std::string>        names;    // BeamNG ground model specs ("ASPHALT", "DIRT_DUSTY|DIRT")
		int                             def = 0;  // surface of materials not in the table
		std::string                     source;
	} g_surf;

	// v2.42: soft ground (grass, dirt, sand...): its small bumps matter less (WorldRefineSoftCm)
	bool SurfSoft(int s)
	{
		if (s < 0 || s >= int(g_surf.names.size())) return false;
		const std::string& n = g_surf.names[s];
		for (const char* k : { "GRASS", "DIRT", "SAND", "MUD", "GRAVEL", "SNOW" })
			if (n.compare(0, strlen(k), k) == 0) return true;
		return false;
	}

	int SurfIndex(SurfTable& t, const std::string& spec)
	{
		for (size_t i = 0; i < t.names.size(); ++i)
			if (t.names[i] == spec) return int(i);
		t.names.push_back(spec);
		return int(t.names.size() - 1);
	}

	bool ParseSurfaces(const std::string& js, SurfTable& t, std::string& err)
	{
		t = SurfTable{};
		std::string def = "ASPHALT";
		JsonStr(js, "default", def);
		t.def = SurfIndex(t, def.empty() ? "ASPHALT" : def);
		const size_t a = js.find("\"materials\"");
		if (a == std::string::npos) {
			err = "no \"materials\" list";
			return false;
		}
		size_t p = js.find('[', a);
		int    bad = 0;
		while (p != std::string::npos) {
			const size_t o = js.find('{', p);
			const size_t close = js.find(']', p);
			if (o == std::string::npos || (close != std::string::npos && close < o)) break;
			const size_t e = js.find('}', o);
			if (e == std::string::npos) break;
			const std::string obj = js.substr(o, e - o + 1);
			std::string       name, hash, beam;
			JsonStr(obj, "gta", name);
			JsonStr(obj, "hash", hash);
			JsonStr(obj, "beamng", beam);
			char*               end = nullptr;
			const unsigned long h = strtoul(hash.c_str(), &end, 0);
			if (hash.empty() || !end || *end || beam.empty()) {
				++bad;
			} else {
				t.byHash[uint32_t(h)] = SurfIndex(t, beam);
				t.gtaName[uint32_t(h)] = name;
			}
			p = e + 1;
		}
		if (t.byHash.empty()) {
			err = "no usable entries";
			return false;
		}
		if (bad) Log("ground: %d entries in GtaBeamD1_surfaces.json not understood (each needs \"hash\" and \"beamng\")", bad);
		return true;
	}

	// GtaBeamD1_surfaces.json (written with the built-in table when it isn't there)
	void LoadSurfaces()
	{
		char path[MAX_PATH + 16];
		PathNextToModule("_surfaces.json", path, sizeof(path));
		std::string js, err;
		bool        fromFile = false;
		if (FILE* f = fopen(path, "rb")) {
			char buf[4096];
			size_t n;
			while ((n = fread(buf, 1, sizeof(buf), f)) > 0) js.append(buf, n);
			fclose(f);
			fromFile = true;
		}
		SurfTable t;
		if (fromFile && ParseSurfaces(js, t, err)) {
			t.source = path;
		} else {
			if (fromFile) Log("ground: GtaBeamD1_surfaces.json not usable (%s): the built-in table is used", err.c_str());
			ParseSurfaces(kSurfacesDefault, t, err);
			t.source = "built-in table";
			if (!fromFile) {
				if (FILE* f = fopen(path, "wb")) {
					fputs(kSurfacesDefault, f);
					fclose(f);
					t.source = std::string("built-in table (written to ") + path + ")";
				}
			}
		}
		g_surf = t;
		std::string list;
		for (size_t i = 0; i < t.names.size(); ++i) list += (i ? ", " : "") + t.names[i];
		Log("ground: %zu GTA materials mapped to %zu BeamNG ground models (%s), others -> %s; from %s", t.byHash.size(), t.names.size(), list.c_str(),
			t.names[t.def].c_str(), t.source.c_str());
	}

	// v2.42: GTA's ground in BeamNG, third version. A tile = 8 m, n x n points 0.5 m apart, measured coarse to fine: first
	// every 2 m (+ each 2 m cell's middle). A 2 m cell that isn't flat (WorldRefineCm), has two surfaces or a point without
	// ground gets its 1 m points (+ its 1 m cells' middles); a 1 m cell that still isn't flat gets its 0.5 m points. Your
	// v2.41 log: real ground made 68% of the 2 m cells go straight to 0.5 m (~200 rays a tile); now most stop at 1 m.
	// A flat cell is drawn by BeamNG as 2 triangles instead of 32 ("lf" in the tile message). Tiles the car reaches soon at
	// speed go to BeamNG as a 2 m draft as soon as their first pass is done and are replaced when finished. Finished tiles
	// are saved on disk (GtaBeamD1_map, your idea) and measured once only; with rays to spare, the ring around the car is
	// measured ahead of need.
	// v2.43: levels (bridges). Every tile belongs to the level the car's ground has there (the deck when the car is on a
	// bridge, the road under it when it's below): the cache and the saved map keep up to 3 levels of the same spot, and
	// the one matching the car's level goes to BeamNG. The ground far under a deck's edge is left out instead of drawn as a
	// wall down to it. Built ground stays in BeamNG (a static world) up to WorldKeepTris triangles.
	struct WTile
	{
		int                   i = 0, j = 0;
		int                   n = 0;       // points per side
		float                 step = 0.5f;
		int                   K = 4;       // the first pass every K points
		int                   state = 1;   // 1 measuring, 2 finished, 3 gap (no ground at too many points: tried again later)
		int                   pass = 0;    // 0 the first pass, then each finer level
		std::vector<float>    z;           // GTA z per point (row by row along +y, x fastest)
		std::vector<uint32_t> mat;         // GTA ground material per point
		std::vector<uint8_t>  ps;          // 0 not needed (yet), 4 needed, 1 waiting for GTA, 2 known, 3 no ground, 5 another level's ground (left out)
		std::vector<uint8_t>  src;         // known points: 0 measured, 1 copied from a neighbour's edge, 2 filled in
		std::vector<uint8_t>  tries;
		std::vector<uint8_t>  fl;          // rays GTA lost for this point
		std::vector<uint8_t>  deep;        // v2.71: 0 the normal window, 1 looked below it, 2 looked above it (no ground in the normal one)
		uint16_t              deepFound = 0, deepMiss = 0;  // v2.71: points the deep search found ground for / found none above or below
		std::vector<uint8_t>  leaf;        // per 0.5 m cell: the size (in points) of the flat cell it is part of (1 = finest)
		std::vector<std::array<int16_t, 3>> test;  // cells to look at when the current pass is done (a0, b0, size)
		int                   left = 0;    // points needed or waiting
		int                   next = 0;    // first point that may still need a ray
		double                rz = 0, rx = 0, ry = 0, rcx = 0, rcy = 0;  // reference plane for the ray windows
		double                pz = 0, px = 0, py = 0;  // its own plane (fitted to its heights), at its centre
		bool                  planeOk = false;
		double                t0 = 0, tGap = 0;
		int                   gaps = 0, filled = 0, partial = 0;
		int                   fb = 0;          // v2.73: points filled in as a hole between ground (not saved to the map file)
		int                   heal = 0;        // v2.7: times an empty or part-empty tile near the car was measured again (this spot, last 2 min)
		bool                  wide = false;    // v2.7: measured again with a tall ray window (a hill or deck above the level it was made for)
		bool                  healOld = false; // v2.7: its last version stays in BeamNG while it is measured again (kept like a draft)
		std::vector<float>    zOld;            // v2.7: that version's heights (what BeamNG's car stands on until the new one comes)
		bool                  noGround = false;// v2.7: empty because its rays found nothing (not another level's ground left out on purpose)
		int                   rays = 0, copied = 0;
		bool                  draft = false;   // a 2 m draft of it is in BeamNG
		bool                  noDraft = false; // no draft for it (too many points without ground)
		bool                  sent = false;    // the finished tile is in BeamNG
		bool                  acked = false;   // BeamNG built what was sent last
		bool                  pre = false;     // measured ahead of need (the ring around the car): not sent until wanted
		bool                  disk = false;    // from the saved map
		double                lv = 0;          // v2.43: its level: its ground's height at its centre (measured)
		double                lvE = 0;         // v2.43: the level it was measured for (the car's level there at the time)
		int                   voids = 0;       // v2.43: points left out as another level's ground (under a bridge's edge)
		int                   tris = 0;        // v2.43: triangles it has in BeamNG
		bool                  hasRef = false;  // v2.43: measured here for a level (rz...): its plane is fitted to that level's points
		int                   kKey = -1;       // v2.43: the plane through its points known so far (while measuring), for the next tile
		bool                  kOk = false;
		double                kz = 0, kx = 0, ky = 0;
		uint32_t              gen = 0;
		uint32_t              lastUse = 0;
		// v2.5: where its level came from: the neighbour the car reaches first (fromKey), or the car's own ground (refCar;
		// carAir: the car was in the air then, so "its ground" was a guess)
		bool                  hasFrom = false, refCar = false, carAir = false;
		int64_t               fromKey = 0;
		bool                  noLevel = false;  // v2.5: nothing at this level here (beside a deck: only the road far below)
		// v2.5: walls and barriers on it (GTA world, metres): boxes along a face; wallsDone: scanned (or none wanted)
		struct Wall
		{
			float cx, cy, z0, z1;   // centre of the footprint, bottom and top
			float dx, dy;           // along the wall (unit)
			float len, thick;       // length along (dx, dy), thickness across it
			float gs = 0;           // the ground's rise per metre along it (bottom and top follow it)
		};
		std::vector<Wall>     walls;
		bool                  wallsDone = false;
		int                   wallRays = 0, flat = 0;  // rays the scan took; points flattened under walls (no humps)
	};
	struct WRay
	{
		int      handle;
		int64_t  key;
		int      pt;
		uint32_t gen;
		uint32_t tick;
	};
	struct WorldState
	{
		bool     haveO = false;
		double   ox = 0, oy = 0;
		bool     fresh = true;      // the next map message starts a new tile set in BeamNG
		bool     active = false;    // streaming (drive mode with World=1)
		std::map<int64_t, WTile> tiles;  // measuring, finished (in BeamNG or waiting to go), gaps
		std::map<int64_t, std::vector<WTile>> cache;  // finished, not in BeamNG now (sent again without rays); v2.43: up to 3 levels per spot
		std::vector<WRay>        rays;
		int      cap = 384;         // rays waiting at most (lowered when GTA refuses one)
		uint32_t tick = 0, lastRefuseTick = 0;
		uint32_t gen = 1;
		// the car (GTA world): position (drawn origin = bottom of the tyres), velocity, up; heading and turn rate
		double   cx = 0, cy = 0, cz = 0, vx = 0, vy = 0, ux = 0, uy = 0, uz = 1;
		double   hd = 0, yawRate = 0, lastHdT = 0;
		bool     haveHd = false;
		bool     carOk = false;
		bool     grounded = false;  // v2.43: the car is on its ground (not jumping or falling): its level is the one under it
		double   vz = 0;
		// where the car is going: points along its path (position, seconds from now)
		std::vector<std::array<double, 3>> path;
		std::map<int64_t, double> cost;   // wanted tiles -> how soon the car gets there (metres-equivalent)
		std::set<int64_t> wanted, keep;
		double   urgentM = 20, draftM = 0, spd = 0;
		// v2.54: tiles swapped for the car's level stay in BeamNG until their new measurement replaces them (key -> by when;
		// 2.53 took them out at once: a hole the car could reach first), and when each spot was swapped last
		std::unordered_map<int64_t, double> stale, swapT;
		uint32_t swapHeld = 0, staleGone = 0;
		int      pitPts = 0, spikePts = 0;  // (v2.55: points lifted out of pits, lowered from spikes)
		std::vector<std::pair<double, double>> spdHist;  // v2.54: (time, speed) over the last 0.3 s: hard hits are logged
		double   hitLogT = 0;
		int      hitLogs = 0;
		std::map<int64_t, double> preFail;  // ring tiles GTA had no ground for: not tried again until then
		// stats (10 s)
		uint32_t started = 0, refused = 0, hits = 0, misses = 0, timeouts = 0, failed = 0, sent = 0, removed = 0, filledPts = 0, gapTiles = 0, errs = 0;
		uint32_t partialTiles = 0, fromCache = 0, tilesDone = 0, raysDone = 0, copiedPts = 0;
		uint32_t cellsTested = 0, why[4] = {}, leafN[4] = {};  // cells looked at; refined because: 1 no ground, 2 two surfaces, 3 not flat; leaves 2 m / 1 m / 0.5 m
		uint32_t drafts = 0, finals = 0, deferred = 0, fromDisk = 0, preDone = 0, preFailed = 0, trisSent = 0;
		uint32_t syncRays = 0, syncRefused = 0;
		uint32_t voidPts = 0, voidTiles = 0, swaps = 0, keptRm = 0, idleSent = 0, levelMiss = 0;  // v2.43
		uint32_t noLevelTiles = 0, levelMoved = 0;                                                // v2.5
		uint32_t underPts = 0, jointPts = 0;                                                      // v2.62
		uint32_t deepRays = 0, deepHits = 0, aboveKept = 0;                    // v2.71
		uint32_t fbPts = 0, fbTiles = 0, fbWater = 0, fbOpen = 0;                // v2.73: hole fill
		uint32_t soonPartial = 0;                                                                 // v2.63
		// v2.51: jumps: the car in the air (GTA's ground more than 1.2 m under it) and where it will land (its flight followed
		// against GTA's map): the ground there is built first, at the landing's level
		bool     air = false, landOk = false;
		double   airH = 0, airT = 0, airCheckT = 0, groundT = 0;
		double   placedT = 0;  // v2.63: when the car was last put somewhere new (drive start, a put-back): GTA streams its collision there
		double   landX = 0, landY = 0, landZ = 0, landGx = 0, landGy = 0, landT = 0, landAt = 0, landFirstX = 0, landFirstY = 0;
		uint32_t jumps = 0, landRays = 0, landLogs = 0, noFall = 0;
		uint32_t wallTiles = 0, wallBoxes = 0, wallRaysN = 0, wallFlat = 0, wallLate = 0, wallLow = 0;  // v2.5: walls scanned
		double   wallUs = 0, wallUsMax = 0;
		size_t   liveTris = 0;
		double   fillT = 0;
		std::set<int64_t> swapLogged;
		double   syncUs = 0, syncUsMax = 0;
		uint32_t maxWaiting = 0;
		double   waitSum = 0;
		uint32_t waitN = 0;
		double   usSum = 0, usMax = 0;
		uint32_t usN = 0;
		double   aheadMin = 1e9, aheadSum = 0;
		uint32_t aheadN = 0;
		ULONGLONG logMs = 0, matMs = 0;
		std::map<uint32_t, uint64_t> matCount;
		uint32_t wsSeen = 0, mapSeen = 0;
		std::string ws;
		std::set<int64_t> gapLogged;
		std::set<int64_t> healLogged;  // v2.7
		uint32_t          healed = 0;
		std::map<int64_t, std::pair<int, double>> healN;  // v2.7: per spot: times measured again, when last (a level swap makes a new tile: no endless loop)
		bool     matDirty = false;
		float    cfgTile = 0, cfgStep = 0;  // the tile layout the cache was measured with
	} g_world;

	inline int64_t WKey(int i, int j) { return (int64_t(i) << 32) | int64_t(uint32_t(j)); }
	inline void    WIJ(int64_t k, int& i, int& j)
	{
		i = int(k >> 32);
		j = int(int32_t(uint32_t(k & 0xFFFFFFFFu)));
	}
	inline double WTileM() { return g_cfg.worldTileM; }

	// the surface BeamNG draws over a cell with corners A (a0,b0), B (+x), C (+x+y), D (+y): two triangles, A-B-C and A-C-D
	// (the BeamNG mod's order). u, v in 0..1 across the cell.
	inline double TriZ(double zA, double zB, double zC, double zD, double u, double v)
	{
		return u >= v ? zA + (zB - zA) * u + (zC - zB) * v : zA + (zC - zD) * u + (zD - zA) * v;
	}

	// height of a finished tile's grid at (x, y) (false outside or not known)
	bool GridZ(const std::vector<float>& z, int n, float step, int ti, int tj, double x, double y, double& out)
	{
		if (n < 2 || int(z.size()) < n * n) return false;
		const double fx = (x - ti * WTileM()) / step, fy = (y - tj * WTileM()) / step;
		if (fx < -0.01 || fy < -0.01 || fx > n - 1 + 0.01 || fy > n - 1 + 0.01) return false;
		const int    a = std::clamp(int(fx), 0, n - 2), b = std::clamp(int(fy), 0, n - 2);
		const double s = std::clamp(fx - a, 0.0, 1.0), t = std::clamp(fy - b, 0.0, 1.0);
		const float  z00 = z[b * n + a], z10 = z[b * n + a + 1], z01 = z[(b + 1) * n + a], z11 = z[(b + 1) * n + a + 1];
		if (!std::isfinite(z00) || !std::isfinite(z10) || !std::isfinite(z01) || !std::isfinite(z11)) return false;
		out = TriZ(z00, z10, z11, z01, s, t);
		return true;
	}

	// height of a draft (only its first-pass corners are known: BeamNG has 2 m cells there)
	bool DraftZ(const WTile& t, double x, double y, double& out)
	{
		const int    K = std::max(1, t.K), n = t.n;
		const double fx = (x - t.i * WTileM()) / (t.step * K), fy = (y - t.j * WTileM()) / (t.step * K);
		const int    m = (n - 1) / K;
		if (fx < -0.01 || fy < -0.01 || fx > m + 0.01 || fy > m + 0.01) return false;
		const int    a = std::clamp(int(fx), 0, m - 1), b = std::clamp(int(fy), 0, m - 1);
		const double s = std::clamp(fx - a, 0.0, 1.0), v = std::clamp(fy - b, 0.0, 1.0);
		const float  zA = t.z[(b * K) * n + a * K], zB = t.z[(b * K) * n + (a + 1) * K], zC = t.z[((b + 1) * K) * n + (a + 1) * K], zD = t.z[((b + 1) * K) * n + a * K];
		if (!std::isfinite(zA) || !std::isfinite(zB) || !std::isfinite(zC) || !std::isfinite(zD)) return false;
		out = TriZ(zA, zB, zC, zD, s, v);
		return true;
	}

	// ---- v2.43: levels
	struct WPlane
	{
		double z = 0, gx = 0, gy = 0, cx = 0, cy = 0;  // height at (cx, cy) and its slope
	};
	inline double PlaneZ(const WPlane& p, double x, double y) { return p.z + p.gx * (x - p.cx) + p.gy * (y - p.cy); }

	// v2.51: how far around the landing spot its level is taken (and the ground built first)
	inline double LandR() { return std::max(12.0, 0.3 * g_world.spd); }

	WPlane CarPlane()
	{
		const WorldState& w = g_world;
		const double      uz = std::max(0.3, w.uz);
		return WPlane{ w.cz, std::clamp(-w.ux / uz, -1.5, 1.5), std::clamp(-w.uy / uz, -1.5, 1.5), w.cx, w.cy };
	}

	bool FitPlaneCalc(const WTile& t, double& pz, double& px, double& py);
	int  WallRay(const double a[3], const double b[3], double p[3], double n[3], uint32_t* mat = nullptr);

	// a live tile's plane: a finished one's own; one being measured: through its points known so far, once its first
	// pass is done (on a ramp it has the ramp's slope: the next tile up the ramp is measured where the ramp is)
	bool TilePlane(WTile& t, WPlane& p)
	{
		const double cx = (t.i + 0.5) * WTileM(), cy = (t.j + 0.5) * WTileM();
		if (t.state == 2 && t.planeOk) {
			p = WPlane{ t.pz, t.px, t.py, cx, cy };
			return true;
		}
		if (t.state == 1 && t.pass > 0) {
			const int key = t.rays + t.copied * 1000 + t.pass * 1000000;
			if (key != t.kKey) {
				t.kKey = key;
				t.kOk = FitPlaneCalc(t, t.kz, t.kx, t.ky);
			}
			if (t.kOk) {
				p = WPlane{ t.kz, t.kx, t.ky, cx, cy };
				return true;
			}
		}
		return false;
	}

	// how soon the car gets to tile k (its path, then 2 x off it; the tiles it doesn't need: by distance, after them)
	double ChainCost(int64_t k)
	{
		const WorldState& w = g_world;
		auto              c = w.cost.find(k);
		if (c != w.cost.end()) return c->second;
		int i, j;
		WIJ(k, i, j);
		return 1e4 + hypot((i + 0.5) * WTileM() - w.cx, (j + 0.5) * WTileM() - w.cy);
	}

	// the car's level at tile (i, j): the car's own ground on the tile it is on (and next to it); elsewhere carried over
	// from the neighbour the car reaches first on its way there (its path: up a ramp onto a bridge, along the road under
	// one; v2.42 took the car's ground plane anywhere). anchored = false: that neighbour's level isn't known yet (being
	// measured): the tile waits for it
	// v2.5: a level carried to the next tile keeps its slope, but no steeper than a steep hill (your v2.43 log: a plane fitted
	// through a deck and the road under it had slopes of 1.4 to 1.7 and sent the next tiles 15 to 25 m up)
	inline WPlane Carried(WPlane p)
	{
		p.gx = std::clamp(p.gx, -1.2, 1.2);
		p.gy = std::clamp(p.gy, -1.2, 1.2);
		return p;
	}

	WPlane ExpectPlane(int i, int j, bool* anchored = nullptr, const WTile** from = nullptr)
	{
		WorldState&  w = g_world;
		const double T = WTileM();
		const double nx = std::clamp(w.cx, i * T, (i + 1) * T), ny = std::clamp(w.cy, j * T, (j + 1) * T);
		if (anchored) *anchored = true;
		if (from) *from = nullptr;
		// v2.51: around where the car will land (a jump): the landing's level
		if (w.landOk && hypot((i + 0.5) * T - w.landX, (j + 0.5) * T - w.landY) < LandR()) return WPlane{ w.landZ, w.landGx, w.landGy, w.landX, w.landY };
		if (hypot(nx - w.cx, ny - w.cy) < 2.0) return CarPlane();
		const double                                     mine = ChainCost(WKey(i, j));
		std::array<std::pair<double, int64_t>, 8>        nb;
		int                                              n = 0;
		for (int dj = -1; dj <= 1; ++dj)
			for (int di = -1; di <= 1; ++di) {
				if (!di && !dj) continue;
				const int64_t k = WKey(i + di, j + dj);
				const double  c = ChainCost(k);
				if (c < mine) nb[n++] = { c, k };
			}
		std::sort(nb.begin(), nb.begin() + n);
		int empty = 0, absent = 0;
		for (int q = 0; q < n; ++q) {
			auto it = w.tiles.find(nb[q].second);
			if (it == w.tiles.end()) {
				++absent;
				continue;
			}
			WPlane p;
			if (TilePlane(it->second, p)) {
				if (from) *from = &it->second;
				return Carried(p);
			}
			if (it->second.state == 1) {  // its level is coming: wait for it
				if (anchored) *anchored = false;
				return CarPlane();
			}
			++empty;  // a gap, a hole, water: no level there
		}
		// the way from the car is empty (beyond a hole, water): any neighbour with a level, else the car's own ground
		if (empty > 0) {
			double  bestC = 1e30;
			WTile*  best = nullptr;
			WPlane  bp;
			for (int dj = -1; dj <= 1; ++dj)
				for (int di = -1; di <= 1; ++di) {
					if (!di && !dj) continue;
					auto it = w.tiles.find(WKey(i + di, j + dj));
					if (it == w.tiles.end()) continue;
					const double c = ChainCost(it->first);
					WPlane       p;
					if (c < bestC && TilePlane(it->second, p)) {
						bestC = c;
						best = &it->second;
						bp = p;
					}
				}
			if (best) {
				if (from) *from = best;
				return Carried(bp);
			}
			return CarPlane();
		}
		if (anchored) *anchored = n == 0 ? true : false;  // (no neighbour on the way: the car's own ground)
		(void)absent;
		return CarPlane();
	}
	inline double ExpectZ(int i, int j, bool* anchored = nullptr, const WTile** from = nullptr)
	{
		return PlaneZ(ExpectPlane(i, j, anchored, from), (i + 0.5) * WTileM(), (j + 0.5) * WTileM());
	}

	// v2.43: how well tile a meets its neighbour b where they touch (a side: the shared edge; a corner: one point): the
	// mean height difference over the points both have (cnt). The same level shares the same points; another level
	// (the road under a deck) doesn't.
	double EdgeDiff(const WTile& a, const WTile& b, int& cnt)
	{
		cnt = 0;
		const int n = a.n, di = b.i - a.i, dj = b.j - a.j;
		if (b.n != n || n < 2 || abs(di) > 1 || abs(dj) > 1 || (!di && !dj) || int(a.z.size()) < n * n || int(b.z.size()) < n * n) return 0;
		double sum = 0;
		for (int q = 0; q < n; ++q) {
			int ax, ay, bx, by;
			if (di && dj) {
				if (q) break;
				ax = di > 0 ? n - 1 : 0;
				ay = dj > 0 ? n - 1 : 0;
				bx = di > 0 ? 0 : n - 1;
				by = dj > 0 ? 0 : n - 1;
			} else if (di) {
				ax = di > 0 ? n - 1 : 0;
				bx = di > 0 ? 0 : n - 1;
				ay = by = q;
			} else {
				ay = dj > 0 ? n - 1 : 0;
				by = dj > 0 ? 0 : n - 1;
				ax = bx = q;
			}
			const float za = a.z[ay * n + ax], zb = b.z[by * n + bx];
			if (!std::isfinite(za) || !std::isfinite(zb)) continue;
			sum += fabs(double(za) - zb);
			++cnt;
		}
		return cnt ? sum / cnt : 0;
	}

	// points of t far under plane p next to a step (another level's ground under an edge, seen from level p)
	int CliffBelow(const WTile& t, const WPlane& p, double D)
	{
		const int n = t.n;
		if (n < 2 || int(t.z.size()) < n * n) return 0;
		const int reach = std::max(1, int(1.0 / t.step + 1e-6));
		int       cnt = 0;
		for (int b = 0; b < n; ++b)
			for (int a = 0; a < n; ++a) {
				const float z = t.z[b * n + a];
				if (!std::isfinite(z) || z >= PlaneZ(p, t.i * WTileM() + a * t.step, t.j * WTileM() + b * t.step) - D) continue;
				bool edge = false;
				for (int db = -reach; db <= reach && !edge; ++db)
					for (int da = -reach; da <= reach && !edge; ++da) {
						const int a2 = a + da, b2 = b + db;
						if ((!da && !db) || a2 < 0 || b2 < 0 || a2 >= n || b2 >= n || sqrt(double(da * da + db * db)) * t.step > 1.01) continue;
						const float z2 = t.z[b2 * n + a2];
						edge = std::isfinite(z2) && z2 - z > D;
					}
				cnt += edge;
			}
		return cnt;
	}
	int VoidCount(const WTile& t)
	{
		int c = 0;
		for (float z : t.z) c += !std::isfinite(z);
		return c;
	}

	// a measurement (stored, or made for another level) fits the car's level at its spot: within WorldLevelM of it; no
	// ground far under it next to a step (a ramp measured from the road beside it: its far end is the road); points left
	// out only if it was measured for this level (they are the road for the road's level); and it meets the neighbour the
	// level came from (from = nullptr: the car's own ground)
	// v2.43: how far the level a tile was measured for is from plane p (its 4 corners; -1: not known)
	double RefOff(const WTile& t, const WPlane& p)
	{
		if (!t.hasRef) return -1;
		const double T = WTileM();
		double       m = 0;
		for (int c = 0; c < 4; ++c) {
			const double x = (t.i + (c & 1)) * T, y = (t.j + (c >> 1)) * T;
			m = std::max(m, fabs(t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy) - PlaneZ(p, x, y)));
		}
		return m;
	}

	bool LevelFits(const WTile& cand, const WPlane& p, const WTile* from)
	{
		const double E = PlaneZ(p, (cand.i + 0.5) * WTileM(), (cand.j + 0.5) * WTileM());
		if (fabs(cand.lv - E) >= g_cfg.worldLevelM) return false;
		// measured for another level (its rays looked from somewhere else: a ramp measured from the road beside it misses
		// the ramp's high end)
		if (RefOff(cand, p) > 1.5) return false;
		if (g_cfg.worldDropM > 0 && CliffBelow(cand, p, g_cfg.worldDropM + 1.0) > 0) return false;
		if (fabs(cand.lvE - E) > 1.5 && VoidCount(cand) > 0) return false;
		if (!from) return true;
		int          cnt = 0;
		const double d = EdgeDiff(cand, *from, cnt);
		return cnt < 2 || d < 0.75;
	}

	// the cached measurement of spot k at level E (nullptr: none within WorldLevelM; with the car's level plane p: none that
	// fits it, LevelFits)
	WTile* CacheFind(int64_t k, double E, size_t* idx = nullptr, const WPlane* p = nullptr, const WTile* from = nullptr)
	{
		auto c = g_world.cache.find(k);
		if (c == g_world.cache.end()) return nullptr;
		WTile* best = nullptr;
		double bestD = g_cfg.worldLevelM;
		for (size_t q = 0; q < c->second.size(); ++q) {
			const double d = fabs(c->second[q].lv - E);
			if (d < bestD && (!p || LevelFits(c->second[q], *p, from))) {
				bestD = d;
				best = &c->second[q];
				if (idx) *idx = q;
			}
		}
		return best;
	}
	// ... taken out of the cache
	bool CacheTake(int64_t k, double E, WTile& out, const WPlane* p = nullptr, const WTile* from = nullptr)
	{
		size_t q = 0;
		if (!CacheFind(k, E, &q, p, from)) return false;
		auto c = g_world.cache.find(k);
		out = std::move(c->second[q]);
		c->second.erase(c->second.begin() + ptrdiff_t(q));
		if (c->second.empty()) g_world.cache.erase(c);
		return true;
	}
	size_t CacheCount()
	{
		size_t n = 0;
		for (auto& kv : g_world.cache) n += kv.second.size();
		return n;
	}

	// GTA's ground under (x, y) from what BeamNG has or will have (finished tiles, drafts, the cache: the level nearest zHint,
	// else the car's)
	bool WorldGroundAt(double x, double y, double& z, bool* acked = nullptr, double zHint = NAN)
	{
		const int ti = int(floor(x / WTileM())), tj = int(floor(y / WTileM()));
		const int64_t k = WKey(ti, tj);
		auto      it = g_world.tiles.find(k);
		if (it != g_world.tiles.end()) {
			const WTile& t = it->second;
			if (t.state == 2 && t.sent) {
				if (acked) *acked = t.acked;
				return GridZ(t.z, t.n, t.step, ti, tj, x, y, z);
			}
			if (t.healOld && !t.zOld.empty()) {  // v2.7: measured again: BeamNG still has its last version
				if (acked) *acked = t.acked;
				return GridZ(t.zOld, t.n, t.step, ti, tj, x, y, z);
			}
			if (t.draft) {
				if (acked) *acked = t.acked;
				return DraftZ(t, x, y, z);
			}
			if (t.state == 2) {
				if (acked) *acked = false;
				return GridZ(t.z, t.n, t.step, ti, tj, x, y, z);
			}
		}
		auto c = g_world.cache.find(k);
		if (c != g_world.cache.end() && !c->second.empty()) {
			const double hint = std::isfinite(zHint) ? zHint : g_world.cz;
			const WTile* best = &c->second[0];
			for (const WTile& v : c->second)
				if (fabs(v.lv - hint) < fabs(best->lv - hint)) best = &v;
			if (acked) *acked = false;
			return GridZ(best->z, best->n, best->step, ti, tj, x, y, z);
		}
		return false;
	}

	int SurfOf(uint32_t mat)
	{
		auto it = g_surf.byHash.find(mat);
		return it == g_surf.byHash.end() ? g_surf.def : it->second;
	}

	// the reference height a ray at point k looks around: the known corners of the smallest cell around it, else the
	// plane. v2.43: the plane (the level this tile is measured for) when those corners are far from it: they are another
	// level (the road under a deck's edge); a window around them would start under the deck and miss it
	double RefZ(const WTile& t, int k)
	{
		const int    a = k % t.n, b = k / t.n;
		const double x = t.i * WTileM() + a * t.step, y = t.j * WTileM() + b * t.step;
		const double plane = t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy);
		for (int s = 2; s <= std::max(2, t.K); s *= 2) {
			const int a0 = std::min(a / s * s, t.n - 1 - s), b0 = std::min(b / s * s, t.n - 1 - s);
			if (a0 < 0 || b0 < 0) break;
			const int c[4] = { b0 * t.n + a0, b0 * t.n + a0 + s, (b0 + s) * t.n + a0, (b0 + s) * t.n + a0 + s };
			double    sum = 0;
			int       cnt = 0;
			for (int q : c)
				if (t.ps[q] == 2 && std::isfinite(t.z[q])) {
					sum += t.z[q];
					++cnt;
				}
			if (cnt >= 2) {
				const double avg = sum / cnt;
				if (t.hasRef && g_cfg.worldDropM > 0 && fabs(avg - plane) > g_cfg.worldDropM) return plane;
				return avg;
			}
		}
		return plane;
	}

	// copy the shared edge from a finished neighbour (the same points: no ray needed, and no step between the tiles)
	void CopyEdge(WTile& t, const WTile& nb, int di, int dj)
	{
		if (nb.n != t.n || fabs(nb.step - t.step) > 1e-4f || nb.state != 2) return;
		const int n = t.n;
		for (int s = 0; s < n; ++s) {
			int mine, theirs;
			if (di == -1) { mine = s * n; theirs = s * n + (n - 1); }
			else if (di == 1) { mine = s * n + (n - 1); theirs = s * n; }
			else if (dj == -1) { mine = s; theirs = (n - 1) * n + s; }
			else { mine = (n - 1) * n + s; theirs = s; }
			if (!std::isfinite(nb.z[theirs]) || t.ps[mine] == 2) continue;
			if (t.hasRef) {
				// v2.43: only points of this tile's level (not another level's ground at a deck's edge)
				const double x = t.i * WTileM() + (mine % n) * t.step, y = t.j * WTileM() + (mine / n) * t.step;
				if (fabs(nb.z[theirs] - (t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy))) > g_cfg.worldLevelM) continue;
			}
			t.z[mine] = nb.z[theirs];
			t.mat[mine] = nb.mat[theirs];
			t.ps[mine] = 2;
			t.src[mine] = 1;
			++t.copied;
		}
	}

	// a measured tile of spot k on the level of plane ref (live or cached; v2.43: never another level's)
	const WTile* FindLevel(int64_t k, const WPlane& ref)
	{
		int i, j;
		WIJ(k, i, j);
		const double E = PlaneZ(ref, (i + 0.5) * WTileM(), (j + 0.5) * WTileM());
		auto         it = g_world.tiles.find(k);
		if (it != g_world.tiles.end() && it->second.state == 2 && fabs(it->second.lv - E) < g_cfg.worldLevelM) return &it->second;
		return CacheFind(k, E);
	}
	// any measured tile of spot k (any level)
	const WTile* FindAny(int64_t k)
	{
		auto it = g_world.tiles.find(k);
		if (it != g_world.tiles.end() && it->second.state == 2) return &it->second;
		auto c = g_world.cache.find(k);
		if (c != g_world.cache.end() && !c->second.empty()) return &c->second[0];
		return nullptr;
	}

	// the first pass: every K points and each K cell's middle; the cells to look at afterwards
	int NeedCoarse(WTile& t)
	{
		int       need = 0;
		const int K = t.K, h = K / 2;
		for (int b = 0; b < t.n; ++b)
			for (int a = 0; a < t.n; ++a) {
				const bool corner = a % K == 0 && b % K == 0;
				const bool centre = K >= 2 && a % K == h && b % K == h;
				const int  k = b * t.n + a;
				if ((corner || centre) && t.ps[k] == 0) {
					t.ps[k] = 4;
					++need;
				}
			}
		t.test.clear();
		if (K > 1)
			for (int b0 = 0; b0 + K <= t.n - 1; b0 += K)
				for (int a0 = 0; a0 + K <= t.n - 1; a0 += K) t.test.push_back({ int16_t(a0), int16_t(b0), int16_t(K) });
		t.pass = 0;
		return need;
	}

	WTile MakeTile(int i, int j)
	{
		WorldState& w = g_world;
		WTile       t;
		t.i = i;
		t.j = j;
		t.step = g_cfg.worldStepNear;
		t.n = int(WTileM() / t.step + 0.5) + 1;
		// the first pass every K points: a power of two that divides the tile (2 m at 0.5 m: 4)
		int K = 1;
		while (K * 2 * t.step <= g_cfg.worldCoarseM + 1e-3 && (t.n - 1) % (K * 2) == 0) K *= 2;
		t.K = K;
		const int np = t.n * t.n;
		t.z.assign(np, NAN);
		t.mat.assign(np, 0);
		t.ps.assign(np, 0);
		t.src.assign(np, 0);
		t.tries.assign(np, 0);
		t.fl.assign(np, 0);
		t.deep.assign(np, 0);
		t.leaf.assign(size_t(t.n - 1) * (t.n - 1), 1);
		t.t0 = NowSeconds();
		t.gen = ++w.gen;
		t.lastUse = w.tick;
		// the reference plane for the ray windows: the car's level there (v2.43: carried over from the live neighbour nearest
		// the car, else the car's own ground plane; v2.42 could take a cached neighbour of another level: a bridge deck's
		// height for the road under it)
		const WTile* from = nullptr;
		const WPlane ref = ExpectPlane(i, j, nullptr, &from);
		t.rcx = ref.cx;
		t.rcy = ref.cy;
		t.rz = ref.z;
		t.rx = ref.gx;
		t.ry = ref.gy;
		t.lvE = t.lv = PlaneZ(ref, (i + 0.5) * WTileM(), (j + 0.5) * WTileM());
		t.hasRef = true;
		// v2.5: where the level came from
		t.hasFrom = from != nullptr;
		t.fromKey = from ? WKey(from->i, from->j) : 0;
		t.refCar = from == nullptr;
		t.carAir = t.refCar && !w.grounded;
		// the edges finished neighbours of the same level already have
		const int nb4[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		for (auto& d : nb4)
			if (const WTile* nb = FindLevel(WKey(i + d[0], j + d[1]), ref)) CopyEdge(t, *nb, d[0], d[1]);
		w.copiedPts += t.copied;
		t.left = NeedCoarse(t);
		return t;
	}

	// v2.43: a tile starts next to the car, or next to a live tile whose level is known (its first pass done): the car's
	// level is carried tile by tile (v2.42 started tiles anywhere from the car's own ground plane, far ahead too: a bridge
	// deck ahead was measured at the road's height under it)
	bool TileStartable(int i, int j)
	{
		bool anchored = false;
		ExpectPlane(i, j, &anchored);
		return anchored;
	}

	// v2.43: twice: the second time without points more than 2.5 m off the first plane (a wall's top, a kerb of another
	// level), so a tile's level is its road's
	// (a tile measured for a level: only the points within WorldLevelM of that level, unless too few)
	bool FitPlaneCalc(const WTile& t, double& opz, double& opx, double& opy)
	{
		bool   planeOk = false;
		double ppz = 0, ppx = 0, ppy = 0;
		double win = g_cfg.worldLevelM;
		bool   useRef = t.hasRef;
		if (useRef) {
			// the points within 1.25 m of the level it is measured for (a ramp's points, not the ground beside it), else
			// within WorldLevelM. v2.5: never all of them (your v2.43 log: a tile measured for the road took the deck 12 m over
			// it, or the ground 28 m under it), unless its level was the car's own ground while the car was in the air (a jump:
			// where it lands is its new level)
			int inT = 0, inW = 0, all = 0;
			for (int b = 0; b < t.n; ++b)
				for (int a = 0; a < t.n; ++a) {
					const float z = t.z[b * t.n + a];
					if (!std::isfinite(z)) continue;
					const double x = t.i * WTileM() + a * t.step, y = t.j * WTileM() + b * t.step;
					const double d = fabs(z - (t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy)));
					inT += d <= 1.25;
					inW += d <= win;
					++all;
				}
			if (inT >= std::max(6, all * 3 / 10)) win = 1.25;
			useRef = inW >= 3;
			if (!useRef && !(t.refCar && t.carAir) && !t.disk) return false;
		}
		// v2.5: passes 1-3 take the points near the plane found so far (not near the level it was measured for): on a ramp the
		// plane walks up to the ramp's own slope (a level carried flat up a ramp measured the ground under its top: your falls
		// at speed onto a bridge)
		const double walkTol = useRef ? std::min(win, 1.25) : 2.5;
		for (int pass = 0; pass < 4; ++pass) {
			const bool   had = planeOk;
			const double pz0 = ppz, px0 = ppx, py0 = ppy;
			if (pass >= 1 && !had) break;
			const double cx = (t.i + 0.5) * WTileM(), cy = (t.j + 0.5) * WTileM();
			double       sxx = 0, syy = 0, sxy = 0, sx = 0, sy = 0, sz = 0, sxz = 0, syz = 0, n = 0;
			for (int b = 0; b < t.n; ++b)
				for (int a = 0; a < t.n; ++a) {
					const float z = t.z[b * t.n + a];
					if (!std::isfinite(z)) continue;
					const double x = t.i * WTileM() + a * t.step - cx, y = t.j * WTileM() + b * t.step - cy;
					if (pass == 0 && useRef && fabs(z - (t.rz + t.rx * (x + cx - t.rcx) + t.ry * (y + cy - t.rcy))) > win) continue;
					if (pass >= 1 && useRef && fabs(z - (t.rz + t.rx * (x + cx - t.rcx) + t.ry * (y + cy - t.rcy))) > g_cfg.worldLevelM) continue;
					if (pass >= 1 && fabs(z - (pz0 + px0 * x + py0 * y)) > walkTol) continue;
					sxx += x * x;
					syy += y * y;
					sxy += x * y;
					sx += x;
					sy += y;
					sz += z;
					sxz += x * z;
					syz += y * z;
					n += 1;
				}
			if (n < 3) break;
			double A[3][4] = { { n, sx, sy, sz }, { sx, sxx, sxy, sxz }, { sy, sxy, syy, syz } };
			bool   ok = true;
			for (int c = 0; c < 3 && ok; ++c) {
				int piv = c;
				for (int r = c + 1; r < 3; ++r)
					if (fabs(A[r][c]) > fabs(A[piv][c])) piv = r;
				if (fabs(A[piv][c]) < 1e-9) {
					ok = false;
					break;
				}
				if (piv != c)
					for (int k = 0; k < 4; ++k) std::swap(A[c][k], A[piv][k]);
				for (int r = 0; r < 3; ++r) {
					if (r == c) continue;
					const double f = A[r][c] / A[c][c];
					for (int k = c; k < 4; ++k) A[r][k] -= f * A[c][k];
				}
			}
			if (!ok) break;
			ppz = A[0][3] / A[0][0];
			ppx = std::clamp(A[1][3] / A[1][1], -2.0, 2.0);
			ppy = std::clamp(A[2][3] / A[2][2], -2.0, 2.0);
			planeOk = true;
			if (pass >= 1 && fabs(ppz - pz0) < 0.02 && fabs(ppx - px0) < 0.005 && fabs(ppy - py0) < 0.005) break;  // (settled)
		}
		if (planeOk) {
			opz = ppz;
			opx = ppx;
			opy = ppy;
		}
		return planeOk;
	}
	void FitPlane(WTile& t) { t.planeOk = FitPlaneCalc(t, t.pz, t.px, t.py); }

	int MajoritySurf(const int* s4, int cnt)
	{
		int bestS = s4[0], bestC = 0;
		for (int k = 0; k < cnt; ++k) {
			int c = 0;
			for (int q = 0; q < cnt; ++q) c += s4[q] == s4[k];
			if (c > bestC) {
				bestC = c;
				bestS = s4[k];
			}
		}
		return bestS;
	}

	// to BeamNG: the finished tile (draft = false), or its 2 m draft (only the first pass's corners)
	void WorldSendTile(WTile& t, bool draft)
	{
		WorldState& w = g_world;
		const int   n = t.n;
		const int   S = draft ? t.K : 1;          // send every S-th point
		const int   m = (n - 1) / S + 1;          // points per side in the message
		if (draft) {
			// a draft only where its 2 m cells have ground (a hole or water: no draft, the finished tile follows)
			int miss = 0, okCells = 0;
			for (int b = 0; b < m; ++b)
				for (int a = 0; a < m; ++a) miss += !std::isfinite(t.z[(b * S) * n + a * S]);
			for (int b = 0; b < m - 1; ++b)
				for (int a = 0; a < m - 1; ++a) {
					const int k = (b * S) * n + a * S;
					okCells += std::isfinite(t.z[k]) && std::isfinite(t.z[k + S]) && std::isfinite(t.z[k + S * n]) && std::isfinite(t.z[k + S * n + S]);
				}
			if (!okCells || miss * 4 > m * m) {
				t.noDraft = true;
				return;
			}
		}
		float       zmin = 1e30f;
		for (int b = 0; b < m; ++b)
			for (int a = 0; a < m; ++a) {
				const float z = t.z[(b * S) * n + a * S];
				if (std::isfinite(z)) zmin = std::min(zmin, z);
			}
		if (zmin > 1e29f) return;
		// cells: the surface most of its 4 corners have
		std::vector<int> pal;
		std::string      cells, leaves;
		bool             anyLeaf = false;
		int              tris = 0;
		cells.reserve(size_t(m - 1) * (m - 1));
		for (int b = 0; b < m - 1; ++b)
			for (int a = 0; a < m - 1; ++a) {
				const int k00 = (b * S) * n + a * S, k10 = k00 + S, k01 = k00 + S * n, k11 = k01 + S;
				const int s4[4] = { SurfOf(t.mat[k00]), SurfOf(t.mat[k10]), SurfOf(t.mat[k01]), SurfOf(t.mat[k11]) };
				const int bestS = MajoritySurf(s4, 4);
				int       slot = -1;
				for (size_t q = 0; q < pal.size(); ++q)
					if (pal[q] == bestS) slot = int(q);
				if (slot < 0) {
					if (pal.size() >= 36) slot = 0;
					else {
						pal.push_back(bestS);
						slot = int(pal.size() - 1);
					}
				}
				cells += "0123456789abcdefghijklmnopqrstuvwxyz"[slot];
				if (!draft) {
					const int lf = t.leaf[b * (n - 1) + a];
					anyLeaf = anyLeaf || lf > 1;
					leaves += char('0' + std::min(lf, 9));
					if (a % lf == 0 && b % lf == 0) tris += 2;
				} else {
					tris += 2;
				}
			}
		std::string msg;
		msg.reserve(size_t(m) * m * 5 + cells.size() * 2 + 256);
		char buf[256];
		w.stale.erase(WKey(t.i, t.j));  // v2.54: this replaces a swapped tile's old measurement in BeamNG
		// v2.52: heights in mm ("zs"): 2.51 sent cm, and on sloped roads that rounding left a step every 0.5 m (jolts at speed)
		snprintf(buf, sizeof(buf), "{\"t\":\"hf\",\"id\":\"g_%d_%d\",\"x0\":%.3f,\"y0\":%.3f,\"z0\":%.3f,\"st\":%.4f,\"n\":%d,\"zs\":0.001,%s\"z\":[", t.i, t.j, t.i * WTileM() - w.ox,
			t.j * WTileM() - w.oy, double(zmin) + g_cfg.worldZLift, t.step * S, m, draft ? "\"d\":1," : "");
		msg += buf;
		for (int b = 0; b < m; ++b)
			for (int a = 0; a < m; ++a) {
				const float z = t.z[(b * S) * n + a * S];
				// -999999 = no ground here: the BeamNG mod leaves out the cells around it
				snprintf(buf, sizeof(buf), (a || b) ? ",%d" : "%d", std::isfinite(z) ? int(lround((z - zmin) * 1000.0)) : -999999);
				msg += buf;
			}
		msg += "],\"s\":[";
		for (size_t q = 0; q < pal.size(); ++q) {
			msg += q ? ",\"" : "\"";
			msg += g_surf.names[pal[q]];
			msg += "\"";
		}
		msg += "],\"m\":\"" + cells + "\"";
		if (anyLeaf) msg += ",\"lf\":\"" + leaves + "\"";  // v2.42: flat cells as one (the BeamNG mod draws 2 triangles for each)
		// v2.5: its walls (boxes): centre x y (from the tile's corner), bottom, top (from z0, at the centre), along x y, length,
		// thickness, rise per metre along it
		if (!draft && !t.walls.empty()) {
			msg += ",\"w\":[";
			bool first = true;
			for (const WTile::Wall& wl : t.walls) {
				snprintf(buf, sizeof(buf), "%s%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f,%.4f", first ? "" : ",", wl.cx - t.i * WTileM(), wl.cy - t.j * WTileM(),
					double(wl.z0) - zmin, double(wl.z1) - zmin, wl.dx, wl.dy, wl.len, wl.thick, wl.gs);
				msg += buf;
				first = false;
				tris += 22;  // (v2.52: corners cut)
			}
			msg += "]";
		}
		msg += "}";
		DrvSend(msg.c_str());
		// v2.61: WorldDebug=1: each finished tile within 25 m of the car as BeamNG gets it (heights in cm over its lowest point, '.' no
		// ground; its walls), in GtaBeamD1_tiles_dbg.txt: for the truss bridge and the barrier teeth
		if (g_cfg.worldDebug && !draft && hypot(t.i * WTileM() + WTileM() * 0.5 - w.cx, t.j * WTileM() + WTileM() * 0.5 - w.cy) < 25.0) {
			static FILE* df = nullptr;
			static long  dsz = 0;
			if (!df) {
				char path[MAX_PATH + 16];
				PathNextToModule("_tiles_dbg.txt", path, sizeof(path));
				df = fopen(path, "wb");
			}
			if (df && dsz < 8000000) {
				std::string o;
				snprintf(buf, sizeof(buf), "tile %d,%d corner GTA %.2f %.2f, %d x %d points %.2f m apart, lowest %.3f, level %.2f, %zu walls (rows along +y, x left to right)\r\n", t.i, t.j,
					t.i * WTileM(), t.j * WTileM(), n, n, t.step, double(zmin), t.lv, t.walls.size());
				o += buf;
				for (int b = 0; b < n; ++b) {
					for (int a = 0; a < n; ++a) {
						const float z = t.z[b * n + a];
						snprintf(buf, sizeof(buf), std::isfinite(z) ? "%6.0f" : "     .", std::isfinite(z) ? (z - zmin) * 100.0 : 0.0);
						o += buf;
					}
					o += "\r\n";
				}
				for (const WTile::Wall& wl : t.walls) {
					snprintf(buf, sizeof(buf), "  wall at %.2f %.2f, %.2f m long, %.2f thick, z %.2f to %.2f, along %.2f %.2f\r\n", wl.cx, wl.cy, wl.len, wl.thick, wl.z0, wl.z1, wl.dx, wl.dy);
					o += buf;
				}
				fputs(o.c_str(), df);
				fflush(df);
				dsz += long(o.size());
			}
		}
		t.acked = false;
		t.lastUse = w.tick;
		if (draft) {
			t.draft = true;
			++w.drafts;
		} else {
			t.sent = true;
			++w.finals;
		}
		w.trisSent += tris;
		t.tris = tris;
		++w.sent;
	}

	// a cell (a0, b0, size s in points): 0 flat (BeamNG's 2 triangles over it fit every point known inside it within
	// WorldRefineCm), 1 a point without ground, 2 two surfaces (only cells bigger than 1 m), 3 not flat
	int CellCheck(const WTile& t, int a0, int b0, int s, bool surfaces)
	{
		const int n = t.n;
		const int A = b0 * n + a0, B = A + s, C = A + s * n + s, D = A + s * n;
		if (t.ps[A] != 2 || t.ps[B] != 2 || t.ps[C] != 2 || t.ps[D] != 2) return 1;
		// v2.42: soft ground (grass, dirt...) gets a looser test: its small bumps matter less than a road's
		const bool   soft = SurfSoft(SurfOf(t.mat[A])) && SurfSoft(SurfOf(t.mat[B])) && SurfSoft(SurfOf(t.mat[C])) && SurfSoft(SurfOf(t.mat[D]));
		const double tol = (soft ? g_cfg.worldRefineSoftCm : g_cfg.worldRefineCm) / 100.0;
		const double zA = t.z[A], zB = t.z[B], zC = t.z[C], zD = t.z[D];
		for (int b = b0; b <= b0 + s; ++b)
			for (int a = a0; a <= a0 + s; ++a) {
				const int k = b * n + a;
				if (t.ps[k] == 3) return 1;
				if (t.ps[k] != 2 || t.src[k] == 2) continue;
				const double zt = TriZ(zA, zB, zC, zD, double(a - a0) / s, double(b - b0) / s);
				if (fabs(t.z[k] - zt) > tol) return 3;
			}
		if (surfaces && s * t.step > 1.01) {
			const int s0 = SurfOf(t.mat[A]);
			const int ce = (b0 + s / 2) * n + a0 + s / 2;
			if (SurfOf(t.mat[B]) != s0 || SurfOf(t.mat[C]) != s0 || SurfOf(t.mat[D]) != s0) return 2;
			if (t.ps[ce] == 2 && SurfOf(t.mat[ce]) != s0) return 2;
		}
		return 0;
	}

	void SetLeaf(WTile& t, int a0, int b0, int s, int v)
	{
		for (int b = b0; b < b0 + s; ++b)
			for (int a = a0; a < a0 + s; ++a) t.leaf[b * (t.n - 1) + a] = uint8_t(v);
	}

	// a cell gets its next finer points: every s/2 inside it, and (when s/2 >= 2) the middles of its 4 sub-cells, which
	// are looked at after this pass
	int MarkSub(WTile& t, int a0, int b0, int s, std::vector<std::array<int16_t, 3>>& next)
	{
		const int n = t.n, h = s / 2;
		int       need = 0;
		auto      want = [&](int a, int b) {
            const int k = b * n + a;
            if (t.ps[k] == 0) {
                t.ps[k] = 4;
                ++need;
            }
		};
		for (int b = b0; b <= b0 + s; b += h)
			for (int a = a0; a <= a0 + s; a += h) want(a, b);
		if (h >= 2) {
			for (int sb = 0; sb < 2; ++sb)
				for (int sa = 0; sa < 2; ++sa) {
					want(a0 + sa * h + h / 2, b0 + sb * h + h / 2);
					next.push_back({ int16_t(a0 + sa * h), int16_t(b0 + sb * h), int16_t(h) });
				}
		}
		return need;
	}

	// a finished tile goes to the saved map (below)
	void MapPut(const WTile& t);

	// v2.43: the ground far under this tile's level next to an edge (a bridge deck seen from the deck: the road 8 m below)
	// is another level: left out (BeamNG gets no cells there) instead of drawn as a wall down to it. Points more than
	// WorldDropM under a point less than 1 m away, and all ground joined to them that is that far under the tile's level.
	// A slope (a hill, a ramp) has no such step and stays; a wall going UP stays too (it stops the car, as in GTA).
	int VoidBelow(WTile& t)
	{
		const double D = g_cfg.worldDropM;
		if (D <= 0) return 0;
		const int    n = t.n;
		const double T = WTileM();
		const int    reach = std::max(1, int(1.0 / t.step + 1e-6));  // points within 1 m
		auto         low = [&](int a, int b) {
            const int k = b * n + a;
            if (!std::isfinite(t.z[k]) || t.ps[k] != 2) return false;
            const double x = t.i * T + a * t.step, y = t.j * T + b * t.step;
            return t.z[k] < t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy) - D;
		};
		std::vector<uint8_t> mark(size_t(n) * n, 0);
		std::vector<int>     todo;
		for (int b = 0; b < n; ++b)
			for (int a = 0; a < n; ++a) {
				if (!low(a, b)) continue;
				const float z = t.z[b * n + a];
				bool        edge = false;
				for (int db = -reach; db <= reach && !edge; ++db)
					for (int da = -reach; da <= reach && !edge; ++da) {
						const int a2 = a + da, b2 = b + db;
						if ((!da && !db) || a2 < 0 || b2 < 0 || a2 >= n || b2 >= n) continue;
						if (sqrt(double(da * da + db * db)) * t.step > 1.01) continue;
						const float z2 = t.z[b2 * n + a2];
						edge = std::isfinite(z2) && t.ps[b2 * n + a2] == 2 && z2 - z > D;
					}
				if (edge) {
					mark[b * n + a] = 1;
					todo.push_back(b * n + a);
				}
			}
		while (!todo.empty()) {
			const int k = todo.back();
			todo.pop_back();
			const int a = k % n, b = k / n;
			const int nb[4][2] = { { a - 1, b }, { a + 1, b }, { a, b - 1 }, { a, b + 1 } };
			for (auto& q : nb) {
				if (q[0] < 0 || q[1] < 0 || q[0] >= n || q[1] >= n) continue;
				const int k2 = q[1] * n + q[0];
				if (mark[k2] || !low(q[0], q[1])) continue;
				mark[k2] = 1;
				todo.push_back(k2);
			}
		}
		int cnt = 0;
		for (int k = 0; k < n * n; ++k)
			if (mark[k]) {
				t.z[k] = NAN;
				t.ps[k] = 5;
				++cnt;
			}
		t.voids = cnt;
		if (!cnt) return 0;
		// flat cells with a point left out: drawn per 0.5 m cell (its cells with ground stay)
		for (int b = 0; b < n - 1; ++b)
			for (int a = 0; a < n - 1; ++a) {
				const int s = t.leaf[b * (n - 1) + a];
				if (s < 2 || a % s || b % s) continue;
				bool any = false;
				for (int bb = b; bb <= b + s && !any; ++bb)
					for (int aa = a; aa <= a + s && !any; ++aa) any = mark[bb * n + aa] != 0;
				if (any) SetLeaf(t, a, b, s, 1);
			}
		return cnt;
	}

	// v2.43: a tile that turned out empty at this level: its draft (2 m points, no edge test yet) out of BeamNG
	void WorldDropDraft(WTile& t)
	{
		if (!t.draft || t.sent) return;
		char m[96];
		snprintf(m, sizeof(m), "{\"t\":\"rm\",\"ids\":[\"g_%d_%d\"]}", t.i, t.j);
		DrvSend(m);
		t.draft = false;
		t.tris = 0;
		t.healOld = false;  // v2.7
		std::vector<float>().swap(t.zOld);
	}

	bool AnyCell(const WTile& t)
	{
		for (int b = 0; b + 1 < t.n; ++b)
			for (int a = 0; a + 1 < t.n; ++a) {
				const int k = b * t.n + a;
				if (std::isfinite(t.z[k]) && std::isfinite(t.z[k + 1]) && std::isfinite(t.z[k + t.n]) && std::isfinite(t.z[k + t.n + 1])) return true;
			}
		return false;
	}

	// v2.5: does this tile's own ground meet the neighbour its level came from along their shared side (the same road going
	// on, its level carried a little wrong), or not (beside a deck: only the road far under it)? The median step between the
	// neighbour's edge points and this tile's first measured points within 1 m of that edge, under 1 m
	bool MeetsTile(const WTile& t, const WTile& nb);
	// v2.73: the tile its level came from, or (your v2.72 hill holes: "nothing at the car's level, left empty") any finished
	// tile beside it: its measured ground meets that tile's at their shared edge = the same ground, its level was carried wrong
	bool MeetsFrom(const WTile& t)
	{
		if (t.hasFrom) {
			auto it = g_world.tiles.find(t.fromKey);
			if (it != g_world.tiles.end() && MeetsTile(t, it->second)) return true;
		}
		for (const auto& d : { std::array<int, 2>{ 1, 0 }, std::array<int, 2>{ -1, 0 }, std::array<int, 2>{ 0, 1 }, std::array<int, 2>{ 0, -1 } }) {
			auto it = g_world.tiles.find(WKey(t.i + d[0], t.j + d[1]));
			if (it != g_world.tiles.end() && it->second.state == 2 && (!t.hasFrom || WKey(t.i + d[0], t.j + d[1]) != t.fromKey) && MeetsTile(t, it->second)) return true;
		}
		return false;
	}
	bool MeetsTile(const WTile& t, const WTile& nb)
	{
		const int    n = t.n, di = nb.i - t.i, dj = nb.j - t.j;
		if (nb.n != n || int(nb.z.size()) < n * n || (di && dj) || (!di && !dj) || abs(di) > 1 || abs(dj) > 1) return false;
		std::vector<double> d;
		const int           reach = std::max(1, int(1.0 / t.step + 1e-6));
		for (int s = 0; s < n; ++s) {
			const int   nbk = di == -1 ? s * n + (n - 1) : di == 1 ? s * n : dj == -1 ? (n - 1) * n + s : s;
			const float zn = nb.z[nbk];
			if (!std::isfinite(zn)) continue;
			for (int r = 1; r <= reach; ++r) {
				const int a = di == -1 ? r : di == 1 ? n - 1 - r : s;
				const int b = di ? s : dj == -1 ? r : n - 1 - r;
				const int k = b * n + a;
				if (t.ps[k] != 2 || t.src[k] != 0 || !std::isfinite(t.z[k])) continue;
				d.push_back(fabs(double(t.z[k]) - zn));
				break;
			}
		}
		if (d.size() < 3) return false;
		std::nth_element(d.begin(), d.begin() + d.size() / 2, d.end());
		return d[d.size() / 2] < 1.0;
	}

	// v2.55: narrow spikes and pits in a measured tile. A spike: over 1 m above everything within 1 m around it (a truss
	// member, a beam or a thin wall top the ray met before the road: the tall panels on your bridge in BeamNG); a pit: 15 cm
	// or more below (a crack, a drain or a pothole where the ray slipped through to the ground under it: your "holes in the
	// hill roads"). Spikes go down, pits up, to the ground around them: a morphological opening, then a closing, over 1 m.
	// Steps (curbs, a deck's edge) and slopes stay as they are; walls are found by their own rays; points copied from a
	// finished neighbour are never changed.
	int FillPits(WTile& t, int& lowered)
	{
		// the tile with a margin of r points around it, filled from finished neighbours at its level where there are any
		const int          n = t.n, r = std::clamp(int(1.0 / t.step + 0.5), 1, 4), m = n + 2 * r;
		std::vector<float> E(size_t(m) * m, NAN), tmp(size_t(m) * m, NAN), sh(size_t(m) * m, NAN);
		for (int dj = -1; dj <= 1; ++dj)
			for (int di = -1; di <= 1; ++di) {
				const WTile* nb = &t;
				if (di || dj) {
					auto it = g_world.tiles.find(WKey(t.i + di, t.j + dj));
					if (it == g_world.tiles.end() || it->second.state != 2 || it->second.n != n || fabs(it->second.step - t.step) > 1e-4f ||
					    fabs(it->second.lv - t.lv) > g_cfg.worldLevelM)
						continue;
					nb = &it->second;
				}
				for (int b = 0; b < m; ++b)
					for (int a = 0; a < m; ++a) {
						const int ga = a - r - di * (n - 1), gb = b - r - dj * (n - 1);  // (a point of that tile)
						if (ga < 0 || gb < 0 || ga >= n || gb >= n) continue;
						if ((di || dj) && std::isfinite(E[b * m + a])) continue;  // (shared edges: the tile's own)
						E[b * m + a] = nb->z[gb * n + ga];
					}
			}
		// the lowest (lo) or highest of the finite values within r of each point
		auto pass = [&](const std::vector<float>& src, std::vector<float>& dst, bool lo) {
			for (int b = 0; b < m; ++b)
				for (int a = 0; a < m; ++a) {
					float v = lo ? 1e30f : -1e30f;
					for (int bb = std::max(0, b - r); bb <= std::min(m - 1, b + r); ++bb)
						for (int aa = std::max(0, a - r); aa <= std::min(m - 1, a + r); ++aa) {
							const float z = src[bb * m + aa];
							if (std::isfinite(z)) v = lo ? std::min(v, z) : std::max(v, z);
						}
					dst[b * m + a] = fabsf(v) < 1e29f ? v : NAN;
				}
		};
		// and only where the ground on BOTH sides of it (along a row, a column or a diagonal, within 1 m) is that much lower
		// (a spike) or higher (a pit): where one side isn't known (the tile's edge, no finished neighbour) a wall or a curb
		// cut by the edge isn't taken for one
		std::vector<float> z0;  // (the values before each pass)
		// v2.56: nanLow: a tall spike (a bridge's truss member over its deck, your 2.55 bridge) counts no ground on the tile itself
		// (water beside the deck) as lower ground; the unknown margin (no finished neighbour) still doesn't count
		auto both = [&](int a, int b, float d, bool nanLow) {  // d > 0: ground on both sides at least d higher; d < 0: at least -d lower
			static const int dirs[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
			const float      zk = z0[b * m + a];
			for (const auto& dv : dirs) {
				int sides = 0;
				for (int sg = -1; sg <= 1; sg += 2)
					for (int q = 1; q <= r; ++q) {
						const int aa = a + sg * dv[0] * q, bb = b + sg * dv[1] * q;
						if (aa < 0 || bb < 0 || aa >= m || bb >= m) break;
						const float z = z0[bb * m + aa];
						if (nanLow && !std::isfinite(z) && aa >= r && bb >= r && aa < r + n && bb < r + n) {
							++sides;
							break;
						}
						if (std::isfinite(z) && (d > 0 ? z >= zk + d : z <= zk + d)) {
							++sides;
							break;
						}
					}
				if (sides == 2) return true;
			}
			return false;
		};
		auto apply = [&](bool spikes) {
			int cnt = 0;
			z0 = E;
			pass(E, tmp, spikes);   // opening (spikes): lowest around, then the highest of those; closing (pits): the reverse
			pass(tmp, sh, !spikes);
			for (int b = 0; b < n; ++b)
				for (int a = 0; a < n; ++a) {
					const int k = b * n + a, e = (b + r) * m + a + r;
					if (!std::isfinite(t.z[k]) || t.src[k] == 1 || !std::isfinite(sh[e])) continue;
					if (spikes ? (t.z[k] - sh[e] > 1.0f && both(a + r, b + r, -1.0f, t.z[k] - sh[e] > 2.5f)) : (sh[e] - t.z[k] > 0.15f && both(a + r, b + r, 0.15f, false))) {
						t.z[k] = sh[e];
						E[e] = sh[e];
						++cnt;
					}
				}
			return cnt;
		};
		lowered += apply(true);
		return apply(false);
	}

	// v2.62: the tile's level as a gentle plane: the level it is measured for at its middle, its slope no steeper than a
	// steep road (a level carried up a ramp next to a deck may lean 1.2 m per m: through the deck on one side)
	inline double GentleLevel(const WTile& t, double x, double y)
	{
		const double cx = (t.i + 0.5) * WTileM(), cy = (t.j + 0.5) * WTileM();
		const double c = t.hasRef ? t.lvE : t.rz + t.rx * (cx - t.rcx) + t.ry * (cy - t.rcy);
		return c + std::clamp(double(t.rx), -0.3, 0.3) * (x - cx) + std::clamp(double(t.ry), -0.3, 0.3) * (y - cy);
	}

	// v2.62: ground far over the tile's level with open space under it is an overpass's deck (your 2.61 freeways under
	// overpasses: the deck became the road's ground, an 8 to 14 m block across the road in BeamNG, and the falls next to it).
	// One ray down from just under each such point (GTA's map has no back faces: from inside the deck it isn't seen): the
	// ground under it replaces it when a car fits between and it is nearer the level. A hill or an embankment has nothing
	// under its surface: it stays.
	int UnderOverhead(WTile& t)
	{
		const double L = g_cfg.worldLevelM;
		int          cnt = 0, rays = 0;
		for (int k = 0; k < t.n * t.n && rays < 300; ++k) {
			if (t.ps[k] != 2 || !std::isfinite(t.z[k])) continue;
			const double x = t.i * WTileM() + (k % t.n) * t.step, y = t.j * WTileM() + (k / t.n) * t.step;
			const double lv = GentleLevel(t, x, y), z = t.z[k];
			if (z <= lv + L) continue;
			const double A[3] = { x, y, z - 0.3 }, B[3] = { x, y, lv - L };
			double       p[3], nn[3];
			uint32_t     m = 0;
			++rays;
			if (WallRay(A, B, p, nn, &m) == 1 && z - p[2] >= 1.8 && fabs(p[2] - lv) < z - lv) {
				t.z[k] = float(p[2]);
				t.mat[k] = m;
				++cnt;
			}
		}
		return cnt;
	}

	// v2.62: a point with no ground at the level (none found, or the ground far under it) that has the level's ground within
	// 1 m on both sides of it (along a row, a column or a diagonal) is a gap in a deck (an expansion joint the rays slipped
	// through), not its edge: joined across. 2.61 left those out: a 0.5 to 1.5 m hole across the road (your "3 foot holes" on
	// the highways). shortcut: the tile's own points only (a joint right on a tile's edge stays); look into finished
	// neighbours like FillPits does if holes still show there.
	int JoinGaps(WTile& t)
	{
		const int    n = t.n, reach = std::max(1, int(1.0 / t.step + 1e-6));
		const double D = std::max(0.5, double(g_cfg.worldDropM));
		auto lvAt = [&](int a, int b) { return t.rz + t.rx * (t.i * WTileM() + a * t.step - t.rcx) + t.ry * (t.j * WTileM() + b * t.step - t.rcy); };
		auto atLevel = [&](int a, int b) {
			if (a < 0 || b < 0 || a >= n || b >= n) return false;
			const int k = b * n + a;
			return t.ps[k] == 2 && std::isfinite(t.z[k]) && fabs(t.z[k] - lvAt(a, b)) <= D;
		};
		static const int dirs[4][2] = { { 1, 0 }, { 0, 1 }, { 1, 1 }, { 1, -1 } };
		int cnt = 0;
		for (int b = 0; b < n; ++b)
			for (int a = 0; a < n; ++a) {
				const int  k = b * n + a;
				const bool gap = (t.ps[k] == 3 && !std::isfinite(t.z[k])) || (t.ps[k] == 2 && std::isfinite(t.z[k]) && t.z[k] < lvAt(a, b) - D);
				if (!gap) continue;
				for (const auto& dv : dirs) {
					int q1 = 0, q2 = 0;
					for (int q = 1; q <= reach && !q1; ++q)
						if (atLevel(a + dv[0] * q, b + dv[1] * q)) q1 = q;
					for (int q = 1; q <= reach && !q2; ++q)
						if (atLevel(a - dv[0] * q, b - dv[1] * q)) q2 = q;
					if (!q1 || !q2) continue;
					const int k1 = (b + dv[1] * q1) * n + a + dv[0] * q1, k2 = (b - dv[1] * q2) * n + a - dv[0] * q2;
					t.z[k] = float((double(t.z[k1]) * q2 + double(t.z[k2]) * q1) / (q1 + q2));
					t.mat[k] = t.mat[q1 <= q2 ? k1 : k2];
					t.ps[k] = 2;
					t.src[k] = 2;
					++cnt;
					break;
				}
			}
		return cnt;
	}

	// all points of a pass answered: the next finer pass where it's needed, or done (holes filled from neighbours / left
	// out / tried again)
	// v2.71: after a fall, what the tiles around a spot were (for the next fix: no WorldDebug needed)
	void WorldDumpAround(double x, double y, double z, const char* what)
	{
		static int dumps = 0;
		if (++dumps > 8) return;
		WorldState& w = g_world;
		const int   ci = int(floor(x / WTileM())), cj = int(floor(y / WTileM()));
		Log("ground dump (%s, GTA %.1f %.1f %.1f): the tiles around it", what, x, y, z);
		for (int dj = -1; dj <= 1; ++dj)
			for (int di = -1; di <= 1; ++di) {
				const int64_t k = WKey(ci + di, cj + dj);
				auto          it = w.tiles.find(k);
				if (it == w.tiles.end()) {
					Log("  tile %d,%d: not live (%s)", ci + di, cj + dj, w.cache.count(k) ? "in the cache" : "never measured here");
					continue;
				}
				const WTile& t = it->second;
				int          miss = 0, voids = 0, deep = 0;
				for (int q = 0; q < t.n * t.n; ++q) {
					miss += !std::isfinite(t.z[q]) && t.ps[q] != 5;
					voids += t.ps[q] == 5;
					deep += q < int(t.deep.size()) && t.deep[q] > 0;
				}
				double zc = NAN;
				GridZ(t.z, t.n, t.step, t.i, t.j, x, y, zc);
				Log("  tile %d,%d: state %d%s%s%s, level %.1f (measured for %.1f%s%s), %d points empty, %d left out (another level), %d looked deeper, "
				    "partial %d, gaps %d, measured again %d, ground at the spot %.1f, cost %.0f m",
					t.i, t.j, t.state, t.sent ? " sent" : "", t.draft ? " draft" : "", t.healOld ? " (old version in BeamNG)" : "", t.lv, t.lvE,
					t.refCar ? ", from the car" : t.hasFrom ? ", from a neighbour" : "", t.noGround ? ", no ground at all" : "", miss, voids, deep, t.partial, t.gaps,
					t.heal, zc, ChainCost(k));
			}
	}

	// v2.73: your v2.72 screenshots: whole tiles of a hill road empty in BeamNG (GTA's rays found no ground, or only ground away
	// from the level they were made for: "left empty"). A point with no ground and known ground both ways along x or along y
	// (in the tiles next to it too, up to 16 m) is filled in between them, as GTA's Tarmac (BeamNG's ASPHALT). Not where GTA has
	// water; not steeper than 60%; not another level's ground left out beside a deck (a drop, a deck's edge: left as it is)
	int FillHoles(WTile& t)
	{
		WorldState& w = g_world;
		const int   n = t.n;
		const int   sd[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
		for (const auto& d : sd) {
			auto nb = w.tiles.find(WKey(t.i + d[0], t.j + d[1]));
			if (nb == w.tiles.end() || nb->second.state != 2 || nb->second.n != n) continue;
			const WTile& o = nb->second;
			for (int q = 0; q < n; ++q) {
				const int a = d[0] < 0 ? 0 : d[0] > 0 ? n - 1 : q, b = d[1] < 0 ? 0 : d[1] > 0 ? n - 1 : q;
				const int oa = d[0] < 0 ? n - 1 : d[0] > 0 ? 0 : q, ob = d[1] < 0 ? n - 1 : d[1] > 0 ? 0 : q;
				const int k = b * n + a, ok = ob * n + oa;
				if (std::isfinite(t.z[k]) || t.ps[k] == 5 || !std::isfinite(o.z[ok])) continue;
				t.z[k] = o.z[ok];
				t.mat[k] = o.mat[ok];
				t.ps[k] = 2;
				t.src[k] = 1;
			}
		}
		// along x and y, up to 16 m: the nearest known ground each way, in this tile or the tiles next to it (a hole can cross a
		// tile's edge; your v2.72 hill gaps were whole tiles)
		const int S = n - 1;
		auto      zAt = [&](int ga, int gb) -> float {
            const int ti = int(floor(double(ga) / S)), tj = int(floor(double(gb) / S));
            const WTile* o = &t;
            if (ti != t.i || tj != t.j) {
                auto it = w.tiles.find(WKey(ti, tj));
                if (it == w.tiles.end() || it->second.n != n || it->second.z.size() != size_t(n * n)) return NAN;
                o = &it->second;
            }
            return o->z[(gb - tj * S) * n + (ga - ti * S)];
		};
		std::vector<float> nz(t.z);
		int                got = 0;
		for (int b = 0; b < n; ++b)
			for (int a = 0; a < n; ++a) {
				const int k = b * n + a;
				if (std::isfinite(t.z[k]) || t.ps[k] == 5) continue;  // (5: another level's ground left out on purpose: beside a deck)
				const int ga = t.i * S + a, gb = t.j * S + b;
				double    sum = 0;
				int       c = 0;
				for (int ax = 0; ax < 2; ++ax) {
					int   s0 = 0, s1 = 0;
					float z0 = NAN, z1 = NAN;
					for (int s = 1; s <= 32 && !s0; ++s) {
						const float z = ax ? zAt(ga, gb - s) : zAt(ga - s, gb);
						if (std::isfinite(z)) s0 = s, z0 = z;
					}
					for (int s = 1; s <= 32 && !s1; ++s) {
						const float z = ax ? zAt(ga, gb + s) : zAt(ga + s, gb);
						if (std::isfinite(z)) s1 = s, z1 = z;
					}
					if (!s0 || !s1 || fabs(z1 - z0) > 0.6 * (s0 + s1) * t.step) continue;
					sum += z0 + (z1 - z0) * s0 / double(s0 + s1);
					++c;
				}
				if (!c) {
					++w.fbOpen;
					continue;
				}
				const float z = float(sum / c);
				float       wh = 0;
				if (Call<int>(N::GET_WATER_HEIGHT, float(t.i * WTileM() + a * t.step), float(t.j * WTileM() + b * t.step), z + 3.0f, &wh) && wh > z - 1.0f) {
					++w.fbWater;
					continue;
				}
				nz[k] = z;
				t.mat[k] = 0x10DD5498u;  // Tarmac
				t.ps[k] = 2;
				t.src[k] = 2;
				++got;
			}
		t.z.swap(nz);
		t.fb += got;
		return got;
	}

	void WorldFinish(int64_t key)
	{
		WorldState& w = g_world;
		auto        it = w.tiles.find(key);
		if (it == w.tiles.end()) return;
		WTile& t = it->second;
		for (int guard = 0; guard < 12; ++guard) {
			std::vector<std::array<int16_t, 3>> next;
			int                                 need = 0;
			for (const auto& c : t.test) {
				const int why = CellCheck(t, c[0], c[1], c[2], true);
				++w.cellsTested;
				if (!why) {
					SetLeaf(t, c[0], c[1], c[2], c[2]);
					continue;
				}
				++w.why[why];
				if (c[2] < 2) continue;
				need += MarkSub(t, c[0], c[1], c[2], next);
			}
			t.test.swap(next);
			++t.pass;
			if (need > 0) {
				t.left = need;
				t.next = 0;
				return;  // more rays
			}
			if (!t.test.empty()) continue;  // finer cells whose points are all known already
			// the flat cells: do they still fit the points measured next to them later (shared edges)? Else finer.
			for (int b = 0; b < t.n - 1; ++b)
				for (int a = 0; a < t.n - 1; ++a) {
					const int s = t.leaf[b * (t.n - 1) + a];
					if (s < 2 || a % s || b % s) continue;
					if (CellCheck(t, a, b, s, false)) {
						SetLeaf(t, a, b, s, 1);
						t.test.push_back({ int16_t(a), int16_t(b), int16_t(s) });
					}
				}
			if (t.test.empty()) break;
		}
		const int np = t.n * t.n;
		// the points nobody measured: on BeamNG's triangles of the flat cell they are in
		for (int b = 0; b < t.n; ++b)
			for (int a = 0; a < t.n; ++a) {
				const int k = b * t.n + a;
				if (t.ps[k] != 0) continue;
				const int ca = std::min(a, t.n - 2), cb = std::min(b, t.n - 2);
				const int s = std::max<int>(1, t.leaf[cb * (t.n - 1) + ca]);
				const int a0 = std::min(ca / s * s, t.n - 1 - s), b0 = std::min(cb / s * s, t.n - 1 - s);
				const int A = b0 * t.n + a0, B = A + s, C = A + s * t.n + s, D = A + s * t.n;
				if (!std::isfinite(t.z[A]) || !std::isfinite(t.z[B]) || !std::isfinite(t.z[C]) || !std::isfinite(t.z[D])) continue;
				const double u = double(a - a0) / s, v = double(b - b0) / s;
				t.z[k] = float(TriZ(t.z[A], t.z[B], t.z[C], t.z[D], u, v));
				t.mat[k] = t.mat[(v < 0.5 ? b0 : b0 + s) * t.n + (u < 0.5 ? a0 : a0 + s)];
				t.ps[k] = 2;
				t.src[k] = 2;
			}
		// v2.62: an overpass's deck over this level: the ground under it (before the level checks: a tile all under a deck
		// was left empty as "nothing at this level"), then gaps in a deck joined (before the edges are left out)
		w.underPts += uint32_t(UnderOverhead(t));
		w.jointPts += uint32_t(JoinGaps(t));
		// v2.43: another level's ground next to an edge left out (a bridge deck's edge: no wall down to the road below)
		if (VoidBelow(t)) w.voidPts += uint32_t(t.voids);
		// v2.5: nothing at this level here: every point it measured is another level's ground (beside a deck: the road far
		// below; your v2.43 log: such tiles became this level's ground, then were swapped and measured again and again).
		// Left empty, unless that ground meets the tile the level came from: then it is the same road and its level was
		// carried a little wrong: it is moved onto its own ground. (A jump: the car's own level was a guess: kept as it is.)
		const double lvMeasured = t.lvE;
		if (t.hasRef && !(t.refCar && t.carAir)) {
			int    meas = 0, at = 0;
			double nearest = 1e9;
			for (int b = 0; b < t.n; ++b)
				for (int a = 0; a < t.n; ++a) {
					const int k = b * t.n + a;
					if (t.ps[k] != 2 || t.src[k] != 0 || !std::isfinite(t.z[k])) continue;
					const double x = t.i * WTileM() + a * t.step, y = t.j * WTileM() + b * t.step;
					const double d = fabs(t.z[k] - (t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy)));
					++meas;
					at += d <= g_cfg.worldLevelM;
					nearest = std::min(nearest, d);
				}
			if (meas >= 4 && at < 3) {
				if (MeetsFrom(t)) {
					const bool hr = t.hasRef;
					t.hasRef = false;
					double pz = 0, px = 0, py = 0;
					const bool ok = FitPlaneCalc(t, pz, px, py);
					t.hasRef = hr;
					if (ok) {
						t.rcx = (t.i + 0.5) * WTileM();
						t.rcy = (t.j + 0.5) * WTileM();
						t.rz = pz;
						t.rx = px;
						t.ry = py;
						t.lvE = pz;
						++w.levelMoved;
						if (g_cfg.worldDebug) Log("ground dbg: %d,%d level moved onto its own ground %.2f (made for %.2f: it meets the tile before it)", t.i, t.j, pz, t.lv);
					}
				} else {
					int cnt = 0;
					for (int k = 0; k < np; ++k)
						if (t.ps[k] != 5) {
							t.z[k] = NAN;
							t.ps[k] = 5;
							++cnt;
						}
					t.voids = cnt;
					t.noLevel = true;
					++w.noLevelTiles;
					if (g_cfg.worldDebug)
						Log("ground dbg: %d,%d nothing at level %.2f here (%d points measured, the nearest %.1f m off): left empty", t.i, t.j, t.lv, meas, nearest);
				}
			}
		}
		if (t.lvE != lvMeasured) w.underPts += uint32_t(UnderOverhead(t));  // v2.62: again on the level it was moved to
		w.pitPts += FillPits(t, w.spikePts);  // v2.55
		// v2.73: holes filled when the car is close (further out GTA may not have loaded its collision yet: measured again first)
		if (g_cfg.worldFill && !t.pre && ChainCost(key) <= 2.0 * w.urgentM && NowSeconds() - w.placedT > 3.0) {
			const int f = FillHoles(t);
			if (f) {
				w.fbPts += uint32_t(f);
				++w.fbTiles;
			}
		}
		for (int b = 0; b < t.n - 1; ++b)
			for (int a = 0; a < t.n - 1; ++a) {
				const int s = t.leaf[b * (t.n - 1) + a];
				if (a % s || b % s) continue;
				++w.leafN[s >= 4 ? 0 : s >= 2 ? 1 : 2];
			}
		int miss = 0;
		for (int k = 0; k < np; ++k) miss += !std::isfinite(t.z[k]) && t.ps[k] != 5;
		if (miss && miss * 100 <= np * 15) {
			for (int pass = 0; pass < 4; ++pass) {
				for (int b = 0; b < t.n; ++b)
					for (int a = 0; a < t.n; ++a) {
						const int k = b * t.n + a;
						if (std::isfinite(t.z[k]) || t.ps[k] == 5) continue;
						double    sum = 0;
						int       c = 0;
						uint32_t  mt = 0;
						const int nb[4][2] = { { a - 1, b }, { a + 1, b }, { a, b - 1 }, { a, b + 1 } };
						for (auto& q : nb) {
							if (q[0] < 0 || q[1] < 0 || q[0] >= t.n || q[1] >= t.n) continue;
							const float zz = t.z[q[1] * t.n + q[0]];
							if (!std::isfinite(zz)) continue;
							sum += zz;
							++c;
							mt = t.mat[q[1] * t.n + q[0]];
						}
						if (c) {
							t.z[k] = float(sum / c);
							t.mat[k] = mt;
							++t.filled;
						}
					}
			}
			int still = 0;
			for (int k = 0; k < np; ++k) still += !std::isfinite(t.z[k]) && t.ps[k] != 5;
			if (!still) {
				w.filledPts += miss;
				miss = 0;
			}
		}
		// v2.63: a tile the car reaches soon (within 2 x the "urgent" distance) goes at once with its empty cells left out (your 2.62 test: on a raised road over
		// water at an angle to the tiles, the tiles along its edges, half road half water, waited 2 tries (2 s+) for ground that
		// isn't there: holes at the "corners" when changing lanes). GTA has its collision loaded that close to the car.
		// (not in the first 3 s after the car was put somewhere new: GTA may still be loading its collision there)
		const bool soonPartial = miss && !t.pre && ChainCost(key) <= 2.0 * w.urgentM && NowSeconds() - w.placedT > 3.0 && AnyCell(t);
		if (soonPartial && t.gaps < 2) ++w.soonPartial;
		if (miss && (t.gaps >= 2 || soonPartial)) {
			// the same points stayed empty after two more tries (water, the edge of a bridge or of the map): the tile goes
			// with only those cells left out. v2.41: unless no cell has ground on all 4 corners (BeamNG can't build that:
			// your car would wait for it): then it stays a gap for good
			if (!AnyCell(t)) {
				t.state = 3;
				t.tGap = NowSeconds();
				t.gaps = 99;  // not tried again (WorldTick: gaps < 5); v2.7: near the car, measured again (noGround)
				t.noGround = true;
				WorldDropDraft(t);
				++w.gapTiles;
				Log("ground: tile %d,%d: no ground in any of its cells (water or a hole): left empty", t.i, t.j);
				return;
			}
			t.partial = miss;
			t.tGap = NowSeconds();  // v2.7: its empty cells are measured again a few seconds later if the car is near
			if (!w.gapLogged.count(key ^ 0x5A5A5A5A) && w.gapLogged.size() < 400) {
				w.gapLogged.insert(key ^ 0x5A5A5A5A);
				Log("ground: tile %d,%d: %d of %d points still have no ground %s: sent without those cells", t.i, t.j, miss, np,
					t.gaps >= 2 ? "after 3 tries" : "(the car is close: at once)");
			}
			miss = 0;
			++w.partialTiles;
		}
		if (miss) {
			if (t.pre) {
				// the ring around the car: no retries (GTA may not have loaded its ground that far out)
				w.preFail[key] = NowSeconds() + 60.0;
				++w.preFailed;
				w.tiles.erase(it);
				return;
			}
			++w.gapTiles;
			t.state = 3;
			t.tGap = NowSeconds();
			++t.gaps;
			if (!w.gapLogged.count(key) && w.gapLogged.size() < 200) {
				w.gapLogged.insert(key);
				Log("ground: tile %d,%d (around %.0f, %.0f): %d of %d points found no ground (water, a tunnel or bridge in the way, outside the map, or GTA hadn't "
				    "loaded its collision there): not sent; tried again in 1 s",
					t.i, t.j, (t.i + 0.5) * WTileM(), (t.j + 0.5) * WTileM(), miss, np);
			}
			return;
		}
		if (t.voids && !AnyCell(t)) {
			// v2.43: all of it is another level's ground (beside a bridge deck, the road far below): nothing at this level
			++w.voidTiles;
			if (t.pre) {
				w.preFail[key] = NowSeconds() + 600.0;
				w.tiles.erase(it);
				return;
			}
			t.state = 3;
			t.tGap = NowSeconds();
			t.gaps = 99;
			WorldDropDraft(t);
			return;
		}
		// v2.7: a tile measured with the tall window is kept when its ground meets a finished neighbour at their shared edge (within
		// 1 m on average: a hillside, a deck), or when none of it is more than WorldLevelM above the level it was measured for (the
		// ground below a ridge or a bridge: a car going over the edge lands on it). Ground high above it that meets nothing (a roof,
		// an overpass, a terrace behind a retaining wall: no car gets there) is left empty
		if (t.wide) {
			int con = 0;
			for (const auto& dd : { std::array<int, 2>{ 1, 0 }, std::array<int, 2>{ -1, 0 }, std::array<int, 2>{ 0, 1 }, std::array<int, 2>{ 0, -1 } }) {
				auto nb = w.tiles.find(WKey(t.i + dd[0], t.j + dd[1]));
				if (nb == w.tiles.end() || nb->second.state != 2) continue;
				int          cnt = 0;
				const double dz = EdgeDiff(t, nb->second, cnt);
				con += cnt >= 3 && dz < 1.0;
			}
			double up = -1e9;
			for (int k = 0; k < t.n * t.n; ++k) {
				if (!std::isfinite(t.z[k])) continue;
				const double x = t.i * WTileM() + (k % t.n) * t.step, y = t.j * WTileM() + (k / t.n) * t.step;
				up = std::max(up, t.z[k] - (t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy)));
			}
			t.wide = false;
			if (!con && up > g_cfg.worldLevelM) {
				for (size_t q = 0; q < t.z.size(); ++q) t.z[q] = NAN;
				t.state = 3;
				t.tGap = NowSeconds();
				t.gaps = 99;
				t.noGround = true;
				if (w.healLogged.size() < 300 && w.healLogged.insert(key * 4 + 3 + 0x7000000).second)
					Log("ground: tile %d,%d: the tall window found ground %.1f m over the level and meeting none of its neighbours (a roof, an overpass, "
					    "a terrace): left empty", t.i, t.j, up);
				return;
			}
			if (w.healLogged.size() < 300 && w.healLogged.insert(key * 4 + 3 + 0x6000000).second)
				Log("ground: tile %d,%d: found with the tall window (%s; up to %.1f m over the level): built", t.i, t.j, con ? "meets a neighbour" : "below the level", up);
		}
		FitPlane(t);
		t.lv = t.planeOk ? t.pz : t.lvE;
		t.state = 2;
		if (g_cfg.worldDebug) Log("ground dbg: %d,%d finished: level %.2f (made for %.2f), %d points left out%s", t.i, t.j, t.lv, t.lvE, t.voids, t.pre ? " (ring)" : "");
		++w.tilesDone;
		w.raysDone += t.rays;
		// v2.5: saved once its walls are known (WorldWallsTick); without the wall scan, now
		if (t.partial == 0 && t.fb == 0 && !g_cfg.worldWalls) MapPut(t);  // (v2.73: a filled hole isn't saved: measured again next drive)
		t.wallsDone = !g_cfg.worldWalls;
		if (t.pre) ++w.preDone;
		// WorldTick sends it (or keeps it for later: the ring, a car standing on its draft)
	}

	// one ray's answer (asynchronous next tick, or synchronous right away)
	void WorldRayResult(const WRay& r, int st, bool hit, float endZ, uint32_t mat, bool timedOut)
	{
		WorldState& w = g_world;
		auto        it = w.tiles.find(r.key);
		if (it == w.tiles.end() || it->second.gen != r.gen || it->second.state != 1) return;  // tile gone or measured again
		WTile& t = it->second;
		if (r.pt < 0 || r.pt >= int(t.ps.size()) || t.ps[r.pt] != 1) return;
		if (st == 2 && hit) {
			t.z[r.pt] = endZ;
			t.mat[r.pt] = mat;
			t.ps[r.pt] = 2;
			// v2.72: ground found above the window is kept as it is (v2.71 dropped what didn't meet measured ground: on Vinewood's hills that
			// was half of it, your log: 1453 dropped of 2000, the holes and the F10 fall loop). A roof becomes a block where the building is
			t.src[r.pt] = 0;
			if (r.pt < int(t.deep.size()) && t.deep[r.pt] == 2) ++w.aboveKept;
			if (r.pt < int(t.deep.size()) && t.deep[r.pt]) ++w.deepHits, ++t.deepFound;
			--t.left;
			++t.rays;
			++w.hits;
			++w.matCount[mat];
			w.matDirty = true;
		} else {
			if (st == 2) ++w.misses;
			else if (timedOut) ++w.timeouts;
			else ++w.failed;
			if (st != 2) {
				// GTA lost or dropped the ray (a full pool?): not "no ground". Fewer rays at once, this point again (up to 4 times)
				w.lastRefuseTick = w.tick;
				w.cap = std::max(16, int(w.rays.size()) * 3 / 4);
				if (++t.fl[r.pt] < 4) {
					t.ps[r.pt] = 4;
					t.next = std::min(t.next, r.pt);
					return;
				}
			}
			// v2.71: no ground in the normal window: below it, then above it (near the car's way; WorldDeep=0: off). A tile where
			// 6 points found nothing either way and none found ground (water, a void): the rest not (rays for the tiles around)
			if (st == 2 && r.pt < int(t.deep.size()) && t.deep[r.pt] == 2) ++t.deepMiss;
			if (st == 2 && g_cfg.worldDeep && !t.wide && !t.pre && r.pt < int(t.deep.size()) && t.deep[r.pt] < 2 && ChainCost(r.key) <= 3.0 * w.urgentM &&
			    (t.deepFound > 0 || t.deepMiss < 6)) {
				++t.deep[r.pt];
				++w.deepRays;
				t.ps[r.pt] = 4;
				t.next = std::min(t.next, r.pt);
				return;
			}
			// no ground in the window: none at this level here (v2.5: no second, wider try: it found other levels' ground)
			++t.tries[r.pt];
			++t.rays;
			t.ps[r.pt] = 3;
			--t.left;
		}
		if (t.left <= 0) WorldFinish(r.key);
	}

	void WorldPoll()
	{
		WorldState& w = g_world;
		for (size_t r = 0; r < w.rays.size();) {
			WRay& ray = w.rays[r];
			if (ray.tick == w.tick) {
				++r;
				continue;
			}
			int       hit = 0, entity = 0;
			NVec3     end{}, normal{};
			uint64_t  mat64 = 0;
			const int st = Call<int>(N::GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL, ray.handle, &hit, &end, &normal, &mat64, &entity);
			const bool timedOut = st == 1 && w.tick - ray.tick > 90;
			if (st == 1 && !timedOut) {
				++r;
				continue;
			}
			const WRay done = ray;
			w.rays[r] = w.rays.back();
			w.rays.pop_back();
			w.waitSum += w.tick - done.tick;
			++w.waitN;
			WorldRayResult(done, st, hit != 0, end.z, uint32_t(mat64 & 0xFFFFFFFFu), timedOut);
		}
	}

	// the next point of tile t that needs a ray (-1: none)
	int NextNeeded(WTile& t)
	{
		const int np = t.n * t.n;
		while (t.next < np && t.ps[t.next] != 4) ++t.next;
		return t.next < np ? t.next : -1;
	}

	// v2.7: how often this spot was measured again lately (forgotten after 2 minutes)
	int HealCount(int64_t key, double now)
	{
		auto it = g_world.healN.find(key);
		return it == g_world.healN.end() || now - it->second.second > 120.0 ? 0 : it->second.first;
	}

	void RayWindow(const WTile& t, int k, float& x, float& y, float& z0, float& z1)
	{
		const int    a = k % t.n, b = k / t.n;
		x = float(t.i * WTileM() + a * t.step);
		y = float(t.j * WTileM() + b * t.step);
		const double ref = RefZ(t, k);
		// v2.5: never higher than WorldUpM over the level (v2.43 looked 20 m up on a second try: an overpass, a lamp post or a
		// sign over the road became ground: the big walls in your screenshots), down WorldDownM (a ramp's slope, a deck's edge)
		z0 = float(ref + g_cfg.worldUpM);
		z1 = float(ref - g_cfg.worldDownM);
		// v2.7: a tile that found no ground at all near the car, measured again: the level it was made for was wrong (your 2.63
		// screenshots 5 and 6: hills beside the road, a highway deck after a jump). Only tiles with nothing in the normal window.
		if (t.wide) {
			z0 = float(ref + 25.0);
			z1 = float(ref - 30.0);
		}
		// v2.71: a point the normal window found nothing for: below it (the foot of a drop off the road: a car going over
		// the edge lands there), then above it (a hillside rising more than WorldUpM over the road: rays starting inside the
		// hill see nothing). What is found above is kept only where it meets the ground found next to it (WorldFinish: not
		// a roof, not an overpass)
		const int dp = k < int(t.deep.size()) ? t.deep[k] : 0;
		if (dp == 1) {
			z0 = float(ref - g_cfg.worldDownM + 0.2);
			z1 = float(ref - g_cfg.worldDownM - 28.0);
		} else if (dp == 2) {
			z0 = float(ref + g_cfg.worldUpM + 22.0);
			z1 = float(ref + g_cfg.worldUpM - 0.2);
		}
	}

	// rays for the tiles being measured, the soonest needed first. v2.42: GTA's immediate rays first (your v2.41 log: they
	// were all refused because the asynchronous ones had filled GTA's ray pool), within WorldSyncMs a frame; then the
	// asynchronous ones (about 10 a frame on your PC). v2.43: immediate rays for every tile the car needs (your v2.42 log:
	// 10 us each, only 0.7% of the time was used), and for the ring when standing still
	void WorldIssue(const std::vector<std::pair<double, int64_t>>& order)
	{
		WorldState& w = g_world;
		if (g_cfg.worldSyncMs > 0) {
			const double t0 = NowSeconds(), limit = g_cfg.worldSyncMs / 1000.0;
			bool         stop = false;
			for (const auto& o : order) {
				if (stop || (o.first >= 1e5 && w.spd > 2.0)) break;  // the ring (cost 1e5+) only when standing
				auto it = w.tiles.find(o.second);
				if (it == w.tiles.end() || it->second.state != 1) continue;
				for (int k = NextNeeded(it->second); k >= 0;) {
					if (NowSeconds() - t0 > limit) {
						stop = true;
						break;
					}
					WTile& t = it->second;
					float  x, y, z0, z1;
					RayWindow(t, k, x, y, z0, z1);
					const double s0 = NowSeconds();
					const int    h = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, x, y, z0, x, y, z1, 1, g_drv.veh, 7);
					if (!h) {
						++w.syncRefused;
						stop = true;
						break;
					}
					int       hit = 0, entity = 0;
					NVec3     end{}, normal{};
					uint64_t  mat64 = 0;
					const int st = Call<int>(N::GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL, h, &hit, &end, &normal, &mat64, &entity);
					const double us = (NowSeconds() - s0) * 1e6;
					w.syncUs += us;
					w.syncUsMax = std::max(w.syncUsMax, us);
					++w.syncRays;
					if (st != 2) {
						++w.syncRefused;  // not answered at once: this point goes to the asynchronous rays
						stop = true;
						break;
					}
					t.ps[k] = 1;
					const uint32_t gen = t.gen;
					WorldRayResult(WRay{ 0, o.second, k, gen, w.tick }, st, hit != 0, end.z, uint32_t(mat64 & 0xFFFFFFFFu), false);
					auto again = w.tiles.find(o.second);
					if (again == w.tiles.end() || again->second.state != 1 || again->second.gen != gen) break;  // done or gone
					it = again;
					k = NextNeeded(again->second);
				}
			}
		}
		int  budget = g_cfg.worldRays;
		bool poolFull = false;
		for (const auto& o : order) {
			if (poolFull || budget <= 0 || int(w.rays.size()) >= w.cap) break;
			auto it = w.tiles.find(o.second);
			if (it == w.tiles.end() || it->second.state != 1) continue;
			WTile& t = it->second;
			for (int k = NextNeeded(t); k >= 0; k = NextNeeded(t)) {
				if (budget <= 0 || int(w.rays.size()) >= w.cap) break;
				float x, y, z0, z1;
				RayWindow(t, k, x, y, z0, z1);
				const int h = Call<int>(N::START_SHAPE_TEST_LOS_PROBE, x, y, z0, x, y, z1, 1, g_drv.veh, 7);
				if (!h) {
					++w.refused;
					w.lastRefuseTick = w.tick;
					w.cap = std::max(16, int(w.rays.size()) * 3 / 4);
					poolFull = true;
					break;
				}
				t.ps[k] = 1;
				w.rays.push_back(WRay{ h, o.second, k, t.gen, w.tick });
				++w.started;
				--budget;
			}
		}
	}

	void AddDisc(std::set<int64_t>& out, double cx, double cy, double rad)
	{
		const double T = WTileM();
		const int    rt = int(rad / T) + 1;
		const int    ci = int(floor(cx / T)), cj = int(floor(cy / T));
		for (int dj = -rt; dj <= rt; ++dj)
			for (int di = -rt; di <= rt; ++di) {
				const int    ti = ci + di, tj = cj + dj;
				const double nx = std::clamp(cx, ti * T, (ti + 1) * T), ny = std::clamp(cy, tj * T, (tj + 1) * T);
				if ((nx - cx) * (nx - cx) + (ny - cy) * (ny - cy) <= rad * rad) out.insert(WKey(ti, tj));
			}
	}

	// where the car will be over the next WorldAheadS seconds (its speed and how fast it is turning), and from that the
	// tiles wanted (around the car + along that path) and how soon each one is reached
	void WorldPlan()
	{
		WorldState&  w = g_world;
		const double spd = sqrt(w.vx * w.vx + w.vy * w.vy);
		w.spd = spd;
		w.path.clear();
		if (w.air && w.landOk) {
			// v2.51: in the air: the ground where it lands and on from there (the way it is flying), and a little around the car
			const double hx = spd > 1.0 ? w.vx / spd : cos(w.hd), hy = spd > 1.0 ? w.vy / spd : sin(w.hd);
			for (int s = 0; s <= 8; ++s) w.path.push_back({ w.landX + hx * spd * 0.2 * s, w.landY + hy * spd * 0.2 * s, w.landT + 0.2 * s });
			// v2.63: and the ground under the whole flight (your tumbling crash: it came down 65 m short of the first guess,
			// where nothing was built, and fell through)
			const double fdx = w.landX - w.cx, fdy = w.landY - w.cy, fl = hypot(fdx, fdy);
			auto planAir = [&](std::set<int64_t>& out, double extra) {
				out.clear();
				AddDisc(out, w.landX, w.landY, LandR() + extra);
				for (const auto& p : w.path) AddDisc(out, p[0], p[1], g_cfg.worldWidth * 0.5 + extra);
				AddDisc(out, w.cx, w.cy, std::max(8.0, 0.3 * spd) + extra);
				for (double s = 4.0; s < fl; s += 4.0) AddDisc(out, w.cx + fdx * s / fl, w.cy + fdy * s / fl, 6.0 + extra);
			};
			planAir(w.wanted, 0.0);
			planAir(w.keep, WTileM());
			w.cost.clear();
			for (int64_t k : w.wanted) {
				int i, j;
				WIJ(k, i, j);
				const double tx = (i + 0.5) * WTileM(), ty = (j + 0.5) * WTileM();
				double       best = 500.0 + hypot(tx - w.cx, ty - w.cy);  // (around the car: after the landing)
				for (const auto& p : w.path) best = std::min(best, (p[2] - w.landT) * std::max(spd, 1.0) + hypot(tx - p[0], ty - p[1]));
				if (fl > 1.0) {  // v2.63: under the flight: nearest the car first
					const double s = std::clamp(((tx - w.cx) * fdx + (ty - w.cy) * fdy) / (fl * fl), 0.0, 1.0);
					best = std::min(best, 10.0 + fl * s + 2.0 * hypot(tx - (w.cx + fdx * s), ty - (w.cy + fdy * s)));
				}
				w.cost[k] = best;
			}
			w.urgentM = std::max(24.0, g_cfg.worldUrgentS * spd + 12.0);
			w.draftM = 0.0;  // (the landing is measured finely at once: no 2 m drafts)
			return;
		}
		const int    N = 24;
		// v2.56: above 40 m/s (144 km/h) the road ahead is asked for earlier, growing with the speed (your 200 mph: the car reached
		// road not built yet, "worst 0.00 s")
		const double fast = std::max(1.0, spd / 40.0);
		const double T = g_cfg.worldAheadS * fast, dt = T / N;
		double       x = w.cx, y = w.cy, h = w.haveHd ? w.hd : 0.0;
		w.path.push_back({ x, y, 0.0 });
		if (spd > 1.0) {
			const double yr = std::clamp(w.yawRate, -1.2, 1.2);
			for (int s = 1; s <= N; ++s) {
				h += yr * dt;
				x += cos(h) * spd * dt;
				y += sin(h) * spd * dt;
				w.path.push_back({ x, y, s * dt });
			}
		}
		auto plan = [&](std::set<int64_t>& out, double extra) {
			out.clear();
			const double R = g_cfg.worldRadius;
			// v2.63: rolling or upside down (a crash tumbling on): the full radius all around it, whatever its speed
			const double nearR = (spd < 15 || w.uz < 0.6) ? R : std::max(12.0, R - (spd - 15) * 0.25);
			AddDisc(out, w.cx, w.cy, nearR + extra);
			const double len = std::max(R, spd * T);
			double       done = 0;
			for (size_t s = 1; s < w.path.size() && done <= len + extra * 2; ++s) {
				AddDisc(out, w.path[s][0], w.path[s][1], g_cfg.worldWidth * 0.5 + extra);
				done = w.path[s][2] * spd;
			}
		};
		plan(w.wanted, 0.0);
		plan(w.keep, WTileM());
		// how soon: metres along the path to the nearest path point + 2 x the distance off it
		w.cost.clear();
		for (int64_t k : w.wanted) {
			int i, j;
			WIJ(k, i, j);
			const double tx = (i + 0.5) * WTileM(), ty = (j + 0.5) * WTileM();
			double       best = 1e30;
			for (const auto& p : w.path) {
				const double c = p[2] * std::max(spd, 1.0) + 2.0 * hypot(tx - p[0], ty - p[1]);
				best = std::min(best, c);
			}
			w.cost[k] = best;
		}
		w.urgentM = std::max(24.0, g_cfg.worldUrgentS * spd * fast + 12.0);
		w.draftM = spd > 6.0 ? g_cfg.worldDraftS * spd * fast + 12.0 : 0.0;  // v2.42: drafts only at speed
	}

	std::string TileIdStr(int64_t k)
	{
		int i, j;
		WIJ(k, i, j);
		char b[48];
		snprintf(b, sizeof(b), "g_%d_%d", i, j);
		return b;
	}

	void WorldWriteMaterials()
	{
		WorldState& w = g_world;
		if (!w.matDirty) return;
		w.matDirty = false;
		char path[MAX_PATH + 16];
		PathNextToModule("_materials.txt", path, sizeof(path));
		FILE* f = fopen(path, "wb");
		if (!f) return;
		std::vector<std::pair<uint64_t, uint32_t>> v;
		for (auto& kv : w.matCount) v.push_back({ kv.second, kv.first });
		std::sort(v.rbegin(), v.rend());
		fprintf(f, "GTA ground materials your drives touched (GtaBeamD1 %s), rays that hit each, and the BeamNG ground model it becomes\r\n", D1_VERSION);
		fprintf(f, "(GtaBeamD1_surfaces.json maps them; unknown ones use \"%s\")\r\n\r\n", g_surf.names[g_surf.def].c_str());
		for (auto& e : v) {
			auto nm = g_surf.gtaName.find(e.second);
			fprintf(f, "0x%08X  %-24s -> %-28s %llu\r\n", e.second, nm != g_surf.gtaName.end() ? nm->second.c_str() : "(not in the table)",
				g_surf.names[SurfOf(e.second)].c_str(), (unsigned long long)e.first);
		}
		fclose(f);
	}

	// a finished tile goes to the cache (GTA's coordinates: still right after a new origin); the oldest go when it's full.
	// v2.43: up to 3 levels of the same spot (a bridge deck and the road under it): one of the same level is replaced
	void CacheTile(int64_t k, WTile&& t)
	{
		WorldState& w = g_world;
		if (t.state != 2) return;
		t.acked = t.sent = t.draft = false;
		t.pre = false;
		t.tris = 0;
		auto& vars = w.cache[k];
		bool  put = false;
		for (WTile& v : vars)
			if (fabs(v.lv - t.lv) < g_cfg.worldLevelM) {
				v = std::move(t);
				put = true;
				break;
			}
		if (!put) {
			if (vars.size() >= 3) vars.erase(vars.begin());
			vars.push_back(std::move(t));
		}
		if (int(w.cache.size()) > g_cfg.worldCacheTiles + 64) {
			std::vector<std::pair<uint32_t, int64_t>> age;
			for (auto& kv : w.cache) {
				uint32_t last = 0;
				for (const WTile& v : kv.second) last = std::max(last, v.lastUse);
				age.push_back({ last, kv.first });
			}
			std::sort(age.begin(), age.end());
			for (size_t q = 0; q + g_cfg.worldCacheTiles < age.size(); ++q) w.cache.erase(age[q].second);
		}
	}

	// ---------------------------------------------------------------------------------------
	// v2.42: the saved map (your idea). Every finished tile is written to GtaBeamD1_map\ next to this plugin, in regions of
	// 16 x 16 tiles (128 m), and read back ahead of the car on the next drive: a road driven once is never measured again.
	// Reading and writing happen on a thread of their own (no stutter in GTA).
	// ---------------------------------------------------------------------------------------
	// v2.43: each record has its level (a bridge deck and the road under it are two records of the same spot)
	struct MapRec
	{
		float       lv = 0;
		std::string data;
	};
	struct MapRegion
	{
		std::map<int64_t, std::vector<MapRec>> recs;  // tile -> its records (one per level)
		bool      loaded = false, loading = false, dirty = false;
		ULONGLONG used = 0;
	};
	struct MapJob
	{
		int                                       kind = 0;  // 1 read, 2 write
		int64_t                                   rk = 0;
		std::string                               path;
		std::vector<std::pair<int64_t, MapRec>> recs;
	};
	struct MapStore
	{
		bool                         init = false, ok = false;
		std::string                  dir;
		std::map<int64_t, MapRegion> regions;
		SRWLOCK                      lock = SRWLOCK_INIT;
		CONDITION_VARIABLE           cv = CONDITION_VARIABLE_INIT;
		std::deque<MapJob>           jobs;
		std::vector<MapJob>          done;  // read jobs come back with their records; write jobs with an error in path ("!...")
		HANDLE                       thread = nullptr;
		ULONGLONG                    saveMs = 0, needMs = 0;
		uint32_t                     readTiles = 0, wroteTiles = 0, regionsRead = 0, errors = 0;
		bool                         oldLogged = false;
		int                          pending = 0;
	} g_map;

	constexpr uint32_t kMapMagic = 0x314D4247;  // "GBM1"
	constexpr uint32_t kMapVersion = 5;         // v2.62: no overpass decks as the road's ground, deck joints closed (v2.52: 4; older files are measured again)
	inline int64_t MapRegionOf(int i, int j) { return WKey(int(floor(i / 16.0)), int(floor(j / 16.0))); }

	std::string MapPath(int64_t rk)
	{
		int ri, rj;
		WIJ(rk, ri, rj);
		char b[64];
		snprintf(b, sizeof(b), "\\r_%d_%d.gbm", ri, rj);
		return g_map.dir + b;
	}

	DWORD WINAPI MapThread(LPVOID)
	{
		for (;;) {
			MapJob job;
			AcquireSRWLockExclusive(&g_map.lock);
			while (g_map.jobs.empty()) SleepConditionVariableSRW(&g_map.cv, &g_map.lock, INFINITE, 0);
			job = std::move(g_map.jobs.front());
			g_map.jobs.pop_front();
			ReleaseSRWLockExclusive(&g_map.lock);
			if (job.kind == 1) {
				job.recs.clear();
				if (FILE* f = fopen(job.path.c_str(), "rb")) {
					uint32_t hdr[6] = {};
					if (fread(hdr, sizeof(uint32_t), 6, f) == 6 && hdr[0] == kMapMagic && hdr[1] != kMapVersion) {
						job.path = "!old";  // v2.42's (no levels: a bridge deck and the road under it were one): measured again
					} else if (hdr[0] == kMapMagic) {
						float tm, st;
						memcpy(&tm, &hdr[2], 4);
						memcpy(&st, &hdr[3], 4);
						if (fabs(tm - g_cfg.worldTileM) < 1e-3 && fabs(st - g_cfg.worldStepNear) < 1e-4) {
							for (uint32_t r = 0; r < hdr[5] && r < 3 * 4096; ++r) {
								uint32_t len = 0;
								int64_t  key = 0;
								MapRec   rec;
								if (fread(&key, 8, 1, f) != 1 || fread(&rec.lv, 4, 1, f) != 1 || fread(&len, 4, 1, f) != 1 || len > 65536) break;
								rec.data.assign(len, '\0');
								if (fread(&rec.data[0], 1, len, f) != len) break;
								job.recs.push_back({ key, std::move(rec) });
							}
						} else {
							job.path = "!layout";  // measured with other tiles (ini): not used
						}
					}
					fclose(f);
				}
			} else if (job.kind == 2) {
				const std::string tmp = job.path + ".tmp";
				bool              ok = false;
				if (FILE* f = fopen(tmp.c_str(), "wb")) {
					float          tm = g_cfg.worldTileM, st = g_cfg.worldStepNear;
					uint32_t       hdr[6] = { kMapMagic, kMapVersion, 0, 0, 0, uint32_t(job.recs.size()) };
					memcpy(&hdr[2], &tm, 4);
					memcpy(&hdr[3], &st, 4);
					ok = fwrite(hdr, sizeof(uint32_t), 6, f) == 6;
					for (auto& r : job.recs) {
						const uint32_t len = uint32_t(r.second.data.size());
						ok = ok && fwrite(&r.first, 8, 1, f) == 1 && fwrite(&r.second.lv, 4, 1, f) == 1 && fwrite(&len, 4, 1, f) == 1 &&
						     fwrite(r.second.data.data(), 1, len, f) == len;
					}
					ok = (fclose(f) == 0) && ok;
				}
				ok = ok && MoveFileExA(tmp.c_str(), job.path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
				job.recs.clear();
				if (!ok) job.path = "!" + job.path;
			}
			AcquireSRWLockExclusive(&g_map.lock);
			g_map.done.push_back(std::move(job));
			ReleaseSRWLockExclusive(&g_map.lock);
		}
		return 0;
	}

	bool MapReady()
	{
		MapStore& m = g_map;
		if (!g_cfg.worldMap) return false;
		if (m.init) return m.ok;
		m.init = true;
		char path[MAX_PATH + 16];
		PathNextToModule("_map", path, sizeof(path));
		m.dir = path;
		CreateDirectoryA(path, nullptr);
		const DWORD at = GetFileAttributesA(path);
		m.ok = at != INVALID_FILE_ATTRIBUTES && (at & FILE_ATTRIBUTE_DIRECTORY);
		if (m.ok) m.thread = CreateThread(nullptr, 0, MapThread, nullptr, 0, nullptr);
		m.ok = m.ok && m.thread;
		int files = 0;
		WIN32_FIND_DATAA fd;
		HANDLE           h = FindFirstFileA((m.dir + "\\r_*.gbm").c_str(), &fd);
		if (h != INVALID_HANDLE_VALUE) {
			do ++files;
			while (FindNextFileA(h, &fd));
			FindClose(h);
		}
		Log("ground: the saved map is in %s (%s; %d regions of 128 m saved so far)", path, m.ok ? "ready" : "NOT usable: tiles are measured every time", files);
		return m.ok;
	}

	void MapQueue(MapJob&& j)
	{
		AcquireSRWLockExclusive(&g_map.lock);
		g_map.jobs.push_back(std::move(j));
		++g_map.pending;
		ReleaseSRWLockExclusive(&g_map.lock);
		WakeConditionVariable(&g_map.cv);
	}

	// a region is wanted: read it from disk (once)
	void MapNeed(int64_t rk)
	{
		if (!MapReady()) return;
		MapRegion& r = g_map.regions[rk];
		r.used = GetTickCount64();
		if (r.loaded || r.loading) return;
		r.loading = true;
		MapJob j;
		j.kind = 1;
		j.rk = rk;
		j.path = MapPath(rk);
		MapQueue(std::move(j));
	}

	std::string MapEncode(const WTile& t)
	{
		const int n = t.n, np = n * n;
		float     zmin = 1e30f;
		for (float z : t.z)
			if (std::isfinite(z)) zmin = std::min(zmin, z);
		std::vector<uint32_t> pal;
		std::string           s;
		s.reserve(16 + size_t(np) * 4 + size_t(n - 1) * (n - 1) + 64);
		auto put = [&](const void* p, size_t len) { s.append(reinterpret_cast<const char*>(p), len); };
		const int32_t  i = t.i, j = t.j;
		const uint8_t  nn = uint8_t(n), K = uint8_t(t.K);
		put(&i, 4);
		put(&j, 4);
		put(&zmin, 4);
		put(&nn, 1);
		put(&K, 1);
		std::vector<uint8_t> pidx(np, 0), kind(np, 0);
		std::vector<int32_t> zc(np, INT32_MIN);
		for (int k = 0; k < np; ++k) {
			if (!std::isfinite(t.z[k])) {
				kind[k] = 2;
				continue;
			}
			zc[k] = int32_t(lround((t.z[k] - zmin) * 1000.0));  // v2.52: mm (2.51: cm)
			kind[k] = t.src[k] == 2 ? 0 : 1;
			size_t q = 0;
			while (q < pal.size() && pal[q] != t.mat[k]) ++q;
			if (q == pal.size()) {
				if (pal.size() < 255) pal.push_back(t.mat[k]);
				else q = 0;
			}
			pidx[k] = uint8_t(q);
		}
		const uint8_t pn = uint8_t(pal.size());
		put(&pn, 1);
		put(pal.data(), pal.size() * 4);
		put(zc.data(), size_t(np) * 4);
		put(pidx.data(), np);
		put(kind.data(), np);
		put(t.leaf.data(), t.leaf.size());
		// v2.43: the level it was measured for (its reference plane: height at its centre, slope)
		const double  cx = (t.i + 0.5) * WTileM(), cy = (t.j + 0.5) * WTileM();
		const float   rf[3] = { float(t.rz + t.rx * (cx - t.rcx) + t.ry * (cy - t.rcy)), float(t.rx), float(t.ry) };
		const uint8_t hr = t.hasRef ? 1 : 0;
		put(rf, sizeof(rf));
		put(&hr, 1);
		// v2.5: its walls (scanned): count, then 9 floats each
		const uint16_t nw = uint16_t(std::min<size_t>(t.walls.size(), 4000));
		const uint8_t  wd = t.wallsDone ? 1 : 0;
		put(&wd, 1);
		put(&nw, 2);
		for (uint16_t q = 0; q < nw; ++q) {
			const WTile::Wall& wl = t.walls[q];
			const float        f[9] = { wl.cx, wl.cy, wl.z0, wl.z1, wl.dx, wl.dy, wl.len, wl.thick, wl.gs };
			put(f, sizeof(f));
		}
		return s;
	}

	bool MapDecode(const std::string& s, WTile& t)
	{
		size_t      o = 0;
		auto        get = [&](void* p, size_t len) {
            if (o + len > s.size()) return false;
            memcpy(p, s.data() + o, len);
            o += len;
            return true;
		};
		int32_t i, j;
		float   zmin;
		uint8_t nn, K, pn;
		if (!get(&i, 4) || !get(&j, 4) || !get(&zmin, 4) || !get(&nn, 1) || !get(&K, 1) || !get(&pn, 1)) return false;
		const int n = nn, np = n * n;
		if (n != int(WTileM() / g_cfg.worldStepNear + 0.5) + 1) return false;
		std::vector<uint32_t> pal(pn);
		std::vector<int32_t>  zc(np);
		std::vector<uint8_t>  pidx(np), kind(np);
		t = WTile{};
		t.leaf.assign(size_t(n - 1) * (n - 1), 1);
		if (!get(pal.data(), size_t(pn) * 4) || !get(zc.data(), size_t(np) * 4) || !get(pidx.data(), np) || !get(kind.data(), np) || !get(t.leaf.data(), t.leaf.size()))
			return false;
		t.i = i;
		t.j = j;
		t.n = n;
		t.K = std::max<int>(1, K);
		t.step = g_cfg.worldStepNear;
		t.z.assign(np, NAN);
		t.mat.assign(np, 0);
		t.ps.assign(np, 3);
		t.src.assign(np, 0);
		t.tries.assign(np, 0);
		t.fl.assign(np, 0);
		t.deep.assign(np, 0);
		for (int k = 0; k < np; ++k) {
			if (kind[k] == 2) continue;
			t.z[k] = zmin + float(zc[k] / 1000.0);
			t.mat[k] = pidx[k] < pal.size() ? pal[pidx[k]] : 0;
			t.ps[k] = 2;
			t.src[k] = kind[k] == 1 ? 0 : 2;
		}
		for (auto& l : t.leaf) l = std::max<uint8_t>(1, l);
		t.state = 2;
		t.disk = true;
		float   rf[3] = {};
		uint8_t hr = 0;
		if (get(rf, sizeof(rf)) && get(&hr, 1) && hr == 1) {
			t.hasRef = true;
			t.rcx = (i + 0.5) * WTileM();
			t.rcy = (j + 0.5) * WTileM();
			t.rz = rf[0];
			t.rx = rf[1];
			t.ry = rf[2];
		}
		// v2.5: walls
		uint8_t  wd = 0;
		uint16_t nw = 0;
		if (get(&wd, 1) && get(&nw, 2)) {
			for (uint16_t q = 0; q < nw; ++q) {
				float f[9];
				if (!get(f, sizeof(f))) break;
				WTile::Wall wl;
				wl.cx = f[0];
				wl.cy = f[1];
				wl.z0 = f[2];
				wl.z1 = f[3];
				wl.dx = f[4];
				wl.dy = f[5];
				wl.len = f[6];
				wl.thick = f[7];
				wl.gs = f[8];
				t.walls.push_back(wl);
			}
			t.wallsDone = wd == 1 || !g_cfg.worldWalls;
		} else {
			t.wallsDone = !g_cfg.worldWalls;
		}
		FitPlane(t);
		t.lv = t.planeOk ? t.pz : double(zmin);
		t.lvE = t.hasRef ? t.rz : t.lv;
		return true;
	}

	// a record into a spot's list: the one of the same level is replaced (at most 3 levels)
	bool MapRecPut(std::vector<MapRec>& v, MapRec&& rec, bool replace)
	{
		for (MapRec& o : v)
			if (fabs(o.lv - rec.lv) < g_cfg.worldLevelM) {
				if (replace) o = std::move(rec);
				return replace;
			}
		if (v.size() >= 3) v.erase(v.begin());
		v.push_back(std::move(rec));
		return true;
	}

	void MapPut(const WTile& t)
	{
		if (!MapReady()) return;
		const int64_t rk = MapRegionOf(t.i, t.j);
		MapRegion&    r = g_map.regions[rk];
		MapRec        rec;
		rec.lv = float(t.lv);
		rec.data = MapEncode(t);
		MapRecPut(r.recs[WKey(t.i, t.j)], std::move(rec), true);
		r.dirty = true;
		r.used = GetTickCount64();
		if (!r.loaded && !r.loading) MapNeed(rk);
	}

	// the record of spot key at level E (nullptr: none within WorldLevelM)
	const MapRec* MapRecAt(const MapRegion& r, int64_t key, double E)
	{
		auto it = r.recs.find(key);
		if (it == r.recs.end()) return nullptr;
		const MapRec* best = nullptr;
		double        bestD = g_cfg.worldLevelM;
		for (const MapRec& rec : it->second)
			if (fabs(rec.lv - E) < bestD) {
				bestD = fabs(rec.lv - E);
				best = &rec;
			}
		return best;
	}

	// the records of spot key within WorldLevelM of E, the nearest first, decoded until one meets its neighbour "from"
	bool MapPick(const MapRegion& r, int64_t key, double E, const WPlane* p, const WTile* from, WTile& out)
	{
		auto it = r.recs.find(key);
		if (it == r.recs.end()) return false;
		std::vector<std::pair<double, const MapRec*>> c;
		for (const MapRec& rec : it->second)
			if (fabs(rec.lv - E) < g_cfg.worldLevelM) c.push_back({ fabs(rec.lv - E), &rec });
		std::sort(c.begin(), c.end());
		for (auto& q : c)
			if (MapDecode(q.second->data, out) && (!p || LevelFits(out, *p, from))) return true;
		return false;
	}

	// 1 found (decoded into out), 0 not saved (at this level), -1 its region isn't read yet
	int MapGet(int64_t key, double E, WTile& out, const WPlane* p = nullptr, const WTile* from = nullptr)
	{
		if (!MapReady()) return 0;
		int i, j;
		WIJ(key, i, j);
		auto r = g_map.regions.find(MapRegionOf(i, j));
		if (r == g_map.regions.end() || !r->second.loaded) {
			MapNeed(MapRegionOf(i, j));
			auto again = g_map.regions.find(MapRegionOf(i, j));
			if (again == g_map.regions.end() || !again->second.loaded) {
				// a tile measured this session is there even before the region is read
				if (again != g_map.regions.end() && MapPick(again->second, key, E, p, from, out)) return 1;
				return -1;
			}
			r = again;
		}
		r->second.used = GetTickCount64();
		return MapPick(r->second, key, E, p, from, out) ? 1 : 0;
	}

	// saved at level E (any level when E is NaN)
	bool MapKnows(int64_t key, double E = NAN)
	{
		int i, j;
		WIJ(key, i, j);
		auto r = g_map.regions.find(MapRegionOf(i, j));
		if (r == g_map.regions.end()) return false;
		if (!std::isfinite(E)) {
			auto it = r->second.recs.find(key);
			return it != r->second.recs.end() && !it->second.empty();
		}
		return MapRecAt(r->second, key, E) != nullptr;
	}

	void MapSave(bool all)
	{
		if (!g_map.ok) return;
		for (auto& kv : g_map.regions) {
			MapRegion& r = kv.second;
			if (!r.dirty || !r.loaded) continue;  // (a region is written only after it was read: nothing on disk is lost)
			MapJob j;
			j.kind = 2;
			j.rk = kv.first;
			j.path = MapPath(kv.first);
			for (auto& rec : r.recs)
				for (const MapRec& one : rec.second) j.recs.push_back({ rec.first, one });
			g_map.wroteTiles += uint32_t(j.recs.size());
			r.dirty = false;
			MapQueue(std::move(j));
		}
		(void)all;
	}

	// every tick: regions read back, regions ahead asked for, dirty ones written every 20 s, far ones forgotten
	void MapTick()
	{
		if (!g_cfg.worldMap || !MapReady()) return;
		MapStore&           m = g_map;
		std::vector<MapJob> done;
		AcquireSRWLockExclusive(&m.lock);
		done.swap(m.done);
		m.pending -= int(done.size());
		ReleaseSRWLockExclusive(&m.lock);
		for (auto& j : done) {
			if (j.kind == 1) {
				MapRegion& r = m.regions[j.rk];
				r.loading = false;
				r.loaded = true;
				if (j.path == "!layout") {
					++m.errors;
					Log("ground: saved map region %s was measured with another tile layout (WorldTileM / WorldStepM): not used, measured again", MapPath(j.rk).c_str());
				} else if (j.path == "!old") {
					if (!m.oldLogged) {
						m.oldLogged = true;
						Log("ground: the saved map from v2.61 or older isn't used (v2.62: roads under overpasses had the deck as their ground): those roads are measured again once");
					}
				}
				for (auto& rec : j.recs) MapRecPut(r.recs[rec.first], std::move(rec.second), false);  // what was measured meanwhile wins
				m.readTiles += uint32_t(j.recs.size());
				if (!j.recs.empty()) ++m.regionsRead;
			} else if (!j.path.empty() && j.path[0] == '!') {
				++m.errors;
				if (m.errors < 5) Log("ground: writing the saved map failed: %s", j.path.c_str() + 1);
			}
		}
		const ULONGLONG now = GetTickCount64();
		WorldState&     w = g_world;
		if (w.carOk && now - m.needMs > 500) {
			m.needMs = now;
			// the regions within WorldMapReadM of the car, and of the road ahead
			const double R = g_cfg.worldMapReadM, T = WTileM() * 16;
			auto         around = [&](double x, double y) {
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        const double px = x + dx * R * 0.7, py = y + dy * R * 0.7;
                        MapNeed(WKey(int(floor(px / T)), int(floor(py / T))));
                    }
			};
			around(w.cx, w.cy);
			if (!w.path.empty()) around(w.path.back()[0], w.path.back()[1]);
			// far regions (not dirty) forgotten
			for (auto it = m.regions.begin(); it != m.regions.end();) {
				int ri, rj;
				WIJ(it->first, ri, rj);
				const double d = hypot((ri + 0.5) * T - w.cx, (rj + 0.5) * T - w.cy);
				if (d > 1500.0 && !it->second.dirty && !it->second.loading && now - it->second.used > 60000) it = m.regions.erase(it);
				else ++it;
			}
		}
		if (now - m.saveMs > 20000) {
			m.saveMs = now;
			MapSave(false);
		}
	}

	// BeamNG's answers: tiles built, the map reply, ground stats
	void WorldReplies()
	{
		WorldState&              w = g_world;
		std::vector<std::string> acks;
		std::string              map, ws;
		uint32_t                 mc, wc;
		AcquireSRWLockExclusive(&g_dnet.lock);
		acks.swap(g_dnet.acks);
		mc = g_dnet.mapCount;
		if (mc != w.mapSeen) map = g_dnet.map;
		wc = g_dnet.wsCount;
		if (wc != w.wsSeen) ws = g_dnet.ws;
		ReleaseSRWLockExclusive(&g_dnet.lock);
		for (const std::string& a : acks) {
			const bool bad = !a.empty() && a[0] == '!';
			int        i, j;
			if (sscanf(a.c_str() + (bad ? 1 : 0), "g_%d_%d", &i, &j) != 2) continue;
			auto it = w.tiles.find(WKey(i, j));
			if (bad) {
				++w.errs;
				if (it != w.tiles.end() && it->second.state == 1) {
					// a draft BeamNG couldn't build: no more drafts for it, it is finished as usual
					it->second.draft = false;
					it->second.noDraft = true;
					continue;
				}
				Log("ground: BeamNG couldn't build tile %d,%d (see its log)", i, j);
				if (it != w.tiles.end()) w.tiles.erase(it);  // measured again
				w.cache.erase(WKey(i, j));
			} else if (it != w.tiles.end()) {
				it->second.acked = true;
			}
		}
		if (mc != w.mapSeen) {
			w.mapSeen = mc;
			double n = -1, epoch = 0;
			JsonNumC(map.c_str(), "n", n);
			JsonNumC(map.c_str(), "epoch", epoch);
			std::string mode, check, v;
			JsonStr(map, "mode", mode);
			JsonStr(map, "check", check);
			JsonStr(map, "v", v);
			size_t mine = 0;
			for (auto& kv : w.tiles) mine += kv.second.sent || kv.second.draft;
			Log("ground: BeamNG mod %s took the mapping (tile set %.0f, it has %.0f tiles; tile mode %s: %s)", v.c_str(), epoch, n, mode.c_str(), check.c_str());
			if (v.empty() || v < std::string("2.53")) {
				Log("ground: WARNING: the BeamNG mod is %s, not 2.53: copy the new gtaBridge.lua (the dents need it; 2.52: the vehicles folder too)", v.empty() ? "old" : v.c_str());
				Notify("BeamNG mod %s is old: copy the 2.53 gtaBridge.lua and restart BeamNG", v.empty() ? "?" : v.c_str());
			}
			if (n == 0 && mine > 0) {
				Log("ground: BeamNG has none of the %zu tiles sent before (restarted or the map loaded again?): sending them again from the cache", mine);
				for (auto it = w.tiles.begin(); it != w.tiles.end();) {
					if (it->second.state == 2) CacheTile(it->first, std::move(it->second));
					it = w.tiles.erase(it);
				}
			}
		}
		if (wc != w.wsSeen) {
			w.wsSeen = wc;
			w.ws = ws;
		}
	}

	// how many metres of built road (finished or draft) lie ahead along the path (for the log)
	void WorldAhead()
	{
		WorldState&  w = g_world;
		const double spd = sqrt(w.vx * w.vx + w.vy * w.vy);
		if (spd < 14 || w.path.size() < 2) return;
		double m = 0;
		for (size_t s = 1; s < w.path.size(); ++s) {
			const double ax = w.path[s - 1][0], ay = w.path[s - 1][1], bx = w.path[s][0], by = w.path[s][1];
			const double seg = hypot(bx - ax, by - ay);
			bool         ok = true;
			for (double d = 0; d < seg; d += 1.0) {
				const double x = ax + (bx - ax) * d / seg, y = ay + (by - ay) * d / seg;
				auto         it = w.tiles.find(WKey(int(floor(x / WTileM())), int(floor(y / WTileM()))));
				if (it == w.tiles.end() || !(it->second.sent || it->second.draft) || !it->second.acked) {
					ok = false;
					break;
				}
				m += 1.0;
			}
			if (!ok) break;
		}
		const double s = m / spd;
		w.aheadMin = std::min(w.aheadMin, s);
		w.aheadSum += s;
		++w.aheadN;
	}

	void WorldLog()
	{
		WorldState&     w = g_world;
		const ULONGLONG now = GetTickCount64();
		if (!w.logMs) w.logMs = now;
		if (now - w.matMs >= 30000) {
			w.matMs = now;
			WorldWriteMaterials();
		}
		if (now - w.logMs < 10000) return;
		const double secs = (now - w.logMs) / 1000.0;
		w.logMs = now;
		if (!w.active && !w.started && w.rays.empty()) return;
		size_t live = 0, built = 0, measuring = 0, gaps = 0, drafting = 0, pre = 0;
		for (auto& kv : w.tiles) {
			const WTile& t = kv.second;
			live += t.sent || t.draft;
			built += (t.sent || t.draft) && t.acked;
			measuring += t.state == 1 && !t.pre;
			pre += t.pre;
			gaps += t.state == 3;
			drafting += t.draft && !t.sent;
		}
		std::vector<std::pair<uint64_t, uint32_t>> top;
		for (auto& kv : w.matCount) top.push_back({ kv.second, kv.first });
		std::sort(top.rbegin(), top.rend());
		std::string ms;
		for (size_t q = 0; q < top.size() && q < 5; ++q) {
			auto nm = g_surf.gtaName.find(top[q].second);
			char b[96];
			snprintf(b, sizeof(b), "%s%s %llu", q ? ", " : "", nm != g_surf.gtaName.end() ? nm->second.c_str() : "?", (unsigned long long)top[q].first);
			ms += b;
		}
		const uint32_t leaves = w.leafN[0] + w.leafN[1] + w.leafN[2];
		Log("ground: %zu tiles in BeamNG (%zu built, %zu still drafts, %zu triangles), %zu being measured, %zu in the ring ahead of need, %zu gaps, %zu in the cache; +%u finished (%.1f a "
		    "second, %.0f rays each, %u edge points from neighbours), +%u drafts sent, %u finished sent (%u waited until the car was off the draft), %.0f triangles a tile, "
		    "+%u from the cache, +%u from the saved map, %u ring tiles done (%u had no ground), %u removed; cells looked at %u: made finer because no ground %u, two "
		    "surfaces %u, not flat %u; flat cells kept 2 m %.0f%% / 1 m %.0f%% / 0.5 m %.0f%%",
			live, built, drafting, w.liveTris, measuring, pre, gaps, CacheCount(), w.tilesDone, w.tilesDone / secs, w.tilesDone ? double(w.raysDone) / w.tilesDone : 0.0,
			w.copiedPts, w.drafts, w.finals, w.deferred, w.sent ? double(w.trisSent) / w.sent : 0.0, w.fromCache, w.fromDisk, w.preDone, w.preFailed, w.removed,
			w.cellsTested, w.why[1], w.why[2], w.why[3], leaves ? 100.0 * w.leafN[0] / leaves : 0.0, leaves ? 100.0 * w.leafN[1] / leaves : 0.0,
			leaves ? 100.0 * w.leafN[2] / leaves : 0.0);
		Log("ground: rays: %u started (%.0f a second), %u refused by GTA (limit now %d waiting), %u immediate (%.0f a second, %.0f us each, max %.0f us, %u refused), "
		    "%u hit, %u no ground, %u failed, %u timed out; GTA answered after %.1f ticks; %u points filled from neighbours, %u gap tiles, %u sent with cells left out; "
		    "road ahead above 50 km/h: worst %.2f s, average %.2f s (%u samples); plugin %.2f ms per tick (max %.2f); saved map: %u tiles read (%u regions), %u written, "
		    "%zu regions in memory, %d jobs waiting, %u errors; materials: %s",
			w.started, w.started / secs, w.refused, w.cap, w.syncRays, w.syncRays / secs, w.syncRays ? w.syncUs / w.syncRays : 0.0, w.syncUsMax, w.syncRefused, w.hits,
			w.misses, w.failed, w.timeouts, w.waitN ? w.waitSum / w.waitN : 0.0, w.filledPts, w.gapTiles, w.partialTiles, w.aheadN ? w.aheadMin : 0.0,
			w.aheadN ? w.aheadSum / w.aheadN : 0.0, w.aheadN, w.usN ? w.usSum / w.usN / 1000.0 : 0.0, w.usMax / 1000.0, g_map.readTiles, g_map.regionsRead,
			g_map.wroteTiles, g_map.regions.size(), g_map.pending, g_map.errors, ms.empty() ? "none yet" : ms.c_str());
		g_map.readTiles = g_map.regionsRead = g_map.wroteTiles = 0;
		if (w.deepRays)
			Log("ground: v2.71: %u rays looked below / above points with no ground in their window, %u found ground (%u of them above it: kept, v2.72)",
				w.deepRays, w.deepHits, w.aboveKept);
		w.deepRays = w.deepHits = w.aboveKept = 0;
		if (w.fbTiles || w.fbOpen)
			Log("ground: v2.73: %u points filled in as holes on %u tiles (Tarmac / ASPHALT); %u points left empty over water, %u with ground on one side only (an edge, not a hole)",
				w.fbPts, w.fbTiles, w.fbWater, w.fbOpen);
		w.fbPts = w.fbTiles = w.fbWater = w.fbOpen = 0;
		Log("ground: levels: %u tiles swapped for the car's level, %u wanted tiles known only at another level (measured), %u points left out under edges (%u tiles "
		    "all another level's); static world: %u far tiles taken out of BeamNG (budget %d triangles), %u sent while standing; v2.5: %u tiles with nothing at "
		    "the car's level (left empty), %u levels moved onto their own ground; v2.54: %u swaps held (that spot was swapped under 5 s ago), %u swapped tiles "
		    "taken out after 2 s without a new one, %zu waiting for their new one; v2.55: %d points lifted out of narrow pits, %d lowered from narrow spikes; "
		    "v2.62: %u points under an overpass (the road under it, not its deck), %u points across gaps in a deck (joints) joined; v2.63: %u tiles near the car "
		    "sent at once with their empty cells left out",
			w.swaps, w.levelMiss, w.voidPts, w.voidTiles, w.keptRm, g_cfg.worldKeepTris, w.idleSent, w.noLevelTiles, w.levelMoved, w.swapHeld, w.staleGone, w.stale.size(), w.pitPts, w.spikePts,
			w.underPts, w.jointPts, w.soonPartial);
		w.swaps = w.levelMiss = w.voidPts = w.voidTiles = w.keptRm = w.idleSent = w.noLevelTiles = w.levelMoved = w.swapHeld = w.staleGone = 0;
		w.pitPts = w.spikePts = 0;
		w.underPts = w.jointPts = w.soonPartial = 0;
		if (w.jumps || w.noFall)
			Log("ground: jumps (v2.51): %u times in the air, %u rays following flights; %u times under the ground it left while still over GTA's (no reset)", w.jumps, w.landRays,
				w.noFall);
		w.jumps = w.landRays = w.noFall = 0;
		if (g_cfg.worldWalls)
			Log("ground: walls (v2.5): %u tiles scanned, %u wall boxes, %u rays (%.0f us a tile, max %.0f), %u ground points under walls flattened, %u tiles "
			    "sent before their scan (the car was close); v2.54: %u faces met too low over their foot and nothing higher up (curbs, joints, a ramp's crest: not walls)",
				w.wallTiles, w.wallBoxes, w.wallRaysN, w.wallTiles ? w.wallUs / w.wallTiles : 0.0, w.wallUsMax, w.wallFlat, w.wallLate, w.wallLow);
		w.wallTiles = w.wallBoxes = w.wallRaysN = w.wallFlat = w.wallLate = w.wallLow = 0;
		w.wallUs = w.wallUsMax = 0;
		if (!w.ws.empty()) {
			double n = 0, b = 0, bms = 0, bmx = 0, rn = 0, rms = 0, rmx = 0, fp95 = 0, fmx = 0, q = 0, tris = 0;
			JsonNumC(w.ws.c_str(), "n", n);
			JsonNumC(w.ws.c_str(), "b", b);
			JsonNumC(w.ws.c_str(), "bms", bms);
			JsonNumC(w.ws.c_str(), "bmx", bmx);
			JsonNumC(w.ws.c_str(), "rn", rn);
			JsonNumC(w.ws.c_str(), "rms", rms);
			JsonNumC(w.ws.c_str(), "rmx", rmx);
			JsonNumC(w.ws.c_str(), "fp95", fp95);
			JsonNumC(w.ws.c_str(), "fmx", fmx);
			JsonNumC(w.ws.c_str(), "q", q);
			JsonNumC(w.ws.c_str(), "tris", tris);
			std::string gm, mode;
			JsonStr(w.ws, "gm", gm);
			JsonStr(w.ws, "mode", mode);
			Log("ground (BeamNG, last second): %.0f tiles, %.0f triangles; %.0f built (%.1f ms average, %.1f max); collision reloads %.0f (%.1f ms average, %.1f max); bridge cost "
			    "per frame p95 %.2f ms, max %.2f; queue %.0f; tile mode %s; triangles per ground model: %s",
				n, tris, b, bms, bmx, rn, rms, rmx, fp95, fmx, q, mode.c_str(), gm.c_str());
		}
		w.started = w.refused = w.hits = w.misses = w.timeouts = w.failed = w.sent = w.removed = w.filledPts = w.gapTiles = w.errs = w.partialTiles = 0;
		w.fromCache = w.tilesDone = w.raysDone = w.copiedPts = w.syncRays = w.syncRefused = 0;
		w.cellsTested = w.why[0] = w.why[1] = w.why[2] = w.why[3] = 0;
		w.leafN[0] = w.leafN[1] = w.leafN[2] = w.leafN[3] = 0;
		w.drafts = w.finals = w.deferred = w.fromDisk = w.preDone = w.preFailed = w.trisSent = 0;
		w.syncUs = w.syncUsMax = 0;
		w.waitSum = 0;
		w.waitN = 0;
		w.usSum = w.usMax = 0;
		w.usN = 0;
		w.aheadMin = 1e9;
		w.aheadSum = 0;
		w.aheadN = 0;
	}

	// is the car on this tile, or about to be (then a finished tile waits: replacing the ground under the wheels at speed
	// would bump the car)
	bool CarOnTile(const WTile& t)
	{
		const WorldState& w = g_world;
		if (w.spd < 5.0) return false;
		const double T = WTileM(), x0 = t.i * T, y0 = t.j * T;
		const double nx = std::clamp(w.cx, x0, x0 + T), ny = std::clamp(w.cy, y0, y0 + T);
		return hypot(nx - w.cx, ny - w.cy) < 4.0 + 0.4 * w.spd;
	}

	// the ring around the car (WorldPrebuildM): with rays to spare, the nearest tile not measured yet (ahead first) is
	// measured and saved, not sent
	void WorldPrebuild()
	{
		WorldState& w = g_world;
		if (g_cfg.worldPrebuildM <= 0 || w.tick % 10) return;
		int pre = 0, measuring = 0;
		for (auto& kv : w.tiles) {
			pre += kv.second.pre && kv.second.state == 1;
			measuring += kv.second.state == 1 && !kv.second.pre;
		}
		if (pre >= 3 || measuring >= g_cfg.worldTilesWork / 2) return;
		const double now = NowSeconds();
		for (auto it = w.preFail.begin(); it != w.preFail.end();) it = it->second < now ? w.preFail.erase(it) : std::next(it);
		const double T = WTileM(), R = g_cfg.worldPrebuildM;
		const int    rt = int(R / T) + 1, ci = int(floor(w.cx / T)), cj = int(floor(w.cy / T));
		const double hx = w.haveHd ? cos(w.hd) : 0.0, hy = w.haveHd ? sin(w.hd) : 0.0;
		double       best = 1e30;
		int64_t      bestK = 0;
		for (int dj = -rt; dj <= rt; ++dj)
			for (int di = -rt; di <= rt; ++di) {
				const double tx = (ci + di + 0.5) * T - w.cx, ty = (cj + dj + 0.5) * T - w.cy;
				const double d = sqrt(tx * tx + ty * ty);
				if (d > R) continue;
				const double score = d - 0.5 * std::max(0.0, tx * hx + ty * hy);
				if (score >= best) continue;
				const int64_t k = WKey(ci + di, cj + dj);
				if (w.tiles.count(k) || w.cache.count(k) || w.preFail.count(k) || MapKnows(k)) continue;
				if (g_cfg.worldMap && g_map.ok) {
					auto r = g_map.regions.find(MapRegionOf(ci + di, cj + dj));
					if (r == g_map.regions.end() || !r->second.loaded) continue;  // its region isn't read yet
				}
				if (!TileStartable(ci + di, cj + dj)) continue;
				best = score;
				bestK = k;
			}
		if (best > 1e29) return;
		int i, j;
		WIJ(bestK, i, j);
		WTile t = MakeTile(i, j);
		t.pre = true;
		w.tiles[bestK] = std::move(t);
		if (w.tiles[bestK].left == 0) WorldFinish(bestK);
	}

	// "rm" for tile k (sent in batches of 60)
	void RmAdd(std::string& rm, int& nrm, int64_t k)
	{
		rm += (nrm ? ",\"" : "\"") + TileIdStr(k) + "\"";
		if (++nrm >= 60) {
			DrvSend(("{\"t\":\"rm\",\"ids\":[" + rm + "]}").c_str());
			rm.clear();
			nrm = 0;
		}
	}

	// v2.43: a static world. Built tiles the car no longer needs stay in BeamNG up to WorldKeepTris triangles (fewer at
	// speed: every new tile makes BeamNG rebuild the collision of all of them) and within WorldKeepM; past that the
	// farthest go first (to the cache: sent again without rays when wanted). Empty tiles (another level's ground) far
	// away are forgotten.
	void WorldKeepBudget(std::string& rm, int& nrm)
	{
		WorldState& w = g_world;
		size_t      tris = 0;
		std::vector<std::pair<double, int64_t>> extra;
		for (auto it = w.tiles.begin(); it != w.tiles.end();) {
			const WTile& t = it->second;
			const double d = hypot((t.i + 0.5) * WTileM() - w.cx, (t.j + 0.5) * WTileM() - w.cy);
			if (t.state == 3 && !w.wanted.count(it->first) && d > g_cfg.worldKeepM) {
				it = w.tiles.erase(it);
				continue;
			}
			if (t.sent || t.draft) {
				tris += size_t(std::max(0, t.tris));
				if (!w.keep.count(it->first)) extra.push_back({ d, it->first });
			}
			++it;
		}
		const double budget = g_cfg.worldKeepTris * std::clamp(1.0 - (w.spd - 15.0) / 40.0, 0.4, 1.0);
		std::sort(extra.rbegin(), extra.rend());  // farthest first
		for (const auto& e : extra) {
			if (double(tris) <= budget && e.first <= g_cfg.worldKeepM) break;
			auto it = w.tiles.find(e.second);
			if (it == w.tiles.end()) continue;
			WTile& t = it->second;
			RmAdd(rm, nrm, it->first);
			tris -= std::min(tris, size_t(std::max(0, t.tris)));
			if (t.state == 2) CacheTile(it->first, std::move(t));
			w.tiles.erase(it);
			++w.keptRm;
			++w.removed;
		}
		w.liveTris = tris;
	}


	// ---------------------------------------------------------------------------------------
	// v2.5: walls and barriers (T3b). GTA's walls, barriers, pillars and building sides are found with flat rays across each
	// finished tile, WorldWallProbeM over its ground (rows and columns 1 m apart, both ways): where a ray meets a steep face
	// (its normal under 60 degrees from flat) something solid starts. The faces are joined into straight pieces and go to
	// BeamNG with the tile as boxes, so BeamNG's car hits them like GTA's car does (v2.43 drew a barrier as humps: your
	// screenshots 1 and 2). Ground points on a thin wall's top are put back on the road (no humps). GTA's map only: parked
	// cars and props are not walls.
	// ---------------------------------------------------------------------------------------
	struct WallHit
	{
		double x, y;    // where a ray met the face
		double nx, ny;  // the face's normal (flat, unit), pointing out of the solid
		double thick;   // how far the solid goes on behind the face, along the normal (0 = not known)
		bool   paired;  // the other side of the same solid was seen too (a thin wall, a barrier)
		double z;       // v2.54: the ray's height there
	};

	// one immediate ray of GTA's map: 1 hit (p, n), 0 nothing, -1 GTA didn't answer at once
	int WallRay(const double a[3], const double b[3], double p[3], double n[3], uint32_t* matOut)
	{
		const int h = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, float(a[0]), float(a[1]), float(a[2]), float(b[0]), float(b[1]), float(b[2]), 1,
			g_drv.veh, 7);
		if (!h) return -1;
		int       hit = 0, ent = 0;
		NVec3     end{}, nrm{};
		uint64_t  mat = 0;
		const int st = Call<int>(N::GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL, h, &hit, &end, &nrm, &mat, &ent);
		if (st != 2) return -1;
		++g_world.wallRaysN;
		if (!hit) return 0;
		if (matOut) *matOut = uint32_t(mat & 0xFFFFFFFFu);
		p[0] = end.x;
		p[1] = end.y;
		p[2] = end.z;
		const double l = sqrt(double(nrm.x) * nrm.x + double(nrm.y) * nrm.y + double(nrm.z) * nrm.z);
		n[0] = l > 1e-6 ? nrm.x / l : 0.0;
		n[1] = l > 1e-6 ? nrm.y / l : 0.0;
		n[2] = l > 1e-6 ? nrm.z / l : 1.0;
		return 1;
	}

	// one line across the tile (a to b): the solid stretches on it from both ways. A face whose normal looks against the
	// direction from a to b opens a stretch, one that looks along it closes one (GTA only reports the side a ray comes
	// from: the far side of a wall is found by the ray coming back). A sloped face is the ground going up: the line ends there.
	void WallLine(const double a[3], const double b[3], const WTile& t, std::vector<WallHit>& out, int& rays)
	{
		const double dx = b[0] - a[0], dy = b[1] - a[1], dz = b[2] - a[2];
		const double L = sqrt(dx * dx + dy * dy);
		if (L < 0.5) return;
		const double ux = dx / L, uy = dy / L;
		struct E { double u, nx, ny; int type; };
		std::vector<E> ev;
		auto march = [&](bool fwd) {
			double u = fwd ? 0.0 : L;  // where the next ray starts (metres from a)
			for (int it = 0; it < 6; ++it) {
				const double s0 = u, s1 = fwd ? L : 0.0;
				if (fwd ? s0 >= L - 0.02 : s0 <= 0.02) return true;
				const double A[3] = { a[0] + ux * s0, a[1] + uy * s0, a[2] + dz * s0 / L }, B[3] = { a[0] + ux * s1, a[1] + uy * s1, a[2] + dz * s1 / L };
				double       p[3], n[3];
				++rays;
				const int r = WallRay(A, B, p, n);
				if (r < 0) return false;
				if (r == 0) return true;
				const double uh = std::clamp((p[0] - a[0]) * ux + (p[1] - a[1]) * uy, 0.0, L);
				if (fabs(n[2]) >= 0.5) return true;  // the ground going up: nothing more on this line that way
				const double fl = sqrt(n[0] * n[0] + n[1] * n[1]);
				const double nx = n[0] / fl, ny = n[1] / fl;
				ev.push_back({ uh, nx, ny, (nx * ux + ny * uy) < 0 ? 1 : -1 });
				u = fwd ? uh + 0.04 : uh - 0.04;
			}
			return true;
		};
		if (!march(true)) return;
		if (ev.empty()) return;  // nothing solid on it
		// the backward march only when the forward one ended inside something (GTA's collision has no back faces)
		int bal = 0;
		for (const E& e : ev) bal += e.type;
		if (bal != 0 || ev.back().type == 1) march(false);
		std::sort(ev.begin(), ev.end(), [](const E& p, const E& q) { return p.u < q.u; });
		std::vector<E> m;
		for (const E& e : ev)
			if (m.empty() || fabs(e.u - m.back().u) > 0.03 || e.type != m.back().type) m.push_back(e);
		// stretches: enter ... leave
		bool         inside = false;
		const E*     open = nullptr;
		auto add = [&](const E* e0, const E* e1, double u0, double u1) {
			if (u1 - u0 < 0.03) u1 = u0 + 0.08;  // a wall drawn as one flat sheet (both its sides at once): 8 cm thick
			const double T = WTileM(), x0 = t.i * T, y0 = t.j * T;
			auto inTile = [&](double x, double y) { return x >= x0 && x < x0 + T && y >= y0 && y < y0 + T; };
			if (e0) {
				const double x = a[0] + ux * u0, y = a[1] + uy * u0;
				const double th = e1 ? (u1 - u0) * fabs(ux * e0->nx + uy * e0->ny) : 0.0;
				if (inTile(x, y)) out.push_back({ x, y, e0->nx, e0->ny, th, e1 != nullptr, a[2] + dz * u0 / L });
			}
			if (e1) {
				const double x = a[0] + ux * u1, y = a[1] + uy * u1;
				const double th = e0 ? (u1 - u0) * fabs(ux * e1->nx + uy * e1->ny) : 0.0;
				if (inTile(x, y)) out.push_back({ x, y, e1->nx, e1->ny, th, e0 != nullptr, a[2] + dz * u1 / L });
			}
		};
		for (const E& e : m) {
			if (e.type == 1) {
				if (!inside) {
					inside = true;
					open = &e;
				}
			} else {
				if (inside) add(open, &e, open->u, e.u);
				else add(nullptr, &e, 0.0, e.u);  // the line began inside it
				inside = false;
				open = nullptr;
			}
		}
		if (inside && open) add(open, nullptr, open->u, L);
	}

	// the tile's ground plane at (x, y) (its own; the level it was measured for when it has none)
	double TileGroundZ(const WTile& t, double x, double y)
	{
		const double cx = (t.i + 0.5) * WTileM(), cy = (t.j + 0.5) * WTileM();
		if (t.planeOk) return t.pz + t.px * (x - cx) + t.py * (y - cy);
		return t.rz + t.rx * (x - t.rcx) + t.ry * (y - t.rcy);
	}

	// faces -> straight pieces -> boxes; a thin wall seen from both sides is one box between its two faces
	void WallBoxes(WTile& t, std::vector<WallHit>& hits)
	{
		struct Seg { double nx, ny, d, t0, t1, thick; bool paired, used; };
		std::vector<Seg> segs;
		std::vector<uint8_t> used(hits.size(), 0);
		const double cosTol = cos(20.0 * kPi / 180.0);
		for (size_t h0 = 0; h0 < hits.size(); ++h0) {
			if (used[h0]) continue;
			std::vector<size_t> c{ h0 };
			used[h0] = 1;
			const WallHit& H = hits[h0];
			for (size_t h = h0 + 1; h < hits.size(); ++h) {
				if (used[h]) continue;
				const WallHit& o = hits[h];
				if (o.nx * H.nx + o.ny * H.ny < cosTol) continue;
				if (fabs((o.x - H.x) * H.nx + (o.y - H.y) * H.ny) > 0.2) continue;
				c.push_back(h);
				used[h] = 1;
			}
			double nx = 0, ny = 0;
			for (size_t q : c) {
				nx += hits[q].nx;
				ny += hits[q].ny;
			}
			const double nl = sqrt(nx * nx + ny * ny);
			if (nl < 1e-6) continue;
			nx /= nl;
			ny /= nl;
			const double tx = -ny, ty = nx;
			std::vector<std::pair<double, size_t>> along;
			double d = 0;
			for (size_t q : c) {
				along.push_back({ hits[q].x * tx + hits[q].y * ty, q });
				d += hits[q].x * nx + hits[q].y * ny;
			}
			d /= double(c.size());
			std::sort(along.begin(), along.end());
			size_t       r0 = 0;
			const double gapM = std::max(1.6, 1.6 * double(g_cfg.worldWallStepM));  // (lines farther apart: hits farther apart)
			for (size_t r = 1; r <= along.size(); ++r) {
				if (r < along.size() && along[r].first - along[r - 1].first <= gapM) continue;
				// one run: along[r0 .. r-1]
				std::vector<double> th;
				int                 pairedN = 0;
				for (size_t q = r0; q < r; ++q) {
					const WallHit& w = hits[along[q].second];
					if (w.thick > 0.02) th.push_back(w.thick);
					pairedN += w.paired;
				}
				double thick = 0.5;
				if (!th.empty()) {
					std::nth_element(th.begin(), th.begin() + th.size() / 2, th.end());
					thick = th[th.size() / 2];
				}
				const bool   paired = pairedN * 2 > int(r - r0);
				// past the last line that met it, up to half the gap to the next line (the next tile's piece starts where this ends)
				const double ext = r - r0 == 1 ? std::clamp(thick, 0.3, 0.8) * 0.5 : 0.5 * std::clamp(double(g_cfg.worldWallStepM), 0.25, 4.0);
				segs.push_back({ nx, ny, d, along[r0].first - ext, along[r - 1].first + ext, std::clamp(thick, 0.1, paired ? 2.5 : 1.5), paired, false });
				r0 = r;
			}
		}
		// boxes: two opposite faces of one thin solid -> one box between them; others -> a box behind the face
		struct Box { double cx, cy, nx, ny, t0, t1, thick; bool thin; };
		std::vector<Box> boxes;
		for (size_t a = 0; a < segs.size(); ++a) {
			Seg& A = segs[a];
			if (A.used) continue;
			int    best = -1;
			double bestOv = 0;
			for (size_t b = 0; b < segs.size(); ++b) {
				if (b == a || segs[b].used) continue;
				const Seg& B = segs[b];
				if (A.nx * B.nx + A.ny * B.ny > -0.9) continue;
				const double th = A.d + B.d;  // A's face at n.p = d, B's (opposite normal) at n.p = -B.d
				if (th < 0.05 || th > 2.5) continue;
				// B's along-axis is reversed (its tangent is -A's)
				const double b0 = -B.t1, b1 = -B.t0;
				const double ov = std::min(A.t1, b1) - std::max(A.t0, b0);
				if (ov > 0.5 * std::min(A.t1 - A.t0, b1 - b0) && ov > bestOv) {
					bestOv = ov;
					best = int(b);
				}
			}
			const double tx = -A.ny, ty = A.nx;
			if (best >= 0) {
				Seg&         B = segs[best];
				const double th = A.d + B.d, mid = A.d - th * 0.5;
				const double t0 = std::min(A.t0, -B.t1), t1 = std::max(A.t1, -B.t0);
				const double tm = (t0 + t1) * 0.5;
				boxes.push_back({ A.nx * mid + tx * tm, A.ny * mid + ty * tm, A.nx, A.ny, t0 - tm, t1 - tm, th, true });
				A.used = B.used = true;
			} else {
				const double mid = A.d - A.thick * 0.5, tm = (A.t0 + A.t1) * 0.5;
				boxes.push_back({ A.nx * mid + tx * tm, A.ny * mid + ty * tm, A.nx, A.ny, A.t0 - tm, A.t1 - tm, A.thick, A.paired });
				A.used = true;
			}
		}
		// heights: the top from the tile's own points on it, else one ray down from WorldUpM; the bottom 0.5 m into the ground.
		// A thin wall's ground points (its top, measured as ground) go back onto the road.
		const int    n = t.n;
		const double T = WTileM(), X0 = t.i * T, Y0 = t.j * T;
		for (const Box& bx : boxes) {
			const double tx = -bx.ny, ty = bx.nx;
			const double g = TileGroundZ(t, bx.cx, bx.cy);
			auto inside = [&](double x, double y, double m) {
				const double ax = (x - bx.cx) * tx + (y - bx.cy) * ty, ac = (x - bx.cx) * bx.nx + (y - bx.cy) * bx.ny;
				return ax >= bx.t0 - m && ax <= bx.t1 + m && fabs(ac) <= bx.thick * 0.5 + m;
			};
			double hTop = -1e9;  // its height over the ground
			for (int b = 0; b < n; ++b)
				for (int a = 0; a < n; ++a) {
					const int k = b * n + a;
					if (!std::isfinite(t.z[k]) || t.src[k] == 2) continue;
					const double x = X0 + a * t.step, y = Y0 + b * t.step;
					if (inside(x, y, 0.05)) hTop = std::max(hTop, double(t.z[k]) - TileGroundZ(t, x, y));
				}
			if (hTop < 0.3) {
				const double A[3] = { bx.cx, bx.cy, g + g_cfg.worldUpM }, B[3] = { bx.cx, bx.cy, g + 0.1 };
				double       p[3], nn[3];
				const int    r = WallRay(A, B, p, nn);
				hTop = (r == 1 && p[2] > g + 0.3) ? p[2] - g : 4.0;
			}
			hTop = std::min(hTop, 8.0);
			WTile::Wall wl;
			wl.cx = float(bx.cx);
			wl.cy = float(bx.cy);
			wl.z0 = float(g - 0.5);
			wl.z1 = float(g + hTop);
			wl.gs = float(t.planeOk ? t.px * tx + t.py * ty : t.rx * tx + t.ry * ty);
			wl.dx = float(tx);
			wl.dy = float(ty);
			wl.len = float(bx.t1 - bx.t0);
			// v2.52: thicker in BeamNG (BeamNG's developers: thin walls grab cars): one seen from one side goes on at least 0.6 m
			// behind its face, a thin one seen from both sides is at least 0.25 m
			double thB = bx.thick, back = 0;
			if (bx.thin) thB = std::max(thB, 0.25);
			else {
				const double t1 = std::max(thB, 0.6);
				back = (t1 - thB) * 0.5;
				thB = t1;
			}
			wl.thick = float(thB);
			// (cx, cy) is the middle of the along-range only when t0 = -t1; put it there
			const double tm = (bx.t0 + bx.t1) * 0.5;
			wl.cx = float(bx.cx + tx * tm - bx.nx * back);
			wl.cy = float(bx.cy + ty * tm - bx.ny * back);
			if (wl.len < 0.1f || wl.thick < 0.05f) continue;
			t.walls.push_back(wl);
			// the ground points measured on it go back onto the road: inside it, and up to 0.3 m past its faces and ends (a
			// wall seen from one side only (cut by the tile's edge, a building): not past its back, where the ground may be high)
			const double mBack = bx.thin ? 0.3 : 0.05;
			// v2.56: down to the road beside it (the nearest point in front of its face, or behind a thin one), not to the tile's
			// plane: on a crowned or banked road the plane is under the road at the barrier: 2.55 left a row of dips along every
			// barrier there (your freeway "teeth")
			auto inBand = [&](double ax, double ac) {
				return ax >= bx.t0 - 0.3 && ax <= bx.t1 + 0.3 && ac <= bx.thick * 0.5 + 0.3 && ac >= -bx.thick * 0.5 - mBack;
			};
			auto roadZ = [&](int a0, int b0, double x, double y) {
				double best = 1e30, z = TileGroundZ(t, x, y);
				const int rr = std::clamp(int(1.5 / t.step + 0.5), 1, 6);
				for (int bb = std::max(0, b0 - rr); bb <= std::min(n - 1, b0 + rr); ++bb)
					for (int aa = std::max(0, a0 - rr); aa <= std::min(n - 1, a0 + rr); ++aa) {
						const int kk = bb * n + aa;
						if (!std::isfinite(t.z[kk]) || t.src[kk] == 3) continue;
						const double qx = X0 + aa * t.step, qy = Y0 + bb * t.step;
						const double qa = (qx - bx.cx) * tx + (qy - bx.cy) * ty, qc = (qx - bx.cx) * bx.nx + (qy - bx.cy) * bx.ny;
						if (inBand(qa, qc) || (qc < 0 && !bx.thin)) continue;  // (not on it; not behind a one-sided one)
						const double d2 = (qx - x) * (qx - x) + (qy - y) * (qy - y);
						if (d2 < best) best = d2, z = t.z[kk];
					}
				return std::min(z, TileGroundZ(t, x, y) + 0.5);
			};
			for (int b = 0; b < n; ++b)
				for (int a = 0; a < n; ++a) {
					const int k = b * n + a;
					if (!std::isfinite(t.z[k])) continue;
					const double x = X0 + a * t.step, y = Y0 + b * t.step;
					const double ax = (x - bx.cx) * tx + (y - bx.cy) * ty, ac = (x - bx.cx) * bx.nx + (y - bx.cy) * bx.ny;
					if (!inBand(ax, ac)) continue;
					const double gz = roadZ(a, b, x, y);
					if (t.z[k] <= gz + 0.2) continue;
					t.z[k] = float(gz);
					t.src[k] = 3;  // flattened (under a wall)
					++t.flat;
				}
		}
	}

	// the wall scan of one finished tile
	void WallScan(WTile& t)
	{
		const double t0 = NowSeconds();
		t.walls.clear();
		t.flat = 0;
		int rays = 0;
		if (AnyCell(t)) {
			std::vector<WallHit> hits;
			const double T = WTileM(), X0 = t.i * T, Y0 = t.j * T, sp = std::clamp(double(g_cfg.worldWallStepM), 0.25, 4.0), h = g_cfg.worldWallProbeM;
			const int    nl = std::max(1, int(T / sp + 0.5));
			for (int k = 0; k < nl; ++k) {
				const double o = (k + 0.5) * T / nl;
				// (from 1.5 m outside the tile: a barrier across its edge is met from outside, so its far face inside the tile is
				// found too; faces outside the tile are the next tile's)
				{
					const double y = Y0 + o, xa = X0 - 1.5, xb = X0 + T + 1.5;
					const double A[3] = { xa, y, TileGroundZ(t, xa, y) + h }, B[3] = { xb, y, TileGroundZ(t, xb, y) + h };
					WallLine(A, B, t, hits, rays);
				}
				{
					const double x = X0 + o, ya = Y0 - 1.5, yb = Y0 + T + 1.5;
					const double A[3] = { x, ya, TileGroundZ(t, x, ya) + h }, B[3] = { x, yb, TileGroundZ(t, x, yb) + h };
					WallLine(A, B, t, hits, rays);
				}
			}
			// v2.54: a face only counts when the ray met it well above the ground at its foot. The rays run along the tile's
			// plane; where the road curves up (a ramp meeting a bridge's deck) the plane runs under the road, so a ray met a
			// curb, a joint or the deck's own front edge from below: a wall across the road (your "invisible wall driving onto
			// a bridge"). Too low: one ray down 10 cm out from the face finds its foot; a second flat ray WorldWallProbeM over
			// that foot says whether a real wall goes on higher up (kept) or not (left out).
			size_t keep = 0;
			for (const WallHit& hh : hits) {
				const double fx = hh.x + hh.nx * 0.1, fy = hh.y + hh.ny * 0.1;
				const double A[3] = { fx, fy, hh.z + 1.0 }, B[3] = { fx, fy, hh.z - 3.0 };
				double       p[3], nn[3];
				++rays;
				if (WallRay(A, B, p, nn) == 1 && hh.z - p[2] < 0.5 * h) {
					const double z2 = p[2] + h;
					const double C[3] = { hh.x + hh.nx * 1.0, hh.y + hh.ny * 1.0, z2 }, D[3] = { hh.x - hh.nx * 1.0, hh.y - hh.ny * 1.0, z2 };
					double       q[3], qn[3];
					++rays;
					if (!(WallRay(C, D, q, qn) == 1 && fabs(qn[2]) < 0.5 && hypot(q[0] - hh.x, q[1] - hh.y) < 0.3)) {
						++g_world.wallLow;
						continue;
					}
				}
				hits[keep++] = hh;
			}
			hits.resize(keep);
			if (!hits.empty()) WallBoxes(t, hits);
		}
		t.wallsDone = true;
		t.wallRays = rays;
		WorldState& w = g_world;
		++w.wallTiles;
		w.wallBoxes += uint32_t(t.walls.size());
		w.wallFlat += uint32_t(t.flat);
		const double us = (NowSeconds() - t0) * 1e6;
		w.wallUs += us;
		w.wallUsMax = std::max(w.wallUsMax, us);
		if (g_cfg.worldDebug && !t.walls.empty()) {
			std::string s;
			for (const WTile::Wall& wl : t.walls) {
				char b[96];
				snprintf(b, sizeof(b), " [%.1f %.1f %.1fx%.2f m, %.1f m tall]", wl.cx, wl.cy, wl.len, wl.thick, wl.z1 - wl.z0 - 0.5f);
				if (s.size() < 600) s += b;
			}
			Log("ground dbg: %d,%d walls: %zu (%d rays, %d points flattened):%s", t.i, t.j, t.walls.size(), rays, t.flat, s.c_str());
		}
		if (t.partial == 0 && t.fb == 0) MapPut(t);
	}

	// every tick: finished tiles without their wall scan, the soonest needed first, within WorldWallMs (the car's next
	// few tiles always)
	void WorldWallsTick(const std::function<double(int64_t)>& costOf)
	{
		WorldState& w = g_world;
		if (!g_cfg.worldWalls) return;
		std::vector<std::pair<double, int64_t>> todo;
		for (auto& kv : w.tiles) {
			const WTile& t = kv.second;
			if (t.state != 2 || t.wallsDone) continue;
			todo.push_back({ t.pre ? 1e5 + hypot((t.i + 0.5) * WTileM() - w.cx, (t.j + 0.5) * WTileM() - w.cy) : costOf(kv.first), kv.first });
		}
		if (todo.empty()) return;
		std::sort(todo.begin(), todo.end());
		const double t0 = NowSeconds(), limit = g_cfg.worldWallMs / 1000.0;
		for (const auto& td : todo) {
			if (td.first > w.urgentM && NowSeconds() - t0 > limit) break;
			auto it = w.tiles.find(td.second);
			if (it == w.tiles.end()) continue;
			WallScan(it->second);
		}
	}

	// every script tick (drive mode or not: rays still out are always collected)
	// v2.51: one immediate ray against GTA's map (not cars, not props): 1 hit (p, n), 0 nothing, -1 GTA didn't answer at once
	int MapRay(const double a[3], const double b[3], double p[3], double n[3])
	{
		const int h = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, float(a[0]), float(a[1]), float(a[2]), float(b[0]), float(b[1]), float(b[2]), 1,
			g_drv.veh, 7);
		if (!h) return -1;
		int       hit = 0, ent = 0;
		NVec3     end{}, nrm{};
		if (Call<int>(N::GET_SHAPE_TEST_RESULT, h, &hit, &end, &nrm, &ent) != 2) return -1;
		++g_world.landRays;
		if (!hit) return 0;
		p[0] = end.x;
		p[1] = end.y;
		p[2] = end.z;
		const double l = sqrt(double(nrm.x) * nrm.x + double(nrm.y) * nrm.y + double(nrm.z) * nrm.z);
		n[0] = l > 1e-6 ? nrm.x / l : 0.0;
		n[1] = l > 1e-6 ? nrm.y / l : 0.0;
		n[2] = l > 1e-6 ? nrm.z / l : 1.0;
		return 1;
	}

	// v2.51 (your jump note: the car fell under the ground after a jump and was put back): in the air (GTA's ground more than 1.2 m
	// under it), its flight is followed against GTA's map (BeamNG's speed and gravity, 0.1 s steps, immediate rays) to where it
	// lands; the tiles there get the landing's level and are built first (v2.5 kept the ramp's level ahead: the landing was
	// "another level", left out). 20 times a second while driving (one ray), the flight 10 times a second while in the air.
	void WorldAir()
	{
		WorldState&  w = g_world;
		const double now = NowSeconds();
		if (!g_cfg.worldLanding) {
			w.air = w.landOk = false;
			return;
		}
		if (now - w.airCheckT < 0.05) return;
		w.airCheckT = now;
		double       p[3], n[3];
		// v2.71: from 2.5 m over the car (2.63 started 0.5 m over it: a car sinking into a gap in a bridge deck was under the
		// deck's top, the ray saw the river 7 m below and took it for a jump: the deck's tiles were built at the river's level,
		// your fall off the bridge in test 2). GTA's surface at or over the car's bottom: not in the air
		const double a[3] = { w.cx, w.cy, w.cz + 2.5 }, b[3] = { w.cx, w.cy, w.cz - 80.0 };
		const int    r = MapRay(a, b, p, n);
		if (r < 0) return;
		w.airH = r == 1 ? w.cz - p[2] : 80.0;
		const bool air = w.airH > 1.2;
		if (air && !w.air) {
			++w.jumps;
			w.airT = now;
			w.landAt = 0;
		}
		if (!air) {
			if (w.air) w.groundT = now;
			w.air = false;
			if (w.landOk && now - w.groundT > 0.3) {
				if (w.landLogs < 20) {
					++w.landLogs;
					Log("ground: landed after %.1f s in the air, %.1f m from where the first guess said", now - w.airT, hypot(w.cx - w.landFirstX, w.cy - w.landFirstY));
				}
				w.landOk = false;
			}
			return;
		}
		w.air = true;
		w.grounded = false;
		if (now - w.landAt < 0.1) return;
		w.landAt = now;
		// the flight: drawn origin (bottom of the tyres) + speed x t + gravity
		const double g = 9.81 * g_cfg.driveGravity;
		double       q0[3] = { w.cx, w.cy, w.cz }, t = 0;
		for (int s = 1; s <= 50; ++s) {
			const double t1 = s * 0.1;
			const double q1[3] = { w.cx + w.vx * t1, w.cy + w.vy * t1, w.cz + w.vz * t1 - 0.5 * g * t1 * t1 };
			const int    h = MapRay(q0, q1, p, n);
			if (h < 0) return;
			if (h == 1) {
				const bool first = !w.landOk;
				w.landX = p[0];
				w.landY = p[1];
				w.landZ = p[2];
				const double nz = std::max(0.3, n[2]);
				w.landGx = n[2] > 0.3 ? std::clamp(-n[0] / nz, -1.2, 1.2) : 0.0;
				w.landGy = n[2] > 0.3 ? std::clamp(-n[1] / nz, -1.2, 1.2) : 0.0;
				w.landT = t + 0.1 * (hypot(p[0] - q0[0], p[1] - q0[1]) / std::max(1e-6, hypot(q1[0] - q0[0], q1[1] - q0[1])));
				w.landOk = true;
				if (first) {
					w.landFirstX = p[0];
					w.landFirstY = p[1];
					if (w.landLogs < 20)
						Log("ground: the car is in the air (%.1f m over GTA's ground): it lands in about %.1f s, %.0f m ahead, %.1f m %s the ground under it now: ground built there first",
							w.airH, w.landT, hypot(p[0] - w.cx, p[1] - w.cy), fabs(p[2] - (w.cz - w.airH)), p[2] < w.cz - w.airH ? "under" : "over");
				}
				return;
			}
			memcpy(q0, q1, sizeof(q0));
			t = t1;
		}
		w.landOk = false;  // (no landing within 5 s: the usual ground)
	}

	void WorldTick()
	{
		WorldState&  w = g_world;
		const double t0 = NowSeconds();
		++w.tick;
		WorldReplies();
		WorldPoll();
		MapTick();
		if (w.active && w.carOk) {
			const bool wasAir = w.air && w.landOk;
			WorldAir();  // v2.51: jumps
			if (w.tick % 3 == 0 || w.wanted.empty() || (w.air && w.landOk) || wasAir) WorldPlan();
			auto costOf = [&](int64_t k) {
				auto c = w.cost.find(k);
				return c == w.cost.end() ? 1e6 : c->second;
			};
			// tiles the car no longer needs: measuring ones stop (finished ones to the cache); v2.43: built ones stay in
			// BeamNG (WorldKeepBudget); the ring's own tiles stay while in it
			std::string rm;
			int         nrm = 0;
			for (auto it = w.tiles.begin(); it != w.tiles.end();) {
				WTile& t = it->second;
				if (w.keep.count(it->first)) {
					t.pre = false;
					++it;
					continue;
				}
				if (t.pre && (t.state == 1 || (t.state == 2 && !t.sent)) &&
				    hypot((t.i + 0.5) * WTileM() - w.cx, (t.j + 0.5) * WTileM() - w.cy) < g_cfg.worldPrebuildM + WTileM()) {
					++it;
					continue;
				}
				if (g_cfg.worldKeepTris > 0 && ((t.state == 2 && t.sent) || (t.state == 3 && t.gaps >= 99))) {
					++it;
					continue;
				}
				if (t.sent || t.draft) RmAdd(rm, nrm, it->first);
				if (t.state == 2) CacheTile(it->first, std::move(t));
				it = w.tiles.erase(it);
				++w.removed;
			}
			// v2.43: tiles of another level than the car's there (a bridge deck while the car is under it, the road below
			// while it's on the deck): swapped for the right one, nearest first (the level carries over from tile to tile).
			// Never the ground under or right in front of the car.
			{
				std::vector<std::pair<double, int64_t>> bad;
				const double                            T = WTileM(), tol = g_cfg.worldLevelM;
				for (auto& kv : w.tiles) {
					const WTile& t = kv.second;
					if (!(t.state == 2 || (t.state == 3 && t.gaps >= 99))) continue;
					if (!w.wanted.count(kv.first)) continue;
					const double nx = std::clamp(w.cx, t.i * T, (t.i + 1) * T), ny = std::clamp(w.cy, t.j * T, (t.j + 1) * T);
					if (hypot(nx - w.cx, ny - w.cy) < 4.0 + 0.4 * w.spd) continue;
					bool         anchored = false;
					const WTile* from = nullptr;
					const WPlane pl = ExpectPlane(t.i, t.j, &anchored, &from);
					const double E = PlaneZ(pl, (t.i + 0.5) * T, (t.j + 0.5) * T);
					if (!anchored) continue;
					const double off = RefOff(t, pl);
					if (off >= 0 ? off <= 1.5 : fabs(t.lvE - E) < 1.0) continue;  // measured for this level
					bool wrong = fabs(t.lv - E) > tol && fabs(t.lvE - E) > tol;
					// measured for another level and something doesn't fit (a ramp measured from the road beside it...).
					// v2.54: not again within 5 s of its last swap (your 2.53 log: 1371 swaps in 10 minutes, the same spots
					// over and over; each one rebuilt in BeamNG)
					if (!wrong && t.state == 2) {
						wrong = !LevelFits(t, pl, from) && (t.voids > 0 || VoidCount(t) > 0 || fabs(t.lv - E) > 1.0 || CliffBelow(t, pl, g_cfg.worldDropM + 1.0) > 0 || !from || [&] {
							int          cnt = 0;
							const double d = EdgeDiff(t, *from, cnt);
							return cnt >= 3 && d > 1.0;
						}());
						auto st = w.swapT.find(kv.first);
						if (wrong && st != w.swapT.end() && t0 - st->second < 5.0) {
							++w.swapHeld;
							continue;
						}
					}
					if (wrong) bad.push_back({ costOf(kv.first), kv.first });
				}
				std::sort(bad.begin(), bad.end());
				for (size_t q = 0; q < bad.size() && q < 6; ++q) {
					auto   it = w.tiles.find(bad[q].second);
					WTile& t = it->second;
					if (w.swapLogged.size() < 40 && w.swapLogged.insert(bad[q].second).second)
						Log("ground: tile %d,%d is on another level (%.1f m) than the car's there (%.1f m): swapped for the right one", t.i, t.j, t.lv,
							ExpectZ(t.i, t.j));
					if (t.sent || t.draft) w.stale[it->first] = t0 + 2.0;  // (BeamNG keeps it until the new one comes)
					w.swapT[it->first] = t0;
					if (t.state == 2) CacheTile(it->first, std::move(t));
					w.tiles.erase(it);
					++w.swaps;
				}
				// swapped tiles whose new measurement didn't come within 2 s (a gap, nothing at this level): out of BeamNG now
				for (auto it = w.stale.begin(); it != w.stale.end();) {
					if (t0 < it->second) {
						++it;
						continue;
					}
					auto tl = w.tiles.find(it->first);
					if (tl == w.tiles.end() || !(tl->second.sent || tl->second.draft)) {
						RmAdd(rm, nrm, it->first);
						++w.staleGone;
					}
					it = w.stale.erase(it);
				}
				if (w.swapT.size() > 4000) w.swapT.clear();
			}
			if (g_cfg.worldKeepTris > 0 && w.tick % 10 == 0) WorldKeepBudget(rm, nrm);
			if (nrm) DrvSend(("{\"t\":\"rm\",\"ids\":[" + rm + "]}").c_str());
			const double now = NowSeconds();
			// v2.5: the wall scan of finished tiles (before they go to BeamNG)
			WorldWallsTick(costOf);
			// finished tiles: to BeamNG (a finished tile replacing a draft waits while the car is on it); ring tiles that are
			// finished go to the cache; drafts for tiles the car reaches soon whose first pass is done
			for (auto it = w.tiles.begin(); it != w.tiles.end();) {
				WTile& t = it->second;
				if (t.state == 2 && t.pre) {
					++it;  // v2.43: the ring's finished tiles stay here (the next ring tile's level comes from them); sent when wanted
					continue;
				}
				if (t.state == 2 && !t.sent && !t.wallsDone) {
					++it;  // v2.5: its wall scan comes first (next tick at the latest for the road right ahead)
					continue;
				}
				if (t.state == 2 && !t.sent) {
					const double T = WTileM();
					const bool   over = t.healOld ? (w.cx > t.i * T - 2.0 && w.cx < (t.i + 1) * T + 2.0 && w.cy > t.j * T - 2.0 && w.cy < (t.j + 1) * T + 2.0) : CarOnTile(t);
					if (t.draft && over) {
						if (!t.lastUse || t.lastUse + 1 != w.tick) ++w.deferred;
						t.lastUse = w.tick;
					} else {
						WorldSendTile(t, false);
						t.healOld = false;
						std::vector<float>().swap(t.zOld);
					}
				} else if (t.state == 1 && !t.pre && !t.draft && !t.noDraft && t.pass > 0 && t.K > 1 && costOf(it->first) <= w.draftM) {
					WorldSendTile(t, true);
				}
				++it;
			}
			// wanted tiles not in BeamNG, nearest first (v2.43: the level carries over from the one before): from the cache
			// or the saved map at once (no rays) when they have it at the car's level there, else measured; gaps again after
			// 1 s
			int measuring = 0;
			for (auto& kv : w.tiles) measuring += kv.second.state == 1 && !kv.second.pre;
			std::vector<std::pair<double, int64_t>> wl, miss;
			wl.reserve(w.wanted.size());
			for (int64_t k : w.wanted) wl.push_back({ costOf(k), k });
			std::sort(wl.begin(), wl.end());
			int fromStore = 0;
			for (const auto& c : wl) {
				const int64_t k = c.second;
				auto          it = w.tiles.find(k);
				if (it == w.tiles.end()) {
					int i, j;
					WIJ(k, i, j);
					bool         anchored = false;
					const WTile* from = nullptr;
					const WPlane pl = ExpectPlane(i, j, &anchored, &from);
					const double E = PlaneZ(pl, (i + 0.5) * WTileM(), (j + 0.5) * WTileM());
					if (!anchored) continue;  // its level isn't known yet: after its neighbour towards the car
					WTile t;
					if (CacheFind(k, E, nullptr, &pl, from)) {
						if (fromStore >= 24) continue;  // next tick
						CacheTake(k, E, t, &pl, from);
						if (g_cfg.worldDebug) Log("ground dbg: %d,%d from the cache: level %.2f (made for %.2f), the car's there %.2f (from %s)", i, j, t.lv, t.lvE, E, from ? "a neighbour" : "the car");
						auto& slot = w.tiles[k];
						slot = std::move(t);
						if (slot.wallsDone) WorldSendTile(slot, false);  // (v2.5: else after its wall scan)
						++w.fromCache;
						++fromStore;
						continue;
					}
					const int got = fromStore < 24 ? MapGet(k, E, t, &pl, from) : (MapKnows(k, E) ? 2 : 0);
					if (got == 1) {
						w.pitPts += FillPits(t, w.spikePts);  // v2.55: (saved before 2.55: its pits too)
						if (g_cfg.worldDebug) Log("ground dbg: %d,%d from the saved map: level %.2f, the car's there %.2f (from %s)", i, j, t.lv, E, from ? "a neighbour" : "the car");
						auto& slot = w.tiles[k];
						slot = std::move(t);
						if (slot.wallsDone) WorldSendTile(slot, false);  // (v2.5: else after its wall scan)
						++w.fromDisk;
						++fromStore;
						continue;
					}
					if (got == 2) continue;                                // saved: next tick
					if (got == -1 && c.first > w.urgentM) continue;  // its region is being read: wait unless the car is close
					if (w.cache.count(k) || MapKnows(k)) ++w.levelMiss;  // known, but only at another level
					miss.push_back({ c.first, k });
				} else if (it->second.state == 3 && it->second.gaps < 5 && now - it->second.tGap > 1.0) {
					miss.push_back({ c.first + 0.5, k });
				} else if (c.first <= std::max(24.0, 2.0 * w.urgentM) && now - it->second.tGap > 1.5 &&
				           ((it->second.state == 3 && it->second.gaps >= 99 && it->second.noGround && HealCount(k, now) < 3) ||
				            (it->second.state == 2 && it->second.sent && !it->second.pre && it->second.partial > 0 && HealCount(k, now) < 2))) {
					// v2.7: empty or part-empty tiles stayed that way for good (v2.63: gaps = 99, partial tiles never again): game-breaking
					// holes in your 2.63 test. Near the car they are measured again, for the car's level there now; the second time
					// with a tall window
					miss.push_back({ c.first + 0.7, k });
				}
			}
			std::sort(miss.begin(), miss.end());
			for (const auto& m : miss) {
				if (measuring >= g_cfg.worldTilesWork) break;
				int i, j;
				WIJ(m.second, i, j);
				auto it = w.tiles.find(m.second);
				if (it != w.tiles.end()) {
					// a gap: its empty points again, all passes (v2.43: for the car's level there now)
					WTile&       t = it->second;
					t.wide = false;
					if (t.state == 2 || t.gaps >= 99) {
						if (w.healN.size() > 4000) w.healN.clear();
						auto& hn = w.healN[m.second];
						hn.first = HealCount(m.second, now) + 1;
						hn.second = now;
						t.heal = hn.first;
						t.wide = t.state == 3 && t.heal >= 2;
						++w.healed;
						if (w.healLogged.size() < 300 && w.healLogged.insert(m.second * 4 + t.heal).second)
							Log("ground: tile %d,%d %s near the car: measured again (try %d%s)", i, j, t.state == 2 ? "has empty cells" : "is empty", t.heal, t.wide ? ", tall window" : "");
						t.gaps = 2;  // (one go: what is still empty is sent without it)
						t.partial = 0;
						t.noGround = false;
						if (t.sent) {
							// its last version stays in BeamNG until the new one comes, kept like a draft (taken out with it, kept over
							// a drive's end); replaced at once unless the car is on it. Its heights answer "ground here?" meanwhile
							// (the fall check, the put-back spot)
							t.draft = true;
							t.healOld = true;
							t.zOld = t.z;
						}
						t.sent = false;
					}
					const WTile* from = nullptr;
					const WPlane ref = ExpectPlane(i, j, nullptr, &from);
					t.rcx = ref.cx;
					t.rcy = ref.cy;
					t.rz = ref.z;
					t.rx = ref.gx;
					t.ry = ref.gy;
					t.lvE = t.lv = PlaneZ(ref, (i + 0.5) * WTileM(), (j + 0.5) * WTileM());
					t.hasRef = true;
					t.hasFrom = from != nullptr;
					t.fromKey = from ? WKey(from->i, from->j) : 0;
					t.refCar = from == nullptr;
					t.carAir = t.refCar && !w.grounded;
					t.noLevel = false;
					if (t.wide) {
						// v2.7: its own ground decides its level (nothing was at the expected one), every point measured afresh
						t.hasRef = false;
						for (size_t q = 0; q < t.ps.size(); ++q) t.ps[q] = 0, t.src[q] = 0;
					}
					t.walls.clear();
					t.wallsDone = false;
					t.kKey = -1;
					t.gen = ++w.gen;
					t.state = 1;
					t.left = 0;
					t.next = 0;
					t.voids = 0;
					for (size_t q = 0; q < t.ps.size(); ++q)
						if (t.ps[q] != 2 || t.src[q] == 2) {
							t.ps[q] = 0;
							t.tries[q] = 0;
							t.fl[q] = 0;
							t.z[q] = NAN;
							if (q < t.deep.size()) t.deep[q] = 0;  // v2.71
						}
					t.deepFound = t.deepMiss = 0;
					std::fill(t.leaf.begin(), t.leaf.end(), uint8_t(1));
					t.left = NeedCoarse(t);
					if (t.left == 0) WorldFinish(m.second);
				} else {
					w.tiles[m.second] = MakeTile(i, j);
					if (g_cfg.worldDebug) Log("ground dbg: %d,%d measured for level %.2f (slope %.2f %.2f)", i, j, w.tiles[m.second].lvE, w.tiles[m.second].rx, w.tiles[m.second].ry);
					if (w.tiles[m.second].left == 0) WorldFinish(m.second);  // every first-pass point came from neighbours
				}
				++measuring;
			}
			if (miss.empty()) WorldPrebuild();
			// v2.43: standing still with nothing else to do: measured ground nearby (the ring, earlier drives) goes to BeamNG
			// too, nearest first, so the world around you is built before you drive there
			if (g_cfg.worldKeepTris > 0 && g_cfg.worldFillPerS > 0 && miss.empty() && measuring == 0 && w.spd < 5.0 &&
			    double(w.liveTris) < g_cfg.worldKeepTris * 0.8 && now - w.fillT > 1.0 / g_cfg.worldFillPerS) {
				w.fillT = now;
				const double T = WTileM(), R = std::max(64.0, double(g_cfg.worldPrebuildM));
				const int    rt = int(R / T) + 1, ci = int(floor(w.cx / T)), cj = int(floor(w.cy / T));
				double       best = 1e30, bestE = 0;
				int64_t      bestK = 0;
				bool         live = false;
				for (int dj = -rt; dj <= rt; ++dj)
					for (int di = -rt; di <= rt; ++di) {
						const double d = hypot((ci + di + 0.5) * T - w.cx, (cj + dj + 0.5) * T - w.cy);
						if (d > R || d >= best) continue;
						const int64_t k = WKey(ci + di, cj + dj);
						auto          lt = w.tiles.find(k);
						if (lt != w.tiles.end()) {
							if (lt->second.state == 2 && !lt->second.sent && lt->second.wallsDone) {
								best = d;  // a ring tile measured, not in BeamNG yet
								bestK = k;
								live = true;
							}
							continue;
						}
						const bool inCache = w.cache.count(k) != 0, onDisk = MapKnows(k);
						if (!inCache && !onDisk) continue;
						bool         anchored = false;
						const WTile* from = nullptr;
						const WPlane pl = ExpectPlane(ci + di, cj + dj, &anchored, &from);
						const double E = PlaneZ(pl, (ci + di + 0.5) * T, (cj + dj + 0.5) * T);
						if (!anchored || (!(inCache && CacheFind(k, E, nullptr, &pl, from)) && !(onDisk && MapKnows(k, E)))) continue;
						best = d;
						bestK = k;
						bestE = E;
						live = false;
					}
				if (best < 1e29 && live) {
					WTile& t = w.tiles[bestK];
					t.pre = false;
					WorldSendTile(t, false);
					++w.idleSent;
				} else if (best < 1e29) {
					WTile        t;
					int          bi, bj;
					const WTile* from = nullptr;
					WIJ(bestK, bi, bj);
					const WPlane pl = ExpectPlane(bi, bj, nullptr, &from);
					if (CacheTake(bestK, bestE, t, &pl, from) || MapGet(bestK, bestE, t, &pl, from) == 1) {
						auto& slot = w.tiles[bestK];
						slot = std::move(t);
						if (slot.wallsDone) WorldSendTile(slot, false);  // (v2.5: else after its wall scan)
						++w.idleSent;
					}
				}
			}
			// rays: the tiles being measured, soonest needed first; finer passes after the first passes of tiles a little
			// further on (drafts first); the ring last
			const double finer = std::max(16.0, w.spd * 1.0);
			std::vector<std::pair<double, int64_t>> order;
			for (auto& kv : w.tiles) {
				const WTile& t = kv.second;
				if (t.state != 1) continue;
				if (t.pre) {
					order.push_back({ 1e5 + hypot((t.i + 0.5) * WTileM() - w.cx, (t.j + 0.5) * WTileM() - w.cy), kv.first });
					continue;
				}
				order.push_back({ costOf(kv.first) + (t.pass > 0 ? finer : 0.0), kv.first });
			}
			std::sort(order.begin(), order.end());
			WorldIssue(order);
			if (w.tick - w.lastRefuseTick > 120 && w.cap < g_cfg.worldInFlight) w.cap = std::min(g_cfg.worldInFlight, w.cap + 16);
			w.maxWaiting = std::max(w.maxWaiting, uint32_t(w.rays.size()));
			// GTA only keeps collision near the player: ask for it where the car will be in 1 and 2 s
			if (w.tick % 15 == 0 && w.spd > 3) {
				for (int s = 1; s <= 2; ++s) CallVoid(N::REQUEST_COLLISION_AT_COORD, float(w.cx + w.vx * s), float(w.cy + w.vy * s), float(w.cz));
			}
			if (w.tick % 5 == 0) WorldAhead();
		}
		const double us = (NowSeconds() - t0) * 1e6;
		if (w.active) {
			w.usSum += us;
			w.usMax = std::max(w.usMax, us);
			++w.usN;
		}
		WorldLog();
	}

	// F10 (World=1): the origin, and the BeamNG mod told the mapping
	void WorldBegin(double gx, double gy)
	{
		WorldState& w = g_world;
		// a changed tile layout (ini) makes the cache useless
		if (w.cfgTile != g_cfg.worldTileM || w.cfgStep != g_cfg.worldStepNear) {
			if (!w.cache.empty() || !w.tiles.empty()) Log("ground: the tile layout changed (ini): measured tiles forgotten");
			w.cache.clear();
			w.tiles.clear();
			w.cfgTile = g_cfg.worldTileM;
			w.cfgStep = g_cfg.worldStepNear;
		}
		MapReady();
		const bool tooFar = w.haveO && hypot(gx - w.ox, gy - w.oy) > g_cfg.worldRebaseM;  // (not "far": a Windows macro)
		if (!w.haveO || tooFar) {
			if (tooFar) Log("ground: the car is %.0f m from the origin: a new origin (the measured tiles are kept and sent again)", hypot(gx - w.ox, gy - w.oy));
			w.ox = floor(gx / 500.0 + 0.5) * 500.0;
			w.oy = floor(gy / 500.0 + 0.5) * 500.0;
			w.haveO = true;
			w.fresh = true;
			for (auto it = w.tiles.begin(); it != w.tiles.end();) {
				if (it->second.state == 2) CacheTile(it->first, std::move(it->second));
				it = w.tiles.erase(it);
			}
		}
		// tiles still measuring from the last drive: measured again (drafts in BeamNG carry on)
		for (auto it = w.tiles.begin(); it != w.tiles.end();)
			it = (it->second.state == 1 && !it->second.draft) ? w.tiles.erase(it) : std::next(it);
		w.active = true;
		g_worldBusy = true;
		w.carOk = false;
		w.haveHd = false;
		w.yawRate = 0;
		w.air = w.landOk = false;  // v2.51
		w.wanted.clear();
		w.cap = g_cfg.worldInFlight;
		size_t mine = 0;
		for (auto& kv : w.tiles) mine += kv.second.sent || kv.second.draft;
		char m[256];
		snprintf(m, sizeof(m), "{\"t\":\"map\",\"ox\":%.1f,\"oy\":%.1f,\"zl\":%.2f,\"tile\":%.2f,\"fresh\":%d,\"n\":%zu}", w.ox, w.oy, double(g_cfg.worldZLift), WTileM(),
			w.fresh ? 1 : 0, mine);
		DrvSend(m);
		Log("ground: GTA -> BeamNG = GTA - (%.0f, %.0f) and %.0f m up; %s; %zu tiles kept from before, %zu in the cache", w.ox, w.oy, g_cfg.worldZLift,
			w.fresh ? "fresh start" : "same origin as the last drive", mine, CacheCount());
		w.fresh = false;
	}

	void WorldEnd(const char* why)
	{
		WorldState& w = g_world;
		if (!w.active) return;
		w.active = false;
		g_worldBusy = false;
		w.carOk = false;
		for (auto it = w.tiles.begin(); it != w.tiles.end();) {
			WTile& t = it->second;
			if (t.state == 1 && t.draft) {
				// a draft stays in BeamNG under the parked car: kept as it is (no more rays)
				++it;
				continue;
			}
			if (t.state == 1 || (t.state == 2 && t.pre)) {
				if (t.state == 2) CacheTile(it->first, std::move(t));
				it = w.tiles.erase(it);
				continue;
			}
			++it;
		}
		MapSave(true);
		w.matDirty = true;
		WorldWriteMaterials();
		Log("ground: streaming stopped (%s); %zu tiles stay in BeamNG, %zu in the cache; the saved map is being written", why, w.tiles.size(), CacheCount());
	}

	// where the car is for the streaming (GTA world): drawn origin, velocity, up. The heading and turn rate come from the
	// velocity (only when moving).
	void WorldCar(const double O[3], const double V[3], const double U[3])
	{
		WorldState&  w = g_world;
		const double now = NowSeconds();
		const double spd = sqrt(V[0] * V[0] + V[1] * V[1]);
		if (spd > 3.0) {
			const double h = atan2(V[1], V[0]);
			if (w.haveHd && now - w.lastHdT > 0.0005 && now - w.lastHdT < 0.5) {
				double dh = h - w.hd;
				dh -= 2 * kPi * floor((dh + kPi) / (2 * kPi));
				const double rate = dh / (now - w.lastHdT);
				w.yawRate += (rate - w.yawRate) * std::min(1.0, (now - w.lastHdT) * 4.0);
			}
			w.hd = h;
			w.haveHd = true;
			w.lastHdT = now;
		} else {
			w.yawRate *= 0.9;
		}
		if (!w.carOk || hypot(O[0] - w.cx, O[1] - w.cy) > 30.0) w.placedT = now;
		w.cx = O[0];
		w.cy = O[1];
		w.cz = O[2];
		w.vx = V[0];
		w.vy = V[1];
		w.ux = U[0];
		w.uy = U[1];
		w.uz = U[2];
		w.vz = V[2];
		// v2.43: on its ground (not jumping or falling): the level under the car is the car's level
		double     gz = 0;
		const bool have = WorldGroundAt(O[0], O[1], gz, nullptr, O[2]);
		// v2.54: a hard hit (BeamNG's car lost 8 m/s within 0.3 s; v2.54: 4): the wall boxes around it (an invisible wall: which box, how
		// high, which tile)
		{
			const double sp3 = sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
			w.spdHist.push_back({ now, sp3 });
			size_t old = 0;
			while (old < w.spdHist.size() && now - w.spdHist[old].first > 0.3) ++old;
			w.spdHist.erase(w.spdHist.begin(), w.spdHist.begin() + ptrdiff_t(old));
			double before = sp3;
			for (const auto& h : w.spdHist) before = std::max(before, h.second);
			if (before - sp3 > 8.0 && now - w.hitLogT > 3.0 && w.hitLogs < 30) {  // v2.56: 8 m/s (4 fired on hard braking)
				w.hitLogT = now;
				++w.hitLogs;
				std::string nearW;
				for (const auto& kv : w.tiles)
					for (const WTile::Wall& wl : kv.second.walls) {
						if (hypot(wl.cx - O[0], wl.cy - O[1]) > 6.0 + wl.len * 0.5 || nearW.size() > 900) continue;
						char b[160];
						snprintf(b, sizeof(b), " [tile %d,%d: at %.1f %.1f, %.1f m long, %.2f m thick, z %.1f to %.1f]", kv.second.i, kv.second.j, wl.cx, wl.cy, wl.len, wl.thick, wl.z0, wl.z1);
						nearW += b;
					}
				Log("crash: BeamNG's car lost %.1f m/s (from %.1f) at GTA %.1f %.1f %.1f, heading %.0f deg; ground under it %s%.1f; wall boxes near it:%s", before - sp3, before,
					O[0], O[1], O[2], atan2(V[1], V[0]) / kDeg, have ? "" : "(not known) ", have ? gz : 0.0, nearW.empty() ? " none" : nearW.c_str());
			}
		}
		w.grounded = !w.air && fabs(V[2]) < 3.0 && (!have || (O[2] - gz < 1.5 && O[2] - gz > -2.0));  // v2.51: and not in the air (WorldAir)
		w.carOk = true;
	}

	// GTA world -> BeamNG world (the fixed mapping)
	void GtaToBeam(const double g[3], double b[3])
	{
		b[0] = g[0] - g_world.ox;
		b[1] = g[1] - g_world.oy;
		b[2] = g[2] + g_cfg.worldZLift;
	}

	// ---------------------------------------------------------------------------------------
	// v2.5: GTA's props (lamp posts, signs, bins, fences, hydrants...) and BeamNG's car. GTA keeps them in its object pool
	// (Script Hook V's worldGetAllObjects): the ones near the car are boxes (their model's size, turned like them). BeamNG's car
	// hitting a soft one (most of them: they break or fly in GTA too) knocks it over in GTA and costs BeamNG's car a little
	// speed (a bump); a solid one (big ones, and the ones GtaBeamD1_props.json calls solid) is a solid box in BeamNG.
	// GtaBeamD1_props_seen.txt lists every model met, how it was treated and why, so the json can be tuned.
	// ---------------------------------------------------------------------------------------
	uint32_t Joaat(const char* s)  // GTA's name hash (Jenkins one-at-a-time, lower case)
	{
		uint32_t h = 0;
		for (; *s; ++s) {
			h += uint32_t(tolower((unsigned char)*s));
			h += h << 10;
			h ^= h >> 6;
		}
		h += h << 3;
		h ^= h >> 11;
		h += h << 15;
		return h;
	}

	// v2.51: kinds. soft: small things (bins, cones, boxes): GTA knocks them over when the drawn car touches them, BeamNG's car
	// loses a little speed (not in BeamNG). breakaway: breakable posts and poles, signs, traffic lights, fences: a box in BeamNG
	// (their trunk at bumper height, measured with rays: no arms over the road) that BeamNG's car really hits; when the car has
	// lost "break" m/s against it the box goes and GTA knocks the prop over. solid: big or unbreakable ones: a box that stays.
	enum PropKind { kPropSoft = 0, kPropSolid = 1, kPropIgnore = 2, kPropBreak = 3 };
	const char* PropKindName(int k) { return k == kPropSoft ? "soft" : k == kPropSolid ? "solid" : k == kPropBreak ? "breakaway" : "ignored"; }

	struct PropModel
	{
		bool        init = false, dimsOk = false, frag = false, listed = false;
		float       mn[3] = {}, mx[3] = {};
		int         kind = kPropSoft;
		std::string why, name;
		float       bump = -1, push = -1, brk = -1;  // from the json (-1: by its size)
		float       mass = -1;                       // v2.52: from the json (kg; -1: by its size)
		std::string fx;                              // v2.7: from the json ("": by what it is)
		uint32_t    seen = 0, hits = 0, breaks = 0;
		// v2.51: what of it BeamNG's car can hit (model frame boxes x0 x1 y0 y1): its trunk at bumper height (rays), else its box
		int         nb = 0;
		float       tb[4][4] = {};
		int         meas = 0;  // 0 not yet, 1 measured with rays, 2 its whole box (short), 3 a post at its foot (rays found nothing)
		int         tries = 0, rays = 0;
	};
	struct PropLive
	{
		int      ent = 0;
		uint32_t model = 0;
		double   c[3] = {}, ax[3][3] = {}, he[3] = {};  // box centre, axes (right, forward, up), half sizes along them
		double   ps[3] = {};                            // its origin (GET_ENTITY_MATRIX)
		double   lastHit = -100;
		bool     sent = false;     // its boxes are in BeamNG
		double   sentC[3] = {}, sentYaw = 0;
		uint32_t scan = 0;
		bool     broken = false;   // v2.51: BeamNG's car broke it: knocked over in GTA, never sent again
		// v2.52: a physics prop (BeamNG simulates its box; GTA's prop copies it): where it was when it went in (origin, axes,
		// the box's middle), whether GTA's own physics is frozen on it
		bool     phys = false, physFailed = false, frozen = false, knocked = false, anchored = false;
		double   P0[3] = {}, A0[3][3] = {}, Cg0[3] = {};
		double   brokenT = 0, brokenC[3] = {};
		bool     checked = true;
		int      copy = 0;         // v2.71: the plugin's copy that took its place in GTA (0: none yet)
		bool     isCopy = false;   // v2.71: this one is a copy
	};
	// v2.71: a map prop swapped for the plugin's copy (where, to bring the map's back when the player is far)
	struct PropCopy
	{
		int      orig = 0;
		uint32_t model = 0;
		double   p[3] = {}, t = 0;
	};
	struct PropCfg
	{
		int   kind = kPropSoft;
		float bump = -1, push = -1, brk = -1, mass = -1;
		std::string name;
		std::string fx;  // v2.7: "sparks", "water", "none" ("": by what it is)
	};
	struct PropState
	{
		// v2.7: GTA particle effects when BeamNG's car breaks a prop (asset, effect; GtaBeamD1_props.json "fxSparks" / "fxWater")
		std::string                   fxSparks[2] = { "core", "ent_brk_sparking_wires" }, fxWater[2] = { "core", "ent_sht_water" };
		std::vector<std::pair<int, double>> fxLoops;  // running water jets: handle, when they stop
		uint32_t                      fxN = 0, fxFail = 0;
		bool                          cfgLoaded = false;
		std::map<uint32_t, PropCfg>   cfg;
		std::string                   cfgSource;
		std::map<uint32_t, PropModel> models;
		std::map<int, PropLive>       live;
		std::vector<int>              buf;
		double                        scanT = 0;
		uint32_t                      scan = 0;
		bool                          seenDirty = false;
		ULONGLONG                     seenMs = 0, logMs = 0;
		int                           movedLogs = 0;
		// stats (10 s)
		uint32_t pool = 0, nearN = 0, hits = 0, bumps = 0, sentN = 0, rmN = 0, scans = 0, breaks = 0, measured = 0, measRays = 0;
		// v2.52: physics props: -1 not known yet, 1 BeamNG spawns them, 0 it can't (fixed boxes instead)
		int           physOk = -1;
		std::set<int> knockedEnts;  // knocked over by BeamNG's car: loose when they go in again
		std::map<int, PropCopy> copies;  // v2.71: by the copy's handle
		std::set<int>           hidden;  // v2.71: map props swapped for a copy (skipped)
		double                  copyT = 0;
		uint32_t                copyN = 0, copyWait = 0, copyFail = 0, copyGone = 0, copyLogs = 0, fallN = 0;  // fallN: v2.73
		uint32_t      physSent = 0, physPoses = 0, physKnocked = 0, physFails = 0, physLogs = 0;
		double   bumpSum = 0, scanUs = 0, scanUsMax = 0, measUs = 0;
	} g_props;

	const char* kPropsDefault = R"JSON({
  "about": "GTA x Beam v2.71: GTA's props near BeamNG's car. v2.71: a knocked prop is swapped for a copy that falls with GTA's physics (the map's own don't move); hydrants listed. v2.7: a broken prop gives GTA's particle effect named by its 'fx' (sparks, water, none; not given: hydrants water, breakaway props sparks); fxSparks / fxWater = [asset, effect]. v2.52: the nearest breakaway and soft ones are physics props (weighted boxes BeamNG simulates, GTA's prop copies them; PropPhysics in the ini): 'mass' (kg) sets one's weight (not listed: by its size), 'break' how hard a post must be hit to snap. kind: 'soft' (bins, cones, boxes: GTA knocks it over and BeamNG's car loses a little speed, 'bump' m/s), 'breakaway' (posts, poles, signs, traffic lights, fences: a box in BeamNG your car really hits; it breaks when the car has lost 'break' m/s against it, then GTA knocks it over), 'solid' (a box that never breaks), 'ignore'. Not listed: breakable and tall or wide = breakaway, big or tall = solid, the rest soft. 'model' is the model name or its hash (0x...). GtaBeamD1_props_seen.txt lists the models your drives met. Ctrl+F11 re-reads this file.",
  "fxSparks": ["core", "ent_brk_sparking_wires"],
  "fxWater": ["core", "ent_sht_water"],
  "props": [
    { "model": "prop_container_01a", "kind": "solid" },
    { "model": "prop_container_05a", "kind": "solid" },
    { "model": "prop_bollard_02a", "kind": "solid" },
    { "model": "prop_bollard_03a", "kind": "solid" },
    { "model": "prop_streetlight_01", "kind": "breakaway", "break": 3.0 },
    { "model": "prop_streetlight_03", "kind": "breakaway", "break": 3.0 },
    { "model": "prop_traffic_01a", "kind": "breakaway", "break": 4.0 },
    { "model": "prop_fire_hydrant_1", "kind": "breakaway", "break": 4.0, "fx": "water" },
    { "model": "prop_fire_hydrant_2", "kind": "breakaway", "break": 4.0, "fx": "water" },
    { "model": "prop_fire_hydrant_2_l1", "kind": "breakaway", "break": 4.0, "fx": "water" },
    { "model": "prop_fire_hydrant_4", "kind": "breakaway", "break": 4.0, "fx": "water" },
    { "model": "prop_bin_01a", "kind": "soft", "bump": 0.6 },
    { "model": "prop_roadcone02a", "kind": "soft", "bump": 0.1 }
  ]
})JSON";

	bool ParseProps(const std::string& js, std::map<uint32_t, PropCfg>& out, int& bad)
	{
		out.clear();
		bad = 0;
		const size_t a = js.find("\"props\"");
		if (a == std::string::npos) return false;
		size_t p = js.find('[', a);
		while (p != std::string::npos) {
			const size_t o = js.find('{', p);
			const size_t close = js.find(']', p);
			if (o == std::string::npos || (close != std::string::npos && close < o)) break;
			const size_t e = js.find('}', o);
			if (e == std::string::npos) break;
			const std::string obj = js.substr(o, e - o + 1);
			p = e + 1;
			std::string model, kind;
			JsonStr(obj, "model", model);
			JsonStr(obj, "kind", kind);
			for (char& ch : kind) ch = char(tolower((unsigned char)ch));
			if (kind == "break") kind = "breakaway";
			if (model.empty() || (kind != "soft" && kind != "solid" && kind != "ignore" && kind != "breakaway")) {
				++bad;
				continue;
			}
			uint32_t h = 0;
			if (model.size() > 2 && model[0] == '0' && (model[1] == 'x' || model[1] == 'X')) h = uint32_t(strtoul(model.c_str(), nullptr, 16));
			else h = Joaat(model.c_str());
			PropCfg c;
			c.kind = kind == "soft" ? kPropSoft : kind == "solid" ? kPropSolid : kind == "breakaway" ? kPropBreak : kPropIgnore;
			double v = 0;
			if (JsonNumC(obj.c_str(), "bump", v)) c.bump = float(v);
			if (JsonNumC(obj.c_str(), "push", v)) c.push = float(v);
			if (JsonNumC(obj.c_str(), "break", v)) c.brk = float(v);
			if (JsonNumC(obj.c_str(), "mass", v)) c.mass = float(v);
			JsonStr(obj, "fx", c.fx);
			c.name = model;
			out[h] = c;
		}
		return true;
	}

	void LoadProps()
	{
		PropState& P = g_props;
		char       path[MAX_PATH + 16];
		PathNextToModule("_props.json", path, sizeof(path));
		std::string js;
		bool        fromFile = false;
		if (FILE* f = fopen(path, "rb")) {
			char   b[4096];
			size_t n;
			while ((n = fread(b, 1, sizeof(b), f)) > 0) js.append(b, n);
			fclose(f);
			fromFile = true;
		}
		int bad = 0;
		if (fromFile && ParseProps(js, P.cfg, bad)) {
			P.cfgSource = path;
			if (js.find("breakaway") == std::string::npos) Log("props: GtaBeamD1_props.json is the 2.5 one (no breakaway props listed): copy the 2.51 one from the kit");
		} else {
			if (fromFile) Log("props: GtaBeamD1_props.json not usable (no \"props\" list): the built-in list is used");
			ParseProps(kPropsDefault, P.cfg, bad);
			P.cfgSource = "built-in list";
			if (!fromFile) {
				if (FILE* f = fopen(path, "wb")) {
					fputs(kPropsDefault, f);
					fclose(f);
					P.cfgSource = std::string("built-in list (written to ") + path + ")";
				}
			}
		}
		if (bad) Log("props: %d entries in GtaBeamD1_props.json not understood (each needs \"model\" and \"kind\": soft, breakaway, solid or ignore)", bad);
		P.models.clear();  // classified again with the new list
		{
			std::string fx[2];
			if (JsonStrs(js, "fxSparks", fx, 2) == 2) P.fxSparks[0] = fx[0], P.fxSparks[1] = fx[1];
			if (JsonStrs(js, "fxWater", fx, 2) == 2) P.fxWater[0] = fx[0], P.fxWater[1] = fx[1];
		}
		CallVoid(N::REQUEST_NAMED_PTFX_ASSET, P.fxSparks[0].c_str());  // v2.7: loaded before the first hit
		CallVoid(N::REQUEST_NAMED_PTFX_ASSET, P.fxWater[0].c_str());
		P.cfgLoaded = true;
		Log("props: %zu models listed (%s)", P.cfg.size(), P.cfgSource.c_str());
	}

	// a model's size and treatment (once per model)
	PropModel& PropModelOf(uint32_t model, int ent)
	{
		PropModel& m = g_props.models[model];
		if (m.init) return m;
		m.init = true;
		NVec3 mn{}, mx{};
		CallVoid(N::GET_MODEL_DIMENSIONS, model, &mn, &mx);
		m.mn[0] = mn.x;
		m.mn[1] = mn.y;
		m.mn[2] = mn.z;
		m.mx[0] = mx.x;
		m.mx[1] = mx.y;
		m.mx[2] = mx.z;
		const float sx = mx.x - mn.x, sy = mx.y - mn.y, sz = mx.z - mn.z;
		m.dimsOk = std::isfinite(sx) && std::isfinite(sy) && std::isfinite(sz) && sx > 0.01f && sy > 0.01f && sz > 0.01f;
		m.frag = Call<int>(N::GET_IS_ENTITY_A_FRAG, ent) != 0;
		auto it = g_props.cfg.find(model);
		if (it != g_props.cfg.end()) {
			m.listed = true;
			m.kind = it->second.kind;
			m.bump = it->second.bump;
			m.push = it->second.push;
			m.brk = it->second.brk;
			m.mass = it->second.mass;
			m.fx = it->second.fx;
			m.name = it->second.name;
			m.why = "listed";
		} else if (!m.dimsOk) {
			m.kind = kPropIgnore;
			m.why = "no size";
		} else if (std::max({ sx, sy, sz }) > 15.0f) {
			m.kind = kPropIgnore;
			m.why = "huge (part of the map)";
		} else if (std::max({ sx, sy, sz }) < 0.25f) {
			m.kind = kPropIgnore;
			m.why = "tiny";
		} else if (m.frag && (sz >= 1.8f || (std::max(sx, sy) >= 1.0f && sz >= 0.6f))) {  // v2.52: wide ones only when not low
			m.kind = kPropBreak;  // v2.51: lamp posts, signs, traffic lights, fences, bus stops
			m.why = "breakable, tall or wide";
		} else if (std::min(sx, sy) >= 1.5f && std::max(sx, sy) >= 2.5f && sz >= 1.0f) {
			m.kind = kPropSolid;
			m.why = "big";
		} else if (!m.frag && sz >= 1.8f) {
			m.kind = kPropSolid;  // v2.51: a tall post GTA can't break
			m.why = "tall, not breakable";
		} else {
			m.kind = kPropSoft;
			m.why = m.frag ? "small, breakable" : "small";
		}
		g_props.seenDirty = true;
		return m;
	}

	// v2.51: the prop's model frame -> GTA world
	inline void PropWorld(const PropLive& L, double a, double b, double h, double out[3])
	{
		for (int k = 0; k < 3; ++k) out[k] = L.ps[k] + L.ax[0][k] * a + L.ax[1][k] * b + L.ax[2][k] * h;
	}

	// v2.51: what of a tall prop BeamNG's car can hit: rays across it at 0.6 m over its foot that see this prop only, from both
	// sides, on lines 0.15-0.45 m apart; the cells they hit, grouped, are its parts (your v2.5 log: traffic lights' arms over the
	// road, 10 m long, counted as hits). Once per model (props of a model are the same shape). false: nothing seen (its
	// collision may not be loaded yet: tried again on the next one met)
	bool PropMeasure(PropModel& m, const PropLive& L)
	{
		const double t0 = NowSeconds();
		const double sx = m.mx[0] - m.mn[0], sy = m.mx[1] - m.mn[1];
		const double cell = std::max(0.1, std::max(sx, sy) / 64.0);
		const double x0 = m.mn[0] - 0.2, y0 = m.mn[1] - 0.2;
		const int    gx = std::clamp(int((sx + 0.4) / cell) + 1, 1, 80), gy = std::clamp(int((sy + 0.4) / cell) + 1, 1, 80);
		std::vector<uint8_t> occ(size_t(gx) * gy, 0);
		std::vector<std::array<double, 2>> pts;  // the points met (the parts' boxes are fitted to them, not to whole cells)
		std::vector<size_t>                ptCell;
		auto mark = [&](double a, double b) {
			const int i = int(floor((a - x0) / cell)), j = int(floor((b - y0) / cell));
			if (i < 0 || j < 0 || i >= gx || j >= gy) return;
			occ[size_t(j) * gx + i] = 1;
			pts.push_back({ a, b });
			ptCell.push_back(size_t(j) * gx + i);
		};
		const double h = m.mn[2] + std::min(0.6, (m.mx[2] - m.mn[2]) * 0.5);
		int          rays = 0, hits = 0;
		// one ray from (a0, b0) to (a1, b1) in the model frame: where it first meets this prop
		auto ray = [&](double a0, double b0, double a1, double b1, double& ha, double& hb) -> bool {
			double A[3], B[3];
			PropWorld(L, a0, b0, h, A);
			PropWorld(L, a1, b1, h, B);
			++rays;
			const int hnd = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, float(A[0]), float(A[1]), float(A[2]), float(B[0]), float(B[1]), float(B[2]), 16,
				g_drv.veh, 7);
			if (!hnd) return false;
			int   hit = 0, ent = 0;
			NVec3 end{}, nrm{};
			if (Call<int>(N::GET_SHAPE_TEST_RESULT, hnd, &hit, &end, &nrm, &ent) != 2 || !hit || ent != L.ent) return false;
			const double d[3] = { end.x - L.ps[0], end.y - L.ps[1], end.z - L.ps[2] };
			ha = d[0] * L.ax[0][0] + d[1] * L.ax[0][1] + d[2] * L.ax[0][2];
			hb = d[0] * L.ax[1][0] + d[1] * L.ax[1][1] + d[2] * L.ax[1][2];
			++hits;
			return true;
		};
		for (int dir = 0; dir < 2; ++dir) {
			const double across = dir == 0 ? sy : sx;
			const int    nl = std::clamp(int(ceil(across / 0.15)), 2, 24);
			for (int k = 0; k < nl; ++k) {
				const double t = (k + 0.5) / nl;
				double       a0, b0, a1, b1;
				if (dir == 0) {
					b0 = b1 = m.mn[1] + t * sy;
					a0 = m.mn[0] - 0.3;
					a1 = m.mx[0] + 0.3;
				} else {
					a0 = a1 = m.mn[0] + t * sx;
					b0 = m.mn[1] - 0.3;
					b1 = m.mx[1] + 0.3;
				}
				double     fa = 0, fb = 0, ba = 0, bb = 0;
				const bool f = ray(a0, b0, a1, b1, fa, fb), bk = ray(a1, b1, a0, b0, ba, bb);
				if (f && bk) {
					const double len = hypot(ba - fa, bb - fb);
					// one solid between them (a post, a container), unless a ray from just inside the first face meets this prop again
					// (two things far apart: a gantry's legs; a ray starting inside a solid doesn't see it)
					bool two = false;
					if (len > 1.5) {
						const double ux = (ba - fa) / len, uy = (bb - fb) / len;
						double       ma = 0, mb = 0;
						two = ray(fa + ux * 0.05, fb + uy * 0.05, ba - ux * 0.05, bb - uy * 0.05, ma, mb);
						if (two) mark(ma, mb);
					}
					if (!two) {
						const int steps = int(len / (cell * 0.5)) + 1;
						for (int s = 0; s <= steps; ++s) mark(fa + (ba - fa) * s / steps, fb + (bb - fb) * s / steps);
					} else {
						mark(fa, fb);
						mark(ba, bb);
					}
				} else if (f) {
					mark(fa, fb);
				} else if (bk) {
					mark(ba, bb);
				}
			}
		}
		// the cells met, grouped (touching ones are one part), the 4 biggest as boxes
		std::vector<int>                       comp(occ.size(), -1);
		std::vector<std::pair<int, std::array<int, 4>>> parts;  // cells, i0 i1 j0 j1
		for (int j = 0; j < gy; ++j)
			for (int i = 0; i < gx; ++i) {
				const size_t s0 = size_t(j) * gx + i;
				if (!occ[s0] || comp[s0] >= 0) continue;
				const int                  id = int(parts.size());
				std::array<int, 4>         bb = { i, i, j, j };
				int                        cnt = 0;
				std::vector<std::pair<int, int>> st{ { i, j } };
				comp[s0] = id;
				while (!st.empty()) {
					const auto q = st.back();
					st.pop_back();
					++cnt;
					bb[0] = std::min(bb[0], q.first);
					bb[1] = std::max(bb[1], q.first);
					bb[2] = std::min(bb[2], q.second);
					bb[3] = std::max(bb[3], q.second);
					for (int dj = -1; dj <= 1; ++dj)
						for (int di = -1; di <= 1; ++di) {
							const int a = q.first + di, b = q.second + dj;
							if (a < 0 || b < 0 || a >= gx || b >= gy) continue;
							const size_t s1 = size_t(b) * gx + a;
							if (occ[s1] && comp[s1] < 0) {
								comp[s1] = id;
								st.push_back({ a, b });
							}
						}
				}
				parts.push_back({ cnt, bb });
			}
		// each part's box: around the points met in it (a thin post: its real width, not whole cells)
		std::vector<std::array<double, 4>> pb(parts.size(), std::array<double, 4>{ 1e9, -1e9, 1e9, -1e9 });
		for (size_t q = 0; q < pts.size(); ++q) {
			auto& b = pb[size_t(comp[ptCell[q]])];
			b[0] = std::min(b[0], pts[q][0]);
			b[1] = std::max(b[1], pts[q][0]);
			b[2] = std::min(b[2], pts[q][1]);
			b[3] = std::max(b[3], pts[q][1]);
		}
		std::vector<int> ord(parts.size());
		for (size_t q = 0; q < ord.size(); ++q) ord[q] = int(q);
		std::sort(ord.begin(), ord.end(), [&](int x, int y) { return parts[size_t(x)].first > parts[size_t(y)].first; });
		m.nb = 0;
		for (size_t q = 0; q < ord.size() && m.nb < 4; ++q) {
			const auto& b = pb[size_t(ord[q])];
			float*      t = m.tb[m.nb++];
			// (at least 10 cm across: a ray meets a post's faces, not its middle)
			const double cx = (b[0] + b[1]) * 0.5, cy = (b[2] + b[3]) * 0.5, hx = std::max(0.05, (b[1] - b[0]) * 0.5), hy = std::max(0.05, (b[3] - b[2]) * 0.5);
			t[0] = float(cx - hx);
			t[1] = float(cx + hx);
			t[2] = float(cy - hy);
			t[3] = float(cy + hy);
		}
		m.rays += rays;
		g_props.measRays += uint32_t(rays);
		g_props.measUs += (NowSeconds() - t0) * 1e6;
		++g_props.measured;
		if (m.nb > 0) {
			m.meas = 1;
			char   b[160];
			double w = 0, l = 0;
			for (int q = 0; q < m.nb; ++q) {
				w = std::max(w, double(m.tb[q][1] - m.tb[q][0]));
				l = std::max(l, double(m.tb[q][3] - m.tb[q][2]));
			}
			snprintf(b, sizeof(b), "%d part(s), the biggest %.2f x %.2f m (%d of %d rays)", m.nb, w, l, hits, rays);
			m.why += std::string("; trunk measured: ") + b;
			g_props.seenDirty = true;
			if (g_props.measured <= 40)
				Log("props: model 0x%08X %s (%.1f x %.1f x %.1f m): what a car hits at 0.6 m: %s", L.model, m.name.empty() ? "" : m.name.c_str(), sx, sy,
					m.mx[2] - m.mn[2], b);
			return true;
		}
		return false;
	}

	// v2.51: a model's parts BeamNG's car can hit (model frame): measured (tall ones), or its whole box. canMeasure: rays allowed now
	bool PropParts(PropModel& m, const PropLive& L, bool canMeasure)
	{
		if (m.meas) return m.nb > 0;
		const bool tall = m.mx[2] - m.mn[2] >= 1.8f;
		if (!tall) {
			m.nb = 1;
			m.tb[0][0] = m.mn[0], m.tb[0][1] = m.mx[0], m.tb[0][2] = m.mn[1], m.tb[0][3] = m.mx[1];
			m.meas = 2;
			return true;
		}
		if (!canMeasure) return false;  // (next scan: each model costs up to ~100 rays)
		if (PropMeasure(m, L)) return true;
		if (++m.tries < 3) return false;
		// nothing seen three times: a post at its foot (GTA's poles stand on their model's origin) if that is inside it, else its box
		m.nb = 1;
		if (m.mn[0] <= 0 && m.mx[0] >= 0 && m.mn[1] <= 0 && m.mx[1] >= 0) {
			const float r = std::min(0.2f, std::min(m.mx[0] - m.mn[0], m.mx[1] - m.mn[1]) * 0.5f);
			m.tb[0][0] = -r, m.tb[0][1] = r, m.tb[0][2] = -r, m.tb[0][3] = r;
			m.why += "; trunk not seen by rays: a post at its foot";
		} else {
			m.tb[0][0] = m.mn[0], m.tb[0][1] = m.mx[0], m.tb[0][2] = m.mn[1], m.tb[0][3] = m.mx[1];
			m.why += "; trunk not seen by rays: its whole box";
		}
		m.meas = 3;
		g_props.seenDirty = true;
		return true;
	}

	// how hard BeamNG's car must hit it (the speed it loses against it, m/s): the json's, else by its thickness (a thin post
	// 2.8, a traffic light pole 4, a fence 2)
	double PropBreakDv(const PropModel& m)
	{
		if (m.kind != kPropBreak) return 0.0;
		double v = m.brk;
		if (!(v > 0)) {
			double t = 0;
			for (int q = 0; q < m.nb; ++q) t = std::max(t, double(std::min(m.tb[q][1] - m.tb[q][0], m.tb[q][3] - m.tb[q][2])));
			v = std::clamp(1.0 + 6.0 * t, 1.5, 4.0);
		}
		return std::max(0.3, v * g_cfg.propBreak);
	}

	// BeamNG's boxes of a prop ("pb"): id, break (m/s, 0 = never), count, then per box: centre x y, bottom, top, along x y (its
	// forward), length, thickness (BeamNG world)
	void PropBoxMsg(std::string& out, const PropLive& L, const PropModel& m)
	{
		const double sz = m.mx[2] - m.mn[2];
		double       fx = L.ax[1][0], fy = L.ax[1][1];
		const double fl = sqrt(fx * fx + fy * fy);
		if (fl < 1e-6) {
			fx = 1;
			fy = 0;
		} else {
			fx /= fl;
			fy /= fl;
		}
		char b[200];
		snprintf(b, sizeof(b), "%s%d,%.2f,%d", out.empty() ? "" : ",", L.ent, PropBreakDv(m), m.nb);
		out += b;
		for (int q = 0; q < m.nb; ++q) {
			const float* t = m.tb[q];
			double       g[3], c[3];
			PropWorld(L, (t[0] + t[1]) * 0.5, (t[2] + t[3]) * 0.5, m.mn[2], g);
			GtaToBeam(g, c);
			snprintf(b, sizeof(b), ",%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.3f,%.3f", c[0], c[1], c[2] - 0.25, c[2] + std::min(sz, 3.0), fx, fy, double(t[3] - t[2]),
				double(t[1] - t[0]));
			out += b;
		}
	}

	void PropsSeenWrite();
	void PropsRemoveAll(const char* why)
	{
		PropState& P = g_props;
		if (P.seenDirty) PropsSeenWrite();
		std::string ids;
		int         n = 0;
		for (auto& kv : P.live)
			if (kv.second.sent || kv.second.phys) {
				ids += (n ? "," : "") + std::to_string(kv.first);
				++n;
			}
		if (n) {
			DrvSend(("{\"t\":\"prm\",\"ids\":[" + ids + "]}").c_str());
			Log("props: %d prop boxes taken out of BeamNG (%s)", n, why);
		}
		P.live.clear();
	}

	// the car's box against a prop's, seen from above (separating axes), and their heights
	bool PropTouch(const DriveState& d, const PropLive& L, double w, double l, double h)
	{
		const double cz0 = d.carO[2], cz1 = d.carO[2] + h;
		const double pz0 = L.c[2] - L.he[2], pz1 = L.c[2] + L.he[2];
		if (pz1 < cz0 + 0.05 || pz0 > cz1) return false;
		double ca[2][2], pa[2][2];
		auto flat = [](const double* v, double* o) {
			const double l2 = sqrt(v[0] * v[0] + v[1] * v[1]);
			o[0] = l2 > 1e-6 ? v[0] / l2 : 1.0;
			o[1] = l2 > 1e-6 ? v[1] / l2 : 0.0;
		};
		flat(d.carR, ca[0]);
		flat(d.carF, ca[1]);
		flat(L.ax[0], pa[0]);
		flat(L.ax[1], pa[1]);
		const double ch[2] = { w * 0.5 * 0.95, l * 0.5 * 0.97 }, ph[2] = { L.he[0], L.he[1] };
		const double cc[2] = { d.carO[0] + d.carU[0] * h * 0.5, d.carO[1] + d.carU[1] * h * 0.5 };
		const double dx = L.c[0] - cc[0], dy = L.c[1] - cc[1];
		const double* axes[4] = { ca[0], ca[1], pa[0], pa[1] };
		for (const double* a : axes) {
			const double dist = fabs(dx * a[0] + dy * a[1]);
			const double rc = ch[0] * fabs(ca[0][0] * a[0] + ca[0][1] * a[1]) + ch[1] * fabs(ca[1][0] * a[0] + ca[1][1] * a[1]);
			const double rp = ph[0] * fabs(pa[0][0] * a[0] + pa[0][1] * a[1]) + ph[1] * fabs(pa[1][0] * a[0] + pa[1][1] * a[1]);
			if (dist > rc + rp) return false;
		}
		return true;
	}

	void PropsSeenWrite()
	{
		PropState& P = g_props;
		char       path[MAX_PATH + 16];
		PathNextToModule("_props_seen.txt", path, sizeof(path));
		FILE* f = fopen(path, "wb");
		if (!f) return;
		fprintf(f, "GTA props your drives met (GtaBeamD1 %s): model (hash; name when GtaBeamD1_props.json lists it), how it was treated and why, its size (m), breakable, "
		           "times seen near the car, times BeamNG's car hit it (soft) or broke it (breakaway). Copy a line's model into GtaBeamD1_props.json to change it.\r\n\r\n",
			D1_VERSION);
		std::vector<std::pair<uint32_t, uint32_t>> order;
		for (auto& kv : P.models) order.push_back({ kv.second.seen, kv.first });
		std::sort(order.rbegin(), order.rend());
		for (auto& o : order) {
			const PropModel& m = P.models[o.second];
			fprintf(f, "0x%08X  %-24s %-9s (%s)  %.2f x %.2f x %.2f m  breakable %d  seen %u  hit %u  broken %u\r\n", o.second, m.name.empty() ? "-" : m.name.c_str(),
				PropKindName(m.kind), m.why.c_str(), m.mx[0] - m.mn[0], m.mx[1] - m.mn[1], m.mx[2] - m.mn[2], int(m.frag), m.seen, m.hits, m.breaks);
		}
		fclose(f);
		P.seenDirty = false;
	}

	// v2.51: knock a prop over in GTA (BeamNG's car broke it, or the drawn car touched a soft one): its physics woken up, pushed
	// along the car's way (a little up, high on it so it tips over)
	// v2.7: sparks from a metal post, a water jet from a hydrant (GTA's own particle effects)
	void PropFx(const PropLive& L, const PropModel& m)
	{
		PropState&   P = g_props;
		std::string  how = m.fx;
		std::string  nm = m.name;
		for (char& ch : nm) ch = char(tolower((unsigned char)ch));
		if (how.empty()) how = nm.find("hydrant") != std::string::npos ? "water" : m.kind == kPropBreak ? "sparks" : "none";
		if (how != "water" && how != "sparks") return;
		const std::string* fx = how == "water" ? P.fxWater : P.fxSparks;
		CallVoid(N::REQUEST_NAMED_PTFX_ASSET, fx[0].c_str());
		if (!Call<int>(N::HAS_NAMED_PTFX_ASSET_LOADED, fx[0].c_str())) {
			if (P.fxFail++ < 5) Log("props: particle asset '%s' not loaded yet (%s skipped)", fx[0].c_str(), how.c_str());
			return;
		}
		CallVoid(N::USE_PARTICLE_FX_ASSET, fx[0].c_str());
		const float z = float(L.c[2] - L.he[2] + (how == "water" ? 0.3 : 0.6));
		bool        ok = false;
		if (how == "water") {
			const int h = Call<int>(N::START_PARTICLE_FX_LOOPED_AT_COORD, fx[1].c_str(), float(L.c[0]), float(L.c[1]), z, 0.0f, 0.0f, 0.0f, 2.0f, 0, 0, 0, 0);
			if (h) P.fxLoops.push_back({ h, NowSeconds() + 12.0 });
			ok = h != 0;
		} else {
			ok = Call<int>(N::START_PARTICLE_FX_NON_LOOPED_AT_COORD, fx[1].c_str(), float(L.c[0]), float(L.c[1]), z, 0.0f, 0.0f, 0.0f, 1.5f, 0, 0, 0) != 0;
		}
		// (the effect names are a guess: the log says whether GTA started one; change them in GtaBeamD1_props.json)
		if (P.fxN++ < 20) Log("props: %s at the broken prop 0x%08X (%s/%s): %s", how.c_str(), L.model, fx[0].c_str(), fx[1].c_str(), ok ? "started" : "GTA didn't start it");
	}

	// v2.71: your v2.7 log: every knocked map prop "moved 0.00 m, upright 1.00" (GTA's map props don't move for a script). The
	// map's prop is hidden (CREATE_MODEL_HIDE_EXCLUDING_SCRIPT_OBJECTS, and made invisible without collision) and a copy the
	// plugin owns is put exactly where it stood; the copy moves (GTA's physics, or BeamNG's box for a physics prop). 0: the
	// model isn't loaded yet (asked for: try again next tick)
	int PropSwap(PropLive& L)
	{
		PropState& P = g_props;
		if (L.isCopy) return L.ent;
		if (L.copy) return L.copy;
		CallVoid(N::REQUEST_MODEL, L.model);
		if (!Call<int>(N::HAS_MODEL_LOADED, L.model)) {
			++P.copyWait;
			return 0;
		}
		const int c = Call<int>(N::CREATE_OBJECT, L.model, float(L.ps[0]), float(L.ps[1]), float(L.ps[2] + 0.05), 0, 1, 1);
		if (!c) {
			if (P.copyFail++ < 10) Log("props: GTA didn't make a copy of prop 0x%08X", L.model);
			return 0;
		}
		const double* R = L.ax[0];
		const double* F = L.ax[1];
		const double* U = L.ax[2];
		CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, c, float(L.ps[0]), float(L.ps[1]), float(L.ps[2]), 0, 0, 0);
		CallVoid(N::SET_ENTITY_ROTATION, c, float(asin(std::clamp(F[2], -1.0, 1.0)) / kDeg), float(atan2(-R[2], U[2]) / kDeg), float(atan2(-F[0], F[1]) / kDeg), 2, 1);
		CallVoid(N::CREATE_MODEL_HIDE_EXCLUDING_SCRIPT_OBJECTS, float(L.ps[0]), float(L.ps[1]), float(L.ps[2]), 0.5f, L.model, 1);
		CallVoid(N::SET_ENTITY_VISIBLE, L.ent, 0, 0);
		CallVoid(N::SET_ENTITY_COLLISION, L.ent, 0, 0);
		// v2.73: the map's own is never deleted or made the plugin's: v2.72 did (mission entity + DELETE_OBJECT) and GTA closed 2 s
		// later both times it took (your sessions 1 and 2). What stood there in your v2.71 test was the copy itself (it didn't move)
		CallVoid(N::SET_MODEL_AS_NO_LONGER_NEEDED, L.model);
		PropCopy pc;
		pc.orig = L.ent;
		pc.model = L.model;
		memcpy(pc.p, L.ps, sizeof(pc.p));
		pc.t = NowSeconds();
		P.copies[c] = pc;
		P.hidden.insert(L.ent);
		L.copy = c;
		++P.copyN;
		if (P.copyLogs++ < 30) Log("props: map prop 0x%08X swapped for the plugin's copy (it moves; the map's is hidden)", L.model);
		return c;
	}

	// the knock itself, on the copy (a script's object: GTA's physics moves it). v2.72: a speed and a turn, set again for up to 3
	// ticks until GTA moves it. A post, sign or lamp tips over the way the car went (turning about the axis up x the car's way); a bin
	// or a hydrant flies off with part of the car's speed. v2.73: your v2.72 log: posts, bins and hydrants "moved 0.00 m" (GTA's
	// physics don't move those models' copies); the small ones flew. When GTA hasn't moved it after 3 ticks the plugin moves it
	// itself (frozen, its pose set every tick): it slides the car's way and tips over its foot's front edge like a falling pole
	struct PropKnockGo
	{
		int    ent;
		float  v[3], w[3];
		int    ticks;
		double t0;
		bool   logged;
		int    fall;                                // v2.73: 0 GTA's physics, 1 the plugin moves it, 2 it lies still
		double piv[3], r0[3], A[3][3], dir[2];      // the front edge of its foot, its origin from there, its axes, the way it goes
		double th, om, thMax, slide, sv, h, tl;     // tipped (rad), turning (rad/s), how far it tips, slid (m), sliding (m/s), height
	};
	std::vector<PropKnockGo> g_propGo;
	void PropSetAxes(int e, const double* O, const double* R, const double* F, const double* U)
	{
		CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, e, float(O[0]), float(O[1]), float(O[2]), 0, 0, 0);
		CallVoid(N::SET_ENTITY_ROTATION, e, float(asin(std::clamp(F[2], -1.0, 1.0)) / kDeg), float(atan2(-R[2], U[2]) / kDeg), float(atan2(-F[0], F[1]) / kDeg), 2, 1);
	}
	void PropPush(int ent, const PropLive& L, const PropModel& m, const double V[3])
	{
		for (const PropKnockGo& g : g_propGo)
			if (g.ent == ent) return;  // (v2.73: hit twice: the first knock carries on)
		const double spd = sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
		double       vx = spd > 0.1 ? V[0] / spd : L.ax[1][0], vy = spd > 0.1 ? V[1] / spd : L.ax[1][1];
		const double hl = sqrt(vx * vx + vy * vy);
		if (hl < 1e-3) vx = 1, vy = 0;
		else vx /= hl, vy /= hl;
		const double k = g_cfg.propPush * (m.push >= 0 ? m.push : 1.0);
		PropKnockGo  g{};
		g.ent = ent;
		g.ticks = 3;
		g.t0 = NowSeconds();
		const bool tall = m.kind == kPropBreak && L.he[2] > 0.8;
		if (tall) {
			const double v = std::min(spd * 0.35, 6.0) * k;
			g.v[0] = float(vx * v), g.v[1] = float(vy * v), g.v[2] = 0.5f;
			g.w[0] = float(-vy * 2.5 * k), g.w[1] = float(vx * 2.5 * k), g.w[2] = 0;
			g.sv = v;
			g.om = 1.0 * k;
		} else {
			const double v = std::min(spd * 0.7, 12.0) * k;
			g.v[0] = float(vx * v), g.v[1] = float(vy * v), g.v[2] = float(std::min(3.0, 0.25 * v));
			g.w[0] = float(-vy * 3.0), g.w[1] = float(vx * 3.0), g.w[2] = 1.0f;
			g.sv = v;
			g.om = 3.0;
		}
		// the plugin's own fall (used when GTA doesn't move it): the copy stands where the map's stood (L's pose)
		g.dir[0] = vx, g.dir[1] = vy;
		const double* U = L.ax[2];
		double        reach = 0;  // its box's half size the car's way
		for (int i = 0; i < 2; ++i) reach += fabs(L.ax[i][0] * vx + L.ax[i][1] * vy) * L.he[i];
		for (int i = 0; i < 3; ++i) g.piv[i] = L.c[i] - U[i] * L.he[2] + (i < 2 ? g.dir[i] * reach : 0.0);
		for (int i = 0; i < 3; ++i) g.r0[i] = L.ps[i] - g.piv[i];
		memcpy(g.A, L.ax, sizeof(g.A));
		g.h = std::max(0.3, 2.0 * L.he[2]);
		g.thMax = U[2] > 0.7 ? 0.5 * kPi : 0.0;  // (already lying: it only slides)
		g_propGo.push_back(g);
	}
	// v2.73: one tick of the plugin's fall: a pole tipping over its foot's edge (turning faster as it goes: 3g/2h sin), sliding
	// the car's way and slowing (6 m/s2) on flat ground
	void PropFallTick(PropKnockGo& g, double now)
	{
		const double dt = std::clamp(now - g.tl, 0.0, 0.1);
		g.tl = now;
		if (g.th < g.thMax) {
			g.om += 1.5 * 9.81 / g.h * sin(std::max(g.th, 0.05)) * dt;
			g.th = std::min(g.thMax, g.th + g.om * dt);
		}
		g.slide += g.sv * dt;
		g.sv = std::max(0.0, g.sv - 6.0 * dt);
		// turned by th about up x the way it goes (Rodrigues)
		const double kx = -g.dir[1], ky = g.dir[0], c = cos(g.th), sn = sin(g.th);
		auto         rot = [&](const double* x, double* o) {
            const double kd = kx * x[0] + ky * x[1];
            const double cr[3] = { ky * x[2], -kx * x[2], kx * x[1] - ky * x[0] };
            o[0] = x[0] * c + cr[0] * sn + kx * kd * (1 - c);
            o[1] = x[1] * c + cr[1] * sn + ky * kd * (1 - c);
            o[2] = x[2] * c + cr[2] * sn;
		};
		double O[3], R[3], F[3], U[3];
		rot(g.r0, O);
		for (int i = 0; i < 3; ++i) O[i] += g.piv[i] + (i < 2 ? g.dir[i] * g.slide : 0.0);
		rot(g.A[0], R);
		rot(g.A[1], F);
		rot(g.A[2], U);
		PropSetAxes(g.ent, O, R, F, U);
		if (g.th >= g.thMax && g.sv <= 0) g.fall = 2;
	}
	void PropGoTick()
	{
		PropState&   P = g_props;
		const double now = NowSeconds();
		for (size_t i = 0; i < g_propGo.size();) {
			PropKnockGo& g = g_propGo[i];
			const bool   ex = Call<int>(N::DOES_ENTITY_EXIST, g.ent) != 0;
			if (ex && g.fall == 1) PropFallTick(g, now);
			else if (ex && g.fall == 0 && g.ticks > 0) {
				const NVec3 v = Call<NVec3>(N::GET_ENTITY_VELOCITY, g.ent);
				if (sqrt(v.x * v.x + v.y * v.y + v.z * v.z) < 0.5) {
					CallVoid(N::FREEZE_ENTITY_POSITION, g.ent, 0);
					CallVoid(N::SET_ENTITY_DYNAMIC, g.ent, 1);
					CallVoid(N::ACTIVATE_PHYSICS, g.ent);
					CallVoid(N::SET_ENTITY_VELOCITY, g.ent, g.v[0], g.v[1], g.v[2]);
					CallVoid(N::SET_ENTITY_ANGULAR_VELOCITY, g.ent, g.w[0], g.w[1], g.w[2]);
					if (--g.ticks == 0) {
						// GTA hasn't moved it: the plugin does (frozen: GTA's physics don't fight it)
						CallVoid(N::FREEZE_ENTITY_POSITION, g.ent, 1);
						g.fall = 1;
						g.tl = now;
						++P.fallN;
					}
				} else
					g.ticks = 0;  // (it moves: GTA's physics carry on)
			}
			// a second later: what happened to it, and whether the map's own is still seen there
			if (!g.logged && now - g.t0 > 1.0) {
				g.logged = true;
				auto ci = P.copies.find(g.ent);
				if (ex && ci != P.copies.end() && P.movedLogs++ < 20) {
					const NVec3 p = Call<NVec3>(N::GET_ENTITY_COORDS, g.ent, 0);
					const int   orig = Call<int>(N::GET_CLOSEST_OBJECT_OF_TYPE, float(ci->second.p[0]), float(ci->second.p[1]), float(ci->second.p[2]), 0.6f, ci->second.model, 0, 0, 0);
					Log("props: the knocked prop 0x%08X 1 s later: its copy moved %.2f m (%s), upright %.2f, visible %d; the map's own there: %s", ci->second.model,
						sqrt((p.x - ci->second.p[0]) * (p.x - ci->second.p[0]) + (p.y - ci->second.p[1]) * (p.y - ci->second.p[1])),
						g.fall ? "the plugin moved it: GTA's physics didn't" : "GTA's physics", Call<float>(N::GET_ENTITY_UPRIGHT_VALUE, g.ent), Call<int>(N::IS_ENTITY_VISIBLE, g.ent),
						!orig || orig == g.ent ? "not found" : Call<int>(N::IS_ENTITY_VISIBLE, orig) ? "still there, VISIBLE (the hide didn't take)" : "still there, hidden");
				}
			}
			if (g.logged && (g.fall == 2 || (g.fall == 0 && g.ticks <= 0) || !ex)) g_propGo.erase(g_propGo.begin() + i);
			else ++i;
		}
	}

	// a knock waiting for its copy's model (map props' models are normally loaded: at once)
	struct PropKnockWait
	{
		int    ent;
		double V[3], t;
	};
	std::vector<PropKnockWait> g_propWait;

	void PropKnock(PropLive& L, const PropModel& m, const double V[3])
	{
		PropFx(L, m);
		const int c = PropSwap(L);
		if (c) {
			PropPush(c, L, m, V);
			return;
		}
		g_propWait.push_back({ L.ent, { V[0], V[1], V[2] }, NowSeconds() });
	}

	// v2.71: the copies: knocks that waited for a model; far from the player (150 m): the copy goes, the map's prop comes back
	void PropCopiesTick()
	{
		PropState&   P = g_props;
		const double now = NowSeconds();
		for (size_t i = 0; i < g_propWait.size();) {
			PropKnockWait& w = g_propWait[i];
			auto           it = P.live.find(w.ent);
			auto           mi = it == P.live.end() ? P.models.end() : P.models.find(it->second.model);
			int            c = 0;
			if (mi != P.models.end()) {
				c = PropSwap(it->second);
				if (c) PropPush(c, it->second, mi->second, w.V);
			}
			if (c || mi == P.models.end() || now - w.t > 1.5) {
				if (!c && P.copyFail++ < 10) Log("props: no copy of a knocked prop within 1.5 s (its model didn't load): it stays");
				g_propWait.erase(g_propWait.begin() + i);
			} else
				++i;
		}
		if (P.copies.empty() || now - P.copyT < 1.0) return;
		P.copyT = now;
		const NVec3 me = Call<NVec3>(N::GET_ENTITY_COORDS, Call<int>(N::PLAYER_PED_ID), 0);
		double      oldT = 1e30;
		int         oldC = 0;
		for (auto& kv : P.copies) {
			if (kv.second.t < oldT) oldT = kv.second.t, oldC = kv.first;
		}
		for (auto it = P.copies.begin(); it != P.copies.end();) {
			const PropCopy& pc = it->second;
			const double    dx = pc.p[0] - me.x, dy = pc.p[1] - me.y;
			const bool      over = P.copies.size() > 60 && it->first == oldC;
			if (dx * dx + dy * dy < 150.0 * 150.0 && !over) {
				++it;
				continue;
			}
			int h = it->first;
			if (Call<int>(N::DOES_ENTITY_EXIST, h)) CallVoid(N::DELETE_OBJECT, &h);
			CallVoid(N::REMOVE_MODEL_HIDE, float(pc.p[0]), float(pc.p[1]), float(pc.p[2]), 0.5f, pc.model, 0);
			P.hidden.erase(pc.orig);
			P.knockedEnts.erase(pc.orig);
			++P.copyGone;
			it = P.copies.erase(it);
		}
	}

	// ---------------------------------------------------------------------------------------
	// v2.52: physics props (your choice): the BeamNG mod spawns a weighted box per prop (its own jbeam "vehicle"): posts tied to
	// the ground by beams that snap, bins loose. GTA's prop copies the box ("pp": how far its middle moved and how it turned
	// since it went in). The box: what a car hits of it (PropParts), up to 6 m tall. The mass: the json's, else by its size.
	// ---------------------------------------------------------------------------------------
	struct PropPhysBox
	{
		double x0, x1, y0, y1, h, mass, brkN;
		bool   anchor;
	};
	bool PropPhysShape(PropModel& m, PropLive& L, bool canMeasure, PropPhysBox& b)
	{
		if (!PropParts(m, L, canMeasure) || m.nb <= 0) return false;
		b.x0 = b.y0 = 1e9;
		b.x1 = b.y1 = -1e9;
		for (int q = 0; q < m.nb; ++q) {
			b.x0 = std::min(b.x0, double(m.tb[q][0]));
			b.x1 = std::max(b.x1, double(m.tb[q][1]));
			b.y0 = std::min(b.y0, double(m.tb[q][2]));
			b.y1 = std::max(b.y1, double(m.tb[q][3]));
		}
		const double sx = b.x1 - b.x0, sy = b.y1 - b.y0, sz = m.mx[2] - m.mn[2];
		if (sx > 3.0 || sy > 3.0) return false;  // (parts far apart: a gantry: a fixed box)
		// at least 12 cm a side (a thinner box is not steady in BeamNG)
		if (sx < 0.12) b.x0 -= (0.12 - sx) * 0.5, b.x1 += (0.12 - sx) * 0.5;
		if (sy < 0.12) b.y0 -= (0.12 - sy) * 0.5, b.y1 += (0.12 - sy) * 0.5;
		b.h = std::clamp(double(sz), 0.12, 6.0);
		const double thick = std::max(b.x1 - b.x0, b.y1 - b.y0);
		b.anchor = m.kind == kPropBreak && (sz >= 1.8 ? thick <= 0.8 : m.listed) && !g_props.knockedEnts.count(L.ent);
		if (m.mass > 0) b.mass = m.mass;
		else if (b.anchor) b.mass = std::clamp(130.0 * thick, 6.0, 60.0) * std::min(double(sz), 12.0);  // a pole: kg per metre by its thickness
		else b.mass = std::clamp(double(m.mx[0] - m.mn[0]) * (m.mx[1] - m.mn[1]) * sz * (m.frag ? 50.0 : 120.0), 4.0, 2000.0);  // hollow bins, solid stuff
		b.mass = std::clamp(b.mass, 4.0, 5000.0);
		// the ground beams' strength: ~15 kN per m/s the car must lose (PropBreak scales it)
		b.brkN = b.anchor ? std::max(2000.0, 15000.0 * PropBreakDv(m)) : 1.0;
		return true;
	}
	// one "pq" entry: id, anchored, mass, strength, bottom middle (BeamNG), rotation (x y z w), size
	void PropPhysStart(std::string& out, PropLive& L, const PropModel& m, const PropPhysBox& b)
	{
		const double cx = (b.x0 + b.x1) * 0.5, cy = (b.y0 + b.y1) * 0.5;
		double       g[3], bb[3], q[4];
		PropWorld(L, cx, cy, m.mn[2], g);
		GtaToBeam(g, bb);
		BasisQuat(L.ax[0], L.ax[1], L.ax[2], q);
		char t[320];
		snprintf(t, sizeof(t), "%s%d,%d,%.1f,%.0f,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f", out.empty() ? "" : ",", L.ent, b.anchor ? 1 : 0, b.mass, b.brkN, bb[0],
			bb[1], bb[2], q[0], q[1], q[2], q[3], b.x1 - b.x0, b.y1 - b.y0, b.h);
		out += t;
		L.phys = true;
		L.anchored = b.anchor;
		memcpy(L.P0, L.ps, sizeof(L.P0));
		memcpy(L.A0, L.ax, sizeof(L.A0));
		// the box's middle, as BeamNG measures it once it settled (a tied one stays 5 cm up)
		PropWorld(L, cx, cy, m.mn[2] + b.h * 0.5, L.Cg0);
		if (b.anchor) L.Cg0[2] += 0.05;
		++g_props.physSent;
	}
	// BeamNG's answers: poses ("pp"), knocked ("pk"), one failed ("pqf"), they work or not ("pqs")
	void PropPhysMsgs()
	{
		PropState&               P = g_props;
		std::vector<std::string> q;
		AcquireSRWLockExclusive(&g_dnet.lock);
		q.swap(g_dnet.ppQ);
		ReleaseSRWLockExclusive(&g_dnet.lock);
		for (const std::string& s : q) {
			std::string ty;
			JsonStr(s, "t", ty);
			if (ty == "pqs") {
				const int ok = int(JsonNum(s, "ok", 0));
				if (ok != P.physOk) {
					std::string why;
					JsonStr(s, "why", why);
					if (ok) Log("props: BeamNG spawns physics props (GTA's props copy them)");
					else {
						Log("props: BeamNG can't spawn physics props (%s): fixed boxes instead", why.c_str());
						Notify("Props: BeamNG physics props didn't work - fixed boxes instead (see the BeamNG log)");
						for (auto& kv : P.live) kv.second.phys = false;
					}
				}
				P.physOk = ok;
				continue;
			}
			if (ty == "pqf") {
				auto it = P.live.find(int(JsonNum(s, "id", 0)));
				if (it != P.live.end()) {
					PropLive& L = it->second;
					L.phys = false;
					L.physFailed = true;
					if (L.frozen) {
						// (it fell through a gap in GTA's ground in BeamNG: GTA's prop back where it stood)
						const double* R = L.A0[0];
						const double* F = L.A0[1];
						const double* U = L.A0[2];
						const int e = L.copy ? L.copy : L.ent;  // (v2.71: the copy)
						CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, e, float(L.P0[0]), float(L.P0[1]), float(L.P0[2]), 0, 0, 0);
						CallVoid(N::SET_ENTITY_ROTATION, e, float(asin(std::clamp(F[2], -1.0, 1.0)) / kDeg), float(atan2(-R[2], U[2]) / kDeg), float(atan2(-F[0], F[1]) / kDeg), 2, 1);
					}
					std::string why;
					JsonStr(s, "why", why);
					if (P.physLogs++ < 40) Log("props: BeamNG's physics prop for 0x%08X failed (%s)%s", L.model, why.c_str(), L.frozen ? ": GTA's prop put back" : "");
				}
				++P.physFails;
				continue;
			}
			if (ty == "pk") {
				auto it = P.live.find(int(JsonNum(s, "id", 0)));
				if (it == P.live.end()) continue;
				PropLive& L = it->second;
				if (L.knocked) continue;
				L.knocked = true;
				P.knockedEnts.insert(L.ent);
				++P.physKnocked;
				++P.breaks;
				auto mi = P.models.find(L.model);
				if (mi != P.models.end()) {
					++mi->second.breaks;
					P.seenDirty = true;
					if (P.physLogs++ < 40)
						Log("props: BeamNG's car knocked a %s prop (model 0x%08X %s, %s) at %.0f km/h: GTA's copies its box", L.anchored ? "tied" : "loose", L.model,
							mi->second.name.empty() ? "" : mi->second.name.c_str(), mi->second.why.c_str(), JsonNum(s, "v", 0) * 3.6);
				}
				continue;
			}
			// "pp": id, how far its middle moved (x y z), how it turned (x y z w), per prop
			const size_t a = s.find('[');
			if (a == std::string::npos) continue;
			const char* p = s.c_str() + a + 1;
			while (*p && *p != ']') {
				double v[8];
				int    k = 0;
				for (; k < 8; ++k) {
					char* e = nullptr;
					v[k] = strtod(p, &e);
					if (e == p) break;
					p = e;
					while (*p == ',' || *p == ' ') ++p;
				}
				if (k < 8) break;
				auto it = P.live.find(int(v[0]));
				if (it == P.live.end() || !it->second.phys) continue;
				PropLive& L = it->second;
				const double qq[4] = { v[4], v[5], v[6], v[7] };
				auto rot = [&](const double* x, double* o) {  // the turn qq applied to x
					const double t[3] = { 2 * (qq[1] * x[2] - qq[2] * x[1]), 2 * (qq[2] * x[0] - qq[0] * x[2]), 2 * (qq[0] * x[1] - qq[1] * x[0]) };
					o[0] = x[0] + qq[3] * t[0] + (qq[1] * t[2] - qq[2] * t[1]);
					o[1] = x[1] + qq[3] * t[1] + (qq[2] * t[0] - qq[0] * t[2]);
					o[2] = x[2] + qq[3] * t[2] + (qq[0] * t[1] - qq[1] * t[0]);
				};
				// its origin goes where the box's move and turn take it; its axes turn with it
				const double r0[3] = { L.P0[0] - L.Cg0[0], L.P0[1] - L.Cg0[1], L.P0[2] - L.Cg0[2] };
				double       r1[3], R[3], F[3], U[3];
				rot(r0, r1);
				rot(L.A0[0], R);
				rot(L.A0[1], F);
				rot(L.A0[2], U);
				const double O[3] = { L.Cg0[0] + v[1] + r1[0], L.Cg0[1] + v[2] + r1[1], L.Cg0[2] + v[3] + r1[2] };
				const int    e = PropSwap(L);  // v2.71: the map's prop doesn't move: its copy does
				if (!e) continue;
				if (!L.frozen) {
					CallVoid(N::FREEZE_ENTITY_POSITION, e, 1);  // (GTA's own physics off it: BeamNG moves it now)
					L.frozen = true;
				}
				CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, e, float(O[0]), float(O[1]), float(O[2]), 0, 0, 0);
				// GTA rotation order 2 (as for the drawn car: DrvSetPuppet)
				const double pitch = asin(std::clamp(F[2], -1.0, 1.0)) / kDeg, yaw = atan2(-F[0], F[1]) / kDeg, roll = atan2(-R[2], U[2]) / kDeg;
				CallVoid(N::SET_ENTITY_ROTATION, e, float(pitch), float(roll), float(yaw), 2, 1);
				++P.physPoses;
			}
		}
	}

	// every tick (drive mode): the pool scanned 4 times a second; hits with the drawn car every tick
	void PropsTick(bool paused)
	{
		PropState&  P = g_props;
		DriveState& d = g_drv;
		const ULONGLONG ms = GetTickCount64();
		PropCopiesTick();  // v2.71 (also out of drive mode: far copies go, the map's props come back)
		PropGoTick();      // v2.72
		if (!g_cfg.props || d.state != 2 || !d.fixedMap || !d.carPoseOk) {
			if (!P.live.empty()) PropsRemoveAll(d.state != 2 ? "drive mode off" : "props off");
			AcquireSRWLockExclusive(&g_dnet.lock);
			g_dnet.pbkQ.clear();
			g_dnet.ppQ.clear();
			ReleaseSRWLockExclusive(&g_dnet.lock);
			return;
		}
		if (!P.cfgLoaded) LoadProps();
		for (auto it = P.fxLoops.begin(); it != P.fxLoops.end();) {  // v2.7: water jets end
			if (NowSeconds() < it->second) { ++it; continue; }
			CallVoid(N::STOP_PARTICLE_FX_LOOPED, it->first, 0);
			it = P.fxLoops.erase(it);
		}
		PropPhysMsgs();  // v2.52
		const double now = NowSeconds();
		const double w = std::max(1.0f, g_modelSize[0].load()), l = std::max(2.0f, g_modelSize[1].load()), h = std::max(0.8f, g_modelSize[2].load());
		// ---- v2.51: BeamNG's car broke props' boxes: over they go in GTA
		{
			std::vector<std::string> q;
			AcquireSRWLockExclusive(&g_dnet.lock);
			q.swap(g_dnet.pbkQ);
			ReleaseSRWLockExclusive(&g_dnet.lock);
			for (const std::string& s : q) {
				const int    id = int(JsonNum(s, "id", 0));
				const double V[3] = { JsonNum(s, "vx", 0), JsonNum(s, "vy", 0), JsonNum(s, "vz", 0) }, dv = JsonNum(s, "dv", 0);
				auto         it = P.live.find(id);
				if (it == P.live.end()) continue;
				PropLive& L = it->second;
				auto      mi = P.models.find(L.model);
				if (mi == P.models.end() || L.broken) continue;
				PropModel& m = mi->second;
				L.broken = true;
				L.sent = false;  // (BeamNG took its box out)
				L.brokenT = now;
				memcpy(L.brokenC, L.c, sizeof(L.brokenC));
				L.checked = false;
				++m.breaks;
				++P.breaks;
				P.seenDirty = true;
				PropKnock(L, m, V);
				if (m.breaks + P.breaks <= 60)
					Log("props: BeamNG's car broke a %s prop (model 0x%08X %s, %s) at %.0f km/h, losing %.1f km/h against it: knocked over in GTA", PropKindName(m.kind), L.model,
						m.name.empty() ? "" : m.name.c_str(), m.why.c_str(), sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]) * 3.6, dv * 3.6);
			}
		}
		// ---- the pool, 4 times a second
		if (now - P.scanT >= 0.25) {
			P.scanT = now;
			const double t0 = NowSeconds();
			++P.scan;
			++P.scans;
			if (P.buf.empty()) P.buf.resize(4096);
			const int n = std::clamp(worldGetAllObjects(P.buf.data(), int(P.buf.size())), 0, int(P.buf.size()));
			P.pool = uint32_t(n);
			const double R = g_cfg.propRangeM;
			std::string  add, ids;
			int          nAdd = 0, nRm = 0;
			const double measEnd = t0 + 0.002;  // models measured within 2 ms a scan (at least one)
			const uint32_t meas0 = P.measured;
			std::vector<std::pair<double, int>> cands;  // v2.52: physics prop candidates (distance squared, entity)
			for (int q = 0; q < n; ++q) {
				const int e = P.buf[q];
				if (!e || P.hidden.count(e) || !Call<int>(N::DOES_ENTITY_EXIST, e)) continue;  // (v2.71: swapped for a copy)
				const NVec3 p = Call<NVec3>(N::GET_ENTITY_COORDS, e, 0);
				const double dx = p.x - d.carO[0], dy = p.y - d.carO[1];
				if (dx * dx + dy * dy > R * R || fabs(p.z - d.carO[2]) > 30.0) continue;
				const uint32_t model = uint32_t(Call<int>(N::GET_ENTITY_MODEL, e));
				PropModel&     m = PropModelOf(model, e);
				if (m.kind == kPropIgnore) continue;
				if (Call<int>(N::IS_ENTITY_ATTACHED, e) || !Call<int>(N::IS_ENTITY_VISIBLE, e)) continue;
				NVec3 fw{}, rt{}, up{}, ps{};
				CallVoid(N::GET_ENTITY_MATRIX, e, &fw, &rt, &up, &ps);
				PropLive& L = P.live[e];
				if (L.scan == 0) ++m.seen, P.seenDirty = true;
				L.ent = e;
				L.isCopy = P.copies.count(e) != 0;  // v2.71: the plugin's copy: GTA's (no box in BeamNG; a soft one can be kicked again)
				L.model = model;
				L.scan = P.scan;
				const double ax[3][3] = { { rt.x, rt.y, rt.z }, { fw.x, fw.y, fw.z }, { up.x, up.y, up.z } };
				memcpy(L.ax, ax, sizeof(ax));
				L.ps[0] = ps.x;
				L.ps[1] = ps.y;
				L.ps[2] = ps.z;
				// the box: the model's size, centred where it is (its corners in the model's frame, turned by the matrix)
				const double mc[3] = { (m.mn[0] + m.mx[0]) * 0.5, (m.mn[1] + m.mx[1]) * 0.5, (m.mn[2] + m.mx[2]) * 0.5 };
				for (int k = 0; k < 3; ++k) {
					L.c[k] = (k == 0 ? ps.x : k == 1 ? ps.y : ps.z) + ax[0][k] * mc[0] + ax[1][k] * mc[1] + ax[2][k] * mc[2];
					L.he[k] = (m.mx[k] - m.mn[k]) * 0.5;
				}
				// v2.51: knocked over: did it really go in GTA (the log says, for the next fix)
				if (L.broken && !L.checked && now - L.brokenT > 1.0) {
					L.checked = true;
					if (P.movedLogs++ < 12) {
						const double mv = sqrt((L.c[0] - L.brokenC[0]) * (L.c[0] - L.brokenC[0]) + (L.c[1] - L.brokenC[1]) * (L.c[1] - L.brokenC[1]) +
							(L.c[2] - L.brokenC[2]) * (L.c[2] - L.brokenC[2]));
						Log("props: the broken prop 0x%08X 1 s later in GTA: moved %.2f m, upright %.2f, broken %d", L.model, mv, Call<float>(N::GET_ENTITY_UPRIGHT_VALUE, e),
							Call<int>(N::HAS_OBJECT_BEEN_BROKEN, e, 0));
					}
				}
				if (L.phys || L.isCopy) continue;  // v2.52: BeamNG simulates it (its pose comes back as "pp")
				{
					// v2.52: posts, signs, bins: a physics prop when near enough (below); litter isn't
					const double vol = 8.0 * L.he[0] * L.he[1] * L.he[2];
					const double dx = L.c[0] - d.carO[0], dy = L.c[1] - d.carO[1], vl = sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1]);
					const bool   behind = vl > 3.0 && (dx * d.carV[0] + dy * d.carV[1]) / vl < -8.0;
					if (g_cfg.propPhysics && P.physOk != 0 && !L.physFailed && !L.broken && up.z >= 0.85f && !behind &&
					    (m.kind == kPropBreak || (m.kind == kPropSoft && vol >= 0.02 && std::max({ L.he[0], L.he[1], L.he[2] }) >= 0.15)))
						cands.push_back({ dx * dx + dy * dy, e });
				}
				if ((m.kind == kPropSolid && g_cfg.propSolid) || m.kind == kPropBreak) {
					// fallen over or broken: not a box any more
					const bool down = up.z < 0.85f || L.broken;
					if (down) {
						if (L.sent) {
							ids += (nRm ? "," : "") + std::to_string(e);
							++nRm;
							++P.rmN;
							L.sent = false;
						}
						continue;
					}
					if (!PropParts(m, L, P.measured == meas0 || NowSeconds() < measEnd)) continue;
					const double yaw = atan2(fw.y, fw.x);
					const double mv = sqrt((L.c[0] - L.sentC[0]) * (L.c[0] - L.sentC[0]) + (L.c[1] - L.sentC[1]) * (L.c[1] - L.sentC[1]) + (L.c[2] - L.sentC[2]) * (L.c[2] - L.sentC[2]));
					double       dyaw = fabs(yaw - L.sentYaw);
					dyaw = std::min(dyaw, 2 * kPi - dyaw);
					if (!L.sent || mv > 0.2 || dyaw > 0.09) {
						PropBoxMsg(add, L, m);
						L.sent = true;
						memcpy(L.sentC, L.c, sizeof(L.sentC));
						L.sentYaw = yaw;
						++nAdd;
						++P.sentN;
						if (nAdd >= 20) {
							DrvSend(("{\"t\":\"pb\",\"p\":[" + add + "]}").c_str());
							add.clear();
							nAdd = 0;
						}
					}
				}
			}
			if (nAdd) DrvSend(("{\"t\":\"pb\",\"p\":[" + add + "]}").c_str());
			// v2.52: physics props: the nearest candidates go in (PropPhysicsMax at once), far ones come out (GTA's prop stays as it is)
			if (g_cfg.propPhysics && P.physOk != 0) {
				const double spd = sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1] + d.carV[2] * d.carV[2]);
				const double Rp = std::clamp(10.0 + 2.0 * spd, 15.0, 45.0);
				int          liveN = 0;
				double       farD2 = -1;
				int          farId = 0;
				const double vl = std::max(1e-6, sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1]));
				for (auto& kv : P.live) {
					PropLive& L = kv.second;
					if (!L.phys) continue;
					const double dx = L.c[0] - d.carO[0], dy = L.c[1] - d.carO[1], d2 = dx * dx + dy * dy;
					const bool   behind = spd > 3.0 && (dx * d.carV[0] + dy * d.carV[1]) / vl < -8.0;  // (passed: its slot goes to one ahead)
					if (d2 > (Rp + 10.0) * (Rp + 10.0) || behind) {
						ids += (nRm ? "," : "") + std::to_string(kv.first);
						++nRm;
						L.phys = false;
					} else {
						++liveN;
						if (d2 > farD2) farD2 = d2, farId = kv.first;
					}
				}
				std::sort(cands.begin(), cands.end());
				// all slots taken and one waits much nearer than the farthest in BeamNG: that one comes out
				if (liveN >= g_cfg.propPhysicsMax && !cands.empty() && cands[0].first < Rp * Rp && cands[0].first < farD2 * 0.4) {
					ids += (nRm ? "," : "") + std::to_string(farId);
					++nRm;
					P.live[farId].phys = false;
					--liveN;
				}
				std::string pq;
				for (const auto& c : cands) {
					if (liveN >= g_cfg.propPhysicsMax || c.first > Rp * Rp) break;
					auto li = P.live.find(c.second);
					if (li == P.live.end()) continue;
					PropLive& L = li->second;
					PropModel& m = P.models[L.model];
					PropPhysBox b{};
					if (!PropPhysShape(m, L, P.measured == meas0 || NowSeconds() < measEnd, b)) continue;
					if (L.sent) {  // (its fixed box out first)
						ids += (nRm ? "," : "") + std::to_string(L.ent);
						++nRm;
						++P.rmN;
						L.sent = false;
					}
					PropPhysStart(pq, L, m, b);
					++liveN;
				}
				if (!pq.empty()) {
					if (nRm) {
						DrvSend(("{\"t\":\"prm\",\"ids\":[" + ids + "]}").c_str());
						ids.clear();
						nRm = 0;
					}
					DrvSend(("{\"t\":\"pq\",\"p\":[" + pq + "]}").c_str());
				}
			}
			// gone or out of range: forgotten (their boxes out of BeamNG)
			for (auto it = P.live.begin(); it != P.live.end();) {
				if (it->second.scan == P.scan || (it->second.phys && it->second.copy)) {  // (v2.71: hidden, its copy follows BeamNG's box)
					++it;
					continue;
				}
				if (it->second.sent || it->second.phys) {
					ids += (nRm ? "," : "") + std::to_string(it->first);
					++nRm;
					++P.rmN;
				}
				it = P.live.erase(it);
			}
			if (nRm) DrvSend(("{\"t\":\"prm\",\"ids\":[" + ids + "]}").c_str());
			P.nearN = uint32_t(P.live.size());
			const double us = (NowSeconds() - t0) * 1e6;
			P.scanUs += us;
			P.scanUsMax = std::max(P.scanUsMax, us);
		}
		// ---- hits: the drawn car against the soft props near it (breakaway and solid ones are BeamNG's: real boxes there)
		const double spd = sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1] + d.carV[2] * d.carV[2]);
		if (!paused && spd > 1.5) {
			for (auto& kv : P.live) {
				PropLive& L = kv.second;
				auto      mi = P.models.find(L.model);
				if (mi == P.models.end()) continue;  // (not classified yet: GtaBeamD1_props.json was just read again)
				PropModel& m = mi->second;
				if (m.kind != kPropSoft || L.broken || L.phys) continue;  // (v2.52: a physics prop is BeamNG's)
				if (L.isCopy) {  // v2.71: the copy of a physics prop BeamNG still moves: BeamNG's too
					auto ci = P.copies.find(L.ent);
					auto oi = ci == P.copies.end() ? P.live.end() : P.live.find(ci->second.orig);
					if (oi != P.live.end() && oi->second.phys) continue;
				}
				if (now - L.lastHit < 2.0) continue;
				const double dx = L.c[0] - d.carO[0], dy = L.c[1] - d.carO[1];
				if (dx * dx + dy * dy > (l + 4.0) * (l + 4.0)) continue;
				if (!PropTouch(d, L, w, l, h)) continue;
				L.lastHit = now;
				++m.hits;
				++P.hits;
				P.seenDirty = true;
				PropKnock(L, m, d.carV);
				// BeamNG: a little speed lost (a bump), by its size unless the json says; v2.51: litter (under 0.05 m3) costs nothing
				const double vol = 8.0 * L.he[0] * L.he[1] * L.he[2];
				double       dv = (m.bump >= 0 ? m.bump : (vol < 0.05 ? 0.0 : std::clamp(0.3 + vol * 1.2, 0.3, 3.0))) * g_cfg.propBump;
				dv = std::min(dv, spd * 0.5);
				if (dv > 0.01) {
					char b[96];
					snprintf(b, sizeof(b), "{\"t\":\"bump\",\"dv\":%.2f,\"m\":\"%08X\"}", dv, L.model);
					DrvSend(b);
					++P.bumps;
					P.bumpSum += dv;
				}
				if (P.hits <= 30)
					Log("props: BeamNG's car hit a %s prop (model 0x%08X %s, %.1f x %.1f x %.1f m, %s) at %.0f km/h: pushed in GTA, BeamNG's car slowed by %.1f km/h", PropKindName(m.kind),
						L.model, m.name.empty() ? "" : m.name.c_str(), 2 * L.he[0], 2 * L.he[1], 2 * L.he[2], m.why.c_str(), spd * 3.6, dv * 3.6);
			}
		}
		if (ms - P.logMs >= 10000) {
			P.logMs = ms;
			Log("props: %u in GTA's pool, %u near the car, %u soft hits (%u bumps, %.1f km/h lost in all), %u broken by BeamNG's car, %u prop boxes sent to BeamNG (%u taken out); "
			    "%u models measured (%u rays, %.0f us); pool scans %u (%.0f us each, max %.0f)",
				P.pool, P.nearN, P.hits, P.bumps, P.bumpSum * 3.6, P.breaks, P.sentN, P.rmN, P.measured, P.measRays, P.measUs, P.scans, P.scans ? P.scanUs / P.scans : 0.0,
				P.scanUsMax);
			if (g_cfg.propPhysics && (P.physSent || P.physPoses || P.physFails || P.physOk == 0)) {
				int liveN = 0;
				for (auto& kv : P.live) liveN += kv.second.phys;
				Log("props: physics (v2.52): %s; %d in BeamNG now, %u sent, %u failed, %u knocked, %u pose updates applied (10 s)",
					P.physOk == 1 ? "BeamNG spawns them" : P.physOk == 0 ? "BeamNG can't (fixed boxes instead)" : "not confirmed yet", liveN, P.physSent, P.physFails, P.physKnocked,
					P.physPoses);
			}
			if (P.copyN || P.copyGone || P.copyFail)
				Log("props: v2.71: %u map props swapped for the plugin's copies (%u ticks waited for a model, %u failed), %u copies out (far away: the map's props back), %zu now; v2.73: %u knocked over by the plugin (GTA's physics didn't move them) (10 s)",
					P.copyN, P.copyWait, P.copyFail, P.copyGone, P.copies.size(), P.fallN);
			P.copyN = P.copyWait = P.copyGone = P.fallN = 0;
			P.physSent = P.physPoses = P.physFails = P.physKnocked = 0;
			P.hits = P.bumps = P.sentN = P.rmN = P.scans = P.breaks = P.measured = P.measRays = 0;
			P.bumpSum = P.scanUs = P.scanUsMax = P.measUs = 0;
		}
		if (P.seenDirty && ms - P.seenMs >= 30000) {
			P.seenMs = ms;
			PropsSeenWrite();
		}
	}

	void SoundWake(const char* when);  // v2.3 (below)

	// v2.41: the radio station GTA plays now ("OFF" when off)
	std::string RadioNow()
	{
		const int   idx = Call<int>(N::GET_PLAYER_RADIO_STATION_INDEX);
		const char* nm = Call<const char*>(N::GET_PLAYER_RADIO_STATION_NAME);
		if (idx == 255 || !nm || !*nm || !_stricmp(nm, "OFF")) return "OFF";
		return nm;
	}

	// v2.3: GTA's radio as the phone radio (the hidden car's engine is off in drive mode)
	void PhoneRadio(bool on)
	{
		if (on) {
			CallVoid(N::SET_AUDIO_FLAG, "MobileRadioInGame", 1);
			CallVoid(N::SET_MOBILE_RADIO_ENABLED_DURING_GAMEPLAY, 1);
			CallVoid(N::SET_MOBILE_PHONE_RADIO_STATE, 1);
		} else {
			CallVoid(N::SET_MOBILE_PHONE_RADIO_STATE, 0);
			CallVoid(N::SET_MOBILE_RADIO_ENABLED_DURING_GAMEPLAY, 0);
			CallVoid(N::SET_AUDIO_FLAG, "MobileRadioInGame", 0);
		}
		g_drv.radioOn = on;
	}

	// v2.41: after F10 off the car's own radio is kept on what played in drive mode for a few seconds (GTA's car radio
	// comes back with its engine on the station it had before)
	struct RadioAfter
	{
		int         veh = 0;
		std::string want;
		double      until = 0;
		uint32_t    chk = 0, fixes = 0;
	} g_radioAfter;

	void RadioAfterTick()
	{
		RadioAfter& r = g_radioAfter;
		if (!r.veh || g_drv.state != 0) return;
		if (NowSeconds() > r.until || !Call<int>(N::DOES_ENTITY_EXIST, r.veh)) {
			if (r.fixes) Log("drive: GTA's car radio set back to %s %u time(s) after F10 off", r.want.c_str(), r.fixes);
			r.veh = 0;
			return;
		}
		if (++r.chk % 5) return;
		if (Call<int>(N::GET_PED_IN_VEHICLE_SEAT, r.veh, -1, 0) != Call<int>(N::PLAYER_PED_ID)) return;
		if (_stricmp(RadioNow().c_str(), r.want.c_str())) {
			CallVoid(N::SET_VEH_RADIO_STATION, r.veh, r.want.c_str());
			++r.fixes;
		}
	}

	// v2.41: F10 off / drive end: GTA's car back on GTA's ground. Your v2.4 test: after an alt-tab the drive ended and
	// GTA's car fell through the map (collision and gravity came back where GTA had no ground loaded, or the hidden car had
	// followed BeamNG's car through a gap). Now: a car under GTA's ground goes back to the last spot it stood on; it waits
	// (frozen) until GTA has the collision around it; then an immediate ray finds the ground right under it.
	struct LandJob
	{
		bool        on = false, frozen = false, moved = false, haveG = false, keepV = false;
		int         veh = 0;
		double      t0 = 0;
		double      x = 0, y = 0, z = 0;  // the GTA car (entity origin)
		double      gz = 0;               // GTA's ground there from the measured tiles (haveG)
		double      V[3] = {};
		float       bottom = 0;           // the model's bottom below its origin (g_tgt.dmin.z)
		std::string why;
	} g_land;

	void LandTick()
	{
		LandJob& L = g_land;
		if (!L.on) return;
		if (!Call<int>(N::DOES_ENTITY_EXIST, L.veh)) {
			L.on = false;
			return;
		}
		const double now = NowSeconds();
		CallVoid(N::REQUEST_COLLISION_AT_COORD, float(L.x), float(L.y), float(L.z));
		const bool loaded = Call<int>(N::HAS_COLLISION_LOADED_AROUND_ENTITY, L.veh) != 0;
		if (!loaded && now - L.t0 < 8.0) {
			if (!L.frozen) {
				CallVoid(N::FREEZE_ENTITY_POSITION, L.veh, 1);
				CallVoid(N::SET_ENTITY_VELOCITY, L.veh, 0.0f, 0.0f, 0.0f);
				L.frozen = true;
				Log("drive: GTA hasn't loaded its ground around the car yet: the car waits (frozen)");
			}
			return;
		}
		// GTA's ground right under the car: an immediate ray (map only) from just above it; else the measured tiles; else
		// GTA's ground height
		const NVec3  p = Call<NVec3>(N::GET_ENTITY_COORDS, L.veh, 0);
		const double tyres = p.z + L.bottom;
		double       gz = 0;
		const char*  src = nullptr;
		{
			const float top = float(tyres + 2.0);
			const int   h = Call<int>(N::START_EXPENSIVE_SYNCHRONOUS_SHAPE_TEST_LOS_PROBE, p.x, p.y, top, p.x, p.y, top - 40.0f, 1, L.veh, 7);
			if (h) {
				int   hit = 0, ent = 0;
				NVec3 end{}, nrm{};
				if (Call<int>(N::GET_SHAPE_TEST_RESULT, h, &hit, &end, &nrm, &ent) == 2 && hit) {
					gz = end.z;
					src = "a ray";
				}
			}
		}
		if (!src && L.haveG && fabs(L.gz - tyres) < 3.0) {
			gz = L.gz;
			src = "the measured tiles";
		}
		if (!src) {
			float g = 0;
			if (Call<int>(N::GET_GROUND_Z_FOR_3D_COORD, p.x, p.y, float(tyres + 2.0), &g, 0, 0)) {
				gz = g;
				src = "GTA's ground height";
			}
		}
		double moved = 0;
		if (src) {
			const double z = gz - L.bottom + 0.03;
			moved = z - p.z;
			if (fabs(moved) > 0.25 || L.frozen) CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, L.veh, p.x, p.y, float(z), 0, 0, 0);
		}
		if (L.frozen) CallVoid(N::FREEZE_ENTITY_POSITION, L.veh, 0);
		if (L.keepV)
			CallVoid(N::SET_ENTITY_VELOCITY, L.veh, float(L.V[0]), float(L.V[1]), float(L.V[2]));
		else
			CallVoid(N::SET_ENTITY_VELOCITY, L.veh, 0.0f, 0.0f, 0.0f);
		Log("drive: GTA's car is back on GTA's ground (%s): collision %s after %.2f s, ground from %s (car moved %+.2f m up)%s%s", L.why.c_str(),
			loaded ? "loaded" : "STILL NOT LOADED", now - L.t0, src ? src : "nowhere (no ground found!)", moved, L.moved ? "; it was put on the last good spot first" : "",
			L.keepV ? "; it carries on at BeamNG's speed" : "");
		if (L.keepV) Log("drive: GTA's car carries on at BeamNG's speed (%.1f km/h)", sqrt(L.V[0] * L.V[0] + L.V[1] * L.V[1] + L.V[2] * L.V[2]) * 3.6);
		L.on = false;
	}

	// ---------------------------------------------------------------------------------------
	// v2.6: GTA's weapons and explosions on BeamNG's car (your picks: dents where it's hit, tyres pop, windows shatter, engine and
	// radiator damage; a blast pushes it and can set it on fire; it burns in BeamNG and after a while blows up like GTA's; hard
	// crashes and blasts hurt you). In drive mode GTA's car is hidden and has no collision, so GTA's bullets fly through it: the
	// shots are found from the shooters (where each one's last bullet landed: GET_PED_LAST_WEAPON_IMPACT_COORD), and their line is
	// tested against the drawn car's box. Explosions come from GTA's explosion list (IS_EXPLOSION_IN_SPHERE). BeamNG finds where its
	// car was hit (its collision triangles) and does the rest; its fire, its explosion and its broken glass come back ("fx").
	// The numbers are in GtaBeamD1_weapons.json (written with these defaults when missing; Ctrl+F11 reads it again).
	// ---------------------------------------------------------------------------------------
	struct WGroup
	{
		float       dent = 1.5f, tire = 1, glass = 1, eng = 0.002f, rad = 0.01f, driver = 15;  // dent: cm deep (v2.61; 2.6: a push in m/s)
		std::string name;
	};
	struct WBlast
	{
		float       accel = 400, r = 1.5f, dt = 0.05f, ign = 0.3f, glassR = 5, hurt = 40, hurtR = 6;
		float       explodeR = 0;  // v2.63: this close to the car's middle (m) it blows the car up at once (a rocket hit)
		std::string name;
	};
	struct WShooter
	{
		double last[3] = {};
		bool   have = false;
		double shootT = -100;
		double pendT = -1;     // a shot seen (IS_PED_SHOOTING going on) with no new impact yet
		bool   was = false;
	};
	struct WeaponState
	{
		bool                               cfgLoaded = false;
		std::string                        cfgSource;
		std::map<uint32_t, WGroup>         groups;
		WGroup                             def;
		std::map<int, WBlast>              blasts;
		WBlast                             other, car;
		float                              burnS = 30, engineFire = 24, crashDv = 7, crashHp = 6, driverR = 0.35f, guess = 1;
		float                              holeCm = 2.2f, fireScale = 0.35f;  // v2.62: bullet hole size; GTA's flames on the car (x the effect's own size)
		int                                shatterHits = 3;                     // v2.63: hits on a cracked windscreen / lamp before it shatters out
		uint32_t                           holeModel = 0;                      // v2.62: the car file the holes are on
		std::vector<int>                   peds;
		std::map<int, WShooter>            sh;
		uint32_t                           scan = 0;
		std::vector<std::array<double, 4>> recent;  // blasts handled: x, y, z (GTA), time
		std::vector<double>                sentT;   // hits sent in the last second
		int                                proofVeh = 0, proofPed = 0;  // the hidden car and you: GTA's own damage off
		bool                               burning = false, boomed = false;
		int                                fire = 0;  // GTA's fire where BeamNG's car burns
		double                             fireP[3] = {};  // (v2.61: on the hidden car: its offset there)
		bool                               fireFx = false;  // v2.61: fire is a particle effect on the hidden car (else a fire on the spot)
		int                                fxPick = -1, fxTries = 0;  // which of kFireFx GTA has
		std::vector<std::array<double, 4>> vh;  // the car's flat velocity: t, x, y, speed (hard crashes)
		int                                crashRun = 0;  // v2.7: ticks in a row over crashDv
		double                             crashT = 0;
		uint32_t                           shots = 0, onCar = 0, guesses = 0, sent = 0, blastsN = 0, fxHits = 0, hurtN = 0, logs = 0, logs2 = 0;
		std::string                        dgLast;  // the broken groups last logged
		double                             hurtSum = 0, logT = 0;
	} g_wpn;

	const char* kWeaponsDefault = R"JSON({
  "about": "GTA x Beam v2.63: GTA's weapons and explosions on BeamNG's car. groups (GTA's weapon groups), what one bullet does: dentCm = how deep it dents the car where it hits (cm), tire / glass = chance (0 to 1) that a tyre / window it hits breaks, engine / radiator = BeamNG's damage per hit near them (radiator 0.1 = destroyed), driver = health it takes from you when it goes through the cabin past your head (100 = all). A group with dentCm 0 and driver 0 is ignored. blasts (GTA's explosion types, -1 = any other): accel = push at 1 m (m/s2, less with the distance squared), r = its core (m), dt = how long it pushes (s), ignite = chance it sets the car on fire, glassR = windows within this break (m), hurt = health it takes from you at the car, none at hurtR (m), explodeR = this close to the car's middle (m) it blows the car up at once (a rocket hit; a car explosion that close is GTA's own explosion of your car). car = the car's own explosion. burnToExplodeS = how long the car burns before it blows up; engineFireHits = engine hits before the engine catches fire; bulletHoleCm = the size of a bullet hole on the drawn car (0 = none); fireScale = GTA's flames on the burning car (1 = the effect's own size); glassShatterHits = more hits on a cracked windscreen / lamp before it shatters out (0 = never); crashMinDv / crashHpPerDv = a crash hurts you when the car's speed changes more than this within 0.25 s (m/s), this much health per m/s more; guessChance = for shooters GTA gives no bullet impact for: x their accuracy. Ctrl+F11 reads this file again.",
  "burnToExplodeS": 30,
  "engineFireHits": 24,
  "bulletHoleCm": 2.2,
  "fireScale": 0.35,
  "glassShatterHits": 3,
  "crashMinDv": 7,
  "crashHpPerDv": 6,
  "guessChance": 1.0,
  "groups": [
    { "group": "GROUP_PISTOL", "dentCm": 0.75, "tire": 1, "glass": 1, "engine": 0.001, "radiator": 0.005, "driver": 15 },
    { "group": "GROUP_SMG", "dentCm": 0.75, "tire": 1, "glass": 1, "engine": 0.001, "radiator": 0.005, "driver": 12 },
    { "group": "GROUP_RIFLE", "dentCm": 1.25, "tire": 1, "glass": 1, "engine": 0.0015, "radiator": 0.0075, "driver": 20 },
    { "group": "GROUP_MG", "dentCm": 1.25, "tire": 1, "glass": 1, "engine": 0.0015, "radiator": 0.0075, "driver": 20 },
    { "group": "GROUP_SHOTGUN", "dentCm": 2, "tire": 1, "glass": 1, "engine": 0.002, "radiator": 0.01, "driver": 30 },
    { "group": "GROUP_SNIPER", "dentCm": 2.5, "tire": 1, "glass": 1, "engine": 0.004, "radiator": 0.02, "driver": 60 },
    { "group": "GROUP_HEAVY", "dentCm": 1.5, "tire": 1, "glass": 1, "engine": 0.0015, "radiator": 0.0075, "driver": 20 },
    { "group": "GROUP_MELEE", "dentCm": 0, "driver": 0 },
    { "group": "GROUP_THROWN", "dentCm": 0, "driver": 0 },
    { "group": "GROUP_STUNGUN", "dentCm": 0, "driver": 0 },
    { "group": "default", "dentCm": 0.75, "tire": 1, "glass": 1, "engine": 0.001, "radiator": 0.005, "driver": 15 }
  ],
  "blasts": [
    { "type": 0, "name": "grenade", "accel": 700, "r": 1.2, "dt": 0.05, "ignite": 0.4, "glassR": 6, "hurt": 70, "hurtR": 7, "explodeR": 1.8 },
    { "type": 1, "name": "grenade launcher", "accel": 700, "r": 1.2, "dt": 0.05, "ignite": 0.4, "glassR": 6, "hurt": 70, "hurtR": 7, "explodeR": 2.5 },
    { "type": 2, "name": "sticky bomb", "accel": 1000, "r": 1.0, "dt": 0.05, "ignite": 0.6, "glassR": 7, "hurt": 90, "hurtR": 8, "explodeR": 2.5 },
    { "type": 3, "name": "molotov", "accel": 60, "r": 1.5, "dt": 0.05, "ignite": 1.0, "glassR": 2, "hurt": 15, "hurtR": 3 },
    { "type": 4, "name": "rocket", "accel": 1200, "r": 1.0, "dt": 0.05, "ignite": 0.8, "glassR": 8, "hurt": 100, "hurtR": 9, "explodeR": 3.5 },
    { "type": 5, "name": "tank shell", "accel": 1800, "r": 1.0, "dt": 0.06, "ignite": 0.9, "glassR": 10, "hurt": 150, "hurtR": 10, "explodeR": 4.5 },
    { "type": 7, "name": "car", "accel": 500, "r": 1.5, "dt": 0.06, "ignite": 0.7, "glassR": 6, "hurt": 60, "hurtR": 7, "explodeR": 2.5 },
    { "type": 9, "name": "petrol pump", "accel": 800, "r": 1.5, "dt": 0.06, "ignite": 0.9, "glassR": 8, "hurt": 80, "hurtR": 9 },
    { "type": 27, "name": "barrel", "accel": 400, "r": 1.5, "dt": 0.05, "ignite": 0.6, "glassR": 5, "hurt": 50, "hurtR": 6 },
    { "type": 34, "name": "gas tank", "accel": 400, "r": 1.5, "dt": 0.05, "ignite": 0.6, "glassR": 5, "hurt": 50, "hurtR": 6 },
    { "type": -1, "name": "other", "accel": 400, "r": 1.5, "dt": 0.05, "ignite": 0.3, "glassR": 5, "hurt": 40, "hurtR": 6 }
  ],
  "car": { "accel": 500, "r": 1.5, "dt": 0.06, "ignite": 1.0, "glassR": 10, "hurt": 200, "hurtR": 6 }
})JSON";

	// the objects of an array ("key": [ {...}, {...} ]); objects without nested braces
	std::vector<std::string> JsonObjects(const std::string& js, const char* key)
	{
		std::vector<std::string> out;
		const size_t             a = js.find(std::string("\"") + key + "\"");
		if (a == std::string::npos) return out;
		size_t       p = js.find('[', a);
		const size_t close = p == std::string::npos ? p : js.find(']', p);
		while (p != std::string::npos) {
			const size_t o = js.find('{', p);
			if (o == std::string::npos || (close != std::string::npos && o > close)) break;
			const size_t e = js.find('}', o);
			if (e == std::string::npos) break;
			out.push_back(js.substr(o, e - o + 1));
			p = e + 1;
		}
		return out;
	}

	void WBlastOf(const std::string& o, WBlast& b)
	{
		double v;
		if (JsonNumC(o.c_str(), "accel", v)) b.accel = float(std::clamp(v, 0.0, 20000.0));
		if (JsonNumC(o.c_str(), "r", v)) b.r = float(std::clamp(v, 0.05, 10.0));
		if (JsonNumC(o.c_str(), "dt", v)) b.dt = float(std::clamp(v, 0.01, 0.5));
		if (JsonNumC(o.c_str(), "ignite", v)) b.ign = float(std::clamp(v, 0.0, 1.0));
		if (JsonNumC(o.c_str(), "glassR", v)) b.glassR = float(std::clamp(v, 0.0, 50.0));
		if (JsonNumC(o.c_str(), "hurt", v)) b.hurt = float(std::clamp(v, 0.0, 1000.0));
		if (JsonNumC(o.c_str(), "hurtR", v)) b.hurtR = float(std::clamp(v, 0.1, 100.0));
		if (JsonNumC(o.c_str(), "explodeR", v)) b.explodeR = float(std::clamp(v, 0.0, 30.0));
		JsonStr(o, "name", b.name);
	}

	bool ParseWeapons(const std::string& js, WeaponState& W)
	{
		const std::vector<std::string> gs = JsonObjects(js, "groups"), bs = JsonObjects(js, "blasts");
		if (gs.empty() && bs.empty()) return false;
		W.groups.clear();
		W.blasts.clear();
		W.def = WGroup{};
		W.other = WBlast{};
		int oldDent = 0;
		for (const std::string& o : gs) {
			std::string n;
			if (!JsonStr(o, "group", n) || n.empty()) continue;
			WGroup g;
			double v;
			if (JsonNumC(o.c_str(), "dentCm", v)) g.dent = float(std::clamp(v, 0.0, 10.0));  // (v2.61; a 2.6 file's "dent" (m/s) is not read)
			else if (o.find("\"dent\"") != std::string::npos) ++oldDent;
			if (JsonNumC(o.c_str(), "tire", v)) g.tire = float(std::clamp(v, 0.0, 1.0));
			if (JsonNumC(o.c_str(), "glass", v)) g.glass = float(std::clamp(v, 0.0, 1.0));
			if (JsonNumC(o.c_str(), "engine", v)) g.eng = float(std::clamp(v, 0.0, 1.0));
			if (JsonNumC(o.c_str(), "radiator", v)) g.rad = float(std::clamp(v, 0.0, 1.0));
			if (JsonNumC(o.c_str(), "driver", v)) g.driver = float(std::clamp(v, 0.0, 1000.0));
			g.name = n;
			if (n == "default") W.def = g;
			else W.groups[(n.size() > 2 && n[0] == '0' && (n[1] == 'x' || n[1] == 'X')) ? uint32_t(strtoul(n.c_str(), nullptr, 16)) : Joaat(n.c_str())] = g;
		}
		for (const std::string& o : bs) {
			double t = -1;
			JsonNumC(o.c_str(), "type", t);
			WBlast b;
			WBlastOf(o, b);
			if (int(t) < 0) W.other = b;
			else W.blasts[int(t)] = b;
		}
		size_t c = js.find("\"car\"");
		while (c != std::string::npos && js.find_first_not_of(" \t\r\n", c + 5) != std::string::npos && js[js.find_first_not_of(" \t\r\n", c + 5)] != ':')
			c = js.find("\"car\"", c + 5);  // (the key, not the blast named "car")
		const size_t co = c == std::string::npos ? c : js.find('{', c);
		const size_t ce = co == std::string::npos ? co : js.find('}', co);
		if (ce != std::string::npos) WBlastOf(js.substr(co, ce - co + 1), W.car);
		double v;
		if (JsonNumC(js.c_str(), "burnToExplodeS", v)) W.burnS = float(std::clamp(v, 1.0, 600.0));
		if (JsonNumC(js.c_str(), "bulletHoleCm", v)) W.holeCm = float(std::clamp(v, 0.0, 10.0));  // v2.62
		if (JsonNumC(js.c_str(), "fireScale", v)) W.fireScale = float(std::clamp(v, 0.05, 3.0));
		if (JsonNumC(js.c_str(), "glassShatterHits", v)) W.shatterHits = int(std::clamp(v, 0.0, 100.0));
		if (JsonNumC(js.c_str(), "engineFireHits", v)) W.engineFire = float(std::clamp(v, 0.0, 1000.0));
		if (JsonNumC(js.c_str(), "crashMinDv", v)) W.crashDv = float(std::clamp(v, 2.0, 100.0));
		if (JsonNumC(js.c_str(), "crashHpPerDv", v)) W.crashHp = float(std::clamp(v, 0.0, 100.0));
		if (JsonNumC(js.c_str(), "guessChance", v)) W.guess = float(std::clamp(v, 0.0, 1.0));
		if (oldDent) Log("weapons: GtaBeamD1_weapons.json is the 2.6 one (\"dent\" in m/s): its %d dent numbers are not used (1.5 cm each); replace it with the 2.62 file (\"dentCm\")", oldDent);
		else if (js.find("\"bulletHoleCm\"") == std::string::npos && js.find("\"fireScale\"") == std::string::npos)
			Log("weapons: GtaBeamD1_weapons.json is from 2.61 or older: its full-strength dents are used (2.62's file has half the damage); replace it with the 2.62 file");
		return true;
	}

	void LoadWeapons()
	{
		WeaponState& W = g_wpn;
		char         path[MAX_PATH + 16];
		PathNextToModule("_weapons.json", path, sizeof(path));
		std::string js;
		bool        fromFile = false;
		if (FILE* f = fopen(path, "rb")) {
			char   b[4096];
			size_t n;
			while ((n = fread(b, 1, sizeof(b), f)) > 0) js.append(b, n);
			fclose(f);
			fromFile = true;
		}
		if (fromFile && ParseWeapons(js, W)) {
			W.cfgSource = path;
		} else {
			if (fromFile) Log("weapons: GtaBeamD1_weapons.json not usable (no \"groups\" or \"blasts\" list): the built-in numbers are used");
			ParseWeapons(kWeaponsDefault, W);
			W.cfgSource = "built-in numbers";
			if (!fromFile) {
				if (FILE* f = fopen(path, "wb")) {
					fputs(kWeaponsDefault, f);
					fclose(f);
					W.cfgSource = std::string("built-in numbers (written to ") + path + ")";
				}
			}
		}
		W.cfgLoaded = true;
		Log("weapons: %zu weapon groups, %zu explosion types (%s); the car burns %.0f s before it blows up, its engine catches fire after %.0f hits; crashes hurt above %.1f m/s",
			W.groups.size(), W.blasts.size(), W.cfgSource.c_str(), W.burnS, W.engineFire, W.crashDv);
	}

	// GTA world -> BeamNG world (the drive's mapping, inverse of DrvMapPoint / DrvMapDir)
	void DrvUnmapPoint(const double G[3], double out[3])
	{
		const DriveState& d = g_drv;
		const double      dx = G[0] - d.Ag[0], dy = G[1] - d.Ag[1];
		out[0] = d.Ab[0] + d.cth * dx + d.sth * dy;
		out[1] = d.Ab[1] - d.sth * dx + d.cth * dy;
		out[2] = d.Ab[2] + G[2] - d.Ag[2];
	}
	void DrvUnmapDir(const double v[3], double out[3])
	{
		const DriveState& d = g_drv;
		out[0] = d.cth * v[0] + d.sth * v[1];
		out[1] = -d.sth * v[0] + d.cth * v[1];
		out[2] = v[2];
	}

	// what drive_start tells the BeamNG mod (the car's burning time, how it blows up and its own blast)
	std::string WeaponsStartJson()
	{
		WeaponState& W = g_wpn;
		if (!g_cfg.weapons) return ",\"wp\":0";
		if (!W.cfgLoaded) LoadWeapons();
		char b[200];
		snprintf(b, sizeof(b), ",\"wp\":1,\"wb\":%.1f,\"wm\":%d,\"wx\":[%.1f,%.2f,%.3f,%.2f,%.1f]", W.burnS, g_cfg.carExplosion, W.car.accel, W.car.r, W.car.dt, W.car.ign,
			W.car.glassR);
		return b;
	}

	// your health down by hp (GTA's player: 200 full, dead at 100)
	void WeaponsHurt(double hp, const char* why)
	{
		WeaponState& W = g_wpn;
		const int    ped = Call<int>(N::PLAYER_PED_ID);
		if (hp < 1.0 || Call<int>(N::IS_PED_DEAD_OR_DYING, ped, 1)) return;
		const int h = Call<int>(N::GET_ENTITY_HEALTH, ped);
		int       nh = int(h - hp);
		if (nh <= 100) nh = 0;
		CallVoid(N::SET_ENTITY_HEALTH, ped, nh, 0, 0);
		++W.hurtN;
		W.hurtSum += h - nh;
		if ((strstr(why, "bullet") ? W.logs++ < 90 : W.logs2++ < 200)) Log("weapons: you lost %.0f health (%s): %d -> %d", hp, why, h, nh);
	}

	// drive mode on: the hidden car (and you in it) don't take GTA's own damage: BeamNG's car takes it, you get it from here
	void WeaponsPuppet(bool on)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		const int    ped = Call<int>(N::PLAYER_PED_ID);
		if (on) {
			if (!g_cfg.weapons || !d.veh) return;
			CallVoid(N::SET_ENTITY_PROOFS, d.veh, 1, 1, 1, 0, 1, 1, 0, 0);
			CallVoid(N::SET_ENTITY_PROOFS, ped, 1, 1, 1, 0, 0, 0, 0, 0);
			W.proofVeh = d.veh;
			W.proofPed = ped;
			W.boomed = false;
			W.vh.clear();
			Log("weapons: GTA's own bullet, fire and explosion damage off the hidden car and you while BeamNG drives (they hit BeamNG's car; you get hurt from here)");
			return;
		}
		if (W.proofPed && Call<int>(N::DOES_ENTITY_EXIST, W.proofPed)) CallVoid(N::SET_ENTITY_PROOFS, W.proofPed, 0, 0, 0, 0, 0, 0, 0, 0);
		W.proofPed = 0;
		if (W.proofVeh && Call<int>(N::DOES_ENTITY_EXIST, W.proofVeh)) {
			if (W.burning && !W.boomed) {
				// still burning in BeamNG: GTA's fire there mustn't blow GTA's car up before BeamNG's does
				CallVoid(N::SET_ENTITY_PROOFS, W.proofVeh, 0, 1, 1, 0, 0, 1, 0, 0);
				Log("weapons: the drive ended with BeamNG's car on fire: it burns on and blows up when BeamNG says");
				return;
			}
			CallVoid(N::SET_ENTITY_PROOFS, W.proofVeh, 0, 0, 0, 0, 0, 0, 0, 0);
		}
		W.proofVeh = 0;
	}

	// v2.61: you got out (foot mode) / back in: on foot GTA's own bullets, fire and explosions hurt you again (none from here)
	void WeaponsFoot(bool out)
	{
		WeaponState& W = g_wpn;
		const int    ped = Call<int>(N::PLAYER_PED_ID);
		if (!g_cfg.weapons || !W.proofPed) return;
		if (out) CallVoid(N::SET_ENTITY_PROOFS, ped, 0, 0, 0, 0, 0, 0, 0, 0);
		else CallVoid(N::SET_ENTITY_PROOFS, ped, 1, 1, 1, 0, 0, 0, 0, 0);
		W.proofPed = ped;
		W.peds.clear();  // (you join / leave the shooters at once)
		W.vh.clear();
	}

	void WeaponsFireOff()
	{
		WeaponState& W = g_wpn;
		if (W.fire && W.fireFx) CallVoid(N::STOP_PARTICLE_FX_LOOPED, W.fire, 0);
		else if (W.fire) CallVoid(N::REMOVE_SCRIPT_FIRE, W.fire);
		W.fire = 0;
	}

	// v2.61: GTA's flames ON the hidden car where BeamNG's burns, so they move with it (2.6 lit a fire on one spot: it floated
	// behind the car as it rolled, or didn't show inside it). GTA's particle effects: the first of these GTA has (logged).
	// shortcut: effect names not checked against your GTA (no game here); none found = 2.6's fire on the spot
	// v2.62: the plane cockpit fire first (smaller); your 2.61 test got the helicopter one: "way too much"
	const char* const kFireFx[][2] = { { "core", "fire_wrecked_plane_cockpit" }, { "core", "ent_ray_heli_aprtmnt_l_fire" } };
	void WeaponsFlames(int veh, const double pg[3])
	{
		WeaponState& W = g_wpn;
		const int    nFx = int(sizeof(kFireFx) / sizeof(kFireFx[0]));
		NVec3        off{};
		if (veh && Call<int>(N::DOES_ENTITY_EXIST, veh)) off = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS, veh, float(pg[0]), float(pg[1]), float(pg[2]));
		const double o[3] = { off.x, off.y, off.z };
		if (W.fire && W.fireFx && fabs(o[0] - W.fireP[0]) + fabs(o[1] - W.fireP[1]) + fabs(o[2] - W.fireP[2]) < 0.75) return;  // (still there)
		if (W.fire && !W.fireFx && hypot(pg[0] - W.fireP[0], pg[1] - W.fireP[1]) < 1.5 && fabs(pg[2] - W.fireP[2]) < 1.5) return;
		WeaponsFireOff();
		if (veh && W.fxPick != -2 && W.fxTries < 20) {
			for (int i = std::max(0, W.fxPick); i < nFx; ++i) {
				CallVoid(N::REQUEST_NAMED_PTFX_ASSET, kFireFx[i][0]);
				if (!Call<int>(N::HAS_NAMED_PTFX_ASSET_LOADED, kFireFx[i][0])) {
					++W.fxTries;  // (asked for: tried again with the next fire report, half a second later)
					return;
				}
				CallVoid(N::USE_PARTICLE_FX_ASSET, kFireFx[i][0]);
				const int h = Call<int>(N::START_PARTICLE_FX_LOOPED_ON_ENTITY, kFireFx[i][1], veh, off.x, off.y, off.z, 0.0f, 0.0f, 0.0f, W.fireScale, 0, 0, 0);
				if (h && Call<int>(N::DOES_PARTICLE_FX_LOOPED_EXIST, h)) {
					if (W.fxPick != i) Log("weapons: GTA's flames on the car: particle effect %s/%s", kFireFx[i][0], kFireFx[i][1]);
					W.fxPick = i;
					W.fire = h;
					W.fireFx = true;
					memcpy(W.fireP, o, sizeof(W.fireP));
					return;
				}
				if (W.fxPick == i) W.fxPick = -1;  // (it worked before: try the list again)
				if (i == nFx - 1) {
					W.fxPick = -2;
					Log("weapons: none of GTA's fire effects started (%d tried): a fire on the spot instead (as 2.6)", nFx);
				}
			}
		}
		W.fire = Call<int>(N::START_SCRIPT_FIRE, float(pg[0]), float(pg[1]), float(pg[2]), 25, 0);
		W.fireFx = false;
		memcpy(W.fireP, pg, sizeof(W.fireP));
	}

	// BeamNG's reports: hits it took, its fire, its explosion, its broken glass (also after the drive: a car left burning)
	void WeaponsFx(double now)
	{
		WeaponState&             W = g_wpn;
		DriveState&              d = g_drv;
		std::vector<std::string> q;
		AcquireSRWLockExclusive(&g_dnet.lock);
		q.swap(g_dnet.fxQ);
		ReleaseSRWLockExclusive(&g_dnet.lock);
		for (const std::string& s : q) {
			std::string k;
			JsonStr(s, "k", k);
			double     pb[3], pg[3] = {};
			const bool hasP = JsonNums(s.c_str(), "p", pb, 3) == 3 && (d.fixedMap || d.state == 2);
			if (hasP) DrvMapPoint(pb, pg);
			if (k == "fire") {
				const bool on = JsonNum(s, "on", 0) > 0.5;
				if (on && !W.boomed) {
					if (!W.burning) {
						Log("weapons: BeamNG's car is on fire (%d nodes burning)%s", int(JsonNum(s, "n", 0)), hasP ? "" : " (no place for GTA's fire: no mapping)");
						Notify("Your car is on fire! It blows up in about %.0f s.", W.burnS);
					}
					W.burning = true;
					if (hasP) WeaponsFlames(W.proofVeh ? W.proofVeh : (g_tgt.mode ? g_tgt.veh : 0), pg);
				} else if (!on && W.burning) {
					W.burning = false;
					WeaponsFireOff();
					Log("weapons: BeamNG's car stopped burning");
					if (d.state == 0) WeaponsPuppet(false);
				}
			} else if (k == "boom") {
				std::string why;
				JsonStr(s, "why", why);
				W.boomed = true;
				W.burning = false;
				WeaponsFireOff();
				{
					std::lock_guard<std::mutex> lk(g_holeMx);
					g_burnt = 1.0f;  // v2.7: drawn charred, like GTA's wrecks
					++g_holeGen;
				}
				const int veh = W.proofVeh ? W.proofVeh : (g_tgt.mode ? g_tgt.veh : 0);
				if (veh && Call<int>(N::DOES_ENTITY_EXIST, veh)) {
					NVec3 vp = Call<NVec3>(N::GET_ENTITY_COORDS, veh, 0);
					W.recent.push_back({ vp.x, vp.y, vp.z, now + 1.0 });  // (its own explosion is no new blast for BeamNG's car)
					const bool inCar = Call<int>(N::IS_PED_IN_VEHICLE, Call<int>(N::PLAYER_PED_ID), veh, 0) != 0;
					CallVoid(N::SET_ENTITY_PROOFS, veh, 0, 0, 0, 0, 0, 0, 0, 0);
					CallVoid(N::EXPLODE_VEHICLE, veh, 1, 0);
					if (inCar) WeaponsHurt(W.car.hurt, "your car blew up with you in it");
					W.proofVeh = 0;
				}
				Log("weapons: BeamNG's car blew up (%s): GTA's explosion at it", why.c_str());
			} else if (k == "dg") {
				// the car's broken groups (all of them, each time the list changes)
				std::string g;
				JsonStr(s, "g", g);
				uint64_t                 mask[4] = {};
				std::vector<std::string> names;
				{
					std::lock_guard<std::mutex> lk(g_modelMx);
					names = g_drvModel.dgNames;
				}
				int known = 0, unknown = 0;
				for (size_t a = 0; a <= g.size();) {
					size_t e = g.find(',', a);
					if (e == std::string::npos) e = g.size();
					const std::string n = g.substr(a, e - a);
					a = e + 1;
					if (n.empty()) continue;
					size_t i = 0;
					while (i < names.size() && names[i] != n) ++i;
					if (i < names.size() && i < 256) {
						mask[i >> 6] |= 1ull << (i & 63);
						++known;
					} else {
						++unknown;
					}
				}
				for (int i = 0; i < 4; ++i) g_dgBroken[i].store(mask[i]);
				const bool first = W.dgLast.empty() && g.empty();
				if (g != W.dgLast && !first && W.logs2++ < 200)
					Log("weapons: BeamNG broke %s%s (%d of the drawn car's groups%s)", g.empty() ? "nothing (repaired)" : g.substr(0, 300).c_str(), "", known,
					unknown ? ", others the car file doesn't name: export the car again with the 2.6 mod" : "");
				W.dgLast = g;
			} else if (k == "hit") {
				++W.fxHits;
				std::string how, tire, glass;
				JsonStr(s, "how", how);
				JsonStr(s, "tire", tire);
				JsonStr(s, "glass", glass);
				if (W.logs++ < 90)
					Log("weapons: BeamNG: a bullet hit its car (%s)%s%s%s%s%s", how.c_str(), tire.empty() ? "" : ", tyre popped: ", tire.c_str(), glass.empty() ? "" : ", glass broken: ",
						glass.c_str(), JsonNum(s, "en", 0) > 0 ? ", engine hit" : JsonNum(s, "ra", 0) > 0 ? ", radiator hit" : "");
				// v2.62: a bullet hole where it hit (the car's frame from BeamNG, less the drawn car's origin in it)
				double hc[3];
				if (W.holeCm > 0 && d.oDone && JsonNums(s.c_str(), "c", hc, 3) == 3) {
					// v2.7: the bullet's direction too: the hole is drawn where its line meets the drawn panel (BeamNG's hit is on
					// its collision triangles, a few cm inside the panel: most of 2.63's holes were inside it, unseen). One hit
					// per spot (shots on the same spot don't push older holes out)
					double dd[3] = { 0, 0, 0 };
					JsonNums(s.c_str(), "dd", dd, 3);
					std::lock_guard<std::mutex> lk(g_holeMx);
					const float r = W.holeCm * 0.01f;
					bool        dup = false;
					for (int q = 0; q < g_holeN && !dup; ++q) {
						const float* o = g_holes[q];
						dup = std::hypot(o[0] - float(hc[0] - d.o[0]), o[1] - float(hc[1] - d.o[1]), o[2] - float(hc[2] - d.o[2])) < 0.5f * r;
					}
					if (!dup) {
						float* h = g_holes[g_holeNext];
						for (int q = 0; q < 3; ++q) h[q] = float(hc[q] - d.o[q]), h[4 + q] = float(dd[q]);
						h[3] = r;
						h[7] = 0;
						g_holeNext = (g_holeNext + 1) % kHoles;
						g_holeN = std::min(g_holeN + 1, kHoles);
						++g_holeGen;
					}
				}
				// v2.63: a hit on a cracked windscreen / lamp: after glassShatterHits of them it shatters out
				if (W.shatterHits > 0 && d.oDone && JsonNums(s.c_str(), "c", hc, 3) == 3) {
					std::lock_guard<std::mutex> lk(g_holeMx);
					for (size_t gi = 0; gi < g_dgBox.size() && gi < 256; ++gi) {
						const bool cracked = g_dgBroken[gi >> 6].load() >> (gi & 63) & 1, gone = g_dgShatter[gi >> 6].load() >> (gi & 63) & 1;
						if (!cracked || gone) continue;
						const auto& b = g_dgBox[gi];
						bool        in = true;
						for (int a = 0; a < 3; ++a) in = in && hc[a] - d.o[a] >= b[a] - 0.12 && hc[a] - d.o[a] <= b[3 + a] + 0.12;  // (BeamNG's hit vs the glass mesh: a few cm apart)
						if (!in || ++g_dgHits[gi] < W.shatterHits) continue;
						g_dgShatter[gi >> 6].fetch_or(1ull << (gi & 63));
						if (W.logs2++ < 200) Log("weapons: the cracked glass / lamp group %zu shattered out after %d more hits", gi, g_dgHits[gi]);
					}
				}
			} else if (k == "repaired") {
				HolesClear("BeamNG's car repaired");
			} else if (k == "dv" || k == "info" || k == "blast" || k == "ignite" || k == "dent") {
				std::string t;
				JsonStr(s, "s", t);
				if (W.logs2++ < 200) Log("weapons: BeamNG: %s", t.c_str());
			}
		}
	}

	// the shot from A to B against the drawn car's box (inflated by m): where it enters it and its direction; false when it
	// misses or starts inside (you shooting out of the car)
	bool WeaponsRay(const DriveState& d, const double A[3], const double B[3], double m, double E[3], double D[3])
	{
		const double w = std::max(1.0f, g_modelSize[0].load()), l = std::max(2.0f, g_modelSize[1].load()), h = std::max(0.8f, g_modelSize[2].load());
		const double half[3] = { w * 0.5 + m, l * 0.5 + m, h * 0.5 + m };
		const double C[3] = { d.carO[0] + d.carU[0] * h * 0.5, d.carO[1] + d.carU[1] * h * 0.5, d.carO[2] + d.carU[2] * h * 0.5 };
		const double* ax[3] = { d.carR, d.carF, d.carU };
		double        t0 = 0, t1 = 1;
		bool          inside = true;
		for (int i = 0; i < 3; ++i) {
			const double a = (A[0] - C[0]) * ax[i][0] + (A[1] - C[1]) * ax[i][1] + (A[2] - C[2]) * ax[i][2];
			const double b = (B[0] - C[0]) * ax[i][0] + (B[1] - C[1]) * ax[i][1] + (B[2] - C[2]) * ax[i][2];
			inside = inside && fabs(a) <= half[i];
			const double dd = b - a;
			if (fabs(dd) < 1e-9) {
				if (fabs(a) > half[i]) return false;
				continue;
			}
			double s0 = (-half[i] - a) / dd, s1 = (half[i] - a) / dd;
			if (s0 > s1) std::swap(s0, s1);
			t0 = std::max(t0, s0);
			t1 = std::min(t1, s1);
			if (t0 > t1) return false;
		}
		if (inside) return false;
		double len = 0;
		for (int k = 0; k < 3; ++k) {
			E[k] = A[k] + (B[k] - A[k]) * t0;
			D[k] = B[k] - A[k];
			len += D[k] * D[k];
		}
		len = sqrt(len);
		if (len < 1e-6) return false;
		for (int k = 0; k < 3; ++k) D[k] /= len;
		return true;
	}

	// one bullet into the drawn car: to BeamNG, and you hurt when it passes your head
	void WeaponsHit(const double E[3], const double D[3], const double B[3], uint32_t weapon, bool guessed)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		const double now = NowSeconds();
		while (!W.sentT.empty() && now - W.sentT.front() > 1.0) W.sentT.erase(W.sentT.begin());
		if (W.sentT.size() >= 30) return;  // (30 a second at most: a minigun)
		const uint32_t grp = Call<uint32_t>(N::GET_WEAPONTYPE_GROUP, weapon);
		auto           it = W.groups.find(grp);
		const WGroup&  g = it != W.groups.end() ? it->second : W.def;
		if (g.dent <= 0 && g.driver <= 0) return;
		W.sentT.push_back(now);
		++W.sent;
		double Eb[3], Db[3], S[3];
		for (int k = 0; k < 3; ++k) S[k] = E[k] - D[k] * 1.0;  // (BeamNG looks from 1 m before the box)
		DrvUnmapPoint(S, Eb);
		DrvUnmapDir(D, Db);
		char m[360];
		snprintf(m, sizeof(m), "{\"t\":\"hit\",\"p\":[%.3f,%.3f,%.3f],\"d\":[%.4f,%.4f,%.4f],\"dv\":%.2f,\"ti\":%.2f,\"gl\":%.2f,\"en\":%.4f,\"ra\":%.4f,\"ef\":%.0f,\"w\":\"%s\"}",
			Eb[0], Eb[1], Eb[2], Db[0], Db[1], Db[2], g.dent, g.tire, g.glass, g.eng, g.rad, W.engineFire, it != W.groups.end() ? g.name.c_str() : "default");
		DrvSend(m);
		if (W.logs++ < 90)
			Log("weapons: a %sbullet (%s) hit the drawn car at %.2f %.2f %.2f going %.2f %.2f %.2f: to BeamNG", guessed ? "(guessed) " : "", it != W.groups.end() ? g.name.c_str() : "default",
				E[0], E[1], E[2], D[0], D[1], D[2]);
		// through the cabin past your head
		if (g.driver > 0 && !d.foot) {
			const NVec3  hd = Call<NVec3>(N::GET_PED_BONE_COORDS, Call<int>(N::PLAYER_PED_ID), 31086, 0.0f, 0.0f, 0.0f);  // SKEL_Head
			const double H[3] = { hd.x, hd.y, hd.z };
			double       t = 0, len2 = 0, dist2 = 0;
			for (int k = 0; k < 3; ++k) {
				t += (H[k] - E[k]) * D[k];
				len2 += (B[k] - E[k]) * (B[k] - E[k]);
			}
			t = std::clamp(t, 0.0, sqrt(len2) + 0.3);
			for (int k = 0; k < 3; ++k) dist2 += (H[k] - (E[k] + D[k] * t)) * (H[k] - (E[k] + D[k] * t));
			if (sqrt(dist2) < W.driverR) WeaponsHurt(g.driver, "a bullet through the cabin");
		}
	}

	// the shooters around: who fired since the last tick, and did the bullet go through the drawn car
	void WeaponsShots(double now)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		const int    me = Call<int>(N::PLAYER_PED_ID);
		if (W.peds.empty() || W.scan++ % 15 == 0) {
			std::vector<int> all(1024);
			const int        n = std::clamp(worldGetAllPeds(all.data(), int(all.size())), 0, int(all.size()));
			W.peds.clear();
			for (int i = 0; i < n; ++i) {
				const int p = all[i];
				if (!p || (p == me && !d.foot)) continue;  // (v2.61: your own shots too when you're out of the car)
				const NVec3 pp = Call<NVec3>(N::GET_ENTITY_COORDS, p, 0);
				if (hypot(pp.x - d.carO[0], pp.y - d.carO[1]) > 150.0) continue;
				W.peds.push_back(p);
				WShooter& S = W.sh[p];
				if (!S.have) {
					NVec3 imp{};
					if (Call<int>(N::GET_PED_LAST_WEAPON_IMPACT_COORD, p, &imp)) {
						S.last[0] = imp.x, S.last[1] = imp.y, S.last[2] = imp.z;
						S.have = true;
					}
				}
			}
			if (W.sh.size() > 2048) W.sh.clear();
		}
		for (int p : W.peds) {
			if (!Call<int>(N::DOES_ENTITY_EXIST, p) || (p == me && !d.foot)) continue;
			WShooter&  S = W.sh[p];
			const bool shooting = Call<int>(N::IS_PED_SHOOTING, p) != 0;
			const bool edge = shooting && !S.was;  // (a new shot, or a burst starting)
			S.was = shooting;
			if (shooting) S.shootT = now;
			if (now - S.shootT > 1.0) continue;
			NVec3      imp{};
			const bool ok = Call<int>(N::GET_PED_LAST_WEAPON_IMPACT_COORD, p, &imp) != 0;
			const bool fresh = ok && S.have && fabs(imp.x - S.last[0]) + fabs(imp.y - S.last[1]) + fabs(imp.z - S.last[2]) > 0.01;
			if (ok) {
				S.last[0] = imp.x, S.last[1] = imp.y, S.last[2] = imp.z;
				S.have = true;
			}
			const NVec3    hd = Call<NVec3>(N::GET_PED_BONE_COORDS, p, 31086, 0.0f, 0.0f, 0.0f);
			const double   A[3] = { hd.x, hd.y, hd.z };
			const uint32_t wpn = Call<uint32_t>(N::GET_SELECTED_PED_WEAPON, p);
			double         E[3], D[3];
			if (fresh) {
				++W.shots;
				S.pendT = -1;
				double B[3] = { imp.x, imp.y, imp.z };
				if (d.foot) {
					// v2.61: parked, GTA's hidden car is solid: the bullet stops on its outside, which may be short of the drawn car
					const double L = sqrt((B[0] - A[0]) * (B[0] - A[0]) + (B[1] - A[1]) * (B[1] - A[1]) + (B[2] - A[2]) * (B[2] - A[2]));
					if (L > 0.1) for (int k = 0; k < 3; ++k) B[k] += (B[k] - A[k]) / L * 1.0;
				}
				if (WeaponsRay(d, A, B, 0.15, E, D)) {
					++W.onCar;
					WeaponsHit(E, D, B, wpn, false);
				}
				continue;
			}
			if (edge && S.pendT < 0) S.pendT = now;
			if (S.pendT >= 0 && now - S.pendT > 0.25) {
				// (a shot with no new impact from GTA within 0.25 s (it hit nothing GTA has, or GTA gave none): aimed at you, it hits
				// as often as the shooter is accurate, if it's fighting you; still firing with none: one such shot every 0.25 s)
				S.pendT = shooting ? now : -1;
				const double acc = std::clamp(Call<int>(N::GET_PED_ACCURACY, p) / 100.0, 0.0, 1.0) * W.guess;
				const double h = std::max(0.8f, g_modelSize[2].load());
				const double C[3] = { d.carO[0] + d.carU[0] * h * 0.6, d.carO[1] + d.carU[1] * h * 0.6, d.carO[2] + d.carU[2] * h * 0.6 };
				double       B[3];
				for (int k = 0; k < 3; ++k) B[k] = A[k] + (C[k] - A[k]) * 1.5;
				if (Call<int>(N::IS_PED_IN_COMBAT, p, me) && double(rand()) / RAND_MAX < acc && WeaponsRay(d, A, B, 0.0, E, D)) {   // (only one fighting you)
					++W.guesses;
					WeaponsHit(E, D, B, wpn, true);
				}
			}
		}
	}

	// GTA's explosions near the car: where, which kind; BeamNG pushes its car, you get hurt by how close it was
	void WeaponsBlasts(double now)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		const double h = std::max(0.8f, g_modelSize[2].load());
		const double C[3] = { d.carO[0] + d.carU[0] * h * 0.5, d.carO[1] + d.carU[1] * h * 0.5, d.carO[2] + d.carU[2] * h * 0.5 };
		auto         any = [&](int type, const double* P, double r) { return Call<int>(N::IS_EXPLOSION_IN_SPHERE, type, float(P[0]), float(P[1]), float(P[2]), float(r)) != 0; };
		W.recent.erase(std::remove_if(W.recent.begin(), W.recent.end(), [&](const std::array<double, 4>& r) { return now - r[3] > 1.2; }), W.recent.end());
		if (!any(-1, C, 30.0)) return;
		// how far: the smallest sphere around the car's middle that has it
		double lo = 0, hi = 30;
		for (int i = 0; i < 9; ++i) {
			const double mid = (lo + hi) * 0.5;
			(any(-1, C, mid) ? hi : lo) = mid;
		}
		const double dist = hi;
		// which way: spheres at that distance around it (26 directions)
		double sum[3] = {};
		int    nIn = 0;
		for (int x = -1; x <= 1; ++x)
			for (int y = -1; y <= 1; ++y)
				for (int z = -1; z <= 1; ++z) {
					if (!x && !y && !z) continue;
					const double l = sqrt(double(x * x + y * y + z * z));
					const double P[3] = { C[0] + x / l * dist, C[1] + y / l * dist, C[2] + z / l * dist };
					if (any(-1, P, std::max(0.5, dist * 0.45))) {
						sum[0] += x / l, sum[1] += y / l, sum[2] += z / l;
						++nIn;
					}
				}
		const double sl = sqrt(sum[0] * sum[0] + sum[1] * sum[1] + sum[2] * sum[2]);
		const double u[3] = { nIn && sl > 1e-6 ? sum[0] / sl : 0.0, nIn && sl > 1e-6 ? sum[1] / sl : 0.0, nIn && sl > 1e-6 ? sum[2] / sl : -1.0 };
		const double P[3] = { C[0] + u[0] * dist, C[1] + u[1] * dist, C[2] + u[2] * dist };
		for (const auto& r : W.recent)
			if (hypot(P[0] - r[0], P[1] - r[1]) < 4.0 && fabs(P[2] - r[2]) < 4.0) return;  // (the same one, still in GTA's list)
		W.recent.push_back({ P[0], P[1], P[2], now });
		// which kind: v2.63: the strongest one there (your RPG shots were taken for the "car" explosion GTA makes of the hidden
		// car, next to the rocket's own, and only burned)
		int type = -1;
		for (const auto& kv : W.blasts)
			if ((type < 0 || kv.second.accel > W.blasts[type].accel) && (any(kv.first, P, std::max(1.5, dist * 0.6 + 0.5)) || any(kv.first, C, dist + 0.5))) type = kv.first;
		const WBlast& b = type >= 0 ? W.blasts[type] : W.other;
		const bool    boom = b.explodeR > 0 && dist <= b.explodeR;  // v2.63: close enough to blow the car up at once
		++W.blastsN;
		double Pb[3];
		DrvUnmapPoint(P, Pb);
		char m[320];
		snprintf(m, sizeof(m), "{\"t\":\"blast\",\"p\":[%.3f,%.3f,%.3f],\"a\":%.1f,\"r\":%.2f,\"dt\":%.3f,\"ig\":%.2f,\"gr\":%.1f,\"ty\":%d,\"xp\":%d}", Pb[0], Pb[1], Pb[2], b.accel, b.r, b.dt, b.ign,
			b.glassR, type, boom ? 1 : 0);
		DrvSend(m);
		Log("weapons: an explosion (%s, type %d) %.1f m from the car's middle at %.1f %.1f %.1f (%d of 26 directions saw it): to BeamNG%s", b.name.empty() ? "?" : b.name.c_str(), type, dist,
			P[0], P[1], P[2], nIn, boom ? " (close enough: the car blows up)" : "");
		if (!d.foot) WeaponsHurt(b.hurt * std::clamp(1.0 - dist / b.hurtR, 0.0, 1.0), "an explosion near the car");  // (on foot GTA's own explosion hurts you)
	}

	// hard crashes hurt you: the car's speed changing faster than any brake can
	void WeaponsCrash(double now)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		if (now < d.fallGraceT) {  // (put back after a fall or repaired: no crash)
			W.vh.clear();
			return;
		}
		// v2.7: your 2.63 log: each time you were hurt BeamNG's car really lost 8-10 m/s, but the hurt went by the change of its
		// velocity (a glance off a wall that turned it, or a bump of the ground, counted in full: 77 km/h "gone" for 32 km/h lost,
		// you died). Now: flat (the ground's bumps up and down don't count), the speed lost, or half the sideways change if that is
		// more (speeding up never counts); it must last 3 ticks (one bad reading doesn't), and impossible speeds are thrown away
		const double vx = d.carV[0], vy = d.carV[1], sp = sqrt(vx * vx + vy * vy);
		if (!std::isfinite(sp) || sp > 120.0) {
			W.vh.clear();
			W.crashRun = 0;
			return;
		}
		W.vh.push_back({ now, vx, vy, sp });
		while (!W.vh.empty() && now - W.vh.front()[0] > 0.25) W.vh.erase(W.vh.begin());
		double loss = 0, turn = 0;
		for (const auto& v : W.vh) {
			loss = std::max(loss, v[3] - sp);
			if (v[3] > 1.0) {  // (sideways to the way it went then: a turn; speeding up along it never counts)
				const double ux = v[1] / v[3], uy = v[2] / v[3], ax = vx - v[1], ay = vy - v[2], al = ax * ux + ay * uy;
				turn = std::max(turn, sqrt(std::max(0.0, ax * ax + ay * ay - al * al)));
			}
		}
		const double dv = std::max(loss, 0.5 * turn);
		W.crashRun = dv > W.crashDv ? W.crashRun + 1 : 0;
		if (W.crashRun >= 3 && now - W.crashT > 1.0) {
			W.crashT = now;
			char why[64];
			snprintf(why, sizeof(why), "a crash: %.0f km/h gone in a moment", dv * 3.6);
			WeaponsHurt((dv - W.crashDv) * W.crashHp, why);
		}
	}

	// ---------------------------------------------------------------------------------------
	// v2.7: traffic and pedestrians (your choice: GTA-side contact now, BeamNG proxy cars later). Every tick, GTA's cars and people
	// near the drawn car are boxes (their model's size, turned like them); one that overlaps the drawn car while they close in is a
	// hit: both get the push of a collision of their two masses (GTA's car: its velocity and GTA's damage at the spot; BeamNG's car:
	// its speed changes and it takes a dent there), people are knocked down and hurt, riders knocked off their bikes. The numbers
	// are in GtaBeamD1_traffic.json (written next to the ini the first time; Ctrl+F11 reads it again). TrafficCollision=0: off.
	// ---------------------------------------------------------------------------------------
	const char* kTrafficDefault = R"JSON({
  "about": "GTA x Beam v2.7 traffic: how GTA's cars and people and BeamNG's car push each other when they touch. Ctrl+F11 in game reads it again.",
  "rangeM": 40,
  "beamMassKg": 1500,
  "classMassKg": [1300, 1400, 1700, 2100, 1600, 1400, 1400, 1500, 250, 2000, 9000, 3000, 2500, 90, 1500, 5000, 5000, 3000, 2200, 6000, 9000, 20000, 1000],
  "bounce": 0.15,
  "pushGta": 1.0,
  "pushBeam": 1.0,
  "dentPerMs": 0.006,
  "dentMaxM": 0.12,
  "gtaDamagePerMs": 25,
  "pedKnockMs": 2.0,
  "pedHurtPerMs": 6,
  "pedThrow": 1.0,
  "pedSlowMs": 0.4,
  "riderKnockMs": 4.0,
  "cooldownS": 0.5,
  "twinsAbout": "v2.71 (TrafficTwins in the ini: how many): BeamNG cars that take a GTA car's place just before it meets yours, so the crash is BeamNG's (both soft body); GTA's car copies its twin and gets GTA damage where the twin is dented. twinPool: BeamNG's model names (vanilla), the GTA car gets the one closest in length; twinFallback: tried when one doesn't spawn. twinLeadS: how far ahead a meeting is seen (v2.73: 0.5, was 1.0: twins took cars that only passed close); twinDentScale: GTA's dent depth per metre of the twin's (v2.72: stepped until GTA shows it), twinDentMaxM: the deepest; twinStillS: a twin standing still this long goes back to GTA.",
  "twinPool": ["fullsize", "covet", "roamer", "pessima", "pickup", "van", "sunburst", "etk800"],
  "twinFallback": ["fullsize", "pickup", "covet"],
  "twinLeadS": 0.5,
  "twinDentScale": 1.0,
  "twinDentMaxM": 0.35,
  "twinStillS": 2.0
}
)JSON";
	struct TrafficState
	{
		bool                    cfgLoaded = false;
		std::string             cfgSource;
		double                  rangeM = 40, beamMass = 1500, bounce = 0.15, pushGta = 1, pushBeam = 1, dentPerMs = 0.006, dentMax = 0.12, gtaDmg = 25;
		double                  pedKnock = 2.0, pedHurt = 6, pedThrow = 1.0, pedSlow = 0.4, riderKnock = 4.0, cooldown = 0.5;
		double                  classMass[23] = { 1300, 1400, 1700, 2100, 1600, 1400, 1400, 1500, 250, 2000, 9000, 3000, 2500, 90, 1500, 5000, 5000, 3000, 2200, 6000, 9000, 20000, 1000 };
		std::vector<int>        buf;
		std::vector<int>        nearV, nearP;  // the cars / people within rangeM (rescanned every 10 ticks)
		std::map<int, double>   lastHit;
		uint32_t                tick = 0, carHits = 0, pedHits = 0, riders = 0, logs = 0;
		double                  logT = 0, dvSum = 0;
		// v2.71: twins
		struct Twin
		{
			int         state = 0;  // 0 asked for, 1 ready (parked in BeamNG), 2 on a GTA car, -1 failed
			std::string model;
			double      w = 0, l = 0, h = 0;
			int         ent = 0;
			double      tA = 0, tPose = 0, tStill = 0, dmax = 0;
			double      mc[3] = {}, mnz = 0, gw = 0, gl = 0, gh = 0;  // the GTA car's model: its middle, bottom, size
			bool        posed = false;
			double      V[3] = {};
			uint32_t    poses = 0;
			std::map<int, double> applied;  // its dents given to GTA's car (by 0.5 m cell)
		};
		std::vector<Twin>       twins;
		bool                    twAsked = false;
		double                  twAskT = 0;
		std::string             twPool[8], twFb[3];
		int                     twPoolN = 0, twFbN = 0;
		double                  twLead = 0.5, twStill = 2.0, twDentScale = 1.0, twDentMax = 0.35;
		struct GtaDent
		{
			int    veh;
			float  x, y, z;
			double want, dmg;
			int    steps;
			double got3;  // v2.73: GTA's deformation there after 3 steps
		};
		std::vector<GtaDent>    gtaDents;  // v2.72: GTA dents being made (stepped), by car
		struct PedPush
		{
			int   ent;
			float vx, vy, vz;
			int   ticks;
		};
		std::vector<PedPush>    pedPush;   // v2.72: knocked people's speed, set on the next ticks
		struct Watch
		{
			int         ent;
			const char* what;
			double      t0, vn, p0[3];
			int         step;
		};
		std::vector<Watch>      watch;     // v2.72: a person / bike hit: what became of it 0.5 s and 1.5 s later (log)
		std::vector<std::pair<int, double>> held;  // v2.73: knocked people kept from GTA's clean-up (until when)
		uint32_t                watchLogs = 0;
		uint32_t                dentLogs = 0;
		std::map<int, double>   twDone;  // GTA cars a twin just left (not twinned again, not pushed the 2.7 way for 2 s)
		uint32_t                twActs = 0, twRel = 0, twDents = 0, twFails = 0, twPoses = 0;
	};
	TrafficState g_traffic;
	double       g_twLoadT = -100;  // v2.72: the start waited for the twins then (BeamNG freezes while it spawns one)

	void LoadTraffic()
	{
		TrafficState& T = g_traffic;
		char          path[MAX_PATH + 16];
		PathNextToModule("_traffic.json", path, sizeof(path));
		std::string js;
		if (FILE* f = fopen(path, "rb")) {
			char   b[4096];
			size_t n;
			while ((n = fread(b, 1, sizeof(b), f)) > 0) js.append(b, n);
			fclose(f);
			T.cfgSource = path;
		} else {
			js = kTrafficDefault;
			T.cfgSource = "built-in numbers";
			if (FILE* o = fopen(path, "wb")) {
				fputs(kTrafficDefault, o);
				fclose(o);
				T.cfgSource = std::string("built-in numbers (written to ") + path + ")";
			}
		}
		const char* s = js.c_str();
		double      v;
		auto        num = [&](const char* k, double& out, double lo, double hi) {
            if (JsonNumC(s, k, v)) out = std::clamp(v, lo, hi);
		};
		num("rangeM", T.rangeM, 5, 120);
		num("beamMassKg", T.beamMass, 100, 50000);
		num("bounce", T.bounce, 0, 1);
		num("pushGta", T.pushGta, 0, 5);
		num("pushBeam", T.pushBeam, 0, 5);
		num("dentPerMs", T.dentPerMs, 0, 0.05);
		num("dentMaxM", T.dentMax, 0, 0.3);
		num("gtaDamagePerMs", T.gtaDmg, 0, 500);
		num("pedKnockMs", T.pedKnock, 0, 30);
		num("pedHurtPerMs", T.pedHurt, 0, 100);
		num("pedThrow", T.pedThrow, 0, 5);
		num("pedSlowMs", T.pedSlow, 0, 5);
		num("riderKnockMs", T.riderKnock, 0, 30);
		num("cooldownS", T.cooldown, 0.1, 5);
		double cm[23];
		const int n = JsonNums(s, "classMassKg", cm, 23);
		for (int i = 0; i < n; ++i) T.classMass[i] = std::clamp(cm[i], 20.0, 100000.0);
		// v2.71: twins (an older file without them: the built-in pool)
		T.twPoolN = std::max(0, JsonStrs(js, "twinPool", T.twPool, 8));
		if (T.twPoolN == 0) T.twPoolN = std::max(0, JsonStrs(kTrafficDefault, "twinPool", T.twPool, 8));
		T.twFbN = std::max(0, JsonStrs(js, "twinFallback", T.twFb, 3));
		if (T.twFbN == 0) T.twFbN = std::max(0, JsonStrs(kTrafficDefault, "twinFallback", T.twFb, 3));
		num("twinLeadS", T.twLead, 0.1, 2.0);
		num("twinDentScale", T.twDentScale, 0, 5);
		num("twinDentMaxM", T.twDentMax, 0.02, 1.0);
		num("twinStillS", T.twStill, 0.5, 20);
		T.cfgLoaded = true;
		Log("traffic (v2.7): %s; range %.0f m, BeamNG's car taken as %.0f kg, pushes x%.1f (GTA) / x%.1f (BeamNG), dents %.1f mm per m/s (max %.0f mm)%s", T.cfgSource.c_str(), T.rangeM,
			T.beamMass, T.pushGta, T.pushBeam, T.dentPerMs * 1000, T.dentMax * 1000, g_cfg.trafficCollision ? "" : "; TrafficCollision=0: off");
	}

	// ---------------------------------------------------------------------------------------
	// v2.71: traffic twins (your v2.7 test: GTA's cars vanished or didn't dent; you chose "BeamNG twins, GTA look"). The BeamNG
	// mod keeps a pool of TrafficTwins BeamNG cars, parked (not drawn, not simulated). A GTA car about to meet yours (their boxes
	// overlap within twinLeadS at their speeds) gets the free twin closest to it in length: the twin is put where it is, with its
	// speed, and BeamNG's physics take over (the crash is soft body against soft body). GTA's car copies its twin's pose every
	// tick and gets GTA's damage where the twin is dented. Standing still for twinStillS, far, gone or 30 s on: back to GTA.
	// A GTA car with no twin free gets v2.7's push.
	// ---------------------------------------------------------------------------------------
	void TwinRelease(TrafficState& T, int k, const char* why)
	{
		TrafficState::Twin& tw = T.twins[k];
		if (tw.state != 2) return;
		char m[128];
		snprintf(m, sizeof(m), "{\"t\":\"twr\",\"k\":%d,\"why\":\"%s\"}", k + 1, why);
		DrvSend(m);
		if (tw.ent && Call<int>(N::DOES_ENTITY_EXIST, tw.ent)) {
			if (tw.posed) CallVoid(N::SET_ENTITY_VELOCITY, tw.ent, float(tw.V[0]), float(tw.V[1]), float(tw.V[2]));
			int h = tw.ent;
			CallVoid(N::SET_VEHICLE_AS_NO_LONGER_NEEDED, &h);
		}
		T.twDone[tw.ent] = NowSeconds() + (tw.dmax < 0.01 ? 4.0 : 0.0);  // (not twinned again for 2 s; 6 s after a twin with no crash)
		if (T.logs++ < 80)
			Log("traffic twins: GTA car %d back to GTA (%s; after %.1f s, %u poses copied, its twin dented up to %.0f mm)", tw.ent, why, NowSeconds() - tw.tA, tw.poses, tw.dmax * 1000);
		tw.state = 1;
		tw.ent = 0;
		tw.posed = false;
		tw.applied.clear();
		tw.dmax = tw.tStill = 0;
		tw.poses = 0;
		++T.twRel;
	}
	void TwinsEnd(TrafficState& T, const char* why)
	{
		for (size_t k = 0; k < T.twins.size(); ++k) TwinRelease(T, int(k), why);
		T.twins.clear();
		T.twAsked = false;
	}
	// v2.72: asked when the drive starts (BeamNG agreed), before its car is put on GTA's ground: spawning a twin freezes BeamNG
	// for 2-3 s (your v2.71 log: 3.2 s), harmless while the car waits there, not while driving. BeamNG keeps them parked
	// between drives: only the first F10 of a BeamNG session waits.
	void TwinsAsk(TrafficState& T)
	{
		if (T.twAsked || !g_cfg.trafficCollision || g_cfg.trafficTwins <= 0) return;
		if (!T.cfgLoaded) LoadTraffic();
		if (T.twPoolN <= 0) return;
		T.twAsked = true;
		T.twAskT = NowSeconds();
		T.twins.assign(size_t(g_cfg.trafficTwins), TrafficState::Twin{});
		std::string m = "{\"t\":\"twp\",\"m\":[", fb;
		for (int k = 0; k < g_cfg.trafficTwins; ++k) m += std::string(k ? "," : "") + "\"" + T.twPool[k % T.twPoolN] + "\"";
		for (int k = 0; k < T.twFbN; ++k) fb += std::string(k ? "," : "") + "\"" + T.twFb[k] + "\"";
		m += "],\"fb\":[" + fb + "]}";
		DrvSend(m.c_str());
		Log("traffic twins: asked BeamNG for %d twins (%s)", g_cfg.trafficTwins, m.c_str());
	}
	void TwinsMsgs(TrafficState& T);
	// still being spawned (the start waits for them, at most 25 s)
	bool TwinsLoading(TrafficState& T)
	{
		if (!T.twAsked || T.twins.empty()) return false;
		TwinsMsgs(T);
		bool waiting = false;
		for (const auto& tw : T.twins) waiting = waiting || tw.state == 0;
		return waiting && NowSeconds() - T.twAskT < 25.0;
	}
	// v2.72: GTA's own dent at a spot of its car (model frame), as deep as its twin's there. v2.71 called SET_VEHICLE_DAMAGE once
	// with a 0.5-1.5 radius: nothing showed (your test). It is stepped now, one call per car per tick, damage = radius from 50 up by
	// 15, until GTA's deformation there (GET_VEHICLE_DEFORMATION_AT_POS) is the twin's dent x twinDentScale (v2.73: capped, below).
	// The spot is moved 10% out from the car's middle (the damage sphere travels in from there onto the body).
	void GtaDentAdd(TrafficState& T, int veh, double x, double y, double z, double depth)
	{
		const double want = std::min(T.twDentMax, depth * T.twDentScale);
		for (auto& g : T.gtaDents)
			if (g.veh == veh && fabs(g.x - x) < 0.4 && fabs(g.y - y) < 0.4 && fabs(g.z - z) < 0.4) {
				g.want = std::max(g.want, want);
				return;
			}
		if (T.gtaDents.size() < 64) T.gtaDents.push_back({ veh, float(x), float(y), float(z), want, 50.0, 0, -1.0 });
	}
	void GtaDentsTick(TrafficState& T)
	{
		std::set<int> done;  // (one call per car per tick)
		for (size_t i = 0; i < T.gtaDents.size();) {
			auto& g = T.gtaDents[i];
			if (done.count(g.veh)) {
				++i;
				continue;
			}
			const bool alive = Call<int>(N::DOES_ENTITY_EXIST, g.veh) != 0;
			NVec3      dv{};
			if (alive) dv = Call<NVec3>(N::GET_VEHICLE_DEFORMATION_AT_POS, g.veh, g.x, g.y, g.z);
			const double got = sqrt(double(dv.x) * dv.x + double(dv.y) * dv.y + double(dv.z) * dv.z);
			// v2.73: your v2.72 log: where GTA's body doesn't dent (the roof, under the car) the readback stayed at 10-20 mm and the 40 steps
			// went up to 650 (a small crash crushed the whole GTA car: each step is a hit of its own). Now at most 110 + 300 x the depth
			// wanted (125 for 5 cm, 215 for 35 cm: what your log's dents that showed needed) and 10 steps; stopped when 3 more steps
			// added under 5 mm
			if (g.steps == 3) g.got3 = got;
			const bool stuck = g.steps >= 6 && g.got3 >= 0 && got - g.got3 < 0.005;
			if (!alive || got >= g.want || g.steps >= 10 || stuck || g.dmg > 110.0 + 300.0 * g.want) {
				if (alive && T.dentLogs++ < 30)
					Log("traffic twins: GTA car %d dented at %.1f %.1f %.1f: wanted %.0f mm, GTA shows %.0f mm after %d steps (damage %.0f)%s", g.veh, g.x, g.y, g.z, g.want * 1000, got * 1000,
						g.steps, g.dmg, stuck ? ": GTA's body doesn't dent more there" : "");
				T.gtaDents.erase(T.gtaDents.begin() + i);
				continue;
			}
			CallVoid(N::SET_VEHICLE_DAMAGE, g.veh, g.x * 1.1f, g.y * 1.1f, g.z, float(g.dmg), float(g.dmg), 1);
			g.dmg += 15.0;
			++g.steps;
			done.insert(g.veh);
			++i;
		}
	}

	// v2.72: your v2.71 test: people and bikes "vanish in place" after a hit. Each hit is watched: 0.5 s and 1.5 s later the log
	// says whether it still exists, is visible, how far it moved, its speed, ragdoll, dead (the next fix needs that)
	// v2.73: your v2.72 log: people hit "exists 0" 0.5 s later (GTA took them away while they flew). A knocked person is the
	// plugin's for 6 s (a mission entity: GTA's clean-up leaves it), then GTA's again. Never one that was already a mission's
	void TrafficHold(TrafficState& T, int e)
	{
		for (const auto& h : T.held)
			if (h.first == e) return;
		if (Call<int>(N::IS_ENTITY_A_MISSION_ENTITY, e)) return;
		CallVoid(N::SET_ENTITY_AS_MISSION_ENTITY, e, 0, 1);
		T.held.push_back({ e, NowSeconds() + 6.0 });
	}
	void TrafficHeldTick(TrafficState& T, bool all)
	{
		const double now = NowSeconds();
		for (size_t i = 0; i < T.held.size();) {
			if (!all && now < T.held[i].second) {
				++i;
				continue;
			}
			int h = T.held[i].first;
			if (Call<int>(N::DOES_ENTITY_EXIST, h)) CallVoid(N::SET_PED_AS_NO_LONGER_NEEDED, &h);
			T.held.erase(T.held.begin() + i);
		}
	}
	void TrafficWatch(TrafficState& T, int e, const char* what, double vn)
	{
		if (T.watchLogs >= 40 || T.watch.size() >= 8) return;
		const NVec3 p = Call<NVec3>(N::GET_ENTITY_COORDS, e, 0);
		T.watch.push_back({ e, what, NowSeconds(), vn, { p.x, p.y, p.z }, 0 });
	}
	void TrafficWatchTick(TrafficState& T)
	{
		for (auto& pp : T.pedPush) {
			if (Call<int>(N::DOES_ENTITY_EXIST, pp.ent)) CallVoid(N::SET_ENTITY_VELOCITY, pp.ent, pp.vx, pp.vy, pp.vz);
			--pp.ticks;
		}
		T.pedPush.erase(std::remove_if(T.pedPush.begin(), T.pedPush.end(), [](const TrafficState::PedPush& q) { return q.ticks <= 0; }), T.pedPush.end());
		const double now = NowSeconds();
		for (size_t i = 0; i < T.watch.size();) {
			auto&        w = T.watch[i];
			const double at = w.step == 0 ? 0.5 : 1.5;
			if (now - w.t0 < at) {
				++i;
				continue;
			}
			const bool ex = Call<int>(N::DOES_ENTITY_EXIST, w.ent) != 0;
			NVec3      p{}, v{};
			if (ex) p = Call<NVec3>(N::GET_ENTITY_COORDS, w.ent, 0), v = Call<NVec3>(N::GET_ENTITY_VELOCITY, w.ent);
			const bool ped = !strcmp(w.what, "person");
			if (T.watchLogs++ < 40)
				Log("traffic: the %s hit at %.0f km/h, %.1f s later: exists %d, visible %d, moved %.1f m, %.1f m/s, %.1f m over the ground%s%s", w.what, w.vn * 3.6, at, int(ex),
					ex ? Call<int>(N::IS_ENTITY_VISIBLE, w.ent) : 0, ex ? sqrt((p.x - w.p0[0]) * (p.x - w.p0[0]) + (p.y - w.p0[1]) * (p.y - w.p0[1]) + (p.z - w.p0[2]) * (p.z - w.p0[2])) : 0.0,
					ex ? sqrt(v.x * v.x + v.y * v.y + v.z * v.z) : 0.0, ex ? Call<float>(N::GET_ENTITY_HEIGHT_ABOVE_GROUND, w.ent) : 0.0f, ped && ex ? (Call<int>(N::IS_PED_RAGDOLL, w.ent) ? ", ragdoll" : ", not ragdoll") : "",
					ped && ex && Call<int>(N::IS_PED_DEAD_OR_DYING, w.ent, 1) ? ", dead" : "");
			if (++w.step >= 2) T.watch.erase(T.watch.begin() + i);
			else ++i;
		}
	}

	// BeamNG's answers: ready ("twi"), poses ("twu": the last one per twin is copied), dents ("twd"), a failed put ("twf")
	void TwinsMsgs(TrafficState& T)
	{
		std::vector<std::string> q;
		AcquireSRWLockExclusive(&g_dnet.lock);
		q.swap(g_dnet.twQ);
		ReleaseSRWLockExclusive(&g_dnet.lock);
		std::vector<std::array<double, 13>> last(T.twins.size());
		std::vector<bool>                   have(T.twins.size(), false);
		const double                        now = NowSeconds();
		for (const std::string& s : q) {
			std::string ty;
			JsonStr(s, "t", ty);
			if (ty == "twu") {
				const size_t a = s.find('[');
				if (a == std::string::npos) continue;
				const char* p = s.c_str() + a + 1;
				while (*p && *p != ']') {
					std::array<double, 13> v{};
					int                    n = 0;
					for (; n < 13; ++n) {
						char* e = nullptr;
						v[n] = strtod(p, &e);
						if (e == p) break;
						p = e;
						while (*p == ',' || *p == ' ') ++p;
					}
					if (n < 13) break;
					const int k = int(v[0]) - 1;
					if (k >= 0 && k < int(T.twins.size())) last[k] = v, have[k] = true;
				}
				continue;
			}
			const int k = int(JsonNum(s, "k", 0)) - 1;
			if (k < 0 || k >= int(T.twins.size())) continue;
			TrafficState::Twin& tw = T.twins[k];
			if (ty == "twi") {
				if (JsonNum(s, "ok", 0) > 0.5) {
					JsonStr(s, "model", tw.model);
					tw.w = JsonNum(s, "w", 0);
					tw.l = JsonNum(s, "l", 0);
					tw.h = JsonNum(s, "h", 0);
					if (tw.state == 0) tw.state = 1;
					Log("traffic twins: twin %d ready: a %s (%.1f x %.1f x %.1f m)", k + 1, tw.model.c_str(), tw.w, tw.l, tw.h);
				} else {
					std::string why;
					JsonStr(s, "why", why);
					if (tw.state == 2) TwinRelease(T, k, "failed");
					tw.state = -1;
					++T.twFails;
					Log("traffic twins: twin %d failed in BeamNG (%s): GTA cars get v2.7's push instead", k + 1, why.c_str());
				}
			} else if (ty == "twf") {
				std::string why;
				JsonStr(s, "why", why);
				++T.twFails;
				if (T.logs++ < 80) Log("traffic twins: twin %d couldn't be put on GTA's car (%s)", k + 1, why.c_str());
				if (tw.state == 2) {
					T.twDone[tw.ent] = now;
					tw.state = 1;
					tw.ent = 0;
				}
			} else if (ty == "twd" && tw.state == 2 && tw.ent && tw.w > 0.1 && tw.l > 0.1 && tw.h > 0.1) {
				// its dents (its frame from its footprint's middle, bottom) -> GTA's car's model frame, scaled by the two sizes
				double sp[24];
				const int ns = JsonNums(s.c_str(), "s", sp, 24) / 4;
				for (int i = 0; i < ns; ++i) {
					const double x = tw.mc[0] + sp[i * 4] * tw.gw / tw.w, y = tw.mc[1] + sp[i * 4 + 1] * tw.gl / tw.l, z = tw.mnz + sp[i * 4 + 2] * tw.gh / tw.h;
					const double dd = sp[i * 4 + 3];
					const int    cell = (int(floor(x * 2)) + 64) * 65536 + (int(floor(y * 2)) + 64) * 256 + (int(floor(z * 2)) + 64);
					double&      ap = tw.applied[cell];
					tw.dmax = std::max(tw.dmax, dd);
					if (dd < ap + 0.015) continue;
					ap = dd;
					GtaDentAdd(T, tw.ent, x, y, z, dd);
					++T.twDents;
				}
			}
		}
		// GTA's car where its twin is (its model's footprint middle, bottom = the twin's)
		for (size_t k = 0; k < T.twins.size(); ++k) {
			TrafficState::Twin& tw = T.twins[k];
			if (!have[k] || tw.state != 2 || !tw.ent || !Call<int>(N::DOES_ENTITY_EXIST, tw.ent)) continue;
			const auto&  v = last[k];
			const double Pb[3] = { v[1], v[2], v[3] }, Fb[3] = { v[4], v[5], v[6] }, Ub[3] = { v[7], v[8], v[9] }, Vb[3] = { v[10], v[11], v[12] };
			double       P[3], F[3], U[3], V[3], R[3];
			DrvMapPoint(Pb, P);
			DrvMapDir(Fb, F);
			DrvMapDir(Ub, U);
			DrvMapDir(Vb, V);
			Orthonormal(F, U);
			Cross3(F, U, R);
			double O[3];
			for (int i = 0; i < 3; ++i) O[i] = P[i] - (R[i] * tw.mc[0] + F[i] * tw.mc[1] + U[i] * tw.mnz);
			CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, tw.ent, float(O[0]), float(O[1]), float(O[2]), 0, 0, 0);
			CallVoid(N::SET_ENTITY_ROTATION, tw.ent, float(asin(std::clamp(F[2], -1.0, 1.0)) / kDeg), float(atan2(-R[2], U[2]) / kDeg), float(atan2(-F[0], F[1]) / kDeg), 2, 1);
			CallVoid(N::SET_ENTITY_VELOCITY, tw.ent, float(V[0]), float(V[1]), float(V[2]));
			memcpy(tw.V, V, sizeof(tw.V));
			tw.posed = true;
			tw.tPose = now;
			++tw.poses;
			++T.twPoses;
			const double sp = sqrt(V[0] * V[0] + V[1] * V[1] + V[2] * V[2]);
			if (sp > 0.7) tw.tStill = 0;
			else if (tw.tStill == 0) tw.tStill = now;
			// the twin fell through a gap in GTA's ground in BeamNG: GTA's car stays on GTA's ground, back to GTA
			double gz = 0;
			if (g_cfg.world && WorldGroundAt(P[0], P[1], gz) && P[2] < gz - 1.0) {  // (v2.72: 1 m, was 2.5: GTA's car went under with it and vanished)
				CallVoid(N::SET_ENTITY_COORDS_NO_OFFSET, tw.ent, float(O[0]), float(O[1]), float(gz - tw.mnz + 0.3), 0, 0, 0);
				TwinRelease(T, int(k), "its twin fell through GTA's ground in BeamNG");
			}
		}
	}

	// two flat boxes (centre, right, forward, half sizes) overlap? (separating axes)
	bool FlatBoxes(const double ca[2], const double ra[2], const double fa[2], double wa, double la, const double cb[2], const double rb[2], const double fb[2], double wb, double lb)
	{
		const double* axes[4] = { ra, fa, rb, fb };
		const double  dx = cb[0] - ca[0], dy = cb[1] - ca[1];
		for (const double* a : axes) {
			const double dist = fabs(dx * a[0] + dy * a[1]);
			const double r1 = wa * fabs(ra[0] * a[0] + ra[1] * a[1]) + la * fabs(fa[0] * a[0] + fa[1] * a[1]);
			const double r2 = wb * fabs(rb[0] * a[0] + rb[1] * a[1]) + lb * fabs(fb[0] * a[0] + fb[1] * a[1]);
			if (dist > r1 + r2) return false;
		}
		return true;
	}

	void TrafficTick(bool paused)
	{
		TrafficState& T = g_traffic;
		DriveState&   d = g_drv;
		if (d.state == 0 && !T.twins.empty()) TwinsEnd(T, "drive mode off");  // v2.71 (v2.72: not while the drive starts)
		if (!T.held.empty()) TrafficHeldTick(T, d.state != 2);                  // v2.73
		if (!g_cfg.trafficCollision || d.state != 2 || !d.fixedMap || !d.carPoseOk || paused) return;
		if (!T.cfgLoaded) LoadTraffic();
		const double now = NowSeconds();
		const int    me = Call<int>(N::PLAYER_PED_ID);
		TwinsAsk(T);  // (asked when the drive started: v2.72)
		if (!T.twins.empty()) TwinsMsgs(T);
		GtaDentsTick(T);  // v2.72
		TrafficWatchTick(T);
		// the cars and people near the car (the pools every 10 ticks)
		if (T.tick++ % 10 == 0) {
			T.buf.resize(1024);
			T.nearV.clear();
			T.nearP.clear();
			const double R2 = T.rangeM * T.rangeM;
			auto isNear = [&](int e) {
				const NVec3  p = Call<NVec3>(N::GET_ENTITY_COORDS, e, 0);
				const double dx = p.x - d.carO[0], dy = p.y - d.carO[1];
				return dx * dx + dy * dy < R2 && fabs(p.z - d.carO[2]) < 15.0;
			};
			int n = std::clamp(worldGetAllVehicles(T.buf.data(), int(T.buf.size())), 0, int(T.buf.size()));
			for (int q = 0; q < n; ++q) {
				const int e = T.buf[q];
				if (e && e != d.veh && e != g_tgt.veh && Call<int>(N::DOES_ENTITY_EXIST, e) && isNear(e)) T.nearV.push_back(e);
			}
			n = std::clamp(worldGetAllPeds(T.buf.data(), int(T.buf.size())), 0, int(T.buf.size()));
			for (int q = 0; q < n; ++q) {
				const int e = T.buf[q];
				if (e && e != me && Call<int>(N::DOES_ENTITY_EXIST, e) && !Call<int>(N::IS_PED_IN_ANY_VEHICLE, e, 0) && isNear(e)) T.nearP.push_back(e);
			}
		}
		const double w = std::max(1.0f, g_modelSize[0].load()), l = std::max(2.0f, g_modelSize[1].load()), h = std::max(0.8f, g_modelSize[2].load());
		const double cc[3] = { d.carO[0] + d.carU[0] * h * 0.5, d.carO[1] + d.carU[1] * h * 0.5, d.carO[2] + h * 0.5 };
		// the drawn car's surface towards a direction (flat), for the dent and GTA's damage
		auto edge = [&](const double n[2]) {
			const double fr = sqrt(d.carR[0] * d.carR[0] + d.carR[1] * d.carR[1]), ff = sqrt(d.carF[0] * d.carF[0] + d.carF[1] * d.carF[1]);
			const double ar = fr > 1e-6 ? fabs((d.carR[0] * n[0] + d.carR[1] * n[1]) / fr) : 0, af = ff > 1e-6 ? fabs((d.carF[0] * n[0] + d.carF[1] * n[1]) / ff) : 1;
			return std::min(ar > 1e-6 ? w * 0.5 / ar : 1e9, af > 1e-6 ? l * 0.5 / af : 1e9);
		};
		auto send = [&](const double dvB[3], const double P[3], const double n[2], double dent, const char* who) {
			double vb[3], pb[3], db[3];
			const double dir[3] = { -n[0], -n[1], 0 };  // into the car
			DrvUnmapDir(dvB, vb);
			DrvUnmapPoint(P, pb);
			DrvUnmapDir(dir, db);
			char m[360];
			snprintf(m, sizeof(m), "{\"t\":\"tc\",\"dv\":[%.3f,%.3f,%.3f],\"p\":[%.3f,%.3f,%.3f],\"d\":[%.4f,%.4f,%.4f],\"dent\":%.3f,\"w\":\"%s\"}", vb[0], vb[1], vb[2], pb[0], pb[1], pb[2],
				db[0], db[1], db[2], dent, who);
			DrvSend(m);
		};
		auto touching = [&](int e, const double c[3], const double ax[3][3], const double he[3], double n[2], double& vn, NVec3& ve) {
			PropLive L;
			memcpy(L.c, c, sizeof(L.c));
			memcpy(L.ax, ax, sizeof(L.ax));
			memcpy(L.he, he, sizeof(L.he));
			if (!PropTouch(d, L, w, l, h)) return false;
			auto lh = T.lastHit.find(e);
			if (lh != T.lastHit.end() && now - lh->second < T.cooldown) return false;
			const double dx = c[0] - cc[0], dy = c[1] - cc[1], dl = sqrt(dx * dx + dy * dy);
			if (dl < 1e-3) return false;
			n[0] = dx / dl;
			n[1] = dy / dl;
			ve = Call<NVec3>(N::GET_ENTITY_VELOCITY, e);
			vn = (d.carV[0] - ve.x) * n[0] + (d.carV[1] - ve.y) * n[1];  // closing speed
			return vn > 0.5;
		};
		for (int e : T.nearV) {
			if (!Call<int>(N::DOES_ENTITY_EXIST, e)) continue;
			const uint32_t model = uint32_t(Call<int>(N::GET_ENTITY_MODEL, e));
			NVec3          mn{}, mx{}, fw{}, rt{}, up{}, ps{};
			CallVoid(N::GET_MODEL_DIMENSIONS, model, &mn, &mx);
			CallVoid(N::GET_ENTITY_MATRIX, e, &fw, &rt, &up, &ps);
			const double ax[3][3] = { { rt.x, rt.y, rt.z }, { fw.x, fw.y, fw.z }, { up.x, up.y, up.z } };
			const double mc[3] = { (mn.x + mx.x) * 0.5, (mn.y + mx.y) * 0.5, (mn.z + mx.z) * 0.5 };
			double       c[3], he[3] = { (mx.x - mn.x) * 0.5, (mx.y - mn.y) * 0.5, (mx.z - mn.z) * 0.5 };
			const double pp[3] = { ps.x, ps.y, ps.z };
			for (int k = 0; k < 3; ++k) c[k] = pp[k] + ax[0][k] * mc[0] + ax[1][k] * mc[1] + ax[2][k] * mc[2];
			if (!T.twins.empty()) {
				// v2.71: about to meet the car (their boxes, 15 cm bigger (v2.73; was 25), at their speeds within twinLeadS) and not too high or
				// low: a twin. v2.73: your v2.72 test, "random forces as I pass": most twins "didn't meet" (seen 1 s ahead with 25 cm to spare,
				// the cars only passed close); for those 0.8 s GTA's car was BeamNG's coasting twin (no driver), then let go
				const NVec3  ve0 = Call<NVec3>(N::GET_ENTITY_VELOCITY, e);
				const int    cls = std::clamp(Call<int>(N::GET_VEHICLE_CLASS, e), 0, 22);
				const bool   kind = cls != 8 && cls != 13 && cls != 14 && cls != 15 && cls != 16 && cls != 21;  // (not bikes, boats, aircraft, trains)
				auto         flat = [](const double* v, double* o) {
                    const double l2 = sqrt(v[0] * v[0] + v[1] * v[1]);
                    o[0] = l2 > 1e-6 ? v[0] / l2 : 1.0;
                    o[1] = l2 > 1e-6 ? v[1] / l2 : 0.0;
				};
				double ra[2], fa[2], rb[2], fb[2];
				flat(d.carR, ra);
				flat(d.carF, fa);
				flat(ax[0], rb);
				flat(ax[1], fb);
				double meetT = -1;
				if (kind && fabs(c[2] - cc[2]) < 3.0) {
					for (int st = 0; st <= 4 && meetT < 0; ++st) {
						const double tt = T.twLead * st / 4.0;
						const double ca[2] = { cc[0] + d.carV[0] * tt, cc[1] + d.carV[1] * tt }, cb[2] = { c[0] + ve0.x * tt, c[1] + ve0.y * tt };
						if (FlatBoxes(ca, ra, fa, w * 0.5 + 0.15, l * 0.5 + 0.15, cb, rb, fb, he[0] + 0.15, he[1] + 0.15)) meetT = tt;
					}
				}
				// twinned (BeamNG has it: back to GTA when they won't meet after all), or a twin just left it (no 2.7 push on top)
				int twK = -1;
				for (size_t k = 0; k < T.twins.size(); ++k)
					if (T.twins[k].state == 2 && T.twins[k].ent == e) twK = int(k);
				if (twK >= 0 && meetT < 0 && T.twins[twK].dmax < 0.01 && now - T.twins[twK].tA > T.twLead + 0.3) TwinRelease(T, twK, "they didn't meet");
				auto dn = T.twDone.find(e);
				if (twK >= 0 || (dn != T.twDone.end() && now - dn->second < 2.0)) continue;
				int best = -1;
				for (size_t k = 0; meetT >= 0 && k < T.twins.size(); ++k)
					if (T.twins[k].state == 1 && (best < 0 || fabs(T.twins[k].l - 2 * he[1]) < fabs(T.twins[best].l - 2 * he[1]))) best = int(k);
				// v2.72: the twin (its own size) must fit where GTA's car is: not into your car or another twin (your v2.71 test: GTA cars
				// crushed "by your presence": a twin put half inside your car is torn apart by BeamNG). Too late to fit: v2.7's push
				if (best >= 0) {
					const double hw = T.twins[best].w * 0.5, hl = T.twins[best].l * 0.5, cb0[2] = { c[0], c[1] };
					bool         clash = FlatBoxes(cc, ra, fa, w * 0.5 + 0.1, l * 0.5 + 0.1, cb0, rb, fb, hw, hl);
					for (const auto& o : T.twins) {
						if (clash || o.state != 2 || !o.ent || !Call<int>(N::DOES_ENTITY_EXIST, o.ent)) continue;
						NVec3 ofw{}, ort{}, oup{}, ops{};
						CallVoid(N::GET_ENTITY_MATRIX, o.ent, &ofw, &ort, &oup, &ops);
						const double oc[2] = { ops.x + ort.x * o.mc[0] + ofw.x * o.mc[1], ops.y + ort.y * o.mc[0] + ofw.y * o.mc[1] };
						const double orr[3] = { ort.x, ort.y, ort.z }, off[3] = { ofw.x, ofw.y, ofw.z };
						double       ro[2], fo[2];
						flat(orr, ro);
						flat(off, fo);
						clash = FlatBoxes(oc, ro, fo, o.w * 0.5 + 0.05, o.l * 0.5 + 0.05, cb0, rb, fb, hw, hl);
					}
					if (clash) {
						if (T.logs++ < 80) Log("traffic twins: GTA car %d is already too close for its twin to fit (%s): v2.7's push for it", e, T.twins[best].model.c_str());
						T.twDone[e] = now + 3.0;
						best = -1;
					}
				}
				if (best >= 0) {
					TrafficState::Twin& tw = T.twins[best];
					double Pg[3], Pb[3], Fb[3], Ub[3], Vb[3];
					for (int k = 0; k < 3; ++k) Pg[k] = pp[k] + ax[0][k] * mc[0] + ax[1][k] * mc[1] + ax[2][k] * mn.z;
					const double fg[3] = { fw.x, fw.y, fw.z }, ug[3] = { up.x, up.y, up.z }, vg[3] = { ve0.x, ve0.y, ve0.z };
					DrvUnmapPoint(Pg, Pb);
					DrvUnmapDir(fg, Fb);
					DrvUnmapDir(ug, Ub);
					DrvUnmapDir(vg, Vb);
					const double spd = sqrt(ve0.x * ve0.x + ve0.y * ve0.y + ve0.z * ve0.z);
					const int    pk = spd < 0.5 && !Call<int>(N::GET_PED_IN_VEHICLE_SEAT, e, -1, 0) ? 1 : 0;
					char         m[420];
					snprintf(m, sizeof(m), "{\"t\":\"twa\",\"k\":%d,\"id\":%d,\"p\":[%.3f,%.3f,%.3f],\"f\":[%.5f,%.5f,%.5f],\"u\":[%.5f,%.5f,%.5f],\"v\":[%.3f,%.3f,%.3f],\"pk\":%d}",
						best + 1, e, Pb[0], Pb[1], Pb[2], Fb[0], Fb[1], Fb[2], Ub[0], Ub[1], Ub[2], Vb[0], Vb[1], Vb[2], pk);
					DrvSend(m);
					CallVoid(N::SET_ENTITY_AS_MISSION_ENTITY, e, 0, 1);  // (GTA doesn't take it away while BeamNG has it)
					tw.state = 2;
					tw.ent = e;
					tw.tA = now;
					tw.tPose = tw.tStill = tw.dmax = 0;
					tw.posed = false;
					tw.poses = 0;
					tw.applied.clear();
					memcpy(tw.mc, mc, sizeof(tw.mc));
					tw.mnz = mn.z;
					tw.gw = mx.x - mn.x;
					tw.gl = mx.y - mn.y;
					tw.gh = mx.z - mn.z;
					++T.twActs;
					if (T.logs++ < 80)
						Log("traffic twins: GTA car %d (class %d, %.1f m long, %.0f km/h%s) meets the car in %.2f s: twin %d (%s, %.1f m) takes its place", e, cls, tw.gl, spd * 3.6,
							pk ? ", parked" : "", meetT, best + 1, tw.model.c_str(), tw.l);
					continue;
				}
			}
			double n[2], vn = 0;
			NVec3  ve{};
			if (!touching(e, c, ax, he, n, vn, ve)) continue;
			T.lastHit[e] = now;
			const int    cls = std::clamp(Call<int>(N::GET_VEHICLE_CLASS, e), 0, 22);
			const double mE = T.classMass[cls], mB = T.beamMass;
			const double J = (1.0 + T.bounce) * vn / (1.0 / mB + 1.0 / mE);  // the impulse along n (N s)
			// (v2.72: GTA's car never gets more than the closing speed; a bike or bicycle leaves faster than your car comes on (1.2 x its
			// speed towards it + 1 m/s): pushed to just your car's speed it stayed inside the drawn car, hidden by it: "vanished in place")
			const bool   light = cls == 8 || cls == 13;
			const double carN = d.carV[0] * n[0] + d.carV[1] * n[1], veN = ve.x * n[0] + ve.y * n[1];
			const double dvE = light ? std::clamp(1.2 * carN + 1.0 - veN, 0.0, 2.0 * vn) : std::min(J / mE * T.pushGta, vn), dvBn = J / mB * T.pushBeam;
			// (on foot the hidden car is solid: GTA's own physics pushes its cars; a train keeps to its rails)
			if (!d.foot && cls != 21) CallVoid(N::SET_ENTITY_VELOCITY, e, float(ve.x + n[0] * dvE), float(ve.y + n[1] * dvE), float(ve.z + (light ? 0.5 : 0.05 * dvE)));
			if (cls == 8 || cls == 13) TrafficWatch(T, e, "motorcycle", vn);
			const double ex = edge(n);
			const double P[3] = { cc[0] + n[0] * ex, cc[1] + n[1] * ex, d.carO[2] + std::min(0.6, h * 0.4) };
			if (T.gtaDmg > 0 && vn > 2.0) {
				// GTA's damage at the spot (in its frame; v2.72: stepped until it shows, like the twins' dents: 1 cm per m/s, 25 cm at most)
				const double rel[3] = { P[0] - pp[0], P[1] - pp[1], P[2] - pp[2] };
				GtaDentAdd(T, e, rel[0] * rt.x + rel[1] * rt.y + rel[2] * rt.z, rel[0] * fw.x + rel[1] * fw.y + rel[2] * fw.z, rel[0] * up.x + rel[1] * up.y + rel[2] * up.z,
					std::min(0.25, 0.01 * vn * T.gtaDmg / 25.0));
			}
			if ((cls == 8 || cls == 13) && vn > T.riderKnock) {
				const int rider = Call<int>(N::GET_PED_IN_VEHICLE_SEAT, e, -1, 0);
				if (rider && rider != me) {
					TrafficHold(T, rider);  // v2.73
					CallVoid(N::KNOCK_PED_OFF_VEHICLE, rider);
					Call<int>(N::SET_PED_TO_RAGDOLL, rider, 2000, 4000, 0, 0, 0, 0);
					++T.riders;
				}
			}
			const double dvB[3] = { -n[0] * dvBn, -n[1] * dvBn, 0 };
			const double dent = vn > 3.0 ? std::min(T.dentMax, T.dentPerMs * vn * std::min(1.0, mE / mB)) : 0.0;
			send(dvB, P, n, dent, cls == 8 ? "a motorcycle" : cls == 13 ? "a bicycle" : "a GTA car");
			++T.carHits;
			T.dvSum += dvBn;
			if (T.logs++ < 40)
				Log("traffic: BeamNG's car and a GTA car (class %d, %.0f kg) met at %.0f km/h closing: GTA's car pushed %.1f km/h, BeamNG's %.1f km/h, dent %.0f mm", cls, mE, vn * 3.6,
					dvE * 3.6, dvBn * 3.6, dent * 1000);
		}
		// v2.71: twins back to GTA
		for (size_t k = 0; k < T.twins.size(); ++k) {
			TrafficState::Twin& tw = T.twins[k];
			if (tw.state != 2) continue;
			const char* why = nullptr;
			NVec3       p{};
			const bool  alive = tw.ent && Call<int>(N::DOES_ENTITY_EXIST, tw.ent);
			if (alive) p = Call<NVec3>(N::GET_ENTITY_COORDS, tw.ent, 0);
			const double dist = alive ? hypot(p.x - cc[0], p.y - cc[1]) : 0;
			if (!alive) why = "GTA's car is gone";
			else if (now - tw.tA > 1.0 && (tw.tPose == 0 || now - tw.tPose > 1.0)) why = "no poses from BeamNG";
			else if (dist > 45.0) why = "far from your car";
			else if (tw.tStill > 0 && now - tw.tStill > T.twStill) why = "standing still";
			else if (tw.dmax < 0.01 && now - tw.tA > 2.0 && dist > 10.0) why = "no crash";
			else if (now - tw.tA > 30.0) why = "30 s";
			if (why) TwinRelease(T, int(k), why);
		}
		for (auto it = T.twDone.begin(); it != T.twDone.end();) it = now - it->second > 5.0 ? T.twDone.erase(it) : std::next(it);
		// people: only by a moving car (on foot the parked car is solid in GTA: people walk around it)
		for (int e : (d.foot || sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1]) < 2.0) ? std::vector<int>{} : T.nearP) {
			if (!Call<int>(N::DOES_ENTITY_EXIST, e) || Call<int>(N::IS_PED_DEAD_OR_DYING, e, 1)) continue;
			const NVec3  p = Call<NVec3>(N::GET_ENTITY_COORDS, e, 0);
			const double c[3] = { p.x, p.y, p.z }, ax[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } }, he[3] = { 0.3, 0.3, 0.9 };
			double n[2], vn = 0;
			NVec3  ve{};
			if (!touching(e, c, ax, he, n, vn, ve) || vn < T.pedKnock) continue;
			T.lastHit[e] = now;
			TrafficHold(T, e);  // v2.73
			Call<int>(N::SET_PED_TO_RAGDOLL, e, 1500, 4000, 0, 0, 0, 0);
			// v2.72: a speed, not a force (v2.71's "strong" force flag is x100). Your v2.71 test: people ragdolled, then "vanished in place":
			// pushed slower than your car comes on, they stayed inside the drawn car, hidden by it. Now, like a real hit: slow (under
			// 22 km/h) they're shoved ahead and aside faster than the car; faster they go up over it (0.75 x its speed along its way,
			// 3-7 m/s up: the car passes under them, they land behind it). pedThrow scales it; set again next tick (the ragdoll starts then)
			{
				const double vl = std::max(0.1, sqrt(d.carV[0] * d.carV[0] + d.carV[1] * d.carV[1]));
				const double fx = d.carV[0] / vl, fy = d.carV[1] / vl, k = T.pedThrow;
				double       vx, vy, vz;
				if (vl < 4.0) {
					// v2.73: your v2.72 test: at ~8 km/h a person was flung ("a very noticeably modded force": 1.2 x + 1.5 m/s). Slow: pushed
					// aside at the car's speed + 0.5 m/s, barely up
					const double sp = vl + 0.5;
					vx = (fx * 0.4 + n[0] * 0.6) * sp, vy = (fy * 0.4 + n[1] * 0.6) * sp, vz = 0.3;
				} else if (vl < 8.0) {
					const double sp = 1.1 * vl + 0.5;
					vx = (fx * 0.6 + n[0] * 0.4) * sp, vy = (fy * 0.6 + n[1] * 0.4) * sp, vz = 1.0;
				} else {
					const double ah = std::min(0.75 * vl, 25.0);
					vx = fx * ah + n[0] * 2.0, vy = fy * ah + n[1] * 2.0, vz = std::clamp(0.25 * vl, 3.0, 7.0);
				}
				T.pedPush.push_back({ e, float(vx * k), float(vy * k), float(vz * k), 2 });
			}
			TrafficWatch(T, e, "person", vn);
			const int hp = int((vn - T.pedKnock) * T.pedHurt);
			if (hp > 0) CallVoid(N::APPLY_DAMAGE_TO_PED, e, hp, 0, 0, 0);
			const double dvB[3] = { -n[0] * T.pedSlow, -n[1] * T.pedSlow, 0 };
			const double P[3] = { cc[0], cc[1], cc[2] };
			send(dvB, P, n, 0.0, "a person");
			++T.pedHits;
			if (T.logs++ < 40) Log("traffic: BeamNG's car hit a person at %.0f km/h: knocked down, %d health", vn * 3.6, hp);
		}
		if (now - T.logT > 10.0) {
			if (T.carHits + T.pedHits > 0 || T.logT == 0)
				Log("traffic: 10 s: %zu cars and %zu people near, %u car hits (BeamNG's car pushed %.1f km/h in all), %u people hit, %u riders knocked off", T.nearV.size(), T.nearP.size(),
					T.carHits, T.dvSum * 3.6, T.pedHits, T.riders);
			if (T.twActs + T.twRel + T.twFails > 0) {
				int ready = 0, on = 0;
				for (const auto& tw : T.twins) ready += tw.state == 1, on += tw.state == 2;
				Log("traffic twins: 10 s: %u GTA cars got a twin, %u back to GTA, %u poses copied, %u dents given to GTA's cars, %u failed; now %d ready, %d on GTA cars", T.twActs, T.twRel,
					T.twPoses, T.twDents, T.twFails, ready, on);
				T.twActs = T.twRel = T.twPoses = T.twDents = T.twFails = 0;
			}
			T.logT = now;
			T.carHits = T.pedHits = T.riders = 0;
			T.dvSum = 0;
			for (auto it = T.lastHit.begin(); it != T.lastHit.end();) it = now - it->second > 10.0 ? T.lastHit.erase(it) : std::next(it);
		}
	}

	void WeaponsTick(bool paused)
	{
		WeaponState& W = g_wpn;
		DriveState&  d = g_drv;
		if (!g_cfg.weapons) return;
		if (!W.cfgLoaded) LoadWeapons();
		const double now = NowSeconds();
		if (g_modelGen.load() != W.holeModel) {
			W.holeModel = g_modelGen.load();
			HolesClear("another car file");  // v2.62
		}
		WeaponsFx(now);
		if (d.state == 2 && d.carPoseOk && !paused && !d.beamPaused) {
			WeaponsShots(now);
			WeaponsBlasts(now);
			if (!d.foot) WeaponsCrash(now);
			else W.vh.clear();
		} else {
			W.vh.clear();
		}
		if (now - W.logT >= 10.0) {
			if (W.shots + W.guesses + W.blastsN + W.fxHits + W.hurtN > 0)
				Log("weapons: 10 s: %u bullets seen landing near (%u through the car), %u guessed hits, %u hits to BeamNG (%u it found on its car), %u explosions, you hurt %u times "
				    "(%.0f health); %zu shooters watched; %d bullet holes on the drawn car",
					W.shots, W.onCar, W.guesses, W.sent, W.fxHits, W.blastsN, W.hurtN, W.hurtSum, W.peds.size(), g_holeN);
			W.shots = W.onCar = W.guesses = W.sent = W.fxHits = W.blastsN = W.hurtN = 0;
			W.hurtSum = 0;
			W.logT = now;
		}
	}

	void TireFxStopAll();
	void DriveStop(const char* why, bool notify)
	{
		DriveState& d = g_drv;
		if (d.state == 0) return;
		if (g_dnet.s != INVALID_SOCKET) {
			// v2.01: "park":1 = the car stays parked here with GTA focused, so BeamNG keeps its own controls off
			const bool parkOn = g_cfg.parkBlock && g_haveModel.load() && GtaFocused();
			for (int i = 0; i < 2; ++i) DrvSend(parkOn ? "{\"t\":\"drive_stop\",\"park\":1}" : "{\"t\":\"drive_stop\"}");
		}
		// v2.41: the radio as it is now goes to GTA's car (off stays off, a station stays that station: your v2.4 test had
		// to switch it off twice). v2.3: the phone radio off again (the GTA car's own radio comes back with its engine)
		std::string radioTo;
		if (g_cfg.driveRadio == 1 && d.engineOff && d.state != 1) {
			radioTo = d.radioOn ? RadioNow() : "OFF";
			Log("drive: GTA's radio at F10 off: %s%s (the car's own radio gets it)", radioTo.c_str(), d.radioOn ? "" : " (the phone radio wasn't on)");
		}
		if (d.radioOn) PhoneRadio(false);
		// v2.3: BeamNG's "mute when unfocused" goes back on now: let it see a focus change so it mutes again
		// (v2.4: not in session mode: the option stays off, BeamNG's volume goes down instead)
		if (d.sw && g_cfg.soundWake != 3 && strncmp(why, "no ", 3) != 0) SoundWake("drive end");
		WorldEnd(why);  // v2.4: GTA's ground stops streaming (the tiles stay in BeamNG, the car on them)
		WeaponsPuppet(false);  // v2.6: GTA's own damage back on (a car still burning in BeamNG stays fire- and explosion-proof)
		PropsRemoveAll(why);  // v2.5: solid prop boxes out of BeamNG
		d.carPoseOk = false;
		if (d.puppet && d.veh && Call<int>(N::DOES_ENTITY_EXIST, d.veh)) {
			// back to a normal GTA car, standing on GTA's ground (v2.41: also when the drive ends while BeamNG's car is being
			// put on GTA's ground: v2.4 left the car without collision and gravity then)
			LandJob& L = g_land;
			L = LandJob{};
			L.on = true;
			L.veh = d.veh;
			L.t0 = NowSeconds();
			L.bottom = g_tgt.dmin.z;
			L.why = why;
			NVec3 p = Call<NVec3>(N::GET_ENTITY_COORDS, d.veh, 0);
			double gz = 0;
			L.haveG = g_cfg.world && WorldGroundAt(p.x, p.y, gz);
			const double tyres = p.z + L.bottom;
			const bool   under = (L.haveG && tyres < gz - 1.0) || (!L.haveG && d.goodOk && tyres < d.goodX[2] - 3.0);
			if (under && d.goodOk) {
				// the hidden car followed BeamNG's car through a gap: back to the last spot it stood on GTA's ground
				double Rg[3];
				Cross3(d.goodF, d.goodU, Rg);
				const double V0[3] = { 0, 0, 0 };
				DrvSetPuppet(d.goodX, Rg, d.goodF, d.goodU, V0);
				L.moved = true;
				Log("drive: GTA's car was %.1f m under GTA's ground at F10 off: put on the last good spot (%.0f m away) first", (L.haveG ? gz : d.goodX[2]) - tyres,
					hypot(d.goodX[0] - p.x, d.goodX[1] - p.y));
				p = Call<NVec3>(N::GET_ENTITY_COORDS, d.veh, 0);
				L.haveG = g_cfg.world && WorldGroundAt(p.x, p.y, gz);
			}
			L.x = p.x;
			L.y = p.y;
			L.z = p.z;
			L.gz = gz;
			// v2.4: the toggle at speed: GTA's physics carry on with BeamNG's speed (it used to stop dead)
			if (!under && d.state == 2 && g_cfg.exitKeepSpeed && d.lastVOk) {
				const double sp = sqrt(d.lastV[0] * d.lastV[0] + d.lastV[1] * d.lastV[1] + d.lastV[2] * d.lastV[2]);
				if (sp > 0.5 && sp < 150.0) {
					L.keepV = true;
					memcpy(L.V, d.lastV, sizeof(L.V));
				}
			}
			if (d.frozen || d.foot) {
				CallVoid(N::FREEZE_ENTITY_POSITION, d.veh, 0);
				d.frozen = false;
			}
			CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);
			CallVoid(N::SET_ENTITY_HAS_GRAVITY, d.veh, 1);
			CallVoid(N::SET_ENTITY_COLLISION, d.veh, 1, 1);
			if (d.engineOff) CallVoid(N::SET_VEHICLE_ENGINE_ON, d.veh, d.engineWasOn ? 1 : 0, 1, 0);  // v2.1: GTA's engine as it was
			if (!radioTo.empty()) {
				CallVoid(N::SET_VEH_RADIO_STATION, d.veh, radioTo.c_str());
				g_radioAfter = RadioAfter{};
				g_radioAfter.veh = d.veh;
				g_radioAfter.want = radioTo;
				g_radioAfter.until = NowSeconds() + 4.0;
			}
			LandTick();  // usually right away (GTA has its collision around the player); else it waits, frozen
		}
		g_driveHide = false;
		if (g_tgt.mode == 1 && g_tgt.veh == d.veh && d.veh) {
			SetVisible(d.veh, false);  // the normal hide mode again (ghost: GTA's wheels turn)
			ResetGtaWheels(d.veh);
		}
		if (d.plog) {
			fclose(d.plog);
			d.plog = nullptr;
		}
		Log("drive: stopped (%s)", why);
		if (notify) Notify("Drive OFF (%s).", why);
		if (d.signLocked && !d.signSkip && g_cfg.driveSteerSign == 0) g_drvLearnedSign = d.sign;
		d.state = 0;
		d.poses.clear();
		d.wHave = false;
		TireFxStopAll();  // v2.73
	}

	void DriveStart()
	{
		DriveState& d = g_drv;
		if (!g_haveModel.load()) {
			Notify("Drive: no BeamNG car loaded yet. Shift+F11 gets it from BeamNG.");
			return;
		}
		DrvModel m;
		uint32_t gen;
		{
			std::lock_guard<std::mutex> lk(g_modelMx);
			m = g_drvModel;
			gen = g_modelGen.load();  // read with the model: a load in between must stop drive mode
		}
		if (!m.ok || !m.frameOk) {
			Notify("Drive: this car file can't be used for driving (made by an old mod or its axes are unclear). Shift+F11 exports it again.");
			Log("drive: car file not usable (loaded %d, vehicle frame %d)", m.ok, m.frameOk);
			return;
		}
		if (g_tgt.mode == 0) Attach();
		if (g_tgt.mode == 0) return;
		const int ped = Call<int>(N::PLAYER_PED_ID);
		if (Call<int>(N::GET_PED_IN_VEHICLE_SEAT, g_tgt.veh, -1, 0) != ped) {
			Notify("Drive: get in the driver's seat of the car first.");
			return;
		}
		// v2.4: with GTA's ground in BeamNG, F10 on only at a standstill for now (BeamNG's car starts from rest)
		if (g_cfg.world) {
			const float kmh = Call<float>(N::GET_ENTITY_SPEED, g_tgt.veh) * 3.6f;
			if (kmh > g_cfg.driveStartMaxKmh) {
				Notify("Drive: stop the car first (F10 on works at a standstill for now; F10 off works at any speed).");
				Log("drive: not started: the car moves at %.0f km/h (DriveStartMaxKmh=%.0f)", kmh, g_cfg.driveStartMaxKmh);
				return;
			}
		}
		if (g_tgt.mode == 2) {
			g_tgt.mode = 1;
			SetVisible(g_tgt.veh, false);
		}
		if (!DrvOpen()) {
			Notify("Drive: network start failed.");
			return;
		}
		uint32_t rc;
		AcquireSRWLockExclusive(&g_dnet.lock);
		g_dnet.n = 0;
		++g_dnet.gen;
		rc = g_dnet.replyCount;
		ReleaseSRWLockExclusive(&g_dnet.lock);
		d = DriveState{};
		d.m = m;
		d.veh = g_tgt.veh;
		d.modelGen = gen;
		d.replySeen = rc;
		d.state = 1;
		// v2.4: GTA's radio as it is now (255 = off): it stays off if it was off
		{
			d.radioWas = Call<int>(N::GET_PLAYER_RADIO_STATION_INDEX);
			const char* rn = Call<const char*>(N::GET_PLAYER_RADIO_STATION_NAME);
			d.radioWasOff = d.radioWas == 255 || !rn || !*rn || !_stricmp(rn, "OFF");
			d.radioName = d.radioWasOff ? "OFF" : rn;
			Log("drive: GTA's radio at F10: station %d (%s)", d.radioWas, rn ? rn : "none");
		}
		AcquireSRWLockShared(&g_dnet.lock);
		d.tpSeen = g_dnet.tpCount;
		d.eyeSeen = g_dnet.eyeCount;  // v2.72 (an older car's eye isn't this one's)
		ReleaseSRWLockShared(&g_dnet.lock);
		if (g_cfg.world) {
			// v2.4: the fixed mapping and GTA's ground around the car (measured while BeamNG answers)
			const NVec3 cp = Call<NVec3>(N::GET_ENTITY_COORDS, g_tgt.veh, 0);
			WorldBegin(cp.x, cp.y);
		}
		d.tStart = d.tLastStart = NowSeconds();
		d.lx = (g_tgt.dmin.x + g_tgt.dmax.x) * 0.5;
		d.ly = (g_tgt.dmin.y + g_tgt.dmax.y) * 0.5;
		d.lz = g_tgt.dmin.z;
		{
			// v2.71: your head in GTA's car (SKEL_Head, in the car's frame), while it still stands where GTA put it
			const int   ped = Call<int>(N::PLAYER_PED_ID);
			const NVec3 hd = Call<NVec3>(N::GET_PED_BONE_COORDS, ped, 31086, 0.0f, 0.0f, 0.0f);
			const NVec3 hl = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS, d.veh, hd.x, hd.y, hd.z);
			d.headG[0] = hl.x;
			d.headG[1] = hl.y;
			d.headG[2] = hl.z;
			d.headOk = Call<int>(N::IS_PED_IN_VEHICLE, ped, d.veh, 0) && std::isfinite(hl.x) && std::isfinite(hl.y) && std::isfinite(hl.z) &&
			           fabs(hl.x) < 3.0 && fabs(hl.y) < 5.0 && fabs(hl.z) < 3.0;
			d.eyeOk = d.seatDone = false;
		}
		if (g_cfg.driveSteerSign != 0) {
			d.sign = g_cfg.driveSteerSign;
			d.signLocked = true;
		} else if (g_drvLearnedSign != 0) {
			d.sign = g_drvLearnedSign;
			d.signLocked = true;
		}
		for (int i = 0; i < kDrvWheels; ++i) d.map[i] = -1;
		d.padGta = g_cfg.padStartGta;
		d.padSupp = 0xFFFFFFFFu;  // what is held when driving starts waits until it's let go
		DrvSendStart();
		Log("drive: asking BeamNG (port %d) to hand over its car; GTA car %d (%s), drawn origin offset %.2f %.2f %.2f m", g_cfg.port, d.veh, g_tgt.name, d.lx, d.ly, d.lz);
		Notify("Drive: connecting to BeamNG...");
	}

	// BeamNG agreed: map the wheels, make the GTA car a puppet
	// v2.3: BeamNG's sound in the background. BeamNG mutes itself when its window LOSES focus and only looks at its
	// "mute when unfocused" option at that moment: your v2.2 test showed the option switched off while BeamNG was already
	// in the background changed nothing, while alt-tabbing from BeamNG straight to GTA worked. So after the BeamNG mod
	// switches the option, BeamNG's window is brought to the front for a moment and GTA's back: BeamNG sees itself lose
	// focus with the option off (and at the end, with it back on: it mutes again). BeamNGSoundWake=message only sends
	// BeamNG's window the focus messages (invisible; UNVERIFIED whether BeamNG reacts to them).
	struct WakeJob
	{
		HWND gta;
		int  mode, ms;
		char when[32];
	};
	struct BeamWin
	{
		HWND best;
		LONG area;
	};
	BOOL CALLBACK FindBeamProc(HWND h, LPARAM lp)
	{
		auto* bw = reinterpret_cast<BeamWin*>(lp);
		if (!IsWindowVisible(h) || GetWindow(h, GW_OWNER)) return TRUE;
		DWORD pid = 0;
		GetWindowThreadProcessId(h, &pid);
		if (!pid || pid == GetCurrentProcessId()) return TRUE;
		HANDLE ph = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!ph) return TRUE;
		char  path[MAX_PATH] = {};
		DWORD n = MAX_PATH;
		const BOOL ok = QueryFullProcessImageNameA(ph, 0, path, &n);
		CloseHandle(ph);
		if (!ok) return TRUE;
		const char* base = strrchr(path, '\\');
		base = base ? base + 1 : path;
		char   low[MAX_PATH];
		size_t i = 0;
		for (; base[i] && i < sizeof(low) - 1; ++i) low[i] = char(tolower((unsigned char)base[i]));
		low[i] = 0;
		if (!strstr(low, "beamng")) return TRUE;
		RECT rc;
		if (!GetWindowRect(h, &rc)) return TRUE;
		const LONG area = (rc.right - rc.left) * (rc.bottom - rc.top);
		if (area > bw->area) {
			bw->area = area;
			bw->best = h;
		}
		return TRUE;
	}
	// v2.4: which window is in front (for the sound wake log)
	std::string WindowText(HWND h)
	{
		if (!h) return "nothing";
		char  title[96] = {};
		GetWindowTextA(h, title, sizeof(title));
		DWORD pid = 0;
		GetWindowThreadProcessId(h, &pid);
		char b[160];
		snprintf(b, sizeof(b), "'%s' (process %lu%s)", title, (unsigned long)pid, pid == GetCurrentProcessId() ? ", GTA" : "");
		return b;
	}
	DWORD WINAPI WakeThread(LPVOID param)
	{
		std::unique_ptr<WakeJob> j(static_cast<WakeJob*>(param));
		Sleep(150);  // the BeamNG mod has switched the option by now (it answers after doing it; at the end: one BeamNG frame)
		BeamWin bw{ nullptr, 0 };
		EnumWindows(FindBeamProc, reinterpret_cast<LPARAM>(&bw));
		if (!bw.best) {
			Log("sound: BeamNG's window not found: not woken (%s)", j->when);
			return 0;
		}
		HWND before = GetForegroundWindow();
		if (before != j->gta) {
			Log("sound: GTA isn't in front any more (%s is): BeamNG not woken (%s)", WindowText(before).c_str(), j->when);
			return 0;
		}
		// v2.4: logged step by step (your v2.3 bounce drives never needed a wake, so it was never really tested)
		const double t0 = NowSeconds();
		const DWORD  me = GetCurrentThreadId();
		const DWORD  gtaTid = GetWindowThreadProcessId(j->gta, nullptr);
		if (gtaTid && gtaTid != me) AttachThreadInput(me, gtaTid, TRUE);  // the foreground thread's input: allowed to hand the focus on
		const BOOL toBeam = SetForegroundWindow(bw.best);
		if (gtaTid && gtaTid != me) AttachThreadInput(me, gtaTid, FALSE);
		Sleep(20);
		const HWND during = GetForegroundWindow();
		const bool beamFront = during == bw.best;
		Sleep(std::max(0, j->ms - 20));
		bool   back = false;
		double backMs = -1;
		for (int t = 0; t < 8 && !back; ++t) {
			HWND        fg = GetForegroundWindow();
			const DWORD fgTid = fg ? GetWindowThreadProcessId(fg, nullptr) : 0;
			if (fgTid && fgTid != me) AttachThreadInput(me, fgTid, TRUE);
			BringWindowToTop(j->gta);
			SetForegroundWindow(j->gta);
			if (fgTid && fgTid != me) AttachThreadInput(me, fgTid, FALSE);
			Sleep(30);
			back = GetForegroundWindow() == j->gta;
			if (back) backMs = (NowSeconds() - t0) * 1000.0;
		}
		Log("sound: wake (%s): before %s; BeamNG's window %s asked to the front: %s; in front 20 ms later: %s (%s); GTA back in front: %s%s",
			j->when, WindowText(before).c_str(), WindowText(bw.best).c_str(), toBeam ? "ok" : "REFUSED by Windows", beamFront ? "BeamNG (good)" : "NOT BeamNG",
			WindowText(during).c_str(), back ? "yes" : "NO (click GTA)", back ? (" after " + std::to_string(int(backMs)) + " ms").c_str() : "");
		if (!beamFront) Log("sound: BeamNG never got the front, so it can't notice its option changed: try BeamNGSoundWake=session");
		return 0;
	}
	void SoundWake(const char* when)
	{
		if (!g_cfg.soundWake || !g_cfg.beamSound) return;
		if (!GtaFocused()) {
			Log("sound: GTA isn't in front: BeamNG not woken (%s); it wakes by itself on its next focus change", when);
			return;
		}
		if (g_exclusiveFs.load() == 1) {
			// exclusive fullscreen: GTA would drop out of fullscreen for a moment
			Log("sound: GTA runs in exclusive fullscreen: no window swap (%s); use borderless/windowed, or BeamNGSoundWake=session", when);
			return;
		}
		Log("sound: waking BeamNG (%s); GTA's display mode: %s", when, g_exclusiveFs.load() == 0 ? "windowed/borderless" : "unknown");
		auto* j = new WakeJob{ GetForegroundWindow(), 1, g_cfg.soundWakeMs, {} };
		strncpy(j->when, when, sizeof(j->when) - 1);
		HANDLE th = CreateThread(nullptr, 0, WakeThread, j, 0, nullptr);
		if (th) CloseHandle(th);
		else delete j;
	}

	void DrvOnOk(const std::string& reply)
	{
		DriveState& d = g_drv;
		{
			double sw = 0;
			d.sw = JsonNumC(reply.c_str(), "sw", sw) && sw > 0.5;
			if (d.sw) SoundWake("drive start");  // BeamNG was muted in the background: wake it so its sound plays
		}
		JsonStr(reply, "veh", d.vehB);
		d.eyeOk = JsonNums(reply.c_str(), "eye", d.eyeB, 3) == 3;  // v2.71: BeamNG's driver camera
		std::string ver;
		JsonStr(reply, "v", ver);
		d.nwB = std::max(0, JsonStrs(reply, "wheels", d.nameB, kDrvWheels));
		std::string spinHow;
		JsonStr(reply, "spin", spinHow);
		// BeamNG's wheels -> the car file's wheels, by name (the same names the export wrote)
		d.mapped = 0;
		bool used[kDrvWheels] = {};
		for (int i = 0; i < d.nwB; ++i) {
			d.map[i] = -1;
			for (int j = 0; j < d.m.nw; ++j) {
				if (!used[j] && d.nameB[i] == d.m.name[j]) {
					d.map[i] = j;
					used[j] = true;
					++d.mapped;
					break;
				}
			}
		}
		if (d.mapped == 0 && d.nwB == d.m.nw) {
			for (int i = 0; i < d.nwB; ++i) d.map[i] = i;
			d.mapped = d.nwB;
			Log("drive: no wheel names in common; matched BeamNG's %d wheels to the car file's in order", d.nwB);
		}
		std::string list;
		for (int i = 0; i < d.nwB; ++i) {
			char b[64];
			snprintf(b, sizeof(b), "%s%s->%s", i ? ", " : "", d.nameB[i].c_str(), d.map[i] >= 0 ? d.m.name[d.map[i]].c_str() : "(none)");
			list += b;
		}
		Log("drive: BeamNG mod %s agreed: vehicle '%s' (car file: '%s'), %d wheels (%d matched: %s), wheel spin from %s", ver.c_str(), d.vehB.c_str(), d.m.vehicle.c_str(),
			d.nwB, d.mapped, list.c_str(), spinHow.empty() ? "the speed" : spinHow.c_str());
		// v2.01: how BeamNG takes the controls, and which pad controls / keys your BeamNG bindings use
		JsonStr(reply, "inp", d.inpHow);
		JsonStr(reply, "fb", d.fb);
		{
			std::string padc, keyc, bhow;
			const bool  hasPad = JsonStr(reply, "padc", padc);
			JsonStr(reply, "keyc", keyc);
			JsonStr(reply, "bhow", bhow);
			double nb = 0;
			JsonNumC(reply.c_str(), "nb", nb);
			d.nBinds = int(nb);
			d.padKnown = hasPad && d.inpHow.find("binds") != std::string::npos;
			d.bindsMode = d.inpHow.find("binds") != std::string::npos;  // v2.4
			std::string padList;
			for (int c = 0; c < kPadCtls; ++c) {
				const std::string n = kPadBeam[c];
				const std::string alt = c == kPLStick ? "thumbly" : c == kPRStick ? "thumbry" : n;
				const std::string pc = "," + padc + ",";
				d.padBound[c] = pc.find("," + n + ",") != std::string::npos || pc.find("," + alt + ",") != std::string::npos;
				if (d.padBound[c]) padList += std::string(padList.empty() ? "" : " ") + kPadName[c];
			}
			d.keyBound = " ";
			for (char ch : keyc) d.keyBound += ch == ',' ? ' ' : ch;
			d.keyBound += " ";
			// v2.1: controls used only in combinations with your BeamNG pad modifiers, and the modifier buttons
			{
				std::string padm, pmod;
				JsonStr(reply, "padm", padm);
				JsonStr(reply, "pmod", pmod);
				JsonStr(reply, "plist", d.plist);
				const std::string pm = "," + padm + ",", pd = "," + pmod + ",";
				std::string cl, ml;
				d.padModBits = 0;
				auto padIdx = [](const std::string& n) {
					for (int c = 0; c < kPadCtls; ++c)
						if (n == kPadBeam[c] || (c == kPLStick && n == "thumbly") || (c == kPRStick && n == "thumbry")) return c;
					return -1;
				};
				for (int c = 0; c < kPadCtls; ++c) {
					d.padCombo[c] = !d.padBound[c] && (pm.find(std::string(",") + kPadBeam[c] + ",") != std::string::npos ||
					                                   (c == kPLStick && pm.find(",thumbly,") != std::string::npos) ||
					                                   (c == kPRStick && pm.find(",thumbry,") != std::string::npos));
					if (pd.find(std::string(",") + kPadBeam[c] + ",") != std::string::npos) {
						d.padModBits |= 1u << c;
						ml += std::string(ml.empty() ? "" : " ") + kPadName[c];
					}
					if (d.padCombo[c]) cl += std::string(cl.empty() ? "" : " ") + kPadName[c];
				}
				// "dpadd=btn_r;dpadu=btn_l/btn_l+btn_r": the modifier sets per combination-only control
				std::string padmx;
				JsonStr(reply, "padmx", padmx);
				for (size_t a = 0; a < padmx.size();) {
					size_t e = padmx.find(';', a);
					if (e == std::string::npos) e = padmx.size();
					const std::string item = padmx.substr(a, e - a);
					a = e + 1;
					const size_t eq = item.find('=');
					if (eq == std::string::npos) continue;
					const int c = padIdx(item.substr(0, eq));
					if (c < 0) continue;
					const std::string sets = item.substr(eq + 1);
					for (size_t b = 0; b <= sets.size();) {
						size_t f = sets.find('/', b);
						if (f == std::string::npos) f = sets.size();
						const std::string set = sets.substr(b, f - b);
						b = f + 1;
						uint32_t mask = 0;
						for (size_t q = 0; q < set.size();) {
							size_t g = set.find('+', q);
							if (g == std::string::npos) g = set.size();
							const int mc = padIdx(set.substr(q, g - q));
							if (mc >= 0) mask |= 1u << mc;
							q = g + 1;
						}
						if (mask && d.comboN[c] < 6) d.comboMask[c][d.comboN[c]++] = mask;
						if (f >= sets.size()) break;
					}
				}
				Log("drive: BeamNG uses your pad list '%s'; pad modifier buttons: %s; used only with a modifier held: %s (%s)", d.plist.empty() ? "?" : d.plist.c_str(),
					ml.empty() ? "none" : ml.c_str(), cl.empty() ? "none" : cl.c_str(), padmx.empty() ? "-" : padmx.c_str());
				if (g_cfg.padToggle >= 0 && d.padCombo[g_cfg.padToggle] && d.padModBits)
					Log("drive: %s is the GTA/BeamNG switch; with a BeamNG modifier held it does your BeamNG combination instead", kPadName[g_cfg.padToggle]);
			}
			if (d.inpHow.find("binds") == std::string::npos) {
				// no bindings: BeamNG takes GTA's basic values (RT throttle, LT brake, left stick, RB handbrake) - the
				// other buttons stay GTA's
				d.padKnown = true;
				padList.clear();
				for (int c = 0; c < kPadCtls; ++c) {
					d.padBound[c] = c == kPRT || c == kPLT || c == kPLStick || c == kPRB;
					if (d.padBound[c]) padList += std::string(padList.empty() ? "" : " ") + kPadName[c];
				}
				d.keyBound = " w s a d up down left right space ";
			}
			if (d.inpHow.empty()) d.inpHow = "fallback (BeamNG mod 2.0?)";
			Log("drive: controls: %s (%d of your BeamNG bindings, from %s); pad controls they use: %s; keys:%s%s", d.inpHow.c_str(), d.nBinds, bhow.empty() ? "?" : bhow.c_str(),
				padList.empty() ? "none" : padList.c_str(), d.keyBound.size() > 300 ? (d.keyBound.substr(0, 300) + "...").c_str() : d.keyBound.c_str(),
				d.fb.empty() ? "" : (" | BeamNG takes these as plain values: " + d.fb).c_str());
			if (d.inpHow.find("binds") == std::string::npos && g_cfg.driveInput)
				Notify("Drive: BeamNG couldn't use your BeamNG bindings; GTA's basic controls drive it. See gtabridge/bindings.txt.");
		}
		if (d.inpHow.find("binds") != std::string::npos && d.fb.find("str") == std::string::npos) d.signLocked = d.signSkip = true;  // steering comes from your bindings
		if (!d.m.vehicle.empty() && !d.vehB.empty() && d.m.vehicle != d.vehB) {
			Notify("Drive: BeamNG's car (%s) isn't the exported one (%s). Shift+F11 exports it.", d.vehB.c_str(), d.m.vehicle.c_str());
		}
		// the GTA car becomes a puppet: no collision with GTA's world (BeamNG has the physics), no
		// gravity, fully hidden (no GTA shadow around ours)
		g_driveHide = true;
		SetVisible(d.veh, false);
		if (!g_cfg.gtaEngineSound && g_cfg.driveRadio != 2) {
			// v2.1: BeamNG's own car sound plays (BeamNGSound); GTA's engine sound of the hidden car would play over it
			d.engineWasOn = Call<int>(N::GET_IS_VEHICLE_ENGINE_RUNNING, d.veh) != 0;
			CallVoid(N::SET_VEHICLE_ENGINE_ON, d.veh, 0, 1, 1);
			d.engineOff = true;
			// v2.3: GTA's car radio stops with its engine: play GTA's radio as the phone radio instead (DriveRadio=phone)
			if (g_cfg.driveRadio == 1) {
				if (d.radioWasOff) {
					// v2.41: off at F10 = no phone radio at all (v2.4 switched it on and then off: it came back on by itself)
					d.radioOffHeld = true;
					Log("drive: GTA's radio was off at F10: it stays off (no phone radio); in GTA pad mode GTA's radio button (D-pad left) turns it on");
				} else {
					PhoneRadio(true);
					// the phone radio picks a station by itself: kept on the one you had for a few seconds
					d.radioWant = d.radioName;
					d.radioEnfUntil = NowSeconds() + 5.0;
					CallVoid(N::SET_RADIO_TO_STATION_NAME, d.radioWant.c_str());
					Log("drive: GTA's radio plays as the phone radio while the hidden car's engine is off (%s, as at F10); GTA's own radio controls in GTA pad mode",
						d.radioWant.c_str());
				}
			}
		}
		CallVoid(N::SET_ENTITY_COLLISION, d.veh, 0, 1);
		CallVoid(N::SET_ENTITY_HAS_GRAVITY, d.veh, 0);
		CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);
		// v2.61: GTA deletes a car nobody sees once you're out of it (your 2.6 test: gone 0.3 s after you got out, the drawn car with it)
		// shortcut: it stays a mission car (never cleaned up by GTA) for the rest of the session; fine for one car
		CallVoid(N::SET_ENTITY_AS_MISSION_ENTITY, d.veh, 1, 1);
		d.puppet = true;
		WeaponsPuppet(true);  // v2.6
		// v2.42: the pose log (one file per GTA session, each drive appended)
		if (g_cfg.drivePoseLog && !d.plog) {
			static bool fresh = true;
			char path[MAX_PATH + 16];
			PathNextToModule("_poses.csv", path, sizeof(path));
			d.plog = fopen(path, fresh ? "wb" : "ab");
			if (d.plog) {
				setvbuf(d.plog, nullptr, _IOFBF, 1 << 16);
				if (fresh) fprintf(d.plog, "# GtaBeamD1 %s pose log. P,arrival,frame clock,sim clock,pose time used,x,y,z,vx,vy,vz (BeamNG). T,now,game clock,drawn BeamNG time,kind,sampled x,y,z,drawn x,y,z,frame time\n", D1_VERSION);
				fprintf(d.plog, "D,%.3f,drive start,clock %s\n", NowSeconds(), g_cfg.drivePoseClock ? "sim" : "frame");
				fresh = false;
			}
		}
		if (!g_cfg.driveSetVelocity) {
			CallVoid(N::FREEZE_ENTITY_POSITION, d.veh, 1);
			d.frozen = true;
		}
		d.tOk = NowSeconds();
		d.lastArrival = d.tOk;
		if (g_cfg.world) {
			// v2.4: GTA's ground first, then BeamNG's car is put on it (state 3); drive mode starts with BeamNG's answer
			d.state = 3;
			d.tpFirstT = d.tOk;
			d.tpSent = false;
			d.tpWhy = "start";
			Notify("Drive: building GTA's ground in BeamNG...");
			TwinsAsk(g_traffic);  // v2.72
			return;
		}
		d.state = 2;
		if (g_cfg.driveInput && d.inpHow.find("binds") != std::string::npos)
			Notify("Drive ON: your BeamNG controls drive it. %s: pad to GTA (aim/shoot) and back. %s: stop.", g_cfg.padToggle >= 0 ? kPadName[g_cfg.padToggle] : "-",
				KeyName(g_cfg.driveKey));
		else
			Notify("Drive ON: GTA's controls drive the BeamNG car. %s again: stop.", KeyName(g_cfg.driveKey));
	}

	// v2.71: your seat (your v2.7 note: the player should sit in each BeamNG car's driver seat). GTA's hidden car is put so
	// your head is where BeamNG's driver camera is: the drawn car's origin offset becomes your head (GTA car frame) + the eye
	// in front of it - BeamNG's eye (the car file's frame) - DriverSeat*M. GTA's car is hidden and without collision in drive
	// mode, so it can sit anywhere. The eye is taken 9 cm ahead of and 7 cm over SKEL_Head (UNVERIFIED: tune with DriverSeat*M)
	// ---------------------------------------------------------------------------------------
	// v2.72: your seat per BeamNG car (your v2.71 test: in some cars you sat behind the seat; editing the ini per car was too much).
	// Ctrl+Shift+F10 in drive mode: seat adjust mode, the arrow keys move you forward / back / left / right and Page Up / Page Down
	// up / down (15 cm a second held); Ctrl+Shift+F10 again saves it for this BeamNG car in GtaBeamD1_seats.json (next to the ini).
	// The offset is from where the seat logic put you (BeamNG's driver camera, or the drawn car's footprint)
	// ---------------------------------------------------------------------------------------
	void SeatsLoad()
	{
		SeatState& S = g_seat;
		S.loaded = true;
		char path[MAX_PATH + 16];
		PathNextToModule("_seats.json", path, sizeof(path));
		FILE* f = fopen(path, "rb");
		if (!f) return;
		std::string js;
		char        b[4096];
		size_t      n;
		while ((n = fread(b, 1, sizeof(b), f)) > 0) js.append(b, n);
		fclose(f);
		// "car name": [right, forward, up]
		for (size_t p = js.find('"'); p != std::string::npos; p = js.find('"', p)) {
			const size_t e = js.find('"', p + 1);
			if (e == std::string::npos) break;
			const std::string name = js.substr(p + 1, e - p - 1);
			size_t       q = e + 1;
			while (q < js.size() && (js[q] == ' ' || js[q] == ':' || js[q] == '\t')) ++q;
			if (q < js.size() && js[q] == '[') {
				double      v[3];
				const char* c = js.c_str() + q + 1;
				int         k = 0;
				for (; k < 3; ++k) {
					char* x = nullptr;
					v[k] = strtod(c, &x);
					if (x == c) break;
					c = x;
					while (*c == ',' || *c == ' ') ++c;
				}
				if (k == 3) S.cars[name] = { std::clamp(v[0], -1.5, 1.5), std::clamp(v[1], -2.0, 2.0), std::clamp(v[2], -1.0, 1.0) };
			}
			p = e + 1;
		}
		Log("seat (v2.72): %zu cars with your seat set (%s)", S.cars.size(), path);
	}
	void SeatsSave()
	{
		char path[MAX_PATH + 16];
		PathNextToModule("_seats.json", path, sizeof(path));
		FILE* f = fopen(path, "wb");
		if (!f) return;
		fputs("{\r\n  \"about\": \"GTA x Beam v2.72: where you sit in each BeamNG car, metres right, forward, up from where the mod put you. Ctrl+Shift+F10 in drive mode: adjust with the arrow keys and Page Up / Page Down, Ctrl+Shift+F10 again saves.\",\r\n  \"seats\": {", f);
		int k = 0;
		for (const auto& kv : g_seat.cars) fprintf(f, "%s\r\n    \"%s\": [%.3f, %.3f, %.3f]", k++ ? "," : "", kv.first.c_str(), kv.second[0], kv.second[1], kv.second[2]);
		fputs("\r\n  }\r\n}\r\n", f);
		fclose(f);
	}
	// the drawn car's offset in GTA's car: the seat logic's, then your offset for this car (you forward = GTA's car back)
	void SeatApply()
	{
		DriveState& d = g_drv;
		auto        it = g_seat.cars.find(d.vehB);
		const auto  o = it != g_seat.cars.end() ? it->second : std::array<double, 3>{ 0, 0, 0 };
		d.lx = g_seat.base[0] - o[0];
		d.ly = g_seat.base[1] - o[1];
		d.lz = g_seat.base[2] - o[2];
	}
	void DrvSeatEye();
	void DrvSeat()
	{
		DriveState& d = g_drv;
		if (d.seatDone) return;
		d.seatDone = true;
		d.lx = (g_tgt.dmin.x + g_tgt.dmax.x) * 0.5;  // (the footprint first: this runs again when the eye comes late)
		d.ly = (g_tgt.dmin.y + g_tgt.dmax.y) * 0.5;
		d.lz = g_tgt.dmin.z;
		DrvSeatEye();
		if (!g_seat.loaded) SeatsLoad();
		g_seat.base[0] = d.lx, g_seat.base[1] = d.ly, g_seat.base[2] = d.lz;
		SeatApply();
		auto it = g_seat.cars.find(d.vehB);
		if (it != g_seat.cars.end())
			Log("seat (v2.72): your seat for %s: %.2f right, %.2f forward, %.2f up (GtaBeamD1_seats.json)", d.vehB.c_str(), it->second[0], it->second[1], it->second[2]);
	}
	// v2.72: the driver's door (your v2.71 note): when GTA opens its hidden car's driver door (you get in or out), the BeamNG mod
	// opens the BeamNG car's driver door (its door latch, like clicking it in BeamNG) and closes it when GTA's shuts; GTA shows it
	// through the dents (the door's nodes move)
	// v2.73: the drawn car's front doors open as far as GTA's own are (your v2.72 test: the BeamNG door (v2.72) never opened:
	// you got in with the drive off, and BeamNG's door latch was a guess per car). GTA's door 0 (front left) and 1 (front right)
	// of the car the BeamNG car is drawn on (attached or driving); GTA's fully open = DoorOpenDeg
	void DoorTick()
	{
		static int   logs = 0;
		static float was[2] = {};
		const int    veh = g_drv.state == 2 && g_drv.veh ? g_drv.veh : g_tgt.mode ? g_tgt.veh : 0;
		const bool   ok = veh && Call<int>(N::DOES_ENTITY_EXIST, veh);
		for (int k = 0; k < 2; ++k) {
			const float r = ok ? std::clamp(Call<float>(N::GET_VEHICLE_DOOR_ANGLE_RATIO, veh, k), 0.0f, 1.0f) : 0.0f;
			g_doorAng[k].store(float(r * g_cfg.doorOpenDeg * kDeg), std::memory_order_relaxed);
			if ((r > 0.15f) != (was[k] > 0.15f) && logs < 40) {
				++logs;
				Log("door (v2.73): GTA's %s front door %s (%.2f): the drawn car's %s", k ? "right" : "left", r > 0.15f ? "opens" : "shut", r,
					g_doorParts[k].load() ? (r > 0.15f ? "opens with it" : "shuts") : "car file has no such door part (nothing turns)");
			}
			was[k] = r;
		}
	}

	// Ctrl+Shift+F10, and each tick while it's on
	void SeatAdjustToggle()
	{
		DriveState& d = g_drv;
		SeatState&  S = g_seat;
		if (!S.on && (d.state != 2 || !d.seatDone)) {
			Notify("Seat adjust: start drive mode (F10) first.");
			return;
		}
		S.on = !S.on;
		if (S.on) {
			Notify("Seat adjust (%s): arrow keys move you, Page Up / Page Down up and down. Ctrl+Shift+F10: save.", d.vehB.c_str());
			return;
		}
		SeatsSave();
		auto       it = S.cars.find(d.vehB);
		const auto o = it != S.cars.end() ? it->second : std::array<double, 3>{ 0, 0, 0 };
		Notify("Seat saved for %s: %.2f right, %.2f forward, %.2f up.", d.vehB.c_str(), o[0], o[1], o[2]);
		Log("seat (v2.72): saved for %s: %.3f right, %.3f forward, %.3f up", d.vehB.c_str(), o[0], o[1], o[2]);
	}
	void SeatAdjustTick()
	{
		SeatState&  S = g_seat;
		DriveState& d = g_drv;
		if (!S.on) return;
		if (d.state != 2) {
			S.on = false;
			SeatsSave();
			return;
		}
		for (int id : { 27, 172, 173, 174, 175, 176, 177 }) CallVoid(N::DISABLE_CONTROL_ACTION, 0, id, 0);  // (GTA's phone on the arrow keys)
		static double lastT = 0;
		const double  tNow = NowSeconds();
		const double  st = 0.15 * std::clamp(tNow - lastT, 0.0, 0.05);  // (15 cm a second held, whatever the frame rate)
		lastT = tNow;
		if (!GtaFocused()) return;
		auto held = [](int vk) { return (GetAsyncKeyState(vk) & 0x8000) != 0; };
		const double dv[3] = { (held(VK_RIGHT) ? st : 0) - (held(VK_LEFT) ? st : 0), (held(VK_UP) ? st : 0) - (held(VK_DOWN) ? st : 0),
			                   (held(VK_PRIOR) ? st : 0) - (held(VK_NEXT) ? st : 0) };
		if (dv[0] == 0 && dv[1] == 0 && dv[2] == 0) return;
		auto& o = S.cars[d.vehB];
		o[0] = std::clamp(o[0] + dv[0], -1.5, 1.5);
		o[1] = std::clamp(o[1] + dv[1], -2.0, 2.0);
		o[2] = std::clamp(o[2] + dv[2], -1.0, 1.0);
		SeatApply();
		const double now = NowSeconds();
		if (now - S.noteT > 0.4) {
			S.noteT = now;
			Notify("Seat: %.2f right, %.2f forward, %.2f up", o[0], o[1], o[2]);
		}
	}

	void DrvSeatEye()
	{
		DriveState& d = g_drv;
		if (!g_cfg.driverSeat) return;
		if (!d.eyeOk || !d.headOk) {
			Log("drive (v2.71): driver's seat: %s: GTA's car stays on the drawn car's footprint", !d.eyeOk ? "BeamNG sent no driver camera" : "your head wasn't found in GTA's car");
			return;
		}
		const double eye[3] = { d.eyeB[0] - d.o[0], d.eyeB[1] - d.o[1], d.eyeB[2] - d.o[2] };  // the car file's frame
		const double kEye[3] = { 0.0, 0.09, 0.07 };
		double       L[3];
		for (int k = 0; k < 3; ++k) L[k] = d.headG[k] + kEye[k] - eye[k] - g_cfg.seatAdj[k];
		const double mv = sqrt((L[0] - d.lx) * (L[0] - d.lx) + (L[1] - d.ly) * (L[1] - d.ly) + (L[2] - d.lz) * (L[2] - d.lz));
		if (eye[2] < 0.3 || eye[2] > 3.0 || fabs(eye[0]) > 1.5 || mv > 2.5) {
			Log("drive (v2.71): driver's seat: BeamNG's eye %.2f %.2f %.2f m (the car file's frame) looks wrong (GTA's car would move %.2f m): not used", eye[0], eye[1], eye[2], mv);
			return;
		}
		Log("drive (v2.71): driver's seat: BeamNG's eye at %.2f %.2f %.2f m (right, forward, up of the car file), your head in GTA's car at %.2f %.2f %.2f: GTA's car "
		    "moved %.2f m against the drawn car (offset %.2f %.2f %.2f -> %.2f %.2f %.2f)",
			eye[0], eye[1], eye[2], d.headG[0], d.headG[1], d.headG[2], mv, d.lx, d.ly, d.lz, L[0], L[1], L[2]);
		d.lx = L[0];
		d.ly = L[1];
		d.lz = L[2];
	}

	// where the car-space origin is in BeamNG's vehicle frame (v2.4: from DriveTick, unchanged)
	void DrvOriginUpdate(double now)
	{
		DriveState& d = g_drv;
		// ---- where the car-space origin is in BeamNG's vehicle frame: from the wheels (BeamNG's hubs vs the
		// car file's), averaged over the first 10 poses; the car file's own value without wheels
		if (!d.oDone) {
			for (size_t i = d.poses.size() - std::min<size_t>(d.newPoses, d.poses.size()); i < d.poses.size() && !d.oDone; ++i) {
				const DrvPose& p = d.poses[i];
				for (int w = 0; w < p.nw && w < d.nwB; ++w) {
					const int j = d.map[w];
					if (j < 0) continue;
					for (int k = 0; k < 3; ++k) {
						const double e = p.w[w][k] - d.m.c[j][k];
						d.oAcc[k] += e;
						d.oMin[k] = std::min(d.oMin[k], e);
						d.oMax[k] = std::max(d.oMax[k], e);
					}
					++d.oN;
				}
				if (d.oN >= 10 * std::max(1, d.mapped) || (d.mapped == 0 && now - d.tOk > 0.2)) d.oDone = true;
			}
			if (!d.oDone && now - d.tOk > 1.0) {
				d.oDone = true;  // not enough wheel readings in 1 s: what there is, else the car file's value
				Log("drive: only %d wheel readings in 1 s", d.oN);
			}
			if (d.oDone) {
				const double* fo = d.m.origin;
				if (d.oN > 0) {
					double spread = 0;
					for (int k = 0; k < 3; ++k) {
						d.o[k] = d.oAcc[k] / d.oN;
						spread = std::max(spread, d.oMax[k] - d.oMin[k]);
					}
					const double diff = sqrt((d.o[0] - fo[0]) * (d.o[0] - fo[0]) + (d.o[1] - fo[1]) * (d.o[1] - fo[1]) + (d.o[2] - fo[2]) * (d.o[2] - fo[2]));
					Log("drive: car origin in BeamNG's vehicle frame %.3f %.3f %.3f m from %d wheel readings (spread %.3f m); the car file says %.3f %.3f %.3f (%s), %.2f m apart%s",
						d.o[0], d.o[1], d.o[2], d.oN, spread, fo[0], fo[1], fo[2], d.m.originHow.c_str(), diff,
						diff > 0.3 ? " - a different car than the export?" : "");
					// v2.52: scattered readings = a wrecked car (wheels off, parts on the road): the car file's value (your 2.51 log: 130 m off,
					// the car fell 3 times)
					if (spread > 0.5) {
						memcpy(d.o, fo, sizeof(d.o));
						Log("drive: the wheel readings are scattered (a wrecked car?): the car file's origin is used");
					}
				} else {
					memcpy(d.o, fo, sizeof(d.o));
					Log("drive: no wheels matched; car origin from the car file: %.3f %.3f %.3f m (%s)", fo[0], fo[1], fo[2], d.m.originHow.c_str());
				}
				DrvSeat();
			}
		}
	}

	// v2.4: ask the BeamNG mod to put its car on GTA's ground: Xg = where the drawn car's origin (the bottom of its tyres,
	// footprint centre) goes in GTA's world, F/U = which way it faces. The mod waits for the tiles under it.
	void DrvSendTp(const double Xg[3], const double F[3], const double U[3], const char* why)
	{
		DriveState& d = g_drv;
		double      b[3];
		GtaToBeam(Xg, b);
		const double* o = d.oDone ? d.o : d.m.origin;
		char          m[512];
		snprintf(m, sizeof(m), "{\"t\":\"tp\",\"p\":[%.3f,%.3f,%.3f],\"f\":[%.5f,%.5f,%.5f],\"u\":[%.5f,%.5f,%.5f],\"o\":[%.3f,%.3f,%.3f],\"lift\":0.20,\"why\":\"%s\"}",
			b[0], b[1], b[2], F[0], F[1], F[2], U[0], U[1], U[2], o[0], o[1], o[2], why);
		AcquireSRWLockShared(&g_dnet.lock);
		d.tpSeen = g_dnet.tpCount;
		ReleaseSRWLockShared(&g_dnet.lock);
		DrvSend(m);
		d.tpSent = true;
		d.tpT = NowSeconds();
		d.tpWhy = why;
		memcpy(d.tpX, Xg, sizeof(d.tpX));  // the spot BeamNG's car goes to: the next "last good spot"
		memcpy(d.tpF, F, sizeof(d.tpF));
		memcpy(d.tpU, U, sizeof(d.tpU));
		Log("drive: asking BeamNG to put its car on GTA's ground (%s): GTA %.2f %.2f %.2f = BeamNG %.2f %.2f %.2f, facing %.2f %.2f %.2f; car origin in its frame %.3f %.3f %.3f (%s)",
			why, Xg[0], Xg[1], Xg[2], b[0], b[1], b[2], F[0], F[1], F[2], o[0], o[1], o[2], d.oDone ? "from its wheels" : "from the car file");
	}

	// are the tiles within r metres of the car built in BeamNG?
	bool WorldNearReady(double r, int& have, int& need)
	{
		const WorldState& w = g_world;
		std::set<int64_t> close;  // (not "near": a Windows macro)
		AddDisc(close, w.cx, w.cy, r);
		have = need = 0;
		for (int64_t k : close) {
			++need;
			auto it = w.tiles.find(k);
			if (it == w.tiles.end()) continue;
			// built in BeamNG, or (v2.41) GTA found no ground there (water, a hole): nothing to wait for
			// (v2.7: one being measured again near the car still has its last version in BeamNG)
			if ((it->second.state == 2 && it->second.sent && it->second.acked) || (it->second.healOld && it->second.acked) ||
			    (it->second.state == 3 && it->second.gaps >= 1))
				++have;
		}
		// v2.41: the tile right under the car must be built
		auto u = w.tiles.find(WKey(int(floor(w.cx / WTileM())), int(floor(w.cy / WTileM()))));
		const bool under = u != w.tiles.end() && ((u->second.state == 2 && u->second.sent) || u->second.healOld) && u->second.acked;
		return need > 0 && have == need && under;
	}

	// v2.4 state 3: GTA's ground is being built in BeamNG; then BeamNG's car is put on it. Drive mode proper (state 2)
	// starts with BeamNG's answer. BeamNG brakes meanwhile ("p":1 in the controls).
	void DriveSetupTick(const CamSample& c, double now)
	{
		DriveState& d = g_drv;
		DrvOriginUpdate(now);
		if (d.fixedMap && g_cfg.driveSetVelocity) CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);  // waits for BeamNG
		std::string tp;
		uint32_t    tc;
		AcquireSRWLockShared(&g_dnet.lock);
		tc = g_dnet.tpCount;
		if (tc != d.tpSeen) tp = g_dnet.tp;
		ReleaseSRWLockShared(&g_dnet.lock);
		if (d.tpSent && tc != d.tpSeen) {
			d.tpSeen = tc;
			std::string ty;
			JsonStr(tp, "t", ty);
			if (ty == "tp_err") {
				std::string e;
				JsonStr(tp, "err", e);
				Log("drive: BeamNG: %.300s", tp.c_str());
				const std::string why = "BeamNG couldn't put its car on GTA's ground: " + e;
				DriveStop(why.c_str(), true);
				return;
			}
			double st = 0, eh = 0, ev = 0, v = 0, tries = 0, keep = 0;
			JsonNumC(tp.c_str(), "keep", keep);  // v2.41: 1 = teleported as it is (damage kept), 0 = BeamNG's reset (repairs)
			JsonNumC(tp.c_str(), "st", st);
			{
				double ssT = 0;  // v2.42: poses are timed by BeamNG's simulation clock
				if (g_cfg.drivePoseClock == 1 && JsonNumC(tp.c_str(), "ss", ssT)) st = ssT;
			}
			JsonNumC(tp.c_str(), "eh", eh);
			JsonNumC(tp.c_str(), "ev", ev);
			JsonNumC(tp.c_str(), "v", v);
			JsonNumC(tp.c_str(), "tries", tries);
			std::string conv, api;
			JsonStr(tp, "conv", conv);
			JsonStr(tp, "api", api);
			Log("drive: BeamNG's car is on GTA's ground (%s): %.0f cm off sideways, %.0f cm %s, speed after %.2f m/s; rotation %s via %s (%s), %.0f tries; %.1f s after asking",
				d.tpWhy, eh * 100.0, fabs(ev) * 100.0, ev >= 0 ? "high" : "low", v, conv.c_str(), api.c_str(), keep > 0 ? "damage kept" : "BeamNG's reset: repaired", tries,
				now - d.tpFirstT);
			d.fallGraceT = std::max(d.fallGraceT, now + 1.5);  // v2.7: no crash damage while it settles
			// the fixed mapping from here on: GTA = BeamNG + origin, WorldZLift lower (no rotation)
			d.fixedMap = true;
			d.Ag[0] = g_world.ox;
			d.Ag[1] = g_world.oy;
			d.Ag[2] = -g_cfg.worldZLift;
			d.Ab[0] = d.Ab[1] = d.Ab[2] = 0;
			d.th = 0;
			d.cth = 1;
			d.sth = 0;
			d.anchored = true;
			// poses from before the teleport are dropped; the motion filter starts again
			d.poseMinSt = st;
			d.poses.clear();
			d.fOk = false;
			// the teleport's spot is on GTA's ground: the last good spot until the car stands somewhere else
			d.goodOk = true;
			d.goodT = now;
			memcpy(d.goodX, d.tpX, sizeof(d.goodX));
			memcpy(d.goodF, d.tpF, sizeof(d.goodF));
			memcpy(d.goodU, d.tpU, sizeof(d.goodU));
			d.lastArrival = now;
			d.state = 2;
			d.fallGraceT = now + 1.0;  // v2.41: it settles first
			if (!strcmp(d.tpWhy, "start")) {
				if (g_cfg.driveInput && d.inpHow.find("binds") != std::string::npos)
					Notify("Drive ON on GTA's ground: your BeamNG controls drive it. %s: pad to GTA and back. %s: stop.", g_cfg.padToggle >= 0 ? kPadName[g_cfg.padToggle] : "-",
						KeyName(g_cfg.driveKey));
				else
					Notify("Drive ON on GTA's ground: GTA's controls drive the BeamNG car. %s again: stop.", KeyName(g_cfg.driveKey));
			} else {
				Notify("Drive: BeamNG's car is back on GTA's ground.");
			}
			return;
		}
		int have = 0, need = 0;
		const bool ready = WorldNearReady(12.0, have, need);
		// where BeamNG's car goes: the GTA car at F10 (start), or the last good spot (fell: v2.4 used the fallen car here)
		const bool fell = !strcmp(d.tpWhy, "fell");
		double     X[3] = { c.ox, c.oy, c.oz }, F[3] = { c.F[0], c.F[1], c.F[2] }, U[3] = { c.U[0], c.U[1], c.U[2] };
		if (fell) {
			memcpy(X, d.tpX, sizeof(X));
			memcpy(F, d.tpF, sizeof(F));
			memcpy(U, d.tpU, sizeof(U));
		}
		// v2.72: the traffic twins first (only the first F10 of a BeamNG session spawns them: each freezes BeamNG 2-3 s)
		if (!d.tpSent && !fell && TwinsLoading(g_traffic)) {
			g_twLoadT = now;
			static double twNoteT = 0;
			if (now - twNoteT > 30.0) {
				twNoteT = now;
				Notify("Drive: BeamNG is loading the traffic twins (first F10 only, about 10 s)...");
			}
			d.tpFirstT = std::max(d.tpFirstT, now - 6.0);  // (the 8 s ground wait starts again after them)
			return;
		}
		if (!d.tpSent) {
			if ((ready && d.oDone) || now - d.tpFirstT > 8.0) {
				if (!ready) Log("drive: only %d of %d tiles near the car built in BeamNG after 8 s: putting the car there anyway", have, need);
				DrvSendTp(X, F, U, fell ? "fell" : "start");
			}
		} else if (now - d.tpT > 4.0) {
			Log("drive: no answer to the teleport after 4 s: asking again");
			DrvSendTp(X, F, U, d.tpWhy);
		}
		if (now - d.tpFirstT > 20.0) DriveStop("GTA's ground wasn't ready in BeamNG after 20 s (send GtaBeamD1.log and gtabridge_log.txt)", true);
	}

	// v2.61: you got out with drive mode on (or back in): BeamNG keeps its car (parked: brake, then parking brake); GTA's
	// hidden car stands solid (frozen) where BeamNG's is, so you, peds and traffic bump into it and your bullets hit it
	void WeaponsFoot(bool out);
	void DriveFoot(bool out)
	{
		DriveState& d = g_drv;
		d.foot = out;
		const int ped = Call<int>(N::PLAYER_PED_ID);
		if (out) {
			CallVoid(N::SET_ENTITY_COLLISION, d.veh, 1, 1);
			CallVoid(N::FREEZE_ENTITY_POSITION, d.veh, 1);
			CallVoid(N::SET_ENTITY_VISIBLE, ped, 1, 0);
			CallVoid(N::RESET_ENTITY_ALPHA, ped);
			Log("drive: you got out: BeamNG keeps the car (parked), GTA's hidden car stands solid where BeamNG's is; get back in to drive, %s ends drive mode",
				KeyName(g_cfg.driveKey));
			Notify("You got out: your BeamNG car stays. Get back in to drive. %s: drive mode off.", KeyName(g_cfg.driveKey));
		} else {
			CallVoid(N::SET_ENTITY_COLLISION, d.veh, 0, 1);
			CallVoid(N::FREEZE_ENTITY_POSITION, d.veh, d.frozen ? 1 : 0);
			if (d.engineOff) CallVoid(N::SET_VEHICLE_ENGINE_ON, d.veh, 0, 1, 1);  // (getting in starts GTA's engine)
			Log("drive: back in: BeamNG drives again");
			Notify("Back in: your BeamNG controls drive it.");
		}
		WeaponsFoot(out);
	}

	// one script tick of drive mode (after SampleCar: c holds the GTA car as GTA has it now)
	// v2.73: tyre smoke (your v2.72 note: "when tires spin they should screech and have smoke like in gta"). Per BeamNG wheel:
	// how fast its tread slips over the ground: its turn (BeamNG's wheel angle, per tick) x radius against its hub's own speed along
	// its way (a burnout, a locked wheel), and its hub's speed sideways (a slide). Over TireSmokeSlipMs: GTA's effect
	// (TireSmokeFx, core/wheel_burnout: GTA's own burnout smoke) on the hidden GTA car at the tyre's foot, following it; it stops
	// 0.25 s after the slip ends. In the air: none. The screech is BeamNG's own tyre sound (it plays from the background).
	struct TireFxState
	{
		int    fx[kDrvWheels] = {};
		double prevAng[kDrvWheels] = {}, prevP[kDrvWheels][3] = {}, slip[kDrvWheels] = {}, lowT[kDrvWheels] = {};
		bool   have[kDrvWheels] = {};
		int    veh = 0, starts = 0, fails = 0, logs = 0;
		double maxSlip = 0, logT = 0;
	} g_tfx;
	void TireFxStop(TireFxState& T)
	{
		for (int j = 0; j < kDrvWheels; ++j) {
			if (T.fx[j]) CallVoid(N::STOP_PARTICLE_FX_LOOPED, T.fx[j], 0);
			T.fx[j] = 0;
			T.have[j] = false;
		}
	}
	void TireFxStopAll() { TireFxStop(g_tfx); }
	void TireFxTick(const double O[3], const double R[3], const double F[3], const double U[3], bool spin, double dt)
	{
		TireFxState& T = g_tfx;
		DriveState&  d = g_drv;
		if (!g_cfg.tireSmoke || !spin || d.state != 2 || d.foot || !d.veh || T.veh != d.veh) {
			TireFxStop(T);
			T.veh = d.veh;
			return;
		}
		if (dt <= 1e-4) return;
		const double now = NowSeconds();
		for (int j = 0; j < kDrvWheels; ++j) {
			if (!d.wOk[j]) continue;
			const double r = std::max(0.1, d.m.radius[j]);
			double       P[3], A[3];
			for (int k = 0; k < 3; ++k) {
				P[k] = O[k] + R[k] * d.wC[j][0] + F[k] * d.wC[j][1] + U[k] * (d.wC[j][2] - r);  // the tyre's foot
				A[k] = R[k] * d.wA[j][0] + F[k] * d.wA[j][1] + U[k] * d.wA[j][2];
			}
			Norm3(A);
			double sl = 0;
			if (T.have[j]) {
				const double da = d.wAngU[j] - T.prevAng[j];
				const double v[3] = { (P[0] - T.prevP[j][0]) / dt, (P[1] - T.prevP[j][1]) / dt, (P[2] - T.prevP[j][2]) / dt };
				double fw[3];
				Cross3(U, A, fw);  // along its way (up x the axle; the sign doesn't matter below)
				Norm3(fw);
				const double along = fabs(Dot3(v, fw)), side = fabs(Dot3(v, A)), tread = fabs(da / dt) * r;
				sl = std::max(fabs(tread - along), 1.3 * side);  // (a slide smokes a little sooner)
				sl = std::min(sl, 60.0);
			}
			T.have[j] = true;
			T.prevAng[j] = d.wAngU[j];
			memcpy(T.prevP[j], P, sizeof(P));
			T.slip[j] += (sl - T.slip[j]) * std::min(1.0, dt * (sl > T.slip[j] ? 20.0 : 6.0));  // (smoothed: pose noise doesn't smoke)
			T.maxSlip = std::max(T.maxSlip, T.slip[j]);
			const bool want = T.slip[j] > g_cfg.tireSmokeMs && g_world.grounded;
			if (want) T.lowT[j] = now;
			if (want && !T.fx[j] && T.fails < 3) {
				CallVoid(N::REQUEST_NAMED_PTFX_ASSET, g_cfg.tireFx[0]);
				if (!Call<int>(N::HAS_NAMED_PTFX_ASSET_LOADED, g_cfg.tireFx[0])) continue;
				const NVec3 off = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS, d.veh, float(P[0]), float(P[1]), float(P[2]));
				CallVoid(N::USE_PARTICLE_FX_ASSET, g_cfg.tireFx[0]);
				T.fx[j] = Call<int>(N::START_PARTICLE_FX_LOOPED_ON_ENTITY, g_cfg.tireFx[1], d.veh, off.x, off.y, off.z, 0.0f, 0.0f, 0.0f, 1.0f, 0, 0, 0);
				if (T.fx[j]) ++T.starts;
				else ++T.fails;
				if (T.logs++ < 12)
					Log("tyre smoke (v2.73): wheel %d slips %.1f m/s: GTA's %s/%s %s", j, T.slip[j], g_cfg.tireFx[0], g_cfg.tireFx[1],
						T.fx[j] ? "started" : T.fails >= 3 ? "didn't start (3 times: no more tries; TireSmokeFx in the ini)" : "didn't start");
			} else if (T.fx[j] && now - T.lowT[j] > 0.25) {
				CallVoid(N::STOP_PARTICLE_FX_LOOPED, T.fx[j], 0);
				T.fx[j] = 0;
			} else if (T.fx[j]) {
				const NVec3 off = Call<NVec3>(N::GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS, d.veh, float(P[0]), float(P[1]), float(P[2]));
				CallVoid(N::SET_PARTICLE_FX_LOOPED_OFFSETS, T.fx[j], off.x, off.y, off.z, 0.0f, 0.0f, 0.0f);
			}
		}
		if (now - T.logT > 10.0) {
			if (T.starts || T.maxSlip > 1.0) Log("tyre smoke (v2.73): 10 s: the most a tyre slipped %.1f m/s (smoke from %.1f), smoke started %d times", T.maxSlip, g_cfg.tireSmokeMs, T.starts);
			T.logT = now;
			T.maxSlip = 0;
			T.starts = 0;
		}
	}

	void DriveTick(CamSample& c, bool paused)
	{
		DriveState& d = g_drv;
		if (d.state == 0) return;
		const double now = NowSeconds();
		const int    ped = Call<int>(N::PLAYER_PED_ID);
		if (!g_tgt.mode || g_tgt.veh != d.veh || !Call<int>(N::DOES_ENTITY_EXIST, d.veh)) {
			DriveStop("the GTA car is gone or detached", true);
			return;
		}
		{
			// v2.61: out of the driver's seat while BeamNG drives: foot mode (BeamNG keeps its car parked; back in = drive)
			const bool out = Call<int>(N::GET_PED_IN_VEHICLE_SEAT, d.veh, -1, 0) != ped;
			if (out && d.state != 2) {
				DriveStop("you left the driver's seat", true);
				return;
			}
			if (out != d.foot) DriveFoot(out);
			if (d.foot) {
				const NVec3 pp = Call<NVec3>(N::GET_ENTITY_COORDS, ped, 0);
				if (hypot(pp.x - d.carO[0], pp.y - d.carO[1]) > 300.0) {  // (GTA stops loading the world there: BeamNG's ground with it)
					DriveStop("you went more than 300 m from your car", true);
					return;
				}
			}
		}
		if (g_modelGen.load() != d.modelGen) {
			DriveStop("a new car file was loaded", true);
			return;
		}
		// v2.41: GTA was paused (alt-tab) or stalled: the BeamNG mod held its car meanwhile (hold messages from the receiver)
		{
			const uint32_t holds = g_holdsSent.load();
			if (d.tickT > 0 && now - d.tickT > 1.0) {
				Log("drive: GTA didn't run for %.1f s (paused or alt-tabbed); %u hold message(s) kept BeamNG's drive going (its car braked)", now - d.tickT, holds - d.holdsSeen);
				d.lastArrival = std::max(d.lastArrival, now);  // BeamNG's car data gets a fresh 5 s
				d.gtaPausedT = now;                            // v2.5: (for the pause sync below)
			}
			d.tickT = now;
			d.holdsSeen = holds;
		}
		// v2.43: pause sync. You pause BeamNG there: GTA's pause menu opens (once per pause) and closes when BeamNG runs
		// again. GTA paused (or alt-tabbed): the BeamNG mod pauses its physics ("gp" in the controls, "hold").
		{
			double pzT;
			bool   pzUser;
			AcquireSRWLockShared(&g_dnet.lock);
			pzT = g_dnet.pzT;
			pzUser = g_dnet.pzUser;
			ReleaseSRWLockShared(&g_dnet.lock);
			const bool bp = pzT > 0 && now - pzT < 0.6;
			if (paused) d.gtaPausedT = now;
			if (bp && !d.beamPaused) {
				d.beamPauseT0 = now;
				Log("drive: BeamNG is paused (%s)", pzUser ? "by you, in BeamNG" : "because GTA is");
			} else if (!bp && d.beamPaused) {
				Log("drive: BeamNG runs again (paused %.1f s)", now - d.beamPauseT0);
				d.fallGraceT = std::max(d.fallGraceT, now + 1.0);  // v2.7: (its speed jumps back: not a crash)
			}
			d.beamPaused = bp;
			d.beamPausedUser = bp && pzUser;
			if (!d.beamPausedUser) {
				d.beamPauseSeen = false;
			} else if (!d.beamPauseSeen && !paused && now - d.gtaPausedT > 1.0) {
				// (v2.5: not in the first second after GTA itself was paused: BeamNG may still report the pause the mod made)
				d.beamPauseSeen = true;
				if (g_cfg.pauseSync) {
					CallVoid(N::ACTIVATE_FRONTEND_MENU, uint32_t(0xD528C7E2) /* FE_MENU_VERSION_SP_PAUSE */, 1, -1);
					d.menuByBeam = true;
					Log("drive: you paused BeamNG: GTA's pause menu opened");
				}
			}
			if (!bp && d.menuByBeam) {
				d.menuByBeam = false;
				if (Call<int>(N::IS_PAUSE_MENU_ACTIVE)) {
					CallVoid(N::SET_FRONTEND_ACTIVE, 0);
					Log("drive: BeamNG runs again: GTA's pause menu closed");
				}
			}
			if (bp) d.lastArrival = std::max(d.lastArrival, now);  // no poses while paused is expected
		}
		// v2.72: the driver's eye from the car's own Lua (BeamNG's game-engine data had no cameras: your v2.71 log): the seat again
		{
			std::string ey;
			uint32_t    ec;
			AcquireSRWLockShared(&g_dnet.lock);
			ec = g_dnet.eyeCount;
			if (ec != d.eyeSeen) ey = g_dnet.eye;
			ReleaseSRWLockShared(&g_dnet.lock);
			if (ec != d.eyeSeen) {
				d.eyeSeen = ec;
				if (JsonNums(ey.c_str(), "e", d.eyeB, 3) == 3) {
					d.eyeOk = true;
					if (d.seatDone) {
						d.seatDone = false;
						DrvSeat();
					}
				}
			}
		}
		// v2.43: your repair binding was used in BeamNG (the car repaired where it is)
		{
			std::string rp;
			uint32_t    rc;
			AcquireSRWLockShared(&g_dnet.lock);
			rc = g_dnet.repCount;
			if (rc != d.repSeen) rp = g_dnet.rep;
			ReleaseSRWLockShared(&g_dnet.lock);
			if (rc != d.repSeen) {
				d.repSeen = rc;
				HolesClear("your repair binding");  // v2.62
				std::string act;
				JsonStr(rp, "a", act);
				Log("drive: BeamNG: your %s binding repaired the car where it is", act.c_str());
				d.fallGraceT = std::max(d.fallGraceT, now + 1.5);  // it is lifted a little and drops back
			}
		}
		// v2.41: the phone radio kept on your station for its first seconds (it picks one by itself)
		if (d.radioOn && !d.radioWant.empty() && now < d.radioEnfUntil && ++d.radioChk % 5 == 0) {
			const std::string cur = RadioNow();
			if (_stricmp(cur.c_str(), d.radioWant.c_str())) {
				CallVoid(N::SET_RADIO_TO_STATION_NAME, d.radioWant.c_str());
				if (d.radioFixes++ < 3) Log("drive: the phone radio played %s: set back to %s (as at F10)", cur.c_str(), d.radioWant.c_str());
			}
		}
		// v2.41: radio off at F10 = no phone radio; GTA's radio button in GTA pad mode switches it on
		if (d.radioOffHeld && !d.radioOn && d.state == 2 && d.padGta && !paused && GtaFocused()) {
			bool press = false;
			for (int ctl : { 81, 82, 85 })  // INPUT_VEH_NEXT_RADIO, INPUT_VEH_PREV_RADIO, INPUT_VEH_RADIO_WHEEL
				press = press || Call<int>(N::IS_DISABLED_CONTROL_JUST_PRESSED, 0, ctl) != 0;
			if (press) {
				d.radioOffHeld = false;
				PhoneRadio(true);
				Log("drive: GTA's radio switched on (your radio button): the phone radio plays");
			}
		}
		// v2.4: until BeamNG's car is on GTA's ground, GTA's ground is measured around the GTA car itself (v2.41: after a fall,
		// around the spot BeamNG's car goes back to: v2.4 measured around the fallen car and BeamNG's car fell again)
		if (g_cfg.world && d.state != 2 && c.carOn) {
			const double V0[3] = { 0, 0, 0 };
			if (d.state == 3 && !strcmp(d.tpWhy, "fell")) {
				WorldCar(d.tpX, V0, d.tpU);
			} else {
				const double O[3] = { c.ox, c.oy, c.oz }, U[3] = { c.U[0], c.U[1], c.U[2] };
				WorldCar(O, V0, U);
			}
		}
		// ---- v2.01: your pad and keyboard (read here), who gets which control, and GTA's own reactions
		// switched off where BeamNG has the control (GTA mustn't drive its hidden car itself either)
		const bool     rawIn = g_cfg.driveInput != 0;
		const bool     focused = GtaFocused();
		ReadPad(now);
		const PadNow&  pad = g_in.pad;
		const uint32_t held = PadHeld(pad);
		// held modifier buttons that are BeamNG's now (not always GTA's). v2.5: also when held across a switch (your v2.43 test:
		// RB + LB held while the pad mode changed stayed dead, so RB + LB + D-pad down switched the mode again instead of doing
		// your BeamNG combination). Their own BeamNG actions still wait until they're let go; as modifiers they count at once.
		d.modMask = (rawIn && !d.padGta) ? (held & d.padModBits & ~g_cfg.padAlwaysGta) : 0u;
		d.modHeld = d.modMask != 0;
		if (rawIn && pad.ok && g_cfg.padToggle >= 0 && g_cfg.padToggle < 14) {
			const bool t = (pad.btn & kPadBit[g_cfg.padToggle]) != 0;
			const bool toBeam = PadOwner(d, g_cfg.padToggle) == 1;  // one of your BeamNG combinations (e.g. RB + D-pad down)
			if (t && !d.toggleWas && !toBeam && d.state == 2 && focused && !paused && !d.foot) {
				d.padGta = !d.padGta;
				if (d.padGta) {
					d.modMask = 0;
					d.modHeld = false;
				}
				if (d.padGta)
					Notify("Pad: GTA mode (LB aim, RB shoot, weapons). %s: back to BeamNG.", kPadName[g_cfg.padToggle]);
				else
					Notify("Pad: BeamNG mode (your BeamNG buttons). %s: GTA mode.", kPadName[g_cfg.padToggle]);
				Log("drive: pad now in %s mode", d.padGta ? "GTA" : "BeamNG");
			}
			d.toggleWas = t;
		}
		uint32_t own = 0;
		for (int c = 0; c < kPadCtls; ++c)
			if (rawIn && PadOwner(d, c) == 1) own |= 1u << c;
		// a control held while it changes hands (the switch, a BeamNG modifier pressed or let go) waits until it's
		// let go: no surprise clutch or shot; what stays BeamNG's (triggers, steering stick) keeps working
		if (d.state == 2) d.padSupp |= held & (d.ownerPrev ^ own);
		d.ownerPrev = own;
		d.padSupp &= held;  // let go: usable again
		d.owner = own;
		std::string keys = (rawIn && focused && !paused && !d.foot) ? ReadKeys() : std::string(" ");
		if (keys.size() > 1) {
			g_in.keyT = now;
			d.kbUsed = true;
			for (size_t a = 1; a < keys.size();) {
				const size_t b = keys.find(' ', a);
				const std::string w = " " + keys.substr(a, b - a) + " ";
				if ((" " + d.keysSeen).find(w) == std::string::npos && d.keysSeen.size() < 200) d.keysSeen += w.substr(1);
				a = b + 1;
			}
		}
		d.keysNow = keys;
		// the pad counts as in use while anything on it is held (a trigger held flat out doesn't change) or it moved in the last 20 s
		const bool padActive = rawIn && pad.ok && (held != 0 || now - pad.lastChange < 20.0);
		if (keys.size() > 1) d.lastDev = 0;
		else if (pad.ok && (held != 0 || now - pad.lastChange < 0.5)) d.lastDev = 1;
		else if (d.lastDev < 0) d.lastDev = pad.ok ? 1 : 0;
		const bool kbActive = rawIn && (!padActive || now - g_in.keyT < 20.0 || Call<int>(N::IS_USING_KEYBOARD_AND_MOUSE, 2) != 0);
		// v2.3: pad buttons that are GTA's but run a BeamNG action (PadGtaActions; v2.4: none by default)
		d.handled = 0;
		if (pad.ok && d.state == 2 && !d.foot) {
			for (int i = 0; i < g_cfg.padActN; ++i) {
				const int  ctl = g_cfg.padActCtl[i];
				if (PadOwner(d, ctl) != 0) continue;
				d.handled |= 1u << ctl;
				const bool down = (pad.btn & kPadBit[ctl]) != 0;
				const bool was = (d.actWas >> ctl & 1u) != 0;
				if (down && !was && focused && !paused && !(d.padSupp >> ctl & 1u)) {
					const std::string m = "{\"t\":\"act\",\"a\":\"" + g_cfg.padActName[i] + "\"}";
					DrvSend(m.c_str());
					++d.actSent;
					Log("drive: %s -> BeamNG action %s", kPadName[ctl], g_cfg.padActName[i].c_str());
				}
			}
			d.actWas = 0;
			for (int i = 0; i < g_cfg.padActN; ++i)
				if (pad.btn & kPadBit[g_cfg.padActCtl[i]]) d.actWas |= 1u << g_cfg.padActCtl[i];
			// v2.4: the plugin's radio button is gone: GTA's own radio controls (radio wheel on D-pad left) work in GTA mode
		}
		d.blocked = d.foot ? 0 : BlockGtaControls(d, rawIn, padActive, kbActive);
		if (pad.ok && held) d.padUsed = true;

		// answers from BeamNG
		std::string reply;
		uint32_t    rc;
		AcquireSRWLockShared(&g_dnet.lock);
		rc = g_dnet.replyCount;
		if (rc != d.replySeen) reply = g_dnet.reply;
		ReleaseSRWLockShared(&g_dnet.lock);
		if (rc != d.replySeen) {
			d.replySeen = rc;
			std::string ty;
			JsonStr(reply, "t", ty);
			Log("drive: BeamNG: %.300s", reply.c_str());
			if (ty == "drive_err") {
				std::string e;
				JsonStr(reply, "err", e);
				std::string why = "BeamNG: " + e;
				DriveStop(why.c_str(), true);
				return;
			}
			if (ty == "drive_ok" && d.state == 1) DrvOnOk(reply);
		}
		if (d.state == 1) {
			if (now - d.tStart > 4.0) {
				DriveStop("no answer from BeamNG - is it running with the 2.01 BeamNG mod, in a level, in a car?", true);
			} else if (now - d.tLastStart > 0.5) {
				d.tLastStart = now;
				DrvSendStart();
			}
			return;
		}

		// ---- controls -> BeamNG. v2.01: the raw pad (only the controls BeamNG has now) and keys go to the BeamNG
		// mod, which runs them through your BeamNG bindings. thr/brk/str/hb are plain values for the parts it
		// has no bindings for (and everything with DriveInput=gta, the v2.0 way: GTA's own controls).
		const float gThr = Call<float>(N::GET_DISABLED_CONTROL_NORMAL, 0, kInputVehAccelerate);
		const float gBrk = Call<float>(N::GET_DISABLED_CONTROL_NORMAL, 0, kInputVehBrake);
		const float gLr = Call<float>(N::GET_DISABLED_CONTROL_NORMAL, 0, kInputVehMoveLR);  // -1 left, +1 right
		const bool  gHb = Call<float>(N::GET_DISABLED_CONTROL_NORMAL, 0, kInputVehHandbrake) > 0.5f;
		float       thr = 0, brk = 0, lr = 0;
		bool        hb = false;
		char        padStr[160] = "";
		std::string keyStr;
		if (!rawIn) {
			thr = gThr;
			brk = gBrk;
			lr = gLr;
			hb = gHb;
		} else if (!paused && d.anchored && !d.foot) {
			const uint32_t use = own & ~d.padSupp;
			if (pad.ok) {
				auto  on = [&](int c) { return (use >> c & 1u) != 0; };
				const float lx = on(kPLStick) ? pad.lx : 0.0f, ly = on(kPLStick) ? pad.ly : 0.0f;
				const float rx = on(kPRStick) ? pad.rx : 0.0f, ry = on(kPRStick) ? pad.ry : 0.0f;
				const float lt = on(kPLT) ? pad.lt : 0.0f, rt = on(kPRT) ? pad.rt : 0.0f;
				WORD        btn = 0;
				for (int c = 0; c < 14; ++c)
					if (on(c) && (pad.btn & kPadBit[c])) btn |= kPadBit[c];
				// v2.5: the BeamNG modifier buttons held (BeamNG mode), for your combinations, even while their own actions wait
				WORD pm = 0;
				for (int c = 0; c < 14; ++c)
					if ((d.modMask >> c & 1u) && (pad.btn & kPadBit[c])) pm |= kPadBit[c];
				snprintf(padStr, sizeof(padStr), "%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%u,%u", lx, ly, rx, ry, lt, rt, unsigned(btn), unsigned(pm));
				d.sentPm = pm;
				thr = rt;
				brk = lt;
				lr = fabsf(lx) > 0.12f ? lx : 0.0f;
				hb = (btn & kPadBit[kPRB]) != 0;
				d.sentLx = lx;
				d.sentLt = lt;
				d.sentRt = rt;
				d.sentBtn = btn;
				d.mxRt = std::max(d.mxRt, pad.rt);
				d.mxLt = std::max(d.mxLt, pad.lt);
				d.mxLx = std::max(d.mxLx, fabsf(pad.lx));
				d.ctlSeen |= held;
			}
			auto k = [&](const char* n) { return keys.find(std::string(" ") + n + " ") != std::string::npos; };
			thr = std::max({ thr, (k("w") || k("up")) ? 1.0f : 0.0f, std::clamp(gThr, 0.0f, 1.0f) });
			brk = std::max({ brk, (k("s") || k("down")) ? 1.0f : 0.0f, std::clamp(gBrk, 0.0f, 1.0f) });
			lr = std::clamp(lr + ((k("d") || k("right")) ? 1.0f : 0.0f) - ((k("a") || k("left")) ? 1.0f : 0.0f) + (pad.ok ? 0.0f : gLr), -1.0f, 1.0f);
			hb = hb || k("space") || gHb;
			keyStr = keys.size() > 2 ? keys.substr(1, std::min<size_t>(keys.size() - 2, 400)) : "";
		}
		d.sentOwner = own & ~d.padSupp;
		thr = std::isfinite(thr) ? std::clamp(thr, 0.0f, 1.0f) : 0.0f;
		brk = std::isfinite(brk) ? std::clamp(brk, 0.0f, 1.0f) : 0.0f;
		lr = std::isfinite(lr) ? std::clamp(lr, -1.0f, 1.0f) : 0.0f;
		if (paused || !d.anchored || d.foot) {
			// paused (v2.61: or you got out): brake to a stop, then the parking brake (a parking brake at speed would spin the car,
			// and a held brake at a standstill makes BeamNG's automatic gearbox reverse). v2.01: "p" tells
			// the BeamNG mod to let go of your bindings and do the same.
			const bool fast = fabs(d.speed) > 3.0;
			thr = lr = 0.0f;
			brk = (paused || d.foot) && fast ? 0.6f : 0.0f;
			hb = (paused || d.foot) && !fast;
		}
		const int  flt = g_cfg.driveFilter >= 0 ? g_cfg.driveFilter
		                 : rawIn ? std::max(0, d.lastDev)
		                         : (Call<int>(N::IS_USING_KEYBOARD_AND_MOUSE, 2) ? 0 : 1);
		const float str = d.sign * lr;
		{
			char msg[1024];
			snprintf(msg, sizeof(msg), "{\"t\":\"drive\",\"q\":%u,\"thr\":%.3f,\"brk\":%.3f,\"str\":%.3f,\"hb\":%d,\"flt\":%d,\"pad\":\"%s\",\"keys\":\"%s\",\"m\":%d,\"p\":%d,\"gp\":%d,\"out\":%d}",
				++d.sendSeq, thr, brk, str, hb ? 1 : 0, flt, padStr, keyStr.c_str(), d.padGta ? 1 : 0, (paused || d.state == 3 || d.foot) ? 1 : 0,  // v2.4: state 3 = GTA's ground being built: brakes on
				(paused && g_cfg.pauseSync) ? 1 : 0, d.foot ? 1 : 0);  // v2.43: GTA paused: BeamNG pauses too; v2.61: you got out
			DrvSend(msg);
		}
		d.thr = thr;
		d.brk = brk;
		d.str = str;
		d.hb = hb;
		d.flt = flt;
		d.lastLr = lr;
		d.lastHb = hb;

		// ---- what the BeamNG car does (the mod sends it 5 times a second)
		{
			std::string vs;
			uint32_t    vc;
			AcquireSRWLockShared(&g_dnet.lock);
			vc = g_dnet.vsCount;
			if (vc != d.vsSeen) vs = g_dnet.vs;
			ReleaseSRWLockShared(&g_dnet.lock);
			if (vc != d.vsSeen) {
				d.vsSeen = vc;
				d.vsHave = true;
				d.vsT = now;
				JsonNumC(vs.c_str(), "thr", d.vThr);
				JsonNumC(vs.c_str(), "brk", d.vBrk);
				JsonNumC(vs.c_str(), "str", d.vStr);
				JsonNumC(vs.c_str(), "pb", d.vPb);
				JsonNumC(vs.c_str(), "clu", d.vClu);
				JsonNumC(vs.c_str(), "rpm", d.vRpm);
				JsonNumC(vs.c_str(), "mx", d.vMx);
				JsonNumC(vs.c_str(), "ws", d.vWs);
				JsonNumC(vs.c_str(), "li", d.vLi);  // v2.3: 1 low beams, 2 high beams, 4 brake lights, 8 reverse, 16/32 signals
				JsonStr(vs, "gear", d.vGear);
				JsonStr(vs, "ign", d.vIgn);
				JsonStr(vs, "eng", d.vEng);
				JsonStr(vs, "gbm", d.vGbm);
				JsonStr(vs, "esc", d.vEsc);  // v2.5
				std::string la;
				if (JsonStr(vs, "la", la) && !la.empty()) {
					d.vLa = la;
					d.laT = now;
				}
			}
		}

		// ---- BeamNG's poses
		DrvPull();
		if (d.beamPaused && d.state == 2) {
			// v2.43: BeamNG paused: the car waits where it is drawn (no poses come; the clock starts again when they do)
			d.lastArrival = std::max(d.lastArrival, now);
			if (d.anchored && g_cfg.driveSetVelocity) CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);
			return;
		}
		if (now - d.lastArrival > (now - g_twLoadT < 2.0 ? 15.0 : 5.0)) {  // (v2.72: BeamNG freezes while it spawns a twin)
			DriveStop("no car data from BeamNG for 5 s", true);
			return;
		}
		if (d.poses.empty() || now - d.lastArrival > 1.0) {
			if (!d.stale && now - d.lastArrival > 1.0) {
				d.stale = true;
				Notify("Drive: no car data from BeamNG for 1 s (paused, minimised or busy?).");
				Log("drive: no poses for %.1f s", now - d.lastArrival);
				if (g_cfg.driveSetVelocity) CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);
			}
			return;
		}
		if (d.stale) {
			d.stale = false;
			Log("drive: poses again after a gap");
		}
		// v2.4: GTA's ground being built, BeamNG's car not on it yet
		if (d.state == 3) {
			DriveSetupTick(c, now);
			return;
		}
		// clock: the quickest delivery over the last 2 s says how BeamNG's clock maps to ours
		double mn = 1e30;
		for (const DrvPose& p : d.poses)
			if (p.rt > now - 2.0) mn = std::min(mn, p.rt - p.st);
		// v2.3: the clock and the delay move slowly (v2.2 could shift them by up to 20 % of the car's speed for a moment:
		// a rubber band each time a late pose came or left the window)
		const double ftc = std::clamp(double(Call<float>(N::GET_FRAME_TIME)), 0.0, 0.1);
		if (mn < 1e29) {
			if (!d.haveOff || fabs(mn - d.off) > 0.25) {
				d.off = mn;
				d.haveOff = true;
			} else {
				const double r = 0.03 * ftc;  // at most 3 % faster or slower (v2.42: was 1 %; BeamNG's simulation clock may run a little slow when it's busy)
				d.off += std::clamp(mn - d.off, -r, r);
			}
		}
		// drawn this far behind: one BeamNG frame plus the delivery jitter (auto), or the ini's value
		{
			const size_t n = d.poses.size();
			double       iv = 0.016;
			if (n >= 3) {
				std::vector<double> ivs, jit;
				for (size_t i = (n > 41 ? n - 40 : 1); i < n; ++i) ivs.push_back(d.poses[i].st - d.poses[i - 1].st);
				for (size_t i = 0; i < n; ++i) jit.push_back(d.poses[i].rt - d.poses[i].st - d.off);  // v2.3: all kept poses (~1.5 s)
				std::sort(ivs.begin(), ivs.end());
				std::sort(jit.begin(), jit.end());
				iv = ivs[ivs.size() / 2];
				const double j90 = std::max(0.0, jit[jit.size() * 9 / 10]);
				const double want = std::clamp(iv + j90 + 0.002, 0.008, 0.15);
				if (g_cfg.driveDelayMs >= 0) {
					d.delay = g_cfg.driveDelayMs / 1000.0;
				} else if (!d.delayInit) {
					d.delay = want;
					d.delayInit = true;
				} else {
					// v2.3: up at most 3 % of real time, down at most 0.5 % (no visible speed change). v2.54: down 1.5 %: your
					// 2.53 log had it creep from 50 to 140 ms behind and stay there (BeamNG's hitches while it rebuilt swapped
					// tiles); that much delay is felt as lag when steering slowly
					d.delay += std::clamp(want - d.delay, -0.015 * ftc, 0.03 * ftc);
				}
				d.delayLo = std::min(d.delayLo, d.delay);
				d.delayHi = std::max(d.delayHi, d.delay);
			}
			d.ivLast = iv;
		}

		// ---- steering sign: GTA's left (-1) must turn the car left. Learned from how the car turns.
		if (!d.signLocked && d.newPoses && d.poses.size() >= 2) {
			for (size_t i = d.poses.size() - std::min<size_t>(d.newPoses, d.poses.size() - 1); i < d.poses.size(); ++i) {
				const DrvPose& a = d.poses[i - 1];
				const DrvPose& b = d.poses[i];
				const double   dt = b.st - a.st;
				if (dt <= 0.001 || dt > 0.2) continue;
				double cr[3];
				Cross3(a.f, b.f, cr);
				const double yawRate = atan2(Dot3(cr, b.u), Dot3(a.f, b.f)) / dt;  // + = turning left
				const double vf = Dot3(b.v, b.f);
				if (fabs(d.lastLr) < 0.3 || d.lastHb || fabs(vf) < 3.0 || fabs(vf) > 35.0 || fabs(yawRate) < 0.03) continue;
				++d.signN;
				if (yawRate * d.lastLr * vf > 0) ++d.signWrong;
			}
			if (d.signN >= 40) {
				const double wrong = double(d.signWrong) / d.signN;
				if (wrong > 0.75) {
					d.sign = -d.sign;
					d.signLocked = true;
					Log("drive: steering was the wrong way round in %.0f%% of %d samples: steering sign now %+.0f (DriveSteerSign=%+.0f fixes it)", wrong * 100.0, d.signN, d.sign,
						d.sign);
					Notify("Drive: steering direction corrected (BeamNG's is the other way round).");
				} else if (wrong < 0.25) {
					d.signLocked = true;
					Log("drive: steering direction confirmed (%.0f%% of %d samples the wrong way): sign %+.0f", wrong * 100.0, d.signN, d.sign);
				} else if (d.signN > 400) {
					d.signN = d.signWrong = 0;  // unclear (drifting?): look again
				}
			}
		}

		DrvOriginUpdate(now);  // v2.4: a function (also used while GTA's ground is built)

		// ---- the wheels drawn this tick: from last tick's pose, like the body GTA reports now
		c.bngWheels = d.wHave;
		if (d.wHave) {
			memcpy(c.bwOk, d.wOk, sizeof(c.bwOk));
			memcpy(c.bwC, d.wC, sizeof(c.bwC));
			memcpy(c.bwA, d.wA, sizeof(c.bwA));
			memcpy(c.bwAng, d.wAng, sizeof(c.bwAng));
		}

		// ---- the read-back check: did GTA take the rotation we set last tick?
		if (d.setValid && d.anchored) {
			const double F[3] = { c.F[0], c.F[1], c.F[2] }, U[3] = { c.U[0], c.U[1], c.U[2] };
			const double ef = acos(std::clamp(Dot3(F, d.setF), -1.0, 1.0)) / kDeg, eu = acos(std::clamp(Dot3(U, d.setU), -1.0, 1.0)) / kDeg;
			const double err = std::max(ef, eu);
			d.rotErrMax = std::max(d.rotErrMax, err);
			++d.rotChecks;
			d.rotBad += err > 4.0 ? 1 : 0;
			if (d.rotChecks >= 30) {
				if (d.rotBad > 15 && !d.rotFailed) {
					if (d.rotMode == 0) {
						d.rotMode = 1;
						Log("drive: GTA's car didn't take the rotation as Euler angles (%d of %d ticks off by > 4 deg, max %.1f): using quaternions", d.rotBad, d.rotChecks, d.rotErrMax);
					} else {
						d.rotFailed = true;
						Log("drive: GTA's car doesn't take the rotation as quaternions either (%d of %d ticks off by > 4 deg, max %.1f)", d.rotBad, d.rotChecks, d.rotErrMax);
						Notify("Drive: GTA's car won't turn the way BeamNG's does. Please send GtaBeamD1.log.");
					}
				}
				d.rotChecks = d.rotBad = 0;
			}
		}

		// ---- BeamNG time to draw now: on GTA's frame clock (the car moves in the same steps as GTA's world,
		// whenever in the frame this script runs). v2.5: GTA in slow motion (the radio or weapon wheel, a special ability):
		// GTA's frame time runs slow but BeamNG doesn't (its speed follows GTA's in a later version): the car is drawn on the
		// real clock then (your v2.43 test: on GTA's slowed clock it fell behind and was put back in step every few frames:
		// the desync with the radio wheel open)
		const double ftRaw = std::clamp(double(Call<float>(N::GET_FRAME_TIME)), 0.0, 0.1);
		const double realDt = d.lastDrawNow > 0 ? std::clamp(now - d.lastDrawNow, 0.0, 0.1) : ftRaw;
		d.lastDrawNow = now;
		// (GTA's time against the real time, smoothed over about a quarter second: slow motion lasts, a hitch is one frame)
		if (!paused && realDt > 0.004) {
			const double a = std::min(1.0, realDt / 0.25);
			d.slowRatio += (std::clamp(ftRaw / realDt, 0.0, 2.0) - d.slowRatio) * a;
		}
		const bool   slowMo = !paused && realDt > 0.004 && d.slowRatio < 0.7;
		const double ft = slowMo ? realDt : ftRaw;
		if (slowMo) {
			d.slowS += realDt;
			++d.slowTicks;
		}
		if (!d.gameInit) {
			d.gameInit = true;
			d.gameT = now;
		} else {
			d.gameT += ft;
			const double err = now - d.gameT;
			if (fabs(err) > 0.1) {
				d.gameT = now;  // a pause or a hitch: start again from the real clock
				++d.gameResets;
			} else {
				d.gameT += err * 0.02;
			}
		}
		const double tB = d.gameT - d.off - d.delay;
		g_dset.beamSt = tB;
		DrvPose      p;
		int          kind = 0;
		if (!d.oDone || !DrvSample(tB, p, kind)) return;
		double r[3], f[3], u[3];
		DrvFrame(p, r, f, u);
		double X[3];
		for (int k = 0; k < 3; ++k) X[k] = p.P[k] + r[k] * d.o[0] + f[k] * d.o[1] + u[k] * d.o[2];
		if (!d.anchored) {
			// BeamNG's car appears exactly where the BeamNG car is drawn now, facing the same way
			d.Ag[0] = c.ox;
			d.Ag[1] = c.oy;
			d.Ag[2] = c.oz;
			memcpy(d.Ab, X, sizeof(d.Ab));
			const double thG = atan2(-double(c.F[0]), double(c.F[1])), thB = atan2(-f[0], f[1]);
			d.th = thG - thB;
			d.cth = cos(d.th);
			d.sth = sin(d.th);
			d.anchored = true;
			Log("drive: anchored: BeamNG %.2f %.2f %.2f (heading %.1f deg) = GTA %.2f %.2f %.2f (heading %.1f deg); BeamNG's world turned %.1f deg; drawn %.0f ms behind BeamNG",
				X[0], X[1], X[2], thB / kDeg, d.Ag[0], d.Ag[1], d.Ag[2], thG / kDeg, d.th / kDeg, d.delay * 1000.0);
		}
		// v2.3 DriveMotion=filter: BeamNG's speed carries the car from tick to tick and its position pulls it back
		// gently (DriveFilterMs). BeamNG's positions are taken on its frame clock while its physics runs on its own,
		// so they wobble by a few cm against their timestamps; placing the car on them directly (v2.2) made GTA's
		// camera see those cm as jitter. The speed comes from the physics itself and is steady.
		double Xd[3] = { X[0], X[1], X[2] }, Vd[3] = { p.v[0], p.v[1], p.v[2] };
		if (g_cfg.driveMotion == 1 && d.anchored) {
			// the drawn origin's own speed: BeamNG's (its reference point) + the turn of the car around it
			double vX[3] = { p.v[0], p.v[1], p.v[2] };
			if (ft > 0.0005) {
				DrvPose p3;
				int     k3 = 0;
				if (DrvSample(tB + ft, p3, k3)) {
					double r3[3], f3[3], u3[3];
					DrvFrame(p3, r3, f3, u3);
					for (int k = 0; k < 3; ++k) vX[k] += ((r3[k] - r[k]) * d.o[0] + (f3[k] - f[k]) * d.o[1] + (u3[k] - u[k]) * d.o[2]) / ft;
				}
			}
			double e2 = 0;
			for (int k = 0; k < 3; ++k) e2 += (X[k] - d.fX[k]) * (X[k] - d.fX[k]);
			if (!d.fOk || e2 > 4.0 || kind == 3) {
				memcpy(d.fX, X, sizeof(d.fX));
				if (d.fOk) ++d.fSnaps;
				d.fOk = true;
			} else {
				const double tau = g_cfg.driveFilterMs / 1000.0;
				const double kf = 1.0 - exp(-ft / tau);
				double       e[3];
				for (int k = 0; k < 3; ++k) {
					d.fX[k] += vX[k] * ft;           // carried by BeamNG's speed
					e[k] = X[k] - d.fX[k];
					d.fX[k] += e[k] * kf;            // pulled onto BeamNG's position
				}
				const double el = sqrt(e[0] * e[0] + e[1] * e[1] + e[2] * e[2]);
				d.fErrSum += el;
				d.fErrMax = std::max(d.fErrMax, el);
				++d.fErrN;
				for (int k = 0; k < 3; ++k) {
					Xd[k] = d.fX[k];
					Vd[k] = vX[k] + (X[k] - d.fX[k]) / tau;  // where the next tick will put it
				}
			}
		}
		// v2.42: how steady the drawn car moves: its speed from tick to tick against BeamNG's (a rubber band = wobble)
		{
			const double vb = sqrt(p.v[0] * p.v[0] + p.v[1] * p.v[1]);  // (level: a rubber band is along the road)
			if (d.wobOk && ft > 0.001 && vb > 10.0) {
				const double sp = sqrt((Xd[0] - d.wobX[0]) * (Xd[0] - d.wobX[0]) + (Xd[1] - d.wobX[1]) * (Xd[1] - d.wobX[1])) / ft;
				const double e = fabs(sp - vb);
				if (e < 30.0) {
					d.wobSum += e;
					d.wobMax = std::max(d.wobMax, e);
					++d.wobN;
					if (d.wobAll.size() < 4000) d.wobAll.push_back(float(e));
				}
			}
			memcpy(d.wobX, Xd, sizeof(d.wobX));
			d.wobOk = true;
			if (d.plog && d.plogRows < 150000) {
				fprintf(d.plog, "T,%.5f,%.5f,%.5f,%d,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.5f\n", now, d.gameT, tB, kind, X[0], X[1], X[2], Xd[0], Xd[1], Xd[2], ft);
				++d.plogRows;
			}
		}
		double O[3], R[3], F[3], U[3], V[3];
		DrvMapPoint(Xd, O);
		DrvMapDir(r, R);
		DrvMapDir(f, F);
		DrvMapDir(u, U);
		DrvMapDir(Vd, V);
		if (g_cfg.driveSetVelocity && ft > 0) {
			// GTA moves the car with V during its next step; the rotation is set for where that step ends
			DrvPose p2;
			int     k2 = 0;
			if (DrvSample(tB + ft, p2, k2)) {
				double r2[3], f2[3], u2[3];
				DrvFrame(p2, r2, f2, u2);
				DrvMapDir(r2, R);
				DrvMapDir(f2, F);
				DrvMapDir(u2, U);
			}
		}
		if (!g_cfg.driveSetVelocity) V[0] = V[1] = V[2] = 0;
		// v2.4: GTA's ground streams around where the drawn car is and goes; F10 off hands this speed to GTA's car
		{
			double Vw[3];
			DrvMapDir(Vd, Vw);
			memcpy(d.lastV, Vw, sizeof(d.lastV));
			d.lastVOk = true;
			// v2.71: not while BeamNG's car is under GTA's ground (fell through a gap, waiting to be put back): the levels around it
			// were measured for the falling car (your test 2 log: the deck's tiles were swapped for the river's level, the
			// put-back spot with them: it fell again)
			if (d.fixedMap && d.underT <= 0) WorldCar(O, Vw, U);
			// v2.5: for the props it hits
			memcpy(d.carO, O, sizeof(d.carO));
			memcpy(d.carR, R, sizeof(d.carR));
			memcpy(d.carF, F, sizeof(d.carF));
			memcpy(d.carU, U, sizeof(d.carU));
			memcpy(d.carV, Vw, sizeof(d.carV));
			d.carPoseOk = true;
		}
		// v2.4: fall-through safety: BeamNG's car well under GTA's ground (a gap in the tiles) -> back on the last good spot
		if (d.fixedMap) {
			double gz;
			bool   acked = false;
			bool   have = WorldGroundAt(O[0], O[1], gz, &acked);
			if (!have && d.goodOk) {
				// over a tile GTA has no ground for (a gap: nothing was sent there): compare with the last ground it stood on
				gz = d.goodX[2];
				have = O[2] < gz - g_cfg.worldFallM;
			}
			if (have && O[2] < gz - g_cfg.worldFallM && now > d.fallGraceT && gz - O[2] < 80.0) {
				// v2.51: only a fall THROUGH GTA's ground counts: still over GTA's ground (a jump off a ramp or a deck's edge, onto ground
				// not built yet) is no reset (your v2.5 jump: put back onto the ramp). Its ground there comes first (WorldAir).
				// v2.62: checked down to 80 m under (was 25: a fall off a high bridge was put back)
				double       p[3], nrm[3];
				const double a[3] = { O[0], O[1], O[2] + 1.0 }, b[3] = { O[0], O[1], O[2] - 80.0 };
				if (MapRay(a, b, p, nrm) == 1) {
					have = false;
					++g_world.noFall;
				}
			}
			// v2.62: put back only after it stays under GTA's ground for WorldFallWaitS (your 2.61 crashes: a car tumbling or
			// flying low was put back before you could watch it), or at once when it is 40 m under (it really fell through)
			const bool under = have && O[2] < gz - g_cfg.worldFallM && now > d.fallGraceT;
			if (!under) {
				if (d.underT > 0 && now - d.underT > 0.2 && d.underLogs < 20) {
					++d.underLogs;
					Log("drive: BeamNG's car was under GTA's ground for %.1f s and came back up: not put back (WorldFallWaitS %.1f)", now - d.underT, g_cfg.worldFallWaitS);
				}
				d.underT = 0;
			} else if (d.underT <= 0) d.underT = now;
			if (have) {
				if (under && (now - d.underT >= g_cfg.worldFallWaitS || gz - O[2] > 40.0)) {
					const double underFor = now - d.underT;
					d.underT = 0;
					// v2.41: your v2.4 test looped here: the ground was measured where the fallen car was, not where it was put
					// back, so it fell again at once (the GTA car floating, BeamNG's car hurt). Now the hidden GTA car goes to the
					// last good spot first, the ground is built there (from the measured tiles: no rays), BeamNG's car follows
					// when it's built, and 3 falls in 30 s end drive mode instead of looping.
					d.fallT = now;
					++d.falls;
					d.fallLog.push_back(now);
					while (!d.fallLog.empty() && d.fallLog.front() < now - 30.0) d.fallLog.erase(d.fallLog.begin());
					Log("drive: BeamNG's car fell through GTA's ground (%.1f m under it at %.1f %.1f for %.1f s; tile there %s; fall %zu in 30 s)", gz - O[2], O[0], O[1],
						underFor, acked ? "built" : "NOT built yet", d.fallLog.size());
					WorldDumpAround(g_world.cx, g_world.cy, g_world.cz, "where it was last on the ground");  // v2.71 (the world froze there)
					if (d.fallLog.size() >= 3) {
						DriveStop("BeamNG's car fell through GTA's ground 3 times in 30 s: send GtaBeamD1.log", true);
						return;
					}
					double X[3] = { O[0], O[1], gz }, Fg[3] = { F[0], F[1], F[2] }, Ug[3] = { 0, 0, 1 };
					if (d.goodOk) {
						memcpy(X, d.goodX, sizeof(X));
						memcpy(Fg, d.goodF, sizeof(Fg));
						memcpy(Ug, d.goodU, sizeof(Ug));
					}
					Log("drive: putting it back on the last good spot (%.1f s ago, %.0f m away) when GTA's ground is built there", d.goodOk ? now - d.goodT : -1.0,
						hypot(X[0] - O[0], X[1] - O[1]));
					Notify("Drive: BeamNG's car fell through a gap in GTA's ground: put back.");
					// the hidden GTA car (no gravity, no collision) waits on that spot: GTA's camera and streaming go there too
					double Rg[3];
					Cross3(Fg, Ug, Rg);
					const double V0[3] = { 0, 0, 0 };
					DrvSetPuppet(X, Rg, Fg, Ug, V0);
					CallVoid(N::SET_ENTITY_VELOCITY, d.veh, 0.0f, 0.0f, 0.0f);
					d.lastVOk = false;
					d.state = 3;
					d.tpSent = false;
					d.tpWhy = "fell";
					d.tpFirstT = now;
					memcpy(d.tpX, X, sizeof(d.tpX));
					memcpy(d.tpF, Fg, sizeof(d.tpF));
					memcpy(d.tpU, Ug, sizeof(d.tpU));
					WorldCar(X, V0, Ug);
					return;
				}
				if (fabs(O[2] - gz) < 0.6 && now - d.goodT > 0.25) {
					d.goodT = now;
					d.goodOk = true;
					d.goodX[0] = O[0];
					d.goodX[1] = O[1];
					d.goodX[2] = gz;
					memcpy(d.goodF, F, sizeof(d.goodF));
					memcpy(d.goodU, U, sizeof(d.goodU));
				}
			}
		}
		DrvSetPuppet(O, R, F, U, V);

		// ---- wheels (car space) for the next tick's drawing
		const double dtT = d.lastTickT > 0 ? std::min(0.1, now - d.lastTickT) : 0.0;
		d.lastTickT = now;
		const double vf = Dot3(p.v, f);
		d.speed = vf;
		d.speedAbs = sqrt(Dot3(p.v, p.v));
		bool any = false;
		for (int w = 0; w < kDrvWheels; ++w) d.wOk[w] = false;
		for (int w = 0; w < p.nw && w < d.nwB; ++w) {
			const int j = d.map[w];
			if (j < 0) continue;
			for (int k = 0; k < 3; ++k) {
				d.wC[j][k] = float(p.w[w][k] - d.o[k]);
				d.wA[j][k] = p.w[w][3 + k];
			}
			if (p.spin) {
				if (!d.ang0Set[w]) {
					d.ang0Set[w] = true;
					d.ang0[w] = p.ang[w];
				}
				d.wAng[j] = float(fmod(p.ang[w] - d.ang0[w], 2.0 * kPi));
				d.wAngU[j] = p.ang[w];
			} else {
				d.roll[j] -= vf * dtT / std::max(0.1, d.m.radius[j]);
				d.roll[j] = fmod(d.roll[j], 2.0 * kPi);
				d.wAng[j] = float(d.roll[j]);
			}
			d.wOk[j] = true;
			any = true;
		}
		d.wHave = any;
		TireFxTick(O, R, F, U, p.spin, dtT);  // v2.73

		// ---- stats
		++d.ticks;
		d.interp += kind == 0;
		d.extrap += kind == 1;
		d.held += kind == 2;
		d.early += kind == 3;
		d.delaySum += d.delay;
		const ULONGLONG nowMs = GetTickCount64();
		if (!d.logMs) d.logMs = nowMs;
		if (nowMs - d.logMs >= 10000) {
			uint32_t bad, dropped;
			AcquireSRWLockShared(&g_dnet.lock);
			bad = g_dnet.bad;
			dropped = g_dnet.dropped;
			ReleaseSRWLockShared(&g_dnet.lock);
			const double secs = (nowMs - d.logMs) / 1000.0;
			Log("drive: GTA moved its car after we placed it (read-back vs set + speed x GTA's step): average %.2f cm, max %.2f cm over %u ticks; drawn pose: %s; camera slip fix in drive mode %s",
				g_dset.driftN ? g_dset.driftSum / g_dset.driftN * 100.0 : 0.0, g_dset.driftMax * 100.0, g_dset.driftN, g_cfg.driveDrawPose ? "set" : "read-back",
				g_cfg.driveSlipFix ? "on" : "off");
			g_dset.driftSum = g_dset.driftMax = 0;
			g_dset.driftN = 0;
			// v2.3
			double wob95 = 0;
			if (!d.wobAll.empty()) {
				std::sort(d.wobAll.begin(), d.wobAll.end());
				wob95 = d.wobAll[d.wobAll.size() * 95 / 100];
			}
			Log("drive: steadiness above 36 km/h (v2.42): the drawn car's speed from tick to tick vs BeamNG's: %.2f km/h off on average, 95%% within %.2f, max %.2f (%u ticks; "
			    "a rubber band shows here); poses timed by BeamNG's %s clock",
				d.wobN ? d.wobSum / d.wobN * 3.6 : 0.0, wob95 * 3.6, d.wobMax * 3.6, d.wobN, g_cfg.drivePoseClock ? "simulation" : "frame");
			d.wobSum = d.wobMax = 0;
			d.wobN = 0;
			d.wobAll.clear();
			Log("drive: smoothness: the car is put %.2f cm (max %.2f) from where GTA's step took it; motion %s (BeamNG's position %.1f cm off on average, "
			    "max %.1f, %u snaps); BeamNG's positions vs its own speed x time: on its frame clock %.1f cm average, max %.1f; on its simulation clock %.1f cm average, "
			    "max %.1f, %u samples; poses later than 2.5 frames: %u, longest gap %.0f ms; drawn %.0f-%.0f ms behind",
				g_dset.corrN ? g_dset.corrSum / g_dset.corrN * 100.0 : 0.0, g_dset.corrMax * 100.0, g_cfg.driveMotion ? "filter" : "direct",
				d.fErrN ? d.fErrSum / d.fErrN * 100.0 : 0.0, d.fErrMax * 100.0, d.fSnaps, d.tnN ? d.tnSum / d.tnN * 100.0 : 0.0, d.tnMax * 100.0,
				d.tnN2 ? d.tnSum2 / d.tnN2 * 100.0 : 0.0, d.tnMax2 * 100.0, d.tnN2, d.gaps, d.gapMax * 1000.0, d.delayLo < 1e8 ? d.delayLo * 1000.0 : 0.0,
				d.delayHi * 1000.0);
			d.tnSum2 = d.tnMax2 = 0;
			d.tnN2 = 0;
			g_dset.corrSum = g_dset.corrMax = 0;
			g_dset.corrN = 0;
			d.fErrSum = d.fErrMax = d.tnSum = d.tnMax = d.gapMax = d.delayHi = 0;
			d.fErrN = d.tnN = d.gaps = 0;
			d.delayLo = 1e9;
			Log("drive: %u poses in %.0f s (%.0f a second, BeamNG frame %.1f ms); drawn %.0f ms behind; ticks %u: between poses %u, past the newest %u, held %u, before the oldest %u; "
			    "bad %u, late %u; frame clock resets %u, clock found again after a BeamNG pause %u; input thr %.2f brk %.2f steer %+.2f handbrake %d filter %d; steering sign %+.0f (%s); rotation %s, check max %.1f deg; speed %.1f m/s; GTA car %.1f %.1f %.1f",
				d.rx, secs, d.rx / secs, d.ivLast * 1000.0, d.ticks ? d.delaySum / d.ticks * 1000.0 : 0.0, d.ticks, d.interp, d.extrap, d.held, d.early, bad, dropped, d.gameResets, d.clockJumps, d.thr,
				d.brk, d.str, d.hb, d.flt, d.sign, d.signLocked ? (g_cfg.driveSteerSign != 0 ? "ini" : "learned") : "learning", d.rotMode ? "quaternion" : "Euler", d.rotErrMax,
				d.speed, O[0], O[1], O[2]);
			if (d.slowTicks) {
				// v2.5
				Log("drive: GTA ran in slow motion for %.1f s of these (radio or weapon wheel, special ability): the car kept BeamNG's real time; drive mode %s",
					d.slowS, d.vEsc.empty() ? "-" : d.vEsc.c_str());
				d.slowS = 0;
				d.slowTicks = 0;
			}
			// v2.01: what went to BeamNG and what the BeamNG car did
			{
				std::string seen, owned;
				for (int c = 0; c < kPadCtls; ++c) {
					if (d.ctlSeen & (1u << c)) seen += std::string(seen.empty() ? "" : " ") + kPadName[c];
					if (d.owner & (1u << c)) owned += std::string(owned.empty() ? "" : " ") + kPadName[c];
				}
				Log("drive input: %s; pad %s%s (%s mode; BeamNG gets: %s), moved: %s, max RT %.2f LT %.2f |LX| %.2f; keys held: %s; GTA controls off: %d; "
				    "BeamNG car: throttle %.2f brake %.2f steering %+.2f clutch %.2f parking brake %.2f, gear %s, %.0f rpm, %.1f m/s, ignition %s, engine %s, gearbox %s (%s); actions: %s",
					d.inpHow.c_str(), g_in.pad.ok ? "#" : "none", g_in.pad.ok ? std::to_string(g_in.pad.index).c_str() : "", d.padGta ? "GTA" : "BeamNG",
					owned.empty() ? "nothing" : owned.c_str(), seen.empty() ? "nothing" : seen.c_str(), d.mxRt, d.mxLt, d.mxLx, d.keysSeen.empty() ? "none" : d.keysSeen.c_str(),
					d.blocked, d.vThr, d.vBrk, d.vStr, d.vClu, d.vPb, d.vGear.empty() ? "?" : d.vGear.c_str(), d.vRpm, d.vWs, d.vIgn.c_str(), d.vEng.c_str(),
					d.vGbm.c_str(), d.vsHave ? (now - d.vsT < 1.0 ? "live" : "OLD") : "NOTHING from BeamNG yet", d.vLa.empty() ? "none" : d.vLa.c_str());
				d.mxRt = d.mxLt = d.mxLx = 0;
				d.ctlSeen = 0;
				d.keysSeen.clear();
			}
			d.logMs = nowMs;
			d.rx = d.ticks = d.interp = d.extrap = d.held = d.early = 0;
			d.delaySum = 0;
			d.rotErrMax = 0;
		}
	}

	// v2.01: the drive check on screen (Shift+F10): what goes to BeamNG, what the BeamNG car does
	void DriveHud(bool paused)
	{
		const DriveState& d = g_drv;
		if (d.state == 0 || !g_cfg.driveHud) return;
		const double now = NowSeconds();
		char         l1[256], l2[256], l3[256];
		if (d.state == 1) {
			snprintf(l1, sizeof(l1), "BeamNG drive: waiting for BeamNG (is the 2.01 BeamNG mod running, in a car?)");
			DrawLine(0.16f, 0.88f, l1, 255, 220, 120);
			return;
		}
		const bool binds = d.inpHow.find("binds") != std::string::npos;
		const PadNow& p = g_in.pad;
		snprintf(l1, sizeof(l1), "BeamNG drive | %s | pad: %s | %s: %s", binds ? "your BeamNG controls" : "GTA's basic controls (no BeamNG bindings)",
			!g_cfg.driveInput ? "through GTA" : p.ok ? (d.padGta ? "GTA mode (aim/shoot)" : "BeamNG mode") : "none found (XInput)",
			g_cfg.padToggle >= 0 ? kPadName[g_cfg.padToggle] : "-", d.padGta ? "back to BeamNG" : "GTA mode");
		std::string btn;
		for (int c = 0; c < 14; ++c)
			if (d.sentBtn & kPadBit[c]) btn += std::string(btn.empty() ? "" : " ") + kPadName[c];
		std::string keys = d.keysNow.size() > 2 ? d.keysNow.substr(1, std::min<size_t>(60, d.keysNow.size() - 2)) : "";
		snprintf(l2, sizeof(l2), "to BeamNG: RT %.2f LT %.2f stick %+.2f%s%s%s%s%s", d.sentRt, d.sentLt, d.sentLx, btn.empty() ? "" : " | ", btn.c_str(), keys.empty() ? "" : " | keys ",
			keys.c_str(), paused ? " | PAUSED: brakes on" : "");
		if (!d.vsHave || now - d.vsT > 1.0) {
			snprintf(l3, sizeof(l3), "BeamNG car: %s", d.vsHave ? "no news for over 1 s" : "nothing yet (BeamNG mod 2.01?)");
		} else {
			snprintf(l3, sizeof(l3), "BeamNG car: throttle %.2f brake %.2f steer %+.2f clutch %.2f pbrake %.2f | gear %s | %.0f rpm | %.0f km/h | engine %s | %s",
				d.vThr, d.vBrk, d.vStr, d.vClu, d.vPb, d.vGear.empty() ? "?" : d.vGear.c_str(), d.vRpm, d.vWs * 3.6, d.vEng.c_str(), d.vGbm.c_str());
		}
		DrawLine(0.16f, 0.855f, l1);
		DrawLine(0.16f, 0.88f, l2, 200, 230, 255);
		DrawLine(0.16f, 0.905f, l3, 200, 255, 200);
		if (!d.vLa.empty() && now - d.laT < 3.0) {
			char l4[256];
			snprintf(l4, sizeof(l4), "BeamNG ran: %.200s", d.vLa.c_str());
			DrawLine(0.16f, 0.93f, l4, 255, 230, 160);
		}
	}

	// ---------------------------------------------------------------------------------------
	// v2.1: the BeamNG gauge (bottom right): speed, gear, rev bar of the BeamNG car, and the pad mode.
	// Speed from BeamNG's poses (60+ a second), gear and rpm from its car state (10 a second, smoothed).
	// ---------------------------------------------------------------------------------------
	void GaugeText(float x, float y, float scale, int font, int just, float wrapR, const char* t, int r, int g, int b, int a = 255)
	{
		CallVoid(N::SET_TEXT_FONT, font);
		CallVoid(N::SET_TEXT_SCALE, 0.0f, scale);
		CallVoid(N::SET_TEXT_COLOUR, r, g, b, a);
		CallVoid(N::SET_TEXT_OUTLINE);
		if (just == 2) {  // right-justified text needs a wrap area ending at its right edge; left is GTA's default
			CallVoid(N::SET_TEXT_JUSTIFICATION, 2);
			CallVoid(N::SET_TEXT_WRAP, 0.0f, wrapR);
		}
		CallVoid(N::BEGIN_TEXT_COMMAND_DISPLAY_TEXT, "STRING");
		CallVoid(N::ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME, t);
		CallVoid(N::END_TEXT_COMMAND_DISPLAY_TEXT, x, y, 0);
	}

	void DriveGauge(bool paused)
	{
		DriveState& d = g_drv;
		if (d.state != 2 || !g_cfg.gauge || paused || Call<int>(N::IS_HUD_HIDDEN)) return;
		const double now = NowSeconds();
		const double dt = d.gaugeT > 0 ? std::clamp(now - d.gaugeT, 0.0, 0.1) : 0.0;
		d.gaugeT = now;
		const bool live = d.vsHave && now - d.vsT < 2.0;
		if (live && d.vRpm >= 0) {
			d.rpmShow += (d.vRpm - d.rpmShow) * std::min(1.0, dt * 12.0);
			d.rpmMaxSeen = std::max(d.rpmMaxSeen, d.vRpm);
		}
		const double redline = d.vMx > 1500 ? d.vMx : std::max(6500.0, d.rpmMaxSeen * 1.02);
		const float  sz = std::clamp(Call<float>(N::GET_SAFE_ZONE_SIZE), 0.8f, 1.0f);
		const float  xr = 1.0f - (1.0f - sz) * 0.5f - 0.012f;  // right edge
		const float  yb = 1.0f - (1.0f - sz) * 0.5f - 0.135f;  // bottom edge: above GTA's vehicle / area / street names
		const float  w = 0.13f, h = 0.082f;
		CallVoid(N::DRAW_RECT, xr - w * 0.5f, yb - h * 0.5f, w, h, 0, 0, 0, 150, 0);
		// speed (big, right) + unit
		char buf[32];
		const double v = d.speedAbs * (g_cfg.gaugeKmh ? 3.6 : 2.23694);
		snprintf(buf, sizeof(buf), "%d", int(v + 0.5));
		GaugeText(0.0f, yb - h + 0.002f, 0.82f, 4, 2, xr - 0.034f, buf, 255, 255, 255);
		GaugeText(xr - 0.032f, yb - h + 0.032f, 0.30f, 4, 1, 0.0f, g_cfg.gaugeKmh ? "KM/H" : "MPH", 200, 200, 200);
		// gear (big, left): N green, R red
		std::string gear = live && !d.vGear.empty() && d.vGear != "nil" ? d.vGear.substr(0, 3) : "-";
		const bool  gN = gear == "N", gR = gear == "R";
		GaugeText(xr - w + 0.006f, yb - h + 0.002f, 0.82f, 4, 1, 0.0f, gear.c_str(), gR ? 255 : gN ? 120 : 255, gN ? 230 : gR ? 90 : 255, gN ? 120 : gR ? 80 : 255);
		// rev bar: white, orange from 80 %, red from 92 % (shift light)
		const float x0 = xr - w + 0.006f, bw = w - 0.012f, by = yb - 0.012f, bh = 0.009f;
		const float f = float(std::clamp(d.rpmShow / redline, 0.0, 1.0));
		CallVoid(N::DRAW_RECT, x0 + bw * 0.5f, by, bw, bh, 70, 70, 70, 200, 0);
		if (f > 0.002f) {
			const int r = 255, g = f > 0.92f ? 60 : f > 0.8f ? 150 : 255, b = f > 0.8f ? 40 : 255;
			CallVoid(N::DRAW_RECT, x0 + bw * f * 0.5f, by, bw * f, bh, r, g, b, 230, 0);
		}
		snprintf(buf, sizeof(buf), "%d rpm", live ? int(d.rpmShow / 10.0 + 0.5) * 10 : 0);
		GaugeText(xr - w + 0.006f, yb - h + 0.050f, 0.26f, 4, 1, 0.0f, buf, 190, 190, 190);
		// v2.3: BeamNG's lights (the drawn car doesn't show them yet)
		if (live && d.vLi > 0) {
			const int li = int(d.vLi);
			if (li & 3) GaugeText(0.0f, yb - h + 0.050f, 0.26f, 4, 2, xr - 0.006f, (li & 2) ? "HIGH" : "LIGHTS", (li & 2) ? 120 : 140, (li & 2) ? 170 : 230, 255);
		}
		// v2.5: the car's drive mode above the box, on the right (your ESC modes: Comfort, Sport, ESC & TC off...)
		if (live && !d.vEsc.empty() && d.vEsc != "-" && d.vEsc != "nil") {
			std::string m = d.vEsc.substr(0, 16);
			for (char& ch : m) ch = char(toupper((unsigned char)ch));
			for (char& ch : m)
				if (ch == '_') ch = ' ';
			const bool off = m.find("OFF") != std::string::npos, sport = m.find("SPORT") != std::string::npos || m.find("RACE") != std::string::npos;
			GaugeText(0.0f, yb - h - 0.024f, 0.28f, 4, 2, xr, m.c_str(), 255, off ? 90 : sport ? 170 : 255, off ? 70 : sport ? 40 : 255);
		}
		// pad mode above the box (the D-pad switch)
		if (g_cfg.driveInput && g_in.pad.ok) {
			const bool beam = !d.padGta;
			GaugeText(xr - w, yb - h - 0.024f, 0.28f, 4, 1, 0.0f, beam ? "PAD: BEAMNG CONTROLS" : "PAD: GTA (AIM / SHOOT)", beam ? 120 : 255, beam ? 200 : 170,
				beam ? 255 : 60);
		}
	}

	void TrecStart();  // v1.9.5 timing recording (below)

	void OnKey()
	{
		const bool shift = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;
		const bool ctrl = (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
		const bool alt = (GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
		if (alt) {
			int expected = 0;
			if (g_capture.compare_exchange_strong(expected, 1)) {
				Notify("D1: capturing one frame of GTA's render passes (and 8 s of timing)...");
			}
			TrecStart();
		} else if (ctrl && shift) {
			const int m = (g_viewMode.load() + 1) % kViewModes;
			g_viewMode.store(m);
			char status[128];
			AcquireSRWLockShared(&g_shared);
			memcpy(status, g_status, sizeof(status));
			ReleaseSRWLockShared(&g_shared);
			Notify("D1 view: %s | %s", ViewModeName(m), status);
		} else if (ctrl) {
			LoadConfig();  // placement/look/shadow/hide settings can be tuned in the ini, then Ctrl+F11
			LoadSurfaces();  // v2.4: GtaBeamD1_surfaces.json (tiles sent from now on use it)
			g_props.cfgLoaded = false;  // v2.5: GtaBeamD1_props.json read again
			g_wpn.cfgLoaded = false;    // v2.6: GtaBeamD1_weapons.json too
			g_traffic.cfgLoaded = false;  // v2.7: GtaBeamD1_traffic.json too
			if (g_tgt.mode == 1) {
				SetVisible(g_tgt.veh, false);  // the hide mode may have changed
			}
			Notify("D1: ini re-read (shadows %s, sun offset %.0f deg, reflections %s, wheels %s, debug view %d).", ShadowName(g_cfg.shadowMode), g_cfg.sunAzimuth,
				g_cfg.environment ? "from GTA's world" : "made-up sky", WheelSrcName(g_cfg.wheelSource), g_cfg.debugView);
		} else if (shift) {
			StartWorker(Job::Export);
		} else {
			if (g_drv.state != 0) {
				DriveStop("F11", true);  // v2.0: F11 first ends drive mode
			} else if (g_tgt.mode == 0) {
				Attach();
			} else if (g_tgt.mode == 1) {
				g_tgt.mode = 2;
				SetVisible(g_tgt.veh, true);
				Notify("D1: GTA car shown too (compare). F11 again: detach.");
			} else {
				Detach("key");
				Notify("D1: detached, GTA car back to normal.");
			}
		}
	}

	// ---------------------------------------------------------------------------------------
	// v1.9.4: camera and car in step (the "skip" at speed)
	// The v1.9.3 clip showed the car jumping ~25 px for single frames at speed while GTA's world stayed
	// smooth, and the log showed a fresh car position for every frame: the camera reading and the car
	// reading were sometimes from two different game steps (at 90 km/h one step is ~20 cm). Seen from
	// the car, GTA's camera moves smoothly, so each tick the camera's position in car space is compared
	// with where it was heading; a jump of one (or two) game steps of the car's own motion is a timing
	// slip, and that camera sample is moved back into step with the car (position and heading).
	// Both of GTA's camera readings (final rendered, gameplay) are checked; auto uses the steadier one.
	// ---------------------------------------------------------------------------------------
	// v1.9.5: Alt+F11 also records 8 s of timing: every script tick (game frame number, game timer, car,
	// both camera readings) and every drawn frame (which sample it used) -> GtaBeamD1_timing.csv, to see
	// exactly how GTA's game frames, our samples and the drawn frames line up while driving fast.
	struct TimingRec
	{
		SRWLOCK                  lock = SRWLOCK_INIT;
		std::atomic<int>         state{ 0 };  // 0 off, 1 recording, 2 writing
		double                   t0 = 0, until = 0;
		std::vector<std::string> rows;
	} g_trec;
	void TrecAdd(const char* fmt, ...)
	{
		if (g_trec.state.load() != 1) return;
		char    buf[640];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		AcquireSRWLockExclusive(&g_trec.lock);
		if (g_trec.rows.size() < 40000) g_trec.rows.emplace_back(buf);
		ReleaseSRWLockExclusive(&g_trec.lock);
	}
	void TrecStart()
	{
		int expected = 0;
		if (!g_trec.state.compare_exchange_strong(expected, 2)) return;
		AcquireSRWLockExclusive(&g_trec.lock);
		g_trec.rows.clear();
		g_trec.rows.emplace_back("kind,t_ms,seq,frame,gta_frame,game_timer_ms,frame_time_ms,car_x,car_y,car_z,car_fx,car_fy,car_fz,vel_x,vel_y,vel_z,"
		                         "fcam_x,fcam_y,fcam_z,fcam_pitch,fcam_roll,fcam_yaw,gcam_x,gcam_y,gcam_z,gcam_pitch,gcam_roll,gcam_yaw,slip_final,slip_game,used_x,used_y,used_z,used_yaw,latest_seq,draw_kind,"
		                         "drive,set_x,set_y,set_z,set_vx,set_vy,set_vz,beam_t,drift_cm,drawn_set,slip_applied");
		g_trec.t0 = NowSeconds();
		g_trec.until = g_trec.t0 + 8.0;
		ReleaseSRWLockExclusive(&g_trec.lock);
		g_trec.state.store(1);
	}
	void TrecFinishIfDue()
	{
		if (g_trec.state.load() != 1 || NowSeconds() < g_trec.until) return;
		g_trec.state.store(2);
		std::string text;
		AcquireSRWLockExclusive(&g_trec.lock);
		for (const auto& r : g_trec.rows) {
			text += r;
			text += "\r\n";
		}
		const size_t n = g_trec.rows.size();
		g_trec.rows.clear();
		ReleaseSRWLockExclusive(&g_trec.lock);
		char path[MAX_PATH + 16];
		PathNextToModule("_timing.csv", path, sizeof(path));
		HANDLE f = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
		bool   ok = false;
		if (f != INVALID_HANDLE_VALUE) {
			DWORD w = 0;
			ok = WriteFile(f, text.data(), DWORD(text.size()), &w, nullptr) != 0;
			CloseHandle(f);
		}
		Log("timing recording: %zu rows (8 s of script ticks and drawn frames) -> %s%s", n, path, ok ? "" : " (COULD NOT WRITE)");
		if (ok) Notify("D1: 8 s timing recording saved -> GtaBeamD1_timing.csv in the GTA folder.");
		else Notify("D1: timing recording could not be written (GTA folder read-only?).");
		g_trec.state.store(0);
	}
	inline double TrecMs() { return (NowSeconds() - g_trec.t0) * 1000.0; }

	struct SlipTrack
	{
		double   rel[2][3] = {};  // camera in car space, last two ticks (in step)
		int      have = 0;
		int      quiet = 0;       // ticks in a row without a big deviation: a slip is only believed after 2
		uint32_t slips = 0, ticks = 0;
	};
	SlipTrack g_slipFinal, g_slipGame;
	double    g_prevCarYaw = 0, g_lastSyncT = 0;
	bool      g_prevCarOk = false;
	int       g_camSourceAuto = 0;  // auto's current choice: 0 final, 1 gameplay
	uint32_t  g_slipFixed = 0, g_camDiffN = 0, g_camSameN = 0;
	double    g_camDiffSum = 0;
	ULONGLONG g_slipLogMs = 0;
	CamSample g_rawCar{};       // v1.9.6: this tick's car before pairing (for the timing recording)
	bool      g_pairUsed = false;  // v1.9.6: the car of the tick before is drawn this tick

	// +1/-1 (+2/-2): the camera sample is that many game steps ahead of/behind the car; 0 in step
	int SlipCheck(SlipTrack& t, double px, double py, double pz, const CamSample& c, const double stepC[3], double stepLen, bool reset)
	{
		const double d0[3] = { px - c.ox, py - c.oy, pz - c.oz };
		double       rel[3] = { d0[0] * c.R[0] + d0[1] * c.R[1] + d0[2] * c.R[2], d0[0] * c.F[0] + d0[1] * c.F[1] + d0[2] * c.F[2],
			                    d0[0] * c.U[0] + d0[1] * c.U[1] + d0[2] * c.U[2] };
		if (reset) {
			t.have = 0;
			t.quiet = 0;
		}
		int  slip = 0;
		bool big = false;
		if (t.have >= 2 && stepLen > 0.03) {
			double d[3], dl = 0;
			for (int k = 0; k < 3; ++k) {
				d[k] = rel[k] - (2.0 * t.rel[1][k] - t.rel[0][k]);
				dl += d[k] * d[k];
			}
			dl = sqrt(dl);
			big = dl > 0.5 * stepLen;
			// only after 2 quiet ticks: right after any jump the prediction itself is off for two ticks,
			// and acting on it could start a back-and-forth (sandbox review)
			if (big && t.quiet >= 2) {
				double bestErr = 1e9;
				for (int sgn : { 1, -1, 2, -2 }) {
					double e = 0;
					for (int k = 0; k < 3; ++k) e += (d[k] - sgn * stepC[k]) * (d[k] - sgn * stepC[k]);
					e = sqrt(e);
					if (e < bestErr) {
						bestErr = e;
						slip = sgn;
					}
				}
				if (bestErr > 0.35 * stepLen) slip = 0;
			}
		}
		++t.ticks;
		if (slip) {
			++t.slips;
			for (int k = 0; k < 3; ++k) rel[k] -= slip * stepC[k];  // where it is, in step
		}
		t.quiet = (big || slip) ? 0 : std::min(t.quiet + 1, 1000);
		memcpy(t.rel[0], t.rel[1], sizeof(t.rel[0]));
		memcpy(t.rel[1], rel, sizeof(rel));
		t.have = std::min(t.have + 1, 2);
		return slip;
	}

	void CamSync(CamSample& c)
	{
		const double now = NowSeconds();
		const bool   gap = now - g_lastSyncT > 0.1;
		g_lastSyncT = now;
		if (!c.carOn) {
			g_slipFinal.have = g_slipGame.have = 0;
			g_prevCarOk = false;
			return;
		}
		const double ft = std::clamp(double(Call<float>(N::GET_FRAME_TIME)), 0.0, 0.1);
		double       stepC[3] = {}, stepLen = 0;
		stepC[0] = (g_carVelW[0] * c.R[0] + g_carVelW[1] * c.R[1] + g_carVelW[2] * c.R[2]) * ft;
		stepC[1] = (g_carVelW[0] * c.F[0] + g_carVelW[1] * c.F[1] + g_carVelW[2] * c.F[2]) * ft;
		stepC[2] = (g_carVelW[0] * c.U[0] + g_carVelW[1] * c.U[1] + g_carVelW[2] * c.U[2]) * ft;
		stepLen = sqrt(stepC[0] * stepC[0] + stepC[1] * stepC[1] + stepC[2] * stepC[2]);
		const double carYaw = atan2(-double(c.F[0]), double(c.F[1])) / kDeg;
		double       dyaw = g_prevCarOk && !gap ? carYaw - g_prevCarYaw : 0.0;
		while (dyaw > 180.0) dyaw -= 360.0;
		while (dyaw < -180.0) dyaw += 360.0;
		g_prevCarYaw = carYaw;
		g_prevCarOk = true;

		const NVec3 gp = Call<NVec3>(N::GET_GAMEPLAY_CAM_COORD);
		const NVec3 gr = Call<NVec3>(N::GET_GAMEPLAY_CAM_ROT, 2);
		const double rawF[6] = { c.px, c.py, c.pz, c.pitch, c.roll, c.yaw };
		const int   sF = SlipCheck(g_slipFinal, c.px, c.py, c.pz, c, stepC, stepLen, gap);
		const int   sG = SlipCheck(g_slipGame, gp.x, gp.y, gp.z, c, stepC, stepLen, gap);
		const double dx = gp.x - c.px, dy = gp.y - c.py, dz = gp.z - c.pz, dd = sqrt(dx * dx + dy * dy + dz * dz);
		double       dyw = double(gr.z) - double(c.yaw);
		while (dyw > 180.0) dyw -= 360.0;
		while (dyw < -180.0) dyw += 360.0;
		const bool same = dd < 0.5 + 2.5 * stepLen && fabs(dyw) < 3.0;  // the same camera (not a cinematic or scripted one)
		g_camDiffSum += dd;
		++g_camDiffN;
		g_camSameN += same ? 1 : 0;
		const int src = g_cfg.cameraSource >= 0 ? g_cfg.cameraSource : g_camSourceAuto;
		const bool useGame = src == 1 && same;
		if (useGame) {
			c.px = gp.x;
			c.py = gp.y;
			c.pz = gp.z;
			c.pitch = gr.x;
			c.roll = gr.y;
			c.yaw = gr.z;
		}
		const int slip = useGame ? sG : sF;
		const bool slipFixOn = g_drv.state == 2 ? (g_cfg.driveSlipFix && !g_drv.foot) : g_cfg.camSlipFix;  // v2.2: drive mode has its own switch (v2.61: not on foot)
		g_dset.slipApplied = (slip && slipFixOn) ? slip : 0;
		if (slip && slipFixOn) {
			c.px -= slip * g_carVelW[0] * ft;
			c.py -= slip * g_carVelW[1] * ft;
			c.pz -= slip * g_carVelW[2] * ft;
			c.yaw = float(c.yaw - slip * dyaw);
			++g_slipFixed;
		}
		if (g_trec.state.load() == 1) {
			// car_* = this tick's car as GTA reported it (before pairing); draw_kind says which car was drawn
			// (pair_prev: the car of the row before, pair_same: this row's car)
			TrecAdd("tick,%.3f,%u,,%d,%d,%.3f,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%d,%d,%.4f,%.4f,%.4f,%.3f,,%s,"
			        "%d,%.4f,%.4f,%.4f,%.3f,%.3f,%.3f,%.5f,%.3f,%d,%d",
				TrecMs(), g_camSeq + 1, Call<int>(N::GET_FRAME_COUNT), Call<int>(N::GET_GAME_TIMER), ft * 1000.0, g_rawCar.ox, g_rawCar.oy, g_rawCar.oz, g_rawCar.F[0],
				g_rawCar.F[1], g_rawCar.F[2], g_carVelW[0], g_carVelW[1], g_carVelW[2], rawF[0], rawF[1], rawF[2], rawF[3], rawF[4], rawF[5], gp.x, gp.y, gp.z, gr.x, gr.y,
				gr.z, sF, sG, c.px, c.py, c.pz, c.yaw, g_pairUsed ? "pair_prev" : "pair_same",
				g_drv.state == 2 ? 1 : 0, g_dset.O[0], g_dset.O[1], g_dset.O[2], g_dset.V[0], g_dset.V[1], g_dset.V[2], g_dset.beamSt, g_dset.drift * 100.0, g_dset.used ? 1 : 0,
				g_dset.slipApplied);
		}

		// every 10 s while it matters: what was seen; auto picks the steadier camera reading
		const ULONGLONG nowMs = GetTickCount64();
		if (nowMs - g_slipLogMs >= 10000) {
			const bool moving = g_slipFinal.ticks > 0;
			if (moving && (g_slipFinal.slips || g_slipGame.slips || stepLen > 0.03)) {
				const double sameShare = g_camDiffN ? double(g_camSameN) / g_camDiffN : 0.0;
				int          next = g_camSourceAuto;
				if (g_cfg.cameraSource < 0) {
					if (g_camSourceAuto == 0 && sameShare > 0.9 && g_slipFinal.slips >= 3 && g_slipGame.slips * 2 < g_slipFinal.slips) next = 1;
					else if (g_camSourceAuto == 1 && g_slipGame.slips >= 3 && g_slipFinal.slips * 2 < g_slipGame.slips) next = 0;
				}
				Log("camera: timing slips against the car in the last 10 s: GTA's final camera %u, gameplay camera %u (same camera %.0f%% of ticks, %.2f m apart on average); "
				    "using %s; %u put back in step%s",
					g_slipFinal.slips, g_slipGame.slips, sameShare * 100.0, g_camDiffN ? g_camDiffSum / g_camDiffN : 0.0, src == 1 ? "gameplay" : "final", g_slipFixed,
					(g_drv.state == 2 ? g_cfg.driveSlipFix : g_cfg.camSlipFix) ? "" : (g_drv.state == 2 ? " (drive mode, DriveCameraSlipFix=0: not fixed)" : " (CameraSlipFix=0: not fixed)"));
				if (next != g_camSourceAuto) {
					Log("camera: switching to GTA's %s camera reading (steadier against the car)", next == 1 ? "gameplay" : "final");
					g_camSourceAuto = next;
				}
			}
			g_slipFinal.slips = g_slipGame.slips = 0;
			g_slipFinal.ticks = g_slipGame.ticks = 0;
			g_slipFixed = g_camDiffN = g_camSameN = 0;
			g_camDiffSum = 0;
			g_slipLogMs = nowMs;
		}
	}

	// ---------------------------------------------------------------------------------------
	// v1.9.6: car/camera pairing (the stutter). The v1.9.5 timing recording (33 m/s, 8 s, 884 game frames)
	// shows GTA's camera reading at a tick moving smoothly with the car position of the tick BEFORE
	// (second difference of the camera seen from the car: 0.46 cm median, 2.5 cm p95) but jittering against
	// the same tick's car position (3.2 cm median, 11 cm p95 = one game step of the car at 8-11 ms frame
	// times). So the car was drawn one game step ahead of the camera GTA drew the world with, by a distance
	// that changes with every frame time: a jump every frame at speed. Now the car pose of the tick before
	// goes with this tick's camera. Auto keeps measuring both pairings and uses the smoother one.
	// ---------------------------------------------------------------------------------------
	CamSample g_prevCar{};
	bool      g_havePrevCar = false;
	int       g_pairAutoM[2] = { 1, 0 };  // auto's choice: [0] normal driving (GTA moves the car: the tick before),
	                                     // [1] v2.1 drive mode (the script places the car: this tick, from the user's v2.01 log)
	int       g_pairModeWas = 0;
	double    g_pairRel[2][3][3] = {};  // [same/previous][last 3 ticks][xyz]
	int       g_pairN = 0, g_pairCnt = 0;
	double    g_pairSum[2] = {};
	ULONGLONG g_pairLogMs = 0;

	void CopyCarPart(CamSample& d, const CamSample& s)
	{
		d.carOn = s.carOn;
		d.ox = s.ox;
		d.oy = s.oy;
		d.oz = s.oz;
		memcpy(d.R, s.R, sizeof(d.R));
		memcpy(d.F, s.F, sizeof(d.F));
		memcpy(d.U, s.U, sizeof(d.U));
		d.distFront = s.distFront;
		d.distRear = s.distRear;
		d.steerDeg = s.steerDeg;
		d.gtaWheels = s.gtaWheels;
		memcpy(d.gDist, s.gDist, sizeof(d.gDist));
		memcpy(d.gSteer, s.gSteer, sizeof(d.gSteer));
		memcpy(d.gTravel, s.gTravel, sizeof(d.gTravel));
		d.bngWheels = s.bngWheels;
		memcpy(d.bwOk, s.bwOk, sizeof(d.bwOk));
		memcpy(d.bwC, s.bwC, sizeof(d.bwC));
		memcpy(d.bwA, s.bwA, sizeof(d.bwA));
		memcpy(d.bwAng, s.bwAng, sizeof(d.bwAng));
	}

	void CamInCar(const CamSample& cam, const CamSample& car, double out[3])
	{
		const double v[3] = { cam.px - car.ox, cam.py - car.oy, cam.pz - car.oz };
		out[0] = v[0] * car.R[0] + v[1] * car.R[1] + v[2] * car.R[2];
		out[1] = v[0] * car.F[0] + v[1] * car.F[1] + v[2] * car.F[2];
		out[2] = v[0] * car.U[0] + v[1] * car.U[1] + v[2] * car.U[2];
	}

	double g_pairT = 0;  // when PairCar last ran (s)

	void PairCar(CamSample& c)
	{
		const CamSample cur = c;
		const double    speed = sqrt(g_carVelW[0] * g_carVelW[0] + g_carVelW[1] * g_carVelW[1] + g_carVelW[2] * g_carVelW[2]);
		const double    now = NowSeconds();
		// the car of the tick before is only used right after it: not after a pause, a loading screen or a
		// teleport (sandbox review)
		if (g_havePrevCar) {
			const double dx = cur.ox - g_prevCar.ox, dy = cur.oy - g_prevCar.oy, dz = cur.oz - g_prevCar.oz;
			if (now - g_pairT > 0.25 || sqrt(dx * dx + dy * dy + dz * dz) > 1.0 + speed * 0.1) {
				g_havePrevCar = false;
				g_pairN = 0;
			}
		}
		g_pairT = now;
		if (c.carOn && g_havePrevCar && speed > 5.0) {
			// measured on the camera reading that gets drawn (CamSync may use GTA's gameplay camera)
			CamSample   mc = c;
			const int   src = g_cfg.cameraSource >= 0 ? g_cfg.cameraSource : g_camSourceAuto;
			if (src == 1) {
				const NVec3 gp = Call<NVec3>(N::GET_GAMEPLAY_CAM_COORD);
				mc.px = gp.x;
				mc.py = gp.y;
				mc.pz = gp.z;
			}
			for (int m = 0; m < 2; ++m) {
				memmove(g_pairRel[m][0], g_pairRel[m][1], sizeof(double) * 6);
				CamInCar(mc, m == 0 ? cur : g_prevCar, g_pairRel[m][2]);
			}
			if (++g_pairN >= 3) {
				double dd[2];
				for (int m = 0; m < 2; ++m) {
					double s2 = 0;
					for (int k = 0; k < 3; ++k) {
						const double d2 = g_pairRel[m][2][k] - 2.0 * g_pairRel[m][1][k] + g_pairRel[m][0][k];
						s2 += d2 * d2;
					}
					dd[m] = sqrt(s2);
				}
				if (dd[0] < 1.0 && dd[1] < 1.0) {  // not a camera cut
					g_pairSum[0] += dd[0];
					g_pairSum[1] += dd[1];
					++g_pairCnt;
				}
			}
		} else {
			g_pairN = 0;
		}
		// v2.1: drive mode has its own choice (switching into drive mode with normal driving's choice was the stutter)
		const int pm = g_drv.state == 2 ? 1 : 0;
		if (pm != g_pairModeWas) {
			g_pairModeWas = pm;
			g_pairSum[0] = g_pairSum[1] = 0;
			g_pairCnt = 0;
			g_pairN = 0;
			g_pairLogMs = GetTickCount64();
			g_slipFinal.have = g_slipGame.have = 0;
			if (g_cfg.carCamPairing < 0) Log("car/camera pairing: %s: using %s", pm ? "drive mode" : "normal driving", g_pairAutoM[pm] ? "the tick before" : "this tick");
		}
		int&      g_pairAuto = g_pairAutoM[pm];
		const int mode = g_cfg.carCamPairing >= 0 ? g_cfg.carCamPairing : g_pairAuto;
		g_rawCar = cur;
		g_pairUsed = mode == 1 && g_havePrevCar && cur.carOn;
		if (g_pairUsed) {
			CopyCarPart(c, g_prevCar);
		}
		g_prevCar = cur;
		g_havePrevCar = cur.carOn;
		const ULONGLONG nowMs = GetTickCount64();
		if (nowMs - g_pairLogMs >= 10000) {
			g_pairLogMs = nowMs;
			if (g_pairCnt >= 200) {
				const double a = g_pairSum[0] / g_pairCnt, b = g_pairSum[1] / g_pairCnt;
				int          next = g_pairAuto;
				if (b < a * 0.6) next = 1;
				else if (a < b * 0.6) next = 0;
				Log("car/camera pairing: camera jitter seen from the car %.2f cm with this tick's car, %.2f cm with the tick before (%d ticks at speed); using %s",
					a * 100.0, b * 100.0, g_pairCnt, (g_cfg.carCamPairing >= 0 ? g_cfg.carCamPairing : next) ? "the tick before" : "this tick");
				if (g_cfg.carCamPairing < 0 && next != g_pairAuto) {
					Log("car/camera pairing: switching to the car of %s", next ? "the tick before" : "this tick");
					g_pairAuto = next;
					g_slipFinal.have = g_slipGame.have = 0;  // the switch itself is one step, not a camera slip
				}
			}
			g_pairSum[0] = g_pairSum[1] = 0;
			g_pairCnt = 0;
		}
	}

	void ScriptTick(uint32_t tick)
	{
		g_tickMs.store(GetTickCount64());  // v2.41: the receiver tells BeamNG "hold" when this stops (GTA paused)
		CamSample   c{};
		const NVec3 p = Call<NVec3>(N::GET_FINAL_RENDERED_CAM_COORD);
		const NVec3 rot = Call<NVec3>(N::GET_FINAL_RENDERED_CAM_ROT, 2);
		c.px = p.x;
		c.py = p.y;
		c.pz = p.z;
		c.pitch = rot.x;
		c.roll = rot.y;
		c.yaw = rot.z;
		c.fov = Call<float>(N::GET_FINAL_RENDERED_CAM_FOV);
		c.nearClip = Call<float>(N::GET_FINAL_RENDERED_CAM_NEAR_CLIP);
		c.farClip = Call<float>(N::GET_FINAL_RENDERED_CAM_FAR_CLIP);
		c.hour = float(Call<int>(N::GET_CLOCK_HOURS)) + float(Call<int>(N::GET_CLOCK_MINUTES)) / 60.0f;
		c.ms = GetTickCount64();
		c.sunVis = 1.0f;
		if (tick % 15 == 0) {
			SampleHud();
		}
		c.hudOn = g_hudOn;
		memcpy(c.hud, g_hud, sizeof(c.hud));
		UpdateDriver(g_tgt.mode ? g_tgt.veh : 0, g_tgt.mode == 1);
		const bool paused = Call<int>(N::IS_PAUSE_MENU_ACTIVE) != 0;
		SampleCar(c);
		DrvDrawCheck(c);       // v2.2: drift check / DriveDrawPose=set (before DriveTick moves the car again)
		DriveTick(c, paused);  // v2.0: after reading the GTA car back, before pairing (moves it for the next tick)
		g_holdOn.store(g_drv.state != 0 ? 1 : 0);
		LandTick();            // v2.41: GTA's car back on GTA's ground after F10 off (waits for GTA's collision)
		RadioAfterTick();      // v2.41: the car's radio follows what played in drive mode
		WorldTick();           // v2.4: GTA's ground for BeamNG (rays out are collected even when not driving)
		PropsTick(paused);     // v2.5: GTA's props and BeamNG's car
		WeaponsTick(paused);   // v2.6: GTA's bullets and explosions on BeamNG's car, its fire, you hurt
		TrafficTick(paused);   // v2.7: GTA's cars and people against BeamNG's car
		DentsTick();           // v2.5: BeamNG's dents on the drawn car
		DriveHud(paused);      // v2.01
		DriveGauge(paused);    // v2.1
		PairCar(c);  // v1.9.6
		CamSync(c);
		TrecFinishIfDue();

		AcquireSRWLockExclusive(&g_shared);
		c.seq = g_camSeq + 1;
		g_camRing[c.seq % kCamRing] = c;
		g_camSeq = c.seq;
		g_paused = paused;
		ReleaseSRWLockExclusive(&g_shared);
		g_ticks.fetch_add(1, std::memory_order_relaxed);

		static float loggedFov = -1, loggedNear = -1, loggedFar = -1;
		if (fabsf(c.fov - loggedFov) > 0.5f || fabsf(c.nearClip - loggedNear) > loggedNear * 0.1f || fabsf(c.farClip - loggedFar) > loggedFar * 0.1f) {
			loggedFov = c.fov;
			loggedNear = c.nearClip;
			loggedFar = c.farClip;
			// v2.2: at most once per 10 s (GTA's follow camera widens its fov with speed: many lines a second)
			static ULONGLONG fovLogMs = 0;
			static uint32_t  fovQuiet = 0;
			const ULONGLONG  fNow = GetTickCount64();
			if (fNow - fovLogMs >= 10000) {
				Log("camera: fov %.2f, near %.4f, far %.1f%s", c.fov, c.nearClip, c.farClip, fovQuiet ? " (more changes since the last line not logged)" : "");
				fovLogMs = fNow;
				fovQuiet = 0;
			} else {
				++fovQuiet;
			}
		}

		static bool keyWas = false;
		const bool  keyNow = (GetAsyncKeyState(g_cfg.key) & 0x8000) != 0;
		if (keyNow && !keyWas && GtaFocused()) {
			OnKey();
		}
		keyWas = keyNow;
		// v2.0: drive mode key (no modifiers)
		static bool driveKeyWas = false;
		const bool  driveKeyNow = (GetAsyncKeyState(g_cfg.driveKey) & 0x8000) != 0;
		if (driveKeyNow && !driveKeyWas && GtaFocused() && g_cfg.driveKey != g_cfg.key) {
			if ((GetAsyncKeyState(VK_SHIFT) & 0x8000) && (GetAsyncKeyState(VK_CONTROL) & 0x8000)) {
				SeatAdjustToggle();  // v2.72
			} else if (GetAsyncKeyState(VK_SHIFT) & 0x8000) {
				g_cfg.driveHud = !g_cfg.driveHud;  // v2.01: Shift+F10 = the drive check text
				Notify("Drive check text %s.", g_cfg.driveHud ? "on" : "off");
			} else if (GetAsyncKeyState(VK_CONTROL) & 0x8000) {
				// v2.3: Ctrl+F10 = the 8 s timing recording (Alt+F11 never reached the plugin in your v2.2 test)
				TrecStart();
				Notify("Timing: recording 8 s -> GtaBeamD1_timing.csv");
				Log("timing recording: started (Ctrl+F10)");
			} else if (g_drv.state != 0) {
				DriveStop("drive key", true);
			} else {
				DriveStart();
			}
		}
		driveKeyWas = driveKeyNow;
		SeatAdjustTick();  // v2.72
		DoorTick();
		// v2.01: GTA focused with a BeamNG car loaded but not driving: the BeamNG mod keeps BeamNG's own controls
		// off its car (a pad BeamNG reads in the background would otherwise drive it while you walk around)
		static double parkT = 0;
		if (g_cfg.parkBlock && g_drv.state == 0 && g_haveModel.load() && GtaFocused()) {
			const double t = NowSeconds();
			if (t - parkT > 0.5 && DrvOpen()) {
				parkT = t;
				DrvSend("{\"t\":\"park\",\"blk\":1}");
			}
		}

		if (tick % 10 == 0 && !paused) {
			RunRayCheck(c);
		}

		char msg[160];
		bool show = false;
		AcquireSRWLockExclusive(&g_shared);
		if (g_notifyCount > 0) {
			memcpy(msg, g_notify[0], sizeof(msg));
			memmove(g_notify[0], g_notify[1], sizeof(g_notify[0]) * 3);
			--g_notifyCount;
			show = true;
		}
		ReleaseSRWLockExclusive(&g_shared);
		if (show) {
			ShowFeed(msg);
		}
	}

	void ScriptMain()
	{
		Log("script thread running");
		uint32_t tick = 0;
		for (;;) {
			if (tick == 60) {
				Notify("GtaBeam D1 %s loaded. Shift+F11: get car from BeamNG. F11: put it on a GTA car.", D1_VERSION);
				if (g_cfg.autoLoad && FileExists(DefaultCarPath().c_str())) {
					StartWorker(Job::Reload);
				}
			}
			const double t0 = NowSeconds();
			ScriptTick(tick++);
			const uint64_t us = uint64_t((NowSeconds() - t0) * 1e6);
			g_tickUsSum.fetch_add(us, std::memory_order_relaxed);
			uint64_t prev = g_tickUsMax.load(std::memory_order_relaxed);
			while (us > prev && !g_tickUsMax.compare_exchange_weak(prev, us)) {
			}
			scriptWait(0);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Depth buffer discovery (unchanged from D0): count what GTA binds and clears each frame
	// ---------------------------------------------------------------------------------------

	struct DsvEntry
	{
		ID3D11DepthStencilView* dsv;
		ID3D11Texture2D*        tex;
		D3D11_TEXTURE2D_DESC    td;
		DXGI_FORMAT             viewFormat;
		int                     viewDim;
		bool                    tex2D;
		int                     id;
		uint32_t                bindsCur, bindsLast;
		uint32_t                defCur, defLast;
		uint32_t                clearsCur, clearsLast;
		uint32_t                sinceClear;
		float                   clearDepth;
		bool                    cleared, clearImmediate;
		uint64_t                total;
	};

	SRWLOCK  g_dsvLock = SRWLOCK_INIT;
	DsvEntry g_dsvs[kMaxDsvs];
	int      g_dsvCount = 0;

	std::atomic<DWORD>    g_ourThread{ 0 };
	ID3D11DeviceContext*  g_immediate = nullptr;
	std::atomic<uint64_t> g_frame{ 0 };

	struct ClearCopy
	{
		ID3D11DepthStencilView* dsv = nullptr;
		ID3D11Texture2D*        src = nullptr;
		ID3D11Texture2D*        dst = nullptr;
		bool                    enabled = false;
	} g_cc;
	std::atomic<uint64_t> g_ccFrame{ 0 }, g_ccCount{ 0 };

	using OMSetRT_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
	using OMSetRTUAV_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*, UINT, UINT,
		ID3D11UnorderedAccessView* const*, const UINT*);
	using ClearDSV_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11DepthStencilView*, UINT, FLOAT, UINT8);
	using ClearRTV_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11RenderTargetView*, const FLOAT[4]);
	using DrawIndexed_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
	using Draw_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
	using DrawIndexedInst_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, INT, UINT);
	using DrawInst_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT, UINT);
	using Dispatch_t = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, UINT);

	struct VtHooks
	{
		void**            vt;
		OMSetRT_t         omSetRT;
		OMSetRTUAV_t      omSetRTUAV;
		ClearDSV_t        clearDSV;
		ClearRTV_t        clearRTV;
		DrawIndexed_t     drawIndexed;
		Draw_t            draw;
		DrawIndexedInst_t drawIndexedInst;
		DrawInst_t        drawInst;
		Dispatch_t        dispatch;
	};
	VtHooks          g_vts[2] = {};
	std::atomic<int> g_vtCount{ 0 };

	inline VtHooks* FindVt(ID3D11DeviceContext* ctx)
	{
		void** vt = *reinterpret_cast<void***>(ctx);
		if (g_vtCount.load(std::memory_order_acquire) > 1 && g_vts[1].vt == vt) {
			return &g_vts[1];
		}
		return &g_vts[0];
	}

	DsvEntry* FindOrAddLocked(ID3D11DepthStencilView* dsv)
	{
		for (int i = 0; i < g_dsvCount; ++i) {
			if (g_dsvs[i].dsv == dsv) {
				return &g_dsvs[i];
			}
		}
		if (g_dsvCount >= kMaxDsvs) {
			return nullptr;
		}
		DsvEntry& e = g_dsvs[g_dsvCount];
		memset(&e, 0, sizeof(e));
		e.dsv = dsv;
		e.id = g_dsvCount + 1;
		D3D11_DEPTH_STENCIL_VIEW_DESC vd{};
		dsv->GetDesc(&vd);
		e.viewFormat = vd.Format;
		e.viewDim = int(vd.ViewDimension);
		ID3D11Resource* res = nullptr;
		dsv->GetResource(&res);
		if (res) {
			ID3D11Texture2D* t = nullptr;
			if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&t))) && t) {
				t->GetDesc(&e.td);
				e.tex = t;
				e.tex2D = true;
				t->Release();
			}
			res->Release();
		}
		++g_dsvCount;
		return &e;
	}

	// ---------------------------------------------------------------------------------------
	// Frame capture (Alt+F11): every render-target change, clear and draw of one GTA frame, written
	// to GtaBeamD1_frame.txt. It shows where GTA finishes the 3D scene, runs its post-processing and
	// draws the HUD - the place to draw the car in D3 (under the HUD, with GTA's post-processing).
	// ---------------------------------------------------------------------------------------

	struct Capture
	{
		SRWLOCK                    lock = SRWLOCK_INIT;
		std::string                text;
		int                        events = 0;
		void*                      bb = nullptr;
		std::map<void*, int>       ids;  // resource -> short id
		uint32_t                   draws = 0, dispatches = 0;
		char                       firstDraws[96] = "";
		int                        nFirst = 0;
		double                     t0 = 0;  // seconds (NowSeconds)
		ID3D11Texture2D*           held[3] = {};  // v1.9.4: pictures saved with the capture (referenced until then)
		char                       heldWhy[3][96] = {};
	} g_cap;

	const char* FmtName(DXGI_FORMAT f);

	int CapId(void* res)
	{
		auto it = g_cap.ids.find(res);
		if (it != g_cap.ids.end()) return it->second;
		const int id = int(g_cap.ids.size()) + 1;
		g_cap.ids[res] = id;
		return id;
	}

	void CapAppend(const char* fmt, ...)
	{
		if (g_cap.events > 60000) return;
		char    buf[512];
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(buf, sizeof(buf), fmt, ap);
		va_end(ap);
		g_cap.text += buf;
		g_cap.text += "\r\n";
		++g_cap.events;
	}

	void CapFlushDraws()
	{
		if (g_cap.draws || g_cap.dispatches) {
			CapAppend("      %u draw(s)%s%s, %u dispatch(es)", g_cap.draws, g_cap.nFirst ? ", first: " : "", g_cap.firstDraws, g_cap.dispatches);
		}
		g_cap.draws = g_cap.dispatches = 0;
		g_cap.nFirst = 0;
		g_cap.firstDraws[0] = 0;
	}

	void CapDescribeView(ID3D11View* v, char* out, size_t n)
	{
		if (!v) {
			snprintf(out, n, "-");
			return;
		}
		ID3D11Resource* res = nullptr;
		v->GetResource(&res);
		if (!res) {
			snprintf(out, n, "?");
			return;
		}
		ID3D11Texture2D* t = nullptr;
		D3D11_TEXTURE2D_DESC td{};
		if (SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&t))) && t) {
			t->GetDesc(&td);
			t->Release();
		}
		snprintf(out, n, "T%d %s %ux%u%s%s", CapId(res), FmtName(td.Format), td.Width, td.Height, td.SampleDesc.Count > 1 ? " msaa" : "",
			res == g_cap.bb ? " [BACKBUFFER]" : "");
		res->Release();
	}

	inline bool Capturing() { return g_capture.load(std::memory_order_relaxed) == 2 && GetCurrentThreadId() != g_ourThread.load(std::memory_order_relaxed); }

	void CapRT(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv, const char* what)
	{
		AcquireSRWLockExclusive(&g_cap.lock);
		CapFlushDraws();
		std::string s;
		char        d[128];
		for (UINT i = 0; i < n && rtvs && i < 8; ++i) {
			CapDescribeView(rtvs[i], d, sizeof(d));
			s += i ? ", " : "";
			s += d;
		}
		char ds[128];
		CapDescribeView(dsv, ds, sizeof(ds));
		CapAppend("%7.3f ms %s %s: RT [%s] depth [%s]", (NowSeconds() - g_cap.t0) * 1000.0, ctx == g_immediate ? "imm" : "def", what, s.c_str(), ds);
		// v1.9.4: keep GTA's reflection map (a float target twice as wide as tall, e.g. R11G11B10 2000x1000
		// in the user's capture) to save as a picture with the capture: the next step towards matching
		// GTA's car reflections exactly
		if (n > 0 && rtvs && rtvs[0] && g_capture.load() == 2) {
			ID3D11Resource* res = nullptr;
			rtvs[0]->GetResource(&res);
			ID3D11Texture2D* t = nullptr;
			if (res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&t))) && t) {
				D3D11_TEXTURE2D_DESC td{};
				t->GetDesc(&td);
				const bool fl = td.Format == DXGI_FORMAT_R11G11B10_FLOAT || td.Format == DXGI_FORMAT_R16G16B16A16_FLOAT || td.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS;
				if (fl && td.Width == td.Height * 2 && td.Width >= 256 && td.ArraySize == 1) {
					bool have = false;
					int  slot = -1;
					for (int k = 0; k < 3; ++k) {
						if (g_cap.held[k] == t) have = true;
						if (!g_cap.held[k] && slot < 0) slot = k;
					}
					if (!have && slot >= 0) {
						t->AddRef();
						g_cap.held[slot] = t;
						snprintf(g_cap.heldWhy[slot], sizeof(g_cap.heldWhy[slot]), "%s %ux%u%s", FmtName(td.Format), td.Width, td.Height, td.SampleDesc.Count > 1 ? " msaa" : "");
					}
				}
				t->Release();
			}
			SafeRelease(res);
		}
		ReleaseSRWLockExclusive(&g_cap.lock);
	}

	void CapDraw(const char* kind, UINT a, UINT b)
	{
		AcquireSRWLockExclusive(&g_cap.lock);
		if (kind[0] == 'X') {
			++g_cap.dispatches;
		} else {
			++g_cap.draws;
			if (g_cap.nFirst < 3) {
				char one[32];
				snprintf(one, sizeof(one), "%s%s(%u%s%u)", g_cap.nFirst ? " " : "", kind, a, b ? "x" : "", b);
				strncat(g_cap.firstDraws, one, sizeof(g_cap.firstDraws) - strlen(g_cap.firstDraws) - 1);
				++g_cap.nFirst;
			}
		}
		ReleaseSRWLockExclusive(&g_cap.lock);
	}

	inline void NoteBind(ID3D11DeviceContext* ctx, ID3D11DepthStencilView* dsv)
	{
		if (!dsv || GetCurrentThreadId() == g_ourThread.load(std::memory_order_relaxed)) {
			return;
		}
		AcquireSRWLockExclusive(&g_dsvLock);
		if (DsvEntry* e = FindOrAddLocked(dsv)) {
			if (ctx == g_immediate) {
				++e->bindsCur;
			} else {
				++e->defCur;
			}
			++e->sinceClear;
			++e->total;
		}
		ReleaseSRWLockExclusive(&g_dsvLock);
	}

	// D3 (v1.9.3): learns GTA's frame from its render-target binds, draws the car before post-processing
	void StageOnBind(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv);

	void STDMETHODCALLTYPE HookOMSetRT(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv)
	{
		NoteBind(ctx, dsv);
		StageOnBind(ctx, n, rtvs, dsv);  // may draw the car first (D3), before GTA's bind below
		if (Capturing()) CapRT(ctx, n, rtvs, dsv, "bind");
		FindVt(ctx)->omSetRT(ctx, n, rtvs, dsv);
	}

	void STDMETHODCALLTYPE HookOMSetRTUAV(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv, UINT uavStart,
		UINT uavCount, ID3D11UnorderedAccessView* const* uavs, const UINT* counts)
	{
		if (n != D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL) {
			NoteBind(ctx, dsv);
			StageOnBind(ctx, n, rtvs, dsv);
			if (Capturing()) CapRT(ctx, n, rtvs, dsv, "bind+uav");
		}
		FindVt(ctx)->omSetRTUAV(ctx, n, rtvs, dsv, uavStart, uavCount, uavs, counts);
	}

	void STDMETHODCALLTYPE HookClearRTV(ID3D11DeviceContext* ctx, ID3D11RenderTargetView* rtv, const FLOAT c[4])
	{
		if (Capturing()) {
			AcquireSRWLockExclusive(&g_cap.lock);
			CapFlushDraws();
			char d[128];
			CapDescribeView(rtv, d, sizeof(d));
			CapAppend("          clear RT %s to %.2f %.2f %.2f %.2f", d, c[0], c[1], c[2], c[3]);
			ReleaseSRWLockExclusive(&g_cap.lock);
		}
		FindVt(ctx)->clearRTV(ctx, rtv, c);
	}

	void STDMETHODCALLTYPE HookDrawIndexed(ID3D11DeviceContext* ctx, UINT a, UINT b, INT c)
	{
		if (Capturing()) CapDraw("I", a, 0);
		FindVt(ctx)->drawIndexed(ctx, a, b, c);
	}
	void STDMETHODCALLTYPE HookDraw(ID3D11DeviceContext* ctx, UINT a, UINT b)
	{
		if (Capturing()) CapDraw("D", a, 0);
		FindVt(ctx)->draw(ctx, a, b);
	}
	void STDMETHODCALLTYPE HookDrawIndexedInst(ID3D11DeviceContext* ctx, UINT a, UINT b, UINT c, INT d, UINT e)
	{
		if (Capturing()) CapDraw("II", a, b);
		FindVt(ctx)->drawIndexedInst(ctx, a, b, c, d, e);
	}
	void STDMETHODCALLTYPE HookDrawInst(ID3D11DeviceContext* ctx, UINT a, UINT b, UINT c, UINT d)
	{
		if (Capturing()) CapDraw("DI", a, b);
		FindVt(ctx)->drawInst(ctx, a, b, c, d);
	}
	void STDMETHODCALLTYPE HookDispatch(ID3D11DeviceContext* ctx, UINT x, UINT y, UINT z)
	{
		if (Capturing()) CapDraw("X", x, y);
		FindVt(ctx)->dispatch(ctx, x, y, z);
	}

	void STDMETHODCALLTYPE HookClearDSV(ID3D11DeviceContext* ctx, ID3D11DepthStencilView* dsv, UINT flags, FLOAT depth, UINT8 stencil)
	{
		if (dsv && GetCurrentThreadId() != g_ourThread.load(std::memory_order_relaxed)) {
			if (Capturing()) {
				AcquireSRWLockExclusive(&g_cap.lock);
				CapFlushDraws();
				char d[128];
				CapDescribeView(dsv, d, sizeof(d));
				CapAppend("          clear depth %s to %.3f (flags %u)", d, depth, flags);
				ReleaseSRWLockExclusive(&g_cap.lock);
			}
			ID3D11Texture2D *src = nullptr, *dst = nullptr;
			AcquireSRWLockExclusive(&g_dsvLock);
			if (DsvEntry* e = FindOrAddLocked(dsv)) {
				++e->clearsCur;
				if (flags & D3D11_CLEAR_DEPTH) {
					e->clearDepth = depth;
					e->cleared = true;
					e->clearImmediate = ctx == g_immediate;
					if (g_cc.enabled && dsv == g_cc.dsv && ctx == g_immediate && e->sinceClear >= 4 && g_cc.src && g_cc.dst) {
						src = g_cc.src;
						dst = g_cc.dst;
						src->AddRef();
						dst->AddRef();
					}
					e->sinceClear = 0;
				}
			}
			ReleaseSRWLockExclusive(&g_dsvLock);
			if (src && dst) {
				ctx->CopyResource(dst, src);
				g_ccFrame.store(g_frame.load());
				g_ccCount.fetch_add(1);
				src->Release();
				dst->Release();
			}
		}
		FindVt(ctx)->clearDSV(ctx, dsv, flags, depth, stencil);
	}

	bool PatchSlot(void** vt, int slot, void* hook)
	{
		DWORD old = 0;
		if (!VirtualProtect(&vt[slot], sizeof(void*), PAGE_EXECUTE_READWRITE, &old)) {
			return false;
		}
		vt[slot] = hook;
		VirtualProtect(&vt[slot], sizeof(void*), old, &old);
		return true;
	}

	void HookContext(ID3D11DeviceContext* ctx, const char* what)
	{
		void**    vt = *reinterpret_cast<void***>(ctx);
		const int count = g_vtCount.load();
		for (int i = 0; i < count; ++i) {
			if (g_vts[i].vt == vt) {
				Log("hooks: %s context uses the same vtable as an already hooked one", what);
				return;
			}
		}
		if (count >= 2) {
			return;
		}
		VtHooks& h = g_vts[count];
		h.vt = vt;
		h.omSetRT = reinterpret_cast<OMSetRT_t>(vt[kSlotOMSetRenderTargets]);
		h.omSetRTUAV = reinterpret_cast<OMSetRTUAV_t>(vt[kSlotOMSetRenderTargetsAndUAVs]);
		h.clearDSV = reinterpret_cast<ClearDSV_t>(vt[kSlotClearDepthStencilView]);
		h.clearRTV = reinterpret_cast<ClearRTV_t>(vt[kSlotClearRenderTargetView]);
		h.drawIndexed = reinterpret_cast<DrawIndexed_t>(vt[kSlotDrawIndexed]);
		h.draw = reinterpret_cast<Draw_t>(vt[kSlotDraw]);
		h.drawIndexedInst = reinterpret_cast<DrawIndexedInst_t>(vt[kSlotDrawIndexedInstanced]);
		h.drawInst = reinterpret_cast<DrawInst_t>(vt[kSlotDrawInstanced]);
		h.dispatch = reinterpret_cast<Dispatch_t>(vt[kSlotDispatch]);
		g_vtCount.store(count + 1, std::memory_order_release);
		const bool ok = PatchSlot(vt, kSlotOMSetRenderTargets, reinterpret_cast<void*>(&HookOMSetRT)) &&
		                PatchSlot(vt, kSlotOMSetRenderTargetsAndUAVs, reinterpret_cast<void*>(&HookOMSetRTUAV)) &&
		                PatchSlot(vt, kSlotClearDepthStencilView, reinterpret_cast<void*>(&HookClearDSV)) &&
		                PatchSlot(vt, kSlotClearRenderTargetView, reinterpret_cast<void*>(&HookClearRTV)) &&
		                PatchSlot(vt, kSlotDrawIndexed, reinterpret_cast<void*>(&HookDrawIndexed)) &&
		                PatchSlot(vt, kSlotDraw, reinterpret_cast<void*>(&HookDraw)) &&
		                PatchSlot(vt, kSlotDrawIndexedInstanced, reinterpret_cast<void*>(&HookDrawIndexedInst)) &&
		                PatchSlot(vt, kSlotDrawInstanced, reinterpret_cast<void*>(&HookDrawInst)) &&
		                PatchSlot(vt, kSlotDispatch, reinterpret_cast<void*>(&HookDispatch));
		Log("hooks: %s context vtable %p %s", what, static_cast<void*>(vt), ok ? "hooked (depth binds/clears counted; draws only counted during Alt+F11 capture)" : "FAILED to hook");
	}

	void UnhookAll()
	{
		const int count = g_vtCount.load();
		for (int i = 0; i < count; ++i) {
			VtHooks& h = g_vts[i];
			PatchSlot(h.vt, kSlotOMSetRenderTargets, reinterpret_cast<void*>(h.omSetRT));
			PatchSlot(h.vt, kSlotOMSetRenderTargetsAndUAVs, reinterpret_cast<void*>(h.omSetRTUAV));
			PatchSlot(h.vt, kSlotClearDepthStencilView, reinterpret_cast<void*>(h.clearDSV));
			PatchSlot(h.vt, kSlotClearRenderTargetView, reinterpret_cast<void*>(h.clearRTV));
			PatchSlot(h.vt, kSlotDrawIndexed, reinterpret_cast<void*>(h.drawIndexed));
			PatchSlot(h.vt, kSlotDraw, reinterpret_cast<void*>(h.draw));
			PatchSlot(h.vt, kSlotDrawIndexedInstanced, reinterpret_cast<void*>(h.drawIndexedInst));
			PatchSlot(h.vt, kSlotDrawInstanced, reinterpret_cast<void*>(h.drawInst));
			PatchSlot(h.vt, kSlotDispatch, reinterpret_cast<void*>(h.dispatch));
		}
	}

	// ---------------------------------------------------------------------------------------
	// Formats
	// ---------------------------------------------------------------------------------------

	DXGI_FORMAT TypelessOf(DXGI_FORMAT f)
	{
		switch (f) {
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return DXGI_FORMAT_R32G8X24_TYPELESS;
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return DXGI_FORMAT_R24G8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT: return DXGI_FORMAT_R32_TYPELESS;
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM: return DXGI_FORMAT_R16_TYPELESS;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	DXGI_FORMAT SrvFormatOf(DXGI_FORMAT typeless)
	{
		switch (typeless) {
		case DXGI_FORMAT_R32G8X24_TYPELESS: return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
		case DXGI_FORMAT_R24G8_TYPELESS: return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		case DXGI_FORMAT_R32_TYPELESS: return DXGI_FORMAT_R32_FLOAT;
		case DXGI_FORMAT_R16_TYPELESS: return DXGI_FORMAT_R16_UNORM;
		default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	const char* FmtName(DXGI_FORMAT f)
	{
		switch (f) {
		case DXGI_FORMAT_R32G8X24_TYPELESS: return "R32G8X24_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT: return "D32_FLOAT_S8X24_UINT";
		case DXGI_FORMAT_R24G8_TYPELESS: return "R24G8_TYPELESS";
		case DXGI_FORMAT_D24_UNORM_S8_UINT: return "D24_UNORM_S8_UINT";
		case DXGI_FORMAT_R32_TYPELESS: return "R32_TYPELESS";
		case DXGI_FORMAT_D32_FLOAT: return "D32_FLOAT";
		case DXGI_FORMAT_R16_TYPELESS: return "R16_TYPELESS";
		case DXGI_FORMAT_D16_UNORM: return "D16_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM: return "R8G8B8A8_UNORM";
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "R8G8B8A8_UNORM_SRGB";
		case DXGI_FORMAT_R8G8B8A8_TYPELESS: return "R8G8B8A8_TYPELESS";
		case DXGI_FORMAT_B8G8R8A8_UNORM: return "B8G8R8A8_UNORM";
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return "B8G8R8A8_UNORM_SRGB";
		case DXGI_FORMAT_B8G8R8A8_TYPELESS: return "B8G8R8A8_TYPELESS";
		case DXGI_FORMAT_R10G10B10A2_UNORM: return "R10G10B10A2_UNORM";
		case DXGI_FORMAT_R11G11B10_FLOAT: return "R11G11B10_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_FLOAT: return "R16G16B16A16_FLOAT";
		case DXGI_FORMAT_R16G16B16A16_TYPELESS: return "R16G16B16A16_TYPELESS";
		case DXGI_FORMAT_R32G32B32A32_FLOAT: return "R32G32B32A32_FLOAT";
		case DXGI_FORMAT_R16G16_FLOAT: return "R16G16_FLOAT";
		case DXGI_FORMAT_R32_FLOAT: return "R32_FLOAT";
		case DXGI_FORMAT_R16_FLOAT: return "R16_FLOAT";
		case DXGI_FORMAT_R8_UNORM: return "R8_UNORM";
		case DXGI_FORMAT_R8G8_UNORM: return "R8G8_UNORM";
		case DXGI_FORMAT_R32G32_FLOAT: return "R32G32_FLOAT";
		default: return "other";
		}
	}

	bool IsSrgb(DXGI_FORMAT f) { return f == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || f == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB; }

	const char* BlendName(int b)
	{
		return b == car::kBlendPremul ? "premul" : b == car::kBlendAddAlpha ? "add-alpha" : b == car::kBlendAdd ? "add" : b == car::kBlendMul ? "mul" : "lerp";
	}

	// ---------------------------------------------------------------------------------------
	// Shaders
	// ---------------------------------------------------------------------------------------

	constexpr char kShader[] = R"(
#define PI 3.14159265
cbuffer Frame : register(b0)
{
	float4 camRight;   // world axes of the camera
	float4 camUp;
	float4 camFwd;
	float4 proj;       // x scale, y scale, our near, our far
	float4 depthP;     // GTA near, GTA far, reversed Z (1/0), occlusion on (1/0)
	float4 screen;     // 1/width, 1/height, relative tolerance, tolerance in metres
	float4 sunDir;     // xyz: world direction to the sun, w: day factor
	float4 carPos;     // xyz: car origin relative to the camera
	float4 carRight;   // world axes of the drawn car
	float4 carFwd;
	float4 carUp;
	float4 look;       // x ambient, y unused, z brightness, w: 1 = encode to sRGB ourselves
	float4 paint0;     // linear paint colours, BeamNG paint slots 1/2/3
	float4 paint1;
	float4 paint2;
	float4 skyTop;     // linear sky colours (made-up sky)
	float4 skyHorizon;
	float4 skyGround;  // w: reflection strength
	float4 sunCol;     // linear sun colour x strength x world visibility
	float4 shadowA;    // x contact map half size (m), y sun map half size (m), z contact strength, w sun strength
	float4 shadowB;    // xyz: shadow direction in car space (from the sun downwards), w softness
	float4 shadowC;    // x shadow map projection: 0 contact (straight down), 1 sun; y half size of the map being drawn
	float4 envP;       // x 0 sky / 1 world environment, y brightness, z screen mip for learning, w screen copy is linear (sRGB format or HDR float) (1/0)
	float4 feat;       // x normal maps on, y normal strength, z normal map green sign, w metallic/roughness maps on
	float4 dbg;        // x debug view, y picking (1/0), zw screen position of render target pixel 0,0
	float4 misc;       // x reflections on see-through parts
	float4 sl0;        // depth maps: xyz light direction (car space), w: sun self-shadow on
	float4 sl1;        // xyz e1, w: 1 / sun map size
	float4 sl2;        // xyz e2, w: radius of the car's bounding sphere
	float4 sl3;        // xyz centre of the bounding sphere (car space), w: ambient occlusion on
	float4 hud;        // minimap rectangle (screen 0..1): x0 y0 x1 y1
	float4 hud2;       // x keep minimap on top, y edge smoothing, z back buffer is sRGB, w margin (pixels)
	float4 lit;        // x specular strength, y camera inside the car, z (re)start learning the environment, w HDR sun/sky ratio (0 = drawing at Present, LDR)
	float4 paintS0;    // v1.9.6: paint slots 1/2/3: metallic, roughness, clear coat, clear coat roughness (BeamNG multiplies
	float4 paintS1;    //         them into the paint layer's factors)
	float4 paintS2;
	float4 dpU;        // v1.9.7: GTA reflection map layout of the upper half of all directions: x half (0 left, 1 right), y swap, z/w signs
	float4 dpL;        //         the same for the lower half
	float4 dpP;        //         x upper usable, y highest mip, z brightness, w lower usable
};
struct LayerP
{
	float4 factor;     // baseColorFactor
	float4 a;          // x has base map, y base UV set, z palette (0 none, 1 map, 2 solid), w palette UV set
	float4 b;          // x solid palette channel (0/1/2), y has opacity map, z opacity UV set, w opacityFactor
	float4 c;          // x paint (instanceDiffuse), y metallic, z roughness, w clear coat
	float4 d;          // x normal map, y metallic map, z roughness map: 0 none, 1 on UV set 1, 2 on UV set 2; w live screen
	float4 e;          // channel read from x opacity, y metallic, z roughness map (0-3); w normal map: 0 X/Y in red/green, 1 in alpha/green
};
cbuffer Mat : register(b1)
{
	LayerP lay[4];
	float4 matInfo;    // x layers, y alpha cutout (0 off), z see-through (1/0), w lowest alpha for see-through
	float4 matKind;    // x mirror, y blend op (0 lerp, 1 premul, 2 add-alpha, 3 add, 4 mul), z no reflections, w material number
	float4 matFx;      // v2.61: x broken (BeamNG broke this glass / lamp: cracked); v2.63: y a wheel (no bullet holes)
};
cbuffer Part : register(b2)
{
	float4 pm0;        // car-space transform of this part (wheels spin/steer/travel), rows
	float4 pm1;
	float4 pm2;
};
cbuffer Holes : register(b4)
{
	float4 holes[256];  // v2.62: bullet holes: car-space centre, radius; v2.7: then the bullet's direction (2 per hole)
	float4 holeInfo;   // x how many, y (v2.7) charred 0..1
};
cbuffer Occl : register(b3)
{
	float4 aoDir[18];  // ambient occlusion map directions (car space); v1.9.5: 12 above the horizon, 6 below
	float4 aoE1[18];
	float4 aoE2[18];
	float4 aoInfo;     // x depth bias, y normal offset (m), z strength, w one texel of the AO maps (uv)
};
Texture2D<float> sceneDepth : register(t0);
Texture2D tBase0 : register(t1);
Texture2D tBase1 : register(t2);
Texture2D tBase2 : register(t3);
Texture2D tBase3 : register(t4);
Texture2D tPal0 : register(t5);
Texture2D tPal1 : register(t6);
Texture2D tPal2 : register(t7);
Texture2D tPal3 : register(t8);
Texture2D tOp0 : register(t9);
Texture2D tOp1 : register(t10);
Texture2D tOp2 : register(t11);
Texture2D tOp3 : register(t12);
Texture2D<float> contactMap : register(t13);
Texture2D<float> sunMap : register(t14);
Texture2D tNrm0 : register(t15);
Texture2D tNrm1 : register(t16);
Texture2D tNrm2 : register(t17);
Texture2D tNrm3 : register(t18);
Texture2D tMet0 : register(t19);
Texture2D tMet1 : register(t20);
Texture2D tMet2 : register(t21);
Texture2D tMet3 : register(t22);
Texture2D tRgh0 : register(t23);
Texture2D tRgh1 : register(t24);
Texture2D tRgh2 : register(t25);
Texture2D tRgh3 : register(t26);
Texture2D envLL : register(t27);              // environment learned from GTA's frames (lat/long)
Texture2D<float> sunDepth : register(t28);    // the car seen from the sun (self-shadows)
Texture2DArray<float> aoMaps : register(t29); // the car seen from 12 directions (ambient occlusion)
Texture2D layerTex : register(t30);           // composite: the car layer
Texture2D screenCopy : register(t31);         // environment learning: GTA's frame
Texture2D llPrev : register(t32);             // environment learning: last environment
Texture2D gAlbedo : register(t33);            // v1.9.5: GTA's G-buffer colour (light measurement)
Texture2D gtaRefl : register(t34);            // v1.9.7: GTA's own reflection map (dual paraboloid)
SamplerState pointSamp : register(s0);
SamplerState linSamp : register(s1);
SamplerState clampSamp : register(s2);
SamplerComparisonState cmpSamp : register(s3);
SamplerState llSamp : register(s4);

// v2.5 (dents), v2.53: each vertex sits in a triangle of 3 of BeamNG's nodes, like BeamNG's flexbodies (si.xyz: the
// nodes; sc: where along the 1st->2nd edge, the 1st->3rd edge and the triangle's normal; si.w: 1 = in it, 2 = it only moves
// with the 1st node, 3 = carried rigidly by the 3 nodes (v2.55: props, as BeamNG does), 0 = it stays as drawn)
struct VSIn { float3 p : POSITION; float3 n : NORMAL; float2 uv0 : TEXCOORD0; float2 uv1 : TEXCOORD1; uint4 si : BLENDINDICES; float3 sc : BLENDWEIGHT; };
Texture2D<float4> nodeTex : register(t35);  // per node 2 texels (v2.53): xyz how far it moved, xyz where it was (car space)
float4 NodeD(uint k) { return nodeTex.Load(int3(k % 1024, k / 1024, 0)); }  // (a texture, 1024 wide: every shader compiler reads it)
float3 NodeMv(uint k) { return NodeD(k * 2).xyz; }
float3 NodeAt(uint k) { return NodeD(k * 2 + 1).xyz; }
float3 SafeN(float3 v) { return v * rsqrt(max(dot(v, v), 1e-12)); }  // (a triangle crushed flat: no normal, no NaN)
// the vertex's triangle of nodes as drawn (r0; edges x0, y0) and now (moved by m0; edges x1, y1)
void NodeTri(uint4 si, out float3 r0, out float3 x0, out float3 y0, out float3 m0, out float3 x1, out float3 y1)
{
	r0 = NodeAt(si.x);
	x0 = NodeAt(si.y) - r0;
	y0 = NodeAt(si.z) - r0;
	m0 = NodeMv(si.x);
	x1 = x0 + NodeMv(si.y) - m0;
	y1 = y0 + NodeMv(si.z) - m0;
}
// how that triangle turned, applied to v
float3 TriTurn(float3 v, float3 x0, float3 y0, float3 x1, float3 y1)
{
	float3 e1 = SafeN(x0), e3 = SafeN(cross(x0, y0)), e2 = cross(e3, e1);
	float3 f1 = SafeN(x1), f3 = SafeN(cross(x1, y1)), f2 = cross(f3, f1);
	return f1 * dot(v, e1) + f2 * dot(v, e2) + f3 * dot(v, e3);
}
// the vertex as drawn + how far its triangle moved it: at rest p = r + a*x + b*y + c*z, now r' + a*x' + b*y' + c*z'
float3 SkinPos(float3 p, uint4 si, float3 sc)
{
	[branch] if (si.w == 0)
		return p;
	[branch] if (si.w == 2)
		return p + NodeMv(si.x);
	float3 r0, x0, y0, m0, x1, y1;
	NodeTri(si, r0, x0, y0, m0, x1, y1);
	[branch] if (si.w == 3)
		return r0 + m0 + TriTurn(p - r0, x0, y0, x1, y1);
	return p + m0 + sc.x * (x1 - x0) + sc.y * (y1 - y0) + sc.z * (SafeN(cross(x1, y1)) - SafeN(cross(x0, y0)));
}
// the normal turns as its triangle turned
float3 SkinNrm(float3 n, uint4 si)
{
	[branch] if (si.w != 1 && si.w != 3)
		return n;
	float3 r0, x0, y0, m0, x1, y1;
	NodeTri(si, r0, x0, y0, m0, x1, y1);
	return TriTurn(n, x0, y0, x1, y1);
}
// v2.55: 1 when an edge of the vertex's triangle of nodes got over 2.5 times as long (BeamNG's part tore there): triangles
// with such a vertex are not drawn (the stretch test below can't measure ones that are thin on screen: the long lines in
// your 2.54 screenshot 1)
float TornV(uint4 si)
{
	[branch] if (si.w != 1)
		return 0.0;
	float3 r0, x0, y0, m0, x1, y1;
	NodeTri(si, r0, x0, y0, m0, x1, y1);
	float3 q0 = x0 - y0, q1 = x1 - y1;
	float  s = max(max(dot(x1, x1) / max(dot(x0, x0), 1e-8), dot(y1, y1) / max(dot(y0, y0), 1e-8)), dot(q1, q1) / max(dot(q0, q0), 1e-8));
	return s > 6.25 ? 1.0 : 0.0;
}
struct VSOut { float4 pos : SV_Position; float viewZ : TEXCOORD0; float3 n : TEXCOORD1; float4 uv : TEXCOORD2; float3 rel : TEXCOORD3; float3 sk : TEXCOORD4; float3 rp : TEXCOORD5; float tv : TEXCOORD6; };
// v2.54: a triangle stretched to over 3 times its drawn size in any direction is torn (its nodes went with two pieces: a
// bumper on the road, the car driving on, your 2.53 screenshots 4 and 6): not drawn. sk = where it is now, rp = where it was
// drawn (car space). The stretch: from how both change across the screen (the screen's own scale cancels out), the
// larger root of a x^2 - b x + c = 0 is the most it was stretched, squared. (Derivatives first, before any branch.)
bool Torn(float3 sk, float3 rp)
{
	float3 rx = ddx(rp), ry = ddy(rp), sx = ddx(sk), sy = ddy(sk);
	float  k = 1.0 / max(length(rx) + length(ry), 1e-20);
	rx *= k;
	ry *= k;
	sx *= k;
	sy *= k;
	float r00 = dot(rx, rx), r01 = dot(rx, ry), r11 = dot(ry, ry);
	float s00 = dot(sx, sx), s01 = dot(sx, sy), s11 = dot(sy, sy);
	float a = r00 * r11 - r01 * r01, b = r11 * s00 - 2.0 * r01 * s01 + r00 * s11, c = s00 * s11 - s01 * s01;
	if (a < 1e-4 * r00 * r11)
		return false;  // (seen nearly edge-on: not measurable here)
	return b + sqrt(max(b * b - 4.0 * a * c, 0.0)) > 18.0 * a;
}

float4 Project(float3 rel, out float viewZ)
{
	float3 v = float3(dot(rel, camRight.xyz), dot(rel, camUp.xyz), dot(rel, camFwd.xyz));
	float a = proj.w / (proj.w - proj.z);
	viewZ = v.z;
	return float4(v.x * proj.x, v.y * proj.y, v.z * a - proj.z * a, v.z);
}

float3 PartPos(float3 p) { return float3(dot(pm0.xyz, p) + pm0.w, dot(pm1.xyz, p) + pm1.w, dot(pm2.xyz, p) + pm2.w); }

VSOut VSCar(VSIn i)
{
	VSOut o;
	float3 p = PartPos(SkinPos(i.p, i.si, i.sc));
	float3 n0 = SkinNrm(i.n, i.si);
	float3 n = float3(dot(pm0.xyz, n0), dot(pm1.xyz, n0), dot(pm2.xyz, n0));
	float3 rel = carPos.xyz + carRight.xyz * p.x + carFwd.xyz * p.y + carUp.xyz * p.z;
	o.pos = Project(rel, o.viewZ);
	o.n = carRight.xyz * n.x + carFwd.xyz * n.y + carUp.xyz * n.z;
	o.uv = float4(i.uv0, i.uv1);
	o.rel = rel;
	o.sk = p;
	o.rp = PartPos(i.p);
	o.tv = TornV(i.si);
	return o;
}

// GTA's stored depth -> metres in front of the camera.
float SceneViewZ(float d)
{
	float n = depthP.x, f = depthP.y;
	return depthP.z > 0.5 ? n * f / (n + d * (f - n)) : n * f / (f - d * (f - n));
}

bool HiddenByGta(float2 sp, float viewZ)
{
	if (depthP.w < 0.5)
		return false;
	float sz = SceneViewZ(sceneDepth.SampleLevel(pointSamp, sp * screen.xy, 0));
	return viewZ > sz * (1.0 + screen.z) + screen.w;
}

// Texture coordinates and their screen derivatives, taken before anything can discard the pixel,
// so every sample below uses explicit gradients.
struct Grads { float2 uv0; float2 uv1; float2 dx0; float2 dy0; float2 dx1; float2 dy1; };
#define SG(t, g, set) t.SampleGrad(linSamp, (set) > 0.5 ? g.uv1 : g.uv0, (set) > 0.5 ? g.dx1 : g.dx0, (set) > 0.5 ? g.dy1 : g.dy0)
float4 SampleBase(int i, Grads g, float set)
{
	if (i == 0) return SG(tBase0, g, set);
	if (i == 1) return SG(tBase1, g, set);
	if (i == 2) return SG(tBase2, g, set);
	return SG(tBase3, g, set);
}
float4 SamplePal(int i, Grads g, float set)
{
	if (i == 0) return SG(tPal0, g, set);
	if (i == 1) return SG(tPal1, g, set);
	if (i == 2) return SG(tPal2, g, set);
	return SG(tPal3, g, set);
}
// v1.9.3: compressed .dds maps keep all their channels, so the material says which one holds the data
float4 Chan(float c) { return float4(c < 0.5 ? 1.0 : 0.0, (c > 0.5 && c < 1.5) ? 1.0 : 0.0, (c > 1.5 && c < 2.5) ? 1.0 : 0.0, c > 2.5 ? 1.0 : 0.0); }
float SampleOp(int i, Grads g, float set, float ch)
{
	float4 m = Chan(ch);
	if (i == 0) return dot(SG(tOp0, g, set), m);
	if (i == 1) return dot(SG(tOp1, g, set), m);
	if (i == 2) return dot(SG(tOp2, g, set), m);
	return dot(SG(tOp3, g, set), m);
}
float2 SampleNrm(int i, Grads g, float set, float sw)
{
	float4 t;
	if (i == 0) t = SG(tNrm0, g, set);
	else if (i == 1) t = SG(tNrm1, g, set);
	else if (i == 2) t = SG(tNrm2, g, set);
	else t = SG(tNrm3, g, set);
	return sw > 0.5 ? t.ag : t.rg;
}
float SampleMet(int i, Grads g, float set, float ch)
{
	float4 m = Chan(ch);
	if (i == 0) return dot(SG(tMet0, g, set), m);
	if (i == 1) return dot(SG(tMet1, g, set), m);
	if (i == 2) return dot(SG(tMet2, g, set), m);
	return dot(SG(tMet3, g, set), m);
}
float SampleRgh(int i, Grads g, float set, float ch)
{
	float4 m = Chan(ch);
	if (i == 0) return dot(SG(tRgh0, g, set), m);
	if (i == 1) return dot(SG(tRgh1, g, set), m);
	if (i == 2) return dot(SG(tRgh2, g, set), m);
	return dot(SG(tRgh3, g, set), m);
}

// Surface frames for normal maps, built from screen derivatives (no tangents needed in the file),
// one per UV set. T follows u, B follows v (downwards in the image).
struct Frames { float3 N; float3 T0; float3 B0; float3 T1; float3 B1; };
void Cotangent(float3 N, float3 dp1, float3 dp2, float2 duv1, float2 duv2, out float3 T, out float3 B)
{
	float3 dp2perp = cross(dp2, N);
	float3 dp1perp = cross(N, dp1);
	T = dp2perp * duv1.x + dp1perp * duv2.x;
	B = dp2perp * duv1.y + dp1perp * duv2.y;
	float invmax = rsqrt(max(max(dot(T, T), dot(B, B)), 1e-24));
	T *= invmax;
	B *= invmax;
}

// One BeamNG layer: colour x map, then the paint palette (R/G/B mask -> paint 1/2/3) multiplied in.
// w = how much of this layer shows (opacityFactor x opacityMap); srf = metallic, roughness, clear coat;
// nW = this layer's normal (world); ccr = clear coat roughness (v1.9.6).
float4 EvalLayer(int i, Grads g, Frames F, out float w, out float3 srf, out float3 nW, out float ccr)
{
	LayerP L = lay[i];
	float4 col = L.factor;
	srf = L.c.yzw;
	nW = F.N;
	w = L.b.w;
	ccr = 0.04;
	if (L.d.w > 0.5) {
		// a live UI texture in BeamNG (dashboard screen, gauges): drawn as a dark, glossy screen
		col = float4(0.010, 0.011, 0.014, 1.0);
		srf = float3(0.0, 0.12, 0.6);
		return col;
	}
	if (L.a.x > 0.5)
		col *= SampleBase(i, g, L.a.y);
	if (L.c.x > 0.5) {
		float3 m = float3(1, 0, 0);
		if (L.a.z > 1.5)
			m = L.b.x < 0.5 ? float3(1, 0, 0) : (L.b.x < 1.5 ? float3(0, 1, 0) : float3(0, 0, 1));
		else if (L.a.z > 0.5) {
			float4 t = SamplePal(i, g, L.a.w);
			m = t.rgb * t.a;
		}
		float s = m.r + m.g + m.b;
		float3 pc = (m.r * paint0.rgb + m.g * paint1.rgb + m.b * paint2.rgb) / max(s, 1e-4);
		col.rgb = lerp(col.rgb, col.rgb * pc, saturate(s));
		// v1.9.6: the paint's own metallic / roughness / clear coat are multiplied into the layer's factors
		// (BeamNG keeps those at 1 on paint layers), and its clear coat roughness is used
		if (L.c.x > 1.5) {
			float4 ps = (m.r * paintS0 + m.g * paintS1 + m.b * paintS2) / max(s, 1e-4);
			srf *= lerp(float3(1, 1, 1), ps.xyz, saturate(s));
			ccr = lerp(ccr, ps.w, saturate(s));
		}
	}
	if (L.b.y > 0.5)
		w *= SampleOp(i, g, L.b.z, L.e.x);
	if (feat.w > 0.5) {
		if (L.d.y > 0.5)
			srf.x *= SampleMet(i, g, L.d.y - 1.0, L.e.y);
		if (L.d.z > 0.5)
			srf.y *= SampleRgh(i, g, L.d.z - 1.0, L.e.z);
	}
	if (feat.x > 0.5 && L.d.x > 0.5) {
		float2 xy = SampleNrm(i, g, L.d.x - 1.0, L.e.w) * 2.0 - 1.0;
		float z = sqrt(saturate(1.0 - dot(xy, xy)));
		xy.y *= feat.z;
		xy *= feat.y;
		float3 T = L.d.x > 1.5 ? F.T1 : F.T0;
		float3 B = L.d.x > 1.5 ? F.B1 : F.B0;
		nW = normalize(T * xy.x + B * xy.y + F.N * max(z, 0.05));
	}
	return col;
}

// Made-up sky seen in direction r (world, z up); blurred towards the average for rough surfaces.
float3 Sky(float3 r, float rough)
{
	float t = r.z;
	float3 sharp = t >= 0.0 ? lerp(skyHorizon.rgb, skyTop.rgb, pow(saturate(t), 0.45))
	                        : lerp(skyHorizon.rgb * 0.55, skyGround.rgb, pow(saturate(-t * 4.0), 0.5));
	float3 avg = lerp(skyGround.rgb, (skyTop.rgb + skyHorizon.rgb) * 0.5, saturate(t * 0.5 + 0.5));
	return lerp(sharp, avg, saturate(rough * 1.3));
}

// The learned environment: a latitude/longitude map around the car (world directions).
// atan2 written out (polynomial, error < 0.0001 rad): some shader compilers (Wine's) lack it
float Atan2(float y, float x)
{
	float ax = abs(x), ay = abs(y);
	float a = min(ax, ay) / max(max(ax, ay), 1e-8);
	float s = a * a;
	float r = ((-0.0464964749 * s + 0.15931422) * s - 0.327622764) * s * a + a;
	if (ay > ax) r = 1.57079637 - r;
	if (x < 0.0) r = 3.14159274 - r;
	if (y < 0.0) r = -r;
	return r;
}
float2 LLuv(float3 d)
{
	float z = clamp(d.z, -1.0, 1.0);
	return float2(Atan2(d.y, d.x) * (0.5 / PI) + 0.5, 0.5 - Atan2(z, sqrt(max(1.0 - z * z, 0.0))) / PI);
}
float3 LLdir(float2 uv)
{
	float phi = (uv.x - 0.5) * 2.0 * PI;
	float th = (0.5 - uv.y) * PI;
	return float3(cos(th) * cos(phi), cos(th) * sin(phi), sin(th));
}
float3 EnvReflOurs(float3 r, float rough)
{
	if (envP.x < 0.5)
		return Sky(r, rough) * envP.y;
	return envLL.SampleLevel(llSamp, LLuv(r), rough * 5.0).rgb * envP.y;
}
// v1.9.7: GTA's own reflection map: two paraboloids side by side, one for the directions above the
// horizon and one for those below. Which half is which and how each is turned/mirrored (L: x half
// + 2 if it turns with the camera, y swap, z/w signs) is found by comparing it with GTA's picture
// (PSDpCalib below).
float2 DpUv(float3 d, float4 L)
{
	float hf = L.x;
	if (hf > 1.5) {
		// turning with the camera: directions relative to where the camera looks (right, forward)
		float2 fw = normalize(camFwd.xy + float2(1e-5, 0.0));
		d.xy = float2(dot(d.xy, float2(fw.y, -fw.x)), dot(d.xy, fw));
		hf -= 2.0;
	}
	float2 p = d.xy / (1.0 + abs(d.z));
	if (L.y > 0.5)
		p = p.yx;
	p *= L.zw * 0.995;  // inside the circle
	return float2((hf + p.x * 0.5 + 0.5) * 0.5, 0.5 - p.y * 0.5);
}
float3 DpLookup(float3 d, float lod)
{
	float3 g = gtaRefl.SampleLevel(clampSamp, DpUv(d, d.z >= 0.0 ? dpU : dpL), lod).rgb;
	return min(max(g, 0.0), 60000.0) * dpP.z;  // no Inf/NaN into GTA's picture (sandbox review)
}
bool DpHave(float3 d) { return (d.z >= 0.0 ? dpP.x : dpP.w) > 0.5; }
float3 EnvRefl(float3 r, float rough)
{
	float3 o = EnvReflOurs(r, rough);
	if (!DpHave(r))
		return o;
	// rough surfaces need the map's smaller mips; without them only the glossy ones use it
	float w = dpP.y >= 4.0 ? 1.0 : saturate((0.35 - rough) / 0.2);
	return lerp(o, DpLookup(r, min(rough * 10.0, dpP.y)), w);
}
float3 EnvDiffuse(float3 n)
{
	if (envP.x < 0.5)
		return Sky(n, 1.0) * envP.y;
	return (envLL.SampleLevel(llSamp, LLuv(n), 4.5).rgb * 0.75 + envLL.SampleLevel(llSamp, float2(0.5, 0.5), 7.0).rgb * 0.25) * envP.y;
}

// Sunlight. At Present (LDR) it is sunCol as set by the plugin. Inside GTA's frame (HDR, v1.9.3) GTA's
// brightness scale is unknown, so the sun is HdrSunToSky times the sky's average brightness, which is
// learned from GTA's own HDR picture (same units as everything else we read from it).
float3 SunLight()
{
	if (lit.w <= 0.0 || envP.x < 0.5)
		return sunCol.rgb;
	float3 sky = 0;
	[unroll] for (int k = 0; k < 4; ++k)
		sky += envLL.SampleLevel(llSamp, float2(0.125 + 0.25 * k, 0.25), 5.0).rgb;
	float l = dot(sky * 0.25, float3(0.2126, 0.7152, 0.0722));
	return sunCol.rgb * lit.w * l;
}

float GGX(float NdotH, float rough)
{
	float a = max(rough * rough, 0.0025);
	float a2 = a * a;
	float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
	return a2 / (PI * d * d);
}

float3 HashColor(float k)
{
	float3 c = frac(float3(0.618034, 0.324718, 0.754878) * (k + 1.0) * 7.3 + float3(0.1, 0.4, 0.7));
	return 0.15 + 0.85 * c;
}

// ---- the car's own shadows and occlusion (depth maps of the car seen along a direction, car space)
float2 MapUV(float3 q, float3 e1, float3 e2)
{
	float rc = sl2.w;
	return float2(dot(q, e1) / rc * 0.5 + 0.5, 0.5 - dot(q, e2) / rc * 0.5);
}
float MapDepth(float3 q, float3 L) { return 0.5 - dot(q, L) / (2.0 * sl2.w); }

float SunSelf(float3 P, float3 Nc)
{
	if (sl0.w < 0.5)
		return 1.0;
	float3 q = P + Nc * 0.02 - sl3.xyz;
	float2 uv = MapUV(q, sl1.xyz, sl2.xyz);
	float d = MapDepth(q, sl0.xyz) - 0.012 / (2.0 * sl2.w);
	// 8 filtered taps on a ring (about 1.5 texels): soft edges instead of stair steps when seen up close
	float t = sl1.w * 1.5;
	float s = sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(-0.92, -0.38) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(0.38, -0.92) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(0.92, 0.38) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(-0.38, 0.92) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(-0.33, -0.14) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(0.14, -0.33) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(0.33, 0.14) * t, d);
	s += sunDepth.SampleCmpLevelZero(cmpSamp, uv + float2(-0.14, 0.33) * t, d);
	return s * 0.125;
}

// visibility of the sky around a point: weighted by the surface normal (diffuse) and by the two
// reflection directions (paint/metal and clear coat)
// v1.9.4: the 12 directions are all above the car's horizon. Reflections that point lower (the sides
// of the car mirror the ground) are not blocked by the car itself, so their occlusion fades out as the
// reflection leaves the directions that were sampled; directions behind the surface no longer count
// for the reflections (they were always "blocked" by the surface itself and darkened the paint).
void Occlusion(float3 P, float3 Nc, float3 Ngc, float3 Rm, float3 Rcc, out float aoD, out float aoS, out float aoC)
{
	aoD = 1.0;
	aoS = 1.0;
	aoC = 1.0;
	if (sl3.w < 0.5)
		return;
	float3 q = P + Ngc * aoInfo.y - sl3.xyz;
	float sD = 0, wD = 0, sS = 0, wS = 0, sC = 0, wC = 0, cS = -1, cC = -1;
	[unroll] for (int k = 0; k < 18; ++k) {
		float3 L = aoDir[k].xyz;
		float2 uv = MapUV(q, aoE1[k].xyz, aoE2[k].xyz);
		float d = MapDepth(q, L) - aoInfo.x;
		// two filtered taps per direction (opposite corners, alternating): softer occlusion edges up close
		float2 o = ((k & 1) ? float2(1.25, -1.25) : float2(1.25, 1.25)) * aoInfo.w;
		float v = 0.5 * (aoMaps.SampleCmpLevelZero(cmpSamp, float3(uv + o, k), d) + aoMaps.SampleCmpLevelZero(cmpSamp, float3(uv - o, k), d));
		float nl = dot(Nc, L);
		float w = saturate(nl) + 0.02;
		sD += w * v;
		wD += w;
		float front = nl > -0.1 ? 1.0 : 0.0;
		float rs = dot(Rm, L), rc = dot(Rcc, L);
		float ws = front * (pow(saturate(rs), 4.0) + 0.02);
		sS += ws * v;
		wS += ws;
		float wc = front * (pow(saturate(rc), 4.0) + 0.02);
		sC += wc * v;
		wC += wc;
		cS = max(cS, rs);
		cC = max(cC, rc);
	}
	aoD = lerp(1.0, sD / wD, aoInfo.z);
	// how well the sampled directions cover each reflection (1 = one of them points along it)
	float kS = saturate((cS - 0.5) / 0.4) * aoInfo.z, kC = saturate((cC - 0.5) / 0.4) * aoInfo.z;
	aoS = wS > 1e-4 ? lerp(1.0, sS / wS, kS) : 1.0;
	aoC = wC > 1e-4 ? lerp(1.0, sC / wC, kC) : 1.0;
}

float3 ToCar(float3 v) { return float3(dot(v, carRight.xyz), dot(v, carFwd.xyz), dot(v, carUp.xyz)); }
// v2.71: smooth value noise (the charred paint: your v2.7 screenshot showed 17 cm blocks)
float Hash3(float3 p) { return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
float VNoise(float3 p)
{
	float3 c = floor(p), f = frac(p);
	f = f * f * (3.0 - 2.0 * f);
	float a0 = lerp(Hash3(c), Hash3(c + float3(1, 0, 0)), f.x), a1 = lerp(Hash3(c + float3(0, 1, 0)), Hash3(c + float3(1, 1, 0)), f.x);
	float b0 = lerp(Hash3(c + float3(0, 0, 1)), Hash3(c + float3(1, 0, 1)), f.x), b1 = lerp(Hash3(c + float3(0, 1, 1)), Hash3(c + float3(1, 1, 1)), f.x);
	return lerp(lerp(a0, a1, f.y), lerp(b0, b1, f.y), f.z);
}

// v2.61: broken glass / lamps: a crack pattern on the car's own surface (cells about 7 cm; lines where two cells meet)
float2 CrackHash(float2 p) { p = float2(dot(p, float2(127.1, 311.7)), dot(p, float2(269.5, 183.3))); return frac(sin(p) * 43758.5453); }
float Cracks(float3 P, float3 N)
{
	float3 a = abs(N);
	float2 q = (a.x > a.y && a.x > a.z) ? P.yz : (a.y > a.z ? P.xz : P.xy);
	q /= 0.07;
	float2 c = floor(q), f = q - c;
	float d1 = 8.0, d2 = 8.0;
	[unroll] for (int y = -1; y <= 1; ++y)
		[unroll] for (int x = -1; x <= 1; ++x) {
			float2 r = float2(x, y) + CrackHash(c + float2(x, y)) - f;
			float d = dot(r, r);
			if (d < d1) { d2 = d1; d1 = d; }
			else if (d < d2) d2 = d;
		}
	return 1.0 - smoothstep(0.015, 0.06, sqrt(d2) - sqrt(d1));
}

struct ShadeInfo
{
	float4 w;          // x/y/z: how much layers 2/3/4 show; w: final opacity
	float3 base;       // colour after layers (linear, unlit)
	float3 srf;        // metallic, roughness, clear coat
	float3 n;          // final normal (world)
	float4 uv;
};

float4 Shade(VSOut i, out ShadeInfo si)
{
	si.w = 0;
	si.base = 0;
	si.srf = 0;
	si.n = 0;
	si.uv = i.uv;
	// derivatives first, while every pixel of the 2x2 block is still running
	Grads g;
	g.uv0 = i.uv.xy;
	g.uv1 = i.uv.zw;
	g.dx0 = ddx(i.uv.xy);
	g.dy0 = ddy(i.uv.xy);
	g.dx1 = ddx(i.uv.zw);
	g.dy1 = ddy(i.uv.zw);
	float3 dp1 = ddx(i.rel);
	float3 dp2 = ddy(i.rel);
	float3 V = normalize(-i.rel);
	float3 N0 = normalize(i.n);
	// v1.9.4: the side of the surface we see comes from the triangle itself (screen derivatives), not
	// from the vertex normal, so odd normals in a car file can't turn a surface into the car
	float3 Ng = cross(dp1, dp2);
	float ngl = length(Ng);
	Ng = ngl > 1e-20 ? Ng / ngl : V;
	if (dot(Ng, V) < 0.0)
		Ng = -Ng;                       // the face, turned towards the camera
	bool backFace = dot(N0, Ng) < 0.0;  // we see the back of this surface (e.g. glass from inside the car)
	float3 N = backFace ? -N0 : N0;     // both sides of thin parts
	Frames F;
	F.N = N;
	Cotangent(N, dp1, dp2, g.dx0, g.dy0, F.T0, F.B0);
	Cotangent(N, dp1, dp2, g.dx1, g.dy1, F.T1, F.B1);

	if (HiddenByGta(i.pos.xy + dbg.zw, i.viewZ))
		discard;
	float w0, ccr;
	float3 srf, nW;
	float4 base = EvalLayer(0, g, F, w0, srf, nW, ccr);
	float cover = base.a * w0;                    // layer 1 opacity (alpha test / see-through)
	float alpha = cover;
	float4 ws = 0;
	[unroll] for (int l = 1; l < 4; ++l) {
		if (l < (int)matInfo.x) {
			float w, cr;
			float3 s, n;
			float4 c = EvalLayer(l, g, F, w, s, n, cr);
			w = saturate(w);
			base.rgb = lerp(base.rgb, c.rgb, w);
			srf = lerp(srf, s, w);
			ccr = lerp(ccr, cr, w);
			nW = normalize(lerp(nW, n, w));
			alpha = alpha + (1.0 - alpha) * saturate(w * c.a);
			if (l == 1) ws.x = w;
			else if (l == 2) ws.y = w;
			else ws.z = w;
		}
	}
	if (matInfo.y > 0.0 && cover < matInfo.y)
		discard;
	bool glass = matInfo.z > 0.5;
	// v1.9.5: one-sided see-through parts (BeamNG doubleSided=0), e.g. window tint: BeamNG draws only their
	// front, with a separate inner surface for the view from inside. Drawing both made the 5% limo tint
	// pass twice from inside (0.25% of the light: a black side window).
	if (matInfo.z > 1.5 && backFace)
		discard;
	if (glass) {
		alpha = max(alpha, matInfo.w);
		if (alpha < 0.004)
			discard;
	} else {
		alpha = 1.0;
	}
	if (matFx.x > 0.5) {
		// v2.61: cracked (2.6 only tinted it whitish: too faint to see)
		float k = Cracks(ToCar(i.rel - carPos.xyz), ToCar(N));
		base.rgb = lerp(base.rgb * 0.6 + 0.18, float3(0.9, 0.92, 0.94), k);
		if (glass) alpha = max(alpha, lerp(0.3, 0.95, k));
	}
	float metal = saturate(srf.x), rough = clamp(srf.y, 0.03, 1.0), cc = saturate(srf.z);
	if (matKind.x > 0.5) {
		metal = 1.0;
		rough = 0.03;
		base.rgb = float3(0.9, 0.9, 0.9);
	}
	// v2.62: bullet holes (like GTA's own): a dark hole, a ring of torn bare metal with a ragged edge, a little darkening around
	// v2.7: a blown-up car: charred dark and dull, a little rust-brown, uneven (like GTA's wrecks)
	if (holeInfo.y > 0.0) {
		float3 Pw = ToCar(i.rel - carPos.xyz);
		float  nz = 0.6 * VNoise(Pw * 2.5) + 0.4 * VNoise(Pw * 9.0 + 17.0);
		float  b = holeInfo.y * (0.82 + 0.15 * nz);
		base.rgb = lerp(base.rgb, lerp(float3(0.025, 0.022, 0.02), float3(0.09, 0.05, 0.03), nz * 0.6), b);
		metal = lerp(metal, 0.15, b);
		rough = lerp(rough, 0.92, b);
		cc = lerp(cc, 0.0, b);
	}
	int nh = (int)holeInfo.x;
	if (nh > 0 && matFx.y < 0.5) {
		float3 Pc = ToCar(i.rel - carPos.xyz);
		float3 NgC = ToCar(Ng);  // v2.7: this surface's own (flat) normal, for where a bullet's line meets it
		float  hk = 0.0, rk = 0.0, sk = 0.0;
		for (int h = 0; h < nh; ++h) {
			float3 dv = Pc - holes[2 * h].xyz;
			float3 bd = holes[2 * h + 1].xyz;
			float  dd;
			if (dot(bd, bd) > 0.25) {
				// v2.7: where the bullet's line meets this surface (BeamNG's hit is on its collision triangles, a few cm inside
				// the panel): up to 30 cm before that hit, 8 cm after it. Round, or oval for a glancing shot (at most 2.5 x long)
				float  cn = dot(bd, NgC);
				if (abs(cn) < 0.05) continue;
				float  tx = dot(dv, NgC) / cn;
				if (tx < -0.30 || tx > 0.08) continue;
				float3 dxp = dv - bd * tx;  // (from where the line meets this surface's plane)
				dd = max(length(dv - bd * dot(dv, bd)), 0.4 * length(dxp)) / holes[2 * h].w;
				dv = dxp;
			} else {
				dd = length(dv) / holes[2 * h].w;
			}
			if (dd < 1.0) {
				float j = frac(sin(dot(floor(dv / max(length(dv), 1e-5) * 7.0) + h, float3(12.9898, 78.233, 37.719))) * 43758.5453);  // (ragged by direction)
				float e = dd + (j - 0.5) * 0.16;
				hk = max(hk, 1.0 - smoothstep(0.30, 0.36, e));
				rk = max(rk, smoothstep(0.30, 0.36, e) * (1.0 - smoothstep(0.60, 0.74, e)));
				sk = max(sk, 1.0 - smoothstep(0.7, 1.0, dd));
			}
		}
		base.rgb *= 1.0 - 0.25 * sk;
		base.rgb = lerp(base.rgb, float3(0.58, 0.58, 0.6), rk);
		metal = lerp(metal, 1.0, rk);
		rough = lerp(rough, 0.45, rk);
		base.rgb = lerp(base.rgb, float3(0.012, 0.012, 0.012), hk);
		metal = lerp(metal, 0.0, hk);
		rough = lerp(rough, 1.0, hk);
		cc *= 1.0 - max(hk, rk);
		if (glass) alpha = max(alpha, max(hk, rk));
	}
	if (dot(nW, V) < 0.02)
		nW = normalize(nW + V * (0.02 - dot(nW, V)));  // keep bumped normals facing us
	si.w = float4(ws.xyz, alpha);
	si.base = base.rgb;
	si.srf = float3(metal, rough, cc);
	si.n = nW;

	// the car's own shadow and occlusion (car space)
	float3 P = ToCar(i.rel - carPos.xyz);
	float3 Nc = ToCar(N);
	float3 Rw = reflect(-V, nW);
	float3 Rc = reflect(-V, N);
	float3 Ngc = ToCar(Ng);              // v1.9.4: map look-ups step off the surface towards the camera's side
	float sunV = SunSelf(P, Ngc);
	float aoD, aoS, aoC;
	Occlusion(P, Nc, Ngc, ToCar(Rw), ToCar(Rc), aoD, aoS, aoC);

	float3 L = sunDir.xyz;
	float3 sunC = SunLight();
	float3 H = normalize(L + V);
	float NdotL = saturate(dot(nW, L)) * saturate(dot(N, L) * 4.0 + 1.0);  // bumps can't light what faces away
	float NdotV = max(dot(nW, V), 1e-3);
	float NdotH = saturate(dot(nW, H));
	float LdotH = saturate(dot(L, H));
	float refl = skyGround.w * (matKind.z > 0.5 ? 0.0 : 1.0);
	float envK = 1.0;
	if (glass) {
		refl *= misc.x;
		if (backFace || lit.y > 0.5)
			envK = 0.06;  // seen from inside the car: glass reflects the dark cabin, not the outside
	}

	// diffuse: light from the surroundings (occluded by the car itself) + sun (shadowed by the car)
	float3 amb = EnvDiffuse(nW) * look.x * (0.12 + 0.88 * aoD);
	float3 diffCol = base.rgb * (1.0 - metal);
	float3 colD = diffCol * (amb + sunC * NdotL * sunV);

	// specular: reflection + sun highlight (GGX, energy-correct), Fresnel by metalness
	float3 F0 = lerp(float3(0.04, 0.04, 0.04), base.rgb, metal);
	float fres = pow(1.0 - NdotV, 5.0);
	float3 Fenv = F0 + (max(1.0 - rough, F0) - F0) * fres;
	float3 specM = EnvRefl(Rw, rough) * Fenv * refl * envK * aoS;
	float3 specEnvDbg = specM;
	float3 Fsun = F0 + (1.0 - F0) * pow(1.0 - LdotH, 5.0);
	float vis = 0.25 / max(LdotH * LdotH, 0.1);
	specM += min(PI * GGX(NdotH, rough) * vis, 60.0) * Fsun * sunC * NdotL * sunV * lit.x;

	// clear coat: a thin glossy layer on top (paint), using the smooth surface normal
	float3 specC = 0;
	if (cc > 0.0) {
		float NdotVc = max(dot(N, V), 1e-3);
		float Fc = cc * (0.04 + 0.96 * pow(1.0 - NdotVc, 5.0));
		float NdotHc = saturate(dot(N, H));
		float ccRough = clamp(ccr, 0.02, 1.0);  // v1.9.6: the paint's clear coat roughness (was fixed 0.04)
		specC = Fc * EnvRefl(Rc, ccRough) * refl * envK * aoC;
		specEnvDbg = specEnvDbg * (1.0 - Fc) + specC;
		specC += cc * min(PI * GGX(NdotHc, max(ccRough, 0.06)) * vis, 80.0) * 0.04 * sunC * saturate(dot(N, L)) * sunV * lit.x;
		colD *= 1.0 - Fc;
		specM *= 1.0 - Fc;
	}

	// output into the car layer, premultiplied
	float4 o;
	int op = (int)(matKind.y + 0.5);
	if (!glass)
		o = float4(colD + specM + specC, 1.0);
	else if (op == 2 || op == 3)
		o = float4((colD + specM + specC) * alpha, 0.0);  // additive glass (lights)
	else
		// see-through: body colour and metal reflections fade with the glass's opacity; the clear coat's
		// surface reflection stays for PreMulAlpha (BeamNG's glass), fades too for LerpAlpha
		o = float4((colD + specM) * alpha + specC * (op == 1 ? 1.0 : alpha), alpha);

	// debug views (ini DebugView)
	int dv = (int)(dbg.x + 0.5);
	if (dv == 1) o = float4(base.rgb, 1);
	else if (dv == 2) o = float4(ws.xyz, 1);
	else if (dv == 3) o = float4(nW * 0.5 + 0.5, 1);
	else if (dv == 4) o = float4(metal, rough, cc, 1);
	else if (dv == 5) o = float4(glass ? float3(alpha, alpha * 0.4, 0.0) : float3(0.0, 0.25, 0.6), 1);
	else if (dv == 6) o = float4(HashColor(matKind.w), 1);
	else if (dv == 7) o = float4(sunV * saturate(sunDir.w * 2.0) * (dot(sunCol.rgb, 1.0) > 0.01 ? 1.0 : 0.0), aoD, aoS, 1);
	else if (dv == 8) o = float4(specEnvDbg, 1);                         // v1.9.4: all reflections of the surroundings
	else if (dv == 9) o = float4(EnvRefl(Rc, 0.04), 1);                 // v1.9.4: the surroundings as the paint's mirror sees them
	else if (dv == 10) o = float4(colD, 1);                              // v1.9.4: diffuse light only
	else if (dv == 11) o = float4((DpHave(Rc) ? DpLookup(Rc, 0.0) : float3(1.0, 0.0, 1.0)), 1);  // v1.9.7: GTA's reflection map as the paint's mirror sees it (magenta: not in use)
	o.rgb *= dv == 0 ? look.z : 1.0;
	if (look.w > 0.5)
		o.rgb = pow(max(o.rgb, 0.0), 1.0 / 2.2);
	return o;
}

float4 PSCar(VSOut i) : SV_Target
{
	bool torn = Torn(i.sk, i.rp) || i.tv > 0.001;
	ShadeInfo si;
	float4 c = Shade(i, si);
	if (torn)
		discard;
	return c;
}

// Part picker: the part under the screen centre is drawn into four 1x1 targets and read back.
struct PickOut { float4 a : SV_Target0; float4 b : SV_Target1; float4 c : SV_Target2; float4 d : SV_Target3; };
PickOut PSPick(VSOut i)
{
	ShadeInfo si;
	float4 col = Shade(i, si);
	PickOut o;
	o.a = float4(matKind.w + 1.0, si.w.x, si.w.y, si.w.w);
	o.b = si.uv;
	o.c = float4(si.base, si.srf.x);
	o.d = float4(si.srf.y, si.srf.z, si.w.z, dot(normalize(i.n), normalize(-i.rel)));
	return o;
}

// ---- depth maps of the car along a direction (sun self-shadow, ambient occlusion)
struct VSOutS { float4 pos : SV_Position; float3 sk : TEXCOORD0; float3 rp : TEXCOORD1; float tv : TEXCOORD2; };
VSOutS VSDepthDir(VSIn i)
{
	VSOutS o;
	o.sk = PartPos(SkinPos(i.p, i.si, i.sc));
	o.rp = PartPos(i.p);
	o.tv = TornV(i.si);
	float3 q = o.sk - sl3.xyz;
	float rc = sl2.w;
	o.pos = float4(dot(q, sl1.xyz) / rc, dot(q, sl2.xyz) / rc, 0.5 - dot(q, sl0.xyz) / (2.0 * rc), 1);
	return o;
}

// ---- ground shadows ---------------------------------------------------------------------------
// The car's mesh is flattened onto its ground plane, either straight down (contact shadow, once per
// car) or along the sun (our sun shadow, every frame), into a small coverage map in car space.
VSOutS VSShadowMap(VSIn i)
{
	VSOutS o;
	float3 p = PartPos(SkinPos(i.p, i.si, i.sc));
	float2 q = p.xy;
	if (shadowC.x > 0.5)
		q = p.xy - shadowB.xy * (max(p.z, 0.0) / min(shadowB.z, -0.05));
	o.pos = float4(q / shadowC.y, 0.5, 1);
	o.sk = p;
	o.rp = PartPos(i.p);
	o.tv = TornV(i.si);
	return o;
}
// (also the depth maps' pixel shader, v2.54: torn triangles cast no shadow)
float PSShadowMap(VSOutS i) : SV_Target
{
	if (Torn(i.sk, i.rp) || i.tv > 0.001)
		discard;
	return 1.0;
}

// Laid onto GTA's own ground: each screen pixel's position comes from GTA's depth buffer.
float ShadowTap(Texture2D<float> m, float2 q, float halfSize, float lod)
{
	float2 uv = float2(q.x / halfSize * 0.5 + 0.5, 0.5 - q.y / halfSize * 0.5);
	if (any(uv < 0.0) || any(uv > 1.0))
		return 0.0;
	return m.SampleLevel(clampSamp, uv, lod);
}
float4 PSGroundShadow(float4 pos : SV_Position) : SV_Target
{
	float d = sceneDepth.SampleLevel(pointSamp, pos.xy * screen.xy, 0);
	float z = SceneViewZ(d);
	if (z > 60.0)
		discard;
	float2 ndc = float2(pos.x * screen.x * 2.0 - 1.0, 1.0 - pos.y * screen.y * 2.0);
	float3 rel = camRight.xyz * (ndc.x * z / proj.x) + camUp.xyz * (ndc.y * z / proj.y) + camFwd.xyz * z - carPos.xyz;
	float3 P = float3(dot(rel, carRight.xyz), dot(rel, carFwd.xyz), dot(rel, carUp.xyz));
	if (P.z < -0.6 || P.z > 1.2)
		discard;
	float soft = shadowB.w;
	float a = 0.0;
	// contact: right under the car, fading with height above the car's ground plane
	if (shadowA.z > 0.0) {
		float h = saturate(1.0 - max(P.z, 0.0) / 0.45) * saturate(1.0 + P.z / 0.35);
		float c = 0.0;
		[unroll] for (int k = 0; k < 4; ++k) {
			// v1.9.7: tighter (was 0.12 m offsets at mip 2 + softness): a soft grey halo read as a dull shadow
			float2 o = float2((k & 1) ? 0.08 : -0.08, (k & 2) ? 0.08 : -0.08) * soft;
			c += ShadowTap(contactMap, P.xy + o, shadowA.x, 1.0 + 0.75 * soft);
		}
		a = max(a, saturate(c * 0.25 * 1.25) * h * shadowA.z);
	}
	// sun: move the pixel along the sun onto the car's ground plane and look it up
	if (shadowA.w > 0.0) {
		float2 q = P.xy - shadowB.xy * (P.z / min(shadowB.z, -0.05));
		float s = 0.0;
		[unroll] for (int k = 0; k < 4; ++k) {
			float2 o = float2((k & 1) ? 0.05 : -0.05, (k & 2) ? 0.05 : -0.05) * soft;
			s += ShadowTap(sunMap, q + o, shadowA.y, 0.75 * soft);
		}
		a = max(a, saturate(s * 0.25) * shadowA.w * saturate(1.0 - max(P.z - 0.8, 0.0) * 2.0));
	}
	if (a < 0.003)
		discard;
	return float4(0, 0, 0, a);
}

float4 VSFull(uint id : SV_VertexID) : SV_Position
{
	float2 uv = float2((id << 1) & 2, id & 2);
	return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1);
}

// ---- environment learning: every direction the camera sees takes GTA's (blurred) colour there;
// directions out of view slowly blend towards their neighbourhood.
float3 ScreenColour(float2 suv, float lod)
{
	float3 s = screenCopy.SampleLevel(clampSamp, suv, lod).rgb;
	if (envP.w < 0.5)
		s = pow(max(s, 0.0), 2.2);
	return min(max(s, 0.0), 60000.0);
}
float4 PSEnvUpdate(float4 pos : SV_Position) : SV_Target
{
	float2 uv = pos.xy * float2(1.0 / 128.0, 1.0 / 64.0);
	float3 d = LLdir(uv);
	uint sw, sh, levels;
	screenCopy.GetDimensions(0, sw, sh, levels);
	float3 v = float3(dot(d, camRight.xyz), dot(d, camUp.xyz), dot(d, camFwd.xyz));
	bool seen = false;
	float3 s = 0;
	if (v.z > 0.05) {
		float2 ndc = float2(v.x * proj.x / v.z, v.y * proj.y / v.z);
		if (all(abs(ndc) < 0.98)) {
			float2 suv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
			bool inHud = hud2.x > 0.5 && suv.x > hud.x && suv.x < hud.z && suv.y > hud.y && suv.y < hud.w;
			if (!inHud) {
				s = ScreenColour(suv, envP.z);
				seen = true;
			}
		}
	}
	if (lit.z > 0.5) {
		// fresh start (v1.9.3): what the camera sees now; elsewhere the picture's average, brighter
		// above the horizon than below
		if (seen)
			return float4(s, 1);
		float3 avg = ScreenColour(float2(0.5, 0.5), float(levels) - 1.0);
		return float4(avg * (d.z > 0.0 ? 1.0 + 0.6 * d.z : 0.6), 1);
	}
	float3 prev = llPrev.SampleLevel(llSamp, uv, 0).rgb;
	if (seen)
		return float4(lerp(prev, s, 0.2), 1);
	// out of view: drift towards the neighbourhood; high sky the camera rarely sees (v1.9.3) towards the
	// sky seen just above the horizon at that compass direction, slightly bluer towards the zenith
	float3 coarse = llPrev.SampleLevel(llSamp, uv, 3.5).rgb;
	float3 target = coarse;
	if (d.z > 0.2) {
		float3 low = llPrev.SampleLevel(llSamp, float2(uv.x, 0.5 - 14.0 / 180.0), 1.5).rgb;
		float k = saturate((d.z - 0.2) / 0.6);
		target = lerp(coarse, low * lerp(float3(1.0, 1.0, 1.0), float3(0.82, 0.9, 1.05), k), 0.7);
	}
	return float4(lerp(prev, target, 0.02), 1);
}

// GTA's HDR picture, halved, into our mip-mapped copy (v1.9.3, drawing inside GTA's frame)
float4 PSDownsample(float4 pos : SV_Position) : SV_Target
{
	uint sw, sh, levels;
	screenCopy.GetDimensions(0, sw, sh, levels);
	float2 uv = pos.xy * 2.0 / float2(sw, sh);
	float3 c = screenCopy.SampleLevel(clampSamp, uv, 0).rgb;
	uint3 bits = asuint(c) & 0x7fffffff;  // no NaNs into the environment
	if (bits.x > 0x7f800000) c.x = 0.0;
	if (bits.y > 0x7f800000) c.y = 0.0;
	if (bits.z > 0x7f800000) c.z = 0.0;
	return float4(min(max(c, 0.0), 60000.0), 1);
}

// v1.9.5: GTA's light levels. Each texel of a 64x36 grid averages, over 4x4 points of the screen, how
// bright GTA's HDR picture is relative to the surface colour in GTA's G-buffer there (= how much light
// falls there), for points with geometry (not sky) and a colour that isn't nearly black. Row 36 holds
// what our own model uses (sky brightness, light from the surroundings straight up).
float4 PSLightProbe(float4 pos : SV_Position) : SV_Target
{
	if (pos.y > 36.0) {
		float3 up = EnvDiffuse(float3(0, 0, 1));
		float3 sky = 0;
		[unroll] for (int k = 0; k < 4; ++k)
			sky += envLL.SampleLevel(llSamp, float2(0.125 + 0.25 * k, 0.25), 5.0).rgb;
		return float4(dot(sky * 0.25, float3(0.2126, 0.7152, 0.0722)), dot(up, float3(0.2126, 0.7152, 0.0722)), envP.x, 1);
	}
	uint sw, sh, sl;
	screenCopy.GetDimensions(0, sw, sh, sl);
	float2 cell = float2(sw, sh) / float2(64.0, 36.0);
	float sr = 0, n = 0, sc = 0, sa = 0;
	[unroll] for (int j = 0; j < 4; ++j) {
		[unroll] for (int i = 0; i < 4; ++i) {
			int2  p = int2((floor(pos.xy) + (float2(i, j) + 0.5) / 4.0) * cell);
			float z = SceneViewZ(sceneDepth.Load(int3(p, 0)));
			float3 a = pow(max(gAlbedo.Load(int3(p, 0)).rgb, 0.0), 2.2);
			float al = dot(a, float3(0.2126, 0.7152, 0.0722));
			float cl = dot(min(max(screenCopy.Load(int3(p, 0)).rgb, 0.0), 60000.0), float3(0.2126, 0.7152, 0.0722));
			if (z < 300.0 && al > 0.04) {
				sr += cl / al;
				n += 1.0;
				sc += cl;
				sa += al;
			}
		}
	}
	return n > 0.0 ? float4(sr / n, n, sc / n, sa / n) : float4(0, 0, 0, 0);
}

// ---- v1.9.7: which layout GTA's reflection map has. One column per screen point (16 x 9 grid), one row
// per layout candidate (32: fixed / turning with the camera x half x swap x sign x x sign y) plus a last
// row with GTA's picture there:
// x brightness, y height of the direction (z), z 1 = usable (in view, not the minimap), w distance (m).
float4 PSDpCalib(float4 pos : SV_Position) : SV_Target
{
	int px = (int)pos.x, py = (int)pos.y;
	int gx = px % 16, gy = px / 16;
	float2 ndc = float2(-0.9 + 1.8 * (gx + 0.5) / 16.0, 0.9 - 1.8 * (gy + 0.5) / 9.0);
	float3 d = normalize(camRight.xyz * (ndc.x / proj.x) + camUp.xyz * (ndc.y / proj.y) + camFwd.xyz);
	float2 suv = float2(ndc.x * 0.5 + 0.5, 0.5 - ndc.y * 0.5);
	const float3 lw = float3(0.2126, 0.7152, 0.0722);
	if (py >= 32) {
		bool inHud = hud2.x > 0.5 && suv.x > hud.x && suv.x < hud.z && suv.y > hud.y && suv.y < hud.w;
		float z = SceneViewZ(sceneDepth.SampleLevel(pointSamp, suv, 0));
		float3 c = min(max(screenCopy.SampleLevel(clampSamp, suv, 3.0).rgb, 0.0), 60000.0);
		return float4(dot(c, lw), d.z, inHud ? 0.0 : 1.0, min(z, 1e6));
	}
	float4 L = float4(((py >> 3) & 1) + 2 * ((py >> 4) & 1), (py >> 2) & 1, ((py >> 1) & 1) ? -1.0 : 1.0, (py & 1) ? -1.0 : 1.0);
	float3 g = min(max(gtaRefl.SampleLevel(clampSamp, DpUv(d, L), 3.0).rgb, 0.0), 60000.0);
	return float4(dot(g, lw), 0.0, 0.0, 0.0);
}

// ---- the car layer onto GTA's picture: smoothed silhouette edges, minimap kept on top
float4 PSComposite(float4 pos : SV_Position) : SV_Target
{
	int2 p = int2(pos.xy);
	float2 uv = pos.xy * screen.xy;
	if (hud2.x > 0.5) {
		float2 m = hud2.w * screen.xy;
		if (uv.x > hud.x - m.x && uv.x < hud.z + m.x && uv.y > hud.y - m.y && uv.y < hud.w + m.y)
			discard;
	}
	float4 c = layerTex.Load(int3(p, 0));
	if (hud2.y > 0.5) {
		float4 n = layerTex.Load(int3(p + int2(0, -1), 0));
		float4 s = layerTex.Load(int3(p + int2(0, 1), 0));
		float4 e = layerTex.Load(int3(p + int2(1, 0), 0));
		float4 w = layerTex.Load(int3(p + int2(-1, 0), 0));
		float amin = min(c.a, min(min(n.a, s.a), min(e.a, w.a)));
		float amax = max(c.a, max(max(n.a, s.a), max(e.a, w.a)));
		if (amax - amin > 0.35) {
			float4 ne = layerTex.Load(int3(p + int2(1, -1), 0));
			float4 nw = layerTex.Load(int3(p + int2(-1, -1), 0));
			float4 se = layerTex.Load(int3(p + int2(1, 1), 0));
			float4 sw = layerTex.Load(int3(p + int2(-1, 1), 0));
			c = (c * 4.0 + (n + s + e + w) * 2.0 + (ne + nw + se + sw)) / 16.0;
		}
	}
	if (c.a < 0.002 && dot(c.rgb, float3(1, 1, 1)) < 0.002)
		discard;
	if (hud2.z > 0.5)
		c.rgb = pow(max(c.rgb, 0.0), 2.2);
	return c;
}

// Debug view: near = white, far = black, an orange line every 10 m.
float4 PSDepthView(float4 pos : SV_Position) : SV_Target
{
	float z = SceneViewZ(sceneDepth.SampleLevel(pointSamp, pos.xy * screen.xy, 0));
	float g = exp(-z / 40.0);
	float band = frac(z / 10.0) < 0.04 ? 1.0 : 0.0;
	return float4(lerp(float3(g, g, g), float3(1.0, 0.35, 0.0), band * 0.8), 1);
}

// Part picker crosshair at the screen centre.
float4 PSCross(float4 pos : SV_Position) : SV_Target
{
	float2 d = abs(pos.xy - floor(0.5 / screen.xy) - 0.5);
	bool on = (d.x < 1.0 && d.y < 12.0 && d.y > 3.0) || (d.y < 1.0 && d.x < 12.0 && d.x > 3.0);
	if (!on)
		discard;
	return float4(1.0, 0.85, 0.1, 1.0);
}

static const float2 kProbe[17] = {
	float2(0.1, 0.1), float2(0.3, 0.1), float2(0.5, 0.1), float2(0.7, 0.1), float2(0.9, 0.1),
	float2(0.2, 0.35), float2(0.5, 0.35), float2(0.8, 0.35),
	float2(0.2, 0.65), float2(0.5, 0.65), float2(0.8, 0.65),
	float2(0.1, 0.9), float2(0.3, 0.9), float2(0.5, 0.9), float2(0.7, 0.9), float2(0.9, 0.9),
	float2(0.5, 0.5) };
float PSProbe(float4 pos : SV_Position) : SV_Target
{
	return sceneDepth.SampleLevel(pointSamp, kProbe[(uint)pos.x], 0);
}
)";

	struct alignas(16) FrameCB
	{
		float camRight[4];
		float camUp[4];
		float camFwd[4];
		float proj[4];
		float depthP[4];
		float screen[4];
		float sunDir[4];
		float carPos[4];
		float carRight[4];
		float carFwd[4];
		float carUp[4];
		float look[4];
		float paint0[4];
		float paint1[4];
		float paint2[4];
		float skyTop[4];
		float skyHorizon[4];
		float skyGround[4];
		float sunCol[4];
		float shadowA[4];
		float shadowB[4];
		float shadowC[4];
		float envP[4];
		float feat[4];
		float dbg[4];
		float misc[4];
		float sl0[4];
		float sl1[4];
		float sl2[4];
		float sl3[4];
		float hud[4];
		float hud2[4];
		float lit[4];
		float paintS0[4];  // v1.9.6: paint slots 1/2/3: metallic, roughness, clear coat, clear coat roughness
		float paintS1[4];
		float paintS2[4];
		float dpU[4];      // v1.9.7: GTA reflection map layout of the upper half of all directions: x half (0 left, 1 right), y swap, z/w signs
		float dpL[4];      //         the same for the lower half
		float dpP[4];      //         x upper usable, y highest mip, z brightness, w lower usable
	};

	struct alignas(16) LayerCB
	{
		float factor[4];
		float a[4];
		float b[4];
		float c[4];
		float d[4];
		float e[4];  // v1.9.3: channels read from opacity / metallic / roughness maps, normal map swizzle
	};

	struct alignas(16) MatCB
	{
		LayerCB lay[4];
		float   matInfo[4];
		float   matKind[4];
		float   matFx[4];  // v2.61
	};

	struct alignas(16) PartCB
	{
		float m[3][4];
	};

	struct alignas(16) HoleCB
	{
		float holes[kHoles][8];
		float info[4];
	};

	struct alignas(16) OcclCB
	{
		float aoDir[kAoDirs][4];
		float aoE1[kAoDirs][4];
		float aoE2[kAoDirs][4];
		float aoInfo[4];
	};

	// ---------------------------------------------------------------------------------------
	// Renderer
	// ---------------------------------------------------------------------------------------

	struct StateBackup
	{
		ID3D11RenderTargetView*   rtv[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
		ID3D11DepthStencilView*   dsv = nullptr;
		ID3D11BlendState*         blend = nullptr;
		float                     blendFactor[4] = {};
		UINT                      sampleMask = 0;
		ID3D11DepthStencilState*  ds = nullptr;
		UINT                      stencilRef = 0;
		ID3D11RasterizerState*    rs = nullptr;
		D3D11_VIEWPORT            vps[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE] = {};
		UINT                      vpCount = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
		D3D11_PRIMITIVE_TOPOLOGY  topo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11InputLayout*        layout = nullptr;
		ID3D11Buffer*             vb = nullptr;
		UINT                      vbStride = 0, vbOffset = 0;
		ID3D11Buffer*             vb1 = nullptr;  // v2.5: slot 1 (the dents' vertex data) and vertex shader view 35
		UINT                      vb1Stride = 0, vb1Offset = 0;
		ID3D11ShaderResourceView* vsSrv35 = nullptr;
		ID3D11Buffer*             ib = nullptr;
		DXGI_FORMAT               ibFormat = DXGI_FORMAT_UNKNOWN;
		UINT                      ibOffset = 0;
		ID3D11VertexShader*       vs = nullptr;
		ID3D11PixelShader*        ps = nullptr;
		ID3D11GeometryShader*     gs = nullptr;
		ID3D11HullShader*         hs = nullptr;
		ID3D11DomainShader*       dsh = nullptr;
		ID3D11ShaderResourceView* psSrv[kPsSlots] = {};
		ID3D11SamplerState*       psSamp[kSamplers] = {};
		ID3D11Buffer*             vsCb[4] = {};
		ID3D11Buffer*             psCb[4] = {};
		// v1.9.3 "deep" save, for drawing in the middle of GTA's frame: every pixel shader slot, vertex and
		// compute shader views and output UAVs (binding GTA's HDR picture as our target unbinds it from
		// any slot it sits in, and GTA's next pass must find everything as it left it)
		bool                       deep = false;
		ID3D11ShaderResourceView*  psSrvAll[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT] = {};
		ID3D11ShaderResourceView*  vsSrv[16] = {};
		ID3D11ShaderResourceView*  csSrv[16] = {};
		ID3D11UnorderedAccessView* csUav[8] = {};
		ID3D11UnorderedAccessView* omUav[8] = {};
		UINT                       omUavStart = 8;

		void SaveDeep(ID3D11DeviceContext* c)
		{
			deep = true;
			Save(c);
			c->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psSrvAll);
			c->VSGetShaderResources(0, 16, vsSrv);
			c->CSGetShaderResources(0, 16, csSrv);
			c->CSGetUnorderedAccessViews(0, 8, csUav);
			UINT nrt = 0;
			for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
				if (rtv[i]) nrt = i + 1;
			if (nrt < 8) {
				c->OMGetRenderTargetsAndUnorderedAccessViews(0, nullptr, nullptr, nrt, 8 - nrt, omUav + nrt);
				for (UINT i = nrt; i < 8; ++i)
					if (omUav[i] && omUavStart == 8) omUavStart = i;
			}
		}

		void Save(ID3D11DeviceContext* c)
		{
			c->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtv, &dsv);
			c->OMGetBlendState(&blend, blendFactor, &sampleMask);
			c->OMGetDepthStencilState(&ds, &stencilRef);
			c->RSGetState(&rs);
			c->RSGetViewports(&vpCount, vps);
			c->IAGetPrimitiveTopology(&topo);
			c->IAGetInputLayout(&layout);
			c->IAGetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
			c->IAGetVertexBuffers(1, 1, &vb1, &vb1Stride, &vb1Offset);
			c->VSGetShaderResources(35, 1, &vsSrv35);
			c->IAGetIndexBuffer(&ib, &ibFormat, &ibOffset);
			c->VSGetShader(&vs, nullptr, nullptr);
			c->PSGetShader(&ps, nullptr, nullptr);
			c->GSGetShader(&gs, nullptr, nullptr);
			c->HSGetShader(&hs, nullptr, nullptr);
			c->DSGetShader(&dsh, nullptr, nullptr);
			c->PSGetShaderResources(0, kPsSlots, psSrv);
			c->PSGetSamplers(0, kSamplers, psSamp);
			c->VSGetConstantBuffers(0, 4, vsCb);
			c->PSGetConstantBuffers(0, 4, psCb);
		}

		void Restore(ID3D11DeviceContext* c)
		{
			if (deep) {
				// output first (so views of the same textures can go back on the inputs), then the inputs
				if (omUavStart < 8) {
					UINT nrt = 0;
					for (UINT i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
						if (rtv[i]) nrt = i + 1;
					const UINT keep[8] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
					c->OMSetRenderTargetsAndUnorderedAccessViews(nrt, rtv, dsv, omUavStart, 8 - omUavStart, omUav + omUavStart, keep);
				} else {
					c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtv, dsv);
				}
				c->PSSetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, psSrvAll);
				c->VSSetShaderResources(0, 16, vsSrv);
				c->CSSetShaderResources(0, 16, csSrv);
				const UINT keep[8] = { UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1), UINT(-1) };
				c->CSSetUnorderedAccessViews(0, 8, csUav, keep);
				for (auto*& v : psSrvAll) SafeRelease(v);
				for (auto*& v : vsSrv) SafeRelease(v);
				for (auto*& v : csSrv) SafeRelease(v);
				for (auto*& v : csUav) SafeRelease(v);
				for (auto*& v : omUav) SafeRelease(v);
			} else {
				c->PSSetShaderResources(0, kPsSlots, psSrv);  // before the depth buffer goes back on the output
				c->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtv, dsv);
			}
			c->OMSetBlendState(blend, blendFactor, sampleMask);
			c->OMSetDepthStencilState(ds, stencilRef);
			c->RSSetState(rs);
			c->RSSetViewports(vpCount, vps);
			c->IASetPrimitiveTopology(topo);
			c->IASetInputLayout(layout);
			c->IASetVertexBuffers(0, 1, &vb, &vbStride, &vbOffset);
			c->IASetVertexBuffers(1, 1, &vb1, &vb1Stride, &vb1Offset);
			c->VSSetShaderResources(35, 1, &vsSrv35);
			c->IASetIndexBuffer(ib, ibFormat, ibOffset);
			c->VSSetShader(vs, nullptr, 0);
			c->PSSetShader(ps, nullptr, 0);
			c->GSSetShader(gs, nullptr, 0);
			c->HSSetShader(hs, nullptr, 0);
			c->DSSetShader(dsh, nullptr, 0);
			c->PSSetSamplers(0, kSamplers, psSamp);
			c->VSSetConstantBuffers(0, 4, vsCb);
			c->PSSetConstantBuffers(0, 4, psCb);
			for (auto*& r : rtv) SafeRelease(r);
			SafeRelease(dsv);
			SafeRelease(blend);
			SafeRelease(ds);
			SafeRelease(rs);
			SafeRelease(layout);
			SafeRelease(vb);
			SafeRelease(vb1);
			SafeRelease(vsSrv35);
			SafeRelease(ib);
			SafeRelease(vs);
			SafeRelease(ps);
			SafeRelease(gs);
			SafeRelease(hs);
			SafeRelease(dsh);
			for (auto*& s : psSrv) SafeRelease(s);
			for (auto*& s : psSamp) SafeRelease(s);
			for (auto*& b : vsCb) SafeRelease(b);
			for (auto*& b : psCb) SafeRelease(b);
		}
	};

	struct DepthSource
	{
		ID3D11DepthStencilView*   dsv = nullptr;
		ID3D11Texture2D*          tex = nullptr;
		D3D11_TEXTURE2D_DESC      td = {};
		int                       id = 0;
		ID3D11ShaderResourceView* liveSrv = nullptr;
		ID3D11Texture2D*          presentCopy = nullptr;
		ID3D11ShaderResourceView* presentCopySrv = nullptr;
		ID3D11Texture2D*          clearCopy = nullptr;
		ID3D11ShaderResourceView* clearCopySrv = nullptr;
	};

	struct Probe
	{
		bool      pending = false;
		uint64_t  frame = 0;
		CamSample cam = {};
		bool      fromClearCopy = false;
	};

	// The car on the GPU.
	constexpr UINT kNodeTexW = 1024;  // v2.5 (dents): width of the node texture (the shader's NodeD uses the same)

	struct GpuCar
	{
		ID3D11Buffer*                          vb = nullptr;
		ID3D11Buffer*                          ib = nullptr;
		std::vector<ID3D11ShaderResourceView*> tex;
		std::vector<int>                       texSwz;  // per texture: channel the shader reads (see car::Texture::swizzle)
		std::vector<car::Draw>                 draws;
		std::vector<car::Material>             mats;
		std::vector<car::Wheel>                wheels;
		std::vector<uint8_t>                   dgHide;  // v2.6: per breakable group: hidden (1) or frosted (0) when broken
		float                                  doorHinge[2][2] = {};  // v2.73: the front doors' hinge lines (car space x, y)
		float                                  size[3] = {};
		int                                    opaqueDraws = 0;
		float                                  paint[3][4] = {};
		bool                                   hasPaint = false;
		float                                  paintPbr[3][4] = {};  // v1.9.6: metallic, roughness, clear coat, clear coat roughness
		bool                                   hasPaintPbr = false;
		uint64_t                               bytes = 0;
		size_t                                 tris = 0;
		double                                 centre[3] = {};  // bounding sphere (car space)
		double                                 radius = 3.0;
		// v2.5 (dents): each vertex's triangle of nodes (vertex buffer slot 1) and where the nodes are now (t35)
		ID3D11Buffer*                          skinVb = nullptr;
		UINT                                   skinStride = 0;  // 0: one shared entry (the file has no node data)
		ID3D11Texture2D*                       nodeBuf = nullptr;  // 1024 texels wide, 2 per node (kNodeTexW)
		ID3D11ShaderResourceView*              nodeSrv = nullptr;
		UINT                                   nodeN = 0;
		uint64_t                               nodeSeq = 0;  // the dents data last copied to nodeBuf
		uint32_t                               gen = 0;      // which loaded car file this is (g_modelGen)

		void Release()
		{
			SafeRelease(vb);
			SafeRelease(ib);
			SafeRelease(skinVb);
			SafeRelease(nodeBuf);
			SafeRelease(nodeSrv);
			skinStride = 0;
			nodeN = 0;
			nodeSeq = 0;
			gen = 0;
			for (auto*& t : tex) SafeRelease(t);
			tex.clear();
			texSwz.clear();
			draws.clear();
			mats.clear();
			wheels.clear();
		}
	};

	// Part picker: four 1x1 targets read back a few frames later.
	struct Picker
	{
		ID3D11Texture2D*        rt[4] = {};
		ID3D11RenderTargetView* rtv[4] = {};
		ID3D11Texture2D*        staging[4] = {};
		ID3D11Texture2D*        depth = nullptr;
		ID3D11DepthStencilView* dsv = nullptr;
		bool                    pending = false;
		uint64_t                frame = 0;
		int                     lastMat = -2;
		ULONGLONG               lastNotifyMs = 0;
	};

	// A depth map (or an array of them) with a shader view.
	struct DepthMaps
	{
		ID3D11Texture2D*                     tex = nullptr;
		std::vector<ID3D11DepthStencilView*> dsv;
		ID3D11ShaderResourceView*            srv = nullptr;
		UINT                                 size = 0, count = 0;

		void Release()
		{
			for (auto*& d : dsv) SafeRelease(d);
			dsv.clear();
			SafeRelease(srv);
			SafeRelease(tex);
			size = count = 0;
		}
	};

	struct Renderer
	{
		bool                     tried = false, ok = false;
		ID3D11Device*            dev = nullptr;
		ID3D11DeviceContext*     ctx = nullptr;
		ID3D11VertexShader*      vsCar = nullptr;
		ID3D11VertexShader*      vsFull = nullptr;
		ID3D11VertexShader*      vsDepthDir = nullptr;
		ID3D11PixelShader*       psCar = nullptr;
		ID3D11PixelShader*       psPick = nullptr;
		ID3D11PixelShader*       psDepthView = nullptr;
		ID3D11PixelShader*       psProbe = nullptr;
		ID3D11PixelShader*       psCross = nullptr;
		ID3D11VertexShader*      vsShadowMap = nullptr;
		ID3D11PixelShader*       psShadowMap = nullptr;
		ID3D11PixelShader*       psGroundShadow = nullptr;
		ID3D11PixelShader*       psEnvUpdate = nullptr;
		ID3D11PixelShader*       psComposite = nullptr;
		ID3D11PixelShader*       psDownsample = nullptr;
		ID3D11PixelShader*       psLightProbe = nullptr;  // v1.9.5
		// v1.9.5: light measurement (GTA's G-buffer colour vs its HDR picture), read back a few frames later
		ID3D11Texture2D*         lightTex = nullptr;
		ID3D11RenderTargetView*  lightRtv = nullptr;
		ID3D11Texture2D*         lightStaging = nullptr;
		bool                     lightPending = false;
		uint64_t                 lightFrame = 0;
		ID3D11Resource*          gbufRes = nullptr;  // GTA's G-buffer colour target (identity) and our view of it
		ID3D11ShaderResourceView* gbufSrv = nullptr;
		bool                     gbufBad = false;
		double                   lightShade = 0, lightLit = 0, lightSky = 0, lightUp = 0;
		int                      lightSamples = 0;
		double                   autoSun = 0, autoAmb = 0;  // v1.9.6: HdrSunToSky / HdrAmbient from the measurement (smoothed)
		float                    lightDayAt = 0, lightSunAt = 0;  // v1.9.6: day factor and sun visibility when the measurement was taken
		// v1.9.7: GTA's reflection map, read in place (a view on GTA's own texture), and finding its orientation:
		// GTA's picture is compared with the map in 32 possible layouts at 144 points (16 x 9 grid) of the screen
		struct DpView
		{
			ID3D11Resource*           res = nullptr;  // identity of GTA's texture (kept alive by the view)
			ID3D11ShaderResourceView* srv = nullptr;
			UINT                      w = 0, h = 0, mips = 0, written = 0;  // mip levels it has / GTA writes
			uint64_t                  lastFrame = 0;
			bool                      bad = false;
		};
		DpView                    dpViews[4];  // GTA may swap between textures (sandbox review)
		int                       dpCur = -1;
		ID3D11ShaderResourceView* dpSrv = nullptr;  // the one to use this frame
		UINT                      dpMips = 0;       // its usable mip levels
		bool                      dpUsable = false;
		ID3D11PixelShader*        psDpCalib = nullptr;
		ID3D11Texture2D*          dpCalTex = nullptr;
		ID3D11RenderTargetView*   dpCalRtv = nullptr;
		ID3D11Texture2D*          dpCalStaging = nullptr;
		bool                      dpCalPending = false;
		uint64_t                  dpCalFrame = 0;
		int                       dpLayout[2] = { -1, -1 };  // found layout: [0] sky half (up), [1] ground half (down)
		int                       dpHist[2][8] = {};         // last decisive winners (+1; 0 = none)
		int                       dpHistPos[2] = {};
		double                    dpScale = 0;               // GTA's picture / map brightness (smoothed)
		double                    dpLastR[2] = {}, dpLastR2[2] = {};  // best / second best correlation (log)
		int                       dpLastN[2] = {}, dpLastBest[2] = { -1, -1 };
		uint32_t                  dpTests = 0, dpDecisive[2] = {};
		ULONGLONG                 dpLogMs = 0, dpFirstHdrMs = 0;
		bool                      dpNotFoundLogged = false;
		ULONGLONG                lightLogMs = 0;
		ID3D11ShaderResourceView* probeHdrSrv = nullptr;  // GTA's full-size HDR picture while drawing inside its frame
		ID3D11BlendState*        layerPremul = nullptr;  // see-through car parts into the car layer
		ID3D11BlendState*        layerAdd = nullptr;
		ID3D11BlendState*        composite = nullptr;    // the car layer onto GTA's picture
		ID3D11SamplerState*      clampLin = nullptr;     // shadow maps, GTA's picture
		ID3D11SamplerState*      cmpSamp = nullptr;      // depth-map comparisons
		ID3D11SamplerState*      llSamp = nullptr;       // environment (wraps around)
		ID3D11RasterizerState*   rsDepth = nullptr;      // depth maps (slope bias)
		// shadow maps: the car flattened onto its ground plane (car space), with mips for softness
		ID3D11Texture2D*          contactTex = nullptr;
		ID3D11RenderTargetView*   contactRtv = nullptr;
		ID3D11ShaderResourceView* contactSrv = nullptr;
		ID3D11Texture2D*          sunTex = nullptr;
		ID3D11RenderTargetView*   sunRtv = nullptr;
		ID3D11ShaderResourceView* sunSrv = nullptr;
		bool                      contactDirty = true;
		// the car's own sun shadow and ambient occlusion
		DepthMaps                 sunDepth;
		DepthMaps                 aoMaps;
		ID3D11Buffer*             occlCb = nullptr;
		ID3D11Buffer*             holeCb = nullptr;  // v2.62
		uint32_t                  holeGenUp = ~0u;
		bool                      aoDirty = true;
		// GTA's picture with mips (environment learning)
		ID3D11Texture2D*          envTex = nullptr;
		ID3D11ShaderResourceView* envSrv = nullptr;
		UINT                      envW = 0, envH = 0, envMips = 0;
		DXGI_FORMAT               envFmt = DXGI_FORMAT_UNKNOWN;
		bool                      envFailed = false;
		// v1.9.3: GTA's HDR picture, halved, with mips (environment learning inside GTA's frame)
		ID3D11Texture2D*          envHdrTex = nullptr;
		ID3D11RenderTargetView*   envHdrRtv = nullptr;
		ID3D11ShaderResourceView* envHdrSrv = nullptr;
		UINT                      envHdrW = 0, envHdrH = 0, envHdrMips = 0;
		// the source the environment learns from this frame (one of the two above)
		ID3D11ShaderResourceView* envSrcSrv = nullptr;
		UINT                      envSrcMips = 0;
		bool                      envSrcHalf = false, envSrcLinear = false;
		int                       lastStage = -1;  // 0 drawn at Present, 1 inside GTA's frame (the environment restarts on a change)
		int                       stageFrames = 0;
		uint32_t                  stageSwaps = 0;  // v1.9.5: switches between drawing inside GTA's frame and at Present (log)
		// learned environment (ping-pong)
		ID3D11Texture2D*          llTex[2] = {};
		ID3D11RenderTargetView*   llRtv[2] = {};
		ID3D11ShaderResourceView* llSrv[2] = {};
		int                       llCur = 0;
		bool                      llInit = true;
		bool                      llReady = false;
		double                    llCam[3] = {};
		ULONGLONG                 llUsedMs = 0;
		// v1.9.5: the other stage's learned surroundings (at Present = LDR, inside GTA's frame = HDR), kept so
		// a switch between the two swaps them instead of starting over
		ID3D11Texture2D*          llTexO[2] = {};
		ID3D11RenderTargetView*   llRtvO[2] = {};
		ID3D11ShaderResourceView* llSrvO[2] = {};
		int                       llCurO = 0;
		bool                      llInitO = true;
		bool                      llReadyO = false;
		double                    llCamO[3] = {};
		ULONGLONG                 llUsedMsO = 0;
		// the car layer
		ID3D11Texture2D*          layerTex = nullptr;  // RGBA16F (v1.9.3): holds HDR values when drawn inside GTA's frame
		ID3D11RenderTargetView*   layerRtv = nullptr;
		ID3D11ShaderResourceView* layerSrv = nullptr;
		UINT                      layerW = 0, layerH = 0;
		ID3D11Resource*           bbRes = nullptr;  // GTA's back buffer (identity only)
		Picker                    pick;
		ID3D11InputLayout*       layout = nullptr;
		ID3D11Buffer*            cb = nullptr;
		ID3D11Buffer*            matCb = nullptr;
		ID3D11Buffer*            partCb = nullptr;
		ID3D11BlendState*        opaque = nullptr;
		ID3D11BlendState*        alphaBlend = nullptr;
		ID3D11RasterizerState*   raster = nullptr;
		ID3D11DepthStencilState* noDepth = nullptr;
		ID3D11DepthStencilState* depthWrite = nullptr;
		ID3D11DepthStencilState* depthRead = nullptr;
		ID3D11SamplerState*      point = nullptr;
		ID3D11SamplerState*      linear = nullptr;
		ID3D11Texture2D*         probeTex = nullptr;
		ID3D11Texture2D*         probeStaging = nullptr;
		ID3D11RenderTargetView*  probeRtv = nullptr;
		ID3D11Texture2D*         ownDepth = nullptr;  // the drawn car's own depth (so it hides itself correctly)
		ID3D11DepthStencilView*  ownDsv = nullptr;
		UINT                     ownW = 0, ownH = 0;

		GpuCar car;

		DepthSource depth;
		Probe       probe;
		UINT        bbW = 0, bbH = 0;
		DXGI_FORMAT bbFmt = DXGI_FORMAT_UNKNOWN;
		bool        reversed = true;
		bool        depthKnown = false;
		int         convVotes = 0;
		bool        convVote = true;
		ULONGLONG   lastDisagreeLogMs = 0;
		bool        useClearCopy = false;
		int         flatLive = 0, flatCopy = 0;
		bool        msaaWarned = false;
		int         missingFrames = 0, challengerFrames = 0;

		int       checks = 0, checksGood = 0;
		char      lastCheck[64] = "no centre check yet";
		ULONGLONG lastCheckLogMs = 0;

		double    cpuMsSum = 0, cpuMsMax = 0;
		uint32_t  statFrames = 0, statDrawn = 0;
		uint32_t  ticksAtStat = 0;
		ULONGLONG statStartMs = 0, lastCandLogMs = 0;
		int       lastTopId = -1;
		float     lastSunVis = 1.0f;
		// v1.9.3 timing: frame intervals, pose samples used per frame
		double    lastPresentT = 0, frameMsMax = 0, frameMsSum = 0;
		uint32_t  hitches = 0;
		uint32_t  lastSeq = 0, seqSame = 0, seqOne = 0, seqMulti = 0;
		uint64_t  ticksUsAtStat = 0;
	} g_r;

	// how many new pose samples arrived since the last drawn frame (0 = the car would repeat its last
	// pose while GTA's picture moved on: a skip)
	void NoteSampleUse(uint32_t seq)
	{
		Renderer& r = g_r;
		if (r.lastSeq) {
			const uint32_t d = seq - r.lastSeq;
			if (d == 0) ++r.seqSame;
			else if (d == 1) ++r.seqOne;
			else ++r.seqMulti;
		}
		r.lastSeq = seq;
	}

	pD3DCompile LoadCompiler()
	{
		const char* dlls[] = { "d3dcompiler_47.dll", "d3dcompiler_46.dll", "d3dcompiler_43.dll" };
		for (const char* name : dlls) {
			if (HMODULE m = LoadLibraryA(name)) {
				if (auto fn = reinterpret_cast<pD3DCompile>(reinterpret_cast<void*>(GetProcAddress(m, "D3DCompile")))) {
					Log("shader compiler: %s", name);
					return fn;
				}
			}
		}
		Log("shader compiler: none found (d3dcompiler_47.dll missing?)");
		return nullptr;
	}

	bool CompileShader(pD3DCompile compile, const char* entry, const char* target, ID3DBlob** out)
	{
		ID3DBlob*     errors = nullptr;
		const HRESULT hr = compile(kShader, sizeof(kShader) - 1, "GtaBeamD1", nullptr, nullptr, entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, out, &errors);
		if (FAILED(hr)) {
			Log("shader %s failed: %s", entry, errors ? static_cast<const char*>(errors->GetBufferPointer()) : "(no message)");
			SafeRelease(errors);
			return false;
		}
		SafeRelease(errors);
		return true;
	}

	bool InitRenderer(IDXGISwapChain* sc)
	{
		Renderer& r = g_r;
		if (r.tried) {
			return r.ok;
		}
		r.tried = true;
		if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), reinterpret_cast<void**>(&r.dev))) || !r.dev) {
			Log("renderer: the swap chain has no D3D11 device");
			return false;
		}
		r.dev->GetImmediateContext(&r.ctx);
		g_immediate = r.ctx;

		DXGI_SWAP_CHAIN_DESC sd{};
		sc->GetDesc(&sd);
		const D3D_FEATURE_LEVEL fl = r.dev->GetFeatureLevel();
		Log("renderer: swap chain %ux%u %s, %u buffers, MSAA x%u, windowed %d, feature level %x.%x, device flags 0x%x", sd.BufferDesc.Width,
			sd.BufferDesc.Height, FmtName(sd.BufferDesc.Format), sd.BufferCount, sd.SampleDesc.Count, sd.Windowed, (fl >> 12) & 0xF, (fl >> 8) & 0xF,
			r.dev->GetCreationFlags());

		HookContext(r.ctx, "immediate");
		ID3D11DeviceContext* deferred = nullptr;
		if (SUCCEEDED(r.dev->CreateDeferredContext(0, &deferred)) && deferred) {
			HookContext(deferred, "deferred");
			deferred->Release();
		} else {
			Log("hooks: no deferred contexts on this device (single-threaded rendering)");
		}

		pD3DCompile compile = LoadCompiler();
		if (!compile) {
			return false;
		}
		const bool  sm5 = fl >= D3D_FEATURE_LEVEL_11_0;
		const char* vsT = sm5 ? "vs_5_0" : "vs_4_0";
		const char* psT = sm5 ? "ps_5_0" : "ps_4_0";
		struct Entry
		{
			const char* name;
			bool        vs;
			ID3DBlob*   blob;
		} entries[] = {
			{ "VSCar", true, nullptr }, { "VSFull", true, nullptr }, { "PSCar", false, nullptr }, { "PSPick", false, nullptr }, { "PSDepthView", false, nullptr },
			{ "PSProbe", false, nullptr }, { "PSCross", false, nullptr }, { "VSShadowMap", true, nullptr }, { "PSShadowMap", false, nullptr },
			{ "PSGroundShadow", false, nullptr }, { "VSDepthDir", true, nullptr }, { "PSEnvUpdate", false, nullptr }, { "PSComposite", false, nullptr },
			{ "PSDownsample", false, nullptr }, { "PSLightProbe", false, nullptr }, { "PSDpCalib", false, nullptr },
		};
		bool ok = true;
		for (Entry& e : entries) {
			ok = ok && CompileShader(compile, e.name, e.vs ? vsT : psT, &e.blob);
		}
		if (ok) {
			auto vs = [&](int i, ID3D11VertexShader** out) { r.dev->CreateVertexShader(entries[i].blob->GetBufferPointer(), entries[i].blob->GetBufferSize(), nullptr, out); };
			auto ps = [&](int i, ID3D11PixelShader** out) { r.dev->CreatePixelShader(entries[i].blob->GetBufferPointer(), entries[i].blob->GetBufferSize(), nullptr, out); };
			vs(0, &r.vsCar);
			vs(1, &r.vsFull);
			ps(2, &r.psCar);
			ps(3, &r.psPick);
			ps(4, &r.psDepthView);
			ps(5, &r.psProbe);
			ps(6, &r.psCross);
			vs(7, &r.vsShadowMap);
			ps(8, &r.psShadowMap);
			ps(9, &r.psGroundShadow);
			vs(10, &r.vsDepthDir);
			ps(11, &r.psEnvUpdate);
			ps(12, &r.psComposite);
			ps(13, &r.psDownsample);
			ps(14, &r.psLightProbe);
			ps(15, &r.psDpCalib);  // v1.9.7
			const D3D11_INPUT_ELEMENT_DESC il[] = {
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				// v2.5 (dents), v2.53: each vertex's triangle of nodes (a second vertex buffer: 4 x 16-bit, 3 floats)
				{ "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT, 1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 1, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			};
			const HRESULT hl = r.dev->CreateInputLayout(il, 6, entries[0].blob->GetBufferPointer(), entries[0].blob->GetBufferSize(), &r.layout);
			if (FAILED(hl)) Log("renderer: input layout FAILED (0x%08X)", unsigned(hl));
		}
		for (Entry& e : entries) SafeRelease(e.blob);
		if (!ok) {
			return false;
		}

		D3D11_BUFFER_DESC bd{};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		bd.ByteWidth = sizeof(FrameCB);
		r.dev->CreateBuffer(&bd, nullptr, &r.cb);
		bd.ByteWidth = sizeof(MatCB);
		r.dev->CreateBuffer(&bd, nullptr, &r.matCb);
		bd.ByteWidth = sizeof(PartCB);
		r.dev->CreateBuffer(&bd, nullptr, &r.partCb);
		bd.ByteWidth = sizeof(OcclCB);
		r.dev->CreateBuffer(&bd, nullptr, &r.occlCb);
		bd.ByteWidth = sizeof(HoleCB);
		r.dev->CreateBuffer(&bd, nullptr, &r.holeCb);  // v2.62
		r.holeGenUp = ~0u;

		D3D11_BLEND_DESC bl{};
		bl.RenderTarget[0].BlendEnable = FALSE;
		bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		r.dev->CreateBlendState(&bl, &r.opaque);
		bl.RenderTarget[0].BlendEnable = TRUE;
		bl.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bl.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		bl.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE;
		r.dev->CreateBlendState(&bl, &r.alphaBlend);
		bl.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;  // premultiplied layer over GTA's picture
		r.dev->CreateBlendState(&bl, &r.composite);
		// into the car layer (premultiplied colour and coverage)
		bl.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		bl.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		bl.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		r.dev->CreateBlendState(&bl, &r.layerPremul);
		bl.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		bl.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		bl.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		r.dev->CreateBlendState(&bl, &r.layerAdd);

		D3D11_RASTERIZER_DESC rd{};
		rd.FillMode = D3D11_FILL_SOLID;
		rd.CullMode = D3D11_CULL_NONE;  // BeamNG car materials are double sided
		rd.DepthClipEnable = TRUE;
		r.dev->CreateRasterizerState(&rd, &r.raster);
		rd.SlopeScaledDepthBias = 1.5f;
		rd.DepthBias = 8;
		rd.DepthBiasClamp = 0.01f;
		r.dev->CreateRasterizerState(&rd, &r.rsDepth);

		D3D11_DEPTH_STENCIL_DESC dd{};
		dd.DepthEnable = FALSE;
		r.dev->CreateDepthStencilState(&dd, &r.noDepth);
		dd.DepthEnable = TRUE;
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dd.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		r.dev->CreateDepthStencilState(&dd, &r.depthWrite);
		dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		r.dev->CreateDepthStencilState(&dd, &r.depthRead);

		D3D11_SAMPLER_DESC smp{};
		smp.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		smp.MaxLOD = D3D11_FLOAT32_MAX;
		r.dev->CreateSamplerState(&smp, &r.point);
		smp.Filter = D3D11_FILTER_ANISOTROPIC;
		smp.MaxAnisotropy = 8;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		r.dev->CreateSamplerState(&smp, &r.linear);
		smp.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		smp.MaxAnisotropy = 1;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		r.dev->CreateSamplerState(&smp, &r.clampLin);
		smp.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		r.dev->CreateSamplerState(&smp, &r.llSamp);
		smp.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
		smp.AddressU = smp.AddressV = smp.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
		smp.BorderColor[0] = smp.BorderColor[1] = smp.BorderColor[2] = smp.BorderColor[3] = 1.0f;
		smp.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
		r.dev->CreateSamplerState(&smp, &r.cmpSamp);

		// ground shadow maps (R8, with mips so the shadows can be soft)
		auto makeMap = [&](UINT size, ID3D11Texture2D** t, ID3D11RenderTargetView** rtv, ID3D11ShaderResourceView** srv) {
			D3D11_TEXTURE2D_DESC td{};
			td.Width = td.Height = size;
			td.MipLevels = 0;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R8_UNORM;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			if (SUCCEEDED(r.dev->CreateTexture2D(&td, nullptr, t)) && *t) {
				r.dev->CreateRenderTargetView(*t, nullptr, rtv);
				r.dev->CreateShaderResourceView(*t, nullptr, srv);
			}
		};
		makeMap(512, &r.contactTex, &r.contactRtv, &r.contactSrv);
		makeMap(1024, &r.sunTex, &r.sunRtv, &r.sunSrv);

		// learned environment maps
		{
			D3D11_TEXTURE2D_DESC td{};
			td.Width = kLLW;
			td.Height = kLLH;
			td.MipLevels = 0;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			for (int k = 0; k < 2; ++k) {
				if (SUCCEEDED(r.dev->CreateTexture2D(&td, nullptr, &r.llTex[k])) && r.llTex[k]) {
					r.dev->CreateRenderTargetView(r.llTex[k], nullptr, &r.llRtv[k]);
					r.dev->CreateShaderResourceView(r.llTex[k], nullptr, &r.llSrv[k]);
				}
				if (SUCCEEDED(r.dev->CreateTexture2D(&td, nullptr, &r.llTexO[k])) && r.llTexO[k]) {
					r.dev->CreateRenderTargetView(r.llTexO[k], nullptr, &r.llRtvO[k]);
					r.dev->CreateShaderResourceView(r.llTexO[k], nullptr, &r.llSrvO[k]);
				}
			}
		}

		// part picker targets
		{
			D3D11_TEXTURE2D_DESC td{};
			td.Width = td.Height = 1;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			td.SampleDesc.Count = 1;
			for (int k = 0; k < 4; ++k) {
				td.Usage = D3D11_USAGE_DEFAULT;
				td.BindFlags = D3D11_BIND_RENDER_TARGET;
				td.CPUAccessFlags = 0;
				if (SUCCEEDED(r.dev->CreateTexture2D(&td, nullptr, &r.pick.rt[k])) && r.pick.rt[k]) {
					r.dev->CreateRenderTargetView(r.pick.rt[k], nullptr, &r.pick.rtv[k]);
				}
				td.Usage = D3D11_USAGE_STAGING;
				td.BindFlags = 0;
				td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
				r.dev->CreateTexture2D(&td, nullptr, &r.pick.staging[k]);
			}
			td.Format = DXGI_FORMAT_D32_FLOAT;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
			td.CPUAccessFlags = 0;
			if (SUCCEEDED(r.dev->CreateTexture2D(&td, nullptr, &r.pick.depth)) && r.pick.depth) {
				r.dev->CreateDepthStencilView(r.pick.depth, nullptr, &r.pick.dsv);
			}
		}

		D3D11_TEXTURE2D_DESC pt{};
		pt.Width = 17;
		pt.Height = 1;
		pt.MipLevels = 1;
		pt.ArraySize = 1;
		pt.Format = DXGI_FORMAT_R32_FLOAT;
		pt.SampleDesc.Count = 1;
		pt.Usage = D3D11_USAGE_DEFAULT;
		pt.BindFlags = D3D11_BIND_RENDER_TARGET;
		r.dev->CreateTexture2D(&pt, nullptr, &r.probeTex);
		if (r.probeTex) {
			r.dev->CreateRenderTargetView(r.probeTex, nullptr, &r.probeRtv);
		}
		pt.Usage = D3D11_USAGE_STAGING;
		pt.BindFlags = 0;
		pt.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		r.dev->CreateTexture2D(&pt, nullptr, &r.probeStaging);

		r.ok = r.vsCar && r.vsFull && r.vsDepthDir && r.psCar && r.psPick && r.psDepthView && r.psProbe && r.psCross && r.vsShadowMap && r.psShadowMap &&
		       r.psGroundShadow && r.psEnvUpdate && r.psComposite && r.psDownsample && r.layerPremul && r.layerAdd && r.composite && r.clampLin && r.cmpSamp && r.llSamp &&
		       r.rsDepth && r.contactSrv && r.sunSrv && r.layout && r.cb && r.matCb && r.partCb && r.occlCb && r.opaque && r.alphaBlend && r.raster && r.noDepth &&
		       r.depthWrite && r.depthRead && r.point && r.linear && r.probeRtv && r.probeStaging;
		Log("renderer: %s (part picker %s, learned environment %s)", r.ok ? "ready" : "FAILED to create its resources",
			(r.pick.rtv[3] && r.pick.staging[3] && r.pick.dsv) ? "ready" : "not available", (r.llSrv[0] && r.llSrv[1]) ? "ready" : "not available");
		r.statStartMs = GetTickCount64();
		return r.ok;
	}

	bool EnsureOwnDepth(UINT w, UINT h)
	{
		Renderer& r = g_r;
		if (r.ownDsv && r.ownW == w && r.ownH == h) {
			return true;
		}
		SafeRelease(r.ownDsv);
		SafeRelease(r.ownDepth);
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_D32_FLOAT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.ownDepth)) || FAILED(r.dev->CreateDepthStencilView(r.ownDepth, nullptr, &r.ownDsv))) {
			Log("renderer: could not create the car's depth buffer %ux%u", w, h);
			SafeRelease(r.ownDepth);
			return false;
		}
		r.ownW = w;
		r.ownH = h;
		return true;
	}

	bool EnsureLayer(UINT w, UINT h)
	{
		Renderer& r = g_r;
		if (r.layerRtv && r.layerW == w && r.layerH == h) {
			return true;
		}
		SafeRelease(r.layerRtv);
		SafeRelease(r.layerSrv);
		SafeRelease(r.layerTex);
		D3D11_TEXTURE2D_DESC td{};
		td.Width = w;
		td.Height = h;
		td.MipLevels = 1;
		td.ArraySize = 1;
		td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.layerTex)) || FAILED(r.dev->CreateRenderTargetView(r.layerTex, nullptr, &r.layerRtv)) ||
		    FAILED(r.dev->CreateShaderResourceView(r.layerTex, nullptr, &r.layerSrv))) {
			Log("renderer: could not create the car layer %ux%u", w, h);
			SafeRelease(r.layerRtv);
			SafeRelease(r.layerSrv);
			SafeRelease(r.layerTex);
			return false;
		}
		r.layerW = w;
		r.layerH = h;
		Log("renderer: car layer %ux%u (%.1f MB)", w, h, w * h * 8 / 1048576.0);
		return true;
	}

	// A D16 depth map (or array) that can be rendered into and compared against.
	bool EnsureDepthMaps(DepthMaps& m, UINT size, UINT count, const char* what)
	{
		Renderer& r = g_r;
		if (m.tex && m.size == size && m.count == count) {
			return true;
		}
		m.Release();
		D3D11_TEXTURE2D_DESC td{};
		td.Width = td.Height = size;
		td.MipLevels = 1;
		td.ArraySize = count;
		td.Format = DXGI_FORMAT_R16_TYPELESS;
		td.SampleDesc.Count = 1;
		td.Usage = D3D11_USAGE_DEFAULT;
		td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &m.tex)) || !m.tex) {
			Log("renderer: could not create %s depth maps (%ux%u x%u)", what, size, size, count);
			return false;
		}
		m.dsv.assign(count, nullptr);
		for (UINT i = 0; i < count; ++i) {
			D3D11_DEPTH_STENCIL_VIEW_DESC dv{};
			dv.Format = DXGI_FORMAT_D16_UNORM;
			if (count > 1) {
				dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
				dv.Texture2DArray.FirstArraySlice = i;
				dv.Texture2DArray.ArraySize = 1;
			} else {
				dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			}
			r.dev->CreateDepthStencilView(m.tex, &dv, &m.dsv[i]);
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
		sv.Format = DXGI_FORMAT_R16_UNORM;
		if (count > 1) {
			sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			sv.Texture2DArray.MipLevels = 1;
			sv.Texture2DArray.ArraySize = count;
		} else {
			sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			sv.Texture2D.MipLevels = 1;
		}
		r.dev->CreateShaderResourceView(m.tex, &sv, &m.srv);
		m.size = size;
		m.count = count;
		Log("renderer: %s depth maps %ux%u x%u (%.1f MB)", what, size, size, count, size * size * count * 2 / 1048576.0);
		return m.srv != nullptr;
	}

	// GTA's finished picture, copied with a full mip chain: the environment is learned from it.
	bool UpdateScreenCopy(ID3D11Texture2D* bb, const D3D11_TEXTURE2D_DESC& bbd)
	{
		Renderer& r = g_r;
		if (bbd.SampleDesc.Count != 1 || bbd.ArraySize != 1) {
			return false;
		}
		if (!r.envTex || r.envW != bbd.Width || r.envH != bbd.Height || r.envFmt != bbd.Format) {
			SafeRelease(r.envSrv);
			SafeRelease(r.envTex);
			if (r.envFailed && r.envFmt == bbd.Format && r.envW == bbd.Width && r.envH == bbd.Height) {
				return false;
			}
			r.envW = bbd.Width;
			r.envH = bbd.Height;
			r.envFmt = bbd.Format;
			D3D11_TEXTURE2D_DESC td{};
			td.Width = bbd.Width;
			td.Height = bbd.Height;
			td.MipLevels = 0;
			td.ArraySize = 1;
			td.Format = bbd.Format;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.envTex)) || !r.envTex || FAILED(r.dev->CreateShaderResourceView(r.envTex, nullptr, &r.envSrv))) {
				SafeRelease(r.envTex);
				r.envFailed = true;
				Log("reflections: could not make a mip-mapped copy of GTA's picture (%s); using the made-up sky", FmtName(bbd.Format));
				return false;
			}
			r.envTex->GetDesc(&td);
			r.envMips = td.MipLevels;
			r.envFailed = false;
			Log("reflections: learning the surroundings from GTA's picture (%ux%u %s, %u mip levels)", bbd.Width, bbd.Height, FmtName(bbd.Format), r.envMips);
		}
		r.ctx->CopySubresourceRegion(r.envTex, 0, 0, 0, 0, bb, 0, nullptr);
		r.ctx->GenerateMips(r.envSrv);
		r.envSrcSrv = r.envSrv;
		r.envSrcMips = r.envMips;
		r.envSrcHalf = false;
		r.envSrcLinear = IsSrgb(r.envFmt);
		return true;
	}

	// v1.9.3: GTA's HDR picture (inside its frame), halved into a mip-mapped float copy
	bool UpdateScreenCopyHdr(ID3D11ShaderResourceView* hdrSrv, UINT w, UINT h)
	{
		Renderer&  r = g_r;
		const UINT hw = std::max(1u, w / 2), hh = std::max(1u, h / 2);
		if (!r.envHdrTex || r.envHdrW != hw || r.envHdrH != hh) {
			SafeRelease(r.envHdrRtv);
			SafeRelease(r.envHdrSrv);
			SafeRelease(r.envHdrTex);
			D3D11_TEXTURE2D_DESC td{};
			td.Width = hw;
			td.Height = hh;
			td.MipLevels = 0;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			td.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
			D3D11_RENDER_TARGET_VIEW_DESC rv{};
			rv.Format = td.Format;
			rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.envHdrTex)) || !r.envHdrTex || FAILED(r.dev->CreateRenderTargetView(r.envHdrTex, &rv, &r.envHdrRtv)) ||
			    FAILED(r.dev->CreateShaderResourceView(r.envHdrTex, nullptr, &r.envHdrSrv))) {
				SafeRelease(r.envHdrRtv);
				SafeRelease(r.envHdrSrv);
				SafeRelease(r.envHdrTex);
				return false;
			}
			r.envHdrTex->GetDesc(&td);
			r.envHdrW = hw;
			r.envHdrH = hh;
			r.envHdrMips = td.MipLevels;
			Log("reflections: learning the surroundings from GTA's HDR picture (copy %ux%u, %u mip levels, %.1f MB)", hw, hh, r.envHdrMips, hw * hh * 8 * 1.34 / 1048576.0);
		}
		const float    zero[4] = {};
		D3D11_VIEWPORT vp{ 0, 0, float(hw), float(hh), 0, 1 };
		r.ctx->OMSetRenderTargets(1, &r.envHdrRtv, nullptr);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psDownsample, nullptr, 0);
		ID3D11SamplerState* ss[kSamplers] = { r.point, r.linear, r.clampLin, r.cmpSamp, r.llSamp };
		r.ctx->PSSetSamplers(0, kSamplers, ss);
		r.ctx->PSSetShaderResources(31, 1, &hdrSrv);
		r.ctx->Draw(3, 0);
		ID3D11ShaderResourceView* none = nullptr;
		r.ctx->PSSetShaderResources(31, 1, &none);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->GenerateMips(r.envHdrSrv);
		r.envSrcSrv = r.envHdrSrv;
		r.envSrcMips = r.envHdrMips;
		r.envSrcHalf = true;
		r.envSrcLinear = true;
		return true;
	}

	// ---------------------------------------------------------------------------------------
	// Car upload (render thread, when the loader has a new car)
	// ---------------------------------------------------------------------------------------

	float ToLinear(float v) { return powf(std::max(v, 0.0f), g_cfg.paintGamma); }

	// texture / view formats for block-compressed data (BC1/2/3/7 have sRGB views for colour maps)
	void BcFormats(DXGI_FORMAT f, bool colour, DXGI_FORMAT& texFmt, DXGI_FORMAT& srvFmt, UINT& blockBytes)
	{
		blockBytes = 0;
		switch (f) {
		case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC1_UNORM_SRGB:
			texFmt = DXGI_FORMAT_BC1_TYPELESS; srvFmt = colour ? DXGI_FORMAT_BC1_UNORM_SRGB : DXGI_FORMAT_BC1_UNORM; blockBytes = 8; break;
		case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC2_UNORM_SRGB:
			texFmt = DXGI_FORMAT_BC2_TYPELESS; srvFmt = colour ? DXGI_FORMAT_BC2_UNORM_SRGB : DXGI_FORMAT_BC2_UNORM; blockBytes = 16; break;
		case DXGI_FORMAT_BC3_UNORM: case DXGI_FORMAT_BC3_UNORM_SRGB:
			texFmt = DXGI_FORMAT_BC3_TYPELESS; srvFmt = colour ? DXGI_FORMAT_BC3_UNORM_SRGB : DXGI_FORMAT_BC3_UNORM; blockBytes = 16; break;
		case DXGI_FORMAT_BC7_UNORM: case DXGI_FORMAT_BC7_UNORM_SRGB:
			texFmt = DXGI_FORMAT_BC7_TYPELESS; srvFmt = colour ? DXGI_FORMAT_BC7_UNORM_SRGB : DXGI_FORMAT_BC7_UNORM; blockBytes = 16; break;
		case DXGI_FORMAT_BC4_UNORM: texFmt = srvFmt = DXGI_FORMAT_BC4_UNORM; blockBytes = 8; break;
		case DXGI_FORMAT_BC5_UNORM: texFmt = srvFmt = DXGI_FORMAT_BC5_UNORM; blockBytes = 16; break;
		default: break;
		}
	}

	DXGI_FORMAT TexFormat(int kind)
	{
		switch (kind) {
		case car::kTexSingle: return DXGI_FORMAT_R8_UNORM;
		case car::kTexNormal: return DXGI_FORMAT_R8G8_UNORM;
		case car::kTexMask: return DXGI_FORMAT_R8G8B8A8_UNORM;
		default: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		}
	}

	void UploadCar(std::unique_ptr<car::Model> m, uint32_t gen)
	{
		Renderer& r = g_r;
		r.car.Release();
		g_haveModel.store(false);
		r.pick.lastMat = -2;
		const ULONGLONG t0 = GetTickCount64();

		D3D11_BUFFER_DESC      bd{};
		D3D11_SUBRESOURCE_DATA init{};
		bd.Usage = D3D11_USAGE_IMMUTABLE;
		bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		bd.ByteWidth = UINT(m->verts.size() * sizeof(car::Vtx));
		init.pSysMem = m->verts.data();
		if (FAILED(r.dev->CreateBuffer(&bd, &init, &r.car.vb))) {
			Log("car upload: vertex buffer FAILED (%u bytes)", bd.ByteWidth);
			Notify("D1: could not put the car in video memory (vertex buffer)");
			return;
		}
		bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		bd.ByteWidth = UINT(m->indices.size() * sizeof(uint32_t));
		init.pSysMem = m->indices.data();
		if (FAILED(r.dev->CreateBuffer(&bd, &init, &r.car.ib))) {
			Log("car upload: index buffer FAILED (%u bytes)", bd.ByteWidth);
			Notify("D1: could not put the car in video memory (index buffer)");
			r.car.Release();
			return;
		}
		uint64_t bytes = uint64_t(m->verts.size()) * sizeof(car::Vtx) + uint64_t(m->indices.size()) * 4;
		// v2.5 (dents), v2.53: each vertex's triangle of nodes (vertex buffer slot 1: 3 node numbers + how, 3 coordinates)
		// and where the nodes are now (t35: per node how far it moved, where it was). Without node data one zero entry that
		// every vertex reads (stride 0): the car is drawn as it is.
		{
			struct SkinV
			{
				uint16_t i[4];
				float    c[3];
			};
			static_assert(sizeof(SkinV) == 20, "skin vertex size");
			const bool skin = m->skinned > 0 && m->skinIdx.size() == m->verts.size() * 4 && m->skinC.size() == m->verts.size() * 3 && !m->nodeCid.empty();
			std::vector<SkinV> sv(skin ? m->verts.size() : 1);
			memset(sv.data(), 0, sv.size() * sizeof(SkinV));
			if (skin) {
				for (size_t v = 0; v < sv.size(); ++v) {
					for (int k = 0; k < 4; ++k) sv[v].i[k] = m->skinIdx[v * 4 + k];
					for (int k = 0; k < 3; ++k) sv[v].c[k] = m->skinC[v * 3 + k];
				}
			}
			D3D11_BUFFER_DESC      sb{};
			D3D11_SUBRESOURCE_DATA si{};
			sb.Usage = D3D11_USAGE_IMMUTABLE;
			sb.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			sb.ByteWidth = UINT(sv.size() * sizeof(SkinV));
			si.pSysMem = sv.data();
			if (FAILED(r.dev->CreateBuffer(&sb, &si, &r.car.skinVb))) {
				Log("car upload: dents vertex buffer FAILED (%u bytes)", sb.ByteWidth);
				Notify("D1: could not put the car in video memory (dents buffer)");
				r.car.Release();
				return;
			}
			r.car.skinStride = skin ? UINT(sizeof(SkinV)) : 0;
			bytes += sb.ByteWidth;
			const size_t nn = m->nodeCid.size();
			if (skin && nn > 0) {
				// a float4 texture, 1024 texels wide (v2.53: 2 per node: moved, where it is on the car)
				const UINT         rows = UINT((nn * 2 + kNodeTexW - 1) / kNodeTexW);
				std::vector<float> nd(size_t(rows) * kNodeTexW * 4, 0.0f);
				if (m->nodePos.size() == nn * 3)
					for (size_t i = 0; i < nn; ++i)
						for (int k = 0; k < 3; ++k) nd[i * 8 + 4 + k] = m->nodePos[i * 3 + k];
				D3D11_TEXTURE2D_DESC   nb{};
				D3D11_SUBRESOURCE_DATA ni{};
				nb.Width = kNodeTexW;
				nb.Height = rows;
				nb.MipLevels = 1;
				nb.ArraySize = 1;
				nb.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				nb.SampleDesc.Count = 1;
				nb.Usage = D3D11_USAGE_DYNAMIC;
				nb.BindFlags = D3D11_BIND_SHADER_RESOURCE;
				nb.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
				ni.pSysMem = nd.data();
				ni.SysMemPitch = kNodeTexW * 16;
				if (FAILED(r.dev->CreateTexture2D(&nb, &ni, &r.car.nodeBuf)) || FAILED(r.dev->CreateShaderResourceView(r.car.nodeBuf, nullptr, &r.car.nodeSrv))) {
					Log("car upload: dents node buffer FAILED (%zu nodes): the car is drawn without dents", nn);
					SafeRelease(r.car.nodeBuf);
					SafeRelease(r.car.nodeSrv);
				} else {
					r.car.nodeN = UINT(nn);
					bytes += uint64_t(rows) * kNodeTexW * 16;
				}
			}
		}
		r.car.gen = gen;
		int      texFailed = 0;
		r.car.tex.assign(m->textures.size(), nullptr);
		r.car.texSwz.assign(m->textures.size(), 0);
		int bcCount = 0;
		for (size_t i = 0; i < m->textures.size(); ++i) {
			const car::Texture& t = m->textures[i];
			if (t.mips.empty()) {
				continue;
			}
			D3D11_TEXTURE2D_DESC td{};
			td.Width = UINT(t.w);
			td.Height = UINT(t.h);
			td.MipLevels = UINT(t.mips.size());
			td.ArraySize = 1;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_IMMUTABLE;
			td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			DXGI_FORMAT srvFmt = DXGI_FORMAT_UNKNOWN;
			UINT        blockBytes = 0;
			if (t.dxgi) {
				// v1.9.3: block-compressed data straight from BeamNG's .dds (colour maps read as sRGB)
				BcFormats(DXGI_FORMAT(t.dxgi), t.kind == car::kTexColor, td.Format, srvFmt, blockBytes);
				if (!blockBytes) {
					++texFailed;
					continue;
				}
			} else {
				td.Format = TexFormat(t.kind);
			}
			std::vector<D3D11_SUBRESOURCE_DATA> sub(t.mips.size());
			int                                 w = t.w, h = t.h;
			for (size_t k = 0; k < t.mips.size(); ++k) {
				sub[k].pSysMem = t.mips[k].data();
				sub[k].SysMemPitch = blockBytes ? UINT(std::max(1, (w + 3) / 4)) * blockBytes : UINT(w * t.bpp);
				bytes += t.mips[k].size();
				w = std::max(1, w / 2);
				h = std::max(1, h / 2);
			}
			ID3D11Texture2D* tex = nullptr;
			if (FAILED(r.dev->CreateTexture2D(&td, sub.data(), &tex)) || !tex) {
				++texFailed;
				Log("car upload: texture %s (%ux%u, %s) could not be created", t.name.c_str(), td.Width, td.Height, FmtName(td.Format));
				continue;
			}
			if (blockBytes) {
				D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
				sv.Format = srvFmt;
				sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				sv.Texture2D.MipLevels = td.MipLevels;
				r.dev->CreateShaderResourceView(tex, &sv, &r.car.tex[i]);
				++bcCount;
			} else {
				r.dev->CreateShaderResourceView(tex, nullptr, &r.car.tex[i]);
			}
			r.car.texSwz[i] = t.swizzle;
			tex->Release();
		}
		r.car.draws = m->draws;
		r.car.dgHide = m->dgHide;
		memcpy(r.car.doorHinge, m->doorHinge, sizeof(r.car.doorHinge));
		for (int k = 0; k < 2; ++k) g_doorParts[k].store(int(std::count_if(m->draws.begin(), m->draws.end(), [k](const car::Draw& x) { return x.door == k; })));
		{   // v2.63: each breakable group's box, for the hits that shatter a cracked one
			std::vector<std::array<float, 6>> box(m->dgNames.size(), { 1e9f, 1e9f, 1e9f, -1e9f, -1e9f, -1e9f });
			for (const car::Draw& dw : m->draws) {
				if (dw.dg < 0 || dw.dg >= int(box.size()) || dw.wheel >= 0) continue;
				for (uint32_t q = dw.start; q < dw.start + dw.count && q < m->indices.size(); ++q) {
					const float* pp = m->verts[m->indices[q]].p;
					for (int a = 0; a < 3; ++a) box[dw.dg][a] = std::min(box[dw.dg][a], pp[a]), box[dw.dg][3 + a] = std::max(box[dw.dg][3 + a], pp[a]);
				}
			}
			std::lock_guard<std::mutex> lk(g_holeMx);
			g_dgBox.swap(box);
			memset(g_dgHits, 0, sizeof(g_dgHits));
			for (auto& b : g_dgShatter) b.store(0);
		}
		r.car.mats = m->materials;
		r.car.wheels = m->wheels;
		memcpy(r.car.size, m->size, sizeof(r.car.size));
		// bounding sphere for the car's depth maps (wheels may steer/rise a little: 5% extra)
		r.car.centre[0] = 0;
		r.car.centre[1] = 0;
		r.car.centre[2] = m->size[2] * 0.5;
		r.car.radius = 0.5 * sqrt(double(m->size[0]) * m->size[0] + double(m->size[1]) * m->size[1] + double(m->size[2]) * m->size[2]) * 1.05 + 0.1;
		r.contactDirty = true;
		r.aoDirty = true;
		r.car.opaqueDraws = m->opaqueDraws;
		r.car.tris = m->indices.size() / 3;
		r.car.bytes = bytes;
		r.car.hasPaint = m->hasPaint;
		for (int i = 0; i < 3; ++i) {
			for (int k = 0; k < 3; ++k) {
				r.car.paint[i][k] = ToLinear(m->paint[i][k]);
			}
			r.car.paint[i][3] = m->paint[i][3];
		}
		r.car.hasPaintPbr = m->hasPaintPbr;
		memcpy(r.car.paintPbr, m->paintPbr, sizeof(r.car.paintPbr));
		if (m->hasPaintPbr)
			Log("paint surface from BeamNG (%s): metallic/roughness/clear coat/clear coat roughness %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f",
				m->paintPbrSource.c_str(), m->paintPbr[0][0], m->paintPbr[0][1], m->paintPbr[0][2], m->paintPbr[0][3], m->paintPbr[1][0], m->paintPbr[1][1],
				m->paintPbr[1][2], m->paintPbr[1][3], m->paintPbr[2][0], m->paintPbr[2][1], m->paintPbr[2][2], m->paintPbr[2][3]);
		else
			Log("paint surface NOT in the car file (%s): using metallic %.2f, roughness %.2f, clear coat %.2f, clear coat roughness %.2f (re-export with Shift+F11 "
			    "after installing the BeamNG mod 1.9.6, or set PaintMetallic/PaintRoughness/... in the ini)",
				m->paintPbrSource.empty() ? "BeamNG mod older than 1.9.6" : m->paintPbrSource.c_str(), kPaintDefault[0], kPaintDefault[1], kPaintDefault[2],
				kPaintDefault[3]);
		g_haveModel.store(true);
		Log("car upload: %zu draws (%d opaque), %zu triangles, %zu textures (%d compressed, %d failed), %zu wheels, %.1f MB video memory, %llu ms (vehicle '%s', %s, %d materials from BeamNG mod %s)",
			r.car.draws.size(), r.car.opaqueDraws, r.car.tris, m->textures.size(), bcCount, texFailed, m->wheels.size(), bytes / 1048576.0,
			static_cast<unsigned long long>(GetTickCount64() - t0), m->vehicle.c_str(), m->frame.c_str(), m->modMaterials,
			m->modVersion.empty() ? "?" : m->modVersion.c_str());
		// one line per material, so look problems can be matched to BeamNG's materials.txt
		for (size_t i = 0; i < m->materials.size(); ++i) {
			const car::Material& mt = m->materials[i];
			char                 lay[360] = "";
			int                  n = 0;
			for (int l = 0; l < mt.nLayers; ++l) {
				const car::Layer& y = mt.layers[l];
				n += snprintf(lay + n, sizeof(lay) - n, " | L%d %.2f %.2f %.2f%s%s%s%s%s%s%s%s m%.2f r%.2f", l + 1, y.factor[0], y.factor[1], y.factor[2],
					y.live ? " LIVE-SCREEN" : y.baseTex >= 0 ? " map" : "",
					y.paint ? (y.palConst >= 0 ? (y.palConst == 0 ? " paint1" : y.palConst == 1 ? " paint2" : " paint3") : y.palTex >= 0 ? " paintmask" : " paint1") : "",
					y.opTex >= 0 ? (y.maskIgnored ? " opmap(IGNORED)" : " opmap") : "", (l > 0 && y.opTex < 0) ? " (no mask)" : "", y.normTex >= 0 ? " nrm" : "",
					y.metalTex >= 0 ? " mmap" : "", y.roughTex >= 0 ? " rmap" : "", y.clearCoat > 0 ? " coat" : "", y.metallic, y.roughness);
				if (n >= int(sizeof(lay)) - 1) break;
			}
			char kind[64] = "";
			if (mt.translucent) {
				snprintf(kind, sizeof(kind), " [see-through, %s]", BlendName(mt.blendOp));
			}
			Log("  material %3zu %-28s%s%s%s", i, mt.name.c_str(), kind, mt.noReflections ? " [no reflections]" : "", lay);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Choosing and reading GTA's depth buffer (unchanged from D0)
	// ---------------------------------------------------------------------------------------

	void ReleaseDepth()
	{
		DepthSource& d = g_r.depth;
		AcquireSRWLockExclusive(&g_dsvLock);
		g_cc = ClearCopy{};
		ReleaseSRWLockExclusive(&g_dsvLock);
		SafeRelease(d.liveSrv);
		SafeRelease(d.presentCopySrv);
		SafeRelease(d.presentCopy);
		SafeRelease(d.clearCopySrv);
		SafeRelease(d.clearCopy);
		SafeRelease(d.tex);
		d = DepthSource{};
		g_r.probe.pending = false;
		g_r.depthKnown = false;
		g_r.convVotes = 0;
		g_r.useClearCopy = false;
		g_r.flatLive = g_r.flatCopy = 0;
	}

	bool MakeCopy(const D3D11_TEXTURE2D_DESC& td, ID3D11Texture2D** tex, ID3D11ShaderResourceView** srv)
	{
		D3D11_TEXTURE2D_DESC cd = td;
		cd.Format = TypelessOf(td.Format);
		cd.Usage = D3D11_USAGE_DEFAULT;
		cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		cd.CPUAccessFlags = 0;
		cd.MiscFlags = 0;
		if (FAILED(g_r.dev->CreateTexture2D(&cd, nullptr, tex)) || !*tex) {
			return false;
		}
		D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
		sv.Format = SrvFormatOf(cd.Format);
		sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sv.Texture2D.MostDetailedMip = 0;
		sv.Texture2D.MipLevels = 1;
		if (FAILED(g_r.dev->CreateShaderResourceView(*tex, &sv, srv)) || !*srv) {
			SafeRelease(*tex);
			return false;
		}
		return true;
	}

	void UseDepth(const DsvEntry& e)
	{
		ReleaseDepth();
		DepthSource&    d = g_r.depth;
		ID3D11Resource* res = nullptr;
		e.dsv->GetResource(&res);
		if (!res) {
			return;
		}
		res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&d.tex));
		res->Release();
		if (!d.tex) {
			return;
		}
		d.tex->GetDesc(&d.td);
		d.dsv = e.dsv;
		d.id = e.id;
		const DXGI_FORMAT tl = TypelessOf(d.td.Format);
		bool              direct = false;
		if (d.td.Format == tl && (d.td.BindFlags & D3D11_BIND_SHADER_RESOURCE)) {
			D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
			sv.Format = SrvFormatOf(tl);
			sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			sv.Texture2D.MostDetailedMip = 0;
			sv.Texture2D.MipLevels = 1;
			direct = SUCCEEDED(g_r.dev->CreateShaderResourceView(d.tex, &sv, &d.liveSrv)) && d.liveSrv;
		}
		bool copied = false;
		if (!direct) {
			copied = MakeCopy(d.td, &d.presentCopy, &d.presentCopySrv);
		}
		Log("depth: using #%d %s %ux%u (bind flags 0x%x) -> %s", d.id, FmtName(d.td.Format), d.td.Width, d.td.Height, d.td.BindFlags,
			direct ? "read directly" : copied ? "read through a copy made at Present" : "NOT READABLE");
		if (g_cfg.depthSource == 2) {
			g_r.useClearCopy = true;
		}
	}

	void EnsureClearCopy()
	{
		DepthSource& d = g_r.depth;
		if (!d.tex || d.clearCopy) {
			return;
		}
		if (!MakeCopy(d.td, &d.clearCopy, &d.clearCopySrv)) {
			Log("depth: could not create the pre-clear copy");
			return;
		}
		AcquireSRWLockExclusive(&g_dsvLock);
		g_cc.dsv = d.dsv;
		g_cc.src = d.tex;
		g_cc.dst = d.clearCopy;
		g_cc.enabled = true;
		ReleaseSRWLockExclusive(&g_dsvLock);
		Log("depth: pre-clear copy armed for #%d", d.id);
	}

	ID3D11ShaderResourceView* CurrentDepthSrv(bool* fromClearCopy)
	{
		DepthSource& d = g_r.depth;
		*fromClearCopy = false;
		if (!d.tex) {
			return nullptr;
		}
		if (g_r.useClearCopy) {
			EnsureClearCopy();
			if (d.clearCopySrv && g_ccCount.load() > 0 && g_frame.load() - g_ccFrame.load() <= 2) {
				*fromClearCopy = true;
				return d.clearCopySrv;
			}
		}
		if (d.liveSrv) {
			return d.liveSrv;
		}
		if (d.presentCopy) {
			g_r.ctx->CopyResource(d.presentCopy, d.tex);
			return d.presentCopySrv;
		}
		return nullptr;
	}

	struct Cand
	{
		DsvEntry e;
		uint32_t score;
	};

	void LogCandidates(const Cand* all, int n, const char* why)
	{
		Log("depth buffers seen (%s): %d. Busiest first, per frame: immediate binds + deferred binds, clears", why, n);
		for (int i = 0; i < n && i < 14; ++i) {
			const DsvEntry& e = all[i].e;
			char            cl[48] = "";
			if (e.cleared) {
				snprintf(cl, sizeof(cl), " (clears to %.3f%s)", e.clearDepth, e.clearImmediate ? "" : ", deferred");
			}
			Log("  #%-2d %-22s %5ux%-5u arr %u mips %u msaa %u bind 0x%03x view %d | %u + %u binds, %u clears%s | total %llu", e.id,
				e.tex2D ? FmtName(e.td.Format) : "(not a 2D texture)", e.td.Width, e.td.Height, e.td.ArraySize, e.td.MipLevels, e.td.SampleDesc.Count,
				e.td.BindFlags, e.viewDim, e.bindsLast, e.defLast, e.clearsLast, cl, static_cast<unsigned long long>(e.total));
		}
	}

	void PickDepth(UINT bbW, UINT bbH)
	{
		static Cand all[kMaxDsvs];
		int         n = 0;
		AcquireSRWLockExclusive(&g_dsvLock);
		for (int i = 0; i < g_dsvCount; ++i) {
			DsvEntry& e = g_dsvs[i];
			e.bindsLast = e.bindsCur;
			e.defLast = e.defCur;
			e.clearsLast = e.clearsCur;
			e.bindsCur = e.defCur = e.clearsCur = 0;
			all[n].e = e;
			all[n].score = e.bindsLast + e.defLast;
			++n;
		}
		ReleaseSRWLockExclusive(&g_dsvLock);
		std::sort(all, all + n, [](const Cand& a, const Cand& b) { return a.score != b.score ? a.score > b.score : a.e.total > b.e.total; });

		const double aspect = double(bbW) / double(bbH);
		Cand         usable[kMaxDsvs];
		int          u = 0;
		const Cand*  msaa = nullptr;
		for (int i = 0; i < n; ++i) {
			const DsvEntry& e = all[i].e;
			if (!e.tex2D || e.td.ArraySize != 1 || all[i].score == 0 || TypelessOf(e.td.Format) == DXGI_FORMAT_UNKNOWN) {
				continue;
			}
			const double a = double(e.td.Width) / double(std::max(1u, e.td.Height));
			if (fabs(a - aspect) > aspect * 0.03 || e.td.Width < bbW / 4) {
				continue;
			}
			if (e.td.SampleDesc.Count > 1) {
				if (!msaa) {
					msaa = &all[i];
				}
				continue;
			}
			usable[u++] = all[i];
		}
		if (msaa && (u == 0 || msaa->score > usable[0].score) && !g_r.msaaWarned) {
			g_r.msaaWarned = true;
			Log("depth: GTA's main depth buffer (#%d) is multisampled (MSAA x%u). Turn MSAA off in GTA's graphics settings.", msaa->e.id,
				msaa->e.td.SampleDesc.Count);
			Notify("D1: MSAA is on. Turn MSAA off in GTA graphics settings.");
		}

		const ULONGLONG now = GetTickCount64();
		const int       topId = u > 0 ? usable[0].e.id : 0;
		if (now - g_r.lastCandLogMs > 60000 || (topId != g_r.lastTopId && now - g_r.lastCandLogMs > 2000)) {
			g_r.lastCandLogMs = now;
			g_r.lastTopId = topId;
			LogCandidates(all, n, topId ? "periodic" : "no usable one");
		}
		if (u == 0) {
			return;
		}
		int cur = -1;
		for (int i = 0; i < u; ++i) {
			if (usable[i].e.dsv == g_r.depth.dsv) {
				cur = i;
			}
		}
		int target = -1;
		if (!g_r.depth.tex) {
			target = 0;
		} else if (cur < 0) {
			if (++g_r.missingFrames >= 30) {
				target = 0;
			}
		} else {
			g_r.missingFrames = 0;
			if (cur != 0 && usable[0].score > usable[cur].score + usable[cur].score / 4) {
				if (++g_r.challengerFrames >= 60) {
					target = 0;
				}
			} else {
				g_r.challengerFrames = 0;
			}
		}
		if (target >= 0 && usable[target].e.dsv != g_r.depth.dsv) {
			const DsvEntry& want = usable[target].e;
			g_r.missingFrames = g_r.challengerFrames = 0;
			UseDepth(want);
			SetStatus("depth #%d %s %ux%u", want.id, FmtName(want.td.Format), want.td.Width, want.td.Height);
		}
	}

	void RunProbe(ID3D11ShaderResourceView* srv, const CamSample& cam, bool fromClearCopy)
	{
		Renderer&      r = g_r;
		D3D11_VIEWPORT vp{ 0, 0, 17, 1, 0, 1 };
		const float    zero[4] = {};
		r.ctx->OMSetRenderTargets(1, &r.probeRtv, nullptr);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->RSSetState(r.raster);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psProbe, nullptr, 0);
		r.ctx->PSSetShaderResources(0, 1, &srv);
		r.ctx->PSSetSamplers(0, 1, &r.point);
		r.ctx->Draw(3, 0);
		r.ctx->CopyResource(r.probeStaging, r.probeTex);
		r.probe.pending = true;
		r.probe.frame = g_frame.load();
		r.probe.cam = cam;
		r.probe.fromClearCopy = fromClearCopy;
	}

	double LinearZ(double d, double n, double f, bool reversed)
	{
		return reversed ? n * f / (n + d * (f - n)) : n * f / (f - d * (f - n));
	}

	bool LatestCam(CamSample& out, uint32_t back)
	{
		AcquireSRWLockShared(&g_shared);
		const uint32_t seq = g_camSeq;
		const bool     ok = seq > back;
		if (ok) {
			out = g_camRing[(seq - back) % kCamRing];
		}
		ReleaseSRWLockShared(&g_shared);
		return ok && GetTickCount64() - out.ms < 500;
	}

	// ---------------------------------------------------------------------------------------
	// v1.9.5: which script sample goes with which drawn frame. GTA runs our script once per game frame
	// (game thread) while frames are drawn on the render thread. Taking "the latest sample" when drawing
	// got this frame's sample sometimes and the one before at other times (v1.9.4 log: up to 41 repeated
	// + 41 skipped samples in 10 s, more now that the car is drawn mid-frame), so the car stepped
	// unevenly against GTA's smoothly moving world. Now each frame takes the sample at a fixed offset
	// from its frame number: the smallest offset seen in the last ~128 frames, so the sample is always
	// there and the car advances exactly one sample per frame.
	// ---------------------------------------------------------------------------------------
	struct Pairing
	{
		int64_t  offs[128] = {};
		int      n = 0, pos = 0;
		uint64_t lastFrame = 0;
		int64_t  k = 0;
	} g_pair;

	bool FrameCam(CamSample& out, uint64_t frameIndex)
	{
		if (!g_cfg.framePairing) {
			return LatestCam(out, uint32_t(g_cfg.frameDelay));
		}
		AcquireSRWLockShared(&g_shared);
		const uint32_t seq = g_camSeq;
		Pairing&       p = g_pair;
		if (frameIndex != p.lastFrame) {
			p.lastFrame = frameIndex;
			const int64_t off = int64_t(seq) - int64_t(frameIndex);
			if (p.n > 0 && off - p.k > 2) p.n = 0;  // samples ran ahead (frames lost): start the window again
			p.offs[p.pos] = off;
			p.pos = (p.pos + 1) % 128;
			p.n = std::min(p.n + 1, 128);
			int64_t k = p.offs[(p.pos + 127) % 128];
			for (int i = 0; i < p.n; ++i) k = std::min(k, p.offs[i]);
			p.k = k;
		}
		int64_t want = int64_t(frameIndex) + p.k - g_cfg.frameDelay;
		want = std::min<int64_t>(want, seq);
		want = std::max<int64_t>(want, int64_t(seq) - 2 - g_cfg.frameDelay);  // never more than 2 samples behind (ticks faster than frames)
		const bool ok = seq > 0 && want > 0;
		if (ok) {
			out = g_camRing[uint32_t(want) % kCamRing];
		}
		ReleaseSRWLockShared(&g_shared);
		return ok && GetTickCount64() - out.ms < 500;
	}

	void ReadProbe()
	{
		Renderer& r = g_r;
		if (!r.probe.pending) {
			return;
		}
		D3D11_MAPPED_SUBRESOURCE m{};
		if (r.ctx->Map(r.probeStaging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) {
			if (g_frame.load() - r.probe.frame > 120) {
				r.probe.pending = false;
			}
			return;
		}
		float v[17];
		memcpy(v, m.pData, sizeof(v));
		r.ctx->Unmap(r.probeStaging, 0);
		r.probe.pending = false;

		float s[16];
		memcpy(s, v, sizeof(s));
		std::sort(s, s + 16);
		const float median = s[8];
		const bool  flat = s[15] - s[0] < 1e-7f;
		int&        flatCount = r.probe.fromClearCopy ? r.flatCopy : r.flatLive;
		flatCount = flat ? flatCount + 1 : 0;
		if (!flat) {
			const bool      looksReversed = median < 0.5f;
			const ULONGLONG nowMs = GetTickCount64();
			if (g_cfg.depthReversed >= 0) {
				if (!r.depthKnown) {
					r.reversed = g_cfg.depthReversed == 1;
					r.depthKnown = true;
					Log("depth convention set by the ini: %s", r.reversed ? "reversed Z" : "standard Z");
				}
			} else if (!r.depthKnown) {
				r.convVotes = (r.convVotes > 0 && looksReversed == r.convVote) ? r.convVotes + 1 : 1;
				r.convVote = looksReversed;
				r.reversed = looksReversed;
				if (r.convVotes >= 3) {
					r.depthKnown = true;
					Log("depth probe (%s): min %.6f median %.6f max %.6f -> %s (settled)", r.probe.fromClearCopy ? "pre-clear copy" : "live", s[0], median,
						s[15], r.reversed ? "reversed Z (1 near, 0 far)" : "standard Z (0 near, 1 far)");
				}
			} else if (looksReversed != r.reversed && nowMs - r.lastDisagreeLogMs > 10000) {
				r.lastDisagreeLogMs = nowMs;
				Log("depth probe: median %.6f looks like %s, keeping %s (something close to the camera?)", median, looksReversed ? "reversed Z" : "standard Z",
					r.reversed ? "reversed Z" : "standard Z");
			}
		}
		if (flatCount == 3) {
			Log("depth probe (%s): all 16 samples equal (%.6f) three times in a row: the depth is cleared or unused at Present",
				r.probe.fromClearCopy ? "pre-clear copy" : "live", s[0]);
			if (!r.probe.fromClearCopy && g_cfg.depthSource == 0 && !r.useClearCopy) {
				r.useClearCopy = true;
				Log("depth: switching to the copy made just before GTA clears it");
			}
		}

		CamSample now{};
		if (!LatestCam(now, 0) || flat) {
			return;
		}
		const CamSample& c = r.probe.cam;
		const double     moved = fabs(now.px - c.px) + fabs(now.py - c.py) + fabs(now.pz - c.pz);
		double           turned = fabs(now.pitch - c.pitch) + fabs(now.yaw - c.yaw);
		turned = std::min(turned, fabs(turned - 360.0));
		RayCheck ray{};
		AcquireSRWLockShared(&g_shared);
		ray = g_ray;
		ReleaseSRWLockShared(&g_shared);
		const ULONGLONG nowMs = GetTickCount64();
		if (moved > 0.05 || turned > 0.3 || !ray.valid || nowMs - ray.ms > 400 || ray.viewZ > 300.0) {
			return;
		}
		const double depthZ = LinearZ(v[16], double(c.nearClip) * g_cfg.nearScale, c.farClip, r.reversed);
		const double ratio = depthZ / ray.viewZ;
		++r.checks;
		const bool good = fabs(ratio - 1.0) < 0.05 || fabs(depthZ - ray.viewZ) < 0.25;
		if (good) {
			++r.checksGood;
		}
		snprintf(r.lastCheck, sizeof(r.lastCheck), "centre %.1f m vs ray %.1f m", depthZ, ray.viewZ);
		if (nowMs - r.lastCheckLogMs > 10000) {
			r.lastCheckLogMs = nowMs;
			Log("centre check: depth %.2f m, raycast %.2f m (%s) | %d of %d good", depthZ, ray.viewZ, good ? "good" : "OFF", r.checksGood, r.checks);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Drawing
	// ---------------------------------------------------------------------------------------

	void SetMapBasis(FrameCB& fc, const double L[3], float enabled, float texel)
	{
		double e1[3], e2[3];
		DirBasis(L, e1, e2);
		for (int k = 0; k < 3; ++k) {
			fc.sl0[k] = float(L[k]);
			fc.sl1[k] = float(e1[k]);
			fc.sl2[k] = float(e2[k]);
			fc.sl3[k] = float(g_r.car.centre[k]);
		}
		fc.sl0[3] = enabled;
		fc.sl1[3] = texel;
		fc.sl2[3] = float(g_r.car.radius);
	}

	// v1.9.6: inside GTA's frame, sun and light from the surroundings from the light measurement (auto)
	float HdrSunNow() { return (g_cfg.hdrSunAuto && g_r.lightSamples >= 3) ? float(g_r.autoSun) : g_cfg.hdrSunToSky; }
	float HdrAmbientNow() { return (g_cfg.hdrAmbAuto && g_r.lightSamples >= 3) ? float(g_r.autoAmb) : g_cfg.hdrAmbient; }

	int    DpLayout(int k);  // v1.9.7 (below)
	double DpScaleNow();
	bool   TexDescOf(ID3D11Resource* res, D3D11_TEXTURE2D_DESC& td);
	void FillFrameCB(FrameCB& fc, const CamSample& cam, UINT w, UINT h, bool occlusion, bool hdr = false)
	{
		double r[3], f[3], u[3];
		CamBasis(cam.pitch, cam.roll * g_cfg.rollSign, cam.yaw, r, f, u);
		for (int k = 0; k < 3; ++k) {
			fc.camRight[k] = float(r[k]);
			fc.camUp[k] = float(u[k]);
			fc.camFwd[k] = float(f[k]);
		}
		const double aspect = double(w) / double(h);
		double       vfov = std::clamp(double(cam.fov), 1.0, 170.0) * kDeg;
		if (g_cfg.fovHorizontal) {
			vfov = 2.0 * atan(tan(vfov * 0.5) / aspect);
		}
		const double ys = 1.0 / tan(vfov * 0.5);
		fc.proj[0] = float(ys / aspect);
		fc.proj[1] = float(ys);
		fc.proj[2] = 0.05f;
		fc.proj[3] = std::max(cam.farClip, 2000.0f);
		fc.depthP[0] = std::max(cam.nearClip * g_cfg.nearScale, 0.001f);
		fc.depthP[1] = std::max(cam.farClip, fc.depthP[0] + 1.0f);
		fc.depthP[2] = g_r.reversed ? 1.0f : 0.0f;
		fc.depthP[3] = occlusion ? 1.0f : 0.0f;
		fc.screen[0] = 1.0f / float(w);
		fc.screen[1] = 1.0f / float(h);
		fc.screen[2] = g_cfg.tolRel;
		fc.screen[3] = g_cfg.tolAbs;

		const Sun    sun = SunModel(cam.hour);
		const double* S = sun.S;
		const double dayF = sun.dayF;
		for (int k = 0; k < 3; ++k) {
			fc.sunDir[k] = float(S[k]);
		}
		fc.sunDir[3] = float(dayF);
		// sun colour: warm when low, white when high; moonlight at night; blocked by GTA's world
		const double hi = std::clamp(sin(sun.elev) * 2.0, 0.0, 1.0);
		const double sunR = 1.0, sunG = 0.62 + 0.34 * hi, sunB = 0.38 + 0.52 * hi;
		const double sunK = 0.9 * dayF * g_cfg.sunStrength * std::clamp(double(cam.sunVis), 0.0, 1.0);
		fc.sunCol[0] = float(sunR * sunK + 0.05 * (1.0 - dayF));
		fc.sunCol[1] = float(sunG * sunK + 0.06 * (1.0 - dayF));
		fc.sunCol[2] = float(sunB * sunK + 0.09 * (1.0 - dayF));
		// made-up sky colours (linear): environment start / Environment=sky
		auto mix = [&](double d, double n) { return float(n + (d - n) * dayF); };
		const double dusk = (1.0 - hi) * dayF;  // orange horizon at sunrise/sunset
		fc.skyTop[0] = mix(0.38, 0.015), fc.skyTop[1] = mix(0.50, 0.02), fc.skyTop[2] = mix(0.72, 0.04);
		fc.skyHorizon[0] = mix(0.80 + 0.15 * dusk, 0.03), fc.skyHorizon[1] = mix(0.80 - 0.15 * dusk, 0.035), fc.skyHorizon[2] = mix(0.82 - 0.35 * dusk, 0.05);
		fc.skyGround[0] = mix(0.22, 0.012), fc.skyGround[1] = mix(0.20, 0.012), fc.skyGround[2] = mix(0.17, 0.014);
		fc.skyGround[3] = g_cfg.reflections;

		fc.carPos[0] = float(cam.ox - cam.px);  // camera-relative keeps float precision far from the map centre
		fc.carPos[1] = float(cam.oy - cam.py);
		fc.carPos[2] = float(cam.oz - cam.pz);
		for (int k = 0; k < 3; ++k) {
			fc.carRight[k] = cam.R[k];
			fc.carFwd[k] = cam.F[k];
			fc.carUp[k] = cam.U[k];
		}
		fc.look[0] = hdr ? HdrAmbientNow() : g_cfg.ambient;
		fc.look[1] = 0.0f;
		fc.look[2] = g_cfg.exposure;
		fc.look[3] = hdr ? 0.0f : 1.0f;  // at Present we encode to sRGB ourselves; inside GTA's frame the output is linear HDR
		const GpuCar& car = g_r.car;
		memcpy(fc.paint0, car.paint[0], sizeof(fc.paint0));
		memcpy(fc.paint1, car.paint[1], sizeof(fc.paint1));
		memcpy(fc.paint2, car.paint[2], sizeof(fc.paint2));
		{
			// v1.9.6: ini number > the paint's own values from BeamNG > defaults
			float* dst[3] = { fc.paintS0, fc.paintS1, fc.paintS2 };
			for (int i = 0; i < 3; ++i)
				for (int k = 0; k < 4; ++k)
					dst[i][k] = g_cfg.paintSurf[k] >= 0 ? g_cfg.paintSurf[k] : car.hasPaintPbr ? std::clamp(car.paintPbr[i][k], 0.0f, 1.0f) : kPaintDefault[k];
		}
		{
			// v1.9.7: GTA's reflection map (only inside its frame: it is in GTA's HDR units)
			float* dst[2] = { fc.dpU, fc.dpL };
			bool   use[2] = {};
			for (int k = 0; k < 2; ++k) {
				const int c = DpLayout(k);
				// the ground half only once the sky half (and so the brightness) is known
				use[k] = hdr && g_r.dpUsable && g_cfg.gtaRefl && c >= 0 && (k == 0 || DpLayout(0) >= 0 || g_cfg.gtaReflScale >= 0);
				const int cc = std::max(c, 0);
				dst[k][0] = float(((cc >> 3) & 1) + 2 * ((cc >> 4) & 1));
				dst[k][1] = float((cc >> 2) & 1);
				dst[k][2] = ((cc >> 1) & 1) ? -1.0f : 1.0f;
				dst[k][3] = (cc & 1) ? -1.0f : 1.0f;
			}
			fc.dpP[0] = use[0] ? 1.0f : 0.0f;
			fc.dpP[1] = float(g_r.dpMips > 0 ? g_r.dpMips - 1 : 0);
			fc.dpP[2] = float(DpScaleNow());
			fc.dpP[3] = use[1] ? 1.0f : 0.0f;
		}

		// ground shadows: map sizes, strengths, and the shadow direction (from the sun downwards) in car
		// space, kept at least 17 degrees steep so the shadow stays inside its map
		const float carHalf = std::max(car.size[0], car.size[1]) * 0.5f;
		fc.shadowA[0] = carHalf + 0.6f;
		fc.shadowA[1] = carHalf + 5.5f;
		fc.shadowA[2] = g_cfg.shadowMode == 2 ? 0.0f : g_cfg.contactStrength * float(0.5 + 0.5 * dayF);
		fc.shadowA[3] = g_cfg.shadowMode == 1 ? g_cfg.sunShadowStrength * float(dayF) * std::clamp(cam.sunVis, 0.0f, 1.0f) : 0.0f;
		const double Lc[3] = { S[0] * cam.R[0] + S[1] * cam.R[1] + S[2] * cam.R[2], S[0] * cam.F[0] + S[1] * cam.F[1] + S[2] * cam.F[2],
			                   S[0] * cam.U[0] + S[1] * cam.U[1] + S[2] * cam.U[2] };
		double       D[3] = { -Lc[0], -Lc[1], -Lc[2] };
		const double minDown = sin(17.0 * kDeg);
		if (D[2] > -minDown) {
			const double hl = sqrt(D[0] * D[0] + D[1] * D[1]);
			const double hk = hl > 1e-6 ? cos(17.0 * kDeg) / hl : 0.0;
			D[0] *= hk;
			D[1] *= hk;
			D[2] = -minDown;
		}
		fc.shadowB[0] = float(D[0]);
		fc.shadowB[1] = float(D[1]);
		fc.shadowB[2] = float(D[2]);
		fc.shadowB[3] = g_cfg.shadowSoftness;

		// the car's own sun shadow map (direction to the sun in car space)
		const bool selfShadow = g_cfg.selfShadows && dayF > 0.01 && Lc[2] > 0.02 && g_r.sunDepth.srv;
		SetMapBasis(fc, Lc, selfShadow ? 1.0f : 0.0f, 1.0f / float(std::max(1u, g_r.sunDepth.size)));
		fc.sl3[3] = (g_cfg.ao && g_r.aoMaps.srv && !g_r.aoDirty) ? 1.0f : 0.0f;

		// environment, maps, debug
		const bool world = (g_cfg.environment == 1 || hdr) && g_r.llReady;
		fc.envP[0] = world ? 1.0f : 0.0f;
		fc.envP[1] = g_cfg.envBrightness;
		// mip of GTA's picture that matches one texel of the learned map (the HDR copy is half size)
		const double pxPerTexel = (2.0 * kPi / kLLW) * h * 0.5 * ys * (g_r.envSrcHalf ? 0.5 : 1.0);
		fc.envP[2] = float(std::clamp(log2(std::max(1.0, pxPerTexel)) - 0.5, 0.0, double(std::max(1u, g_r.envSrcMips) - 1)));
		fc.envP[3] = g_r.envSrcLinear ? 1.0f : 0.0f;
		fc.feat[0] = g_cfg.normalMaps ? 1.0f : 0.0f;
		fc.feat[1] = g_cfg.normalStrength;
		fc.feat[2] = g_cfg.normalGreen;
		fc.feat[3] = g_cfg.surfaceMaps ? 1.0f : 0.0f;
		fc.dbg[0] = float(g_cfg.debugView);
		fc.misc[0] = g_cfg.glassReflections;

		// minimap and composite
		fc.hud[0] = cam.hud[0];
		fc.hud[1] = cam.hud[1];
		fc.hud[2] = cam.hud[2];
		fc.hud[3] = cam.hud[3];
		fc.hud2[0] = (g_cfg.hudMask && cam.hudOn && !hdr) ? 1.0f : 0.0f;  // inside GTA's frame the HUD comes after us anyway
		fc.hud2[1] = g_cfg.edgeSmoothing ? 1.0f : 0.0f;
		fc.hud2[2] = (!hdr && IsSrgb(g_r.bbFmt)) ? 1.0f : 0.0f;
		fc.hud2[3] = g_cfg.hudMargin;

		// camera inside the car? (glass then reflects the dark cabin)
		const double rel[3] = { cam.px - cam.ox, cam.py - cam.oy, cam.pz - cam.oz };
		const double cx = rel[0] * cam.R[0] + rel[1] * cam.R[1] + rel[2] * cam.R[2];
		const double cyy = rel[0] * cam.F[0] + rel[1] * cam.F[1] + rel[2] * cam.F[2];
		const double cz = rel[0] * cam.U[0] + rel[1] * cam.U[1] + rel[2] * cam.U[2];
		const bool   inside = fabs(cx) < car.size[0] * 0.5 - 0.05 && fabs(cyy) < car.size[1] * 0.5 - 0.1 && cz > 0.25 && cz < car.size[2] - 0.03;
		fc.lit[0] = g_cfg.specStrength;
		fc.lit[1] = inside ? 1.0f : 0.0f;
		fc.lit[2] = 0.0f;
		fc.lit[3] = hdr ? std::max(0.0f, HdrSunNow()) : 0.0f;
	}

	void FillMatCB(MatCB& mc, const car::Material& m, int index, bool hasPaint, ID3D11ShaderResourceView* srvs[24])
	{
		const GpuCar& c = g_r.car;
		auto          tex = [&](int i) -> ID3D11ShaderResourceView* { return (i >= 0 && i < int(c.tex.size())) ? c.tex[i] : nullptr; };
		auto          swz = [&](int i) -> float { return (i >= 0 && i < int(c.texSwz.size())) ? float(c.texSwz[i]) : 0.0f; };
		mc.matInfo[0] = float(std::clamp(m.nLayers, 1, 4));
		mc.matInfo[1] = m.alphaCut;
		mc.matInfo[2] = m.translucent ? ((!m.doubleSided && g_cfg.oneSidedGlass) ? 2.0f : 1.0f) : 0.0f;  // 2: see-through, front side only (v1.9.5)
		mc.matInfo[3] = g_cfg.glassMinAlpha;
		mc.matKind[0] = m.mirror ? 1.0f : 0.0f;
		mc.matKind[1] = float(m.blendOp);
		mc.matKind[2] = m.noReflections ? 1.0f : 0.0f;
		mc.matKind[3] = float(index);
		for (int l = 0; l < 4; ++l) {
			const car::Layer& y = m.layers[l];
			LayerCB&          L = mc.lay[l];
			memcpy(L.factor, y.factor, sizeof(L.factor));
			srvs[l] = tex(y.baseTex);
			srvs[4 + l] = tex(y.palTex);
			srvs[8 + l] = y.maskIgnored ? nullptr : tex(y.opTex);
			srvs[12 + l] = tex(y.normTex);
			srvs[16 + l] = tex(y.metalTex);
			srvs[20 + l] = tex(y.roughTex);
			L.a[0] = srvs[l] ? 1.0f : 0.0f;
			L.a[1] = float(y.baseUV);
			L.a[2] = y.palConst >= 0 ? 2.0f : srvs[4 + l] ? 1.0f : 0.0f;
			L.a[3] = float(y.palUV);
			L.b[0] = float(std::max(y.palConst, 0));
			L.b[1] = srvs[8 + l] ? 1.0f : 0.0f;
			L.b[2] = float(y.opUV);
			L.b[3] = y.opFactor;
			// v1.9.6: 2 = paint colour + the paint's surface values (BeamNG materials 1.5), 1 = colour only (older materials)
			L.c[0] = (hasPaint && y.paint) ? (m.version >= 1.5f ? 2.0f : 1.0f) : 0.0f;
			L.c[1] = y.metallic;
			L.c[2] = y.roughness;
			L.c[3] = y.clearCoat;
			L.d[0] = srvs[12 + l] ? 1.0f + float(y.normUV) : 0.0f;
			L.d[1] = srvs[16 + l] ? 1.0f + float(y.metalUV) : 0.0f;
			L.d[2] = srvs[20 + l] ? 1.0f + float(y.roughUV) : 0.0f;
			L.d[3] = y.live ? 1.0f : 0.0f;
			L.e[0] = swz(y.opTex);
			L.e[1] = swz(y.metalTex);
			L.e[2] = swz(y.roughTex);
			L.e[3] = swz(y.normTex);
		}
	}

	// Car-space transform for a wheel part: spin about the axle, steer about the vertical through the
	// hub, move up/down with the suspension. From GTA's own wheels when they could be read, else the
	// speed model.
	void WheelMatrix(PartCB& pc, const car::Wheel& w, int mode, const CamSample& cam, int idx)
	{
		if (cam.bngWheels && idx >= 0 && idx < kDrvWheels && cam.bwOk[idx]) {
			// v2.0 drive mode: BeamNG's hub (centre, axle direction, turn). M = (turn the file's axle onto
			// BeamNG's) * (spin about the file's axle); p' = M (p - c_file) + c_beamng
			double a0[3] = { w.axis[0], w.axis[1], w.axis[2] }, a1[3] = { cam.bwA[idx][0], cam.bwA[idx][1], cam.bwA[idx][2] };
			Norm3(a0);
			Norm3(a1);
			if (Dot3(a0, a1) < 0) a1[0] = -a1[0], a1[1] = -a1[1], a1[2] = -a1[2];
			const double sp = mode == 1 ? double(cam.bwAng[idx]) : 0.0;
			const double cs = cos(sp), sn = sin(sp), t = 1.0 - cs;
			const double ax = a0[0], ay = a0[1], az = a0[2];
			const double A[3][3] = {
				{ t * ax * ax + cs, t * ax * ay - sn * az, t * ax * az + sn * ay },
				{ t * ax * ay + sn * az, t * ay * ay + cs, t * ay * az - sn * ax },
				{ t * ax * az - sn * ay, t * ay * az + sn * ax, t * az * az + cs },
			};
			double v[3];
			Cross3(a0, a1, v);
			const double c = Dot3(a0, a1), s2 = Dot3(v, v);
			double       B[3][3] = { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 } };
			if (s2 > 1e-12) {
				const double K[3][3] = { { 0, -v[2], v[1] }, { v[2], 0, -v[0] }, { -v[1], v[0], 0 } };
				const double kk = (1.0 - c) / s2;
				for (int i = 0; i < 3; ++i)
					for (int j = 0; j < 3; ++j) B[i][j] += K[i][j] + kk * (K[i][0] * K[0][j] + K[i][1] * K[1][j] + K[i][2] * K[2][j]);
			}
			double M[3][3];
			for (int i = 0; i < 3; ++i)
				for (int j = 0; j < 3; ++j) M[i][j] = B[i][0] * A[0][j] + B[i][1] * A[1][j] + B[i][2] * A[2][j];
			const double c0[3] = { w.c[0], w.c[1], w.c[2] };
			for (int i = 0; i < 3; ++i) {
				pc.m[i][0] = float(M[i][0]);
				pc.m[i][1] = float(M[i][1]);
				pc.m[i][2] = float(M[i][2]);
				pc.m[i][3] = float(double(cam.bwC[idx][i]) - (M[i][0] * c0[0] + M[i][1] * c0[1] + M[i][2] * c0[2]));
			}
			return;
		}
		const double r = std::max(0.05f, w.radius);
		const int    g = (w.front ? 0 : 2) + (w.c[0] < 0 ? 0 : 1);
		double       dist, steer, travel = 0.0;
		if (cam.gtaWheels) {
			dist = cam.gDist[g];
			steer = std::clamp(double(cam.gSteer[g]), -0.9, 0.9);
			travel = cam.gTravel[g];
		} else {
			dist = w.front ? cam.distFront : cam.distRear;
			steer = w.front ? double(cam.steerDeg) * kDeg : 0.0;
		}
		const double spin = mode == 1 ? -fmod(dist / r, 2.0 * kPi) : 0.0;
		// R = Rz(steer) * Raxis(spin), applied about the hub centre c, then moved up by the travel
		const double ax = w.axis[0], ay = w.axis[1], az = w.axis[2];
		const double cs = cos(spin), sn = sin(spin), t = 1.0 - cs;
		const double A[3][3] = {
			{ t * ax * ax + cs, t * ax * ay - sn * az, t * ax * az + sn * ay },
			{ t * ax * ay + sn * az, t * ay * ay + cs, t * ay * az - sn * ax },
			{ t * ax * az - sn * ay, t * ay * az + sn * ax, t * az * az + cs },
		};
		const double cz = cos(steer), sz = sin(steer);
		const double Z[3][3] = { { cz, -sz, 0 }, { sz, cz, 0 }, { 0, 0, 1 } };
		double       M[3][3];
		for (int i = 0; i < 3; ++i)
			for (int j = 0; j < 3; ++j) M[i][j] = Z[i][0] * A[0][j] + Z[i][1] * A[1][j] + Z[i][2] * A[2][j];
		const double c[3] = { w.c[0], w.c[1], w.c[2] };
		for (int i = 0; i < 3; ++i) {
			pc.m[i][0] = float(M[i][0]);
			pc.m[i][1] = float(M[i][1]);
			pc.m[i][2] = float(M[i][2]);
			pc.m[i][3] = float(c[i] - (M[i][0] * c[0] + M[i][1] * c[1] + M[i][2] * c[2]) + (i == 2 ? travel : 0.0));
		}
	}

	template <class T>
	bool Upload(ID3D11Buffer* b, const T& v)
	{
		D3D11_MAPPED_SUBRESOURCE m{};
		if (FAILED(g_r.ctx->Map(b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
			return false;
		}
		memcpy(m.pData, &v, sizeof(v));
		g_r.ctx->Unmap(b, 0);
		return true;
	}

	void BindCommon(ID3D11ShaderResourceView* depthSrv)
	{
		Renderer&     r = g_r;
		ID3D11Buffer* cbs[5] = { r.cb, r.matCb, r.partCb, r.occlCb, r.holeCb };
		r.ctx->VSSetConstantBuffers(0, 4, cbs);
		r.ctx->PSSetConstantBuffers(0, 5, cbs);
		r.ctx->PSSetShaderResources(0, 1, &depthSrv);
		ID3D11SamplerState* ss[kSamplers] = { r.point, r.linear, r.clampLin, r.cmpSamp, r.llSamp };
		r.ctx->PSSetSamplers(0, kSamplers, ss);
		r.ctx->GSSetShader(nullptr, nullptr, 0);
		r.ctx->HSSetShader(nullptr, nullptr, 0);
		r.ctx->DSSetShader(nullptr, nullptr, 0);
	}

	void UnbindPsSrvs(UINT from = 1)
	{
		ID3D11ShaderResourceView* none[kPsSlots] = {};
		g_r.ctx->PSSetShaderResources(from, kPsSlots - from, none);
	}

	ID3D11BlendState* GlassBlend(int op)
	{
		return (op == car::kBlendAdd || op == car::kBlendAddAlpha) ? g_r.layerAdd : g_r.layerPremul;
	}

	enum class Pass
	{
		ShadowMap,  // opaque parts only, no materials
		Main,       // everything; see-through parts last with their blend
		Pick,       // everything with depth writes, no blending (front-most part wins)
	};

	// Draws the car's parts with the current shaders.
	void DrawParts(const CamSample& cam, Pass pass, bool moving)
	{
		Renderer& r = g_r;
		GpuCar&   c = r.car;
		PartCB    ident{};
		ident.m[0][0] = ident.m[1][1] = ident.m[2][2] = 1.0f;
		Upload(r.partCb, ident);
		const float  zero[4] = {};
		const bool   withMaterials = pass != Pass::ShadowMap;
		int          lastMat = -2, lastPart = -1;  // part: -1 body, else wheel*4 + mode
		bool         lastBroken = false, lastWheel = false;
		const size_t n = pass == Pass::ShadowMap ? size_t(c.opaqueDraws) : c.draws.size();
		for (size_t i = 0; i < n; ++i) {
			const car::Draw& d = c.draws[i];
			const bool       see = int(i) >= c.opaqueDraws;
			if (pass == Pass::Main && int(i) == c.opaqueDraws) {
				r.ctx->OMSetDepthStencilState(r.depthRead, 0);  // see-through parts last, over the opaque car
				lastMat = -2;
			}
			if (!withMaterials && d.mat >= 0 && d.mat < int(c.mats.size()) && c.mats[d.mat].translucent) {
				continue;
			}
			// v2.6: glass and lights BeamNG broke: gone (its broken material is invisible) or frosted
			const bool broken = d.dg >= 0 && d.dg < 256 && (g_dgBroken[d.dg >> 6].load(std::memory_order_relaxed) >> (d.dg & 63) & 1);
			if (broken && d.dg < int(c.dgHide.size()) && c.dgHide[d.dg]) continue;
			if (broken && (g_dgShatter[d.dg >> 6].load(std::memory_order_relaxed) >> (d.dg & 63) & 1)) continue;  // v2.63: shattered out
			// v2.73: a front door turns about its hinge as far as GTA's door is open (DoorTick)
			const float doorA = d.door >= 0 ? g_doorAng[d.door & 1].load(std::memory_order_relaxed) : 0.0f;
			const int   part = doorA > 0.005f ? 1000 + d.door
			                   : (moving && g_cfg.wheels && d.wheel >= 0 && d.wheel < int(c.wheels.size())) ? d.wheel * 4 + d.wheelMode : -1;
			if (part != lastPart) {
				lastPart = part;
				if (part < 0) {
					Upload(r.partCb, ident);
				} else if (part >= 1000) {
					// about the vertical through the hinge: the left door's back swings out to the left (-x), the right one's to the right
					const double a = d.door == 0 ? -doorA : doorA, cs = cos(a), sn = sin(a), hx = c.doorHinge[d.door & 1][0], hy = c.doorHinge[d.door & 1][1];
					PartCB pc{};
					pc.m[0][0] = float(cs), pc.m[0][1] = float(-sn), pc.m[0][3] = float(hx - (cs * hx - sn * hy));
					pc.m[1][0] = float(sn), pc.m[1][1] = float(cs), pc.m[1][3] = float(hy - (sn * hx + cs * hy));
					pc.m[2][2] = 1.0f;
					Upload(r.partCb, pc);
				} else {
					PartCB pc{};
					WheelMatrix(pc, c.wheels[d.wheel], d.wheelMode, cam, d.wheel);
					Upload(r.partCb, pc);
				}
			}
			const bool onWheel = d.wheel >= 0;  // v2.63: no bullet holes on wheels (they turn; your rims looked shot through)
			if (withMaterials && (d.mat != lastMat || broken != lastBroken || onWheel != lastWheel)) {
				lastMat = d.mat;
				lastBroken = broken;
				lastWheel = onWheel;
				const int                 mi = std::clamp(d.mat, 0, int(c.mats.size()) - 1);
				const car::Material&      m = c.mats[mi];
				MatCB                     mc{};
				ID3D11ShaderResourceView* srvs[24] = {};
				FillMatCB(mc, m, mi, c.hasPaint, srvs);
				mc.matFx[0] = broken ? 1.0f : 0.0f;  // v2.61: cracked (the shader draws the cracks)
				mc.matFx[1] = onWheel ? 1.0f : 0.0f;  // v2.63: a wheel: no bullet holes
				Upload(r.matCb, mc);
				r.ctx->PSSetShaderResources(1, 12, srvs);
				r.ctx->PSSetShaderResources(15, 12, srvs + 12);
				if (pass == Pass::Main && see) {
					r.ctx->OMSetBlendState(GlassBlend(m.blendOp), zero, 0xFFFFFFFF);
				}
			}
			r.ctx->DrawIndexed(d.count, d.start, 0);
		}
	}

	// The car flattened onto its ground plane into a shadow map (car space), then blurred via mips.
	void RenderShadowMap(const FrameCB& fc, ID3D11RenderTargetView* rtv, ID3D11ShaderResourceView* srv, UINT size, bool sun, const CamSample& cam)
	{
		Renderer&     r = g_r;
		FrameCB       f2 = fc;
		f2.shadowC[0] = sun ? 1.0f : 0.0f;
		f2.shadowC[1] = sun ? fc.shadowA[1] : fc.shadowA[0];
		Upload(r.cb, f2);
		ID3D11ShaderResourceView* none[2] = {};
		r.ctx->PSSetShaderResources(13, 2, none);
		const float    clear[4] = {};
		D3D11_VIEWPORT vp{ 0, 0, float(size), float(size), 0, 1 };
		r.ctx->OMSetRenderTargets(1, &rtv, nullptr);
		r.ctx->ClearRenderTargetView(rtv, clear);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->VSSetShader(r.vsShadowMap, nullptr, 0);
		r.ctx->PSSetShader(r.psShadowMap, nullptr, 0);
		DrawParts(cam, Pass::ShadowMap, sun);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->GenerateMips(srv);
	}

	// The car's depth seen from far along direction L (car space) into one depth map.
	void RenderDepthDir(const FrameCB& fc, const double L[3], ID3D11DepthStencilView* dsv, UINT size, bool moving, const CamSample& cam)
	{
		Renderer& r = g_r;
		FrameCB   f2 = fc;
		SetMapBasis(f2, L, 1.0f, 1.0f / float(size));
		Upload(r.cb, f2);
		D3D11_VIEWPORT vp{ 0, 0, float(size), float(size), 0, 1 };
		r.ctx->OMSetRenderTargets(0, nullptr, dsv);
		r.ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.rsDepth);
		r.ctx->OMSetDepthStencilState(r.depthWrite, 0);
		r.ctx->VSSetShader(r.vsDepthDir, nullptr, 0);
		r.ctx->PSSetShader(r.psShadowMap, nullptr, 0);  // (v2.54: drops torn triangles; no colour target bound)
		DrawParts(cam, Pass::ShadowMap, moving);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->RSSetState(r.raster);
	}

	// Ambient occlusion: the car seen from 12 sky directions, once per car.
	void RenderAO(const FrameCB& fc, const CamSample& cam)
	{
		Renderer& r = g_r;
		if (!EnsureDepthMaps(r.aoMaps, kAoSize, kAoDirs, "ambient occlusion")) {
			r.aoDirty = false;
			return;
		}
		OcclCB          oc{};
		const ULONGLONG t0 = GetTickCount64();
		for (int k = 0; k < kAoDirs; ++k) {
			// zenith, 5 at 55 degrees, 6 at 15 degrees above the car's horizon; v1.9.5: 6 at 25 degrees below it
			double el, az;
			if (k == 0) {
				el = 90.0;
				az = 0.0;
			} else if (k < 6) {
				el = 55.0;
				az = 36.0 + 72.0 * (k - 1);
			} else if (k < 12) {
				el = 15.0;
				az = 60.0 * (k - 6);
			} else {
				el = -25.0;
				az = 30.0 + 60.0 * (k - 12);
			}
			const double L[3] = { cos(el * kDeg) * cos(az * kDeg), cos(el * kDeg) * sin(az * kDeg), sin(el * kDeg) };
			double       e1[3], e2[3];
			DirBasis(L, e1, e2);
			for (int j = 0; j < 3; ++j) {
				oc.aoDir[k][j] = float(L[j]);
				oc.aoE1[k][j] = float(e1[j]);
				oc.aoE2[k][j] = float(e2[j]);
			}
			RenderDepthDir(fc, L, r.aoMaps.dsv[k], kAoSize, false, cam);
		}
		const double rc = r.car.radius;
		oc.aoInfo[0] = float(0.03 / (2.0 * rc));  // depth bias: 3 cm
		oc.aoInfo[1] = 0.04f;                      // normal offset: 4 cm
		oc.aoInfo[2] = g_cfg.aoStrength;
		oc.aoInfo[3] = 1.0f / float(kAoSize);
		Upload(r.occlCb, oc);
		r.aoDirty = false;
		Log("lighting: ambient occlusion baked (%d directions, %ux%u, %llu ms)", kAoDirs, kAoSize, kAoSize, static_cast<unsigned long long>(GetTickCount64() - t0));
	}

	// The learned environment: GTA's frame copied with mips, then one 128x64 pass.
	void UpdateEnvironment(const FrameCB& fc, const CamSample& cam)
	{
		Renderer& r = g_r;
		if (!r.llRtv[0] || !r.llRtv[1] || !r.envSrcSrv) {
			return;
		}
		// relearn after attaching or a jump (teleport, new place)
		const double jump = fabs(cam.ox - r.llCam[0]) + fabs(cam.oy - r.llCam[1]) + fabs(cam.oz - r.llCam[2]);
		const bool attachedNew = g_attachedNew.exchange(false);
		if (attachedNew || jump > 60.0) {
			r.llInit = true;
		}
		if (attachedNew) {
			r.llInitO = true;  // the other stage's set is from before attaching (a jump only restarts the set in use)
		}
		r.llCam[0] = cam.ox;
		r.llCam[1] = cam.oy;
		r.llCam[2] = cam.oz;
		FrameCB f2 = fc;
		f2.lit[2] = r.llInit ? 1.0f : 0.0f;
		Upload(r.cb, f2);
		const int      next = 1 - r.llCur;
		D3D11_VIEWPORT vp{ 0, 0, float(kLLW), float(kLLH), 0, 1 };
		const float    zero[4] = {};
		ID3D11ShaderResourceView* none = nullptr;
		r.ctx->PSSetShaderResources(27, 1, &none);
		r.ctx->OMSetRenderTargets(1, &r.llRtv[next], nullptr);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psEnvUpdate, nullptr, 0);
		ID3D11ShaderResourceView* in[2] = { r.envSrcSrv, r.llSrv[r.llCur] };
		r.ctx->PSSetShaderResources(31, 2, in);
		r.ctx->Draw(3, 0);
		ID3D11ShaderResourceView* out[2] = {};
		r.ctx->PSSetShaderResources(31, 2, out);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->GenerateMips(r.llSrv[next]);
		r.llCur = next;
		if (r.llInit) {
			Log("lighting: learning the surroundings from GTA's frames (fresh start)");
		}
		r.llInit = false;
		r.llReady = true;
		r.ctx->IASetInputLayout(r.layout);
	}

	// Part picker: draw the car again into 1x1 targets whose single pixel is the screen centre.
	void RunPick(const FrameCB& fcIn, UINT w, UINT h, const CamSample& cam)
	{
		Renderer& r = g_r;
		Picker&   p = r.pick;
		if (!p.rtv[3] || !p.staging[3] || !p.dsv) {
			return;
		}
		const UINT cx = w / 2, cy = h / 2;
		FrameCB    fc = fcIn;
		fc.dbg[1] = 1.0f;
		fc.dbg[2] = float(cx);
		fc.dbg[3] = float(cy);
		fc.dbg[0] = 0.0f;
		Upload(r.cb, fc);
		const float zero[4] = {};
		for (int k = 0; k < 4; ++k) {
			r.ctx->ClearRenderTargetView(p.rtv[k], zero);
		}
		r.ctx->ClearDepthStencilView(p.dsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		r.ctx->OMSetRenderTargets(4, p.rtv, p.dsv);
		D3D11_VIEWPORT vp{ -float(cx), -float(cy), float(w), float(h), 0, 1 };
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.depthWrite, 0);
		r.ctx->VSSetShader(r.vsCar, nullptr, 0);
		r.ctx->PSSetShader(r.psPick, nullptr, 0);
		DrawParts(cam, Pass::Pick, true);
		ID3D11RenderTargetView* noRt[4] = {};
		r.ctx->OMSetRenderTargets(4, noRt, nullptr);
		for (int k = 0; k < 4; ++k) {
			r.ctx->CopyResource(p.staging[k], p.rt[k]);
		}
		p.pending = true;
		p.frame = g_frame.load();
	}

	void ReadPick()
	{
		Renderer& r = g_r;
		Picker&   p = r.pick;
		if (!p.pending) {
			return;
		}
		float                    v[4][4] = {};
		D3D11_MAPPED_SUBRESOURCE m{};
		if (r.ctx->Map(p.staging[0], 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) {
			if (g_frame.load() - p.frame > 120) {
				p.pending = false;
			}
			return;
		}
		memcpy(v[0], m.pData, sizeof(v[0]));
		r.ctx->Unmap(p.staging[0], 0);
		for (int k = 1; k < 4; ++k) {
			if (SUCCEEDED(r.ctx->Map(p.staging[k], 0, D3D11_MAP_READ, 0, &m))) {
				memcpy(v[k], m.pData, sizeof(v[k]));
				r.ctx->Unmap(p.staging[k], 0);
			}
		}
		p.pending = false;
		const int mi = int(lroundf(v[0][0])) - 1;
		if (mi == p.lastMat) {
			return;  // a message only when the part under the centre changes
		}
		p.lastMat = mi;
		p.lastNotifyMs = GetTickCount64();
		if (mi < 0 || mi >= int(r.car.mats.size())) {
			Notify("D1 pick: no BeamNG car part at the screen centre");
			return;
		}
		const car::Material& mt = r.car.mats[mi];
		Notify("D1 pick: %s | layer2 shows %.2f | colour %.2f %.2f %.2f | metal %.2f rough %.2f", mt.name.c_str(), v[0][1], v[2][0], v[2][1], v[2][2], v[2][3],
			v[3][0]);
		Log("pick: material %d '%s' (%d layer(s)%s%s) | layer weights L2 %.3f L3 %.3f L4 %.3f | opacity %.3f | colour (linear) %.3f %.3f %.3f | metal %.3f rough %.3f "
		    "coat %.3f | uv1 %.4f %.4f uv2 %.4f %.4f | facing %.2f",
			mi, mt.name.c_str(), mt.nLayers, mt.translucent ? ", see-through " : "", mt.translucent ? BlendName(mt.blendOp) : "", v[0][1], v[0][2], v[3][2], v[0][3],
			v[2][0], v[2][1], v[2][2], v[2][3], v[3][0], v[3][1], v[1][0], v[1][1], v[1][2], v[1][3], v[3][3]);
	}

	// ---- v1.9.5: GTA's light levels (G-buffer colour vs HDR picture), measured every 20 frames inside its frame
	void LightProbe(const FrameCB& fc, ID3D11ShaderResourceView* depthSrv)
	{
		Renderer&       r = g_r;
		ID3D11Resource* g = g_gbufSeen;
		if (!g || !r.probeHdrSrv || r.lightPending || g_frame.load() - r.lightFrame < 20 || !r.psLightProbe || !depthSrv || !r.llReady) return;
		if (g != r.gbufRes) {
			SafeRelease(r.gbufSrv);
			r.gbufRes = g;
			r.gbufBad = false;
			D3D11_TEXTURE2D_DESC td{};
			ID3D11Texture2D*     gt = nullptr;
			if (SUCCEEDED(g->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&gt))) && gt) {
				gt->GetDesc(&td);
				gt->Release();
			}
			if (td.Width && (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) && td.SampleDesc.Count == 1) {
				D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
				sv.Format = td.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS ? DXGI_FORMAT_B8G8R8A8_UNORM : td.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ? DXGI_FORMAT_R8G8B8A8_UNORM : td.Format;
				sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
				sv.Texture2D.MipLevels = 1;
				if (FAILED(r.dev->CreateShaderResourceView(g, &sv, &r.gbufSrv))) r.gbufSrv = nullptr;
			}
			if (!r.gbufSrv) {
				r.gbufBad = true;
				Log("light: GTA's G-buffer colour can't be read (bind flags 0x%x); no light measurement", td.BindFlags);
			}
		}
		if (r.gbufBad || !r.gbufSrv) return;
		if (!r.lightTex) {
			D3D11_TEXTURE2D_DESC td{};
			td.Width = 64;
			td.Height = 37;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.lightTex)) || FAILED(r.dev->CreateRenderTargetView(r.lightTex, nullptr, &r.lightRtv))) {
				SafeRelease(r.lightTex);
				return;
			}
			td.Usage = D3D11_USAGE_STAGING;
			td.BindFlags = 0;
			td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.lightStaging))) {
				SafeRelease(r.lightRtv);
				SafeRelease(r.lightTex);
				return;
			}
		}
		Upload(r.cb, fc);
		const float    zero[4] = {};
		D3D11_VIEWPORT vp{ 0, 0, 64.0f, 37.0f, 0, 1 };
		ID3D11ShaderResourceView* none[3] = {};
		r.ctx->PSSetShaderResources(31, 3, none);
		r.ctx->OMSetRenderTargets(1, &r.lightRtv, nullptr);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psLightProbe, nullptr, 0);
		ID3D11ShaderResourceView* d0 = depthSrv;
		r.ctx->PSSetShaderResources(0, 1, &d0);
		ID3D11ShaderResourceView* ll = r.llSrv[r.llCur];
		r.ctx->PSSetShaderResources(27, 1, &ll);
		ID3D11ShaderResourceView* in[3] = { r.probeHdrSrv, nullptr, r.gbufSrv };
		r.ctx->PSSetShaderResources(31, 3, in);
		r.ctx->Draw(3, 0);
		r.ctx->PSSetShaderResources(31, 3, none);
		r.ctx->PSSetShaderResources(27, 1, none);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->CopyResource(r.lightStaging, r.lightTex);
		r.lightPending = true;
		r.lightFrame = g_frame.load();
		r.lightDayAt = fc.sunDir[3];
		r.lightSunAt = g_sunVis;
		r.ctx->IASetInputLayout(r.layout);
	}

	void ReadLight()
	{
		Renderer& r = g_r;
		if (!r.lightPending || !r.lightStaging) return;
		D3D11_MAPPED_SUBRESOURCE m{};
		if (r.ctx->Map(r.lightStaging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) {
			if (g_frame.load() - r.lightFrame > 120) r.lightPending = false;
			return;
		}
		std::vector<float> ratios;
		ratios.reserve(64 * 36);
		for (int y = 0; y < 36; ++y) {
			const float* row = reinterpret_cast<const float*>(static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch);
			for (int x = 0; x < 64; ++x)
				if (row[x * 4 + 1] >= 8.0f && std::isfinite(row[x * 4])) ratios.push_back(row[x * 4]);
		}
		const float* last = reinterpret_cast<const float*>(static_cast<const uint8_t*>(m.pData) + size_t(36) * m.RowPitch);
		const float  sky = last[0], up = last[1];
		r.ctx->Unmap(r.lightStaging, 0);
		r.lightPending = false;
		if (ratios.size() < 200) return;
		std::sort(ratios.begin(), ratios.end());
		const double shade = ratios[ratios.size() * 20 / 100], lit = ratios[ratios.size() * 85 / 100];
		const double k = r.lightSamples ? 0.2 : 1.0;
		r.lightShade += (shade - r.lightShade) * k;
		r.lightLit += (lit - r.lightLit) * k;
		r.lightSky += (sky - r.lightSky) * k;
		r.lightUp += (up - r.lightUp) * k;
		++r.lightSamples;
		{
			// v1.9.6: what our model needs to match: sun = (light in sun - in shade) over 0.75 x our sky,
			// light from the surroundings = light in shade over our surroundings straight up (clamped, smoothed)
			const double tSun = std::clamp(std::max(0.0, r.lightLit - r.lightShade) / std::max(1e-4, 0.75 * r.lightSky), 1.0, 20.0);
			const double tAmb = std::clamp(r.lightShade / std::max(1e-4, r.lightUp), 0.2, 1.5);
			if (r.autoSun <= 0) {
				r.autoSun = g_cfg.hdrSunToSky;  // start from the fixed values: no jump when auto takes over (sandbox review)
				r.autoAmb = g_cfg.hdrAmbient;
			}
			// the sun only from daylight with the sun not blocked: at night or in a tunnel the difference between
			// lit and shaded parts is street lights / headlights, not the sun (sandbox review)
			if (r.lightDayAt > 0.5f && r.lightSunAt > 0.8f) r.autoSun += (tSun - r.autoSun) * 0.08;
			r.autoAmb += (tAmb - r.autoAmb) * 0.08;
		}
		const ULONGLONG now = GetTickCount64();
		if (now - r.lightLogMs >= 10000) {
			r.lightLogMs = now;
			const double sunToSky = std::max(0.0, r.lightLit - r.lightShade) / std::max(1e-4, 0.75 * r.lightSky);
			const double amb = r.lightShade / std::max(1e-4, r.lightUp);
			Log("light (GTA's G-buffer colour vs its HDR picture): light in shade %.3f, in sun %.3f (x%.1f); our model: sky %.3f, surroundings straight up %.3f -> "
			    "to match GTA: HdrSunToSky about %.1f, HdrAmbient about %.2f; using %.1f (%s) / %.2f (%s)",
				r.lightShade, r.lightLit, r.lightLit / std::max(1e-4, r.lightShade), r.lightSky, r.lightUp, sunToSky, amb, HdrSunNow(), g_cfg.hdrSunAuto ? "auto" : "fixed",
				HdrAmbientNow(), g_cfg.hdrAmbAuto ? "auto" : "fixed");
		}
	}

	// ---- v1.9.7: GTA's own reflection map ----------------------------------------------------------
	// GTA draws its surroundings into a dual paraboloid map early in every frame (R11G11B10 float,
	// 2048x1024 on the user's PC, written mip by mip); its own cars reflect it. The plugin reads it in
	// place (a view on GTA's texture, no copy) while drawing inside GTA's frame. Its orientation is not
	// documented, so it is found by comparing the map with GTA's own picture of the same directions.
	void DpPrepare(bool hdr)
	{
		Renderer&      r = g_r;
		const uint64_t f = g_frame.load();
		r.dpUsable = false;
		if (!hdr || !g_cfg.gtaRefl) return;
		const ULONGLONG nowMs = GetTickCount64();
		if (!r.dpFirstHdrMs) r.dpFirstHdrMs = nowMs;
		if (ID3D11Resource* g = g_dpSeen) {
			int idx = -1;
			for (int i = 0; i < 4; ++i)
				if (r.dpViews[i].res == g) idx = i;
			if (idx < 0) {
				// a new texture: a free slot, else the one unused the longest
				idx = 0;
				for (int i = 0; i < 4; ++i) {
					if (!r.dpViews[i].res) {
						idx = i;
						break;
					}
					if (r.dpViews[i].lastFrame < r.dpViews[idx].lastFrame) idx = i;
				}
				Renderer::DpView& v = r.dpViews[idx];
				SafeRelease(v.srv);
				v = Renderer::DpView{};
				v.res = g;
				D3D11_TEXTURE2D_DESC td{};
				if (TexDescOf(g, td) && (td.BindFlags & D3D11_BIND_SHADER_RESOURCE) && td.SampleDesc.Count == 1) {
					D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
					sv.Format = td.Format;
					sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
					sv.Texture2D.MostDetailedMip = 0;
					sv.Texture2D.MipLevels = UINT(-1);
					if (FAILED(r.dev->CreateShaderResourceView(g, &sv, &v.srv))) v.srv = nullptr;
					v.w = td.Width;
					v.h = td.Height;
					v.mips = td.MipLevels;
				}
				v.bad = !v.srv;
				if (v.bad)
					Log("GTA reflections: GTA's reflection map can't be read (bind flags 0x%x)", td.BindFlags);
				else
					Log("GTA reflections: found GTA's reflection map %s %ux%u, %u mip levels (GTA writes %u)%s", FmtName(td.Format), td.Width, td.Height, td.MipLevels,
						g_dpSeenMips, (r.dpLayout[0] < 0 && g_cfg.gtaReflLayout[0] < 0) ? "; finding its orientation..." : "");
			}
			Renderer::DpView& v = r.dpViews[idx];
			v.lastFrame = f;
			v.written = std::max(v.written, g_dpSeenMips);
			r.dpCur = idx;
		}
		// views of textures GTA stopped using are let go (they keep GTA's texture alive)
		for (int i = 0; i < 4; ++i) {
			Renderer::DpView& v = r.dpViews[i];
			if (v.res && i != r.dpCur && f - v.lastFrame > 600) {
				SafeRelease(v.srv);
				v = Renderer::DpView{};
			}
		}
		// the last map stays in use for a moment if GTA skips a frame (no flicker)
		if (r.dpCur >= 0) {
			Renderer::DpView& v = r.dpViews[r.dpCur];
			if (v.res && v.srv && !v.bad && f - v.lastFrame <= 30) {
				r.dpSrv = v.srv;
				r.dpMips = std::max(1u, std::min(v.mips, std::max(v.written, 1u)));
				r.dpUsable = true;
			}
		}
		if (!r.dpUsable && r.dpCur < 0 && !r.dpNotFoundLogged && nowMs - r.dpFirstHdrMs > 15000) {
			r.dpNotFoundLogged = true;
			Log("GTA reflections: GTA's reflection map not found in its frame (graphics setting 'Reflection Quality' off?); using the learned surroundings");
		}
	}

	int DpLayout(int k) { return g_cfg.gtaReflLayout[k] >= 0 ? g_cfg.gtaReflLayout[k] : g_r.dpLayout[k]; }
	double DpScaleNow() { return g_cfg.gtaReflScale >= 0 ? g_cfg.gtaReflScale : (g_r.dpScale > 0 ? g_r.dpScale : 1.0); }

	void DpCalibrate(const FrameCB& fc, ID3D11ShaderResourceView* depthSrv)
	{
		Renderer& r = g_r;
		const bool fixed = g_cfg.gtaReflLayout[0] >= 0 && g_cfg.gtaReflLayout[1] >= 0 && g_cfg.gtaReflScale >= 0;
		const bool locked = r.dpLayout[0] >= 0 && r.dpLayout[1] >= 0;
		if (!r.dpUsable || fixed || r.dpCalPending || !r.psDpCalib || !depthSrv || !r.envHdrSrv || r.envSrcSrv != r.envHdrSrv) return;
		if (g_frame.load() - r.dpCalFrame < (locked ? 90u : 12u)) return;
		if (!r.dpCalTex) {
			D3D11_TEXTURE2D_DESC td{};
			td.Width = 144;
			td.Height = 33;
			td.MipLevels = 1;
			td.ArraySize = 1;
			td.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			td.SampleDesc.Count = 1;
			td.Usage = D3D11_USAGE_DEFAULT;
			td.BindFlags = D3D11_BIND_RENDER_TARGET;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.dpCalTex)) || FAILED(r.dev->CreateRenderTargetView(r.dpCalTex, nullptr, &r.dpCalRtv))) {
				SafeRelease(r.dpCalTex);
				return;
			}
			td.Usage = D3D11_USAGE_STAGING;
			td.BindFlags = 0;
			td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			if (FAILED(r.dev->CreateTexture2D(&td, nullptr, &r.dpCalStaging))) {
				SafeRelease(r.dpCalRtv);
				SafeRelease(r.dpCalTex);
				return;
			}
		}
		Upload(r.cb, fc);
		const float    zero[4] = {};
		D3D11_VIEWPORT vp{ 0, 0, 144.0f, 33.0f, 0, 1 };
		ID3D11ShaderResourceView* none[4] = {};
		r.ctx->PSSetShaderResources(31, 4, none);
		r.ctx->OMSetRenderTargets(1, &r.dpCalRtv, nullptr);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psDpCalib, nullptr, 0);
		ID3D11ShaderResourceView* d0 = depthSrv;
		r.ctx->PSSetShaderResources(0, 1, &d0);
		ID3D11ShaderResourceView* in[4] = { r.envHdrSrv, nullptr, nullptr, r.dpSrv };
		r.ctx->PSSetShaderResources(31, 4, in);
		r.ctx->Draw(3, 0);
		r.ctx->PSSetShaderResources(31, 4, none);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);
		r.ctx->CopyResource(r.dpCalStaging, r.dpCalTex);
		r.dpCalPending = true;
		r.dpCalFrame = g_frame.load();
		r.ctx->IASetInputLayout(r.layout);
	}

	void ReadDpCal()
	{
		Renderer& r = g_r;
		if (!r.dpCalPending || !r.dpCalStaging) return;
		D3D11_MAPPED_SUBRESOURCE m{};
		if (r.ctx->Map(r.dpCalStaging, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, &m) != S_OK) {
			if (g_frame.load() - r.dpCalFrame > 120) r.dpCalPending = false;
			return;
		}
		static float v[33][144][4];
		for (int y = 0; y < 33; ++y) memcpy(v[y], static_cast<const uint8_t*>(m.pData) + size_t(y) * m.RowPitch, sizeof(v[y]));
		constexpr int kScr = 32;  // the row with GTA's picture
		r.ctx->Unmap(r.dpCalStaging, 0);
		r.dpCalPending = false;
		++r.dpTests;
		// per half of all directions (0 above the horizon, 1 below): Pearson correlation of log brightness
		// between GTA's picture and the map in each layout
		for (int k = 0; k < 2; ++k) {
			// points far enough away that GTA's other cars/people (missing from the map) and the map's own
			// position matter less: 12 m above the horizon, 6 m below (mostly road close by)
			std::vector<int> pts;
			for (int i = 0; i < 144; ++i) {
				const float* sc = v[kScr][i];
				if (sc[2] < 0.5f || !std::isfinite(sc[0]) || !(sc[3] >= (k == 0 ? 12.0f : 6.0f))) continue;
				if (k == 0 ? sc[1] > 0.03f : sc[1] < -0.03f) pts.push_back(i);
			}
			r.dpLastN[k] = int(pts.size());
			// brightness: GTA's picture over the map in the found layout, median over the points above the
			// horizon (every test with enough points)
			if (k == 0 && DpLayout(0) >= 0 && pts.size() >= 12) {
				const int           lc = DpLayout(0);
				std::vector<double> ratio;
				for (int i : pts)
					if (v[lc][i][0] > 1e-3f) ratio.push_back(double(v[kScr][i][0]) / double(v[lc][i][0]));
				if (ratio.size() >= 12) {
					std::nth_element(ratio.begin(), ratio.begin() + ratio.size() / 2, ratio.end());
					const double med = std::clamp(ratio[ratio.size() / 2], 0.1, 10.0);
					r.dpScale = r.dpScale > 0 ? r.dpScale + (med - r.dpScale) * 0.2 : med;
				}
			}
			if (pts.size() < (k == 0 ? 24u : 16u)) continue;
			// the directions below the horizon (fewer, closer points) are only decided once the sky half is
			// known, and must use the other half of the map (sandbox review)
			if (k == 1 && DpLayout(0) < 0) continue;
			double best = -2, second = -2;
			int    bi = -1;
			for (int c = 0; c < 32; ++c) {
				if (k == 1 && ((DpLayout(0) >> 3) & 1) == ((c >> 3) & 1)) continue;
				double sx = 0, sy = 0, sxx = 0, syy = 0, sxy = 0;
				for (int i : pts) {
					const double x = log(std::max(1e-4, double(v[kScr][i][0]))), y = log(std::max(1e-4, double(v[c][i][0])));
					sx += x;
					sy += y;
					sxx += x * x;
					syy += y * y;
					sxy += x * y;
				}
				const double n = double(pts.size());
				const double cov = sxy - sx * sy / n, vx = sxx - sx * sx / n, vy = syy - sy * sy / n;
				const double rr = (vx > 1e-9 && vy > 1e-9) ? cov / sqrt(vx * vy) : 0.0;
				if (rr > best) {
					second = best;
					best = rr;
					bi = c;
				} else if (rr > second) {
					second = rr;
				}
			}
			r.dpLastR[k] = best;
			r.dpLastR2[k] = second;
			r.dpLastBest[k] = bi;
			const bool decisive = bi >= 0 && best >= 0.5 && best - second >= 0.08;
			if (!decisive) continue;
			++r.dpDecisive[k];
			r.dpHist[k][r.dpHistPos[k]] = bi + 1;
			r.dpHistPos[k] = (r.dpHistPos[k] + 1) % 8;
			int votes = 0;
			for (int h = 0; h < 8; ++h) votes += r.dpHist[k][h] == bi + 1 ? 1 : 0;
			const int need = r.dpLayout[k] < 0 ? 4 : 6;  // switching away from a found layout needs more
			if (bi != r.dpLayout[k] && votes >= need) {
				Log("GTA reflections: %s the horizon = layout %d (correlation %.2f, next best %.2f, %d of the last 8 tests)%s", k == 0 ? "above" : "below", bi, best,
					second, votes, r.dpLayout[k] >= 0 ? " - changed" : "");
				r.dpLayout[k] = bi;
			}
		}
		const ULONGLONG now = GetTickCount64();
		if (now - r.dpLogMs >= 10000) {
			r.dpLogMs = now;
			Log("GTA reflections: %s; layout above %d / below %d (-1 = not found yet); last test: above best %d r=%.2f (next %.2f, %d points), below best %d r=%.2f "
			    "(next %.2f, %d points); decisive tests %u/%u of %u; brightness x%.2f (%s; measured x%.2f)",
				(DpLayout(0) >= 0 || DpLayout(1) >= 0) ? "in use" : "not in use yet", DpLayout(0), DpLayout(1), r.dpLastBest[0], r.dpLastR[0], r.dpLastR2[0], r.dpLastN[0],
				r.dpLastBest[1], r.dpLastR[1], r.dpLastR2[1], r.dpLastN[1], r.dpDecisive[0], r.dpDecisive[1], r.dpTests, DpScaleNow(),
				g_cfg.gtaReflScale >= 0 ? "fixed, GtaReflectionScale" : "auto", r.dpScale);
		}
	}

	// v2.5 (dents): the newest node moves (DentsTick, script thread) into the car's node buffer
	void DentsUpload(GpuCar& c)
	{
		if (!c.nodeBuf || !c.nodeN) return;
		AcquireSRWLockShared(&g_dentShared.lock);
		if (g_dentShared.seq != c.nodeSeq) {
			if (g_dentShared.gen == c.gen && g_dentShared.data.size() == size_t(c.nodeN) * 8) {
				D3D11_MAPPED_SUBRESOURCE ms{};
				if (SUCCEEDED(g_r.ctx->Map(c.nodeBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
					// row by row (the driver's row pitch can be wider than 1024 texels)
					const size_t total = g_dentShared.data.size(), perRow = size_t(kNodeTexW) * 4;
					for (size_t at = 0, row = 0; at < total; at += perRow, ++row)
						memcpy(static_cast<char*>(ms.pData) + row * ms.RowPitch, g_dentShared.data.data() + at, std::min(perRow, total - at) * sizeof(float));
					g_r.ctx->Unmap(c.nodeBuf, 0);
					g_dentUploads.fetch_add(1);
				}
			}
			c.nodeSeq = g_dentShared.seq;  // (data for another car file: skipped)
		}
		ReleaseSRWLockShared(&g_dentShared.lock);
	}

	// Draws the car (maps, environment, the car layer) and puts it on target: GTA's back buffer at
	// Present, or GTA's HDR scene picture inside its frame (hdr = true, v1.9.3).
	bool DrawCar(ID3D11RenderTargetView* bbRtv, UINT w, UINT h, ID3D11ShaderResourceView* depthSrv, const CamSample& cam, bool occlusion, bool picking, bool hdr = false)
	{
		Renderer& r = g_r;
		GpuCar&   c = r.car;
		if (!c.vb || !EnsureOwnDepth(w, h) || !EnsureLayer(w, h)) {
			return false;
		}
		if (g_cfg.selfShadows) {
			EnsureDepthMaps(r.sunDepth, UINT(g_cfg.selfShadowSize), 1, "sun self-shadow");
		}
		if (fabsf(cam.sunVis - r.lastSunVis) > 0.45f) {
			Log("lighting: the sun is %s by GTA's world (raycasts, visibility %.2f)", cam.sunVis < r.lastSunVis ? "blocked" : "no longer blocked", cam.sunVis);
			r.lastSunVis = cam.sunVis;
		}
		DpPrepare(hdr);  // v1.9.7
		FrameCB fc{};
		FillFrameCB(fc, cam, w, h, occlusion && depthSrv, hdr);
		const float zero[4] = {};
		BindCommon(depthSrv);
		const UINT stride = sizeof(car::Vtx), offset = 0;
		r.ctx->IASetInputLayout(r.layout);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->IASetVertexBuffers(0, 1, &c.vb, &stride, &offset);
		// v2.5 (dents): each vertex's triangle of nodes (slot 1) and where they are now (t35; none: drawn as it is)
		DentsUpload(c);
		const UINT skinOffset = 0;
		r.ctx->IASetVertexBuffers(1, 1, &c.skinVb, &c.skinStride, &skinOffset);
		r.ctx->VSSetShaderResources(35, 1, &c.nodeSrv);
		r.ctx->IASetIndexBuffer(c.ib, DXGI_FORMAT_R32_UINT, 0);
		r.ctx->RSSetState(r.raster);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		UnbindPsSrvs();

		// maps of the car: contact shadow and ambient occlusion (once per car), ground sun shadow and
		// sun self-shadow (every frame)
		const bool groundShadow = depthSrv && (fc.shadowA[2] > 0.0f || fc.shadowA[3] > 0.0f);
		if (groundShadow && fc.shadowA[2] > 0.0f && r.contactDirty) {
			RenderShadowMap(fc, r.contactRtv, r.contactSrv, 512, false, cam);
			r.contactDirty = false;
		}
		if (groundShadow && fc.shadowA[3] > 0.0f) {
			RenderShadowMap(fc, r.sunRtv, r.sunSrv, 1024, true, cam);
		}
		if (g_cfg.ao && r.aoDirty) {
			RenderAO(fc, cam);
			fc.sl3[3] = (r.aoMaps.srv && !r.aoDirty) ? 1.0f : 0.0f;
		}
		if (fc.sl0[3] > 0.5f && r.sunDepth.srv) {
			const double L[3] = { fc.sl0[0], fc.sl0[1], fc.sl0[2] };
			RenderDepthDir(fc, L, r.sunDepth.dsv[0], r.sunDepth.size, true, cam);
		}
		if (g_cfg.environment == 1 || hdr) {
			const int stage = hdr ? 1 : 0;
			if (r.lastStage != stage) {
				// v1.9.5: LDR and HDR brightness differ, so each has its own learned surroundings and a switch
				// swaps them. v1.9.4 restarted learning on a switch (only after 30 frames), so a car drawn
				// for a frame with the other's brightness flickered super dark / super light.
				if (r.lastStage >= 0 && r.llTexO[0] && r.llTexO[1]) {
					for (int k = 0; k < 2; ++k) {
						std::swap(r.llTex[k], r.llTexO[k]);
						std::swap(r.llRtv[k], r.llRtvO[k]);
						std::swap(r.llSrv[k], r.llSrvO[k]);
					}
					std::swap(r.llCur, r.llCurO);
					std::swap(r.llInit, r.llInitO);
					std::swap(r.llReady, r.llReadyO);
					std::swap(r.llCam, r.llCamO);
					std::swap(r.llUsedMs, r.llUsedMsO);
					++r.stageSwaps;
				}
				if (r.lastStage < 0 || GetTickCount64() - r.llUsedMs > 3000) {
					r.llInit = true;  // stale: what's around now
				} else {
					r.llCam[0] = cam.ox;  // recent: still where we are (the jump check must not restart it)
					r.llCam[1] = cam.oy;
					r.llCam[2] = cam.oz;
				}
				r.lastStage = stage;
				r.stageFrames = 0;
			}
			++r.stageFrames;
			r.llUsedMs = GetTickCount64();
			UpdateEnvironment(fc, cam);
			fc.envP[0] = r.llReady ? 1.0f : 0.0f;
			if (hdr) LightProbe(fc, depthSrv);  // v1.9.5
			if (hdr) DpCalibrate(fc, depthSrv);  // v1.9.7
		}

		// the car into its layer
		if (!Upload(r.cb, fc)) {
			return false;
		}
		const float clear[4] = {};
		r.ctx->ClearRenderTargetView(r.layerRtv, clear);
		r.ctx->ClearDepthStencilView(r.ownDsv, D3D11_CLEAR_DEPTH, 1.0f, 0);
		D3D11_VIEWPORT vp{ 0, 0, float(w), float(h), 0, 1 };
		r.ctx->OMSetRenderTargets(1, &r.layerRtv, r.ownDsv);
		r.ctx->RSSetViewports(1, &vp);
		r.ctx->RSSetState(r.raster);
		BindCommon(depthSrv);
		ID3D11ShaderResourceView* maps[2] = { r.contactSrv, r.sunSrv };
		r.ctx->PSSetShaderResources(13, 2, maps);
		ID3D11ShaderResourceView* lightMaps[3] = { (fc.envP[0] > 0.5f) ? r.llSrv[r.llCur] : nullptr, r.sunDepth.srv, r.aoMaps.srv };
		r.ctx->PSSetShaderResources(27, 3, lightMaps);
		ID3D11ShaderResourceView* gtaRefl = (fc.dpP[0] > 0.5f || fc.dpP[3] > 0.5f) ? r.dpSrv : nullptr;  // v1.9.7
		r.ctx->PSSetShaderResources(34, 1, &gtaRefl);

		// shadows on GTA's ground first (under everything we draw): black with coverage in the layer
		if (groundShadow) {
			r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
			r.ctx->OMSetDepthStencilState(r.noDepth, 0);
			r.ctx->IASetInputLayout(nullptr);
			r.ctx->VSSetShader(r.vsFull, nullptr, 0);
			r.ctx->PSSetShader(r.psGroundShadow, nullptr, 0);
			r.ctx->Draw(3, 0);
		}

		r.ctx->IASetInputLayout(r.layout);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsCar, nullptr, 0);
		r.ctx->PSSetShader(r.psCar, nullptr, 0);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.depthWrite, 0);
		if (r.holeCb && g_holeGen.load() != r.holeGenUp) {
			// v2.62: the bullet holes, when they changed
			HoleCB hc{};
			{
				std::lock_guard<std::mutex> lk(g_holeMx);
				memcpy(hc.holes, g_holes, sizeof(hc.holes));
				hc.info[0] = float(g_holeN);
				hc.info[1] = g_burnt;
				r.holeGenUp = g_holeGen.load();
			}
			Upload(r.holeCb, hc);
		}
		DrawParts(cam, Pass::Main, true);

		// the layer onto GTA's picture
		r.ctx->OMSetRenderTargets(1, &bbRtv, nullptr);
		UnbindPsSrvs();
		r.ctx->PSSetShaderResources(30, 1, &r.layerSrv);
		r.ctx->OMSetBlendState(r.composite, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psComposite, nullptr, 0);
		r.ctx->Draw(3, 0);
		UnbindPsSrvs();

		if (picking) {
			// crosshair at the screen centre, then (every 10th frame) the picker pass
			r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
			r.ctx->PSSetShader(r.psCross, nullptr, 0);
			r.ctx->Draw(3, 0);
			if (!r.pick.pending && g_frame.load() % 10 == 0) {
				r.ctx->IASetInputLayout(r.layout);
				r.ctx->PSSetShaderResources(13, 2, maps);
				r.ctx->PSSetShaderResources(27, 3, lightMaps);
				r.ctx->PSSetShaderResources(34, 1, &gtaRefl);
				RunPick(fc, w, h, cam);
			}
		}
		UnbindPsSrvs();
		return true;
	}

	void DrawDepthView(ID3D11RenderTargetView* rtv, UINT w, UINT h, ID3D11ShaderResourceView* depthSrv, const CamSample& cam)
	{
		Renderer& r = g_r;
		FrameCB   fc{};
		FillFrameCB(fc, cam, w, h, true);
		if (!Upload(r.cb, fc)) {
			return;
		}
		const float    zero[4] = {};
		D3D11_VIEWPORT vp{ 0, 0, float(w), float(h), 0, 1 };
		r.ctx->OMSetRenderTargets(1, &rtv, nullptr);
		r.ctx->OMSetBlendState(r.opaque, zero, 0xFFFFFFFF);
		r.ctx->OMSetDepthStencilState(r.noDepth, 0);
		r.ctx->RSSetState(r.raster);
		r.ctx->RSSetViewports(1, &vp);
		BindCommon(depthSrv);
		r.ctx->IASetInputLayout(nullptr);
		r.ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		r.ctx->VSSetShader(r.vsFull, nullptr, 0);
		r.ctx->PSSetShader(r.psDepthView, nullptr, 0);
		r.ctx->Draw(3, 0);
	}


	// ---------------------------------------------------------------------------------------
	// D3 (v1.9.3): drawing the car INSIDE GTA's frame, into its HDR scene picture, right before
	// GTA's post-processing (exposure/tone mapping, bloom, anti-aliasing) and before its HUD.
	//
	// How the place is found (no game code is read; only the render targets GTA binds, as in D0):
	//  - every frame, the plugin notes each render-target bind on the immediate context;
	//  - at Present it looks for the LAST bind of a screen-size floating-point target together with
	//    GTA's main depth buffer (= the HDR scene picture: in the user's capture R16G16B16A16_FLOAT
	//    2560x1440 + R32G8X24 depth), and the next target GTA binds after it (= where post-processing
	//    starts: in the capture an R11G11B10 1280x720 bloom target);
	//  - when 30 frames in a row agree, it locks on. From then on, when GTA binds that post-processing
	//    target, the car is drawn into the HDR picture first (state saved and restored), then GTA's
	//    bind goes ahead. Which HDR picture the post-processing reads is checked from the shader views
	//    GTA has bound one bind later.
	//  - v1.9.4: GTA swaps between look-alike textures from frame to frame (the v1.9.3 log never got two
	//    agreeing frames), so places are matched by description (format, size, MSAA, depth, and which
	//    "scene exit" of the frame it is), and the picture to draw into is picked anew every frame by its
	//    role: the last HDR picture drawn with the depth buffer, or the one before it.
	// If anything is off (not found, structure changes, paused), the car is drawn at Present as before.
	// ---------------------------------------------------------------------------------------
	struct StageBind
	{
		ID3D11Resource* rt = nullptr;  // first render target (identity only, not referenced)
		ID3D11Resource* ds = nullptr;
		UINT            w = 0, h = 0, samples = 1;
		DXGI_FORMAT     fmt = DXGI_FORMAT_UNKNOWN;      // texture format
		DXGI_FORMAT     viewFmt = DXGI_FORMAT_UNKNOWN;  // GTA's view format
		bool            sceneDepth = false;             // with GTA's main depth buffer (or one just like it)
		UINT            mip = 0;                        // v1.9.7: mip level of the target view
	};
	// v1.9.4: what a place in GTA's frame looks like. GTA swaps between look-alike textures from frame to
	// frame (the v1.9.3 log: the HDR picture was another texture on every frame), so the place is learned
	// from descriptions (format, size, order), not from which texture it is.
	struct StageSig
	{
		UINT        w = 0, h = 0, samples = 0;  // the first target of the post-processing
		DXGI_FORMAT fmt = DXGI_FORMAT_UNKNOWN;
		bool        depth = false;
		int         ord = 0;                           // which "scene exit" into such a target it is (0 = the first of the frame)
		DXGI_FORMAT sceneFmt = DXGI_FORMAT_UNKNOWN;    // the HDR picture's format
		// v1.9.5: the exit comes after GTA drew into ANOTHER HDR texture than the first one of the frame. In
		// the user's v1.9.4 capture GTA sometimes binds the same 1280x720 target in the middle of the scene
		// (after the first HDR texture) and again where post-processing really starts (after the second):
		// counting only exits after the second texture keeps the place the same in every frame.
		bool        afterOther = false;
		bool        operator==(const StageSig& o) const
		{
			return w == o.w && h == o.h && samples == o.samples && fmt == o.fmt && depth == o.depth && ord == o.ord && sceneFmt == o.sceneFmt &&
			       afterOther == o.afterOther;
		}
		bool operator!=(const StageSig& o) const { return !(*this == o); }
		bool Valid() const { return w > 0; }
	};
	struct StageView
	{
		ID3D11Texture2D*          tex = nullptr;  // referenced while kept
		ID3D11RenderTargetView*   rtv = nullptr;
		ID3D11ShaderResourceView* srv = nullptr;
		UINT                      w = 0, h = 0;
		uint64_t                  lastUse = 0;
		bool                      bad = false;  // can't be drawn into (logged once)
	};
	struct Stage
	{
		std::vector<StageBind> binds;  // this frame
		// learning
		StageSig        cand, alt;  // this layout (agreeing frames) and the one before it
		int             agree = 0;
		uint32_t        changes = 0;
		uint32_t        pingPong = 0, framesSeen = 0;  // frames whose HDR picture was another texture than the frame before
		ID3D11Resource* lastL = nullptr;
		// which picture the first post-processing pass reads: 0 the last HDR picture drawn with the depth
		// buffer, 1 the HDR picture before it, 2 something else
		int roleVotes[3] = {};
		int voteWait = 0;  // v2.52: frames waited for votes before locking
		// locked
		bool      locked = false;
		StageSig  sig;
		int       role = 0;
		int       hdrChoice = -1;
		int       mismatch = 0;
		StageView views[4];
		// this frame (live, from the binds as they come)
		bool            lastRealWasScene = false;
		bool            sceneSeen = false, injected = false;
		int             exits = 0;
		ID3D11Resource* firstScene = nullptr;  // v1.9.5: the first HDR picture GTA drew into this frame
		ID3D11Resource* curScene = nullptr;    // the latest HDR picture GTA drew into with its depth
		ID3D11Resource* otherScene = nullptr;  // the HDR picture before it (another texture), if any
		DXGI_FORMAT     curSceneView = DXGI_FORMAT_UNKNOWN;
		int             lateScene = 0;
		bool            armSrv = false;
		int             srvChecksLeft = 0;
		bool            srvFoundThisFrame = false;
		ID3D11Resource* srvFound = nullptr;
		ID3D11Resource *srvL = nullptr, *srvOther = nullptr;
		ID3D11Texture2D* drawnInto = nullptr;
		// stats
		uint32_t  statInjected = 0, statFallback = 0, statLate = 0, statNoTarget = 0;
		ULONGLONG lastLearnLogMs = 0, lastRoleLogMs = 0;
		bool      everLocked = false;
	} g_stage;

	bool IsFloatTarget(DXGI_FORMAT f)
	{
		switch (f) {
		case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_TYPELESS: case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_TYPELESS: case DXGI_FORMAT_R32G32B32_FLOAT:
			return true;
		default: return false;
		}
	}
	DXGI_FORMAT FloatViewOf(DXGI_FORMAT f)
	{
		if (f == DXGI_FORMAT_R16G16B16A16_TYPELESS) return DXGI_FORMAT_R16G16B16A16_FLOAT;
		if (f == DXGI_FORMAT_R32G32B32A32_TYPELESS) return DXGI_FORMAT_R32G32B32A32_FLOAT;
		return f;
	}

	bool StageWanted() { return g_cfg.drawStage == 1 && g_r.ok && g_viewMode.load() != 2; }

	bool TexDescOf(ID3D11Resource* res, D3D11_TEXTURE2D_DESC& td)
	{
		ID3D11Texture2D* t = nullptr;
		if (!res || FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&t))) || !t) return false;
		t->GetDesc(&td);
		t->Release();
		return true;
	}

	void DescribeRes(ID3D11Resource* res, char* out, size_t n)
	{
		D3D11_TEXTURE2D_DESC td{};
		if (TexDescOf(res, td)) {
			snprintf(out, n, "%s %ux%u", FmtName(td.Format), td.Width, td.Height);
		} else {
			snprintf(out, n, "?");
		}
	}
	void DescribeSig(const StageSig& g, char* out, size_t n)
	{
		if (!g.Valid()) {
			snprintf(out, n, "none");
			return;
		}
		snprintf(out, n, "HDR %s, then post-processing into %s %ux%u%s%s (scene exit #%d %s)", FmtName(g.sceneFmt), FmtName(g.fmt), g.w, g.h,
			g.samples > 1 ? " msaa" : "", g.depth ? " +depth" : "", g.ord + 1, g.afterOther ? "after the second HDR texture" : "after the first HDR texture");
	}

	// GTA's main depth buffer, or one just like it (size, format)
	bool IsSceneDepth(ID3D11Resource* ds)
	{
		ID3D11Resource* main = g_r.depth.tex;
		if (!ds || !main) return false;
		if (ds == main) return true;
		static ID3D11Resource* lastDs = nullptr;
		static ID3D11Resource* lastMain = nullptr;
		static bool            lastOk = false;
		if (ds == lastDs && main == lastMain) return lastOk;
		lastDs = ds;
		lastMain = main;
		D3D11_TEXTURE2D_DESC a{}, b{};
		lastOk = TexDescOf(ds, a) && TexDescOf(main, b) && a.Width == b.Width && a.Height == b.Height && a.Format == b.Format &&
		         a.SampleDesc.Count == b.SampleDesc.Count;
		return lastOk;
	}
	// a screen-size float picture drawn with GTA's depth buffer = the HDR scene
	bool IsSceneBind(const StageBind& b)
	{
		return b.rt && b.sceneDepth && b.w == g_r.bbW && b.h == g_r.bbH && b.samples == 1 && IsFloatTarget(b.fmt);
	}
	bool SigMatches(const StageBind& b, const StageSig& g)
	{
		return g.Valid() && b.w == g.w && b.h == g.h && b.samples == g.samples && b.fmt == g.fmt && (b.ds != nullptr) == g.depth;
	}

	// the shader views GTA's first post-processing pass left bound: a screen-size float picture = its input
	void StageSrvCheck(ID3D11DeviceContext* ctx)
	{
		Stage&                    s = g_stage;
		ID3D11ShaderResourceView* v[16] = {};
		ctx->PSGetShaderResources(0, 16, v);
		ID3D11Resource* found = nullptr;
		for (auto* sv : v) {
			if (!sv || found) continue;
			ID3D11Resource* res = nullptr;
			sv->GetResource(&res);
			D3D11_TEXTURE2D_DESC td{};
			if (res && TexDescOf(res, td) && td.Width == g_r.bbW && td.Height == g_r.bbH && td.SampleDesc.Count == 1 && IsFloatTarget(td.Format) &&
			    (td.BindFlags & D3D11_BIND_RENDER_TARGET)) {
				found = res;
			}
			SafeRelease(res);
		}
		for (auto*& sv : v) SafeRelease(sv);
		if (!found) return;
		s.srvFoundThisFrame = true;
		s.srvChecksLeft = 0;
		s.srvFound = found;
	}

	bool StageInject();

	void StageOnBind(ID3D11DeviceContext* ctx, UINT n, ID3D11RenderTargetView* const* rtvs, ID3D11DepthStencilView* dsv)
	{
		if (ctx != g_immediate || !g_immediate || GetCurrentThreadId() == g_ourThread.load(std::memory_order_relaxed)) {
			return;
		}
		Stage&    s = g_stage;
		StageBind b{};
		if (n > 0 && n <= D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT && rtvs && rtvs[0]) {
			ID3D11Resource* res = nullptr;
			rtvs[0]->GetResource(&res);
			if (res) {
				b.rt = res;
				D3D11_RENDER_TARGET_VIEW_DESC vd{};
				rtvs[0]->GetDesc(&vd);
				b.viewFmt = vd.Format;
				if (vd.ViewDimension == D3D11_RTV_DIMENSION_TEXTURE2D) b.mip = vd.Texture2D.MipSlice;
				D3D11_TEXTURE2D_DESC td{};
				if (TexDescOf(res, td)) {
					b.w = td.Width;
					b.h = td.Height;
					b.fmt = td.Format;
					b.samples = td.SampleDesc.Count;
				}
				res->Release();
			}
		}
		if (dsv) {
			ID3D11Resource* res = nullptr;
			dsv->GetResource(&res);
			b.ds = res;
			b.sceneDepth = IsSceneDepth(res);
			SafeRelease(res);
		}
		if (s.armSrv) {
			s.armSrv = false;
			s.srvChecksLeft = 12;
		}
		if (s.srvChecksLeft > 0 && !s.srvFoundThisFrame) {
			--s.srvChecksLeft;
			StageSrvCheck(ctx);  // what the previous post-processing pass read
		}
		if (s.binds.size() < 4096) {
			s.binds.push_back(b);
		}
		if (n >= 4 && rtvs[3] && b.rt && b.sceneDepth && b.w == g_r.bbW && b.h == g_r.bbH &&
		    (b.fmt == DXGI_FORMAT_B8G8R8A8_UNORM || b.fmt == DXGI_FORMAT_B8G8R8A8_TYPELESS || b.fmt == DXGI_FORMAT_R8G8B8A8_UNORM || b.fmt == DXGI_FORMAT_R8G8B8A8_TYPELESS)) {
			g_gbufSeen = b.rt;  // v1.9.5: G-buffer colour (light measurement)
		}
		if (b.rt && b.fmt == DXGI_FORMAT_R11G11B10_FLOAT && b.samples == 1 && b.h >= 64 && b.w == 2 * b.h && !b.ds) {
			// v1.9.7: GTA's reflection map: written mip by mip, so bound several times in a row
			if (b.rt == g_dpCand) {
				g_dpCandMaxMip = std::max(g_dpCandMaxMip, b.mip);
				if (++g_dpCandBinds >= 3 && !g_dpSeen) {
					g_dpSeen = b.rt;
					g_dpSeen->AddRef();  // sandbox review: GTA can't free it before we use it this frame
				}
				if (g_dpSeen == b.rt) g_dpSeenMips = g_dpCandMaxMip + 1;
			} else if (!g_dpSeen) {
				g_dpCand = b.rt;
				g_dpCandBinds = 1;
				g_dpCandMaxMip = b.mip;
			}
		}
		if (!b.rt || b.rt == g_r.bbRes) {
			return;  // GTA rebinds its back buffer between passes; depth-only binds: neither counts
		}
		if (IsSceneBind(b)) {
			if (!s.firstScene) s.firstScene = b.rt;
			if (s.curScene && s.curScene != b.rt) s.otherScene = s.curScene;
			s.curScene = b.rt;
			s.curSceneView = b.viewFmt;
			s.sceneSeen = true;
			if (s.injected) ++s.lateScene;
			s.lastRealWasScene = true;
			return;
		}
		const StageSig& want = s.locked ? s.sig : s.cand;
		const bool      afterOther = s.curScene && s.firstScene && s.curScene != s.firstScene;
		if (s.lastRealWasScene && SigMatches(b, want) && afterOther == want.afterOther) {
			if (s.exits == want.ord) {
				s.armSrv = true;
				s.srvL = s.curScene;
				s.srvOther = s.otherScene;
				if (s.locked && s.sceneSeen && !s.injected && StageWanted()) {
					s.injected = StageInject();
				}
			}
			++s.exits;
		}
		s.lastRealWasScene = false;
	}

	void StageReleaseViews()
	{
		for (StageView& v : g_stage.views) {
			SafeRelease(v.rtv);
			SafeRelease(v.srv);
			SafeRelease(v.tex);
			v = StageView{};
		}
	}

	void StageUnlock(const char* why)
	{
		Stage& s = g_stage;
		if (s.locked) {
			Log("D3: stopped drawing inside GTA's frame (%s); drawing at Present until it is found again", why);
		}
		s.locked = false;
		s.mismatch = 0;
		// v2.52: the next lock goes by fresh votes only (your 2.51 log: near water it relocked with the water layout's votes and drew
		// the car into a picture nobody shows)
		s.roleVotes[0] = s.roleVotes[1] = s.roleVotes[2] = 0;
		s.voteWait = 0;
		StageReleaseViews();
	}

	// views of the HDR picture we draw into this frame (kept for the few textures GTA swaps between)
	StageView* StageViewFor(ID3D11Resource* res, DXGI_FORMAT gtaView)
	{
		Stage&     s = g_stage;
		StageView* slot = nullptr;
		for (StageView& v : s.views) {
			if (v.tex && static_cast<ID3D11Resource*>(v.tex) == res) {
				v.lastUse = g_frame.load();
				return v.bad ? nullptr : &v;
			}
		}
		for (StageView& v : s.views) {
			if (!v.tex) {
				slot = &v;
				break;
			}
		}
		if (!slot) {
			slot = &s.views[0];
			for (StageView& v : s.views)
				if (v.lastUse < slot->lastUse) slot = &v;
			SafeRelease(slot->rtv);
			SafeRelease(slot->srv);
			SafeRelease(slot->tex);
			*slot = StageView{};
		}
		ID3D11Texture2D* tex = nullptr;
		if (FAILED(res->QueryInterface(__uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&tex))) || !tex) return nullptr;
		slot->tex = tex;  // keeps our reference
		slot->lastUse = g_frame.load();
		D3D11_TEXTURE2D_DESC td{};
		tex->GetDesc(&td);
		char why[160] = "";
		if (td.SampleDesc.Count != 1) snprintf(why, sizeof(why), "it is multisampled (MSAA x%u): turn MSAA off in GTA", td.SampleDesc.Count);
		else if (!(td.BindFlags & D3D11_BIND_RENDER_TARGET) || !(td.BindFlags & D3D11_BIND_SHADER_RESOURCE)) snprintf(why, sizeof(why), "it can't be drawn into / read (bind flags 0x%x)", td.BindFlags);
		if (!why[0]) {
			const DXGI_FORMAT vf = gtaView != DXGI_FORMAT_UNKNOWN ? gtaView : FloatViewOf(td.Format);
			D3D11_RENDER_TARGET_VIEW_DESC rv{};
			rv.Format = vf;
			rv.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			D3D11_SHADER_RESOURCE_VIEW_DESC sv{};
			sv.Format = vf;
			sv.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			sv.Texture2D.MipLevels = 1;
			if (FAILED(g_r.dev->CreateRenderTargetView(tex, &rv, &slot->rtv)) || FAILED(g_r.dev->CreateShaderResourceView(tex, &sv, &slot->srv))) {
				snprintf(why, sizeof(why), "views of it could not be made (%s)", FmtName(vf));
			}
		}
		if (why[0]) {
			SafeRelease(slot->rtv);
			SafeRelease(slot->srv);
			slot->bad = true;
			Log("D3: found GTA's HDR picture but can't draw into it: %s. Drawing at Present.", why);
			return nullptr;
		}
		slot->w = td.Width;
		slot->h = td.Height;
		return slot;
	}

	void StageLock(const StageSig& g, int role, const char* how)
	{
		Stage& s = g_stage;
		StageUnlock("relearning");
		s.locked = true;
		s.everLocked = true;
		s.sig = g;
		s.role = role;
		s.hdrChoice = g_cfg.hdrTarget;
		s.mismatch = 0;
		char a[192];
		DescribeSig(g, a, sizeof(a));
		Log("D3: drawing the car INSIDE GTA's frame from now on: %s; drawn into %s. GTA swapped its HDR picture between textures on %u of %u frames.", a, how,
			s.pingPong, s.framesSeen);
		Notify("D1: drawing the car inside GTA's frame (D3 test): GTA's exposure, bloom and HUD now apply to it.");
	}

	// at Present: learn from this frame's binds, then start the next frame
	void StageEndFrame()
	{
		Stage&    s = g_stage;
		Renderer& r = g_r;
		if (s.injected) ++s.statInjected;
		if (s.locked && s.lateScene) ++s.statLate;
		// which picture did the post-processing read?
		if (s.srvFoundThisFrame && s.srvFound) {
			const int role = s.srvFound == s.srvL ? 0 : (s.srvOther && s.srvFound == s.srvOther) ? 1 : 2;
			++s.roleVotes[role];
			if (s.roleVotes[0] + s.roleVotes[1] + s.roleVotes[2] > 400)
				for (int& v : s.roleVotes) v /= 2;
		}
		// this frame's layout: the last HDR picture drawn with the depth buffer (L), the first real target after it (P)
		int L = -1;
		for (int i = 0; i < int(s.binds.size()); ++i)
			if (IsSceneBind(s.binds[i])) L = i;
		StageSig g{};
		if (L >= 0) {
			int P = -1;
			for (int j = L + 1; j < int(s.binds.size()); ++j) {
				const StageBind& b = s.binds[j];
				if (b.rt && b.rt != r.bbRes) {
					P = j;
					break;
				}
			}
			// v2.52: post-processing keeps the screen's shape (your log: 1280x720 for 2560x1440); near water GTA binds a 512x512
			// R32_FLOAT target there first, which isn't it
			if (P >= 0) {
				const StageBind& p = s.binds[P];
				const double     ar = double(p.w) * r.bbH, as = double(p.h) * r.bbW;
				if (p.w * 8 < r.bbW || fabs(ar - as) > 0.03 * as) P = -1;
			}
			if (P >= 0) {
				const StageBind& p = s.binds[P];
				g.w = p.w;
				g.h = p.h;
				g.samples = p.samples;
				g.fmt = p.fmt;
				g.depth = p.ds != nullptr;
				g.sceneFmt = s.binds[L].fmt;
				ID3D11Resource* first = nullptr;
				for (int k = 0; k <= L && !first; ++k)
					if (IsSceneBind(s.binds[k])) first = s.binds[k].rt;
				g.afterOther = s.binds[L].rt != first;
				int             ord = 0;
				bool            lastScene = false;
				ID3D11Resource* lastSceneRt = nullptr;
				for (int k = 0; k < P; ++k) {
					const StageBind& b = s.binds[k];
					if (!b.rt || b.rt == r.bbRes) continue;
					if (IsSceneBind(b)) {
						lastScene = true;
						lastSceneRt = b.rt;
						continue;
					}
					if (lastScene && SigMatches(b, g) && (lastSceneRt != first) == g.afterOther) ++ord;
					lastScene = false;
				}
				g.ord = ord;
			}
			++s.framesSeen;
			if (s.lastL && s.binds[L].rt != s.lastL) ++s.pingPong;
			s.lastL = s.binds[L].rt;
		}
		if (g.Valid() && r.depth.tex) {
			if (g == s.cand) {
				++s.agree;
			} else {
				s.alt = s.cand;
				s.cand = g;
				s.agree = 1;
				++s.changes;
				if (!s.locked) s.roleVotes[0] = s.roleVotes[1] = s.roleVotes[2] = 0, s.voteWait = 0;
			}
			if (s.locked) {
				s.mismatch = g == s.sig ? 0 : s.mismatch + 1;
				const int tv = s.roleVotes[0] + s.roleVotes[1] + s.roleVotes[2];
				if (s.mismatch > 240) StageUnlock("GTA's frame changed (resolution or settings?)");
				else if (s.hdrChoice != g_cfg.hdrTarget) StageUnlock("HdrTarget changed");
				// v2.52: checked all the time: the post-processing must read the picture the car goes into
				else if (g_cfg.hdrTarget == 0 && tv >= 40 && s.roleVotes[s.role] * 10 < tv * 3) {
					Log("D3: GTA's post-processing reads another picture than the one the car is drawn into (votes %d/%d/%d): learning again", s.roleVotes[0],
						s.roleVotes[1], s.roleVotes[2]);
					StageUnlock("the car would be hidden");
				}
			}
			const int votesNow = s.roleVotes[0] + s.roleVotes[1] + s.roleVotes[2];
			if (!s.locked && s.agree >= 30 && StageWanted() && g_cfg.hdrTarget == 0 && votesNow < 10 && ++s.voteWait < 300) {
				// v2.52: not before this layout's own votes say which picture its post-processing reads
			} else if (!s.locked && s.agree >= 30 && StageWanted()) {
				// which picture to draw into
				const int total = s.roleVotes[0] + s.roleVotes[1] + s.roleVotes[2];
				int         role = 0;
				const char* how = "the last HDR picture GTA drew with its depth buffer";
				bool        ok = true;
				if (g_cfg.hdrTarget == 1) {
					how = "HdrTarget=1: the last HDR picture GTA drew with its depth buffer";
				} else if (g_cfg.hdrTarget == 2) {
					role = 1;
					how = "HdrTarget=2: the HDR picture before the last one";
				} else if (total >= 10) {
					if (s.roleVotes[2] * 10 > total * 6) {
						ok = false;
						if (GetTickCount64() - s.lastRoleLogMs > 30000) {
							s.lastRoleLogMs = GetTickCount64();
							Log("D3: GTA's post-processing reads another picture than the HDR pictures it draws (votes %d/%d/%d); not drawing inside the frame "
							    "(HdrTarget=1 or 2 forces it)",
								s.roleVotes[0], s.roleVotes[1], s.roleVotes[2]);
						}
					} else if (s.roleVotes[1] > s.roleVotes[0]) {
						role = 1;
						how = "the HDR picture before the last one (that's what the post-processing reads)";
					} else {
						how = "the last HDR picture GTA drew with its depth buffer (that's what the post-processing reads)";
					}
				}
				if (ok) StageLock(g, role, how);
			}
		} else {
			s.agree = 0;
			if (s.locked && ++s.mismatch > 240) StageUnlock("GTA's frame changed");
		}
		if (!s.locked && g_cfg.drawStage == 1 && !s.everLocked && g_frame.load() > 600 && GetTickCount64() - s.lastLearnLogMs > 30000) {
			s.lastLearnLogMs = GetTickCount64();
			char a[192], b[192];
			DescribeSig(s.cand, a, sizeof(a));
			DescribeSig(s.alt, b, sizeof(b));
			Log("D3: GTA's HDR picture not found yet (%zu binds this frame; HDR+depth bind %s, post-processing start %s; agreeing frames %d, layout changes %u; "
			    "this layout: %s; the one before: %s; HDR picture swapped texture on %u of %u frames; post-processing reads votes %d/%d/%d); drawing at Present",
				s.binds.size(), L >= 0 ? "found" : "NOT found", g.Valid() ? "found" : "NOT found", s.agree, s.changes, a, b, s.pingPong, s.framesSeen, s.roleVotes[0],
				s.roleVotes[1], s.roleVotes[2]);
		}
		s.binds.clear();
		s.lastRealWasScene = false;
		s.sceneSeen = false;
		s.injected = false;
		s.exits = 0;
		s.firstScene = s.curScene = s.otherScene = nullptr;
		g_gbufSeen = nullptr;  // found again in the next frame (no reference is held)
		SafeRelease(g_dpSeen);  // v1.9.7: found again in the next frame
		g_dpCand = nullptr;
		g_dpCandBinds = 0;
		g_dpCandMaxMip = 0;
		g_dpSeenMips = 0;
		s.curSceneView = DXGI_FORMAT_UNKNOWN;
		s.lateScene = 0;
		s.armSrv = false;
		s.srvChecksLeft = 0;
		s.srvFoundThisFrame = false;
		s.srvFound = s.srvL = s.srvOther = nullptr;
		s.drawnInto = nullptr;
	}

	bool StageInject()
	{
		Renderer& r = g_r;
		Stage&    s = g_stage;
		if (!r.ok || !g_haveModel.load()) return false;
		// v1.9.4: the picture of THIS frame (GTA swaps between look-alike textures)
		ID3D11Resource* target = s.role == 1 ? s.otherScene : s.curScene;
		if (!target) {
			++s.statNoTarget;
			return false;
		}
		StageView* view = StageViewFor(target, s.role == 0 ? s.curSceneView : DXGI_FORMAT_UNKNOWN);
		if (!view) {
			++s.statNoTarget;
			return false;
		}
		CamSample cam{};
		if (!FrameCam(cam, g_frame.load() + 1) || !cam.carOn) return false;  // the frame being drawn now (Present comes later)
		if (g_trec.state.load() == 1) TrecAdd("draw,%.3f,%u,%llu,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,%u,%s", TrecMs(), cam.seq, (unsigned long long)(g_frame.load() + 1), g_camSeq, "inside");
		bool paused = false;
		AcquireSRWLockShared(&g_shared);
		paused = g_paused;
		ReleaseSRWLockShared(&g_shared);
		if (paused) return false;
		LARGE_INTEGER q0, q1, qf;
		QueryPerformanceCounter(&q0);
		const int mode = g_viewMode.load();
		g_ourThread.store(GetCurrentThreadId());
		StateBackup st;
		st.SaveDeep(r.ctx);
		r.ctx->OMSetRenderTargets(0, nullptr, nullptr);  // GTA's depth buffer off the output before we read it
		bool                      fromClearCopy = false;
		ID3D11ShaderResourceView* depthSrv = CurrentDepthSrv(&fromClearCopy);
		bool                      drawn = false;
		if (UpdateScreenCopyHdr(view->srv, view->w, view->h)) {
			r.probeHdrSrv = view->srv;
			drawn = DrawCar(view->rtv, view->w, view->h, depthSrv, cam, mode != 1, mode == 3, true);
			r.probeHdrSrv = nullptr;
		}
		s.drawnInto = view->tex;
		st.Restore(r.ctx);
		g_ourThread.store(0);
		if (drawn) NoteSampleUse(cam.seq);
		if (Capturing()) {
			AcquireSRWLockExclusive(&g_cap.lock);
			CapFlushDraws();
			char tgt[96];
			DescribeRes(view->tex, tgt, sizeof(tgt));
			CapAppend("%7.3f ms ======== D1: the BeamNG car is drawn HERE (before the next bind), into %s: %s ========", (NowSeconds() - g_cap.t0) * 1000.0, tgt,
				drawn ? "ok" : "FAILED");
			ReleaseSRWLockExclusive(&g_cap.lock);
		}
		QueryPerformanceCounter(&q1);
		QueryPerformanceFrequency(&qf);
		const double ms = double(q1.QuadPart - q0.QuadPart) * 1000.0 / double(qf.QuadPart);
		r.cpuMsSum += ms;
		r.cpuMsMax = std::max(r.cpuMsMax, ms);
		return drawn;
	}

	// ---------------------------------------------------------------------------------------
	// Frame capture at Present: start (armed) or finish (recording)
	// ---------------------------------------------------------------------------------------

	// ---- v1.9.4: float pictures saved as .bmp with the capture (tone mapped c/(1+c), gamma 2.2)
	float HalfToFloat(uint16_t h)
	{
		const uint32_t e = (h >> 10) & 31, m = h & 1023;
		float          v = e == 0 ? ldexpf(float(m), -24) : e == 31 ? 65504.0f : ldexpf(float(m | 1024), int(e) - 25);
		return (h & 0x8000) ? -v : v;
	}
	float SmallFloat(uint32_t bits, int mantBits)
	{
		const uint32_t e = bits >> mantBits, m = bits & ((1u << mantBits) - 1);
		if (e == 0) return ldexpf(float(m), -14 - mantBits);
		if (e == 31) return 65504.0f;
		return ldexpf(float(m | (1u << mantBits)), int(e) - 15 - mantBits);
	}
	bool SavePicture(ID3D11Texture2D* src, const char* suffix, char* outPath, size_t outSize)
	{
		Renderer& r = g_r;
		if (!src || !r.dev || !r.ctx) return false;
		D3D11_TEXTURE2D_DESC td{};
		src->GetDesc(&td);
		DXGI_FORMAT fmt = td.Format == DXGI_FORMAT_R16G16B16A16_TYPELESS ? DXGI_FORMAT_R16G16B16A16_FLOAT : td.Format;
		if (fmt != DXGI_FORMAT_R11G11B10_FLOAT && fmt != DXGI_FORMAT_R16G16B16A16_FLOAT) return false;
		ID3D11Texture2D* plain = nullptr;
		D3D11_TEXTURE2D_DESC pd = td;
		pd.SampleDesc.Count = 1;
		pd.SampleDesc.Quality = 0;
		pd.MipLevels = 1;
		pd.ArraySize = 1;
		pd.MiscFlags = 0;
		pd.CPUAccessFlags = 0;
		pd.Usage = D3D11_USAGE_DEFAULT;
		pd.BindFlags = 0;
		pd.Format = fmt;
		if (td.SampleDesc.Count > 1) {
			if (FAILED(r.dev->CreateTexture2D(&pd, nullptr, &plain)) || !plain) return false;
			r.ctx->ResolveSubresource(plain, 0, src, 0, fmt);
		}
		D3D11_TEXTURE2D_DESC sd = pd;
		sd.Usage = D3D11_USAGE_STAGING;
		sd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
		sd.Format = td.SampleDesc.Count > 1 ? fmt : td.Format;
		ID3D11Texture2D* st = nullptr;
		if (FAILED(r.dev->CreateTexture2D(&sd, nullptr, &st)) || !st) {
			SafeRelease(plain);
			return false;
		}
		r.ctx->CopySubresourceRegion(st, 0, 0, 0, 0, plain ? plain : src, 0, nullptr);
		D3D11_MAPPED_SUBRESOURCE m{};
		bool ok = false;
		if (SUCCEEDED(r.ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
			const UINT step = std::max(1u, (td.Width + 1023) / 1024);
			const UINT ow = td.Width / step, oh = td.Height / step;
			const UINT rowBytes = (ow * 3 + 3) & ~3u;
			std::vector<uint8_t> img(size_t(rowBytes) * oh, 0);
			for (UINT y = 0; y < oh; ++y) {
				const uint8_t* row = static_cast<const uint8_t*>(m.pData) + size_t(y * step) * m.RowPitch;
				uint8_t*       out = img.data() + size_t(oh - 1 - y) * rowBytes;  // bottom-up
				for (UINT x = 0; x < ow; ++x) {
					float c[3];
					if (fmt == DXGI_FORMAT_R11G11B10_FLOAT) {
						uint32_t v;
						memcpy(&v, row + size_t(x * step) * 4, 4);
						c[0] = SmallFloat(v & 0x7ff, 6);
						c[1] = SmallFloat((v >> 11) & 0x7ff, 6);
						c[2] = SmallFloat(v >> 22, 5);
					} else {
						uint16_t h[4];
						memcpy(h, row + size_t(x * step) * 8, 8);
						for (int k = 0; k < 3; ++k) c[k] = HalfToFloat(h[k]);
					}
					for (int k = 0; k < 3; ++k) {
						float v = c[k] > 0.0f && c[k] < 1e30f ? c[k] : 0.0f;
						v = powf(v / (1.0f + v), 1.0f / 2.2f);
						out[x * 3 + (2 - k)] = uint8_t(std::clamp(int(v * 255.0f + 0.5f), 0, 255));  // BGR
					}
				}
			}
			r.ctx->Unmap(st, 0);
			PathNextToModule(suffix, outPath, outSize);
			HANDLE f = CreateFileA(outPath, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (f != INVALID_HANDLE_VALUE) {
				uint8_t hdr[54] = { 'B', 'M' };
				const uint32_t size = 54 + uint32_t(img.size());
				auto put32 = [&](int at, uint32_t v) { memcpy(hdr + at, &v, 4); };
				put32(2, size);
				put32(10, 54);
				put32(14, 40);
				put32(18, ow);
				put32(22, oh);
				hdr[26] = 1;
				hdr[28] = 24;
				put32(34, uint32_t(img.size()));
				DWORD w = 0;
				WriteFile(f, hdr, 54, &w, nullptr);
				WriteFile(f, img.data(), DWORD(img.size()), &w, nullptr);
				CloseHandle(f);
				ok = true;
			}
		}
		SafeRelease(st);
		SafeRelease(plain);
		return ok;
	}

	void CaptureAtPresent(ID3D11Texture2D* bb)
	{
		const int st = g_capture.load();
		if (st == 2) {
			AcquireSRWLockExclusive(&g_cap.lock);
			CapFlushDraws();
			CapAppend("%7.3f ms Present", (NowSeconds() - g_cap.t0) * 1000.0);
			std::string text;
			text.swap(g_cap.text);
			const int events = g_cap.events;
			const int targets = int(g_cap.ids.size());
			g_cap.ids.clear();
			ReleaseSRWLockExclusive(&g_cap.lock);
			g_capture.store(0);
			char path[MAX_PATH + 16];
			PathNextToModule("_frame.txt", path, sizeof(path));
			HANDLE f = CreateFileA(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (f != INVALID_HANDLE_VALUE) {
				DWORD written = 0;
				WriteFile(f, text.data(), DWORD(text.size()), &written, nullptr);
				CloseHandle(f);
			}
			Log("capture: one frame, %d events, %d textures -> %s", events, targets, path);
			// v1.9.4: GTA's reflection map(s) and our learned surroundings as pictures
			int saved = 0;
			for (int k = 0; k < 3; ++k) {
				ID3D11Texture2D* t = nullptr;
				char             why[96];
				AcquireSRWLockExclusive(&g_cap.lock);
				t = g_cap.held[k];
				g_cap.held[k] = nullptr;
				memcpy(why, g_cap.heldWhy[k], sizeof(why));
				ReleaseSRWLockExclusive(&g_cap.lock);
				if (!t) continue;
				char suffix[32], out[MAX_PATH + 32];
				snprintf(suffix, sizeof(suffix), "_gtarefl%d.bmp", k + 1);
				if (SavePicture(t, suffix, out, sizeof(out))) {
					Log("capture: saved GTA's picture %s (likely its reflection map) -> %s", why, out);
					++saved;
				} else {
					Log("capture: couldn't save GTA's picture %s", why);
				}
				t->Release();
			}
			if (g_r.llTex[g_r.llCur]) {
				char out[MAX_PATH + 32];
				if (SavePicture(g_r.llTex[g_r.llCur], "_ourenv.bmp", out, sizeof(out))) {
					Log("capture: saved the surroundings the plugin learned (128x64, all directions) -> %s", out);
					++saved;
				}
			}
			Notify("D1: frame captured (%d events, %d picture(s)) -> GtaBeamD1_frame.txt in the GTA folder.", events, saved);
		} else if (st == 1) {
			AcquireSRWLockExclusive(&g_cap.lock);
			for (auto*& t : g_cap.held) SafeRelease(t);
			g_cap.text.clear();
			g_cap.ids.clear();
			g_cap.events = 0;
			g_cap.draws = g_cap.dispatches = 0;
			g_cap.nFirst = 0;
			g_cap.firstDraws[0] = 0;
			g_cap.bb = bb;
			g_cap.t0 = NowSeconds();
			D3D11_TEXTURE2D_DESC bd{};
			bb->GetDesc(&bd);
			CapAppend("GtaBeamD1 %s frame capture: every render-target bind, clear and draw count of one GTA frame.", D1_VERSION);
			CapAppend("Back buffer T%d %s %ux%u. 'imm' = immediate context, 'def' = deferred. Draw kinds: I indexed, D plain, II/DI instanced (count x instances), X compute.",
				CapId(bb), FmtName(bd.Format), bd.Width, bd.Height);
			ReleaseSRWLockExclusive(&g_cap.lock);
			g_capture.store(2);
		}
	}

	// ---------------------------------------------------------------------------------------
	// Present callback (render thread)
	// ---------------------------------------------------------------------------------------

	void OnPresent(void* swapChain)
	{
		auto* sc = static_cast<IDXGISwapChain*>(swapChain);
		if (!sc) {
			return;
		}
		LARGE_INTEGER qf, q0, q1;
		QueryPerformanceFrequency(&qf);
		QueryPerformanceCounter(&q0);
		const uint64_t frame = g_frame.fetch_add(1) + 1;
		if (frame % 120 == 1) {
			BOOL fs = FALSE;
			if (SUCCEEDED(sc->GetFullscreenState(&fs, nullptr))) g_exclusiveFs.store(fs ? 1 : 0);  // v2.3: for the BeamNG sound wake
		}
		Renderer&      r = g_r;
		const bool     canDraw = InitRenderer(sc);
		if (!r.dev || !r.ctx) {
			return;
		}
		ID3D11Texture2D* bb = nullptr;
		if (FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&bb))) || !bb) {
			return;
		}
		CaptureAtPresent(bb);
		D3D11_TEXTURE2D_DESC bbd{};
		bb->GetDesc(&bbd);
		r.bbRes = bb;  // identity only
		// v1.9.3: frame intervals (hitches)
		{
			const double now = NowSeconds();
			if (r.lastPresentT > 0) {
				const double fms = (now - r.lastPresentT) * 1000.0;
				const double avg = r.statFrames ? r.frameMsSum / r.statFrames : fms;
				r.frameMsSum += fms;
				r.frameMsMax = std::max(r.frameMsMax, fms);
				if (fms > 25.0 && fms > avg * 2.0) ++r.hitches;
			}
			r.lastPresentT = now;
		}
		// D3: was the car already drawn inside this frame? Then learn from the frame and start the next.
		const bool injected = g_stage.injected;
		StageEndFrame();
		if (bbd.Width != r.bbW || bbd.Height != r.bbH) {
			Log("screen: %ux%u %s", bbd.Width, bbd.Height, FmtName(bbd.Format));
			r.bbW = bbd.Width;
			r.bbH = bbd.Height;
			ReleaseDepth();
			AcquireSRWLockExclusive(&g_dsvLock);
			g_dsvCount = 0;
			ReleaseSRWLockExclusive(&g_dsvLock);
		}
		r.bbFmt = bbd.Format;

		// a newly loaded car goes to video memory here
		if (canDraw) {
			std::unique_ptr<car::Model> pending;
			uint32_t                    pendingGen = 0;
			{
				std::lock_guard<std::mutex> lk(g_modelMx);
				pending = std::move(g_pendingModel);
				pendingGen = g_modelGen.load();  // (set with the model, under the same lock)
			}
			if (pending) {
				UploadCar(std::move(pending), pendingGen);
			}
		}

		PickDepth(bbd.Width, bbd.Height);
		ReadProbe();
		ReadLight();
		ReadDpCal();  // v1.9.7
		if (canDraw) {
			ReadPick();
		}

		CamSample  cam{};
		const bool haveCam = FrameCam(cam, frame);
		if (haveCam && !injected && g_trec.state.load() == 1) TrecAdd("draw,%.3f,%u,%llu,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,,%u,%s", TrecMs(), cam.seq, (unsigned long long)frame, g_camSeq, "present");
		bool       paused = false;
		AcquireSRWLockShared(&g_shared);
		paused = g_paused;
		ReleaseSRWLockShared(&g_shared);
		const int mode = g_viewMode.load();

		bool drawn = false;
		g_ourThread.store(GetCurrentThreadId());
		StateBackup st;
		st.Save(r.ctx);
		bool                      fromClearCopy = false;
		ID3D11ShaderResourceView* depthSrv = CurrentDepthSrv(&fromClearCopy);
		ID3D11RenderTargetView*   rtv = nullptr;
		if (canDraw && haveCam && !paused && SUCCEEDED(r.dev->CreateRenderTargetView(bb, nullptr, &rtv)) && rtv) {
			if (mode == 2 && depthSrv) {
				DrawDepthView(rtv, bbd.Width, bbd.Height, depthSrv, cam);
			}
			if (injected) {
				drawn = true;  // already drawn inside GTA's frame (D3)
			} else if (cam.carOn && g_haveModel.load()) {
				if (g_cfg.environment == 1 && mode != 2) {
					UpdateScreenCopy(bb, bbd);  // GTA's picture, before our car is on it
				}
				drawn = DrawCar(rtv, bbd.Width, bbd.Height, depthSrv, cam, mode != 1, mode == 3);
				if (drawn) {
					NoteSampleUse(cam.seq);
					++g_stage.statFallback;
				}
			}
		}
		if (canDraw && depthSrv && haveCam && !r.probe.pending && frame % 15 == 0) {
			RunProbe(depthSrv, cam, fromClearCopy);
		}
		st.Restore(r.ctx);
		g_ourThread.store(0);
		SafeRelease(rtv);
		SafeRelease(bb);

		QueryPerformanceCounter(&q1);
		const double ms = double(q1.QuadPart - q0.QuadPart) * 1000.0 / double(qf.QuadPart);
		r.cpuMsSum += ms;
		r.cpuMsMax = std::max(r.cpuMsMax, ms);
		++r.statFrames;
		r.statDrawn += drawn ? 1 : 0;
		const ULONGLONG now = GetTickCount64();
		if (now - r.statStartMs >= 10000) {
			const double   secs = double(now - r.statStartMs) / 1000.0;
			const uint32_t ticks = g_ticks.load();
			const uint64_t tickUs = g_tickUsSum.load();
			const uint32_t nt = std::max(1u, ticks - r.ticksAtStat);
			const uint32_t used = r.seqSame + r.seqOne + r.seqMulti;
			Stage&         sg = g_stage;
			Log("timing: frame %.2f ms avg / %.1f ms max, %u hitch(es) (>25 ms and >2x avg); our script tick %.3f ms avg / %.3f ms max; car pose per drawn frame: "
			    "new %u, repeated %u, skipped one or more %u%s",
				r.frameMsSum / std::max(1u, r.statFrames), r.frameMsMax, r.hitches, double(tickUs - r.ticksUsAtStat) / 1000.0 / nt, g_tickUsMax.exchange(0) / 1000.0,
				r.seqOne, r.seqSame, r.seqMulti, used ? "" : " (car not drawn)");
			Log("D3: %s; frames drawn inside GTA's frame %u, at Present %u, scene drawn on after us %u, no picture to draw into %u, switches inside<->Present %u",
				!g_cfg.drawStage ? "off (DrawStage=present)" : sg.locked ? "locked on" : "learning", sg.statInjected, sg.statFallback, sg.statLate, sg.statNoTarget,
				r.stageSwaps);
			sg.statInjected = sg.statFallback = sg.statLate = sg.statNoTarget = 0;
			r.stageSwaps = 0;
			r.ticksUsAtStat = tickUs;
			r.frameMsSum = r.frameMsMax = 0;
			r.hitches = 0;
			r.seqSame = r.seqOne = r.seqMulti = 0;
			Log("stats: %.1f fps, plugin %.3f ms avg / %.3f ms max per frame (CPU), car drawn in %u of %u frames (%zu tris, %.0f MB), script ticks per frame %.2f, depth #%d %s%s, %s, sun visible %.2f",
				r.statFrames / secs, r.cpuMsSum / std::max(1u, r.statFrames), r.cpuMsMax, r.statDrawn, r.statFrames, r.car.tris, r.car.bytes / 1048576.0,
				double(ticks - r.ticksAtStat) / std::max(1u, r.statFrames), r.depth.id, !r.depth.tex ? "none" : r.useClearCopy ? "pre-clear copy" : "live",
				!r.depthKnown ? "" : r.reversed ? " reversed-Z" : " standard-Z", r.lastCheck, cam.sunVis);
			r.statStartMs = now;
			r.ticksAtStat = ticks;
			r.cpuMsSum = r.cpuMsMax = 0;
			r.statFrames = r.statDrawn = 0;
			if (r.depth.tex) {
				SetStatus("depth #%d %s, %s", r.depth.id, r.useClearCopy ? "copy" : "live", !r.depthKnown ? "Z unknown" : r.reversed ? "rev-Z" : "std-Z");
			}
		}
	}
}

extern "C" __declspec(dllexport) const char* GtaBeamD1Version()
{
	return D1_VERSION;
}

extern "C" BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	switch (reason) {
	case DLL_PROCESS_ATTACH:
		g_module = instance;
		DisableThreadLibraryCalls(instance);
		InitLog();
		Log("GtaBeamD1 %s (GTA x Beam kit %s: D1 + D3 + T2 drive mode + T3 GTA's ground in BeamNG) loaded", D1_VERSION, D1_VERSION);
		if (GetModuleHandleA("GtaBeamD0.asi")) {
			Log("WARNING: GtaBeamD0.asi is loaded too. Remove it from the GTA folder (D1 replaces it).");
		}
		LoadConfig();
		LoadSurfaces();  // v2.4
		scriptRegister(instance, ScriptMain);
		presentCallbackRegister(OnPresent);
		break;
	case DLL_PROCESS_DETACH:
		presentCallbackUnregister(OnPresent);
		scriptUnregister(instance);
		if (!reserved) {
			UnhookAll();
			// v2.0: the drive receiver thread must not run on in unloaded code
			g_dnet.stop.store(true);
			if (g_dnet.s != INVALID_SOCKET) closesocket(g_dnet.s);
		}
		Log("unloaded");
		break;
	}
	return TRUE;
}
