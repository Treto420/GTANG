# GTA x Beam kit v2.55

Built 2026-10-09. Copy `Gta x Beam V2.54` to `Gta x Beam V2.55`, then extract this zip over it.

## What's new
1. **Dents: last touch-up.**
   - **Steering wheel, mirrors, antenna, lights.** BeamNG carries these "props" rigidly on 3 nodes. 2.54 stretched them with those nodes instead.
     - The 3 nodes sit close together, while the wheel and the antenna reach far from them, so a few mm of node movement got blown up many times.
     - That is the giant steering wheel (screenshot 2) and the lines shooting into the sky (screenshot 1).
     - They now move and turn with their nodes but never change size.
   - Any other point of a panel that sits far from its 3 nodes is carried the same rigid way. That's less roughness where a few nodes hold a big piece.
   - **Torn pieces** are now also hidden when they are thin on screen. 2.54 could only measure wide ones; the lines got past it.
2. **Holes in the hill roads (screenshots 5 and 6).**
   - Where GTA's road has a crack, drain or pothole, the measuring ray slipped through to the ground under it. That left a narrow pit in BeamNG's road.
   - Pits up to about 2 m wide and over 15 cm deep are now filled to the road around them.
   - Your saved map gets the same fix when it's read, so no re-measuring is needed.
3. **The bridge wall (screenshots 3 and 4).**
   - The tall pale panels in BeamNG's picture are its *ground*, not wall boxes: the rays hit the bridge's beams before the road.
   - Anything sticking up over 1 m and less than about 2 m wide is now lowered to the road around it.
   - Real walls and barriers aren't affected: they are found by their own rays.
   - On a test road with a 3 m tall, 0.5 m wide beam across it every 25 m, 2.54 fell through 3 times and never got past the first one. 2.55 drives straight through.

## Install
1. From `GTA\`, copy all 4 files to `F:\Games\GTA V\Grand Theft Auto V Legacy\` (replace).
2. **Only the GTA files changed.**
   - The BeamNG mod stays 2.53.
   - No new car export is needed.
3. Check `GtaBeamD1.log`:
   - It says 2.55.
   - The `car dents (v2.55)` line now has `N carried rigidly (props, ...)`.
   - The `ground: levels:` lines now end with `points lifted out of narrow pits, ... lowered from narrow spikes`.

## Test (in this order)
**1. The bridge from screenshot 3** (main)
- Drive onto it and across.
- PASS: no wall. In BeamNG, no tall panels on the road.

**2. Steering wheel:** look inside the car.
- PASS: normal size.

**3. Dents:** a hard crash.
- PASS: no lines to the sky, no stretched sheets, nothing blown up in size.

**4. Hill roads from screenshots 5 and 6.**
- PASS: no holes in BeamNG's road.

## Send back
- **Always send `GtaBeamD1.log` and BeamNG's `gtabridge\gtabridge_log.txt`.**
  - If the bridge wall is still there, the log's `crash:` line says what BeamNG had there. That's the fastest way to the fix.
- One line per test (PASS/FAIL), plus screenshot pairs.

## Not checked yet (can't be tested here)
- **Whether the bridge panels were beams.** That's my read of your screenshot 4: pale like the ground, many thin panels side by side. If the wall stays, the log decides.
- **Your steering wheel's exact setup.** If it's a prop it's fixed. If it's a part on a few nodes, the "far from its nodes" rule covers it.

## Known gaps
- Glass and lights that BeamNG hides when they break still show in GTA.
- Physics props: next.
- Traffic (with pedestrians and motorcycles) is a later phase.
