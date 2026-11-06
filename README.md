# GTA x Beam

**Drive BeamNG.drive's soft-body cars in GTA V's world.**

GTA V runs the world, the walking and the guns. BeamNG.drive runs the car. Both games run at the same time and talk to each other on your PC. In GTA you see and drive your BeamNG car, with its real crumple, on GTA's streets, and GTA's traffic, people and lamp posts react to it like they would to a normal GTA car.

> Single-player only (story mode). Script Hook V closes GTA V in GTA Online. Free, non-commercial fan project; not affiliated with Rockstar Games or BeamNG GmbH.

## Quick start (5 minutes)
1. **Download** this page as a ZIP (green **Code** button → **Download ZIP**) and unzip it.
2. **GTA V files:** copy everything in the `GTA` folder into your GTA V folder (the one with `GTA5.exe`).
3. **BeamNG mod:** copy the `BeamNG\gtabridge` folder into `%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\mods\unpacked\` (paste that path into Explorer's address bar). Say yes to replacing files.
4. **Start BeamNG first.** Load the level **Grid, Small, Pure** and spawn your car.
5. **Start GTA V** (story mode). Get in a GTA car about the **same size** as your BeamNG car (see Tips).
6. In GTA press **Shift+F11** (gets your car from BeamNG; the first time takes a while), then **F11** (puts it on your GTA car), then **F10** (drive).

## Requirements
- GTA V **Legacy** for PC, story mode, with [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) and its ASI loader (`dinput8.dll`).
- BeamNG.drive (made on 0.40).
- A PC that can run both games at once. 8 GB of video memory or more is recommended; video memory is what runs out first.

## Keys (in GTA)
| Key | What it does |
|---|---|
| **Shift+F11** | Get the car from BeamNG (export). Do it again after you change the car in BeamNG. |
| **F11** | Put the BeamNG car on the GTA car you're in or next to (press again to take it off) |
| **F10** | Drive mode on/off: BeamNG's physics drive the car |
| **Shift+F10** | On-screen drive check (what goes to BeamNG and back) |
| **Ctrl+Shift+F10** | Seat adjust in drive mode: arrow keys and Page Up / Page Down, press again to save |
| **Ctrl+F10** | Timing recording (only for bug reports) |
| **Ctrl+F11** | Re-read the ini and json files without restarting |
| **Ctrl+Shift+F11** | Show cars: park the BeamNG car on a GTA car and keep it there (up to 4) |
| **Ctrl+Shift+F12** | View modes (for testing) |
| **D-pad down** | Swap the controller between GTA (aim, shoot) and BeamNG (drive) while driving |

Driving uses your own BeamNG controls and bindings (gears, clutch, drive modes).

## Tips
- **Pick a GTA car the same size as your BeamNG car.** GTA's traffic, people and props hit an invisible GTA car hidden under your BeamNG car. If the GTA car is smaller, things go into your BeamNG car before they react; if it's bigger, they react before they touch it. A trainer such as Menyoo makes it easy to spawn the right one (a sedan for a sedan, a jeep for a lifted truck).
- **Run BeamNG small and light.** Window mode at 1280x720 with the lowest graphics settings. You only see GTA's picture, so BeamNG's graphics are wasted video memory.
- **Start BeamNG first**, on Grid, Small, Pure, with only your car spawned.
- **The first drive in an area is slower.** GTA's ground is measured and built in BeamNG as you go. It's saved in `GtaBeamD1_map\` next to GTA, so later drives there are instant.
- **Controller:** a PlayStation pad works through DS4Windows.

## What works
- **Your BeamNG car in GTA,** drawn with its paint, materials, glass and lights, in GTA's light, shadows and reflections.
- **BeamNG's physics.** GTA's streets, hills, walls and ground surfaces are built in BeamNG, so the car grips, crumples and breaks the BeamNG way.
- **GTA's world reacts the vanilla way** (`PuppetSolid=1`, new in 2.77). GTA's cars get pushed and dented, people ragdoll, lamp posts break and bins fly, and your BeamNG car takes the hit.
- **Real damage on the car.** BeamNG's dents, broken glass, lost bumpers and wheels show in GTA.
- **GTA's guns.** Bullets dent the car, leave holes, pop tyres and break windows. Explosions push it and set it on fire; a car that blows up becomes a charred wreck.
- **On foot.** Get out, walk around the car, shoot it. Get back in and keep driving.
- **Show cars** (Ctrl+Shift+F11), **pause sync** (pausing one game pauses the other), **BeamNG's engine sound** and GTA's radio on the phone.

## Known issues
- Things react to GTA's hidden car, not the exact shape of your BeamNG car (see Tips). The parts of your BeamNG car that stick out past the GTA car crumple against walls BeamNG's way; GTA's cars, people and signs push your car and dent it at the contact point.
- With `PuppetSolid=1` you sit in the GTA car's seat in first person, not BeamNG's.
- Some hillsides and bridge decks can have holes or show up a few seconds late.
- The camera can lag a little at low speed.
- Show cars stand still and can look flat-lit or show through the car you drive.
- Door glass is missing on some cars. Tyre smoke is off (`TireSmoke=1` to try it).
- Other mods' extra objects on the car in BeamNG (like a DARS driver) aren't brought into GTA.

## Settings
Everything is in `GtaBeamD1.ini`, with comments. The ones you're most likely to want:
- `PuppetSolid=1` (default): GTA's world reacts to your car with GTA's own physics. `0` goes back to the 2.76 way (traffic you touch vanishes, set by `TrafficMode`).
- `Weapons=0` turns off weapon damage on the car.
- `DriveGravity` sets how heavy the car feels (1.1).
- `ShowCarsMax` (4) is how many show cars you can park; each one uses video memory.
- `DriverSeatForwardM`, `DriverSeatUpM`, `DriverSeatRightM` move you in the seat for every car.

Game content lives in JSON next to the ini: `GtaBeamD1_surfaces.json` (GTA ground → BeamNG grip), `GtaBeamD1_props.json` (props), `GtaBeamD1_weapons.json` (damage per weapon; `crashMinDv` is how hard a crash must be to hurt you), `GtaBeamD1_traffic.json` (masses).

## Bug reports
Please attach both logs:
- `GtaBeamD1.log` (GTA V folder)
- `gtabridge_log.txt` (`%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\gtabridge\`)

## Building from source
The source is in `source\`.
- Windows with Visual Studio 2022: run `source\build_vs2022.bat` (needs the Script Hook V SDK's `ScriptHookV.lib`).
- Linux/CI: `zig c++ -target x86_64-windows-gnu` (see `source\test\build_all.sh`). That folder also has the tests, with a fake Script Hook V and a fake BeamNG for runs under Wine. They test the code, not GTA's physics.

## Credits and licences
- [Script Hook V](http://www.dev-c.com/gtav/scripthookv/) by Alexander Blade.
- [cgltf](https://github.com/jkuhlmann/cgltf) (MIT), [stb_image](https://github.com/nothings/stb) (MIT / public domain), [bcdec](https://github.com/iOrange/bcdec) (MIT / Unlicense).
- Inspired by [SkyCraft](https://github.com/chasmlol/SkyCraft) (Skyrim x Minecraft passthrough).
- Licence texts are in `licences\`.
- No Rockstar or BeamNG game files are included. The mod reads your own installs at run time.
