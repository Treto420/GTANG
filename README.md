# GTA x Beam kit v2.5

Built 2026-10-08. Copy `Gta x Beam V2.43` to `Gta x Beam V2.5`, then extract this zip over it.

## What's new
1. **Crash physics, first pass.**
   - **Walls and barriers** are solid boxes in BeamNG now (jersey barriers, highway walls, building sides), not humps in the ground.
   - **Soft props.** Lamp posts, signs, bins, hydrants and small poles fall over in GTA when you hit them, and BeamNG's car loses a little speed. Big props (containers, bollards) are solid boxes in BeamNG.
   - **Dents.** The drawn car bends where BeamNG's car bends. A repair takes them out again. This needs a new export (Shift+F11).
2. **Bridges and double decks.**
   - Found the cause of the walls under bridges and the falls at speed. When a ray found nothing, the mod looked 20 m up and 50 m down. That grabbed the bridge above you or the road far below.
   - Now it only looks 4 m up and 12 m down from your level.
   - Up a ramp at speed, the ground ahead now follows the ramp's slope. Before, it was sometimes measured flat, under the ramp's top, and you dropped through.
   - A tile with nothing at your level stays empty instead of taking another level's ground. That is likely also your weird ground spots (screenshots 3 and 4).
3. **RB+LB+D-pad down.** GTA now sends the modifier buttons you hold, even ones you held through a pad-mode switch. Your BeamNG combos work in BeamNG pad mode. In GTA pad mode they stay GTA's.
4. **Gravity x1.1** while GTA drives (`DriveGravity` in the ini). It goes back to normal when the drive ends.
5. **Drive mode on the gauge:** COMFORT / SPORT / ESC & TC OFF, top right of the gauge.
6. **Radio wheel.** GTA's slow motion no longer slows the mod's clock, so the car stays in sync. Changing BeamNG's own speed (the radio wheel, Franklin) comes later.
7. **Pause.** Fixed a rare case where un-pausing GTA opened GTA's pause menu by itself and both games stayed paused.

## Install
1. From the zip's `GTA\` folder, copy all 4 files to `F:\Games\GTA V\Grand Theft Auto V Legacy\` (replace):
   - `GtaBeamD1.asi`
   - `GtaBeamD1.ini`
   - `GtaBeamD1_surfaces.json`
   - `GtaBeamD1_props.json` (new)
2. Copy `BeamNG\gtaBridge.lua` to `%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\mods\unpacked\gtabridge\lua\ge\extensions\gtaBridge.lua` (replace).
3. **Restart BeamNG.** Load **Grid, Small, Pure** and spawn your car.
4. **Export the car again.** In GTA, press Shift+F11. The dents need the 2.5 export.
5. Check the versions:
   - `GtaBeamD1.log` says 2.5.
   - BeamNG console (`~`): `gtaBridge.status()` says 2.5.
6. Optional, in the BeamNG console: `gtaBridge.dentsCheck()`. Its line in `gtabridge_log.txt` says how many nodes and flexbody groups the export will write.

Your saved map is measured once more, because walls are part of it now.

## Test (in this order)
**1. Bridges and double decks** (the main fix)
- Drive the double-deck spot, then over a bridge fast.
- PASS: no falls, and BeamNG's window shows no walls under the decks.

**2. Walls**
- Drive along a highway barrier and scrape it.
- PASS: BeamNG's window shows boxes along the barrier (not humps), and the car scrapes along them instead of riding over them.

**3. Soft props**
- Hit a lamp post, a sign and a bin.
- PASS: each falls over in GTA, and the car slows a little.

**4. Dents**
- Hit a wall at about 50 km/h. Then press RB+LB+D-pad down (repair).
- PASS: the GTA car shows the dent where BeamNG's car has it, and the repair takes it out.

**5. Your repair combo**
- In BeamNG pad mode, press RB+LB+D-pad down.
- PASS: the car is repaired, and the pad mode does not switch.

**6. Gravity and gauge**
- Drive around: is it less bouncy?
- Switch ESC modes: the gauge shows the new mode.

**7. Radio wheel**
- Hold the radio wheel while driving.
- PASS: no desync or rubber band.

## If something's off
- **Car snags on walls that aren't there:** set `WorldWalls=0`, then Ctrl+F11. Tell me where it happened.
- **A prop is handled wrong:**
  - Find its line in `GtaBeamD1_props_seen.txt`.
  - Add the model to `GtaBeamD1_props.json` as `soft`, `solid` or `ignore`.
  - Press Ctrl+F11.
- **Dents look wrong** (spikes, the wrong part bends): set `Dents=0` and send the logs.
- **Gravity feels off:** change `DriveGravity` (1.0 is BeamNG's own).

## Send back
- `GtaBeamD1.log`
- BeamNG's `gtabridge\gtabridge_log.txt`
- `GtaBeamD1_props_seen.txt` (next to the .asi)
- one line per test (PASS/FAIL + how it felt), and a short crash clip if you can

## Launcher (your question)
Yes, it's doable, but later (after crash physics):
- A `.bat` (or a small launcher) can start BeamNG and GTA.
- The mod can pause and minimize BeamNG until GTA's F10. The pause part already exists.

## Not checked yet (can't be tested here)
- BeamNG's gravity call (`core_environment.setGravity`). The log shows the value read back.
- The drive-mode name (`esc` controller, `getCurrentConfigData`). The log lists the controllers' functions once.
- The bump that slows the car (`applyClusterVelocityScaleAdd`).
- In the car's own Lua:
  - its direction (`obj:getDirectionVector`; the fallback is the car's ref nodes)
  - the flexbodies' node group field
  - Both show in `gtaBridge.dentsCheck()` and in the log.
- GTA side:
  - how hard props get pushed (`APPLY_FORCE_TO_ENTITY`)
  - the prop names in `GtaBeamD1_props.json`. A name that doesn't match is just unused.

## Known gaps
- BeamNG doesn't know about GTA's traffic yet.
- Dents count from the drive start. Damage the car had before isn't shown.
- Shading doesn't follow the dents yet.
- Water has no ground.
