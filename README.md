# GTA x Beam kit v2.43

Built 2026-10-06. Copy `Gta x Beam V2.42` to `Gta x Beam V2.43`, then extract this zip over it.

## What's fixed
1. **Bridges and highways.**
   - Every tile is now measured for the level the car is on, carried tile by tile along the way you drive: up a ramp onto a bridge, or along the road under it.
   - The ground far below a deck's edge is left out. No more big wall hanging under the bridge.
   - The cache and the saved map keep up to 3 levels of the same spot (a deck and the road under it). The one that fits your level is used.
   - Your v2.42 saved map is ignored (it mixed decks and roads). Those roads get measured once more, then saved again.
   - My test: a ramp up onto an 8 m deck. Drove under it, then up the ramp and over it, then under it again. The car stayed on the right surface every time (within 0.6 m), with no walls and no falls.
2. **Static world.**
   - Built ground stays in BeamNG after you drive past it, up to 30,000 triangles (fewer at speed) and within 400 m.
   - Standing still, ground already measured nearby goes to BeamNG too, 4 tiles a second.
   - Past that budget, the farthest tiles go first. They come back from the saved map later (no rays, but BeamNG rebuilds them).
3. **Your repair combo (RB+LB+X).**
   - BeamNG's reset / recover / home actions were blocked on purpose, because they move BeamNG's car away from the GTA world.
   - Now they repair the car where it is: upright, facing the same way, 0.5 m up.
4. **Pause.**
   - GTA paused or alt-tabbed: BeamNG's physics pause.
   - You pause BeamNG: GTA's pause menu opens. Unpause BeamNG: it closes.
5. **Map change crash.** The mod now stops all its work while BeamNG changes maps. (I can't confirm this was the cause; see test 6.)

## Install
1. From the zip's `GTA\` folder, copy `GtaBeamD1.asi`, `GtaBeamD1.ini` and `GtaBeamD1_surfaces.json` to `F:\Games\GTA V\Grand Theft Auto V Legacy\` (replace).
2. Copy `BeamNG\gtaBridge.lua` to `%LOCALAPPDATA%\BeamNG\BeamNG.drive\current\mods\unpacked\gtabridge\lua\ge\extensions\gtaBridge.lua` (replace).
3. **Restart BeamNG.** From the main menu, load **Grid, Small, Pure** and spawn your car. (This also turns on the concrete grip: no map reload needed.)
4. Check the versions:
   - `GtaBeamD1.log` says 2.43.
   - BeamNG console (`~`): `gtaBridge.status()` says 2.43.

## Test (in this order)
**1. Bridges** (the main test)
- Drive up onto the highway bridge that wouldn't let you on. Then drive under a bridge.
- PASS: you can drive on it, nothing falls through, and BeamNG's window shows no wall under the deck.

**2. Static world**
- Drive around a block twice.
- PASS: on the second lap, BeamNG's window shows no tiles being rebuilt around the block.

**3. Repair**
- Damage the car, then RB+LB+X.
- PASS: the car is repaired where it is and drives on.
- If nothing happens, send `gtabridge\bindings.txt`. Your binding's line should end with `as REPAIR WHERE IT IS (v2.43)`.

**4. Pause**
- Esc in GTA: BeamNG's car freezes. Esc again: it runs.
- Alt-tab out of GTA: same.
- In BeamNG's window, pause it yourself (J twice: the first J undoes the mod's pause, the second one is yours). Go back to GTA: its pause menu opens. Unpause BeamNG: GTA's menu closes.
- PASS: all three, and no jump or rubber band when it runs again.

**5. Concrete**
- BeamNG console (`~`): `gtaBridge.world()`.
- PASS: the concrete line says **BeamNG has it (grip x 0.85)**, and sidewalks grip a little less.

**6. Map change** (optional)
- In BeamNG, load another map from inside a map.
- PASS: no crash. If it still crashes, disable the gtabridge mod and try once more. Then tell me if it still crashes without it.

## If something's off
- **Wrong ground on a bridge:** set `WorldDebug=1` in the ini, Ctrl+F11, drive there once, and send `GtaBeamD1.log`.
- **BeamNG stutters with the static world:** lower `WorldKeepTris` (try 15000). More triangles = slower collision rebuilds (~0.2 ms per 1,000 triangles).
- **Want more of the world kept:** raise `WorldKeepTris` (try 60000).
- **Pause sync gets in the way:** `PauseSync=0` in the ini, Ctrl+F11.
- **Repair combo should stay BeamNG's own:** empty `BeamNGRepairActions=` in the ini.

## Send back
- `GtaBeamD1.log`
- BeamNG's `gtabridge\gtabridge_log.txt`
- a short clip of the bridge
- one line per test (PASS/FAIL + how it felt)

## Not checked yet (can't be tested here)
- BeamNG's pause function: `simTimeAuthority.pause` (another BeamNG mod uses it). The mod checks that BeamNG's physics clock really stops, and says so in its log.
- GTA's pause menu open/close from the plugin (`ACTIVATE_FRONTEND_MENU` / `SET_FRONTEND_ACTIVE`).

## Known gaps (not faked, on purpose)
- Walls, barriers and props are still ramps in the ground. Breakables come with v2.5.
- Water has no ground: the car falls in and is put back.
- BeamNG doesn't know about GTA's traffic yet.
