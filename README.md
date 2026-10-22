# GTA x Beam

**Drive BeamNG.drive's soft-body cars in GTA V's world.**

GTA x Beam is a "passthrough" bridge. GTA V runs the world, walking and gunplay; BeamNG.drive runs the car physics. Both games run at the same time and talk over a local UDP connection. In GTA you see your BeamNG car, with its real crumple, on GTA's streets.

> Single-player only. Script Hook V closes GTA V in GTA Online. Personal, non-commercial fan project; not affiliated with Rockstar Games or BeamNG GmbH.

## Features
- **Your BeamNG car in GTA.** Export any BeamNG car (Shift+F11). It is drawn in GTA with its paint, materials, glass and lights, lit by GTA's sun, shadows and reflections.
- **Drive mode (F10).** BeamNG's physics drive the car. GTA's streets, hills, bridges, walls and props are measured and built in BeamNG as you go, with GTA's ground surfaces (asphalt, dirt, grass...).
- **Your BeamNG controls.** Pad and keyboard go through your own BeamNG bindings: manual gears, clutch, drive modes. One button swaps the pad between GTA and BeamNG.
- **Real damage.** BeamNG's dents, broken glass, lost bumpers and wheels show on the car in GTA. Crashes hurt you.
- **GTA's weapons.** Bullets dent the car, leave holes, pop tyres and break windows. Explosions push it and set it on fire. A car that blows up becomes a charred wreck.
- **Traffic crashes, three ways** (`TrafficMode` in the ini):
  - `twins` (default): a GTA car about to hit yours gets a BeamNG "twin", so the crash happens in BeamNG, soft body against soft body. The GTA car copies its twin's movement and takes GTA damage where the twin is dented. Trucks and buses longer than 7.5 m get GTA's own crash instead.
  - `gta`: GTA's own crash for every car. Your car is the only soft body.
  - `despawn`: any car you touch vanishes, for flat-out highway runs.
  Pedestrians are knocked down harder the faster you hit them; riders come off their bikes.
- **Show cars.** Park a BeamNG car on a GTA car with Ctrl+Shift+F11 and it stays there, drawn with its paint and dents. Then bring the next car from BeamNG. Up to 4 at once.
- **Tyre smoke.** Burnouts, locked wheels and slides make GTA's own tyre smoke at the BeamNG car's wheels; the screech is BeamNG's tyre sound.
- **Breakable props.** Lamp posts, signs, bins and hydrants fall the way you hit them. Breaking one can set off a GTA particle effect, such as sparks or a hydrant's water (set in `GtaBeamD1_props.json`).
- **Your driver's seat.** In drive mode you sit where the BeamNG car's driver camera is, so first person looks out of the BeamNG car. Ctrl+Shift+F10 lets you move yourself in the seat with the arrow keys; it's saved per car. The drawn car's front doors open with GTA's when you get in or out.
- **On foot.** Get out, walk around the car and shoot it. The BeamNG gauge and drive mode stay on.
- **Pause sync, sound and radio.** Pausing one game pauses the other. BeamNG's engine sound plays while GTA has focus, and GTA's radio plays as the phone radio.

## Requirements
- GTA V **Legacy** (PC, story mode) with [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) and its ASI loader (`dinput8.dll`).
- BeamNG.drive (tested on the current public release).
- A PC that can run both games at once. A GPU with 8 GB or more VRAM is recommended.

## Install
1. **GTA V:** copy everything in `GTA\` into your GTA V folder (next to `GTA5.exe`):
   - `GtaBeamD1.asi`
   - `GtaBeamD1.ini`
   - `GtaBeamD1_surfaces.json`, `GtaBeamD1_props.json`, `GtaBeamD1_weapons.json`, `GtaBeamD1_traffic.json`
2. **BeamNG.drive:** drag the `BeamNG\gtabridge` folder into `%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\mods\unpacked\` and say yes to replacing files. It already has the mod's layout (`lua\ge\extensions\gtaBridge.lua`, `scripts\gtabridge\modScript.lua` which loads the mod when BeamNG starts, and `vehicles\gtab_prop\`).
3. Start BeamNG first, load the empty level **Grid, Small, Pure** and spawn your car. Then start GTA V (story mode).

## How to use
| Key | What it does |
|---|---|
| **Shift+F11** | Get the car from BeamNG (exports it; the first time takes a while) |
| **F11** | Put the BeamNG car on the GTA car you're in or next to |
| **F10** | Drive mode on/off (sit in the driver's seat, standing still) |
| **Shift+F10** | The on-screen drive check text |
| **Ctrl+F11** | Re-read the ini and json files |
| **Ctrl+Shift+F11** | Show cars: park the BeamNG car (put on a GTA car with F11) there; next to a show car with no car on, take it away |
| **Ctrl+Shift+F12** | View modes for testing (normal, car always on top, depth view, part picker) |
| **Ctrl+Shift+F10** | Seat adjust (in drive mode): arrow keys and Page Up / Page Down move you; press again to save it for this car |
| **D-pad down** | Swap the pad between GTA and BeamNG (in drive mode) |

## Settings
Everything is in `GtaBeamD1.ini`, with comments. Common ones:
- `TrafficCollision=0` turns off contact with GTA's traffic and people (they pass through your car).
- `TrafficMode=twins|gta|despawn` picks how traffic crashes work (see Features). `TrafficTwins` sets how many BeamNG twins wait for GTA's cars (4 by default); `TwinMaxLengthM` (7.5) is the longest GTA vehicle that gets a twin.
- `ShowCarsMax` (4) is how many show cars you can park. Each one keeps its model in video memory.
- `DriverSeatForwardM`, `DriverSeatUpM`, `DriverSeatRightM` move you in the seat for every car (Ctrl+Shift+F10 does it per car, saved in `GtaBeamD1_seats.json`).
- `Weapons=0` turns off weapon damage on the car.
- `DriveGravity` sets how heavy the car feels (1.1 by default).
- `World=0` uses BeamNG's own map instead of GTA's ground (for testing). `WorldFill=0` leaves holes in GTA's ground empty instead of filling them with asphalt.
- `TireSmoke=0` turns the tyre smoke off; `TireSmokeSlipMs` sets how much a tyre must slip to smoke (BeamNG's own slip, in m/s); `TireSmokeFx` and `TireSmokeScale` pick GTA's effect and its size.
- `DoorOpenDeg` sets how far the drawn front doors open.

Game content lives in JSON next to the ini:
- `GtaBeamD1_surfaces.json`: GTA ground material to BeamNG ground model (grip).
- `GtaBeamD1_props.json`: which props break, stay solid or get knocked over, and their effects.
- `GtaBeamD1_weapons.json`: damage per weapon group and explosion type.
- `GtaBeamD1_traffic.json`: which BeamNG cars are the twins (`twinPool`), GTA damage per dent, masses and pedestrian knock-downs.

## Logs and bug reports
Please attach both logs to any bug report:
- `GtaBeamD1.log` (GTA V folder)
- `gtabridge_log.txt` (`%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\gtabridge\`)

## Known limits
- GTA's cars crumple only in BeamNG. In GTA they show GTA's own damage at the dented spots. The twins are vanilla BeamNG cars matched by size, not GTA's models.
- Up to 4 GTA cars at once get a twin (`TrafficTwins`). Others, and bikes, get the simpler push. Trucks over `TwinMaxLengthM` get GTA's own crash.
- `TrafficMode=gta` is new and rougher: your hidden GTA car is made solid only for the moment of contact. If GTA doesn't react, the simpler push takes over.
- Show cars stand still: their wheels and doors don't move, they aren't in BeamNG, and they are gone when GTA restarts. In drive mode hitting one is the simple push (no BeamNG crash).
- The first F10 of a BeamNG session loads the twins (about 10 s; BeamNG freezes for a moment per car). Later F10s reuse them.
- GTA drivers may not react to your car before a hit.
- The first drive through an area measures it. A saved map (`GtaBeamD1_map\`) makes later drives instant there. Delete that folder to measure again.
- Some hillsides and bridge decks can appear a few seconds late. They fill in as you approach. A hole GTA's rays can't measure, with ground on two sides of it, is filled with flat asphalt between them (it may not match a bumpy surface exactly).
- Only parts named "door" in the car file open (front doors). They turn about their front edge; the door glass and inner panel go with them if their names say door.

## Building from source
The source is in `source\`.
- Windows with Visual Studio 2022: run `build_vs2022.bat`. It needs the Script Hook V SDK's `ScriptHookV.lib`.
- Linux/CI: build with `zig c++ -target x86_64-windows-gnu` (see `source\test\build_all.sh`). That folder also has the tests, including a fake Script Hook V and a fake BeamNG for end-to-end runs under Wine.

## Credits and licences
- [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) by Alexander Blade.
- [cgltf](https://github.com/jkuhlmann/cgltf) (MIT), [stb_image](https://github.com/nothings/stb) (MIT / public domain), [bcdec](https://github.com/iOrange/bcdec) (MIT / Unlicense).
- Inspired by [SkyCraft](https://github.com/chasmlol/SkyCraft) (Skyrim x Minecraft passthrough).
- Licence texts are in `licences\`.
- No Rockstar or BeamNG game files are included. The mod reads your own installs at run time.
