-- gtaBridge.lua  (BeamNG side of the GTA <-> BeamNG bridge)
-- Phase 1, T0 + T1:
--   T0: UDP ping/pong with the sender (GTA script later, Python test sender now)
--   T1: build drivable static collision at runtime from a list of points/triangles
--   v1.3: collision is reloaded once per frame (not per surface); per-frame cost is measured
--   v1.4: surfaces are queued and built within a per-frame time budget (no big hitches on bursts)
--   v1.6: D1 car look test: "export" message / gtaBridge.exportCar() writes the player car as glTF
--         (BeamNG's own exporter) for the GTA plugin to draw
--   v1.7: full material layers + paint palette, own texture files, wheel axes; results also go to
--         on-screen messages and <user folder>/gtabridge/gtabridge_log.txt (console spam from other
--         mods can't hide them any more)
--   v1.8: named colours (White, black, red, InvisibleBlack...), texture paths without an extension,
--         surface maps listed in materials.txt
--   v1.9: normal, metallic and roughness maps exported (and their UV sets); each material's
--         translucentBlendOp and cubemap (BeamNG's interior glass uses a black one = no reflections);
--         which wheels are driven (from the car's powertrain) for burnout wheel spin in GTA
--   v1.9.1: live UI textures (screens, gauges: texture paths starting with '@') are flagged as
--         baseColorLive so the GTA plugin draws them as dark glossy screens instead of skipping them
--   v1.9.3: .dds textures are copied as they are (no PNG conversion): the GTA plugin 1.9.3 loads them
--         block-compressed (full quality, less video memory, much faster export).
--         gtaBridge.setTextureMode('png') brings back the old conversion.
--   v1.9.6: the paints' metallic / roughness / clear coat / clear coat roughness go into the car file
--         (BeamNG multiplies them into paint layers; without them the GTA plugin drew rough bare metal).
--   v2.0: drive mode (T2): "drive_start" hands the player car to GTA's controls (input.event in its
--         vehicle Lua); its pose (position, direction, velocity, wheel hubs + turn) goes back every frame.
--         "drive_stop" (or 0.5 s without controls) = throttle off, parking brake on.
--   v2.01: YOUR BeamNG controls drive the car. The GTA plugin reads your pad and keyboard itself and
--         sends their raw state; this mod looks them up in your own BeamNG bindings (Options > Controls)
--         and runs each bound action's own code, exactly what BeamNG does when you press it (clutch,
--         shifting, drive modes, lights...). BeamNG's own reading of your devices is blocked meanwhile
--         (no double presses). The car's throttle/brake/gear/rpm go back to GTA (on-screen check).
--         gtaBridge.binds() writes what it found to <user folder>/gtabridge/bindings.txt.
--   v2.2: sound switch uses BeamNG's real setting (AudioMuteOnWindowLoseFocus) and puts back exactly what
--         was there; optional background fps limit while driving (GTA's BeamNGBackgroundFps); the
--         background fps settings are logged at drive start; action categories matched exactly (debug
--         categories like vehicle_debug never forwarded: D-pad right reloaded the car), reload_vehicle /
--         reload_all_vehicles never forwarded.
--   v2.3: drive_ok says when BeamNG was muted in the background ("sw": the GTA plugin then brings BeamNG's window
--         to the front for a moment so BeamNG notices its option changed); "act" runs one of your BeamNG actions by
--         name (GTA's PadGtaActions: D-pad right = toggle_headlights); the car's lights go to GTA in "vs" ("li");
--         the log file is not written while GTA drives (a file write every 10 s may have been the rubber band).
--   v2.4: T3, GTA's ground in BeamNG. "map" sets the fixed GTA -> BeamNG mapping (BeamNG = GTA - origin, + 200 m up);
--         "hf" height tiles (GTA's ground, measured by the GTA plugin) are built as ProceduralMesh tiles, one sub-mesh per
--         surface, each with a material whose groundType is the BeamNG ground model (ASPHALT, DIRT, GRASS...) GTA's
--         ground material maps to (GtaBeamD1_surfaces.json on the GTA side); "rm" removes tiles; "tp" puts the car on
--         GTA's ground at the GTA car's spot (after the tiles under it are built; the rotation convention is checked by
--         reading the car back). At drive end a flat pad under the car keeps it from falling off the last tiles.
--         Sound: "session" mode (the mute option off for the whole BeamNG session, BeamNG's volume down while GTA
--         isn't driving). Poses carry BeamNG's simulation clock too ("ss", measurement only). The car state ("vs")
--         carries the damage and the ground ids under the wheels.
--   v2.51: dents work (the export wrote one position per node instead of three: GTA couldn't read them); props are boxes in
--         BeamNG your car really hits ("pb": breakaway ones break when the car has lost their "break" speed against them, GTA
--         then knocks them over: "pbk"); reload_vehicle can be your repair where the car is; combinations you press are logged;
--         flat ground cells whose edges don't meet their neighbours' points are built finely (no small steps: less bumping).
--   v2.6: GTA's weapons and explosions on BeamNG's car: "hit" (a bullet's line through the drawn car: a dent where it meets the car,
--         tyres pop, glass and lights break, engine and radiator damage, the engine catches fire after enough hits), "blast" (an
--         explosion: the car pushed from where it went off, maybe set on fire, the glass near it broken); the car's fire, its
--         explosion after burning a while, its broken glass / lights go back to GTA ("fx"). The export writes each mesh's
--         deformGroup ("dg") so GTA hides / frosts the glass BeamNG broke (export the car again: Shift+F11).
--
-- Install: <BeamNG user folder>/mods/unpacked/gtabridge/lua/ge/extensions/gtaBridge.lua
-- Console helpers (press ~):
--   gtaBridge.status()        -- prints what worked (socket lib, mesh API)
--   gtaBridge.binds()         -- v2.01: which of your BeamNG bindings the bridge uses -> gtabridge/bindings.txt
--   gtaBridge.unblock()       -- v2.01: give BeamNG its own controls back now (if it ever stays blocked)
--   gtaBridge.exportCar()     -- D1: export your current car to <user folder>/gtabridge/car.gltf
--   gtaBridge.exportStatus()  -- D1: result of the last export (path, size, timing)
--   gtaBridge.matCheck()      -- material report for your car -> <user folder>/gtabridge/materials.txt
--   gtaBridge.setTextureMode('png') -- v1.9.3: convert textures to PNG again ('dds' = copy them as they are, default)
--   gtaBridge.selfTest()      -- builds ramp/curb/wall/bumpy patch ahead of your car, no network needed
--   gtaBridge.clear()         -- removes everything this mod spawned
--   gtaBridge.greenRoom()     -- overlay test: green floor + walls around your car (greenRoom(false) removes)
--   gtaBridge.camOff()        -- give the camera back after the overlay test
--   gtaBridge.setCamOffset(x,y,z) / setFovMode('horizontal') / setFovScale(1.1) / flipForward(true)
--   gtaBridge.surfaceTest()   -- v2.4: 4 strips ahead of your car (asphalt, dirt, grass, sand) to feel the ground models
--   gtaBridge.world()         -- v2.4: GTA ground tiles: count, ground models, tile mode, teleport convention
--   gtaBridge.soundSession(false) -- v2.4: switch the sound "session" mode off (the GTA plugin's BeamNGSoundWake=session sets it)

local M = {}
M.version = '2.74.1'
local logTag = 'gtaBridge'

-- ---------------------------------------------------------------- config
local cfg = {
  listenIp   = '127.0.0.1',
  listenPort = 47011,      -- BeamNG listens here
  maxPacketsPerFrame = 512,
  maxDatagram = 8192,      -- luasocket's safe UDP size; bigger messages arrive fragmented
  material   = 'track_editor_A_center', -- any existing material works; missing one = checker texture
  -- BeamNG (Torque) treats CLOCKWISE triangles as front faces. Senders (GTA script, test tool) use the
  -- usual counter-clockwise order, so we flip here. v1.1 didn't, which made every surface face inward.
  flipWinding = true,
  fragTimeout = 1.0,       -- seconds before an incomplete fragmented message is dropped
  buildBudgetMs = 3.5,     -- max time per frame spent decoding+building queued surfaces (at least 1 per frame)
}

-- ---------------------------------------------------------------- state
local socketLib, socketLibName
local udp
local peerIp, peerPort
local surfaces = {}        -- id -> scene object
local surfaceTris = {}     -- id -> triangle count
local collisionDirty = false   -- set by builds/removals; collision is reloaded ONCE per frame
local frags = {}           -- msgId -> {count, got, parts, t}
local stats = { rx = 0, tx = 0, pings = 0, builds = 0, lastBuildMs = 0, maxBuildMs = 0, errors = 0,
                reloads = 0, lastReloadMs = 0, maxReloadMs = 0, sumReloadMs = 0,
                lastFrameMs = 0, maxFrameMs = 0, windowMaxFrameMs = 0,
                maxQueue = 0, lastWaitMs = 0, maxWaitMs = 0, cancelled = 0, avgBuildMs = 1.5 }
local caps = { socket = 'none', mesh = 'untested', collisionReload = 'untested' }

-- Everything we log also goes to <user folder>/gtabridge/gtabridge_log.txt (last 1000 lines; v2.43: was 300),
-- so other mods spamming the console can't hide it.
local logLines, logDirty, logFlushedAt = {}, false, 0
local function record(level, msg)
  local okT, t = pcall(os.date, '%H:%M:%S')
  logLines[#logLines + 1] = string.format('[%s] %s %s', okT and t or '?', level, msg)
  if #logLines > 1000 then table.remove(logLines, 1) end
  logDirty = true
end
local function logI(...) local m = table.concat({...}, ' '); log('I', logTag, m); record('I', m) end
local function logE(...) stats.errors = stats.errors + 1; local m = table.concat({...}, ' '); log('E', logTag, m); record('E', m) end
-- on-screen message (BeamNG's UI toast), readable whatever the console is doing
local function screen(msg, secs)
  pcall(function() guihooks.message(msg, secs or 8, 'gtaBridge') end)
end

-- ---------------------------------------------------------------- time
local function now()
  if socketLib and socketLib.gettime then return socketLib.gettime() end
  return os.clock()
end

local driveActive = false   -- v2.3: set while GTA drives (no log file writes then, except every 60 s)
local function flushLog(force)
  if logDirty and (force or now() - logFlushedAt > (driveActive and 60 or 1)) then
    pcall(writeFile, '/gtabridge/gtabridge_log.txt', table.concat(logLines, '\n') .. '\n')
    logDirty, logFlushedAt = false, now()
  end
end

-- ---------------------------------------------------------------- socket discovery
-- BeamNG ships LuaSocket, but the require path is not documented. Try the likely ones.
local function findSocket()
  local candidates = {
    { 'global socket', function() return rawget(_G, 'socket') end },
    { 'socket',                         function() return require('socket') end },
    { 'socket.socket',                  function() return require('socket.socket') end },
    { 'libs/luasocket/socket.socket',   function() return require('libs/luasocket/socket.socket') end },
    { 'luasocket/socket.socket',        function() return require('luasocket/socket.socket') end },
    { 'socket.core',                    function() return require('socket.core') end },
  }
  for _, c in ipairs(candidates) do
    local ok, lib = pcall(c[2])
    if ok and type(lib) == 'table' and lib.udp then
      return lib, c[1]
    end
  end
  return nil
end

local function openSocket()
  socketLib, socketLibName = findSocket()
  if not socketLib then
    caps.socket = 'NOT FOUND'
    logE('LuaSocket not found. Send the "socket" require line from lua/vehicle/protocols/outgauge.lua in your install.')
    return
  end
  caps.socket = socketLibName
  udp = socketLib.udp()
  local ok, err = udp:setsockname(cfg.listenIp, cfg.listenPort)
  if not ok then
    logE('bind failed on', cfg.listenIp .. ':' .. cfg.listenPort, tostring(err))
    caps.socket = socketLibName .. ' (BIND FAILED: ' .. tostring(err) .. ')'
    udp:close(); udp = nil
    return
  end
  udp:settimeout(0)
  logI('listening on', cfg.listenIp .. ':' .. cfg.listenPort, 'using', socketLibName)
end

local function send(tbl)
  if not (udp and peerIp) then return end
  local s = jsonEncode(tbl)
  udp:sendto(s, peerIp, peerPort)
  stats.tx = stats.tx + 1
end

-- ---------------------------------------------------------------- math helpers
local function sub(a, b) return { a[1]-b[1], a[2]-b[2], a[3]-b[3] } end
local function cross(a, b) return { a[2]*b[3]-a[3]*b[2], a[3]*b[1]-a[1]*b[3], a[1]*b[2]-a[2]*b[1] } end
local function norm(a)
  local l = math.sqrt(a[1]*a[1] + a[2]*a[2] + a[3]*a[3])
  if l < 1e-9 then return { 0, 0, 1 } end
  return { a[1]/l, a[2]/l, a[3]/l }
end

-- ---------------------------------------------------------------- player car frame (for test shapes)
local function getPlayerVeh()
  local ok, v = pcall(function() return getPlayerVehicle and getPlayerVehicle(0) end)
  if ok and v then return v end
  ok, v = pcall(function() return be:getPlayerVehicle(0) end)
  if ok and v then return v end
  return nil
end

-- ground height straight below a point, ignoring our own surfaces is not possible, so call this
-- BEFORE spawning anything under the car. Returns z and which method worked.
local function groundBelow(x, y, zStart)
  local ok, h = pcall(function() return be:getSurfaceHeightBelow(vec3(x, y, zStart + 2)) end)
  if ok and type(h) == 'number' and h > -1e6 then return h, 'getSurfaceHeightBelow' end
  ok, h = pcall(function() return castRayStatic(vec3(x, y, zStart + 2), vec3(0, 0, -1), 50) end)
  if ok and type(h) == 'number' and h < 50 then return zStart + 2 - h, 'castRayStatic' end
  return nil
end

-- returns origin (x,y,z = ground under the car) and yaw basis (fwd, right) flattened to the ground plane
local function carFrame()
  local v = getPlayerVeh()
  if not v then return { 0, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 } end
  local p = v:getPosition()
  local d = v:getDirectionVector()
  local f = norm({ d.x, d.y, 0 })
  local r = { f[2], -f[1], 0 }
  local z, how = groundBelow(p.x, p.y, p.z)
  if not z then z, how = p.z - 0.5, 'GUESS (refnode - 0.5 m)' end
  caps.ground = string.format('%s, %.2f m below car ref', how, p.z - z)
  return { p.x, p.y, z }, f, r
end

-- ---------------------------------------------------------------- mesh building
-- surface message:
--   { t="surf", id="name", verts={x,y,z, x,y,z,...}, tris={i0,i1,i2,...} (0-based),
--     rel=true|false (true = verts are car-relative: x=right, y=forward, z=up from ground under car),
--     material="optional" }
-- 'anchor' = car frame frozen at the last {t="anchor"} message, so a surface stays put in the
-- world while being rebuilt (true = re-read the car frame on every call, which makes it MOVE with the car)
local anchorFrame
local function toWorld(verts, rel)
  local out = {}
  if rel then
    local o, f, r
    if rel == 'anchor' then
      if not anchorFrame then anchorFrame = { carFrame() } end
      o, f, r = anchorFrame[1], anchorFrame[2], anchorFrame[3]
    else
      o, f, r = carFrame()
    end
    for i = 1, #verts, 3 do
      local x, y, z = verts[i], verts[i+1], verts[i+2]
      out[#out+1] = { o[1] + r[1]*x + f[1]*y, o[2] + r[2]*x + f[2]*y, o[3] + z }
    end
  else
    for i = 1, #verts, 3 do out[#out+1] = { verts[i], verts[i+1], verts[i+2] } end
  end
  return out
end

local worldTiles = {}   -- v2.4: id -> GTA ground tile info (see "T3" below)
local worldDriveEnded     -- v2.4: set in the T3 section (forward declarations)
local buildTileMsg
local dents = {}          -- v2.5 (D2): dents shown in GTA (filled in the T3 section)
local wpn = {}            -- v2.6: GTA's weapons and explosions on BeamNG's car (filled after the dents)
local function removeSurface(id)
  local obj = surfaces[id]
  if obj then
    if type(obj) == 'table' and obj.objs then
      -- v2.4: a tile built as one object per surface ("split" tile mode)
      for _, o in ipairs(obj.objs) do pcall(function() o:delete() end) end
    else
      pcall(function() obj:delete() end)
    end
    surfaces[id] = nil
    surfaceTris[id] = nil
    collisionDirty = true
  end
  worldTiles[id] = nil
end

local function liveTris()
  local n = 0
  for _, c in pairs(surfaceTris) do n = n + c end
  return n
end

local function reloadCollisionNow()
  -- Static collision must be rebuilt after adding/removing static geometry.
  local t0 = now()
  local ok = pcall(function() be:reloadStaticCollision(false) end)
  if ok then caps.collisionReload = 'be:reloadStaticCollision(false)'
  else
    ok = pcall(function() be:reloadCollision() end)
    if ok then caps.collisionReload = 'be:reloadCollision()'
    else caps.collisionReload = 'FAILED'; logE('no collision reload function worked') end
  end
  collisionDirty = false
  local ms = (now() - t0) * 1000
  stats.reloads = stats.reloads + 1
  stats.lastReloadMs = ms
  stats.sumReloadMs = stats.sumReloadMs + ms
  if ms > stats.maxReloadMs then stats.maxReloadMs = ms end
  stats.lastReloadT = now()
  stats.wReloads = (stats.wReloads or 0) + 1   -- v2.4: per "ws" window (1 s)
  stats.wReloadSum = (stats.wReloadSum or 0) + ms
  stats.wReloadMax = math.max(stats.wReloadMax or 0, ms)
  return ok
end

-- v1.3: don't reload per surface. Mark dirty; onUpdate reloads once after handling all packets.
local function reloadCollision() collisionDirty = true end

-- Build one ProceduralMesh. Vertices are stored relative to the object position (first vertex)
-- to keep float precision good when GTA coordinates get large.
local function buildSurface(id, worldVerts, tris, material)
  local t0 = now()
  if #worldVerts < 3 or #tris < 3 then return false, 'empty surface' end

  local origin = worldVerts[1]
  local verts, normals, uvs, faces = {}, {}, {}, {}
  for i, w in ipairs(worldVerts) do
    verts[i] = { x = w[1]-origin[1], y = w[2]-origin[2], z = w[3]-origin[3] }
    uvs[i]   = { u = w[1] * 0.25, v = w[2] * 0.25 }
  end
  -- one flat normal per triangle (good enough for collision; smooth shading is not needed)
  local ni = 0
  for k = 1, #tris, 3 do
    local a, b, c = tris[k], tris[k+1], tris[k+2]
    local A, B, C = worldVerts[a+1], worldVerts[b+1], worldVerts[c+1]
    if not (A and B and C) then return false, 'triangle index out of range at ' .. k end
    local n = norm(cross(sub(B, A), sub(C, A)))
    normals[#normals+1] = { x = n[1], y = n[2], z = n[3] }
    if cfg.flipWinding then b, c = c, b end
    faces[#faces+1] = { v = a, n = ni, u = a }
    faces[#faces+1] = { v = b, n = ni, u = b }
    faces[#faces+1] = { v = c, n = ni, u = c }
    ni = ni + 1
  end

  local mesh = { verts = verts, uvs = uvs, normals = normals, faces = faces, material = material or cfg.material }

  removeSurface(id)
  local name = 'gtab_surf_' .. tostring(id)
  local ok, err = pcall(function()
    local obj = createObject('ProceduralMesh')
    if not obj then error('createObject("ProceduralMesh") returned nil') end
    obj:setPosition(vec3(origin[1], origin[2], origin[3]))
    obj.canSave = false
    obj:registerObject(name)
    local added = pcall(function() scenetree.MissionGroup:addObject(obj) end)
    if not added then scenetree.MissionGroup:addObject(obj.obj) end
    obj:createMesh({ { mesh } })
    surfaces[id] = obj
    surfaceTris[id] = #tris / 3
  end)
  if not ok then
    caps.mesh = 'FAILED: ' .. tostring(err)
    logE('ProceduralMesh build failed:', tostring(err))
    return false, tostring(err)
  end
  caps.mesh = 'ProceduralMesh:createMesh OK'
  reloadCollision()

  local ms = (now() - t0) * 1000
  stats.builds = stats.builds + 1
  stats.lastBuildMs = ms
  if ms > stats.maxBuildMs then stats.maxBuildMs = ms end
  return true, ms
end

-- ---------------------------------------------------------------- test shapes (car-relative, meters)
-- x = right, y = forward, z = up
local function addBox(verts, tris, cx, cy, cz, sx, sy, sz, pitch)
  -- axis-aligned box in car frame, optional pitch (radians) around the x axis (for ramps)
  local base = #verts / 3
  local hx, hy, hz = sx/2, sy/2, sz/2
  local cp, sp = math.cos(pitch or 0), math.sin(pitch or 0)
  local corners = {
    {-hx,-hy,-hz},{ hx,-hy,-hz},{ hx, hy,-hz},{-hx, hy,-hz},
    {-hx,-hy, hz},{ hx,-hy, hz},{ hx, hy, hz},{-hx, hy, hz},
  }
  for _, c in ipairs(corners) do
    local y = c[2]*cp - c[3]*sp
    local z = c[2]*sp + c[3]*cp
    verts[#verts+1] = cx + c[1]; verts[#verts+1] = cy + y; verts[#verts+1] = cz + z
  end
  local q = { {0,2,1},{0,3,2}, {4,5,6},{4,6,7}, {0,1,5},{0,5,4}, {1,2,6},{1,6,5}, {2,3,7},{2,7,6}, {3,0,4},{3,4,7} }
  for _, t in ipairs(q) do
    tris[#tris+1] = base + t[1]; tris[#tris+1] = base + t[2]; tris[#tris+1] = base + t[3]
  end
end

local function bumpyPatch(size, step, ox, oy, phase)
  local verts, tris = {}, {}
  local n = math.floor(size / step) + 1
  for j = 0, n-1 do
    for i = 0, n-1 do
      local x, y = ox + i*step - size/2, oy + j*step
      local z = 0.15 * math.sin(x*0.6 + (phase or 0)) * math.cos(y*0.5)
      verts[#verts+1] = x; verts[#verts+1] = y; verts[#verts+1] = z
    end
  end
  for j = 0, n-2 do
    for i = 0, n-2 do
      local a = j*n + i
      tris[#tris+1] = a;   tris[#tris+1] = a+1;   tris[#tris+1] = a+n+1
      tris[#tris+1] = a;   tris[#tris+1] = a+n+1; tris[#tris+1] = a+n
    end
  end
  return verts, tris
end

function M.selfTest()
  anchorFrame = { carFrame() }   -- measure the ground once, before anything is spawned
  local v, t
  -- ramp: 4 m wide, 8 m long, ~15 degrees, starts 12 m ahead
  v, t = {}, {}; addBox(v, t, 0, 16, 0.75, 4, 8.3, 0.4, math.rad(15))
  buildSurface('ramp', toWorld(v, 'anchor'), t)
  -- curb: 15 cm tall, 30 cm wide, 30 m long, 3 m to the right
  v, t = {}, {}; addBox(v, t, 3, 15, 0.075, 0.3, 30, 0.15)
  buildSurface('curb', toWorld(v, 'anchor'), t)
  -- wall: 2 m tall, 0.5 m thick, 30 m long, 6 m to the left
  v, t = {}, {}; addBox(v, t, -6, 15, 1.0, 0.5, 30, 2.0)
  buildSurface('wall', toWorld(v, 'anchor'), t)
  -- bumpy patch (simulated GTA probe grid), 20x20 m at 1 m spacing, 8 m to the right
  v, t = bumpyPatch(20, 1, 12, 5, 0)
  buildSurface('patch', toWorld(v, 'anchor'), t)
  M.status()
end

-- ---------------------------------------------------------------- overlay test: camera mirror
-- GTA sends its gameplay camera relative to its car (x right, y forward, z up).
-- We put BeamNG's camera at the same spot relative to the BeamNG car, so OBS can layer the
-- BeamNG car (on a green room) over the GTA picture.
local camCfg = {
  offset = { 0, 0, 0 },    -- car-local shift from GTA car origin to BeamNG ref node (calibrate per car)
  fovMode = 'vertical',     -- GTA gives vertical FOV; switch to 'horizontal' if BeamNG looks zoomed wrong
  fovScale = 1.0,
  flipForward = false,      -- set true if the camera ends up facing backwards
}
local camState = { last = nil, freeSet = false }
caps.cam = 'untested'

local function setFreeCam()
  if camState.freeSet then return end
  local ok = pcall(function() core_camera.setByName(0, 'free') end)
  if not ok then ok = pcall(function() commands.setFreeCamera() end) end
  camState.freeSet = ok
end

local function setCamPose(pos, fwd, up, fovDeg)
  local results = {}
  -- method 1: core_camera + quatFromDir
  local ok1 = pcall(function()
    local q = quatFromDir(vec3(fwd[1], fwd[2], fwd[3]), vec3(up[1], up[2], up[3]))
    core_camera.setPosition(0, vec3(pos[1], pos[2], pos[3]))
    core_camera.setRotation(0, q)
  end)
  if ok1 then results[#results+1] = 'core_camera.setPosition/Rotation' else
    -- method 2: global setCameraPosRot
    local ok2 = pcall(function()
      local q = quatFromDir(vec3(fwd[1], fwd[2], fwd[3]), vec3(up[1], up[2], up[3]))
      setCameraPosRot(pos[1], pos[2], pos[3], q.x, q.y, q.z, q.w)
    end)
    if ok2 then results[#results+1] = 'setCameraPosRot' else results[#results+1] = 'POSE FAILED' end
  end
  local okF = pcall(function() core_camera.setFOV(0, fovDeg) end)
  if not okF then okF = pcall(function() setCameraFovDeg(fovDeg) end) end
  results[#results+1] = okF and 'fov ok' or 'FOV FAILED'
  caps.cam = table.concat(results, ', ')
end

local function applyCam()
  local m = camState.last
  if not m then return end
  local v = getPlayerVeh()
  if not v then return end
  setFreeCam()
  local p = v:getPosition()
  local d = v:getDirectionVector()
  local u = v:getDirectionVectorUp()
  local f = norm({ d.x, d.y, d.z })
  if camCfg.flipForward then f = { -f[1], -f[2], -f[3] } end
  local up = norm({ u.x, u.y, u.z })
  local r = norm(cross(f, up))
  local function toWorldDir(a)
    return { r[1]*a[1] + f[1]*a[2] + up[1]*a[3], r[2]*a[1] + f[2]*a[2] + up[2]*a[3], r[3]*a[1] + f[3]*a[2] + up[3]*a[3] }
  end
  local o = camCfg.offset
  local lp = { m.p[1] + o[1], m.p[2] + o[2], m.p[3] + o[3] }
  local w = toWorldDir(lp)
  local pos = { p.x + w[1], p.y + w[2], p.z + w[3] }
  local fov = m.fov or 50
  if camCfg.fovMode == 'horizontal' and m.w and m.h and m.h > 0 then
    fov = math.deg(2 * math.atan(math.tan(math.rad(fov) / 2) * (m.w / m.h)))
  end
  setCamPose(pos, norm(toWorldDir(m.f)), norm(toWorldDir(m.u)), fov * camCfg.fovScale)
end

local camErrLogged = false
local function safeApplyCam()
  local ok, err = pcall(applyCam)
  if not ok and not camErrLogged then camErrLogged = true; caps.cam = 'ERROR: ' .. tostring(err); logE('camera mirror failed:', tostring(err)) end
end

function M.camOff()
  camState.last = nil; camState.freeSet = false
  pcall(function() core_camera.setByName(0, 'orbit') end)
end
function M.setCamOffset(x, y, z) camCfg.offset = { x or 0, y or 0, z or 0 }; logI('cam offset', x, y, z) end
function M.setFovMode(mode) camCfg.fovMode = mode; logI('fov mode', mode) end
function M.setFovScale(s) camCfg.fovScale = s; logI('fov scale', s) end
function M.setBuildBudget(ms) cfg.buildBudgetMs = ms; logI('build budget', ms, 'ms') end
function M.setFlipWinding(b) cfg.flipWinding = b and true or false; logI('flipWinding', tostring(cfg.flipWinding)) end
function M.flipForward(b) camCfg.flipForward = b and true or false end

-- ---------------------------------------------------------------- overlay test: green room
-- A flat green floor (drivable) and tall green walls around the car, no ceiling (keeps sunlight).
local function greenMaterial()
  local name = 'gtab_green'
  local ok = pcall(function()
    if scenetree.findObject(name) then return end
    local mat = createObject('Material')
    mat:setField('diffuseColor', 0, '0 1 0 1')
    mat:setField('emissive', 0, '1')
    mat:registerObject(name)
  end)
  caps.greenMaterial = ok and name or 'FAILED (using default material)'
  return ok and name or nil
end

function M.greenRoom(on)
  if on == false then
    removeSurface('green_room'); reloadCollision(); return
  end
  local half, wallH, floorZ = 60, 30, 0.02
  local v, t = {}, {}
  -- one-sided faces that point at the room centre: floor faces up, walls face inward.
  -- (v1.2 made them two-sided; a floor with an up AND a down face can crush the car.)
  local centre = { 0, 0, wallH / 2 }
  local function quad(a, b, c, d)
    local base = #v / 3
    for _, q in ipairs({ a, b, c, d }) do v[#v+1] = q[1]; v[#v+1] = q[2]; v[#v+1] = q[3] end
    local n = cross(sub(b, a), sub(c, a))
    local mid = { (a[1]+c[1])/2, (a[2]+c[2])/2, (a[3]+c[3])/2 }
    local toC = sub(centre, mid)
    local facesIn = n[1]*toC[1] + n[2]*toC[2] + n[3]*toC[3] > 0
    local order = facesIn and { {0,1,2},{0,2,3} } or { {0,2,1},{0,3,2} }
    for _, tri in ipairs(order) do
      t[#t+1] = base + tri[1]; t[#t+1] = base + tri[2]; t[#t+1] = base + tri[3]
    end
  end
  local h = half
  quad({-h,-h,floorZ},{h,-h,floorZ},{h,h,floorZ},{-h,h,floorZ})              -- floor
  quad({-h,-h,0},{h,-h,0},{h,-h,wallH},{-h,-h,wallH})                       -- walls
  quad({h,-h,0},{h,h,0},{h,h,wallH},{h,-h,wallH})
  quad({h,h,0},{-h,h,0},{-h,h,wallH},{h,h,wallH})
  quad({-h,h,0},{-h,-h,0},{-h,-h,wallH},{-h,h,wallH})
  local ok, res = buildSurface('green_room', toWorld(v, true), t, greenMaterial())
  logI('green room', ok and 'built' or ('FAILED ' .. tostring(res)), 'material:', tostring(caps.greenMaterial))
end

-- ---------------------------------------------------------------- D1: car export for the GTA plugin
-- Uses BeamNG's own exporter (lua/ge/extensions/util/export.lua, extension util_export). It grabs
-- the car's GPU mesh, i.e. exactly what BeamNG draws, so mod parts (rims, tyres) come with it.
-- Notes from reading export.lua (BeamNG.drive 0.39):
--   * texture coordinates are OFF by default (exportTexCoords = false); we switch them on
--   * the module caches textures between runs (savedImages), so we reload it before each export
--   * its .glb writer never closes the file, so we write a .gltf ourselves from the handler
--   * the export finishes in the same frame or a later one (util_export.updateGFX)
--   * v1.7: it exports texture images only, never colour values (baseColorFactor) or the paint
--     palette setup, and only the first material layer. So the mod now reads every material
--     layer itself (BeamNG docs: layer 1 = paint via colorPaletteMap R/G/B -> paint 1/2/3,
--     layer 2 = details masked by opacityMap), converts the textures it needs to PNG files in
--     /gtabridge/tex/, and drops the exporter's embedded images (smaller file).
local exportCfg = {
  vpath = '/gtabridge/car.gltf',   -- BeamNG virtual path (inside the BeamNG user folder)
  texDir = '/gtabridge/tex/',      -- converted textures (kept between exports as a cache)
  matDump = '/gtabridge/materials.txt',
  timeout = 240,                   -- seconds
  textures = 'dds',                -- v1.9.3: 'dds' = copy BeamNG's .dds files as they are, 'png' = convert (v1.6-v1.9.1)
}
local exportJob = nil   -- the export in progress: {t0, ip, port, paint, vehicle}
local lastExport = nil  -- the reply of the last finished export (for gtaBridge.exportStatus())

local function v4(p)
  if not p then return nil end
  local ok, t = pcall(function() return { p.x, p.y, p.z, p.w } end)
  return ok and t or nil
end

-- v1.9.6: a paint's surface values {metallic, roughness, clearcoat, clearcoatRoughness}. BeamNG docs:
-- paint layers keep metallicFactor/roughnessFactor/clearCoatFactor at 1 and the paint's own values are
-- multiplied in, so without these the GTA plugin drew every paint as rough bare metal ("alloy" look).
local function finite(x)
  x = tonumber(x)
  if x == nil or x ~= x or math.abs(x) == math.huge then return nil end
  return x
end

local function pbrOf(p)
  if type(p) ~= 'table' then return nil end
  local m = finite(p.metallic or p[1])
  local r = finite(p.roughness or p[2])
  local cc = finite(p.clearcoat or p.clearCoat or p[3])
  local ccr = finite(p.clearcoatRoughness or p.clearCoatRoughness or p[4])
  if not (m or r or cc or ccr) then return nil end
  return { m or 0, r or 0.5, cc or 0, ccr or 0.04 }
end

local function pbrList(t)
  -- {paint1, paint2, paint3} as tables, or 12 numbers in a row, or the same as a text
  if type(t) == 'string' then
    local nums = {}
    for x in t:gmatch('[-%d%.eE]+') do nums[#nums + 1] = tonumber(x) end
    t = nums
  end
  if type(t) ~= 'table' then return nil end
  local out = {}
  if type(t[1]) == 'number' then
    for i = 1, 3 do
      local b = (i - 1) * 4
      if t[b + 1] then out[i] = pbrOf({ t[b + 1], t[b + 2], t[b + 3], t[b + 4] }) end
    end
  else
    for i = 1, 3 do out[i] = pbrOf(t[i]) end
  end
  if not out[1] then return nil end
  for i = 2, 3 do out[i] = out[i] or out[1] end
  return out
end

local function readPaint(veh)
  -- paint slots 1/2/3: the same fields export.lua reads for paintable materials
  local paint = {}
  pcall(function() paint.color = v4(veh.color) end)
  pcall(function() paint.palette0 = v4(veh.colorPalette0) end)
  pcall(function() paint.palette1 = v4(veh.colorPalette1) end)
  -- v1.9.6: metallic / roughness / clear coat of each paint. Tried in order, the first that works wins
  -- (UNVERIFIED which one this BeamNG version has; the log says which):
  --   1. the vehicle's config (what a .pc file saves as "paints")
  --   2. the vehicle object's metallic paint data
  local tries = {
    { 'config.paints', function()
        local vd
        if core_vehicle_manager.getVehicleData then vd = core_vehicle_manager.getVehicleData(veh:getID()) end
        vd = vd or core_vehicle_manager.getPlayerVehicleData()
        return vd and vd.config and vd.config.paints
      end },
    { 'getMetallicPaintData()', function() return veh:getMetallicPaintData() end },
    { 'metallicPaintData field', function() return veh.metallicPaintData end },
  }
  local seen = {}
  for _, tr in ipairs(tries) do
    local ok, v = pcall(tr[2])
    if ok and v ~= nil then
      local okL, list = pcall(pbrList, v)
      if okL and list then
        paint.pbr = list
        paint.pbrSource = tr[1]
        break
      end
      seen[#seen + 1] = tr[1] .. ' = ' .. type(v)
    else
      seen[#seen + 1] = tr[1] .. ': ' .. (ok and 'nil' or 'error')
    end
  end
  if paint.pbr and paint.pbrSource == 'config.paints' then
    -- does the config follow the live paint? (a repaint in the UI might not update it; UNVERIFIED)
    pcall(function()
      local vd
      if core_vehicle_manager.getVehicleData then vd = core_vehicle_manager.getVehicleData(veh:getID()) end
      vd = vd or core_vehicle_manager.getPlayerVehicleData()
      local bc = vd.config.paints[1].baseColor
      local c = paint.color
      if type(bc) == 'table' and c then
        local b1, b2, b3 = tonumber(bc[1] or bc.x), tonumber(bc[2] or bc.y), tonumber(bc[3] or bc.z)
        if b1 and b2 and b3 then
          local same = math.abs(b1 - c[1]) < 0.01 and math.abs(b2 - c[2]) < 0.01 and math.abs(b3 - c[3]) < 0.01
          paint.pbrMatchesColour = same
          logI(string.format('paint 1 colour: config %.3f %.3f %.3f, live %.3f %.3f %.3f (%s)', b1, b2, b3, c[1], c[2], c[3], same and 'same' or 'DIFFERENT'))
        end
      end
    end)
  end
  if paint.pbr then
    local p = paint.pbr
    logI(string.format('paint surface (from %s): metallic/roughness/clear coat/clear coat roughness %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f',
      paint.pbrSource, p[1][1], p[1][2], p[1][3], p[1][4], p[2][1], p[2][2], p[2][3], p[2][4], p[3][1], p[3][2], p[3][3], p[3][4]))
  else
    paint.pbrSource = 'not found (' .. table.concat(seen, '; ') .. ')'
    logI('paint surface values not found:', paint.pbrSource)
  end
  return paint
end

local function realPathOf(vpath)
  local ok, rp = pcall(function() return FS:getFileRealPath(vpath) end)
  if ok and type(rp) == 'string' and rp ~= '' then return rp, 'FS:getFileRealPath' end
  ok, rp = pcall(function() return FS:getUserPath() end)
  if ok and type(rp) == 'string' and rp ~= '' then
    rp = rp:gsub('[/\\]+$', '') .. vpath
    return (rp:gsub('/', '\\')), 'FS:getUserPath'
  end
  return nil, 'unknown (set CarFile in GtaBeamD1.ini)'
end

local function fileExists(p)
  local ok, e = pcall(function() return FS:fileExists(p) end)
  return ok and e and true or false
end

-- ---- materials ------------------------------------------------------------------------------
-- per-layer fields we read (BeamNG Material, versions 1 and 1.5). kind: vec / num / bool / map / info
local LAYER_FIELDS = {
  { 'baseColorFactor', 'vec' }, { 'baseColorMap', 'map' }, { 'diffuseMapUseUV', 'num' },
  { 'colorPaletteMap', 'map' }, { 'colorPaletteMapUseUV', 'num' }, { 'instanceDiffuse', 'bool' },
  { 'opacityFactor', 'num' }, { 'opacityMap', 'map' }, { 'opacityMapUseUV', 'num' },
  { 'metallicFactor', 'num' }, { 'roughnessFactor', 'num' }, { 'clearCoatFactor', 'num' },
  { 'diffuseColor', 'vec' }, { 'colorMap', 'map' },   -- version 1 materials
  { 'normalMap', 'map' }, { 'normalMapUseUV', 'num' },             -- v1.9: drawn in GTA
  { 'metallicMap', 'map' }, { 'metallicMapUseUV', 'num' },
  { 'roughnessMap', 'map' }, { 'roughnessMapUseUV', 'num' },
  { 'clearCoatMap', 'info' }, { 'detailNormalMap', 'info' }, { 'emissiveFactor', 'info' }, { 'emissiveMap', 'info' },  -- report only
}
-- material-wide fields (layer 0)
local MATERIAL_FIELDS = { 'translucentBlendOp', 'cubemap', 'dynamicCubemap', 'doubleSided', 'castShadows' }

local function getF(mat, name, layer)
  local ok, v = pcall(function() return mat:getField(name, layer) end)
  if ok and type(v) == 'string' then return v end
  return ''
end

-- Torque's named colours (materials use them, e.g. "White", "black", "InvisibleBlack")
local NAMED_COLORS = {
  white = { 1, 1, 1, 1 }, black = { 0, 0, 0, 1 }, invisibleblack = { 0, 0, 0, 0 }, transparent = { 0, 0, 0, 0 },
  red = { 1, 0, 0, 1 }, green = { 0, 1, 0, 1 }, blue = { 0, 0, 1, 1 }, yellow = { 1, 1, 0, 1 },
  cyan = { 0, 1, 1, 1 }, magenta = { 1, 0, 1, 1 }, gray = { 0.5, 0.5, 0.5, 1 }, grey = { 0.5, 0.5, 0.5, 1 },
  orange = { 1, 0.5, 0, 1 },
}
local unknownColors = {}
local function parseVec(s)
  s = tostring(s or '')
  local named = NAMED_COLORS[s:lower():gsub('%s', '')]
  if named then return { named[1], named[2], named[3], named[4] } end
  local t = {}
  for n in s:gmatch('[-%d%.eE+]+') do t[#t + 1] = tonumber(n) end
  if #t == 0 then
    if s ~= '' then unknownColors[s] = true end
    return nil
  end
  while #t < 4 do t[#t + 1] = 1 end
  return t
end

local function safeName(p)
  return (p:lower():gsub('^/', ''):gsub('[^%w%.%-_]', '_'))
end

-- read a file from BeamNG's virtual file system (also inside zipped mods) as raw bytes
local function readBytes(src)
  local ok, d = pcall(function() return readFile(src) end)
  if ok and type(d) == 'string' and #d > 0 then return d end
  local f = io.open(src, 'rb')
  if not f then return nil end
  d = f:read('*all')
  f:close()
  if not d or #d == 0 then return nil end
  return d
end

local function copyFile(src, dst)
  local d = readBytes(src)
  if not d then return false end
  return writeFile(dst, d) ~= false
end

-- Make a texture the GTA plugin can read: returns 'tex/<name>' (relative to car.gltf) or nil, why
local texCache = {}
local function resolveTexture(path, stats2)
  if not path or path == '' then return nil end
  if texCache[path] ~= nil then return texCache[path] or nil, 'cached failure' end
  if path:sub(1, 1) == '@' then texCache[path] = false; return nil, 'live UI texture (skipped)' end
  local src = path
  if src:sub(1, 1) ~= '/' and fileExists('/' .. src) then src = '/' .. src end
  if not fileExists(src) then
    -- texture-cooker names (.color.png etc.) are .dds on disk; many materials leave the extension out
    local tries = { (src:gsub('%.png$', '.dds')), src .. '.dds', src .. '.png', src .. '.jpg', src .. '.color.dds',
                    src .. '.normal.dds', src .. '.data.dds' }
    local hit
    for _, alt in ipairs(tries) do
      if alt ~= src and fileExists(alt) then hit = alt; break end
    end
    if hit then src = hit
    else
      local okF, found = pcall(function() return FS:findFiles('/', path:match('[^/]+$') or path, -1, false, false) end)
      if okF and found and #found == 1 then src = found[1] else texCache[path] = false; return nil, 'file not found' end
    end
  end
  local ext = (src:match('%.(%w+)$') or ''):lower()
  -- v1.9.3: .dds files are handed over as they are (the GTA plugin reads them); PNG only as a fallback
  if ext == 'dds' and exportCfg.textures == 'dds' then
    local outDds = safeName(src)
    local dstDds = exportCfg.texDir .. outDds
    if fileExists(dstDds) then
      if stats2 then stats2.reused = stats2.reused + 1 end
      texCache[path] = 'tex/' .. outDds
      return texCache[path]
    end
    local d = readBytes(src)
    if d and #d >= 128 and d:sub(1, 4) == 'DDS ' and writeFile(dstDds, d) ~= false and fileExists(dstDds) then
      if stats2 then stats2.ddsCopied = (stats2.ddsCopied or 0) + 1 end
      texCache[path] = 'tex/' .. outDds
      return texCache[path]
    end
    if stats2 then stats2.ddsFailed = (stats2.ddsFailed or 0) + 1 end
    -- could not copy it: convert to PNG below (v1.9.1 behaviour)
  end
  local out = safeName(src) .. ((ext == 'dds') and '.png' or '')
  local dst = exportCfg.texDir .. out
  if not fileExists(dst) then
    local done = false
    if ext == 'dds' then
      local cached = '/temp/' .. src:lower():gsub('^/', '') .. '.png'   -- where the exporter converted it
      if fileExists(cached) then done = copyFile(cached, dst) end
      if not done then
        cached = '/temp/' .. src:lower() .. '.png'
        if fileExists(cached) then done = copyFile(cached, dst) end
      end
      if not done then
        local okC, res = pcall(function() return convertDDSToPNG(src, dst) end)
        done = okC and res ~= false and fileExists(dst)
      end
      if stats2 then stats2.converted = stats2.converted + 1 end
    elseif ext == 'png' or ext == 'jpg' or ext == 'jpeg' then
      done = copyFile(src, dst)
      if stats2 then stats2.copied = stats2.copied + 1 end
    else
      texCache[path] = false
      return nil, 'unsupported type .' .. ext
    end
    if not done then texCache[path] = false; return nil, 'conversion failed' end
  elseif stats2 then
    stats2.reused = stats2.reused + 1
  end
  texCache[path] = 'tex/' .. out
  return texCache[path]
end

-- Everything the plugin needs about one material, plus raw text for materials.txt
local function describeMaterial(name, withTextures, stats2, lines)
  local mat = scenetree.findObject(name)
  if not mat then
    lines[#lines + 1] = name .. ': NOT FOUND in the scene'
    return nil
  end
  local okC, cls = pcall(function() return mat:getClassName() end)
  if okC and tostring(cls):lower() ~= 'material' then
    lines[#lines + 1] = name .. ': not a Material (' .. tostring(cls) .. ')'
    return nil
  end
  local ver = tonumber(getF(mat, 'version', 0)) or 1
  local active
  pcall(function() active = tonumber(mat.activeLayers) end)
  active = active or tonumber(getF(mat, 'activeLayers', 0)) or 1
  active = math.max(1, math.min(4, active))
  local d = {
    v = ver, n = active, layers = {},
    translucent = getF(mat, 'translucent', 0), alphaRef = tonumber(getF(mat, 'alphaRef', 0)),
    alphaTest = getF(mat, 'alphaTest', 0), doubleSided = getF(mat, 'doubleSided', 0),
    blendOp = getF(mat, 'translucentBlendOp', 0), cubemap = getF(mat, 'cubemap', 0),
  }
  local wide = {}
  for _, f in ipairs(MATERIAL_FIELDS) do
    local s = getF(mat, f, 0)
    if s ~= '' then wide[#wide + 1] = f .. '=' .. s end
  end
  lines[#lines + 1] = string.format('%s  (version %s, %d layer(s), translucent %s, alphaTest %s, alphaRef %s%s)', name, tostring(ver),
    active, d.translucent, d.alphaTest, tostring(d.alphaRef), #wide > 0 and (', ' .. table.concat(wide, ', ')) or '')
  for l = 0, active - 1 do
    local L = {}
    local raw = {}
    for _, f in ipairs(LAYER_FIELDS) do
      local key, kind = f[1], f[2]
      local s = getF(mat, key, l)
      if s ~= '' then raw[#raw + 1] = key .. '=' .. s end
      if kind == 'info' then
        -- listed in materials.txt only
      elseif kind == 'vec' then L[key] = parseVec(s)
      elseif kind == 'num' then L[key] = tonumber(s)
      elseif kind == 'bool' then L[key] = (s == '1' or s == 'true') and true or nil
      elseif kind == 'map' and s ~= '' then
        -- BeamNG's common solid paint masks (nullcolormaskR/G/B): no texture needed, just the slot
        local solid = key == 'colorPaletteMap' and s:lower():match('nullcolormask(%a)')
        if solid then
          L.paletteConst = solid:upper()
        elseif s:sub(1, 1) == '@' and (key == 'baseColorMap' or key == 'colorMap') then
          -- a live UI texture (a screen or gauge BeamNG draws at run time): nothing to export,
          -- the GTA plugin draws a dark glossy screen there
          L.baseColorLive = true
          raw[#raw + 1] = '  (live UI texture: drawn as a screen in GTA)'
        elseif withTextures then
          local rel, why = resolveTexture(s, stats2)
          L[key] = rel
          if not rel then raw[#raw + 1] = '  !! ' .. key .. ' not usable: ' .. tostring(why) end
        else
          L[key] = s
        end
      end
    end
    -- version 1 materials: colorMap/diffuseColor are the base colour
    if ver < 1.5 then
      L.baseColorFactor = L.diffuseColor or L.baseColorFactor
      L.baseColorMap = L.colorMap or L.baseColorMap
    end
    L.diffuseColor, L.colorMap = nil, nil
    d.layers[l + 1] = L
    lines[#lines + 1] = string.format('   layer %d: %s', l + 1, table.concat(raw, ' | '))
  end
  return d
end

local function buildMaterials(root, lines)
  local st = { converted = 0, copied = 0, reused = 0, materials = 0, missing = 0, ddsCopied = 0, ddsFailed = 0 }
  for _, m in ipairs(root.materials or {}) do
    if m.name then
      local d = describeMaterial(m.name, true, st, lines)
      if d then
        m.extras = m.extras or {}
        m.extras.gta = d
        st.materials = st.materials + 1
      else
        st.missing = st.missing + 1
      end
    end
  end
  return st
end

-- The exporter embedded every texture as base64 images; we use our own PNG files instead.
local function stripImages(root)
  local n = 0
  for _, img in ipairs(root.images or {}) do
    local bv = img.bufferView and root.bufferViews and root.bufferViews[img.bufferView + 1]
    if bv then
      local b = root.buffers and root.buffers[bv.buffer + 1]
      if b then
        b.uri = 'data:application/octet-stream;base64,AAAAAA=='
        b.byteLength = 4
      end
      bv.byteOffset = 0
      bv.byteLength = 4
      n = n + 1
    end
  end
  root.images, root.textures = nil, nil
  for _, m in ipairs(root.materials or {}) do
    m.pbrMetallicRoughness, m.normalTexture, m.emissiveTexture, m.occlusionTexture, m.ambientOcclusionMap = nil, nil, nil, nil, nil
  end
  return n
end

-- ---- wheels (for spinning/steering in GTA) --------------------------------------------------
-- v1.9: which wheels the engine drives. The powertrain lists devices with an inputName chain
-- (wheelaxleRL -> differential_R -> driveshaft -> gearbox -> ... -> mainEngine); a wheel axle with
-- "connectedWheel" whose chain reaches an engine or motor is driven. UNVERIFIED on every car:
-- returns nil (unknown) when nothing is found, and the GTA plugin then assumes rear-wheel drive.
local function drivenWheelSet(vdata)
  local pt = vdata and vdata.powertrain
  if type(pt) ~= 'table' then return nil, 'no powertrain data' end
  local byName = {}
  for _, d in pairs(pt) do
    if type(d) == 'table' and d.name then byName[d.name] = d end
  end
  local set, found = {}, 0
  for _, d in pairs(pt) do
    if type(d) == 'table' and d.connectedWheel then
      local cur, steps, powered = d, 0, false
      while cur and steps < 40 do
        local ty = tostring(cur.type or ''):lower()
        if ty:find('engine') or ty:find('motor') then powered = true; break end
        local up = cur.inputName
        if not up or up == cur.name then break end
        cur = byName[up]
        steps = steps + 1
      end
      if powered then
        set[tostring(d.connectedWheel)] = true
        found = found + 1
      end
    end
  end
  if found == 0 then return nil, 'no wheel axle reaches an engine' end
  return set
end

local function readWheels(veh)
  local out, drivenNames, why = {}, {}, nil
  local ok, err = pcall(function()
    local vd = core_vehicle_manager.getPlayerVehicleData()
    local vdata = vd and vd.vdata
    local wheels = vdata and vdata.wheels
    if not wheels then error('vdata.wheels missing') end
    local okD, set, w2 = pcall(drivenWheelSet, vdata)
    if not okD then set, why = nil, tostring(set) else why = w2 end
    local byName = {}
    for cid, n in pairs(vdata.nodes or {}) do
      if type(n) == 'table' and n.name then byName[n.name] = n.cid or cid end
    end
    local function cidOf(x)
      if type(x) == 'number' then return x end
      return byName[x]
    end
    for _, w in pairs(wheels) do
      if type(w) == 'table' and w.node1 ~= nil and w.node2 ~= nil then
        local a, b = cidOf(w.node1), cidOf(w.node2)
        if a and b then
          local p1, p2 = vec3(veh:getNodePosition(a)), vec3(veh:getNodePosition(b))
          local wname = tostring(w.name or #out)
          local driven = nil
          if set then driven = set[wname] == true end
          if driven then drivenNames[#drivenNames + 1] = wname end
          out[#out + 1] = { name = wname, c1 = { p1.x, p1.z, -p1.y }, c2 = { p2.x, p2.z, -p2.y },
                            r = tonumber(w.radius) or 0, width = tonumber(w.tireWidth) or 0, driven = driven }
        end
      end
    end
  end)
  if not ok then logE('wheels:', tostring(err)) end
  local drivenText = #drivenNames > 0 and table.concat(drivenNames, ',') or ('unknown' .. (why and (' (' .. why .. ')') or ''))
  logI('export: driven wheels:', drivenText)
  return out, drivenText
end

-- v2.74.1: mesh nodes outside the glTF scene aren't the car. BeamNG's exporter also picks up other mods' objects that way
-- (DARS / Driver Arms: its driver's pieces, some parked 100 m away: the car came out 100 m long and GTA couldn't place it).
-- Their mesh is left out (the node stays, so no index changes). Not done if it would leave out most of the car.
local function dropStrays(root)
  local sc = root.scenes and root.scenes[(tonumber(root.scene) or 0) + 1]
  if not (sc and sc.nodes and #sc.nodes > 0 and root.nodes) then return 0, {} end
  local keep = {}
  local function walk(i)
    if keep[i] then return end
    keep[i] = true
    for _, c in ipairs((root.nodes[i + 1] or {}).children or {}) do walk(c) end
  end
  for _, i in ipairs(sc.nodes) do walk(i) end
  local cut, total, names = {}, 0, {}
  for i, n in ipairs(root.nodes) do
    if n.mesh ~= nil then
      total = total + 1
      if not keep[i - 1] then cut[#cut + 1] = n end
    end
  end
  if #cut * 2 > total then return 0, { 'none: ' .. #cut .. ' of ' .. total .. ' meshes are outside the scene' } end
  for _, n in ipairs(cut) do
    n.mesh = nil
    if #names < 3 then names[#names + 1] = tostring(n.name) end
  end
  return #cut, names
end

-- glTF arrays must not be empty; cgltf refuses "primitives":{} etc.
local function sanitizeGltf(root)
  for _, m in ipairs(root.meshes or {}) do
    if m.primitives and #m.primitives == 0 then m.primitives = nil; m.name = m.name or 'empty_mesh' end
  end
  for _, n in ipairs(root.nodes or {}) do
    if n.children and #n.children == 0 then n.children = nil end
  end
  local anyNodes = false
  for _, s in ipairs(root.scenes or {}) do
    if s.nodes and #s.nodes > 0 then anyNodes = true end
  end
  if not anyNodes then root.scenes = nil; root.scene = nil end   -- an empty scene would encode as [] (invalid)
  for _, k in ipairs({ 'images', 'textures', 'materials', 'animations', 'samplers' }) do
    if root[k] and #root[k] == 0 then root[k] = nil end
  end
end

local function exportReply(tbl)
  lastExport = tbl
  if exportJob and exportJob.ip then
    local s = jsonEncode(tbl)
    pcall(function() udp:sendto(s, exportJob.ip, exportJob.port) end)
    stats.tx = stats.tx + 1
  end
  exportJob = nil
end

local function exportFail(msg)
  logE('export: ' .. msg)
  screen('GTA bridge: export FAILED - ' .. msg, 12)
  exportReply({ t = 'export_err', err = msg })
end

local function onExported(root)
  if not exportJob then return end
  if not root then
    exportFail('the exporter was busy, try again')
    return
  end
  local t1 = now()
  local info = {}
  local okW, err = pcall(function()
    local lines = { 'Materials of ' .. tostring(exportJob.vehicle) .. ' (GTA bridge ' .. M.version .. ')', '' }
    unknownColors = {}
    info.mats = buildMaterials(root, lines)
    local unk = {}
    for k in pairs(unknownColors) do unk[#unk + 1] = k end
    if #unk > 0 then
      lines[#lines + 1] = ''
      lines[#lines + 1] = 'Colour names not understood (drawn white): ' .. table.concat(unk, ', ')
      logE('export: colour names not understood: ' .. table.concat(unk, ', '))
    end
    info.stripped = stripImages(root)
    info.wheels, info.driven = readWheels(exportJob.veh)
    -- v2.5 (dents): the car's nodes; v2.53: each flexbody's own nodes, each prop's 3 nodes
    local okN, nodes, flex, note, props = pcall(dents.exportNodes, exportJob.veh)
    if okN and nodes then
      info.nodes, info.flex, info.props = nodes, flex, props
      info.dentsNote = tostring(note)
      logI('export: nodes for the dents: ' .. tostring(note))
    else
      logE('export: no nodes for the dents (the car is drawn without them): ' .. tostring(okN and note or nodes))
    end
    lines[#lines + 1] = ''
    lines[#lines + 1] = 'Driven wheels: ' .. tostring(info.driven)
    do
      local pp = exportJob.paint and exportJob.paint.pbr
      if pp then
        lines[#lines + 1] = string.format('Paint surface (%s): metallic/roughness/clear coat/clear coat roughness %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f, %.2f/%.2f/%.2f/%.2f',
          tostring(exportJob.paint.pbrSource), pp[1][1], pp[1][2], pp[1][3], pp[1][4], pp[2][1], pp[2][2], pp[2][3], pp[2][4], pp[3][1], pp[3][2], pp[3][3], pp[3][4])
      else
        lines[#lines + 1] = 'Paint surface: ' .. tostring(exportJob.paint and exportJob.paint.pbrSource)
      end
    end
    pcall(writeFile, exportCfg.matDump, table.concat(lines, '\n') .. '\n')
    local nStray, strays = dropStrays(root)
    if nStray > 0 or #strays > 0 then
      logI(string.format('export: %d meshes outside the car left out (another mod\'s objects): %s', nStray, table.concat(strays, ', ')))
    end
    sanitizeGltf(root)
    root.extras = root.extras or {}
    local vp
    pcall(function() local p = exportJob.veh:getPosition(); vp = { p.x, p.z, -p.y } end)
    -- v2.6: which mesh breaks with which deformGroup (glass, lights): GTA hides / frosts it when BeamNG breaks it
    pcall(function()
      local vd = core_vehicle_manager.getPlayerVehicleData()
      local dg, ndg = wpn.exportDg(vd and vd.vdata or {})
      info.dg = dg
      logI('export: ' .. tostring(ndg) .. ' meshes with a deformGroup (glass, lights: hidden in GTA when BeamNG breaks them)')
    end)
    root.extras.gtaBridge = { version = M.version, paint = exportJob.paint, vehicle = exportJob.vehicle,
                              wheels = info.wheels, vehPos = vp, nodes = info.nodes, flex = info.flex, props = info.props, dg = info.dg }
    local s = jsonEncode(root)
    local wr = writeFile(exportCfg.vpath, s)
    if wr == false then error('writeFile failed for ' .. exportCfg.vpath) end
    if not fileExists(exportCfg.vpath) then error('file missing after writeFile: ' .. exportCfg.vpath) end
    exportJob.bytes = #s
  end)
  if not okW then
    exportFail('writing failed: ' .. tostring(err))
    return
  end
  local real, how = realPathOf(exportCfg.vpath)
  local ms = info.mats or {}
  local reply = {
    t = 'export_ok', path = real, pathFrom = how, vpath = exportCfg.vpath,
    ms = (now() - exportJob.t0) * 1000, writeMs = (now() - t1) * 1000, bytes = exportJob.bytes,
    meshes = #(root.meshes or {}), materials = ms.materials, materialsMissing = ms.missing,
    texConverted = ms.converted, texCopied = ms.copied, texReused = ms.reused, imagesStripped = info.stripped,
    texDds = ms.ddsCopied, texDdsFailed = ms.ddsFailed, textureMode = exportCfg.textures,
    wheels = #(info.wheels or {}), drivenWheels = info.driven, paint = exportJob.paint, vehicle = exportJob.vehicle,
    modVersion = M.version, dents = info.dentsNote and info.dentsNote:sub(1, 700) or nil,   -- v2.52: in GTA's log too
  }
  local line = string.format('export: %s -> %s (%.1f MB, %d meshes, %d materials, textures %d new (%d .dds as they are, %d .dds failed -> png) + %d reused, %d wheels (driven %s), %.1f s)',
    tostring(reply.vehicle), tostring(real), reply.bytes / 1048576, reply.meshes, ms.materials or 0,
    (ms.converted or 0) + (ms.copied or 0) + (ms.ddsCopied or 0), ms.ddsCopied or 0, ms.ddsFailed or 0, ms.reused or 0, reply.wheels, tostring(info.driven), reply.ms / 1000)
  logI(line)
  screen(string.format('GTA bridge: car exported (%.0f MB, %d materials, %d wheels, %.1f s)', reply.bytes / 1048576,
    ms.materials or 0, reply.wheels, reply.ms / 1000), 10)
  exportReply(reply)
end

-- Start an export of the player's car. ip/port: who gets the reply (nil from the console).
function M.exportCar(ip, port)
  if exportJob then
    logE('export: one is already running')
    screen('GTA bridge: an export is already running', 6)
    return false
  end
  local veh = getPlayerVeh()
  if not veh then
    logE('export: no player vehicle')
    screen('GTA bridge: no car to export (get in one)', 8)
    if ip then pcall(function() udp:sendto(jsonEncode({ t = 'export_err', err = 'no player vehicle in BeamNG' }), ip, port) end) end
    return false
  end
  pcall(function() extensions.unload('util_export') end)   -- fresh module state (texture cache)
  extensions.load('util/export')
  local ex = extensions.util_export
  if not ex or not ex.export then
    logE('export: util_export not available')
    screen('GTA bridge: BeamNG exporter not available', 8)
    if ip then pcall(function() udp:sendto(jsonEncode({ t = 'export_err', err = 'util_export not available' }), ip, port) end) end
    return false
  end
  ex.embedBuffers = true
  ex.gltfBinaryFormat = false
  ex.exportNormals = true
  ex.exportTexCoords = true
  ex.exportTangents = false   -- normal maps use surface frames built in the GTA plugin's shader
  ex.exportColors = false
  ex.exportBeams = false
  texCache = {}
  local vehicle
  pcall(function() vehicle = veh:getJBeamFilename() end)
  exportJob = { t0 = now(), ip = ip, port = port, paint = readPaint(veh), vehicle = vehicle, veh = veh }
  logI('export: started for', tostring(vehicle))
  screen('GTA bridge: exporting ' .. tostring(vehicle) .. '...', 6)
  local ok, err = pcall(ex.export, onExported)
  if not ok then
    exportFail('exporter error: ' .. tostring(err))
    return false
  end
  return true
end

-- v1.9.3: 'dds' (default) copies BeamNG's .dds textures as they are; 'png' converts them (v1.9.1 and older plugins)
function M.setTextureMode(mode)
  if mode == 'dds' or mode == 'png' then
    exportCfg.textures = mode
    logI('export: texture mode', mode)
    screen('GTA bridge: textures will be ' .. (mode == 'dds' and 'copied as .dds' or 'converted to PNG') .. ' on the next export', 8)
  else
    logE('setTextureMode: use \'dds\' or \'png\'')
  end
end

function M.exportStatus()
  if exportJob then logI('export: running for', string.format('%.1f s', now() - exportJob.t0)) end
  if lastExport then logI('export: last result', jsonEncode(lastExport)) end
  screen(lastExport and ('GTA bridge: last export ' .. tostring(lastExport.t)) or 'GTA bridge: no export yet', 8)
  return lastExport
end

-- Material report for the car you're in (no export needed): /gtabridge/materials.txt + a message.
-- gtaBridge.matCheck() for all materials, gtaBridge.matCheck('name') for one.
function M.matCheck(only)
  local veh = getPlayerVeh()
  if not veh then screen('GTA bridge: get in a car first', 6); return end
  local names = {}
  if only then names = { only } else
    local ok, list = pcall(function() return veh:getMaterialNames() end)
    if ok and list then for _, n in pairs(list) do names[#names + 1] = n end end
  end
  local lines = { 'Materials (matCheck, GTA bridge ' .. M.version .. ')', '' }
  local found = 0
  for _, n in ipairs(names) do
    if describeMaterial(n, false, nil, lines) then found = found + 1 end
  end
  local _, drivenText = readWheels(veh)
  lines[#lines + 1] = ''
  lines[#lines + 1] = 'Driven wheels: ' .. tostring(drivenText)
  pcall(writeFile, exportCfg.matDump, table.concat(lines, '\n') .. '\n')
  local real = realPathOf(exportCfg.matDump)
  local msg = string.format('GTA bridge: %d of %d materials written to %s', found, #names, tostring(real or exportCfg.matDump))
  logI(msg)
  screen(msg, 12)
end

local function checkExportTimeout()
  if exportJob and now() - exportJob.t0 > exportCfg.timeout then
    exportFail('timed out after ' .. exportCfg.timeout .. ' s')
  end
end

-- ---------------------------------------------------------------- v2.01: BeamNG's own controls
-- The GTA plugin reads your pad (XInput: Xbox pads, DS4Windows) and, while GTA has focus, your keyboard,
-- and sends their raw state every GTA frame. Here that state is matched against YOUR BeamNG bindings
-- (core_input_bindings, i.e. Options > Controls, or the inputmap files) and each bound action's own code
-- is run (core_input_actions: onChange / onDown / onUp with VALUE, FILTERTYPE, ANGLE... filled in), the
-- way BeamNG runs it when the control moves. Only vehicle actions are forwarded (not cameras, menus,
-- resets...). Names follow Torque3D/BeamNG: keys "up", "lshift", "x"...; pad "thumblx", "triggerr",
-- "btn_a", "btn_l", "dpadd"... UNVERIFIED in game: the runtime table layouts (both are logged to
-- /gtabridge/bindings.txt) and how BeamNG shapes deadzones/linearity (approximated here).

-- XInput button bits (XINPUT_GAMEPAD wButtons) -> BeamNG's pad control names
local XI_BITS = {
  { 0x0001, 'dpadu' }, { 0x0002, 'dpadd' }, { 0x0004, 'dpadl' }, { 0x0008, 'dpadr' },
  { 0x0010, 'btn_start' }, { 0x0020, 'btn_back' }, { 0x0040, 'btn_lt' }, { 0x0080, 'btn_rt' },
  { 0x0100, 'btn_l' }, { 0x0200, 'btn_r' }, { 0x1000, 'btn_a' }, { 0x2000, 'btn_b' }, { 0x4000, 'btn_x' }, { 0x8000, 'btn_y' },
}
local PAD_AXES = { thumblx = 1, thumbly = 2, thumbrx = 3, thumbry = 4, triggerl = 5, triggerr = 6 }
local PAD_CENTRED = { thumblx = true, thumbly = true, thumbrx = true, thumbry = true }
local PAD_BTN = {}
for _, b in ipairs(XI_BITS) do PAD_BTN[b[2]] = b[1] end
local PAD_ALIAS = { upov = 'dpadu', dpov = 'dpadd', lpov = 'dpadl', rpov = 'dpadr' }
-- A PlayStation pad that BeamNG sees as a DirectInput device (no DS4Windows) has its own control names.
-- Its usual layout in XInput terms (EXPERIMENTAL, only used when no XInput pad has driving bindings):
-- {xinput control, how}: nil = as it is, 'neg' = upside down, 'trig' = trigger as -1..1, 'btn' = trigger as a button
local DS_MAP = {
  button0 = { 'btn_x' }, button1 = { 'btn_a' }, button2 = { 'btn_b' }, button3 = { 'btn_y' },
  button4 = { 'btn_l' }, button5 = { 'btn_r' }, button6 = { 'triggerl', 'btn' }, button7 = { 'triggerr', 'btn' },
  button8 = { 'btn_back' }, button9 = { 'btn_start' }, button10 = { 'btn_lt' }, button11 = { 'btn_rt' },
  xaxis = { 'thumblx' }, yaxis = { 'thumbly', 'neg' }, zaxis = { 'thumbrx' }, rzaxis = { 'thumbry', 'neg' },
  rxaxis = { 'triggerl', 'trig' }, ryaxis = { 'triggerr', 'trig' },
  upov = { 'dpadu' }, dpov = { 'dpadd' }, lpov = { 'dpadl' }, rpov = { 'dpadr' },
}
local KEY_ALIAS = { enter = 'return', lctrl = 'lcontrol', rctrl = 'rcontrol', control = 'ctrl' }
local MOD_KEYS = { shift = { 'lshift', 'rshift' }, ctrl = { 'lcontrol', 'rcontrol' }, alt = { 'lalt', 'ralt' } }
local MOD_OF_KEY = { lshift = 'shift', rshift = 'shift', shift = 'shift', lcontrol = 'ctrl', rcontrol = 'ctrl', ctrl = 'ctrl',
                     lalt = 'alt', ralt = 'alt', alt = 'alt' }
-- v2.2: never forwarded, whatever GTA's ini says (reloading the car mid-drive puts it back at its spawn)
local ALWAYS_DENY = { reload_vehicle = true, reload_all_vehicles = true }
-- defaults; the GTA plugin sends its ini's lists with drive_start
local bindOpts = {
  cats = { 'vehicle' },   -- v2.2: an action whose category IS one of these (or one of these + "_..." except "_debug") is forwarded
  deny = { reset_physics = true, reset_all_physics = true, recover_vehicle = true, recover_vehicle_alt = true,
           recover_to_last_road = true, loadHome = true, saveHome = true, dropPlayerAtCamera = true,
           dropPlayerAtCameraNoReset = true, toggleWalkingMode = true, switch_next_vehicle = true,
           switch_previous_vehicle = true, vehicle_selector = true, parts_selector = true,
           reload_vehicle = true, reload_all_vehicles = true },
  also = {},
  rep = {},               -- v2.43: actions that repair the car where it is (GTA's BeamNGRepairActions)
  ds = true,              -- PlayStation pad lists may be used
  padList = 'auto',       -- v2.1: auto / xinput / playstation (GTA's BeamNGPadBindings)
}

local function truthy(x) return x == true or x == 1 or x == '1' or x == 'true' end
local function keysOf(t)
  if type(t) ~= 'table' then return type(t) end
  local ks = {}
  for k, v in pairs(t) do ks[#ks + 1] = tostring(k) .. (type(v) == 'function' and '()' or '') end
  table.sort(ks)
  return table.concat(ks, ' ')
end
local function splitList(s)
  local t = {}
  for w in tostring(s or ''):gmatch('[^,%s]+') do t[#t + 1] = w end
  return t
end
local function numOf(x)
  if type(x) == 'number' then return x end
  if type(x) == 'string' then
    local last
    for n in x:gmatch('[-%d%.]+') do last = tonumber(n) or last end   -- "-0.15 0.15" (old deadzone text) -> 0.15
    return last
  end
  return nil
end

-- "ctrl q", "shift-ctrl q", "ctrl-q", "lshift" -> key, {shift=, ctrl=, alt=}
local function parseKeyControl(ctl)
  ctl = tostring(ctl or ''):lower()
  local parts = {}
  for p in ctl:gmatch('%S+') do parts[#parts + 1] = p end
  if #parts == 0 then return nil end
  if #parts == 1 and #parts[1] > 1 and parts[1]:find('%-') and not parts[1]:find('^%-') then
    local bits = {}
    for p in parts[1]:gmatch('[^%-]+') do bits[#bits + 1] = p end
    parts = bits
  end
  local mods, n = {}, 0
  for i = 1, #parts - 1 do
    for m in parts[i]:gmatch('[^%-+]+') do
      local mk = MOD_OF_KEY[KEY_ALIAS[m] or m]
      if mk then mods[mk] = true; n = n + 1 end
    end
  end
  local key = parts[#parts]
  key = KEY_ALIAS[key] or key
  return key, mods, n
end

local function readBindingsRuntime()
  local ib = rawget(_G, 'core_input_bindings')
  if type(ib) ~= 'table' and extensions then ib = extensions.core_input_bindings end
  if type(ib) ~= 'table' then return nil, 'core_input_bindings not loaded' end
  local src, how
  if type(ib.bindings) == 'table' and next(ib.bindings) ~= nil then
    src, how = ib.bindings, 'core_input_bindings.bindings'
  elseif type(ib.getBindings) == 'function' then
    local ok, b = pcall(ib.getBindings)
    if ok and type(b) == 'table' then src, how = b, 'core_input_bindings.getBindings()' end
  end
  if not src then return nil, 'core_input_bindings has no bindings table (it has: ' .. keysOf(ib) .. ')' end
  local devs = {}
  for k, e in pairs(src) do
    if type(e) == 'table' then
      local c = type(e.contents) == 'table' and e.contents or e
      local list = c.bindings or e.bindings
      if type(list) == 'table' then
        devs[#devs + 1] = { devname = tostring(e.devname or e.device or e.deviceName or c.devname or (type(k) == 'string' and k) or ('device' .. tostring(k))),
                            name = tostring(c.name or e.name or ''), vidpid = tostring(c.vidpid or e.vidpid or ''), list = list }
      end
    end
  end
  return devs, how
end

-- fallback: the inputmap files (factory .json + your .diff changes)
local function readBindingsFiles()
  local okF, files = pcall(function() return FS:findFiles('/settings/inputmaps/', '*.json', 0, false, false) end)
  if not okF or type(files) ~= 'table' then return nil, 'no inputmap files found' end
  local devs = {}
  for _, path in ipairs(files) do
    local name = path:match('([^/]+)%.json$')
    local okB, base = pcall(jsonReadFile, path)
    if name and okB and type(base) == 'table' then
      local list = {}
      for _, b in ipairs(base.bindings or {}) do list[#list + 1] = b end
      local okD, diff = pcall(jsonReadFile, '/settings/inputmaps/' .. name .. '.diff')
      if okD and type(diff) == 'table' then
        local function same(a, b) return a.control == b.control and a.action == b.action end
        for _, r in ipairs(diff.removed or {}) do
          for i = #list, 1, -1 do if same(list[i], r) then table.remove(list, i) end end
        end
        for _, b in ipairs(diff.bindings or {}) do
          for i = #list, 1, -1 do if same(list[i], b) then table.remove(list, i) end end
          list[#list + 1] = b
        end
      end
      devs[#devs + 1] = { devname = name, name = tostring(base.name or name), vidpid = tostring(base.vidpid or name), list = list }
    end
  end
  return devs, 'inputmap files (/settings/inputmaps)'
end

local function readActionFiles(vehDir)
  local acts, n = {}, 0
  local function load(path, vehicleSpecific)
    local ok, t = pcall(jsonReadFile, path)
    if ok and type(t) == 'table' then
      for name, a in pairs(t) do
        if type(a) == 'table' and acts[name] == nil then
          if vehicleSpecific then
            a.ctx = a.ctx or 'vlua'
            a.cat = a.cat or 'vehicle_specific'
          end
          acts[name] = a
          n = n + 1
        end
      end
    end
  end
  local okF, files = pcall(function() return FS:findFiles('/lua/ge/extensions/core/input/actions/', '*.json', 0, false, false) end)
  if okF and type(files) == 'table' then for _, p in ipairs(files) do load(p, false) end end
  if vehDir and vehDir ~= '' then
    okF, files = pcall(function() return FS:findFiles('/vehicles/' .. vehDir .. '/', 'input_actions*.json', 0, false, false) end)
    if okF and type(files) == 'table' then for _, p in ipairs(files) do load(p, true) end end
  end
  return acts, n
end

local function readActions(vehDir)
  local ia = rawget(_G, 'core_input_actions')
  if type(ia) ~= 'table' and extensions then ia = extensions.core_input_actions end
  local acts, how
  if type(ia) == 'table' then
    for _, fn in ipairs({ 'getActiveActions', 'getActions', 'getAllActions' }) do
      if type(ia[fn]) == 'function' then
        local ok, a = pcall(ia[fn])
        if ok and type(a) == 'table' and next(a) ~= nil then acts, how = a, 'core_input_actions.' .. fn .. '()'; break end
      end
    end
  end
  local fileActs, nFile = readActionFiles(vehDir)
  if not acts then return fileActs, 'action files (' .. nFile .. ')', ia end
  local added = 0
  for name, a in pairs(fileActs) do
    if acts[name] == nil then acts[name] = a; added = added + 1 end
  end
  return acts, how .. (added > 0 and (' + ' .. added .. ' from files') or ''), ia
end

-- v2.2: categories matched exactly ("vehicle" = vehicle, vehicle_specific...; never vehicle_debug or any *_debug)
local function catMatches(cat, c)
  c = c:lower()
  if c == '' then return false end
  if cat == c then return true end
  if cat:sub(1, #c + 1) == c .. '_' and not cat:find('_debug$') and not cat:find('_debug_') then return true end
  return false
end

local function actionAllowed(name, def)
  if bindOpts.rep[name] then return true, 'repair' end   -- v2.43: repairs the car where it is (BeamNG's own would move it); v2.51: also reload_vehicle
  if ALWAYS_DENY[name] then return false, 'never forwarded (reloads the BeamNG car)' end
  if bindOpts.deny[name] then return false, 'never forwarded (BeamNGActionsNever)' end
  if type(def) ~= 'table' then return false, 'action not defined' end
  if not (def.onChange or def.onDown or def.onUp) then return false, 'action has no code' end
  local ctx = tostring(def.ctx or 'tlua')
  if ctx == 'ui' then return false, 'menu/UI action' end
  if bindOpts.also[name] then return true end
  local cat = tostring(def.cat or ''):lower()
  for _, c in ipairs(bindOpts.cats) do
    if catMatches(cat, c) then return true end
  end
  if cat:find('debug') then return false, 'category "' .. cat .. '" is a debug one' end
  return false, 'category "' .. cat .. '" is not a vehicle one'
end

-- the last word of a pad control ("modifier1 upov" -> "upov") and its modifier words
local function padCtlParts(ctl)
  local parts = {}
  for p in tostring(ctl or ''):lower():gmatch('[^%s]+') do parts[#parts + 1] = p end
  return parts[#parts] or '', #parts > 1
end

-- v2.01.1: decided by the device's control names (XInput: btn_a, thumblx, triggerr, dpadu...; DirectInput:
-- button0, xaxis, rxaxis, upov...), the device name only when the list says nothing
local function devKind(dev)
  local n = tostring(dev.devname or ''):lower()
  if n:find('^key') then return 'key' end
  if n:find('^mouse') then return 'mouse' end
  local xi, di = 0, 0
  for _, b in pairs(dev.list) do
    local c = padCtlParts(type(b) == 'table' and (b.control or b.ctrl) or '')
    if PAD_AXES[c] or c:find('^btn_') or c:find('^dpad') then xi = xi + 1
    elseif c:find('^button%d+$') or c:find('^r?[xyz]axis$') or c:find('^slider') then di = di + 1 end
  end
  local s = (tostring(dev.vidpid) .. ' ' .. tostring(dev.name)):lower()
  dev.sce = s:find('sce_pad') ~= nil   -- v2.1: BeamNG's own PlayStation support (your log: "sce_pad_standard")
  local ps = dev.sce or s:find('054c') or s:find('dualsense') or s:find('dualshock') or s:find('wireless controller')
  if xi > di then return 'xi' end
  if di > 0 then return ps and 'ds' or 'di' end
  if n:find('^xi') or n:find('xinput') then return 'xi' end
  return ps and 'ds' or 'di'
end

local DRIVE_ACTS = { accelerate = 'thr', brake = 'brk', accelerate_brake = 'thr', steering = 'str', steer_left = 'str',
                     steer_right = 'str', parkingbrake = 'hb', parkingbrake_toggle = 'hb' }

-- every binding the bridge will run, plus a report (/gtabridge/bindings.txt)
local function buildBinds(veh)
  local vehDir
  pcall(function() vehDir = veh:getJBeamFilename() end)
  local B = { pad = {}, key = {}, padCtl = {}, keyCtl = {}, has = { pad = {}, key = {} }, lines = {}, unknown = {}, combos = {} }
  local L = B.lines
  local devs, how = readBindingsRuntime()
  if not devs or #devs == 0 then
    local why = how
    devs, how = readBindingsFiles()
    how = tostring(how) .. ' (runtime: ' .. tostring(why) .. ')'
  end
  local acts, ahow, ia = readActions(vehDir)
  B.acts = acts   -- v2.3: for "act" (an action by name)
  B.how, B.ahow = tostring(how), tostring(ahow)
  L[#L + 1] = 'GTA bridge ' .. M.version .. ': your BeamNG controls as the bridge reads them (car: ' .. tostring(vehDir) .. ')'
  L[#L + 1] = 'bindings from: ' .. B.how
  L[#L + 1] = 'actions from: ' .. B.ahow
  local okIb, ib = pcall(function() return rawget(_G, 'core_input_bindings') or (extensions and extensions.core_input_bindings) end)
  L[#L + 1] = 'core_input_bindings has: ' .. (okIb and keysOf(ib) or '?')
  L[#L + 1] = 'core_input_actions has: ' .. keysOf(ia)
  do
    local okA, l = pcall(function() return ActionMap:getList() end)
    local names = {}
    if okA and type(l) == 'table' then
      for _, m in pairs(l) do
        local okN, nm = pcall(function() return type(m) == 'string' and m or m:getName() end)
        names[#names + 1] = okN and tostring(nm) or tostring(m)
      end
    end
    L[#L + 1] = 'action maps: ' .. (#names > 0 and table.concat(names, ', ') or ('? (' .. tostring(l) .. ')'))
  end
  L[#L + 1] = string.format('forwarded: actions whose category is "%s" (or starts with it + "_", never *_debug), not: %s', table.concat(bindOpts.cats, '", "'),
    (function() local t = {} for k in pairs(bindOpts.deny) do t[#t + 1] = k end for k in pairs(ALWAYS_DENY) do if not bindOpts.deny[k] then t[#t + 1] = k end end table.sort(t) return table.concat(t, ' ') end)())
  L[#L + 1] = ''
  if not devs or #devs == 0 then
    L[#L + 1] = 'NO BINDINGS FOUND'
    return B
  end
  -- which pad list: GTA sends ONE pad (XInput). v2.1: auto = the PlayStation list when it has driving bindings
  -- (a PlayStation pad plugged in is the pad you hold; DS4Windows' Xbox pad is only its translation), else the
  -- XInput list with driving bindings, else the first XInput list. BeamNGPadBindings in GTA's ini can force it.
  local kinds = {}
  for i, dev in ipairs(devs) do kinds[i] = devKind(dev) end
  local function driving(dev)
    for _, b in pairs(dev.list) do
      if type(b) == 'table' and DRIVE_ACTS[tostring(b.action)] then return true end
    end
    return false
  end
  local xiUse, xiAny, dsUse, dsAny
  for i, dev in ipairs(devs) do
    if kinds[i] == 'xi' then
      xiAny = xiAny or i
      if not xiUse and driving(dev) then xiUse = i end
    elseif kinds[i] == 'ds' then
      dsAny = dsAny or i
      if not dsUse and driving(dev) then dsUse = i end
    end
  end
  xiUse = xiUse or xiAny
  local padUse
  if bindOpts.padList == 'xinput' then padUse = xiUse or dsUse
  elseif bindOpts.padList == 'playstation' then padUse = dsUse or dsAny or xiUse
  else padUse = dsUse or xiUse end
  if not bindOpts.ds and kinds[padUse or 0] == 'ds' then padUse = xiUse end
  B.padDev = padUse and devs[padUse].devname or nil
  B.padKind = padUse and kinds[padUse] or nil
  -- v2.1: BeamNG's pad modifiers ("modifier1 upov" = hold modifier 1's button + D-pad up): the buttons are the
  -- ones bound to customModifier1/2/3 in the same list
  local function padName(dev, kind, name)
    name = tostring(name or ''):lower()
    if kind == 'ds' then
      local m = DS_MAP[name]
      if not m then return nil end
      local how = m[2]
      if how == 'trig' and dev.sce then how = nil end   -- BeamNG's own PlayStation support: triggers 0..1 like XInput
      return m[1], how
    end
    return PAD_ALIAS[name] or name, nil
  end
  B.modCtl = {}
  if padUse then
    local dev = devs[padUse]
    for _, b in pairs(dev.list) do
      local n = type(b) == 'table' and tostring(b.action or ''):match('^customModifier(%d)$')
      if n then
        local nm = padName(dev, kinds[padUse], b.control or b.ctrl)
        if nm and PAD_BTN[nm] then B.modCtl[tonumber(n)] = nm
        else B.modNote = B.modNote or {}; B.modNote[tonumber(n)] = tostring(b.control or b.ctrl) end
      end
    end
  end
  -- v2.52: the other pad list's combinations (modifier + button) run too, like BeamNG with both pads on (your choice: your
  -- Xbox list has the repair on RB+LB+D-pad down, your DualSense list cruise control). Single buttons stay the used list's.
  local padAlso
  if padUse then
    if kinds[padUse] == 'ds' then padAlso = xiUse elseif kinds[padUse] == 'xi' and bindOpts.ds then padAlso = dsUse or dsAny end
    if padAlso == padUse then padAlso = nil end
  end
  local modMap2 = {}   -- the other list's modifier number -> the used list's
  if padAlso then
    local dev = devs[padAlso]
    for _, b in pairs(dev.list) do
      local n = type(b) == 'table' and tonumber(tostring(b.action or ''):match('^customModifier(%d)$'))
      local nm = n and padName(dev, kinds[padAlso], b.control or b.ctrl)
      if nm and PAD_BTN[nm] then
        local m
        for k = 1, 3 do if B.modCtl[k] == nm then m = k end end
        for k = 1, 3 do if not m and not B.modCtl[k] then m = k end end
        if m then B.modCtl[m] = nm; modMap2[n] = m end
      end
    end
  end
  B.padAlso = padAlso and devs[padAlso].devname or nil
  B.padCombo, B.modUsed = {}, {}
  local seenPair = {}
  for i, dev in ipairs(devs) do
    local kind = kinds[i]
    local also = padAlso ~= nil and i == padAlso
    local use = kind == 'key' or ((kind == 'xi' or kind == 'ds') and (i == padUse or also))
    L[#L + 1] = string.format('device %s "%s" (%s, %s): %d bindings%s', dev.devname, dev.name, dev.vidpid,
      kind == 'key' and 'keyboard' or kind == 'xi' and 'pad, XInput' or kind == 'ds' and (dev.sce and 'PlayStation pad, BeamNG\'s own support' or 'PlayStation pad seen as DirectInput')
        or kind == 'mouse' and 'mouse' or 'DirectInput (wheel/joystick)',
      (function() local n = 0 for _ in pairs(dev.list) do n = n + 1 end return n end)(),
      use and ((kind == 'xi' or kind == 'ds') and (also and ' - its combinations (modifier + button) run too (v2.52)' or ' - THE PAD LIST USED') or '') or ((kind == 'xi' or kind == 'ds') and ' - not used (another pad list is used: BeamNGPadBindings in GTA\'s ini)'
        or ' - not used by the bridge yet'))
    if use and not also and (kind == 'xi' or kind == 'ds') then
      local ms = {}
      for n = 1, 3 do
        local note = B.modNote and B.modNote[n]
        if note and note:lower():match('^button1[23]$') then note = note .. ': PS / touchpad button, not readable through XInput' end
        ms[#ms + 1] = string.format('modifier%d = %s', n, B.modCtl[n] or ('none' .. (note and (' (' .. note .. ')') or '')))
      end
      L[#L + 1] = '  pad modifiers: ' .. table.concat(ms, ', ')
    end
    for _, b in pairs(dev.list) do
      if type(b) == 'table' and b.action then
        local action, ctl = tostring(b.action), tostring(b.control or b.ctrl or '')
        local def = acts[action]
        local ok, why = actionAllowed(action, def)
        if ok and why == 'repair' then
          -- v2.43: your repair binding: the car is repaired where it is (upright, a little lifted)
          def = { onDown = 'if gtaBridge and gtaBridge.repairHere then gtaBridge.repairHere(\'' .. action .. '\') end', ctx = 'tlua',
                  cat = def and def.cat or 'vehicle', repair = true }
        end
        local entry, note
        local pairKey = (kind == 'key' and 'k|' or 'p|') .. ctl:lower() .. '|' .. action
        if use and ok and seenPair[pairKey] then ok, why = false, 'same binding listed twice' end
        if use and ok then
          seenPair[pairKey] = true
          local e = { action = action, def = def, centred = truthy(def.isCentered), inv = truthy(b.isInverted),
                      dzR = math.max(0, math.min(0.95, numOf(b.deadzoneResting) or numOf(b.deadzone) or 0)),
                      dzE = math.max(0, math.min(0.95, numOf(b.deadzoneEnd) or 0)), lin = numOf(b.linearity) or 1,
                      angle = numOf(b.angle) or 0, lock = numOf(b.lockType) or 0, last = 0, down = false, ctlName = ctl }
          local ft = numOf(b.filterType)
          if kind == 'key' then
            local key, mods, nm = parseKeyControl(ctl)
            if key then
              e.dev, e.key, e.mods, e.nMods = 'key', key, mods, nm
              e.isModKey = MOD_OF_KEY[key] ~= nil
              e.flt = (ft and ft >= 0) and ft or 0
              B.key[#B.key + 1] = e
              B.keyCtl[key] = true
              entry = e
            else
              note = 'key not understood'
            end
          else
            -- "modifier1 upov", "modifier1-modifier2 dpov", "modifier1 modifier2 button11": modifier words, then the control
            local words = {}
            for w in ctl:lower():gmatch('%S+') do words[#words + 1] = w end
            local mods, nMods, badWord = {}, 0, nil
            for wi = 1, #words - 1 do
              for part in words[wi]:gmatch('[^%-]+') do
                local n = tonumber(part:match('^modifier(%d)$'))
                if n then
                  if not mods[n] then mods[n] = true; nMods = nMods + 1 end
                else
                  badWord = part
                end
              end
            end
            local name, how2
            if words[#words] then name, how2 = padName(dev, kind, words[#words]) end
            local missing
            if also then
              -- (its modifier numbers in the used list's numbering)
              local m2 = {}
              for n in pairs(mods) do if modMap2[n] then m2[modMap2[n]] = true else missing = n end end
              mods = m2
            end
            for n in pairs(mods) do if not B.modCtl[n] then missing = n end end
            if #words == 0 then
              note = 'empty control'
            elseif also and nMods == 0 then
              note = 'single buttons come from the pad list used'
            elseif badWord then
              note = 'pad control not understood'
              B.unknown[#B.unknown + 1] = ctl
            elseif missing then
              note = string.format('needs modifier%d, which no button is bound to in this list (customModifier%d)', missing, missing)
              B.combos[#B.combos + 1] = ctl
            elseif name and (PAD_AXES[name] or PAD_BTN[name]) then
              e.dev, e.ctl, e.xf = 'pad', name, how2
              e.axis = PAD_AXES[name] ~= nil and how2 ~= 'btn'
              e.ctlCentred = PAD_CENTRED[name] or how2 == 'trig'
              e.flt = (ft and ft >= 0) and ft or 1
              e.mods, e.nMods = mods, nMods
              B.pad[#B.pad + 1] = e
              if nMods > 0 then
                B.padCombo[name] = true
                local set = {}
                for n in pairs(mods) do B.modUsed[B.modCtl[n]] = true; set[#set + 1] = B.modCtl[n] end
                table.sort(set)
                B.padComboSets = B.padComboSets or {}
                B.padComboSets[name] = B.padComboSets[name] or {}
                B.padComboSets[name][table.concat(set, '+')] = true
              else
                B.padCtl[name] = true
              end
              entry = e
            else
              note = (kind == 'ds' and words[#words]:match('^button1[23]$')) and 'PS / touchpad button: not readable through XInput (DS4Windows)'
                or 'pad control not understood'
              if note == 'pad control not understood' then B.unknown[#B.unknown + 1] = ctl end
            end
          end
          if entry and DRIVE_ACTS[action] then B.has[entry.dev][DRIVE_ACTS[action]] = true end
          if entry and action == 'accelerate_brake' then B.has[entry.dev].brk = true end
        end
        local extra = ''
        if entry then
          extra = string.format(' FORWARDED (%s%s%s%s%s)%s', entry.dev == 'pad' and (entry.axis and (entry.ctlCentred and 'axis -1..1' or 'axis 0..1') or 'button') or 'key',
            entry.inv and ', inverted' or '', entry.dzR > 0 and string.format(', deadzone %.2f', entry.dzR) or '',
            entry.lin ~= 1 and string.format(', linearity %.2f', entry.lin) or '', entry.centred and ', centred action' or '',
            entry.def and entry.def.repair and ' as REPAIR WHERE IT IS (v2.43)' or '')
        else
          extra = ' skipped (' .. tostring(note or why or 'device not used') .. ')'
        end
        L[#L + 1] = string.format('  %-18s -> %-28s [%s, %s]%s', ctl, action, tostring(def and def.cat or '?'), tostring(def and def.ctx or '?'), extra)
      end
    end
  end
  -- the pad controls GTA hands over: plain ones + the modifier buttons your combinations use; combo-only
  -- controls (padm) go to BeamNG only while one of its modifier buttons is held
  for k in pairs(B.modUsed) do B.padCtl[k] = true end
  local pm, pmod = {}, {}
  for k in pairs(B.padCombo) do if not B.padCtl[k] then pm[#pm + 1] = k end end
  for k in pairs(B.modUsed) do pmod[#pmod + 1] = k end
  table.sort(pm); table.sort(pmod)
  B.padComboList, B.padModList = pm, pmod
  -- "dpadd=btn_r;dpadu=btn_l/btn_l+btn_r": per combination-only control, the modifier button sets that use it
  local mx = {}
  for _, k in ipairs(pm) do
    local sets = {}
    for st in pairs((B.padComboSets or {})[k] or {}) do sets[#sets + 1] = st end
    table.sort(sets)
    mx[#mx + 1] = k .. '=' .. table.concat(sets, '/')
  end
  B.padComboSetsStr = table.concat(mx, ';')
  local pc, kc = {}, {}
  for k in pairs(B.padCtl) do pc[#pc + 1] = k end
  for k in pairs(B.keyCtl) do kc[#kc + 1] = k end
  table.sort(pc); table.sort(kc)
  B.padList, B.keyList = pc, kc
  local function has(d) local t = {} for k in pairs(B.has[d]) do t[#t + 1] = k end table.sort(t) return #t > 0 and table.concat(t, '/') or 'none' end
  L[#L + 1] = ''
  L[#L + 1] = string.format('forwarded: pad %d bindings on %d controls (%s), keyboard %d bindings on %d keys; driving inputs bound: pad %s, keyboard %s',
    #B.pad, #pc, table.concat(pc, ' '), #B.key, #kc, has('pad'), has('key'))
  L[#L] = L[#L] .. string.format('; pad list: %s%s; combination-only controls: %s; modifier buttons used: %s', tostring(B.padDev or 'none'),
    B.padAlso and (' (+ the combinations of ' .. B.padAlso .. ')') or '',
    #pm > 0 and table.concat(pm, ' ') or 'none', #pmod > 0 and table.concat(pmod, ' ') or 'none')
  B.summary = L[#L]
  if #B.combos > 0 then L[#L + 1] = string.format('pad combinations not usable: %d (%s)', #B.combos, table.concat(B.combos, ', '):sub(1, 400)) end
  if #B.unknown > 0 then L[#L + 1] = 'pad controls not understood: ' .. table.concat(B.unknown, ' ') end
  return B
end

local function writeBindsReport(B)
  pcall(writeFile, '/gtabridge/bindings.txt', table.concat(B.lines, '\n') .. '\n')
end

function M.binds()
  local veh = getPlayerVeh()
  if not veh then screen('GTA bridge: get in a car first', 6); return end
  local ok, B = pcall(buildBinds, veh)
  if not ok then logE('binds:', tostring(B)); screen('GTA bridge: reading your controls failed: ' .. tostring(B), 10); return end
  writeBindsReport(B)
  local real = realPathOf('/gtabridge/bindings.txt')
  local msg = string.format('GTA bridge: %d pad + %d keyboard bindings forwarded; details in %s', #B.pad, #B.key, tostring(real))
  logI('binds: ' .. B.lines[#B.lines])
  screen(msg, 12)
  return B
end

-- BeamNG's own device handling is switched off while GTA drives (and while GTA is focused with the car
-- parked), so a pad that BeamNG also reads in the background can't press things twice or drive the car
-- while you walk around in GTA. Action map names from BeamNG's docs (UNVERIFIED: the result is logged).
local NATIVE_MAPS = { 'VehicleCommonActionMap', 'VehicleSpecificActionMap', 'NormalActionMap' }
local native = { blocked = false, saved = {}, t = 0, reapplied = 0, userOff = false }
local BLOCK_FILE = '/gtabridge/blocked_maps.txt'   -- which maps the bridge switched off (survives a Lua reload)
local function setNativeBlocked(on, why, force)
  if on == native.blocked and not force then return end
  local res, offList = {}, {}
  for _, name in ipairs(NATIVE_MAPS) do
    local okF, m = pcall(function() return scenetree.findObject(name) end)
    if okF and m then
      if on then
        local okR, was = pcall(function() return m.enabled end)
        native.saved[name] = not (okR and was == false)
        local okS = pcall(function() m.enabled = false end)
        if not okS then okS = pcall(function() m:setEnabled(false) end) end
        local okC, now2 = pcall(function() return m.enabled end)
        res[#res + 1] = name .. (okS and ((okC and now2 == false) and ' off' or ' off?') or ' FAILED')
        if native.saved[name] then offList[#offList + 1] = name end
      else
        local want = native.saved[name] ~= false
        local okS = pcall(function() m.enabled = want end)
        if not okS then pcall(function() m:setEnabled(want) end) end
        res[#res + 1] = name .. ' back'
      end
    else
      res[#res + 1] = name .. ' not found'
    end
  end
  native.blocked = on
  native.t = now()
  pcall(writeFile, BLOCK_FILE, on and table.concat(offList, '\n') or '')
  logI('controls: BeamNG\'s own device controls ' .. (on and 'blocked' or 'given back') .. ' (' .. tostring(why) .. '): ' .. table.concat(res, ', '))
end

-- while blocked: once a second switch off again whatever BeamNG switched back on (it may rebuild its maps)
local function keepNativeBlocked()
  if not native.blocked or now() - native.t < 1 then return end
  native.t = now()
  for _, name in ipairs(NATIVE_MAPS) do
    local okF, m = pcall(function() return scenetree.findObject(name) end)
    if okF and m then
      local okR, en = pcall(function() return m.enabled end)
      if okR and en ~= false then
        if native.saved[name] == nil then native.saved[name] = true end
        local okS = pcall(function() m.enabled = false end)
        if not okS then pcall(function() m:setEnabled(false) end) end
        native.reapplied = native.reapplied + 1
        if native.reapplied <= 3 then logI('controls: ' .. name .. ' was on again: switched off again') end
      end
    end
  end
end

-- a previous run of this mod (Lua reloaded while it had BeamNG's controls off): give them back
local function restoreNativeFromFile()
  local ok, txt = pcall(function() return readFile(BLOCK_FILE) end)
  if not ok or type(txt) ~= 'string' or txt == '' then return end
  local back = {}
  for name in txt:gmatch('[%w_]+') do
    local okF, m = pcall(function() return scenetree.findObject(name) end)
    if okF and m then
      local okS = pcall(function() m.enabled = true end)
      if not okS then pcall(function() m:setEnabled(true) end) end
      back[#back + 1] = name
    end
  end
  pcall(writeFile, BLOCK_FILE, '')
  if #back > 0 then logI('controls: switched back on what an earlier run left off: ' .. table.concat(back, ', ')) end
end

function M.unblock()
  native.userOff = true   -- no blocking for a parked car until the next drive start
  setNativeBlocked(false, 'gtaBridge.unblock()', true)
  for _, name in ipairs(NATIVE_MAPS) do
    local okF, m = pcall(function() return scenetree.findObject(name) end)
    if okF and m then
      local okS = pcall(function() m.enabled = true end)
      if not okS then pcall(function() m:setEnabled(true) end) end
    end
  end
  screen('GTA bridge: BeamNG has its own controls back' .. (drive and ' (GTA is still driving: they go off again)' or ''), 6)
end

-- ---------------------------------------------------------------- v2.0 drive mode (T2)
-- The GTA plugin sends its controls ("drive", every GTA frame). v2.01: they are the raw pad/keyboard state,
-- run through your BeamNG bindings (above); without usable bindings, v2.0's throttle/brake/steering/
-- parking brake values go to the car's vehicle Lua as input.event. Every frame the car's pose goes back:
-- position, forward/up, velocity and each wheel's hub (centre, axle direction, turn angle), all relative
-- to the car (x right, y forward, z up). v2.01: every 0.2 s the car's own throttle/brake/gear/rpm too.
local drive = nil
local driveCfg = { failsafeS = 0.5, refreshS = 0.25, endS = 5, vsS = 0.1, parkS = 2.5 }   -- v2.1: car state 10 a second (GTA's gauge)
local park = { t = -1e9, block = true }

local function v3(p)
  if p == nil then return nil end
  return { p.x or p[1] or 0, p.y or p[2] or 0, p.z or p[3] or 0 }
end
local function dot(a, b) return a[1]*b[1] + a[2]*b[2] + a[3]*b[3] end
local atan2 = math.atan2 or function(y, x) return math.atan(y, x) end   -- LuaJIT has atan2 (5.3+ only atan)

-- the wheels' hub nodes (node1/node2, like the export) and a node that turns with each wheel
local function driveWheels(veh)
  local list, keysSeen = {}, nil
  local vd = core_vehicle_manager.getPlayerVehicleData()
  local vdata = vd and vd.vdata
  local wheels = vdata and vdata.wheels
  if not wheels then return list, 'vdata.wheels missing' end
  local byName = {}
  for cid, n in pairs(vdata.nodes or {}) do
    if type(n) == 'table' and n.name then byName[n.name] = n.cid or cid end
  end
  local function cidOf(x)
    if type(x) == 'number' then return x end
    if type(x) == 'string' then return byName[x] end
    if type(x) == 'table' then return x.cid or (x.name and byName[x.name]) end
    return nil
  end
  local function nodePos(cid)
    local ok, p = pcall(function() return v3(veh:getNodePosition(cid)) end)
    return ok and p or nil
  end
  for _, w in pairs(wheels) do
    if type(w) == 'table' and w.node1 ~= nil and w.node2 ~= nil then
      local a, b = cidOf(w.node1), cidOf(w.node2)
      local p1, p2 = a and nodePos(a), b and nodePos(b)
      if p1 and p2 then
        if not keysSeen then
          local ks = {}
          for k, v in pairs(w) do ks[#ks + 1] = tostring(k) .. ':' .. type(v) end
          table.sort(ks)
          keysSeen = table.concat(ks, ' ')
        end
        local e = { name = tostring(w.name or #list), a = a, b = b, r = tonumber(w.radius) or 0.33 }
        local c = { (p1[1] + p2[1]) * 0.5, (p1[2] + p2[2]) * 0.5, (p1[3] + p2[3]) * 0.5 }
        local ax = norm(sub(p2, p1))
        for _, key in ipairs({ 'hubNodes', 'rimNodes', 'treadNodes', 'nodes' }) do
          local t = w[key]
          if e.s == nil and type(t) == 'table' then
            for k, nn in pairs(t) do
              local cid = cidOf(nn)
              if not cid and type(k) == 'number' and type(nn) ~= 'table' then cid = k end
              if cid and cid ~= a and cid ~= b then
                local q = nodePos(cid)
                if q then
                  q = sub(q, c)
                  local along = dot(q, ax)
                  local perp = { q[1] - ax[1] * along, q[2] - ax[2] * along, q[3] - ax[3] * along }
                  if math.sqrt(dot(perp, perp)) > 0.05 then e.s = cid; e.sKey = key; break end
                end
              end
            end
          end
        end
        -- v2.63: the wheel's own nodes (tyre, rim) on each side of its hub, while it is on: when it comes off the car in a
        -- crash (they leave its hub nodes behind) GTA draws it where they are (drivePose)
        local src = type(w.nodes) == 'table' and w.nodes or w.treadNodes
        if type(src) == 'table' then
          local ra, rb = {}, {}
          for k, nn in pairs(src) do
            local cid = cidOf(nn)
            if not cid and type(k) == 'number' and type(nn) ~= 'table' then cid = k end
            if cid and cid ~= a and cid ~= b and #ra + #rb < 48 then
              local q = nodePos(cid)
              if q then
                local sd = dot(sub(q, c), ax)
                if sd < -0.01 then ra[#ra + 1] = cid elseif sd > 0.01 then rb[#rb + 1] = cid end
              end
            end
          end
          if #ra >= 3 and #rb >= 3 then
            local function mean(t) local m = { 0, 0, 0 }; for _, cid in ipairs(t) do local q = nodePos(cid) or c; for i = 1, 3 do m[i] = m[i] + q[i] / #t end end; return m end
            local ca, cb = mean(ra), mean(rb)
            local mid = { (ca[1] + cb[1]) * 0.5, (ca[2] + cb[2]) * 0.5, (ca[3] + cb[3]) * 0.5 }
            local dm = sub(mid, c)
            e.ring = { a = ra, b = rb, oa = dot(dm, ax), d0 = math.sqrt(dot(dm, dm)) }
          end
        end
        list[#list + 1] = e
      end
    end
  end
  return list, keysSeen
end

local function drivePose(veh, dt)
  local d = drive
  local p = v3(veh:getPosition())
  local f = norm(v3(veh:getDirectionVector()))
  local u0 = v3(veh:getDirectionVectorUp())
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local vel
  if d.velApi ~= false then
    local okV, vv = pcall(function() return veh:getVelocity() end)
    if okV and vv then vel = v3(vv); d.velApi = true; d.vel = vel else d.velApi = false end
  end
  if not vel then
    -- no velocity call: from the position change (smoothed)
    vel = d.vel or { 0, 0, 0 }
    if d.prevP and dt > 0 then
      local k = 0.5
      for i = 1, 3 do vel[i] = vel[i] + ((p[i] - d.prevP[i]) / dt - vel[i]) * k end
    end
    d.vel = vel
  end
  d.prevP = p
  local parts, slips = {}, {}
  local spinAll = #d.wheels > 0
  for _, w in ipairs(d.wheels) do
    local p1, p2 = v3(veh:getNodePosition(w.a)), v3(veh:getNodePosition(w.b))
    local c = { (p1[1] + p2[1]) * 0.5, (p1[2] + p2[2]) * 0.5, (p1[3] + p2[3]) * 0.5 }
    local ax = norm(sub(p2, p1))
    if dot(ax, r) < 0 then ax = { -ax[1], -ax[2], -ax[3] } end
    -- v2.63: the wheel off its hub (a crash broke it off; your 2.62 test: the drawn wheels stayed on the car): drawn where its
    -- own nodes are
    local rg = w.ring
    if rg then
      local ca, cb = { 0, 0, 0 }, { 0, 0, 0 }
      for _, cid in ipairs(rg.a) do local q = v3(veh:getNodePosition(cid)); for i = 1, 3 do ca[i] = ca[i] + q[i] / #rg.a end end
      for _, cid in ipairs(rg.b) do local q = v3(veh:getNodePosition(cid)); for i = 1, 3 do cb[i] = cb[i] + q[i] / #rg.b end end
      local rax = norm(sub(cb, ca))
      local mid = { (ca[1] + cb[1]) * 0.5, (ca[2] + cb[2]) * 0.5, (ca[3] + cb[3]) * 0.5 }
      local dm = sub(mid, c)
      local off = math.abs(math.sqrt(dot(dm, dm)) - rg.d0) > 0.25 or dot(rax, ax) < 0.85
      if off then
        c = { mid[1] - rax[1] * rg.oa, mid[2] - rax[2] * rg.oa, mid[3] - rax[3] * rg.oa }
        ax = rax
      end
      if off ~= (w.off or false) then
        w.off = off
        logI(string.format('drive: BeamNG\'s %s wheel %s', tostring(w.name), off and 'came off its hub: GTA draws it where it is' or 'is on its hub again'))
      end
    end
    local ang = 0
    if w.s then
      local q = sub(v3(veh:getNodePosition(w.s)), c)
      local along = dot(q, ax)
      local x = { q[1] - ax[1] * along, q[2] - ax[2] * along, q[3] - ax[3] * along }
      local ua = dot(u, ax)
      local e1 = norm({ u[1] - ax[1] * ua, u[2] - ax[2] * ua, u[3] - ax[3] * ua })
      local e2 = cross(ax, e1)
      local a = atan2(dot(x, e2), dot(x, e1))   -- + = turned about the right-pointing axle
      if w.prev == nil then
        w.acc = a
        w.rate = -dot(vel, f) / math.max(0.1, w.r)   -- rolling forward turns it the negative way
      else
        local dd = a - w.prev
        dd = dd - 2 * math.pi * math.floor((dd + math.pi) / (2 * math.pi))
        if dt > 0.002 then
          -- more than half a turn per frame is possible at speed: take the whole turns that fit how fast it was turning
          local expect = (w.rate or 0) * dt
          dd = dd + 2 * math.pi * math.floor((expect - dd) / (2 * math.pi) + 0.5)
          w.rate = math.max(-400, math.min(400, dd / dt))
        end
        w.acc = w.acc + dd
        if w.acc ~= w.acc or math.abs(w.acc) > 1e9 then w.acc, w.rate = a, 0 end   -- NaN / runaway: start again
      end
      w.prev = a
      ang = w.acc
    else
      spinAll = false
    end
    -- v2.74: how fast its tread slips over the ground (GTA's tyre smoke; your v2.73 test: GTA's own guess from the drawn poses saw
    -- 60 m/s in plain driving): the tread's speed (its turn rate x radius) against its hub's speed along its way, and the hub's speed
    -- sideways (a slide), on this frame's own clock
    local sl = 0
    if w.prevC and dt > 0.002 and w.rate then
      local hv = { vel[1] + (c[1] - w.prevC[1]) / dt, vel[2] + (c[2] - w.prevC[2]) / dt, vel[3] + (c[3] - w.prevC[3]) / dt }
      local fw = cross(u, ax)
      -- (sideways counts 0.8: a tyre in a hard, clean corner slides a few degrees without smoking; a drift is far more)
      sl = math.max(math.abs(math.abs(w.rate) * math.max(0.1, w.r or 0.33) - math.abs(dot(hv, fw))), 0.8 * math.abs(dot(hv, ax)))
      if sl ~= sl then sl = 0 end
    end
    w.prevC = c
    slips[#slips + 1] = string.format('%.1f', math.min(99, sl))
    parts[#parts + 1] = string.format('%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.4f', dot(c, r), dot(c, f), dot(c, u), dot(ax, r), dot(ax, f), dot(ax, u), ang)
  end
  d.seq = d.seq + 1
  -- v2.4: "ss" = BeamNG's simulation clock (sum of dtSim, the time its physics was advanced), next to "st" (its frame
  -- clock). The GTA plugin measures which one BeamNG's positions fit better (P1, measurement only).
  local s = string.format('{"t":"pose","q":%d,"st":%.5f,"ss":%.5f,"p":[%.4f,%.4f,%.4f],"f":[%.6f,%.6f,%.6f],"u":[%.6f,%.6f,%.6f],"v":[%.4f,%.4f,%.4f],"sp":%d,"w":[%s],"sl":[%s]}',
    d.seq, d.st, d.ss or d.st, p[1], p[2], p[3], f[1], f[2], f[3], u[1], u[2], u[3], vel[1], vel[2], vel[3], spinAll and 1 or 0, table.concat(parts, ','), table.concat(slips, ','))
  udp:sendto(s, d.ip, d.port)
  d.poses = d.poses + 1
  stats.tx = stats.tx + 1
end

-- v2.71: the driver's eye in the car's frame (right, forward, up from the car's position: the frame of the poses' wheels),
-- from BeamNG's driver camera (camerasInternal, type "driver"; jbeam space: right = -x, forward = -y, up = z). The jbeam ->
-- frame shift comes from the car's nodes (their jbeam positions against where they are now), so it doesn't matter which
-- node the car's position is. GTA puts its player's head there. UNVERIFIED: the vdata field names (the log says what it found)
local function driverEye(veh)
  local vd
  if core_vehicle_manager.getVehicleData then vd = core_vehicle_manager.getVehicleData(veh:getID()) end
  if not (vd and vd.vdata and vd.vdata.camerasInternal) then vd = core_vehicle_manager.getPlayerVehicleData() or vd end
  local vdata = vd and vd.vdata
  if not vdata then return nil, 'no vehicle data' end
  local cams = vdata.camerasInternal
  if type(cams) ~= 'table' then return nil, 'no camerasInternal' end
  local c, how
  for k, e in pairs(cams) do
    if type(e) == 'table' and tonumber(e.x) and tonumber(e.y) and tonumber(e.z) and (e.type == 'driver' or k == 'driver' or e.name == 'driver') then c, how = e, 'driver camera'; break end
  end
  if not c then
    for _, e in pairs(cams) do
      if type(e) == 'table' and tonumber(e.x) and tonumber(e.y) and tonumber(e.z) then c, how = e, 'first inside camera (' .. tostring(e.type or e.name) .. ')'; break end
    end
  end
  if not c then return nil, 'no inside camera with a position' end
  local f = norm(v3(veh:getDirectionVector()))
  local u0 = v3(veh:getDirectionVectorUp())
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local acc, n = { 0, 0, 0 }, 0
  for cid, nd in pairs(vdata.nodes or {}) do
    if n >= 64 then break end
    local jp = type(nd) == 'table' and nd.pos
    local jx, jy, jz = jp and tonumber(jp.x or jp[1]), jp and tonumber(jp.y or jp[2]), jp and tonumber(jp.z or jp[3])
    if jx and jy and jz then
      local okP, q = pcall(function() return v3(veh:getNodePosition(nd.cid or cid)) end)
      if okP and q then
        acc[1] = acc[1] + dot(q, r) + jx
        acc[2] = acc[2] + dot(q, f) + jy
        acc[3] = acc[3] + dot(q, u) - jz
        n = n + 1
      end
    end
  end
  if n == 0 then return nil, how .. ', but no node positions to place it' end
  local e = { -tonumber(c.x) + acc[1] / n, -tonumber(c.y) + acc[2] / n, tonumber(c.z) + acc[3] / n }
  return e, string.format('%s at jbeam %.2f %.2f %.2f, from %d nodes', how, tonumber(c.x), tonumber(c.y), tonumber(c.z), n)
end

-- v2.72: the same in the car's own Lua (v.data.camerasInternal; its nodes' jbeam positions against where they are now)
dents.EYE = [==[   -- (a field: Lua's 200-locals limit, no globals)
do
  local cams, c, how = v.data and v.data.camerasInternal, nil, nil
  if type(cams) == 'table' then
    for k, e in pairs(cams) do
      if type(e) == 'table' and tonumber(e.x) and tonumber(e.y) and tonumber(e.z) and (e.type == 'driver' or k == 'driver' or e.name == 'driver') then c, how = e, 'driver camera'; break end
    end
    if not c then for _, e in pairs(cams) do if type(e) == 'table' and tonumber(e.x) then c, how = e, 'first inside camera (' .. tostring(e.type or e.name) .. ')'; break end end end
  end
  local function tell(e, h)
    obj:queueGameEngineLua(string.format('if gtaBridge and gtaBridge.eyeFromCar then gtaBridge.eyeFromCar(%s, %q) end', e and string.format('{%.4f,%.4f,%.4f}', e[1], e[2], e[3]) or 'nil', h))
  end
  if not c then tell(nil, 'no camerasInternal in the car either') else
    local f, u = obj:getDirectionVector(), obj:getDirectionVectorUp()
    local fl = math.sqrt(f.x * f.x + f.y * f.y + f.z * f.z)
    local fx, fy, fz = f.x / fl, f.y / fl, f.z / fl
    local du = u.x * fx + u.y * fy + u.z * fz
    local ux, uy, uz = u.x - fx * du, u.y - fy * du, u.z - fz * du
    local ul = math.sqrt(ux * ux + uy * uy + uz * uz)
    ux, uy, uz = ux / ul, uy / ul, uz / ul
    local rx, ry, rz = fy * uz - fz * uy, fz * ux - fx * uz, fx * uy - fy * ux
    local a1, a2, a3, n = 0, 0, 0, 0
    for cid, nd in pairs(v.data.nodes or {}) do
      if n >= 64 then break end
      local jp = type(nd) == 'table' and nd.pos
      if jp and tonumber(jp.x) then
        local q = obj:getNodePosition(nd.cid or cid)
        a1 = a1 + (q.x * rx + q.y * ry + q.z * rz) + jp.x
        a2 = a2 + (q.x * fx + q.y * fy + q.z * fz) + jp.y
        a3 = a3 + (q.x * ux + q.y * uy + q.z * uz) - jp.z
        n = n + 1
      end
    end
    if n == 0 then tell(nil, how .. ', but no node positions') else
      tell({ -tonumber(c.x) + a1 / n, -tonumber(c.y) + a2 / n, tonumber(c.z) + a3 / n }, string.format('%s at jbeam %.2f %.2f %.2f, from %d nodes (the car\'s Lua)', how, c.x, c.y, c.z, n))
    end
  end
end
]==]

-- v2.0's direct inputs (fallback when your bindings can't be used). Only the parts listed in 'want'.
local function driveInputCmd(thr, brk, str, hb, flt, want)
  local t = {}
  if not want or want.thr then t[#t + 1] = string.format("input.event('throttle',%.3f,%d)", thr, flt) end
  if not want or want.brk then t[#t + 1] = string.format("input.event('brake',%.3f,%d)", brk, flt) end
  if not want or want.str then t[#t + 1] = string.format("input.event('steering',%.3f,%d)", str, flt) end
  if not want or want.hb then t[#t + 1] = string.format("input.event('parkingbrake',%d,%d)", hb, flt) end
  return table.concat(t, ' ')
end

-- in the car's Lua: report the first error of each action back here (gtaBridge.vErr)
local VEH_HELPER = [[gtaBridgeReport = function(a, e) gtaBridgeErrs = gtaBridgeErrs or {} if not gtaBridgeErrs[a] then gtaBridgeErrs[a] = true obj:queueGameEngineLua(string.format("if gtaBridge and gtaBridge.vErr then gtaBridge.vErr(%q, %q) end", tostring(a), tostring(e))) end end]]
-- in the car's Lua: what the car actually does (throttle in/out, brake, steering, clutch, gear, rpm...)
local VEH_STATE = [[
local e = electrics.values
local function n(x) x = tonumber(x) return x and string.format('%.3f', x) or '-' end
local s = table.concat({ n(e.throttle_input), n(e.brake_input), n(e.steering_input), n(e.parkingbrake_input), n(e.clutch_input),
  n(e.throttle), n(e.brake), tostring(e.gear), n(e.rpm), n(e.wheelspeed), tostring(e.ignitionLevel), tostring(e.engineRunning),
  tostring(e.gearboxMode), n(e.parkingbrake), n(e.maxrpm), n(e.lowbeam), n(e.highbeam), n(e.brakelights), n(e.reverse), n(e.signal_L), n(e.signal_R) }, '|')
-- v2.4: the damage BeamNG counts (did a teleport repair the car?) and the ground ids under the wheels (does a GTA dirt
-- tile really give dirt?). Both UNVERIFIED names, so each is read inside pcall; '-' = not available.
local dmg, gm = '-', {}
pcall(function() dmg = string.format('%.0f', beamstate.damage) end)
pcall(function()
  for _, w in pairs(wheels.wheelRotators) do
    if type(w) == 'table' and #gm < 8 then gm[#gm + 1] = tostring(w.contactMaterialID1) end
  end
end)
s = s .. '|' .. dmg .. '|' .. (#gm > 0 and table.concat(gm, ',') or '-')
-- v2.5: the car's drive mode (your ESC modes: Comfort, Sport, ESC & TC off...): the esc controller's current configuration
-- (its name, as BeamNG shows it when you switch), else the driveModes controller's current mode. Each read in pcall: '-' = none
local mode, probe = '-', '-'
pcall(function()
  local c = controller.getController('esc')
  if c and c.getCurrentConfigData then
    local d = c.getCurrentConfigData()
    if type(d) == 'table' and d.name then mode = tostring(d.name) end
  end
end)
if mode == '-' then pcall(function()
  local c = controller.getController('driveModes')
  if c and c.getCurrentDriveModeKey then
    local k = c.getCurrentDriveModeKey()
    if k ~= nil then mode = tostring(k) end
  end
end) end
if not gtabModeProbed then
  -- once: what the two controllers offer (for the log, in case the names differ in your BeamNG)
  gtabModeProbed = true
  local names = {}
  for _, cn in ipairs({ 'esc', 'driveModes' }) do
    pcall(function()
      local c = controller.getController(cn)
      if type(c) == 'table' then
        local fns = {}
        for k, v in pairs(c) do if type(v) == 'function' and #fns < 24 then fns[#fns + 1] = tostring(k) end end
        table.sort(fns)
        names[#names + 1] = cn .. ': ' .. table.concat(fns, ' ')
      end
    end)
  end
  probe = #names > 0 and table.concat(names, '; ') or 'no esc or driveModes controller'
end
s = s .. '|' .. mode:gsub('[|"\\]', '/') .. '|' .. probe:gsub('[|"\\]', '/')
obj:queueGameEngineLua(string.format('if gtaBridge and gtaBridge.vstate then gtaBridge.vstate(%d, %q) end', obj:getID(), s))
]]

-- VALUE, FILTERTYPE, ANGLE, LOCKTYPE, PLAYER, VEHICLEID in an action's code (BeamNG docs). Numbers go in
-- brackets: "-VALUE" with a negative value must not become "--0.5" (a Lua comment).
local function fillCode(code, val, b, vid)
  local s = tostring(code)
  s = s:gsub('%f[%w_]VALUE%f[^%w_]', string.format('(%.4f)', val))
  s = s:gsub('%f[%w_]FILTERTYPE%f[^%w_]', tostring(math.floor(b.flt or 0)))
  s = s:gsub('%f[%w_]ANGLE%f[^%w_]', string.format('(%.3f)', b.angle or 0))
  s = s:gsub('%f[%w_]LOCKTYPE%f[^%w_]', tostring(math.floor(b.lock or 0)))
  s = s:gsub('%f[%w_]PLAYER%f[^%w_]', '0')
  s = s:gsub('%f[%w_]VEHICLEID%f[^%w_]', tostring(vid or 0))
  return s
end

local function runAction(d, b, ev, val)
  local code = b.def[ev]
  if type(code) ~= 'string' or code == '' then return end
  local s = fillCode(code, val, b, d.id)
  local ctx = tostring(b.def.ctx or 'tlua')
  if ctx == 'vlua' or ctx == 'bvlua' then
    d.vq[#d.vq + 1] = string.format('do local f, e = (loadstring or load)(%q) local ok = false if f then ok, e = pcall(f) end if not ok and gtaBridgeReport then gtaBridgeReport(%q, e) end end',
      s, b.action)
  else
    local f, err = (loadstring or load)(s)
    local ok, e2 = false, err
    if f then ok, e2 = pcall(f) end
    if not ok and not d.geErr[b.action] then
      d.geErr[b.action] = true
      logE('drive: action ' .. b.action .. ' (' .. ctx .. ') failed: ' .. tostring(e2))
    end
  end
  d.acts = d.acts + 1
  if ev ~= 'onUp' and not (ev == 'onChange' and val == 0) then
    d.recent[b.action] = true
    d.seenActs[b.action] = true
  end
end

local function fireBind(d, b, v)
  if b.def.onChange then runAction(d, b, 'onChange', v) end
  local down = v >= 0.5
  if down ~= b.down then
    b.down = down
    if down and b.nMods and b.nMods > 0 then
      -- v2.51: which of your combinations ran (your v2.5 test: RB + LB + D-pad down did something else than your repair)
      d.comboLogs = (d.comboLogs or 0) + 1
      if d.comboLogs <= 40 then logI('drive: pad combination ' .. tostring(b.ctlName) .. ' -> ' .. tostring(b.action) .. (b.def.repair and ' (repair where the car is)' or '')) end
    end
    runAction(d, b, down and 'onDown' or 'onUp', down and 1 or 0)
  end
  b.last = v
end

-- raw pad value -> what the binding hands the action (inversion, deadzones, linearity: approximated)
local function shapeValue(b, v)
  if b.xf == 'neg' then v = -v elseif b.xf == 'trig' then v = v * 2 - 1 end
  if b.inv then
    if b.ctlCentred then v = -v else v = 1 - v end
  end
  local sgn = v < 0 and -1 or 1
  local a = math.abs(v)
  if b.dzR > 0 then a = a <= b.dzR and 0 or (a - b.dzR) / (1 - b.dzR) end
  if b.dzE > 0 then a = a / (1 - b.dzE) end
  a = math.min(1, a)
  if b.lin ~= 1 and b.lin > 0 then a = a ^ b.lin end
  v = sgn * a
  if b.xf == 'trig' then v = (v + 1) * 0.5 elseif b.ctlCentred and not b.centred then v = math.max(0, v) end
  return math.floor(v * 1000 + 0.5) / 1000
end

local function padValue(d, b)
  local p = d.pad
  if not p then return 0 end
  local ai = PAD_AXES[b.ctl]
  if ai then
    if b.xf == 'btn' then return (p[ai] or 0) > 0.5 and 1 or 0 end
    return p[ai] or 0
  end
  local bit = PAD_BTN[b.ctl]
  return (bit and math.floor((p.btn or 0) / bit) % 2 == 1) and 1 or 0
end

local function keyDown(keys, key)
  local any = MOD_KEYS[key]
  if any then return keys[any[1]] or keys[any[2]] end
  return keys[key] and true or false
end

-- one pass over your bindings with the newest state from GTA
local function evalBinds(d)
  local B = d.binds
  if not B then return end
  local keys = d.keys or {}
  B.modLatch = B.modLatch or {}
  local held = { shift = keyDown(keys, 'shift'), ctrl = keyDown(keys, 'ctrl'), alt = keyDown(keys, 'alt') }
  -- keys: a binding with modifiers needs exactly those held; a plain one fires when no binding with the
  -- held modifiers exists on that key (Torque's rule: exact modifiers first, else without them)
  local modHit = {}
  for _, b in ipairs(B.key) do
    if b.nMods > 0 and not b.isModKey then
      local on = keyDown(keys, b.key) and (b.mods.shift or false) == (held.shift or false) and (b.mods.ctrl or false) == (held.ctrl or false)
                 and (b.mods.alt or false) == (held.alt or false)
      b.cur = on and 1 or 0
      if on then modHit[b.key] = true; B.modLatch[b.key] = true end
    end
  end
  -- Torque gives the release to the binding that got the press: after ctrl+q, letting go of ctrl first must
  -- not press plain q
  -- latched at the start of this frame: no plain tap for them this frame (the release frame of a combination)
  local keyLatchWas = {}
  for k in pairs(B.modLatch) do keyLatchWas[k] = true end
  for k in pairs(B.modLatch) do if not keyDown(keys, k) then B.modLatch[k] = nil end end
  for _, b in ipairs(B.key) do
    if b.nMods == 0 or b.isModKey then b.cur = (keyDown(keys, b.key) and (b.isModKey or not (modHit[b.key] or B.modLatch[b.key]))) and 1 or 0 end
  end
  -- pads (v2.1): BeamNG's pad modifiers work like the keyboard's: a combination needs exactly its modifier
  -- buttons held; a plain binding on that control is quiet while a combination on it is (or was) active
  local p = d.pad
  local function padDown(name)
    if not p then return false end
    local bit = PAD_BTN[name]
    return bit ~= nil and math.floor((p.btn or 0) / bit) % 2 == 1
  end
  B.padLatch = B.padLatch or {}
  local mh = {}
  -- v2.5: GTA also sends the modifier buttons held as modifiers ("pm"), even while their own actions wait to be let go
  -- (held across a pad mode switch): your combinations work at once
  local function pmDown(name)
    local bit = PAD_BTN[name]
    return p ~= nil and bit ~= nil and math.floor((p.pm or 0) / bit) % 2 == 1
  end
  for n, nm in pairs(B.modCtl or {}) do mh[n] = padDown(nm) or pmDown(nm) end
  local function modsExact(b)
    for n = 1, 3 do if (b.mods and b.mods[n] or false) ~= (mh[n] or false) then return false end end
    return true
  end
  local comboHit = {}
  for _, b in ipairs(B.pad) do
    if b.nMods and b.nMods > 0 then
      local on = modsExact(b)
      b.cur = on and shapeValue(b, padValue(d, b)) or 0
      if on and b.cur ~= 0 then comboHit[b.ctl] = true; B.padLatch[b.ctl] = true end
    end
  end
  local padLatchWas = {}
  for k in pairs(B.padLatch) do padLatchWas[k] = true end
  for k in pairs(B.padLatch) do if not padDown(k) and not comboHit[k] then B.padLatch[k] = nil end end
  for _, b in ipairs(B.pad) do
    if not b.nMods or b.nMods == 0 then
      b.cur = (comboHit[b.ctl] or B.padLatch[b.ctl]) and 0 or shapeValue(b, padValue(d, b))
    end
  end
  -- a button/key pressed and let go between two BeamNG frames (GTA sends more often): press + release now
  local taps = d.tapBtn or {}
  local tapK = d.tapKeys or {}
  for _, b in ipairs(B.key) do
    if b.cur == 0 and b.last == 0 and b.nMods == 0 and not b.isModKey and tapK[b.key] and not B.modLatch[b.key] and not keyLatchWas[b.key] then fireBind(d, b, 1) end
  end
  for _, b in ipairs(B.pad) do
    if not b.axis and b.cur == 0 and b.last == 0 and taps[b.ctl] then
      if b.nMods and b.nMods > 0 then
        if modsExact(b) then fireBind(d, b, 1) end
      elseif not B.padLatch[b.ctl] and not padLatchWas[b.ctl] then
        local comboNow = false
        for _, c in ipairs(B.pad) do if c.ctl == b.ctl and c.nMods and c.nMods > 0 and modsExact(c) then comboNow = true end end
        if not comboNow then fireBind(d, b, 1) end
      end
    end
  end
  d.tapBtn, d.tapKeys = {}, {}
  for _, b in ipairs(B.key) do if b.cur ~= b.last then fireBind(d, b, b.cur) end end
  for _, b in ipairs(B.pad) do if b.cur ~= b.last then fireBind(d, b, b.cur) end end
end

-- every bound control back to rest (onUp / 0), e.g. when GTA stops sending
local function releaseBinds(d)
  local B = d.binds
  if not B then return end
  for _, list in ipairs({ B.key, B.pad }) do
    for _, b in ipairs(list) do
      if b.last ~= 0 or b.down then fireBind(d, b, 0) end
    end
  end
end

local function flushVehicle(d, veh)
  if #d.vq > 0 then
    veh:queueLuaCommand(table.concat(d.vq, '\n'))
    d.vq = {}
  end
end

local function driveApply(veh)
  local d = drive
  local t = now()
  local i = d.inp
  local stale = t - d.lastInputT > driveCfg.failsafeS or d.paused
  if stale then
    -- GTA went quiet: everything let go; brake while fast (a parking brake at speed spins the car), then the
    -- parking brake (a held brake at a standstill makes the automatic gearbox reverse)
    if not d.staleLogged then
      logI(d.paused and 'drive: GTA is paused: controls let go, throttle off, brake / parking brake on'
        or ('drive: no controls from GTA for ' .. driveCfg.failsafeS .. ' s: controls let go, throttle off, parking brake on'))
      d.staleLogged = true
      releaseBinds(d)
    end
    flushVehicle(d, veh)
    d.tapBtn, d.tapKeys = {}, {}
    local v = d.vel or { 0, 0, 0 }
    local fast = math.sqrt(dot(v, v)) > 3
    local key = string.format('stale|%d', fast and 1 or 0)
    if key ~= d.lastKey or t - d.lastApplyT > driveCfg.refreshS then
      -- its own command: nothing else in the queue can stop it
      veh:queueLuaCommand(driveInputCmd(0, fast and 0.6 or 0, 0, fast and 0 or 1, 2) .. " input.event('clutch',0,2)")
      d.lastKey, d.lastApplyT = key, t
      d.applied = d.applied + 1
    end
    return
  elseif d.staleLogged then
    d.staleLogged = false
    d.lastKey = nil
    -- the failsafe's brake / parking brake off again (your bindings take over from here)
    d.vq[#d.vq + 1] = "input.event('brake',0,2) input.event('parkingbrake',0,2)"
    logI('drive: controls from GTA again')
  end
  if d.binds then evalBinds(d) end
  if d.fallback then
    local key = string.format('%.3f|%.3f|%.3f|%d|%d', i.thr, i.brk, i.str, i.hb, i.flt)
    if key ~= d.lastKey or t - d.lastApplyT > driveCfg.refreshS then
      d.vq[#d.vq + 1] = driveInputCmd(i.thr, i.brk, i.str, i.hb, i.flt, d.fallback)
      d.lastKey, d.lastApplyT = key, t
      d.applied = d.applied + 1
    end
  end
  flushVehicle(d, veh)
end

-- the car's own numbers (from VEH_STATE in its Lua)
function M.vstate(id, s)
  local d = drive
  if not d or (d.id ~= nil and id ~= d.id) then return end
  local f = {}
  for x in (tostring(s) .. '|'):gmatch('([^|]*)|') do f[#f + 1] = x end
  local function n(i) return tonumber(f[i]) end
  d.vs = { thrIn = n(1), brkIn = n(2), strIn = n(3), pbIn = n(4), cluIn = n(5), thr = n(6), brk = n(7), gear = f[8], rpm = n(9),
           ws = n(10), ign = f[11], eng = f[12], gbm = f[13], pb = n(14), mx = n(15), t = now(),
           dmg = n(22), gm = f[23], esc = f[24] }   -- v2.4: damage, ground ids under the wheels; v2.5: drive mode
  if f[25] and f[25] ~= '-' then logI('drive: the car\'s drive mode controllers: ' .. tostring(f[25])) end
  if d.vs.esc ~= d.escWas then
    -- v2.5: the drive mode shown on GTA's gauge
    d.escWas = d.vs.esc
    d.escChanges = (d.escChanges or 0) + 1
    if d.escChanges <= 30 then logI('drive: drive mode now ' .. tostring(d.vs.esc)) end
  end
  if d.vs.gm and d.vs.gm ~= '-' and d.vs.gm ~= d.gmWas then
    -- v2.4: logged when the ground under the wheels changes (does a GTA dirt tile give BeamNG's dirt?)
    d.gmWas = d.vs.gm
    d.gmChanges = (d.gmChanges or 0) + 1
    if d.gmChanges <= 40 then logI('drive: ground ids under the wheels now ' .. tostring(d.vs.gm)) end
  end
  -- v2.3: lights as bits: 1 low beams, 2 high beams, 4 brake lights, 8 reverse, 16 left signal, 32 right signal
  local li = 0
  for bit, i in pairs({ [1] = 16, [2] = 17, [4] = 18, [8] = 19, [16] = 20, [32] = 21 }) do
    if (n(i) or 0) > 0.5 then li = li + bit end
  end
  d.vs.li = li
  local la = {}
  for a in pairs(d.recent) do la[#la + 1] = a end
  table.sort(la)
  d.recent = {}
  local vs = d.vs
  local function clean(x) return (tostring(x):gsub('[%c"\\]', '')) end
  local msg = string.format('{"t":"vs","thr":%.3f,"brk":%.3f,"str":%.3f,"pb":%.3f,"clu":%.3f,"gear":"%s","rpm":%.0f,"mx":%.0f,"ws":%.2f,"li":%d,"ign":"%s","eng":"%s","gbm":"%s","la":"%s","inp":"%s","dmg":%.0f,"gm":"%s","esc":"%s"}',
    vs.thrIn or -1, vs.brkIn or -1, vs.strIn or 0, vs.pbIn or vs.pb or -1, vs.cluIn or -1, clean(vs.gear), vs.rpm or -1, vs.mx or -1, vs.ws or 0, vs.li or 0,
    clean(vs.ign), clean(vs.eng), clean(vs.gbm), clean(table.concat(la, ','):sub(1, 200)), clean(d.inpHow), vs.dmg or -1, clean(vs.gm or '-'), clean(vs.esc or '-'):sub(1, 40))
  pcall(function() udp:sendto(msg, d.ip, d.port) end)
  -- did the throttle we asked for arrive? (once, a few seconds in)
  if not d.checked and d.wantThrT and now() - d.wantThrT > 1.5 and (vs.thrIn or 0) < 0.05 then
    d.checked = true
    logE(string.format('drive: GTA asks for throttle but the car\'s throttle input stays at %.2f (gear %s, engine %s, ignition %s, gearbox %s)',
      vs.thrIn or -1, tostring(vs.gear), tostring(vs.eng), tostring(vs.ign), tostring(vs.gbm)))
  end
end

function M.vErr(action, err)
  logE('drive: in the car, action ' .. tostring(action) .. ' failed: ' .. tostring(err))
end

-- ---------------------------------------------------------------- v2.2: BeamNG settings while GTA drives
-- BeamNG mutes itself when its window loses focus. v2.1 tried a Torque pref that BeamNG 0.39 no longer has.
-- The real option (found in your settings dump): AudioMuteOnWindowLoseFocus (Options > Audio). While GTA
-- drives it is set to false so BeamNG's car sound plays from the background, and afterwards it is put back to
-- WHATEVER IT WAS (if you turned it off yourself, it stays off). Optional (GTA's BeamNGBackgroundFps): BeamNG's
-- background frame limit (fpsLimitBackgroundEnabled / fpsLimitBackground) changed the same way.
-- What was changed is written to sound_restore.txt first, so a Lua reload or a crash still puts it back.
-- UNVERIFIED: whether settings.setValue applies at once (the read-back is logged).
local SOUND_KEY = 'AudioMuteOnWindowLoseFocus'
local BGFPS_ON, BGFPS = 'fpsLimitBackgroundEnabled', 'fpsLimitBackground'
local SOUND_FILE = '/gtabridge/sound_restore.txt'   -- what to put back (survives a Lua reload or a crash)
local saved = { list = {}, active = false }          -- { key, old value } in the order changed

local function settingsApi()
  for _, t in ipairs({ rawget(_G, 'settings'), rawget(_G, 'core_settings_settings'), extensions and extensions.core_settings_settings }) do
    if type(t) == 'table' and type(t.getValue) == 'function' and type(t.setValue) == 'function' then return t end
  end
  return nil
end
local function getS(st, k) local ok, v = pcall(st.getValue, k) if ok then return v end return nil end
local function valText(v) return type(v) .. ':' .. tostring(v) end
local function textVal(s)
  local ty, v = tostring(s):match('^(%a+):(.*)$')
  if ty == 'boolean' then return v == 'true' end
  if ty == 'number' then return tonumber(v) end
  if ty == 'string' then return v end
  return nil
end
local function writeRestore()
  local note = {}
  for _, e in ipairs(saved.list) do note[#note + 1] = e[1] .. '=' .. valText(e[2]) end
  pcall(writeFile, SOUND_FILE, table.concat(note, '\n'))
end
-- change one setting for the drive; the first old value is kept
local function changeSetting(st, k, v, res)
  local old = getS(st, k)
  if old == nil then res[#res + 1] = k .. ' not found'; return false end
  if old == v then res[#res + 1] = k .. ' already ' .. tostring(v); return false end
  local seen = false
  for _, e in ipairs(saved.list) do if e[1] == k then seen = true end end
  if not seen then saved.list[#saved.list + 1] = { k, old }; writeRestore() end
  local ok, err = pcall(st.setValue, k, v)
  res[#res + 1] = string.format('%s %s -> %s: %s, reads back %s', k, tostring(old), tostring(v), ok and 'ok' or ('FAILED ' .. tostring(err)), tostring(getS(st, k)))
  return ok and getS(st, k) == v
end

-- ---- v2.4: sound "session" mode (GTA's BeamNGSoundWake=session). The bounce (BeamNG's window in front for a moment
-- at each drive start/end) is needed because BeamNG only looks at its mute option when its window loses focus. In
-- session mode the option is switched off ONCE, at load while BeamNG still has the focus (you start BeamNG first),
-- and stays off until BeamNG closes: BeamNG plays sound in the background all session. To keep it quiet while you
-- walk in GTA, its master volume is turned down outside drive mode and back up when a drive starts (UNVERIFIED:
-- the volume setting's name, logged at load, and whether it applies while BeamNG is in the background).
-- The original values are written to session_restore.txt first (a crash or Lua reload puts them back).
local session, modConfig, writeModConfig, sessionOn, sessionOff, sessionVolume, readModConfig, restoreSessionFromFile
do
local SESSION_FILE = '/gtabridge/session_restore.txt'
local CONFIG_FILE = '/gtabridge/config.json'    -- the mod's own remembered choices (sound session mode)
session = { on = false, list = {}, volKey = nil, vol = nil, quiet = 0, quietNow = false }
-- v2.41: concreteGrip (CONCRETE's grip from your ASPHALT's), tpKeepDamage (false = the teleport repairs the car, as in v2.4)
modConfig = { soundSession = false, quietVolume = 0, concreteGrip = 0.85, tpKeepDamage = true }

readModConfig = function()
  local ok, t = pcall(jsonReadFile, CONFIG_FILE)
  if ok and type(t) == 'table' then
    modConfig.soundSession = t.soundSession == true
    modConfig.quietVolume = tonumber(t.quietVolume) or 0
    -- v2.42: 0.85 (you asked for 80-90 % of asphalt's grip); a 0.97 left by 2.41 counts as "not set"
    local g = tonumber(t.concreteGrip)
    if g == nil or math.abs(g - 0.97) < 1e-6 then g = 0.85 end
    modConfig.concreteGrip = math.max(0.3, math.min(1.5, g))
    modConfig.tpKeepDamage = t.tpKeepDamage ~= false
  end
end
writeModConfig = function()
  pcall(writeFile, CONFIG_FILE, jsonEncode({ soundSession = modConfig.soundSession, quietVolume = modConfig.quietVolume, concreteGrip = modConfig.concreteGrip,
    tpKeepDamage = modConfig.tpKeepDamage }))
end
local function writeSessionRestore()
  local note = {}
  for _, e in ipairs(session.list) do note[#note + 1] = e[1] .. '=' .. valText(e[2]) end
  pcall(writeFile, SESSION_FILE, table.concat(note, '\n'))
end
-- the master volume setting: the first of these names BeamNG has (UNVERIFIED: AudioMasterVol)
local function volumeKey(st)
  for _, k in ipairs({ 'AudioMasterVol', 'AudioMasterVolume', 'masterVolume', 'AudioVolumeMaster' }) do
    if type(getS(st, k)) == 'number' then return k end
  end
  return nil
end
-- every Audio* setting BeamNG has, for the log (to find the volume's real name if the guess above is wrong)
local function audioSettingsText(st)
  local keys = {}
  pcall(function()
    local impl = st.impl or rawget(st, 'impl')
    local dv = impl and (impl.defaultValues or impl.values)
    if type(dv) == 'table' then
      for k in pairs(dv) do if tostring(k):lower():find('audio') then keys[#keys + 1] = tostring(k) end end
    end
  end)
  table.sort(keys)
  local parts = {}
  for _, k in ipairs(keys) do parts[#parts + 1] = k .. '=' .. tostring(getS(st, k)) end
  return #parts > 0 and table.concat(parts, ', ') or '(BeamNG\'s list of settings not readable)'
end
-- remember a setting's original value once for the session, then set it
local function sessionSet(st, k, v, res)
  local old = getS(st, k)
  if old == nil then res[#res + 1] = k .. ' not found'; return false end
  local seen = false
  for _, e in ipairs(session.list) do if e[1] == k then seen = true end end
  if not seen then session.list[#session.list + 1] = { k, old }; writeSessionRestore() end
  if old == v then res[#res + 1] = k .. ' already ' .. tostring(v); return false end
  local ok, err = pcall(st.setValue, k, v)
  res[#res + 1] = string.format('%s %s -> %s: %s, reads back %s', k, tostring(old), tostring(v), ok and 'ok' or ('FAILED ' .. tostring(err)), tostring(getS(st, k)))
  return ok and getS(st, k) == v
end
local function sessionOriginal(k)
  for _, e in ipairs(session.list) do if e[1] == k then return e[2] end end
  return nil
end
-- quiet = true: BeamNG's volume down (GTA isn't driving); false: its own volume back (GTA drives)
sessionVolume = function(quiet, why)
  if not session.on then return end
  local st = settingsApi()
  if not st then return end
  session.volKey = session.volKey or volumeKey(st)
  if not session.volKey then
    if not session.volWarned then session.volWarned = true; logE('sound session: no master volume setting found; BeamNG stays at its volume while you walk (' .. audioSettingsText(st) .. ')') end
    return
  end
  local res = {}
  if quiet then
    local orig = sessionOriginal(session.volKey) or getS(st, session.volKey)
    sessionSet(st, session.volKey, (tonumber(orig) or 1) * modConfig.quietVolume, res)
  else
    local orig = sessionOriginal(session.volKey)
    if orig ~= nil then
      local ok = pcall(st.setValue, session.volKey, orig)
      res[#res + 1] = string.format('%s back to %s%s', session.volKey, tostring(orig), ok and '' or ' (FAILED)')
    end
  end
  session.quietNow = quiet
  logI('sound session: ' .. (quiet and 'BeamNG quiet' or 'BeamNG\'s volume back') .. ' (' .. tostring(why) .. '): ' .. table.concat(res, '; '))
end
-- switch the session mode on: returns true when the mute option was really changed now (BeamNG then needs one focus
-- change to notice: the GTA plugin wakes it once)
sessionOn = function(why)
  local st = settingsApi()
  if not st then logE('sound session: BeamNG\'s settings API not found'); return false end
  local res = {}
  local changed = sessionSet(st, SOUND_KEY, false, res)
  session.on = true
  logI('sound session ON (' .. tostring(why) .. '): ' .. table.concat(res, '; ') .. '; audio settings: ' .. audioSettingsText(st))
  return changed
end
sessionOff = function(why)
  if not session.on and #session.list == 0 then return end
  local st = settingsApi()
  local res = {}
  if st then
    for i = #session.list, 1, -1 do
      local e = session.list[i]
      local ok = pcall(st.setValue, e[1], e[2])
      res[#res + 1] = e[1] .. ' put back to ' .. tostring(e[2]) .. (ok and '' or ' (FAILED)')
    end
  end
  session.list, session.on, session.quietNow = {}, false, false
  pcall(writeFile, SESSION_FILE, '')
  logI('sound session OFF (' .. tostring(why) .. '): ' .. table.concat(res, '; '))
end
restoreSessionFromFile = function()
  local ok, txt = pcall(function() return readFile(SESSION_FILE) end)
  if not ok or type(txt) ~= 'string' or txt == '' then return end
  local st = settingsApi()
  local back = {}
  if st then
    for line in txt:gmatch('[^\n]+') do
      local k, v = line:match('^([%w_]+)=(.+)$')
      local val = v and textVal(v)
      if k and val ~= nil and pcall(st.setValue, k, val) then back[#back + 1] = k .. ' = ' .. tostring(val) end
    end
  end
  pcall(writeFile, SESSION_FILE, '')
  if #back > 0 then logI('sound session: put back what an earlier run changed: ' .. table.concat(back, ', ')) end
end
function M.soundSession(on, quietVolume)
  if quietVolume ~= nil then modConfig.quietVolume = math.max(0, math.min(1, tonumber(quietVolume) or 0)) end
  modConfig.soundSession = on and true or false
  writeModConfig()
  if on then
    sessionOn('gtaBridge.soundSession(true)')
    if not driveActive then sessionVolume(true, 'not driving') end
  else
    sessionOff('gtaBridge.soundSession(false)')
  end
  screen('GTA bridge: sound session mode ' .. (on and 'ON' or 'OFF'), 6)
end
end

-- snd: 0 none, 1 BeamNG's sound in the background for this drive (v2.2 way), 2 session mode (v2.4);
-- bgf: -1 keep, 0 no background limit, n = limit n
local function driveSettings(on, snd, bgf, quietVol)
  local st = settingsApi()
  if not st then
    if on then logE('settings: BeamNG\'s settings API (settings.getValue/setValue) not found: sound and fps settings not changed') end
    return
  end
  local res = {}
  if on then
    res[#res + 1] = string.format('found %s=%s, %s=%s, %s=%s', SOUND_KEY, tostring(getS(st, SOUND_KEY)), BGFPS_ON, tostring(getS(st, BGFPS_ON)),
      BGFPS, tostring(getS(st, BGFPS)))
    local woke = false
    snd = tonumber(snd) or (snd and 1 or 0)
    if snd == 2 then
      -- v2.4 session mode: switched on once (and remembered for the next BeamNG start); its volume back for the drive
      if quietVol ~= nil then modConfig.quietVolume = math.max(0, math.min(1, tonumber(quietVol) or 0)) end
      if not modConfig.soundSession then modConfig.soundSession = true; writeModConfig() end
      if not session.on then woke = sessionOn('GTA asked for session mode') end
      sessionVolume(false, 'drive start')
      res[#res + 1] = 'sound: session mode'
    else
      if session.on or modConfig.soundSession then
        modConfig.soundSession = false
        writeModConfig()
        sessionOff('GTA uses ' .. (snd == 1 and 'the per-drive switch (bounce)' or 'no BeamNG sound'))
      end
      if snd == 1 then woke = changeSetting(st, SOUND_KEY, false, res) end
    end
    bgf = tonumber(bgf) or -1
    if bgf == 0 then changeSetting(st, BGFPS_ON, false, res)
    elseif bgf > 0 then
      changeSetting(st, BGFPS_ON, true, res)
      changeSetting(st, BGFPS, bgf, res)
    end
    saved.active = true
    if woke then res[#res + 1] = 'BeamNG was muted in the background: GTA wakes its window (BeamNGSoundWake)' end
    logI('settings: while GTA drives: ' .. table.concat(res, '; '))
    return woke
  else
    if not saved.active and #saved.list == 0 then return end
    for i = #saved.list, 1, -1 do
      local e = saved.list[i]
      local ok = pcall(st.setValue, e[1], e[2])
      res[#res + 1] = e[1] .. ' put back to ' .. tostring(e[2]) .. (ok and '' or ' (FAILED)')
    end
    saved.list, saved.active = {}, false
    pcall(writeFile, SOUND_FILE, '')
    if #res > 0 then logI('settings: drive over: ' .. table.concat(res, '; ')) end
  end
end

local function restoreSoundFromFile()
  local ok, txt = pcall(function() return readFile(SOUND_FILE) end)
  if not ok or type(txt) ~= 'string' or txt == '' then return end
  local st = settingsApi()
  local back = {}
  if st then
    for line in txt:gmatch('[^\n]+') do
      local k, v = line:match('^([%w_]+)=(.+)$')
      local val = v and textVal(v)
      if k and val ~= nil and pcall(st.setValue, k, val) then back[#back + 1] = k .. ' = ' .. tostring(val) end
    end
  end
  pcall(writeFile, SOUND_FILE, '')
  if #back > 0 then logI('settings: put back what an earlier run changed: ' .. table.concat(back, ', ')) end
end

-- v2.43: pause sync. GTA paused (its pause menu: "gp" in the controls; alt-tabbed: "hold") -> BeamNG's physics pause;
-- GTA runs again -> they run again (only when this mod paused them: a pause you made in BeamNG stays yours). You pause
-- BeamNG -> the mod tells GTA ("pz", by = "user") and GTA opens its pause menu. The pause function is found at run time
-- (simTimeAuthority.pause in current BeamNG, bullettime.pause in older ones) and checked: BeamNG's physics clock must stop.
local pz = { api = nil, byUs = false, took = false, override = false, gta = false, zeroN = 0, paused = false, sentT = 0 }
function pz.findApi()
  if pz.api ~= nil then return pz.api end
  local sta = rawget(_G, 'simTimeAuthority')
  local bt = rawget(_G, 'bullettime')
  if type(sta) == 'table' and type(sta.pause) == 'function' then
    pz.api, pz.set = 'simTimeAuthority.pause', function(on) sta.pause(on) end
    if type(sta.getPause) == 'function' then pz.get = function() return sta.getPause() end end
  elseif type(bt) == 'table' and type(bt.pause) == 'function' then
    pz.api, pz.set = 'bullettime.pause', function(on) bt.pause(on) end
    if type(bt.getPause) == 'function' then pz.get = function() return bt.getPause() end end
  else
    local okB = pcall(function() assert(type(be.setPhysicsRunning) ~= 'nil') end)
    if okB then pz.api, pz.set = 'be:setPhysicsRunning', function(on) be:setPhysicsRunning(not on) end
    else pz.api = false end
  end
  logI('pause: BeamNG\'s pause function: ' .. tostring(pz.api or 'none found (pausing GTA only brakes the car)'))
  return pz.api
end
-- every frame while GTA drives: is BeamNG paused (its physics clock stands still), and by whom
function pz.update(d, dtReal, dtSim)
  local zero = type(dtSim) == 'number' and dtSim <= 0 and (tonumber(dtReal) or 0) > 0
  pz.zeroN = zero and pz.zeroN + 1 or 0
  local paused = pz.zeroN >= 2
  if pz.get then
    local ok, v = pcall(pz.get)
    if ok and v == true then paused = true end
  end
  local t = now()
  -- v2.5: just unpaused by the mod (GTA runs again): BeamNG's clock may still read 0 for a frame or two. Not "paused by you"
  -- (GTA would open its pause menu, pause itself and BeamNG with it: both stuck)
  if t < (pz.graceT or 0) and not pz.byUs then paused, pz.zeroN = false, 0 end
  if pz.byUs then
    if paused then
      pz.took = true
    elseif pz.took and t - pz.byUsT > 0.3 then
      -- the mod's pause ended in BeamNG (you unpaused it there): BeamNG runs until GTA runs and pauses again
      pz.byUs, pz.override = false, true
      logI('pause: you unpaused BeamNG while GTA is paused: it runs (it is paused again the next time GTA is)')
    elseif not pz.took and t - pz.byUsT > 1.5 then
      logE('pause: ' .. tostring(pz.api) .. ' didn\'t stop BeamNG\'s physics: pause sync off (pausing GTA only brakes the car)')
      pz.byUs, pz.api = false, false
      pcall(pz.set, false)
    end
  end
  if paused ~= pz.paused then
    pz.paused = paused
    logI('pause: BeamNG ' .. (paused and ('paused (' .. (pz.byUs and 'by the mod: GTA is paused' or 'by you') .. ')') or 'runs again'))
  end
  if paused and t - pz.sentT >= 0.2 and udp and d.ip then
    pz.sentT = t
    pcall(function() udp:sendto('{"t":"pz","by":"' .. (pz.byUs and 'gta' or 'user') .. '"}', d.ip, d.port) end)
  end
end
-- GTA paused (on) or running again (off)
function pz.gtaPaused(d, on, why)
  if not d.pauseSync or on == pz.gta then return end
  pz.gta = on
  if on then
    if pz.override or pz.paused or pz.byUs then return end   -- already paused (by you: it stays yours)
    if not pz.findApi() then return end
    local ok, err = pcall(pz.set, true)
    if ok then
      pz.byUs, pz.took, pz.byUsT = true, false, now()
      logI('pause: ' .. why .. ': BeamNG paused (' .. pz.api .. ')')
    else
      logE('pause: ' .. pz.api .. ' failed: ' .. tostring(err))
      pz.api = false
    end
  else
    pz.override = false
    if pz.byUs then
      pz.byUs = false
      pz.graceT = now() + 0.5
      pcall(pz.set, false)
      logI('pause: GTA runs again: BeamNG runs again')
    end
  end
end
function pz.reset()
  if pz.byUs then pcall(pz.set, false); logI('pause: the drive ended: BeamNG runs again') end
  pz.byUs, pz.took, pz.override, pz.gta, pz.zeroN, pz.paused = false, false, false, false, 0, false
end
M.pauseState = function() return { api = pz.api, byUs = pz.byUs, paused = pz.paused, gta = pz.gta, override = pz.override } end
M.driveOn = function() return drive ~= nil end

-- v2.5: BeamNG's gravity x DriveGravity (GTA's ini, default 1.1) while GTA drives (your idea: Los Santos felt bouncy). Read
-- and set through BeamNG's environment (core_environment.getGravity / setGravity: UNVERIFIED names, the log says what
-- happened); without a readable gravity nothing is changed (no guessing its sign). Put back when drive mode ends; if
-- BeamNG's Lua reloads meanwhile, gravity_restore.txt puts it back when this mod loads again.
local grav = { was = nil, file = '/gtabridge/gravity_restore.txt' }   -- (one local: Lua allows 200 in the main chunk)
function grav.get()
  local g
  pcall(function() g = core_environment.getGravity() end)
  if type(g) ~= 'number' then pcall(function() g = extensions.core_environment.getGravity() end) end
  return type(g) == 'number' and g or nil
end
function grav.set(g)
  local ok = pcall(function() core_environment.setGravity(g) end)
  if not ok then ok = pcall(function() extensions.core_environment.setGravity(g) end) end
  return ok
end
function grav.on(mult)
  mult = tonumber(mult) or 1
  if math.abs(mult - 1) < 0.001 then return end
  local g0 = grav.get()
  if not g0 then logE('drive: gravity: BeamNG\'s gravity could not be read (core_environment.getGravity): left as it is'); return end
  local base = grav.was or g0   -- (a second drive start: from BeamNG's own)
  if math.abs(base) < 0.5 or math.abs(base) > 50 then logE('drive: gravity: BeamNG\'s gravity reads ' .. tostring(base) .. ': left as it is'); return end
  local g = base * mult
  local ok = grav.set(g)
  if ok then
    grav.was = base
    pcall(writeFile, grav.file, string.format('%.5f', base))
  end
  logI(string.format('drive: gravity %.2f -> %.2f (x%.2f): %s; reads back %s', base, g, mult, ok and 'set' or 'FAILED (core_environment.setGravity)', tostring(grav.get())))
end
function grav.off(why)
  if not grav.was then return end
  local ok = grav.set(grav.was)
  logI(string.format('drive: gravity back to %.2f (%s): %s; reads back %s', grav.was, tostring(why), ok and 'set' or 'FAILED', tostring(grav.get())))
  grav.was = nil
  pcall(writeFile, grav.file, '')
end
-- at load: a gravity left changed by a Lua reload during a drive goes back
function grav.restoreFile()
  local txt
  pcall(function() txt = readFile(grav.file) end)
  local g = tonumber(txt or '')
  if not g or math.abs(g) < 0.5 or math.abs(g) > 50 then return end
  if grav.set(g) then
    logI(string.format('drive: gravity put back to %.2f (it was changed for GTA\'s drive when BeamNG\'s Lua reloaded)', g))
    pcall(writeFile, grav.file, '')
  end
end

local function driveEnd(why, tellGta)
  if not drive then return end
  local d = drive
  pcall(pz.reset)
  pcall(function()
    releaseBinds(d)
    flushVehicle(d, d.veh)
  end)
  pcall(function() d.veh:queueLuaCommand(driveInputCmd(0, 0, 0, 1, 2) .. " input.event('clutch',0,2)") end)
  if tellGta then
    pcall(function() udp:sendto(jsonEncode({ t = 'drive_err', err = why }), d.ip, d.port) end)
  end
  logI('drive: ended (' .. tostring(why) .. ')')
  screen('GTA bridge: drive mode ended (' .. tostring(why) .. ')', 6)
  drive = nil
  driveActive = false
  pcall(grav.off, why)   -- v2.5
  pcall(driveSettings, false)
  pcall(sessionVolume, true, 'drive end')   -- v2.4 session mode: quiet while you walk in GTA
  if worldDriveEnded then pcall(worldDriveEnded, d) end   -- v2.4: the parking pad under the car (T3, below)
  flushLog(true)
end

local function driveUpdate(dtReal, dtSim, dtRaw)
  if not drive then return end
  local d = drive
  local veh = getPlayerVeh()
  local id
  if veh then pcall(function() id = veh:getID() end) end
  if not veh or (d.id ~= nil and id ~= d.id) then
    driveEnd('the BeamNG car changed or is gone', true)
    return
  end
  d.veh = veh
  if now() - d.lastInputT > driveCfg.endS then
    driveEnd('no controls from GTA for ' .. driveCfg.endS .. ' s (GTA closed?)', false)
    return
  end
  local dt = (type(dtRaw) == 'number' and dtRaw > 0 and dtRaw < 1) and dtRaw or (tonumber(dtReal) or 0)
  d.st = d.st + dt
  -- v2.4 (P1, measurement): BeamNG's simulation clock, the time its physics was advanced (0 while paused)
  local dts = (type(dtSim) == 'number' and dtSim >= 0 and dtSim < 1) and dtSim or dt
  d.ss = (d.ss or 0) + dts
  local okA, errA = pcall(driveApply, veh)
  if not okA and not d.applyErr then d.applyErr = true; logE('drive: input failed:', tostring(errA)) end
  pcall(pz.update, d, dtReal, dtSim)   -- v2.43: pause sync
  if not pz.paused then   -- (paused: no poses; GTA's car waits, "pz" tells GTA why)
    local okP, errP = pcall(drivePose, veh, dt)
    if not okP and not d.poseErr then d.poseErr = true; logE('drive: pose failed:', tostring(errP)) end
  end
  local t = now()
  if t - d.vsT >= driveCfg.vsS then
    d.vsT = t
    pcall(function() veh:queueLuaCommand(VEH_STATE) end)
  end
  local okD, errD = pcall(dents.tick, d, veh)   -- v2.5 (D2)
  if not okD and not d.dentErr then d.dentErr = true; logE('dents: ' .. tostring(errD)) end
  if t - d.logT >= 10 then
    local vs = d.vs or {}
    local la = {}
    for a in pairs(d.seenActs) do la[#la + 1] = a end
    table.sort(la)
    d.seenActs = {}
    logI(string.format('drive: %d poses, %d GTA messages, %d actions run (%s), %d direct input updates in %.0f s; from GTA: pad %s (mode %s), keys "%s", fallback thr %.2f brk %.2f steer %+.2f hb %d; ' ..
      'car: throttle in %s out %s, brake in %s, steering %s, clutch %s, parking brake %s, gear %s, %s rpm, wheels %s m/s, ignition %s, engine %s, gearbox %s, lights %s; velocity %s',
      d.poses, d.msgs, d.acts, #la > 0 and table.concat(la, ' '):sub(1, 300) or 'none', d.applied, t - d.logT, d.pad and 'yes' or 'no', tostring(d.padMode),
      tostring(d.keyStr or ''):sub(1, 80), d.inp.thr, d.inp.brk, d.inp.str, d.inp.hb,
      tostring(vs.thrIn), tostring(vs.thr), tostring(vs.brkIn), tostring(vs.strIn), tostring(vs.cluIn), tostring(vs.pbIn), tostring(vs.gear), tostring(vs.rpm),
      tostring(vs.ws), tostring(vs.ign), tostring(vs.eng), tostring(vs.gbm), tostring(vs.li), d.velApi and 'getVelocity' or 'from positions'))
    if (d.dentMsgs or 0) > 0 then
      logI(string.format('dents: %d messages to GTA (%.1f KB) in %.0f s', d.dentMsgs, (d.dentBytes or 0) / 1024, t - d.logT))
      d.dentMsgs, d.dentBytes = 0, 0
    end
    d.poses, d.applied, d.msgs, d.acts, d.logT = 0, 0, 0, 0, t
  end
end

-- fast path for "drive" (up to ~150 a second): no JSON decode
local function driveInput(str)
  if not drive then return end
  local d = drive
  local thr = tonumber(str:match('"thr":(%-?[%d%.]+)'))
  local brk = tonumber(str:match('"brk":(%-?[%d%.]+)'))
  local st = tonumber(str:match('"str":(%-?[%d%.]+)'))
  local hb = tonumber(str:match('"hb":(%d)'))
  local flt = tonumber(str:match('"flt":(%d)'))
  if not (thr and brk and st and hb and flt) then logE('drive: bad controls message'); return end
  local function c01(x) return math.max(0, math.min(1, x)) end
  d.inp = { thr = c01(thr), brk = c01(brk), str = math.max(-1, math.min(1, st)), hb = hb ~= 0 and 1 or 0, flt = math.max(0, math.min(2, flt)) }
  -- v2.01: raw pad ("lx,ly,rx,ry,lt,rt,buttons") and keys ("up lshift x")
  local ps = str:match('"pad":"([^"]*)"')
  if ps and ps ~= '' then
    local v = {}
    for x in (ps .. ','):gmatch('([^,]*),') do v[#v + 1] = tonumber(x) or 0 end
    d.pad = { v[1] or 0, v[2] or 0, v[3] or 0, v[4] or 0, v[5] or 0, v[6] or 0, btn = math.floor(v[7] or 0), pm = math.floor(v[8] or 0) }
    -- buttons seen since the last BeamNG frame (a tap shorter than a frame still counts)
    if d.pad.btn > 0 then
      d.tapBtn = d.tapBtn or {}
      for name, bit in pairs(PAD_BTN) do
        if math.floor(d.pad.btn / bit) % 2 == 1 then d.tapBtn[name] = true end
      end
    end
  else
    d.pad = nil
  end
  local ks = str:match('"keys":"([^"]*)"') or ''
  d.keyStr = ks
  local keys = {}
  d.tapKeys = d.tapKeys or {}
  for k in ks:gmatch('%S+') do keys[k] = true; d.tapKeys[k] = true end
  d.keys = keys
  d.padMode = tonumber(str:match('"m":(%d)')) or 0
  d.paused = str:match('"p":1') ~= nil   -- GTA paused: let go of everything, brake / parking brake
  local out = str:match('"out":1') ~= nil   -- v2.61: you got out of the car in GTA (drive mode stays): parked the same way
  if out ~= (d.out or false) then
    d.out = out
    logI(out and 'drive: you got out in GTA: the car parks (brake, then parking brake) and waits; bullets and explosions still reach it'
      or 'drive: you are back in the car in GTA')
  end
  pz.gtaPaused(d, str:match('"gp":1') ~= nil, 'GTA is paused')   -- v2.43: and BeamNG pauses
  d.msgs = d.msgs + 1
  -- for the "did the throttle arrive" check: the throttle we're asking for, by either path
  local want = d.inp.thr > 0.5
  if not want and d.binds then
    for _, b in ipairs(d.binds.pad) do if (b.action == 'accelerate' or b.action == 'accelerate_brake') and b.last > 0.5 then want = true end end
    for _, b in ipairs(d.binds.key) do if b.action == 'accelerate' and b.last > 0.5 then want = true end end
  end
  if want then d.wantThrT = d.wantThrT or now() else d.wantThrT = nil end
  d.lastInputT = now()
  d.ip, d.port = peerIp, peerPort
  if d.holdLogged then
    d.holdLogged = false
    logI('drive: GTA runs again (' .. tostring(d.holds or 0) .. ' hold messages while it was paused)')
    d.holds = 0
  end
end


-- ---------------------------------------------------------------- build queue state (v1.4)
local queue, qHead, qTail = {}, 1, 0
local pendingBuild = {}    -- id -> newest queued build entry (so a remove can cancel it)

local function queueLen() return qTail - qHead + 1 end

-- ---------------------------------------------------------------- message handling
local handlers = {}

-- v2.41: GTA paused (alt-tab): its plugin's network thread says "hold" twice a second while GTA's script is stopped.
-- The drive stays; the car brakes / parks as for a GTA pause. (v2.4 ended the drive after 5 s without controls.)
handlers.hold = function(m)
  local d = drive
  if not d then return end
  d.lastInputT = now()
  d.paused = true
  d.holds = (d.holds or 0) + 1
  pz.gtaPaused(d, true, 'GTA is alt-tabbed or paused (hold)')   -- v2.43
  if not d.holdLogged then
    d.holdLogged = true
    logI('drive: GTA is paused or alt-tabbed (hold): the drive stays, the car brakes and waits')
  end
end

handlers.cam = function(m) camState.last = m end
handlers.anchor = function(m)
  anchorFrame = { carFrame() }
  send({ t = 'anchor_ok', o = anchorFrame[1], f = anchorFrame[2], r = anchorFrame[3], ground = caps.ground })
end

-- car position + surface height under it + bridge cost since the last state query
handlers.state = function(m)
  local v = getPlayerVeh()
  local pos, surf
  if v then
    local p = v:getPosition()
    pos = { p.x, p.y, p.z }
    surf = groundBelow(p.x, p.y, p.z)
  end
  local n = 0 for _ in pairs(surfaces) do n = n + 1 end
  send({ t = 'state', seq = m.seq, pos = pos, surf = surf, surfaces = n, tris = liveTris(),
         reloadMs = stats.lastReloadMs, maxReloadMs = stats.maxReloadMs,
         avgReloadMs = stats.reloads > 0 and stats.sumReloadMs / stats.reloads or 0,
         frameMs = stats.windowMaxFrameMs, queue = queueLen(), waitMs = stats.lastWaitMs })
  stats.windowMaxFrameMs = 0
end

handlers.resetstats = function(m)
  stats.builds, stats.lastBuildMs, stats.maxBuildMs = 0, 0, 0
  stats.reloads, stats.lastReloadMs, stats.maxReloadMs, stats.sumReloadMs = 0, 0, 0, 0
  stats.lastFrameMs, stats.maxFrameMs, stats.windowMaxFrameMs = 0, 0, 0
  stats.maxQueue, stats.lastWaitMs, stats.maxWaitMs, stats.cancelled = 0, 0, 0, 0
  send({ t = 'resetstats_ok' })
end
handlers.camoff = function(m) M.camOff() end
handlers.greenroom = function(m) M.greenRoom(m.on ~= false); send({ t = 'greenroom_ok', caps = caps }) end

-- D1: export the player car; the reply (export_ok / export_err) comes when the file is written
handlers.export = function(m)
  if exportJob then
    send({ t = 'export_err', err = 'an export is already running' })
    return
  end
  send({ t = 'export_started' })
  M.exportCar(peerIp, peerPort)
end

-- v2.01: GTA's lists (its ini) for which actions are forwarded
local function applyBindOpts(m)
  if type(m.cats) == 'string' and m.cats ~= '' then bindOpts.cats = splitList(m.cats) end
  if type(m.deny) == 'string' then
    bindOpts.deny = {}
    for _, a in ipairs(splitList(m.deny)) do bindOpts.deny[a] = true end
  end
  if type(m.also) == 'string' then
    bindOpts.also = {}
    for _, a in ipairs(splitList(m.also)) do bindOpts.also[a] = true end
  end
  bindOpts.rep = {}
  if type(m.rep) == 'string' then
    for _, a in ipairs(splitList(m.rep)) do bindOpts.rep[a] = true end
  end
  if m.ds ~= nil then bindOpts.ds = truthy(m.ds) end
  if type(m.pb) == 'string' and (m.pb == 'auto' or m.pb == 'xinput' or m.pb == 'playstation') then bindOpts.padList = m.pb end
end

-- v2.0 drive mode (v2.01: with your BeamNG bindings)
handlers.drive_start = function(m)
  local veh = getPlayerVeh()
  if not veh then send({ t = 'drive_err', err = 'no player car in BeamNG' }); return end
  local id, vehicle
  pcall(function() id = veh:getID() end)
  pcall(function() vehicle = veh:getJBeamFilename() end)
  if drive and drive.id == id and drive.ip == peerIp and drive.port == peerPort then
    -- GTA asked again (it didn't see our answer yet): same session
    send(drive.okMsg)
    return
  end
  if drive then driveEnd('a new drive request', false) end
  applyBindOpts(m)
  local okW, wl, keys = pcall(driveWheels, veh)
  if not okW then logE('drive: wheels:', tostring(wl)); wl, keys = {}, nil end
  local names, spinKeys, spinN = {}, {}, 0
  for _, w in ipairs(wl) do
    names[#names + 1] = w.name
    if w.s then spinN = spinN + 1; spinKeys[w.sKey] = true end
  end
  local how = {}
  for k in pairs(spinKeys) do how[#how + 1] = k end
  local spinHow = spinN == #wl and #wl > 0 and ('wheel nodes (' .. table.concat(how, ',') .. ')') or 'the speed'
  -- v2.01: your bindings
  local B
  local okB, resB = pcall(buildBinds, veh)
  if okB then B = resB; writeBindsReport(B) else logE('drive: reading your BeamNG controls failed:', tostring(resB)) end
  if m['in'] == 0 then B = nil end   -- GTA's ini says DriveInput=gta: GTA's own controls, the v2.0 way
  local fallback
  if not B or (#B.pad + #B.key) == 0 then
    B, fallback = nil, { thr = true, brk = true, str = true, hb = true }
  else
    local need = {}
    if not (B.has.pad.thr or B.has.key.thr) then need.thr = true end
    if not (B.has.pad.brk or B.has.key.brk) then need.brk = true end
    if not (B.has.pad.str or B.has.key.str) then need.str = true end
    -- (no parking brake binding: none, like in BeamNG; GTA's handbrake button may be bound to something else)
    if next(need) then fallback = need end
  end
  local inpHow = B and (fallback and 'binds+fallback' or 'binds') or 'fallback'
  local fbList = {}
  for k in pairs(fallback or {}) do fbList[#fbList + 1] = k end
  table.sort(fbList)
  drive = { ip = peerIp, port = peerPort, veh = veh, id = id, wheels = wl, st = 0, seq = 0,
            inp = { thr = 0, brk = 0, str = 0, hb = 0, flt = 2 }, lastInputT = now(), lastKey = nil, lastApplyT = 0,
            poses = 0, applied = 0, msgs = 0, acts = 0, logT = now(), vsT = 0, binds = B, fallback = fallback, inpHow = inpHow,
            vq = {}, recent = {}, seenActs = {}, geErr = {}, pauseSync = m.ps ~= 0 and m.ps ~= false and m.ps ~= nil }
  pcall(pz.reset)
  local padc = B and table.concat(B.padList, ',') or ''
  local keyc = B and table.concat(B.keyList, ',') or ''
  if #keyc > 3000 then keyc = keyc:sub(1, 3000):gsub(',[^,]*$', '') end
  drive.okMsg = { t = 'drive_ok', v = M.version, veh = vehicle, wheels = names, spin = spinHow, inp = inpHow, fb = table.concat(fbList, ','), padc = padc, keyc = keyc,
                  nb = B and (#B.pad + #B.key) or 0, bhow = B and B.how or 'none',
                  padm = B and table.concat(B.padComboList or {}, ',') or '', pmod = B and table.concat(B.padModList or {}, ',') or '',
                  padmx = B and B.padComboSetsStr or '',
                  plist = B and tostring(B.padDev or '') or '' }
  do  -- v2.71: the driver's seat for GTA's player
    local okE, eye, eyeHow = pcall(driverEye, veh)
    if okE and eye then
      drive.okMsg.eye = { tonumber(string.format('%.3f', eye[1])), tonumber(string.format('%.3f', eye[2])), tonumber(string.format('%.3f', eye[3])) }
      logI(string.format('drive: driver\'s eye %.2f %.2f %.2f m (right, forward, up of the car): %s', eye[1], eye[2], eye[3], tostring(eyeHow)))
    else
      -- v2.72: the game engine's vehicle data had no cameras in your v2.71 log: read them in the car's own Lua (v.data), answered
      -- a frame later ("eye" to GTA, which seats you again)
      logI('drive: no driver camera in the game engine\'s data (' .. tostring(okE and eyeHow or eye) .. '): asking the car\'s own Lua')
      pcall(function() veh:queueLuaCommand(dents.EYE) end)
    end
  end
  -- the car starts from rest: inputs to 0, parking brake off (v2.0 left it on at the end), report helper in
  pcall(function() veh:queueLuaCommand(VEH_HELPER .. "\ninput.event('throttle',0,2) input.event('brake',0,2) input.event('parkingbrake',0,2) input.event('clutch',0,2)") end)
  native.userOff = false
  setNativeBlocked(true, 'GTA drives')
  -- v2.2: sound (snd) and the background fps limit (bgf); the current values are always logged
  -- v2.4: snd 2 = sound session mode (sq: BeamNG's volume while GTA isn't driving, 0..1)
  local sndMode = (m.snd == false or m.snd == 0) and 0 or (tonumber(m.snd) == 2 and 2 or 1)
  local okS, woke = pcall(driveSettings, true, sndMode, m.bgf, m.sq)
  if not okS then logE('settings: ' .. tostring(woke)) elseif woke then drive.okMsg.sw = 1 end
  pcall(grav.on, m.grav)   -- v2.5
  pcall(dents.start, drive, m.dz)   -- v2.5 (D2)
  do local okW, errW = pcall(wpn.start, drive, m) if not okW then logE('weapons: ' .. tostring(errW)) end end   -- v2.6
  driveActive = true
  send(drive.okMsg)
  logI(string.format('drive: GTA drives %s now: %d wheels (%s), wheel turn from %s; wheel data keys: %s', tostring(vehicle), #wl,
    table.concat(names, ','), spinHow, tostring(keys)))
  if B then
    logI(string.format('drive: your controls: %s; %s', inpHow, B.summary or ''))
    for i = 1, #B.lines do
      local l = B.lines[i]
      if l:find('^device ') or l:find('^bindings from') or l:find('^  pad modifiers') or l:find('^pad combinations not usable') or l:find('^pad controls not understood') then logI('drive:   ' .. l) end
    end
  else
    logI('drive: your BeamNG bindings could not be used: GTA\'s throttle/brake/steering/parking brake go in directly (v2.0 way); see gtabridge/bindings.txt')
  end
  screen('GTA bridge: GTA is driving ' .. tostring(vehicle) .. (B and ' with your BeamNG controls' or ''), 6)
end

-- v2.3: one of your BeamNG actions by name, pressed and let go (GTA's PadGtaActions, e.g. D-pad right = toggle_headlights)
handlers.act = function(m)
  local d = drive
  if not d then return end
  local name = tostring(m.a or '')
  local acts = d.binds and d.binds.acts
  if not acts then acts = readActions((function() local v; pcall(function() v = d.veh:getJBeamFilename() end); return v end)()) end
  local def = acts and acts[name]
  local ok, why = actionAllowed(name, def)
  if not ok then
    if not (d.actErr and d.actErr[name]) then
      d.actErr = d.actErr or {}
      d.actErr[name] = true
      logE('drive: GTA asked for action ' .. name .. ': not run (' .. tostring(why) .. ')')
    end
    return
  end
  local b = { def = def, action = name, flt = 0, angle = 0, lock = 0, last = 0, down = false }
  fireBind(d, b, 1)
  fireBind(d, b, 0)
  logI('drive: GTA asked for action ' .. name .. ': run')
end

handlers.drive_stop = function(m)
  if drive then driveEnd('GTA stopped drive mode', false) end
  if m.park == 1 then park.t, park.block = now(), true end   -- parked in GTA now: no gap before GTA's next "park"
end

-- v2.01: GTA is focused with the car parked (not driving): keep BeamNG's own controls off the car
handlers.park = function(m)
  park.t = now()
  park.block = m.blk ~= 0 and m.blk ~= false
end

handlers.ping = function(m)
  stats.pings = stats.pings + 1
  send({ t = 'pong', seq = m.seq, ts = m.ts })
end

handlers.diag = function(m)
  send({ t = 'diag', version = M.version, caps = caps, stats = stats, surfaces = (function() local n=0 for _ in pairs(surfaces) do n=n+1 end return n end)() })
end

-- v1.4 build queue. Surface messages are queued (still as raw JSON text) and decoded+built in
-- onUpdate within cfg.buildBudgetMs per frame, in arrival order. Removes keep their place in line.
local function enqueue(e)
  qTail = qTail + 1
  queue[qTail] = e
  local n = queueLen()
  if n > stats.maxQueue then stats.maxQueue = n end
end

local function runBuild(e)
  local okD, m = pcall(jsonDecode, e.raw)
  if not okD or type(m) ~= 'table' then logE('bad surf json'); return end
  local wait = (now() - e.t) * 1000
  stats.lastWaitMs = wait
  if wait > stats.maxWaitMs then stats.maxWaitMs = wait end
  if m.t == 'hf' then
    -- v2.4: a GTA ground tile (T3)
    local okT, ok2, res2 = pcall(buildTileMsg, m)
    if okT and ok2 then
      send({ t = 'hf_ok', id = m.id, ms = res2 })
    else
      local err = okT and res2 or ok2
      logE('tile ' .. tostring(m.id) .. ' failed: ' .. tostring(err))
      send({ t = 'hf_err', id = m.id, err = tostring(err) })
    end
    return
  end
  local ok, res = buildSurface(m.id or 'default', toWorld(m.verts or {}, m.rel), m.tris or {}, m.material)
  if ok then
    send({ t = 'surf_ok', id = m.id, ms = res, waitMs = wait, seq = m.seq })
  else
    send({ t = 'surf_err', id = m.id, err = res, seq = m.seq, caps = caps })
  end
end

local function processQueue()
  local t0 = now()
  local budget = cfg.buildBudgetMs / 1000
  local builtAny = false
  while qHead <= qTail do
    local e = queue[qHead]
    -- stop if the next build would push us past the budget (always do at least one per frame)
    if e.kind == 'build' and not e.cancelled and builtAny
       and (now() - t0) + stats.avgBuildMs / 1000 > budget then break end
    queue[qHead] = nil
    qHead = qHead + 1
    if e.kind == 'build' then
      if pendingBuild[e.id] == e then pendingBuild[e.id] = nil end
      if e.cancelled then
        stats.cancelled = stats.cancelled + 1
      else
        local tb = now()
        runBuild(e)
        stats.avgBuildMs = stats.avgBuildMs * 0.9 + (now() - tb) * 1000 * 0.1
        builtAny = true
      end
    else
      removeSurface(e.id)
      if not e.quiet then send({ t = 'remove_ok', id = e.id }) end   -- v2.4: tile removals ("rm") are not answered
    end
  end
  if qHead > qTail then queue, qHead, qTail = {}, 1, 0 end
end

local function clearQueue()
  queue, qHead, qTail = {}, 1, 0
  pendingBuild = {}
end

-- ---------------------------------------------------------------- v2.4 T3: GTA's ground (height tiles)
-- The GTA plugin measures GTA's ground with rays (a height and GTA's ground material per point) and sends 8 m tiles
-- ("hf"). The mapping is fixed: BeamNG = GTA - origin (ox, oy), and zl (200 m) higher, so BeamNG's own map stays far
-- below. A tile is one ProceduralMesh with one sub-mesh per surface; each surface's material has groundType = the
-- BeamNG ground model GTA's material maps to (ASPHALT, DIRT, GRASS...; GTA's GtaBeamD1_surfaces.json), so the tyres
-- feel GTA's ground. A BeamNG developer said a procedural mesh's material ground model can be changed; UNVERIFIED in
-- 0.39: the ground ids under the wheels are logged while driving, and gtaBridge.surfaceTest() builds strips to feel.
local world, worldUpdate, reloadDue   -- (the rest of this section is local to the block: Lua's 200-locals limit)
do
world = {
  ox = nil, oy = nil, zl = 200, tileM = 8, epoch = 0, fresh = false,
  groundModels = nil, gmList = '', resolved = {}, mats = {},
  mode = 'submesh', checked = false, checkNote = 'not checked yet',
  tp = nil, conv = nil, api = nil, padOn = false, carP = nil, urgent = false,
  wsT = 0, built = 0, buildMsSum = 0, buildMsMax = 0, frameCosts = {}, logT = 0, sumBuilt = 0,
}
local SURF_COLOR = {
  ASPHALT = '0.16 0.16 0.17 1', ASPHALT_WET = '0.1 0.1 0.13 1', ASPHALT_OLD = '0.26 0.26 0.26 1', ASPHALT_PREPPED = '0.13 0.13 0.13 1',
  CONCRETE = '0.55 0.55 0.52 1', DIRT = '0.45 0.32 0.2 1', DIRT_DUSTY = '0.62 0.48 0.3 1', DIRT_DUSTY_LOOSE = '0.64 0.5 0.33 1',
  GRAVEL = '0.52 0.5 0.46 1', GRAVEL_WET = '0.4 0.38 0.35 1', GRASS = '0.25 0.5 0.18 1', SAND = '0.8 0.7 0.45 1', MUD = '0.3 0.22 0.14 1',
  ROCK = '0.42 0.42 0.44 1', ICE = '0.8 0.9 1 1', SNOW = '0.95 0.95 0.97 1', METAL = '0.5 0.56 0.62 1', METAL_TREAD = '0.45 0.5 0.55 1',
  WOOD = '0.55 0.36 0.2 1', RUMBLE_STRIP = '0.7 0.2 0.2 1', COBBLESTONE = '0.46 0.41 0.36 1', SLIPPERY = '0.1 0.1 0.3 1', PLASTIC = '0.7 0.7 0.75 1',
  WALL = '0.62 0.6 0.56 1', PROP = '0.85 0.55 0.15 1',   -- v2.5: GTA's walls and barriers, solid props
}

-- v2.41: GTA's concrete (sidewalks, car parks) had no BeamNG ground model: it drove and looked like asphalt. BeamNG
-- loads ground models when a map starts, from art/groundmodels.json and levels/<map>/groundModels/*.json (BeamNG's
-- documentation). So the mod writes levels/<map>/groundModels/gtabridge.json into your BeamNG user folder once, with
-- CONCRETE made from YOUR install's ASPHALT (grip x concreteGrip, v2.42: 0.85 by default, in gtabridge/config.json); BeamNG uses
-- it after the map is loaded again (once). Until then concrete is drawn grey but drives as asphalt.
local function currentLevel()
  local lv
  pcall(function() lv = tostring(getMissionFilename() or ''):match('levels/([^/]+)/') end)
  return lv
end
-- returns true when CONCRETE is in BeamNG now (from this map load)
local function ensureConcrete(set, asphalt)
  world.concreteNote = 'not needed'
  local live = set.CONCRETE and true or false
  local lv = currentLevel()
  if not lv then world.concreteNote = live and 'BeamNG has it' or 'no map loaded'; return live end
  local path = '/levels/' .. lv .. '/groundModels/gtabridge.json'
  local exists = false
  pcall(function() exists = FS:fileExists(path) end)
  if live and not exists then world.concreteNote = 'BeamNG has it (its own)'; return true end
  if type(asphalt) ~= 'table' then
    world.concreteNote = live and 'BeamNG has it' or (exists and ('written to ' .. path .. ': load the map again once') or 'ASPHALT not readable: not made')
    return live
  end
  local grip = tonumber(modConfig and modConfig.concreteGrip) or 0.85
  local c = {}
  for k, v in pairs(asphalt) do c[k] = v end
  c.aliases = nil   -- (ASPHALT's other names stay ASPHALT's)
  for _, k in ipairs({ 'staticFrictionCoefficient', 'slidingFrictionCoefficient' }) do
    if tonumber(c[k]) then c[k] = tonumber(c[k]) * grip end
  end
  if exists then
    -- written before: the same grip = in use (or waiting for the map to be loaded again); another grip (v2.42: 0.85) = written again
    local old
    pcall(function() old = jsonReadFile(path) end)
    local oc = type(old) == 'table' and (old.CONCRETE or old.concrete) or nil
    if type(oc) == 'table' and tonumber(oc.staticFrictionCoefficient) and tonumber(c.staticFrictionCoefficient)
      and math.abs(tonumber(oc.staticFrictionCoefficient) - tonumber(c.staticFrictionCoefficient)) < 1e-4 then
      world.concreteNote = live and ('BeamNG has it (grip x ' .. grip .. ')') or ('written to ' .. path .. ': load the map again once')
      return live
    end
  end
  local ok = pcall(writeFile, path, jsonEncode({ CONCRETE = c }))
  pcall(function() ok = ok and FS:fileExists(path) end)
  world.concreteNote = ok and ('made from your ASPHALT (grip x ' .. grip .. ') in ' .. path .. ': load the map again once' .. (live and ' (the old one is used until then)' or ''))
    or ('writing ' .. path .. ' FAILED')
  logI('ground: CONCRETE (GTA sidewalks, car parks) ' .. world.concreteNote)
  if ok then screen('GTA bridge: concrete ground ' .. (exists and 'changed' or 'added') .. ' for GTA\'s sidewalks (grip x ' .. grip .. '). Load this map again once (Escape > Freeroam > the same map) so BeamNG uses it.', 15) end
  return live
end
M.concreteNote = function() return world.concreteNote end

-- BeamNG's ground models (art/groundmodels.json in your install, v2.41: and the map's own), so a name GTA's table asks
-- for can be checked
local function loadGroundModels()
  if world.groundModels ~= nil then return end
  local set, names = {}, {}
  local asphalt
  local function add(t)
    if type(t) ~= 'table' then return end
    for k, v in pairs(t) do
      if type(k) == 'string' and type(v) == 'table' and not set[k:upper()] then set[k:upper()] = true; names[#names + 1] = k:upper() end
      if type(k) == 'string' and k:upper() == 'ASPHALT' and type(v) == 'table' and not asphalt then asphalt = v end
    end
  end
  for _, p in ipairs({ 'art/groundmodels.json', '/art/groundmodels.json' }) do pcall(function() add(jsonReadFile(p)) end) end
  pcall(function()
    local files = FS:findFiles('/art/groundmodels/', '*.json', -1, false, false)
    for _, f in ipairs(files or {}) do add(jsonReadFile(f)) end
  end)
  -- the map's own (loaded with the map; a file written after that waits for the next map load)
  local lv = currentLevel()
  if lv and not world.gmWrittenNow then
    pcall(function()
      local files = FS:findFiles('/levels/' .. lv .. '/groundModels/', '*.json', -1, false, false)
      for _, f in ipairs(files or {}) do add(jsonReadFile(f)) end
    end)
  end
  local okC, live = pcall(ensureConcrete, set, asphalt)
  if world.concreteNote and world.concreteNote:find('load the map again') then world.gmWrittenNow = true end
  if okC and not live then set.CONCRETE = nil end
  table.sort(names)
  if #names >= 3 then
    world.groundModels = set
    world.gmList = table.concat(names, ' ')
    logI('ground: BeamNG has ' .. #names .. ' ground models: ' .. world.gmList)
  else
    world.groundModels = false   -- not readable: names are used as GTA's table gives them
    world.gmList = '?'
    logE('ground: BeamNG\'s ground model list (art/groundmodels.json) not readable: the names from GTA\'s table are used as they are')
  end
end

-- "CONCRETE|ASPHALT" -> the first name BeamNG has
local function resolveGround(spec)
  spec = tostring(spec or 'ASPHALT')
  local hit = world.resolved[spec]
  if hit then return hit end
  local first
  for raw in spec:gmatch('[^|,%s]+') do
    local name = raw:upper()
    first = first or name
    if not world.groundModels or world.groundModels[name] then world.resolved[spec] = name; return name end
  end
  local fb = (world.groundModels and world.groundModels.ASPHALT) and 'ASPHALT' or first or 'ASPHALT'
  logE('ground: none of "' .. spec .. '" is one of BeamNG\'s ground models: ' .. fb .. ' instead (edit GtaBeamD1_surfaces.json)')
  world.resolved[spec] = fb
  return fb
end

-- v2.41: what a surface looks like = the first name GTA's table asks for ("CONCRETE|ASPHALT" -> grey even while
-- BeamNG has no CONCRETE yet and it drives as ASPHALT)
local function lookOf(spec)
  return (tostring(spec or 'ASPHALT'):match('[^|,%s]+') or 'ASPHALT'):upper()
end

-- one material per ground model (and look), created at run time: its groundType is what BeamNG's tyres feel
local function surfMaterial(gm, look)
  look = SURF_COLOR[look or ''] and look or gm
  local key = look == gm and gm or (look .. '>' .. gm)
  local m = world.mats[key]
  if m then return m.name end
  local name = 'gtab_gm_' .. tostring(key):gsub('[^%w_]', '_')
  local ok, err = pcall(function()
    if scenetree.findObject(name) then return end
    local mat = createObject('Material')
    mat:setField('diffuseColor', 0, SURF_COLOR[look] or '0.6 0.3 0.6 1')
    mat:setField('groundType', 0, gm)
    mat:registerObject(name)
  end)
  local back = '?'
  pcall(function() back = tostring(scenetree.findObject(name):getField('groundType', 0)) end)
  world.mats[key] = { name = name, ok = ok, back = back }
  if ok then
    logI(string.format('ground: material %s made with groundType %s (reads back "%s")', name, gm, back))
  else
    logE('ground: material for ' .. tostring(gm) .. ' failed: ' .. tostring(err) .. ' (tiles use ' .. cfg.material .. ')')
  end
  return ok and name or cfg.material
end

-- a ProceduralMesh from ready mesh tables (one per sub-mesh), vertices relative to origin
local function makeProcMesh(name, origin, meshList)
  local obj = createObject('ProceduralMesh')
  if not obj then error('createObject("ProceduralMesh") returned nil') end
  obj:setPosition(vec3(origin[1], origin[2], origin[3]))
  obj.canSave = false
  obj:registerObject(name)
  local added = pcall(function() scenetree.MissionGroup:addObject(obj) end)
  if not added then scenetree.MissionGroup:addObject(obj.obj) end
  obj:createMesh({ meshList })
  return obj
end

-- v2.5: boxes (GTA's walls, barriers, pillars; solid props) as one sub-mesh: each box = centre x y, bottom, top (relative
-- to the mesh's origin, at the centre), along x y (unit), length, thickness, rise per metre along it (the ground's slope:
-- bottom and top follow it); faces outward, counter-clockwise from outside, no bottom.
-- v2.52: corners cut at 45 degrees (BeamNG's developers: cars catch on walls' 90 degree edges), and joined (overlap = true:
-- walls, whose pieces meet end to end) each piece goes on past its ends by its cut, so the long faces run on without a gap
world.boxMesh = function(list, material, overlap)   -- (a field of world: Lua's 200-locals limit)
  local verts, uvs, normals, faces = {}, {}, {}, {}
  local function addFace(nrm, pts, w, h)
    local v0 = #verts
    local ni = #normals
    normals[ni + 1] = nrm
    for c = 1, #pts do verts[#verts + 1] = pts[c] end
    if #pts == 4 then
      uvs[v0 + 1] = { u = 0, v = 0 }; uvs[v0 + 2] = { u = w, v = 0 }; uvs[v0 + 3] = { u = w, v = h }; uvs[v0 + 4] = { u = 0, v = h }
    else
      for c = 1, #pts do uvs[v0 + c] = { u = pts[c].x * 0.5, v = pts[c].y * 0.5 } end
    end
    for t = 2, #pts - 1 do
      local a, b, c = v0, v0 + t - 1, v0 + t
      if cfg.flipWinding then b, c = c, b end
      faces[#faces + 1] = { v = a, n = ni, u = a }
      faces[#faces + 1] = { v = b, n = ni, u = b }
      faces[#faces + 1] = { v = c, n = ni, u = c }
    end
  end
  for q = 1, #list - 8, 9 do
    local cx, cy, z0, z1 = tonumber(list[q]), tonumber(list[q + 1]), tonumber(list[q + 2]), tonumber(list[q + 3])
    local dx, dy, len, th = tonumber(list[q + 4]), tonumber(list[q + 5]), tonumber(list[q + 6]), tonumber(list[q + 7])
    local gs = math.max(-2, math.min(2, tonumber(list[q + 8]) or 0))
    if cx and cy and z0 and z1 and dx and dy and len and th and z1 > z0 and len > 0 and th > 0 then
      local nx, ny = -dy, dx
      local cut = math.min(0.15, th * 0.45, len * 0.2)
      local hl, ht = len * 0.5 + (overlap and cut or 0), th * 0.5
      -- the outline, counter-clockwise from above: (along, across)
      local o = { { hl, -ht + cut }, { hl, ht - cut }, { hl - cut, ht }, { -hl + cut, ht }, { -hl, ht - cut }, { -hl, -ht + cut }, { -hl + cut, -ht }, { hl - cut, -ht } }
      local function P(sa, sb, z) return { x = cx + dx * sa + nx * sb, y = cy + dy * sa + ny * sb, z = z + gs * sa } end
      for k = 1, 8 do
        local A, B = o[k], o[k % 8 + 1]
        local ea, eb = B[1] - A[1], B[2] - A[2]
        local el = math.sqrt(ea * ea + eb * eb)
        if el > 1e-4 then
          -- outward normal of the edge (the outline goes counter-clockwise): (eb, -ea) in (along, across)
          local na, nb = eb / el, -ea / el
          addFace({ x = dx * na + nx * nb, y = dy * na + ny * nb, z = 0 }, { P(A[1], A[2], z0), P(B[1], B[2], z0), P(B[1], B[2], z1), P(A[1], A[2], z1) }, el * 0.5, (z1 - z0) * 0.5)
        end
      end
      local top = {}
      for k = 1, 8 do top[k] = P(o[k][1], o[k][2], z1) end
      addFace({ x = 0, y = 0, z = 1 }, top)
    end
  end
  if #faces == 0 then return nil, 0 end
  return { verts = verts, uvs = uvs, normals = normals, faces = faces, material = material }, #faces / 3
end

-- a tile message -> mesh: z = heights above z0 in units of zs m (v2.52: mm; cm before) (n x n points, step st, row by row along +y), s = surfaces
-- (palette), m = one base-36 character per cell (palette index), cells row by row; v2.5: w = its walls (boxes)
buildTileMsg = function(m)
  local t0 = now()
  local id = tostring(m.id or 'tile')
  local n = math.floor(tonumber(m.n) or 0)
  local st = tonumber(m.st) or 1
  local x0, y0, z0 = tonumber(m.x0), tonumber(m.y0), tonumber(m.z0)
  local z = m.z
  local zs = tonumber(m.zs) or 0.01   -- v2.52: GTA 2.52 sends mm (2.51: cm)
  if n < 2 or n > 129 or not (x0 and y0 and z0) or type(z) ~= 'table' or #z < n * n then return false, 'bad tile (n ' .. tostring(n) .. ', ' .. tostring(type(z) == 'table' and #z or 'no') .. ' heights)' end
  local pal, palLook = {}, {}
  for i, s in ipairs(type(m.s) == 'table' and m.s or { 'ASPHALT' }) do
    pal[i] = resolveGround(s)
    palLook[i] = lookOf(s)
    if palLook[i] ~= pal[i] then pal[i] = palLook[i] .. '>' .. pal[i] end   -- shown as one, drives as the other (v2.41)
  end
  if #pal == 0 then pal[1] = resolveGround('ASPHALT') end
  local cells = tostring(m.m or '')
  local verts, uvs = {}, {}
  local zmax = 0
  local missing = {}   -- v2.4: points GTA has no ground for (-999999): the cells around them are left out
  for j = 0, n - 1 do
    for i = 0, n - 1 do
      local k = j * n + i
      local zc = tonumber(z[k + 1]) or 0
      if zc <= -999000 then missing[k] = true; zc = 0 end
      local h = zc * zs
      if h > zmax then zmax = h end
      verts[k + 1] = { x = i * st, y = j * st, z = h }
      uvs[k + 1] = { u = (x0 + i * st) * 0.25, v = (y0 + j * st) * 0.25 }
    end
  end
  local subs, order = {}, {}
  -- v2.42: "lf" = per cell the size of the flat cell it is part of (GTA found every point in it within a few cm of the
  -- same 2 triangles): one cell of that size is drawn as 2 triangles instead of 2 per 0.5 m cell
  local lf = type(m.lf) == 'string' and #m.lf == (n - 1) * (n - 1) and m.lf or nil
  -- v2.51: a flat cell drawn as one whose edge points (measured for a finer neighbour, or by the next tile) aren't on its
  -- straight edges leaves a step against that neighbour (up to WorldRefineCm, also between tiles): drawn finely instead
  local split
  if lf then
    local function zz(k) if missing[k] then return nil end return verts[k + 1].z end
    for j = 0, n - 2 do
      for i = 0, n - 2 do
        local c = j * (n - 1) + i
        local sz = tonumber(lf:sub(c + 1, c + 1)) or 1
        if sz > 1 and i % sz == 0 and j % sz == 0 and i + sz <= n - 1 and j + sz <= n - 1 then
          local a = j * n + i
          local A, B, C, D = zz(a), zz(a + sz), zz(a + sz * n + sz), zz(a + sz * n)
          local bad = false
          if A and B and C and D then
            for t = 1, sz - 1 do
              local f = t / sz
              for _, q in ipairs({ { a + t, A + (B - A) * f }, { a + sz * n + t, D + (C - D) * f }, { a + t * n, A + (D - A) * f }, { a + t * n + sz, B + (C - B) * f } }) do
                local z = zz(q[1])
                if z and math.abs(z - q[2]) > 0.004 then bad = true end   -- v2.52: 4 mm (2.51: 1 cm)
              end
            end
          end
          if bad then
            split = split or {}
            for jj = j, j + sz - 1 do for ii = i, i + sz - 1 do split[jj * (n - 1) + ii] = true end end
            world.splitCells = (world.splitCells or 0) + 1
          end
        end
      end
    end
  end
  for j = 0, n - 2 do
    for i = 0, n - 2 do
      local c = j * (n - 1) + i
      local sz = lf and (tonumber(lf:sub(c + 1, c + 1)) or 1) or 1
      if split and split[c] then sz = 1 end
      if sz > 1 and (i % sz ~= 0 or j % sz ~= 0 or i + sz > n - 1 or j + sz > n - 1) then
        if i % sz ~= 0 or j % sz ~= 0 then goto nextCell end
        sz = 1
      end
      local a0 = j * n + i
      if missing[a0] or missing[a0 + sz] or missing[a0 + sz * n] or missing[a0 + sz * n + sz] then goto nextCell end
      local si = (tonumber(cells:sub(c + 1, c + 1), 36) or 0) + 1
      if not pal[si] then si = 1 end
      local gmName = pal[si]
      local sub = subs[gmName]
      if not sub then sub = { normals = {}, faces = {} }; subs[gmName] = sub; order[#order + 1] = gmName end
      local a = j * n + i
      for _, t in ipairs({ { a, a + sz, a + sz * n + sz }, { a, a + sz * n + sz, a + sz * n } }) do
        local A, B, C = verts[t[1] + 1], verts[t[2] + 1], verts[t[3] + 1]
        local ux, uy, uz = B.x - A.x, B.y - A.y, B.z - A.z
        local vx, vy, vz = C.x - A.x, C.y - A.y, C.z - A.z
        local nx, ny, nz = uy * vz - uz * vy, uz * vx - ux * vz, ux * vy - uy * vx
        local l = math.sqrt(nx * nx + ny * ny + nz * nz)
        if l < 1e-9 then nx, ny, nz, l = 0, 0, 1, 1 end
        local ni = #sub.normals
        sub.normals[ni + 1] = { x = nx / l, y = ny / l, z = nz / l }
        local p, q, r = t[1], t[2], t[3]
        if cfg.flipWinding then q, r = r, q end
        local f = sub.faces
        f[#f + 1] = { v = p, n = ni, u = p }
        f[#f + 1] = { v = q, n = ni, u = q }
        f[#f + 1] = { v = r, n = ni, u = r }
      end
      ::nextCell::
    end
  end
  if #order == 0 then return false, 'no ground in any cell' end
  table.sort(order)
  local list, gms, tris, gmt = {}, {}, 0, {}
  for _, gmName in ipairs(order) do
    local sub = subs[gmName]
    local look, phys = gmName:match('^([^>]+)>(.+)$')
    list[#list + 1] = { verts = verts, uvs = uvs, normals = sub.normals, faces = sub.faces, material = surfMaterial(phys or gmName, look) }
    gms[#gms + 1] = gmName
    tris = tris + #sub.faces / 3
    gmt[gmName] = #sub.faces / 3
  end
  -- v2.5: GTA's walls and barriers on this tile: solid boxes
  local nWalls = 0
  if type(m.w) == 'table' and #m.w >= 9 then
    local gm = resolveGround('CONCRETE|ASPHALT')
    local wm, wt = world.boxMesh(m.w, surfMaterial(gm, 'WALL'), true)
    if wm then
      list[#list + 1] = wm
      gms[#gms + 1] = 'WALL>' .. gm
      tris = tris + wt
      gmt['WALL'] = wt
      nWalls = math.floor(#m.w / 9)
      world.wallsBuilt = (world.wallsBuilt or 0) + nWalls
    end
  end
  removeSurface(id)
  local ok, err = pcall(function()
    if world.mode == 'split' and #list > 1 then
      local objs = {}
      for k, mesh in ipairs(list) do objs[#objs + 1] = makeProcMesh('gtab_' .. id .. '_' .. k, { x0, y0, z0 }, { mesh }) end
      surfaces[id] = { objs = objs }
    else
      surfaces[id] = makeProcMesh('gtab_' .. id, { x0, y0, z0 }, list)
    end
  end)
  if not ok then
    caps.mesh = 'FAILED: ' .. tostring(err)
    return false, tostring(err)
  end
  caps.mesh = 'ProceduralMesh:createMesh OK'
  surfaceTris[id] = tris
  local span = (n - 1) * st
  worldTiles[id] = { epoch = world.epoch, x0 = x0, y0 = y0, z0 = z0, x1 = x0 + span, y1 = y0 + span, zmax = z0 + zmax, gms = table.concat(gms, '+'), gmt = gmt, t = now() }
  collisionDirty = true
  if world.carP then
    local cx, cy = x0 + span * 0.5, y0 + span * 0.5
    if (cx - world.carP[1]) ^ 2 + (cy - world.carP[2]) ^ 2 < 12 * 12 then world.urgent = true end   -- near the car: collision now (v2.41: 12 m, was 24)
  end
  local ms = (now() - t0) * 1000
  world.built = world.built + 1
  world.sumBuilt = world.sumBuilt + 1
  world.buildMsSum = world.buildMsSum + ms
  world.buildMsMax = math.max(world.buildMsMax, ms)
  stats.builds = stats.builds + 1
  stats.lastBuildMs = ms
  if ms > stats.maxBuildMs then stats.maxBuildMs = ms end
  return true, ms
end

-- v2.5: GTA's props (the GTA plugin decides): "bump" = BeamNG's car hit a soft one (a lamp post, a sign, a bin: GTA knocks it
-- over): the car loses dv m/s along its way (applyClusterVelocityScaleAdd on its reference node, the way BeamMP sets
-- velocities: no damage); "pr" = solid props as boxes (id, x, y, bottom, top, along x y, length, thickness, slope; BeamNG
-- world), "prm" = taken out
world.props = { boxes = {}, bumps = 0, dvSum = 0, sent = 0, err = nil, logT = 0 }   -- (a field of world: Lua's 200-locals limit)
handlers.bump = function(m)
  local d = drive
  if not d then return end
  local veh = getPlayerVeh()
  if not veh then return end
  local dv = math.max(0, math.min(5, tonumber(m.dv) or 0))
  local ok, err = pcall(function()
    local v = veh:getVelocity()
    local sp = math.sqrt(v.x * v.x + v.y * v.y + v.z * v.z)
    if sp < 0.5 then dv = 0; return end
    dv = math.min(dv, sp * 0.5)
    local k = -dv / sp
    veh:applyClusterVelocityScaleAdd(veh:getRefNodeId(), 1, v.x * k, v.y * k, v.z * k)
  end)
  if not ok then
    if not world.props.err then logE('props: a bump could not slow the car (applyClusterVelocityScaleAdd): ' .. tostring(err)) end
    world.props.err = tostring(err)
    return
  end
  world.props.bumps = world.props.bumps + 1
  world.props.dvSum = world.props.dvSum + dv
  if world.props.bumps <= 20 then logI(string.format('props: GTA says the car hit a soft prop (model %s): %.1f km/h lost', tostring(m.m), dv * 3.6)) end
end
-- v2.51: props as boxes BeamNG's car really hits ("pb"; the GTA plugin decides which): per prop its boxes (its trunk at bumper
-- height: GTA measures it, no arms over the road) and "break" (m/s): a breakaway one (lamp post, sign, traffic light, fence)
-- goes when BeamNG's car has lost that much speed against it since it touched it (the car is dented and slowed and drives on),
-- and GTA knocks the prop over ("pbk"); 0 = a solid one (it stays). A box that would appear inside the car waits until the car
-- is clear of it. "prm" = taken out.
world.props.want, world.props.breaks, world.props.waits = {}, 0, 0
handlers.pb = function(m)
  if type(m.p) ~= 'table' then return end
  local L, q = m.p, 1
  while q + 2 <= #L do
    local id, brk, n = math.floor(tonumber(L[q]) or 0), tonumber(L[q + 1]) or 0, math.floor(tonumber(L[q + 2]) or 0)
    if n < 1 or n > 8 or q + 2 + n * 8 > #L then logE('props: a "pb" message not understood'); break end
    local bx = {}
    for b = 0, n - 1 do
      local o, t = q + 3 + b * 8, {}
      for k = 0, 7 do t[k + 1] = tonumber(L[o + k]) end
      if t[1] and t[2] and t[3] and t[4] and t[5] and t[6] and t[7] and t[8] and t[4] > t[3] and t[7] > 0 and t[8] > 0 then bx[#bx + 1] = t end
    end
    q = q + 3 + n * 8
    if #bx > 0 then world.props.want['gtap_' .. id] = { id = id, brk = math.max(0, brk), bx = bx } end
  end
end
-- the car's own box (from its nodes, in its frame: right = forward x up; once per drive)
world.props.carExt = function(veh)
  local P = world.props
  if P.ext and P.extVeh == veh then return P.ext end
  local vd = core_vehicle_manager.getPlayerVehicleData()
  local nodes = vd and vd.vdata and vd.vdata.nodes
  if type(nodes) ~= 'table' then return nil end
  local f = norm(v3(veh:getDirectionVector()))
  local u0 = v3(veh:getDirectionVectorUp())
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local lo, hi, cnt = { 1e9, 1e9, 1e9 }, { -1e9, -1e9, -1e9 }, 0
  for _, nd in pairs(nodes) do
    if type(nd) == 'table' and type(nd.cid) == 'number' then
      local ok, p = pcall(function() return v3(veh:getNodePosition(nd.cid)) end)
      if ok and p then
        local l = { dot(p, r), dot(p, f), dot(p, u) }
        for k = 1, 3 do lo[k] = math.min(lo[k], l[k]); hi[k] = math.max(hi[k], l[k]) end
        cnt = cnt + 1
      end
    end
  end
  if cnt < 4 then return nil end
  P.ext = { c = { (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2, (lo[3] + hi[3]) / 2 }, h = { (hi[1] - lo[1]) / 2, (hi[2] - lo[2]) / 2, (hi[3] - lo[3]) / 2 } }
  P.extVeh = veh
  logI(string.format('props: the car is %.2f x %.2f x %.2f m (its %d nodes): prop boxes are checked against that', hi[1] - lo[1], hi[2] - lo[2], hi[3] - lo[3], cnt))
  return P.ext
end
-- the car's box (cb: centre c, right r, forward f, half sizes h) against a prop box t (x, y, bottom, top, along x y, length,
-- thickness), seen from above and in height; m = margin (m)
world.props.overlap = function(cb, t, m)
  if t[4] < cb.c[3] - cb.h[3] - m or t[3] > cb.c[3] + cb.h[3] + m then return false end
  local function flat(v) local l = math.sqrt(v[1] * v[1] + v[2] * v[2]) if l < 1e-6 then return 1, 0 end return v[1] / l, v[2] / l end
  local rx, ry = flat(cb.r)
  local fx, fy = flat(cb.f)
  local dxc, dyc = t[1] - cb.c[1], t[2] - cb.c[2]
  for _, a in ipairs({ { rx, ry }, { fx, fy }, { t[5], t[6] }, { -t[6], t[5] } }) do
    local rc = cb.h[1] * math.abs(rx * a[1] + ry * a[2]) + cb.h[2] * math.abs(fx * a[1] + fy * a[2])
    local rb = t[7] * 0.5 * math.abs(t[5] * a[1] + t[6] * a[2]) + t[8] * 0.5 * math.abs(-t[6] * a[1] + t[5] * a[2])
    if math.abs(dxc * a[1] + dyc * a[2]) > rc + rb + m then return false end
  end
  return true
end
world.props.build = function(sid, w)
  local P = world.props
  local mat = surfMaterial(resolveGround('METAL|ASPHALT'), 'PROP')
  local o, list = w.bx[1], {}
  for _, b in ipairs(w.bx) do
    for _, v in ipairs({ b[1] - o[1], b[2] - o[2], b[3] - o[3], b[4] - o[3], b[5], b[6], b[7], b[8], 0 }) do list[#list + 1] = v end
  end
  local mesh = world.boxMesh(list, mat)
  if not mesh then return end
  removeSurface(sid)
  local ok, obj = pcall(makeProcMesh, sid, { o[1], o[2], o[3] }, { mesh })
  if not ok then logE('props: box ' .. sid .. ' failed: ' .. tostring(obj)); return end
  surfaces[sid] = obj
  surfaceTris[sid] = 10 * #w.bx
  P.boxes[sid] = { id = w.id, brk = w.brk, bx = w.bx }
  P.sent = P.sent + 1
  collisionDirty = true
  if P.sent <= 20 then
    logI(string.format('props: %s prop %s is %d box(es) here (the first %.2f x %.2f x %.1f m)', w.brk > 0 and string.format('breakaway (%.1f m/s)', w.brk) or 'solid', sid,
      #w.bx, o[7], o[8], o[4] - o[3]))
  end
end
world.props.breakBox = function(sid, b, vel, lost)
  local P = world.props
  removeSurface(sid)
  P.boxes[sid] = nil
  world.urgent = true   -- (collision reloaded this frame)
  P.breaks = P.breaks + 1
  local d = drive
  if d then
    pcall(function() udp:sendto(string.format('{"t":"pbk","id":%d,"vx":%.3f,"vy":%.3f,"vz":%.3f,"dv":%.3f}', b.id, vel[1], vel[2], vel[3], lost), d.ip, d.port) end)
  end
  if P.breaks <= 30 then
    logI(string.format('props: the car broke prop %s: it lost %.1f km/h against it (breaks at %.1f): its box is gone, GTA knocks it over', sid, lost * 3.6, b.brk * 3.6))
  end
end
-- every frame while GTA drives: new boxes in (not inside the car), breakaway ones the car pushes on
world.props.tick = function()
  local P, d = world.props, drive
  if not d or not world.ox or (next(P.want) == nil and next(P.boxes) == nil) then return end
  local veh = d.veh or getPlayerVeh()
  if not veh then return end
  local ext = P.carExt(veh)
  if not ext then return end
  local p = v3(veh:getPosition())
  local f = norm(v3(veh:getDirectionVector()))
  local u0 = v3(veh:getDirectionVectorUp())
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local c = ext.c
  local cb = { c = { p[1] + r[1] * c[1] + f[1] * c[2] + u[1] * c[3], p[2] + r[2] * c[1] + f[2] * c[2] + u[2] * c[3], p[3] + r[3] * c[1] + f[3] * c[2] + u[3] * c[3] },
               r = r, f = f, h = ext.h }
  local vel = v3(veh:getVelocity())
  local built = 0
  for sid, w in pairs(P.want) do
    if built >= 8 then break end   -- (8 a frame: many at once at a drive start)
    local inside = false
    for _, b in ipairs(w.bx) do if P.overlap(cb, b, 0.3) then inside = true; break end end
    if inside then
      if not w.waited then w.waited = true; P.waits = P.waits + 1 end
    else
      P.want[sid] = nil
      P.build(sid, w)
      built = built + 1
    end
  end
  local t = now()
  for sid, b in pairs(P.boxes) do
    if b.brk and b.brk > 0 then
      local touch = false
      local b1 = b.bx[1]
      if (b1[1] - cb.c[1]) ^ 2 + (b1[2] - cb.c[2]) ^ 2 < 144 then
        for _, bb in ipairs(b.bx) do if P.overlap(cb, bb, 0.05) then touch = true; break end end
      end
      if touch then
        if not b.t0 then
          b.t0, b.vp, b.tp, b.lost = t, vel, t, 0
          local sp = math.sqrt(dot(vel, vel))
          b.dir = sp > 0.3 and { vel[1] / sp, vel[2] / sp, vel[3] / sp } or nil
        end
        -- v2.52: only what the box takes counts: speed lost faster than brakes can (12 m/s2); 2.51 counted braking next to a post
        local dt = math.max(0, t - b.tp)
        if b.dir and dt > 0 then b.lost = b.lost + math.max(0, dot({ b.vp[1] - vel[1], b.vp[2] - vel[2], b.vp[3] - vel[3] }, b.dir) - 12 * dt) end
        b.vp, b.tp = vel, t
        if b.lost >= b.brk then P.breakBox(sid, b, vel, b.lost) end
      else
        b.t0, b.vp, b.dir, b.lost = nil, nil, nil, nil
      end
    end
  end
end
-- all of them out (the drive ended without GTA's "prm": GTA closed; a new session with another origin)
world.props.clear = function(why)
  local n = 0
  for id in pairs(world.props.boxes) do
    removeSurface(id)
    n = n + 1
  end
  world.props.boxes, world.props.want, world.props.ext = {}, {}, nil
  if n > 0 then
    collisionDirty = true
    logI('props: ' .. n .. ' prop boxes taken out (' .. tostring(why) .. ')')
  end
end
handlers.prm = function(m)
  for _, i in ipairs(type(m.ids) == 'table' and m.ids or {}) do
    local id = 'gtap_' .. tostring(math.floor(tonumber(i) or 0))
    pcall(world.phys.remove, math.floor(tonumber(i) or 0), 'GTA took it out')
    world.props.want[id] = nil
    if world.props.boxes[id] then
      removeSurface(id)
      world.props.boxes[id] = nil
    end
  end
end
-- v2.52: physics props (your choice): GTA's props as small weighted boxes BeamNG simulates. The mod's own jbeam "vehicle"
-- (vehicles/gtab_prop: 8 corner nodes, beams, triangles; no 3D model, drawn as yellow lines): size and mass from GTA ($gx $gy
-- $gz $gm); posts are tied to 4 fixed nodes in the ground by beams that snap at $gb newtons (part gtab_prop_anchor). GTA's
-- prop copies the box: "pp" = per prop how far its middle moved and how it turned since it settled. "pq" = spawn these
-- (id, anchored, mass, strength, bottom centre x y z, rotation x y z w, size x y z; BeamNG world), "prm" = take out.
-- How BeamNG places a spawned vehicle is not documented: the first spawns are measured, and a fixed offset or a mirrored
-- rotation is learned and corrected; when spawning can't work the GTA plugin is told ("pqs") and uses fixed boxes.
world.phys = { list = {}, want = {}, ok = nil, how = nil, comp = nil, conj = false, fails = 0, spawned = 0, spawnMs = 0, spawnMax = 0,
               spawnT = 0, poses = 0, knocked = 0, logT = 0, noDraw = false, tries = 0 }
handlers.pq = function(m)
  local P = world.phys
  if type(m.p) ~= 'table' then return end
  local L = m.p
  for q = 1, #L - 13, 14 do
    local v = {}
    for k = 0, 13 do v[k + 1] = tonumber(L[q + k]) end
    local id = v[1] and math.floor(v[1])
    if id and v[14] and not P.list[id] then
      P.want[id] = { id = id, anchor = v[2] == 1, mass = math.max(4, math.min(5000, v[3])), brk = math.max(1, v[4]), pos = { v[5], v[6], v[7] },
                     q = { v[8], v[9], v[10], v[11] }, size = { math.max(0.12, v[12]), math.max(0.12, v[13]), math.max(0.12, v[14]) }, t = now() }
    end
  end
  if P.ok == false then pcall(function() udp:sendto('{"t":"pqs","ok":0,"why":' .. jsonEncode(P.why or 'spawning failed') .. '}', drive.ip, drive.port) end) end
end
-- quaternion helpers ({x, y, z, w}) and a box frame from its nodes
world.phys.qrot = function(q, v)
  local t = cross({ q[1], q[2], q[3] }, v)
  t = { 2 * t[1], 2 * t[2], 2 * t[3] }
  local c = cross({ q[1], q[2], q[3] }, t)
  return { v[1] + q[4] * t[1] + c[1], v[2] + q[4] * t[2] + c[2], v[3] + q[4] * t[3] + c[3] }
end
world.phys.qOfFrame = function(r, f, u)   -- columns: local x, y, z in world
  local m00, m01, m02, m10, m11, m12, m20, m21, m22 = r[1], f[1], u[1], r[2], f[2], u[2], r[3], f[3], u[3]
  local tr = m00 + m11 + m22
  if tr > 0 then local s = math.sqrt(tr + 1) * 2; return { (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25 * s }
  elseif m00 > m11 and m00 > m22 then local s = math.sqrt(1 + m00 - m11 - m22) * 2; return { 0.25 * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s }
  elseif m11 > m22 then local s = math.sqrt(1 + m11 - m00 - m22) * 2; return { (m01 + m10) / s, 0.25 * s, (m12 + m21) / s, (m02 - m20) / s }
  else local s = math.sqrt(1 + m22 - m00 - m11) * 2; return { (m02 + m20) / s, (m12 + m21) / s, 0.25 * s, (m10 - m01) / s } end
end
-- the box's corners (world): g0..g7 = (-x,-y,0) (+x,-y,0) (-x,+y,0) (+x,+y,0), then the same at the top
world.phys.corners = function(e)
  local veh = e.veh
  local ok, pos = pcall(function() return v3(veh:getPosition()) end)
  if not ok or not pos then return nil end
  local c = {}
  for k = 1, 8 do
    local ok2, p = pcall(function() return v3(veh:getNodePosition(e.cids[k])) end)
    if not ok2 or not p then return nil end
    c[k] = { pos[1] + p[1], pos[2] + p[2], pos[3] + p[3] }
  end
  return c
end
world.phys.frame = function(c)
  local function sub(a, b) return { a[1] - b[1], a[2] - b[2], a[3] - b[3] } end
  local x = norm(sub(c[2], c[1]))
  local y0 = sub(c[3], c[1])
  local z = norm(cross(x, y0))
  local y = cross(z, x)
  local m = { 0, 0, 0 }
  for k = 1, 8 do for i = 1, 3 do m[i] = m[i] + c[k][i] / 8 end end
  return m, x, y, z
end
world.phys.spawnOne = function(w)
  local P = world.phys
  local q = P.conj and { -w.q[1], -w.q[2], -w.q[3], w.q[4] } or w.q
  local pos = { w.pos[1], w.pos[2], w.pos[3] + 0.05 }   -- (5 cm up: it settles on GTA's ground; never starts inside it)
  if P.comp then
    -- a learned placement offset (in the box's frame, per metre of its size): taken back
    local o = P.qrot(w.q, { P.comp[1] * w.size[1], P.comp[2] * w.size[2], P.comp[3] * w.size[3] })
    pos = { pos[1] - o[1], pos[2] - o[2], pos[3] - o[3] }
  end
  local cfgT = { format = 2, model = 'gtab_prop', parts = w.anchor and { gtab_prop_anchor = 'gtab_prop_anchor' } or {},
                 vars = { ['$gx'] = w.size[1], ['$gy'] = w.size[2], ['$gz'] = w.size[3], ['$gm'] = w.mass, ['$gb'] = w.brk } }
  local opts = { config = cfgT, pos = vec3(pos[1], pos[2], pos[3]), rot = quat(q[1], q[2], q[3], q[4]), autoEnterVehicle = false, cling = false,
                 vehicleName = 'gtab_prop_' .. w.id }
  local car = drive and drive.veh or getPlayerVeh()
  local t0 = os.clock()
  local veh, err, how
  for _, way in ipairs({ P.how or 'core_vehicles', 'spawn' }) do
    if not veh then
      local ok, v = pcall(function()
        if way == 'core_vehicles' then return core_vehicles.spawnNewVehicle('gtab_prop', opts) end
        return spawn.spawnVehicle('gtab_prop', serialize(cfgT), opts.pos, opts.rot, { autoEnterVehicle = false, cling = false, vehicleName = opts.vehicleName })
      end)
      if ok and v then veh, how = v, way elseif not ok then err = (err and (err .. '; ') or '') .. way .. ': ' .. tostring(v) end
    end
  end
  local ms = (os.clock() - t0) * 1000
  P.spawnMs, P.spawnMax = P.spawnMs + ms, math.max(P.spawnMax, ms)
  -- (BeamNG's player car stays the one GTA drives)
  pcall(function() if car and be:getPlayerVehicle(0):getID() ~= car:getID() then be:enterVehicle(0, car) end end)
  if not veh then return nil, err or 'no vehicle came back' end
  P.how = how
  P.spawned = P.spawned + 1
  local vid
  pcall(function() vid = veh:getID() end)
  return { id = w.id, w = w, veh = veh, vid = vid, t0 = now(), ms = ms }
end
world.phys.remove = function(id, why)
  local P = world.phys
  P.want[id] = nil
  local e = P.list[id]
  if not e then return end
  P.list[id] = nil
  pcall(function() e.veh:delete() end)
end
world.phys.clear = function(why)
  local P = world.phys
  local n = 0
  for id in pairs(P.list) do P.remove(id, why); n = n + 1 end
  P.want = {}
  if n > 0 then logI('props: ' .. n .. ' physics props taken out (' .. tostring(why) .. ')') end
end
world.phys.fail = function(why, id)
  local P = world.phys
  P.fails = P.fails + 1
  if id then pcall(function() udp:sendto(string.format('{"t":"pqf","id":%d,"why":%s}', id, jsonEncode(why)), drive.ip, drive.port) end) end
  if P.ok == nil and P.fails >= 3 then
    P.ok, P.why = false, why
    logE('props: physics props don\'t work here (' .. tostring(why) .. '): GTA uses fixed boxes instead')
    pcall(function() udp:sendto('{"t":"pqs","ok":0,"why":' .. jsonEncode(why) .. '}', drive.ip, drive.port) end)
    P.clear('physics props off')
  end
end
-- a spawned box: its nodes found, its size and place checked (and the placement learned from the first ones)
world.phys.ready = function(e)
  local P = world.phys
  if not e.cids then
    local cids = {}
    pcall(function()
      local vd = core_vehicle_manager.getVehicleData(e.vid)
      for _, nd in pairs(vd and vd.vdata and vd.vdata.nodes or {}) do
        local k = type(nd) == 'table' and type(nd.name) == 'string' and tonumber(nd.name:match('^g(%d)$'))
        if k then cids[k + 1] = nd.cid end
      end
    end)
    if #cids < 8 then cids = { 0, 1, 2, 3, 4, 5, 6, 7 } end
    e.cids = cids
  end
  local c = P.corners(e)
  if not c then return false end
  local w = e.w
  local function dist(a, b) return math.sqrt((a[1] - b[1]) ^ 2 + (a[2] - b[2]) ^ 2 + (a[3] - b[3]) ^ 2) end
  local sx, sy, sz = dist(c[1], c[2]), dist(c[1], c[3]), dist(c[1], c[5])
  if sx < 0.01 and sy < 0.01 then return false end   -- (its nodes aren't there yet)
  if math.abs(sx - w.size[1]) > 0.03 + 0.1 * w.size[1] or math.abs(sy - w.size[2]) > 0.03 + 0.1 * w.size[2] or math.abs(sz - w.size[3]) > 0.03 + 0.1 * w.size[3] then
    return nil, string.format('its size came out %.2f x %.2f x %.2f m, not %.2f x %.2f x %.2f (the size wasn\'t passed: jbeam variables)', sx, sy, sz, w.size[1], w.size[2], w.size[3])
  end
  local m, x = P.frame(c)
  -- where its bottom is vs where it was asked to be (the box's frame)
  local b = { (c[1][1] + c[2][1] + c[3][1] + c[4][1]) / 4, (c[1][2] + c[2][2] + c[3][2] + c[4][2]) / 4, (c[1][3] + c[2][3] + c[3][3] + c[4][3]) / 4 }
  local want = { w.pos[1], w.pos[2], w.pos[3] + 0.05 }
  local off = { b[1] - want[1], b[2] - want[2], b[3] - want[3] }
  local wx = P.qrot(w.q, { 1, 0, 0 })
  local align = math.abs(dot(x, wx))
  if align < 0.97 then
    if not P.conj and P.ok == nil then
      P.conj = true
      logI('props: spawned boxes come out turned the other way: rotations mirrored from now on')
      return nil, 'turned wrong', true
    end
    return nil, string.format('turned wrong (%.2f)', align)
  end
  if math.sqrt(dot(off, off)) > 0.03 then
    if not P.comp and P.ok == nil then
      local qi = { -w.q[1], -w.q[2], -w.q[3], w.q[4] }
      local lo = P.qrot(qi, off)
      P.comp = { lo[1] / w.size[1], lo[2] / w.size[2], lo[3] / w.size[3] }
      logI(string.format('props: BeamNG puts a spawned box %.2f m from where it was asked (%.2f %.2f %.2f in its frame): taken back from now on', math.sqrt(dot(off, off)), lo[1], lo[2], lo[3]))
      return nil, 'placed off', true
    end
    if math.sqrt(dot(off, off)) > 0.3 then return nil, string.format('placed %.2f m off', math.sqrt(dot(off, off))) end
  end
  return true
end
world.phys.tick = function()
  local P, d = world.phys, drive
  if not d or not world.ox then return end
  if next(P.want) == nil and next(P.list) == nil then return end
  local t = now()
  -- one spawn a frame, 4 a second at most (each one may hitch BeamNG: the log says how long), nearest first, never inside the car
  if P.ok ~= false and t - P.spawnT > 0.25 then
    local car = d.veh or getPlayerVeh()
    local cp = car and v3(car:getPosition())
    local list = {}
    for id, w in pairs(P.want) do
      local dd = cp and ((w.pos[1] - cp[1]) ^ 2 + (w.pos[2] - cp[2]) ^ 2) or 0
      local r = math.max(w.size[1], w.size[2]) * 0.5 + 2.6
      if dd > r * r then list[#list + 1] = { w, dd } end
    end
    table.sort(list, function(a, b) return a[2] < b[2] end)
    -- (only where GTA's ground is in BeamNG's collision already: else a loose one would fall through)
    local best
    for _, it in ipairs(list) do
      local w = it[1]
      local okR, h = pcall(function() return castRayStatic(vec3(w.pos[1], w.pos[2], w.pos[3] + 1.0), vec3(0, 0, -1), 3) end)
      if okR and type(h) == 'number' and h < 2.5 then best = w; break end
    end
    if best then
      P.want[best.id] = nil
      P.spawnT = t
      local e, err = P.spawnOne(best)
      if e then P.list[best.id] = e else P.fail(tostring(err), best.id) end
    end
  end
  local out = {}
  for id, e in pairs(P.list) do
    if not e.ok then
      local r, why, learned = P.ready(e)
      if learned then
        P.remove(id, why)
        P.want[id] = e.w   -- (spawned again, placed the learned way)
      elseif r then
        e.ok, e.readyT = true, t
        if P.ok == nil then
          P.ok = true
          logI(string.format('props: physics props work (%s; the first spawn took %.0f ms)', tostring(P.how), e.ms))
          pcall(function() udp:sendto('{"t":"pqs","ok":1}', d.ip, d.port) end)
        end
      elseif r == nil or t - e.t0 > 3 then
        why = why or 'its nodes never came'
        P.remove(id, why)
        P.fail(why, id)
        if P.fails <= 5 then logE('props: physics prop ' .. id .. ' failed: ' .. tostring(why)) end
      end
    elseif not e.m0 then
      -- settled (0.4 s after it came): this is where GTA's prop is
      if t - e.readyT > 0.4 then
        local c = P.corners(e)
        if c then
          local m, x, y, z = P.frame(c)
          e.m0, e.q0i = m, P.qOfFrame(x, y, z)
          e.zlo0 = math.min(c[1][3], c[2][3], c[3][3], c[4][3], c[5][3], c[6][3], c[7][3], c[8][3])   -- (its lowest corner: a fall goes by that)
          e.q0i = { -e.q0i[1], -e.q0i[2], -e.q0i[3], e.q0i[4] }
        end
      end
    else
      local c = P.corners(e)
      if c then
        local m, x, y, z = P.frame(c)
        local qn = P.qOfFrame(x, y, z)
        -- turned since it settled: qn * inverse(q0)
        local a, b = qn, e.q0i
        local qr = { a[4] * b[1] + a[1] * b[4] + a[2] * b[3] - a[3] * b[2], a[4] * b[2] - a[1] * b[3] + a[2] * b[4] + a[3] * b[1],
                     a[4] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[4], a[4] * b[4] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3] }
        if qr[4] < 0 then qr = { -qr[1], -qr[2], -qr[3], -qr[4] } end
        local dm = { m[1] - e.m0[1], m[2] - e.m0[2], m[3] - e.m0[3] }
        local moved = math.sqrt(dot(dm, dm))
        local zlo = math.min(c[1][3], c[2][3], c[3][3], c[4][3], c[5][3], c[6][3], c[7][3], c[8][3])
        if zlo < (e.zlo0 or zlo) - 1.5 then
          -- fell through the ground (a gap in GTA's): out, and GTA's prop goes back where it stood
          P.remove(id, 'fell through the ground')
          pcall(function() udp:sendto(string.format('{"t":"pqf","id":%d,"why":"fell through the ground"}', id), d.ip, d.port) end)
          if P.knocked <= 30 then logI('props: physics prop ' .. id .. ' fell through GTA\'s ground: taken out') end
          goto nextProp
        end
        local ang = 2 * math.acos(math.min(1, qr[4]))
        if not e.moving and (moved > 0.02 or ang > 0.017) then e.moving = true end
        if e.moving then
          local lm, lq = e.lastM or { 0, 0, 0 }, e.lastQ or { 0, 0, 0, 1 }
          local dd = (dm[1] - lm[1]) ^ 2 + (dm[2] - lm[2]) ^ 2 + (dm[3] - lm[3]) ^ 2
          local dq = math.abs(qr[1] * lq[1] + qr[2] * lq[2] + qr[3] * lq[3] + qr[4] * lq[4])
          if (dd > 0.005 ^ 2 or dq < 0.99999) and t - (e.sentT or 0) > 0.033 then
            e.lastM, e.lastQ, e.sentT = dm, qr, t
            out[#out + 1] = string.format('%d,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f', id, dm[1], dm[2], dm[3], qr[1], qr[2], qr[3], qr[4])
          end
        end
        if not e.knocked and (moved > 0.15 or ang > 0.17) then
          e.knocked = true
          P.knocked = P.knocked + 1
          local v = d.vel or { 0, 0, 0 }
          local sp = math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2)
          if P.knocked <= 30 then logI(string.format('props: the car knocked %s prop %d (%.0f kg) at %.0f km/h: moved %.2f m, tilted %.0f deg', e.w.anchor and 'post' or 'loose', id, e.w.mass, sp * 3.6, moved, math.deg(ang))) end
          pcall(function() udp:sendto(string.format('{"t":"pk","id":%d,"v":%.2f}', id, sp), d.ip, d.port) end)
        end
        -- drawn in BeamNG's window (it has no 3D model): its edges in yellow
        if not P.noDraw then
          local okD = pcall(function()
            local col = ColorF(1, 0.75, 0.1, 1)
            for _, ed in ipairs({ { 1, 2 }, { 2, 4 }, { 4, 3 }, { 3, 1 }, { 5, 6 }, { 6, 8 }, { 8, 7 }, { 7, 5 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, { 4, 8 } }) do
              local A, B = c[ed[1]], c[ed[2]]
              debugDrawer:drawLine(vec3(A[1], A[2], A[3]), vec3(B[1], B[2], B[3]), col)
            end
          end)
          if not okD then P.noDraw = true end
        end
      end
    end
    ::nextProp::
  end
  if #out > 0 then
    P.poses = P.poses + #out
    for k = 1, #out, 20 do
      local part = {}
      for j = k, math.min(#out, k + 19) do part[#part + 1] = out[j] end
      pcall(function() udp:sendto('{"t":"pp","p":[' .. table.concat(part, ',') .. ']}', d.ip, d.port) end)
    end
  end
  if t - P.logT > 10 then
    P.logT = t
    local n, nw = 0, 0
    for _ in pairs(P.list) do n = n + 1 end
    for _ in pairs(P.want) do nw = nw + 1 end
    if n + nw + P.spawned > 0 then
      logI(string.format('props: physics: %d here, %d waiting, %d spawned (%.0f ms each, max %.0f), %d failed, %d knocked, %d pose updates to GTA (10 s)', n, nw, P.spawned,
        P.spawned > 0 and P.spawnMs / P.spawned or 0, P.spawnMax, P.fails, P.knocked, P.poses))
    end
    P.spawned, P.spawnMs, P.spawnMax, P.poses = 0, 0, 0, 0
  end
end
-- v2.51: how bumpy the ride is on GTA's ground (to compare versions): jolts = the car's vertical speed changing faster than
-- 1.5 g, counted above 50 km/h, per km
world.feel = { n = 0, km = 0 }
world.feel.tick = function(dtSim)
  local d, F = drive, world.feel
  if not d or not world.ox or type(d.vel) ~= 'table' or type(dtSim) ~= 'number' or dtSim <= 0.001 then F.vz = nil; return end
  local v = d.vel
  local sp = math.sqrt(v[1] * v[1] + v[2] * v[2])
  if F.vz and sp > 14 then
    F.km = F.km + sp * dtSim / 1000
    local a = math.abs((v[3] - F.vz) / dtSim)
    local jolt = a > 14.7
    if jolt and not F.inJolt then
      F.n = F.n + 1
      -- v2.61: where (GTA's coordinates), for your "bouncy on some flat roads": the 5 hardest of each 10 s
      local car = d.veh or getPlayerVeh()
      local cp = car and car:getPosition()
      if cp then
        F.at = F.at or {}
        F.at[#F.at + 1] = { a / 9.81, cp.x + world.ox, cp.y + world.oy, sp * 3.6 }
        table.sort(F.at, function(x, y) return x[1] > y[1] end)
        if #F.at > 5 then F.at[6] = nil end
      end
    end
    F.inJolt = jolt
  end
  F.vz = v[3]
end

-- ---------------------------------------------------------------- v2.5 (D2): dents shown in GTA
-- The car's own Lua (gtabDents, put in when the drive starts) watches its nodes: how far each one moved since the
-- drive started, in the car's own frame (driving and turning don't count, only bending; the wheels' nodes are left
-- out). Nodes that moved 5 mm or more (v2.53; was 1 cm) go to GTA ("dm" messages, one per range of 256 node ids)
-- whenever they change by 2 mm or more, and one range more each time (a lost message is fixed soon). The GTA plugin bends its drawn car with
-- them (it knows which nodes each vertex follows from the car file: the export below writes the nodes into it).
-- A repairing teleport (your repair binding, putting the car back after a fall) takes the reference again once the
-- car settled. UNVERIFIED in BeamNG 0.39: obj:getDirectionVector/Up in the car's Lua (else from the car's refNodes),
-- the flexbodies' "_group_nodes" form (the export logs a sample).
dents.WHEELS = [==[
local function gtabWheelNodes(data)
  -- the wheels' nodes (they turn with the wheel: nothing on the body may follow them), by id
  local set, byName = {}, {}
  for cid, n in pairs(data.nodes or {}) do
    if type(n) == 'table' and n.name then byName[n.name] = n.cid or cid end
  end
  local function add(x)
    if type(x) == 'number' then set[x] = true
    elseif type(x) == 'string' and byName[x] then set[byName[x]] = true
    elseif type(x) == 'table' and type(x.cid) == 'number' then set[x.cid] = true end
  end
  for _, w in pairs(data.wheels or {}) do
    if type(w) == 'table' then
      for k, val in pairs(w) do
        if type(k) == 'string' and k:lower():find('node', 1, true) and k ~= 'nodeArm' then   -- (nodeArm: on the suspension)
          if type(val) == 'table' and val.cid == nil then
            for _, x in pairs(val) do add(x) end
          else
            add(val)
          end
        end
      end
    end
  end
  return set, byName
end
]==]

-- in the car's Lua
dents.VEH = dents.WHEELS .. [==[
gtabDents = gtabDents or { errs = {} }
local D = gtabDents
D.ver = 1
local function report(kind, note)
  obj:queueGameEngineLua(string.format('if gtaBridge and gtaBridge.dentsInfo then gtaBridge.dentsInfo(%d, %d, %d, %q, %q) end', obj:getID(),
    D.nn or -1, D.cids and #D.cids or -1, tostring(kind), tostring(note or '')))
end
-- the car's frame: forward, up (BeamNG's direction vectors), right = forward x up (the GTA plugin's car space)
function D.frame()
  local f, u
  pcall(function() f = obj:getDirectionVector() u = obj:getDirectionVectorUp() end)
  if not (f and u) then
    local rn = v.data.refNodes and (v.data.refNodes[0] or v.data.refNodes[1])
    if not rn then return nil end
    local a, b, c = obj:getNodePosition(rn.ref), obj:getNodePosition(rn.back), obj:getNodePosition(rn.up)
    f = { x = a.x - b.x, y = a.y - b.y, z = a.z - b.z }
    u = { x = c.x - a.x, y = c.y - a.y, z = c.z - a.z }
    D.frameHow = 'refNodes'
  else
    D.frameHow = 'getDirectionVector'
  end
  local fx, fy, fz = f.x, f.y, f.z
  local l = math.sqrt(fx * fx + fy * fy + fz * fz)
  if not (l > 1e-6) then return nil end
  fx, fy, fz = fx / l, fy / l, fz / l
  local ux, uy, uz = u.x, u.y, u.z
  local dd = ux * fx + uy * fy + uz * fz
  ux, uy, uz = ux - fx * dd, uy - fy * dd, uz - fz * dd
  l = math.sqrt(ux * ux + uy * uy + uz * uz)
  if not (l > 1e-6) then return nil end
  ux, uy, uz = ux / l, uy / l, uz / l
  return fy * uz - fz * uy, fz * ux - fx * uz, fx * uy - fy * ux, fx, fy, fz, ux, uy, uz
end
-- every watched node in the car's frame (relative to the car's position: a move or turn of the whole car cancels out)
function D.read(out)
  local rx, ry, rz, fx, fy, fz, ux, uy, uz = D.frame()
  if not rx then return false end
  local C = D.cids
  for i = 1, #C do
    local p = obj:getNodePosition(C[i])
    local x, y, z = p.x, p.y, p.z
    out[i * 3 - 2] = x * rx + y * ry + z * rz
    out[i * 3 - 1] = x * fx + y * fy + z * fz
    out[i * 3] = x * ux + y * uy + z * uz
  end
  return true
end
-- the nodes to watch: all but the wheels' (and any inside a wheel: hub, tyre), sorted by id
function D.init()
  local skip, byName = gtabWheelNodes(v.data)
  local function cidOf(x) if type(x) == 'number' then return x end return byName[x] end
  local W = {}
  for _, w in pairs(v.data.wheels or {}) do
    local a, b = type(w) == 'table' and cidOf(w.node1), type(w) == 'table' and cidOf(w.node2)
    if a and b then
      local pa, pb = obj:getNodePosition(a), obj:getNodePosition(b)
      local ax, ay, az = pb.x - pa.x, pb.y - pa.y, pb.z - pa.z
      local l = math.sqrt(ax * ax + ay * ay + az * az)
      if l > 1e-4 then
        W[#W + 1] = { x = (pa.x + pb.x) * 0.5, y = (pa.y + pb.y) * 0.5, z = (pa.z + pb.z) * 0.5, ax = ax / l, ay = ay / l, az = az / l,
                      r = (tonumber(w.radius) or 0.35) + 0.03 }
      end
    end
  end
  local all = {}
  D.nn = 0
  for _, n in pairs(v.data.nodes or {}) do
    if type(n) == 'table' and type(n.cid) == 'number' then
      D.nn = D.nn + 1
      if not skip[n.cid] then all[#all + 1] = n.cid end
    end
  end
  table.sort(all)
  local keep, inWheels = {}, 0
  for _, c in ipairs(all) do
    local p = obj:getNodePosition(c)
    local inW = false
    for _, w in ipairs(W) do
      local dx, dy, dz = p.x - w.x, p.y - w.y, p.z - w.z
      local al = dx * w.ax + dy * w.ay + dz * w.az
      if math.abs(al) < 0.3 then
        local qx, qy, qz = dx - al * w.ax, dy - al * w.ay, dz - al * w.az
        if qx * qx + qy * qy + qz * qz < w.r * w.r then inW = true break end
      end
    end
    if inW then inWheels = inWheels + 1 else keep[#keep + 1] = c end
  end
  D.cids = keep
  D.blk, D.first, D.blocks = {}, {}, {}
  for i, c in ipairs(keep) do
    local b = math.floor(c / 256)
    D.blk[i] = b
    if not D.first[b] then D.first[b] = i; D.blocks[#D.blocks + 1] = b end
  end
  D.skipped = (D.nn - #all) .. ' wheel nodes, ' .. inWheels .. ' inside the wheels'
end
-- the reference: where the nodes are now (force: again, after a repair)
function D.takeRef(force)
  if D.R and not force then report('kept', 'the reference from before') return end
  D.init()
  local R = {}
  if not D.read(R) then report('error', 'the car\'s frame could not be read') return end
  D.R, D.C, D.S, D.A, D.ch = R, {}, {}, {}, {}
  for i = 1, #D.cids * 3 do D.S[i] = 0 end
  for i = 1, #D.cids do D.A[i] = false end
  for _, b in ipairs(D.blocks) do D.send(b) end   -- GTA: all of the car as it was
  report(force and 'again' or 'taken', D.skipped .. ' left out; frame from ' .. tostring(D.frameHow))
end
function D.send(b)
  local t, S, C = {}, D.S, D.cids
  local hi = (b + 1) * 256
  for i = D.first[b] or (#C + 1), #C do
    local c = C[i]
    if c >= hi then break end
    local k = i * 3
    local x, y, z = S[k - 2], S[k - 1], S[k]
    if x ~= 0 or y ~= 0 or z ~= 0 then t[#t + 1] = string.format('%d,%d,%d,%d', c, x, y, z) end
  end
  obj:queueGameEngineLua(string.format("if gtaBridge and gtaBridge.dents then gtaBridge.dents(%d,%d,%d,%d,'%s') end", obj:getID(), D.nn, b * 256, hi, table.concat(t, ';')))
end
function D.tick()
  local R = D.R
  if not R then return end
  local C = D.C
  if not D.read(C) then return end
  local S, A, B, ch = D.S, D.A, D.blk, D.ch
  local floor, abs = math.floor, math.abs
  for i = 1, #D.cids do
    local k = i * 3
    local dx, dy, dz = C[k - 2] - R[k - 2], C[k - 1] - R[k - 1], C[k] - R[k]
    local m2 = dx * dx + dy * dy + dz * dz
    local a = A[i]
    if a then a = m2 > 0.000009 else a = m2 > 0.000025 end   -- v2.53: on at 5 mm, off below 3 mm (1 cm left steps in GTA's panels)
    A[i] = a
    local qx, qy, qz = 0, 0, 0
    if a then qx, qy, qz = floor(dx * 1000 + 0.5), floor(dy * 1000 + 0.5), floor(dz * 1000 + 0.5) end
    local sx, sy, sz = S[k - 2], S[k - 1], S[k]
    if abs(qx - sx) > 1 or abs(qy - sy) > 1 or abs(qz - sz) > 1 or (a ~= (sx ~= 0 or sy ~= 0 or sz ~= 0)) then
      S[k - 2], S[k - 1], S[k] = qx, qy, qz
      ch[B[i]] = true
    end
  end
  local nb = #D.blocks
  if nb > 0 then
    D.rr = (D.rr or 0) % nb + 1
    ch[D.blocks[D.rr]] = true
  end
  for b in pairs(ch) do
    D.send(b)
    ch[b] = nil
  end
end
function D.run(name, a)
  local ok, e = pcall(D[name], a)
  if not ok and not D.errs[name] then
    D.errs[name] = true
    report('error', name .. ': ' .. tostring(e))
  end
end
]==]

dents.wheelNodes = (loadstring or load)(dents.WHEELS .. '\nreturn gtabWheelNodes')()

-- export: the car's nodes (id, position in glTF axes relative to the car like the wheels' nodes, wheel node) and, v2.53,
-- each flexbody's own nodes (its "_group_nodes": the nodes BeamNG bends its mesh with) and each prop's 3 nodes (a
-- mirror, a light: carried rigidly). GTA's loader puts every vertex of a part in a triangle of that part's nodes.
function dents.exportNodes(veh)
  local vd = core_vehicle_manager.getPlayerVehicleData()
  local vdata = vd and vd.vdata
  if not vdata or type(vdata.nodes) ~= 'table' then return nil, nil, 'no vehicle data' end
  local wheelSet, byName = dents.wheelNodes(vdata)
  local list, isNode = {}, {}
  for _, n in pairs(vdata.nodes) do
    if type(n) == 'table' and type(n.cid) == 'number' then list[#list + 1] = n; isNode[n.cid] = true end
  end
  table.sort(list, function(a, b) return a.cid < b.cid end)
  local C, P, W, nw = {}, {}, {}, 0
  local function r3(x) return math.floor(x * 1000 + 0.5) / 1000 end
  for i, n in ipairs(list) do
    local p = vec3(veh:getNodePosition(n.cid))
    C[i] = n.cid
    P[i * 3 - 2], P[i * 3 - 1], P[i * 3] = r3(p.x), r3(p.z), r3(-p.y)
    W[i] = wheelSet[n.cid] and 1 or 0
    nw = nw + W[i]
  end
  -- node ids out of whatever form the processed data keeps them in (ids, names, {cid=}, {[id]=true}, nested lists)
  local function cidsOf(t, set, depth)
    if type(t) ~= 'table' or depth > 3 then return end
    for k, x in pairs(t) do
      local c
      if type(x) == 'number' then c = x
      elseif type(x) == 'string' then c = byName[x]
      elseif type(x) == 'table' then if type(x.cid) == 'number' then c = x.cid else cidsOf(x, set, depth + 1) end
      elseif x == true then c = type(k) == 'number' and k or byName[k] end
      if c and isNode[c] then set[c] = true end
    end
  end
  local function oneCid(x) if type(x) == 'number' and isNode[x] then return x end return byName[x] end
  local sets, nf, nfNo, sample, keys = {}, 0, 0, nil, nil
  for _, fb in pairs(vdata.flexbodies or {}) do
    if type(fb) == 'table' and type(fb.mesh) == 'string' then
      if not keys then
        keys = {}
        for k in pairs(fb) do keys[#keys + 1] = tostring(k) end
        table.sort(keys)
      end
      local gn = fb._group_nodes
      if not sample then
        local cnt, first = 0, nil
        for k, x in pairs(type(gn) == 'table' and gn or {}) do cnt = cnt + 1; if not first then first = tostring(k) .. '=' .. type(x) .. ' ' .. tostring(x) end end
        sample = string.format('%s: %s with %d entries like [%s]', fb.mesh, type(gn), cnt, tostring(first))
      end
      local set = sets[fb.mesh] or {}
      local before = next(set)
      cidsOf(gn, set, 0)
      if next(set) then sets[fb.mesh] = set; if not before then nf = nf + 1 end else nfNo = nfNo + 1 end
    end
  end
  local flex, nodesIn = {}, 0
  for mesh, set in pairs(sets) do
    local t = {}
    for c in pairs(set) do t[#t + 1] = c end
    table.sort(t)
    flex[mesh] = t
    nodesIn = nodesIn + #t
  end
  local props, np = {}, 0
  for _, pr in pairs(vdata.props or {}) do
    if type(pr) == 'table' and type(pr.mesh) == 'string' and not flex[pr.mesh] and not props[pr.mesh] then
      local r, x, y = oneCid(pr.idRef or pr['idRef:']), oneCid(pr.idX or pr['idX:']), oneCid(pr.idY or pr['idY:'])
      if r and x and y then props[pr.mesh] = { r, x, y }; np = np + 1 end
    end
  end
  local nkeys = {}
  for k in pairs(list[1] or {}) do nkeys[#nkeys + 1] = tostring(k) end
  table.sort(nkeys)
  local note = string.format('%d nodes (%d of the wheels); %d flexbody meshes with their own nodes (%.0f each on average), %d without; %d props on 3 nodes (%s; a flexbody\'s fields: %s; a node\'s fields: %s)',
    #C, nw, nf, nf > 0 and nodesIn / nf or 0, nfNo, np, tostring(sample), keys and table.concat(keys, ' '):sub(1, 200) or 'no flexbodies', table.concat(nkeys, ' '):sub(1, 200))
  if #C == 0 then return nil, nil, 'the car has no nodes' end
  if nf == 0 then flex = nil end
  return { c = C, p = P, w = W }, flex, note, np > 0 and props or nil
end

-- GE side, while GTA drives
function dents.start(d, rate)
  d.dz = tonumber(rate) or 0
  d.dentT, d.dentInj, d.dentRef, d.dentRefAt = 0, false, 'soft', now() + 1.0
  d.dentRx, d.dentRefT, d.dentMsgs, d.dentBytes, d.dentReinj = nil, nil, 0, 0, 0
  if d.dz > 0 then logI(string.format('dents: on (%.0f checks a second)', d.dz)) else logI('dents: off (GTA\'s ini: Dents=0)') end
end
function dents.tick(d, veh)
  if not d.dz or d.dz <= 0 then return end
  local t = now()
  if not d.dentInj then
    if d.dentReinj > 10 then return end
    veh:queueLuaCommand(dents.VEH)
    d.dentInj, d.dentInjT = true, t
  end
  if d.dentRef and t >= d.dentRefAt and not world.tp then
    veh:queueLuaCommand("if gtabDents then gtabDents.run('takeRef', " .. (d.dentRef == 'force' and 'true' or 'false') .. ") end")
    d.dentRef, d.dentRefT = nil, t
  end
  if t - d.dentT >= 1 / d.dz then
    d.dentT = t
    veh:queueLuaCommand("if gtabDents then gtabDents.run('tick') end")
  end
  -- nothing from the car for 3 s (its Lua reloaded?): the code goes in again
  if d.dentRefT and not pz.paused and t - d.dentRefT > 3 and (not d.dentRx or t - d.dentRx > 3) and t - d.dentInjT > 3 then
    d.dentReinj = d.dentReinj + 1
    d.dentInj, d.dentRef, d.dentRefAt, d.dentRefT = false, 'soft', t + 0.5, nil
    if d.dentReinj <= 10 then logI('dents: nothing from the car for 3 s: its Lua code goes in again (' .. d.dentReinj .. ')') end
    if d.dentReinj > 10 then logE('dents: the car never answers: dents off for this drive') end
  end
end
function dents.afterTeleport(msg)
  local d = drive
  if not (d and d.dz and d.dz > 0) then return end
  if type(msg) == 'table' and msg.keep == 0 then
    d.dentRef, d.dentRefAt = 'force', now() + 1.0   -- BeamNG's reset repaired the car
  elseif d.dentRef then
    d.dentRefAt = math.max(d.dentRefAt or 0, now() + 1.0)   -- (dropped 0.2 m: its springs settle first)
  end
end
function M.dents(id, nn, a, b, s)
  local d = drive
  if not d or (d.id ~= nil and id ~= d.id) then return end
  d.dentRx = now()
  local msg = string.format('{"t":"dm","nn":%d,"a":%d,"b":%d,"d":"%s"}', tonumber(nn) or -1, tonumber(a) or 0, tonumber(b) or 0, tostring(s):gsub('[^%d,;%-]', ''))
  pcall(function() udp:sendto(msg, d.ip, d.port) end)
  d.dentMsgs = (d.dentMsgs or 0) + 1
  d.dentBytes = (d.dentBytes or 0) + #msg
end
-- BeamNG console: gtaBridge.dentsCheck() -> what the next export (Shift+F11) writes for the dents (nodes, flexbody groups)
function M.dentsCheck()
  local veh = getPlayerVeh()
  if not veh then logI('dents check: no player car'); return end
  local ok, nodes, flex, note = pcall(dents.exportNodes, veh)
  local n = 0
  for _ in pairs(type(flex) == 'table' and flex or {}) do n = n + 1 end
  logI('dents check: ' .. (ok and (nodes and (tostring(note) .. '; meshes with their own nodes: ' .. n) or ('nothing: ' .. tostring(note))) or ('error: ' .. tostring(nodes))))
  flushLog(true)
end
function M.dentsInfo(id, nn, tracked, kind, note)
  local d = drive
  if d and id == d.id then d.dentRx = now() end
  if kind == 'error' then
    logE('dents: in the car: ' .. tostring(note))
  else
    logI(string.format('dents: the car\'s reference %s: %s nodes, %s watched (%s)', kind == 'kept' and 'kept' or (kind == 'again' and 'taken again (repaired)' or 'taken'),
      tostring(nn), tostring(tracked), tostring(note)))
  end
end

-- ---------------------------------------------------------------- v2.6: GTA's weapons and explosions on BeamNG's car
-- GTA sends the bullets that went through its drawn car ("hit": a ray, from 1 m before the car's box, and what the weapon does)
-- and the explosions near it ("blast": where, how hard). In the car's own Lua (gtabWpn, put in when the drive starts):
--   a bullet finds the first of the car's collision triangles on its line (else the node nearest it) and there: a dent (a short
--   push from just outside it: BeamNG's "planets", negative mass pushes), a tyre popped (beamstate.deflateTire), glass / lights
--   broken (beamstate.triggerDeformGroup), engine and radiator damage (the engine's own applyDeformGroupDamage), the engine on fire
--   after enough engine hits (fire.igniteNode);
--   a blast pushes the whole car from where it went off (a planet for dt seconds), can set it on fire, breaks the glass near it;
--   every 0.25 s: the glass / lights BeamNG broke (beamstate.deformGroupsTriggerBeam) and its fire (fire.hotNodes) go to GTA, and
--   after burning wb seconds (or when its fuel goes up in a fireball) it blows up: its own blast, BeamNG's explodeVehicle too when
--   GTA's ini says CarExplosion=beamng, and GTA's explosion at it ("boom").
-- All names read from BeamNG 0.39's own Lua (lua/vehicle: beamstate, fire, powertrain, combustionEngine). UNVERIFIED in game: how
-- strong a planet pushes (logged: the car's speed change after each blast), the flexbodies' deformGroup field (the export also
-- reads the beams' deformSwitches, BeamNG's own list of which mesh breaks with which group).
wpn.VEH = [==[
gtabWpn = gtabWpn or { errs = {} }
local W = gtabWpn
local G = 6.674e-11
local sqrt, abs = math.sqrt, math.abs
local function js(s) return (tostring(s):gsub('[%c"\\]', ' ')) end
local function report(s) obj:queueGameEngineLua(string.format('if gtaBridge and gtaBridge.wfx then gtaBridge.wfx(%d, %q) end', obj:getID(), s)) end
local function cidOf(x) if type(x) == 'number' then return x end if type(x) == 'table' and type(x.cid) == 'number' then return x.cid end return nil end
local function nodeCid(i) local n = v.data.nodes[i] return (type(n) == 'table' and n.cid) or i end
local P = {}
local function pos(c) local p = P[c] if not p then local q = obj:getNodePosition(c) p = { q.x, q.y, q.z } P[c] = p end return p end
-- the car's triangles, glass / light beams, wheels, engine block and radiator nodes (read again when the car is new)
function W.init()
  W.tris, W.glass, W.wheels, W.engine, W.rad, W.devs, W.nb = {}, {}, {}, {}, {}, {}, {}
  for _, t in pairs(v.data.triangles or {}) do
    if type(t) == 'table' and type(t.id1) == 'number' and type(t.id2) == 'number' and type(t.id3) == 'number' then
      W.tris[#W.tris + 1] = { nodeCid(t.id1), nodeCid(t.id2), nodeCid(t.id3) }
    end
  end
  local radGroups = {}
  pcall(function()
    for _, dev in pairs(powertrain.getDevicesByCategory('engine') or {}) do
      W.devs[#W.devs + 1] = dev
      for _, c in pairs(dev.engineBlockNodes or {}) do W.engine[#W.engine + 1] = cidOf(c) or c end
      for _, dg in ipairs(dev.deformGroups or {}) do
        if dg.groupType == 'radiator' then for _, n in ipairs(dg.groupNames or {}) do radGroups[n] = true end end
      end
    end
  end)
  if not next(radGroups) then radGroups.radiator_damage = true end
  for _, b in pairs(v.data.beams or {}) do
    if type(b) == 'table' and b.id1 and b.id2 then
      if type(b.deformSwitches) == 'table' then
        local gs = {}
        for _, g in pairs(b.deformSwitches) do if type(g) == 'table' and g.deformGroup then gs[#gs + 1] = g.deformGroup end end
        if #gs > 0 then W.glass[#W.glass + 1] = { nodeCid(b.id1), nodeCid(b.id2), gs } end
      end
      local g, r = b.deformGroup, false
      if type(g) == 'string' then r = radGroups[g] elseif type(g) == 'table' then for _, x in ipairs(g) do r = r or radGroups[x] end end
      if r then W.rad[#W.rad + 1] = { nodeCid(b.id1), nodeCid(b.id2) } end
      -- v2.61: plain beams by node (a dent changes their rest lengths)
      if type(b.cid) == 'number' and (tonumber(b.beamType) or 0) == 0 then
        local a, c = nodeCid(b.id1), nodeCid(b.id2)
        W.nb[a] = W.nb[a] or {}; W.nb[a][#W.nb[a] + 1] = { b.cid, c }
        W.nb[c] = W.nb[c] or {}; W.nb[c][#W.nb[c] + 1] = { b.cid, a }
      end
    end
  end
  for id, w in pairs(v.data.wheels or {}) do
    if type(w) == 'table' then
      local a, b = cidOf(w.node1), cidOf(w.node2)
      if a and b then W.wheels[#W.wheels + 1] = { id = id, a = a, b = b, r = tonumber(w.radius) or 0.35, wd = tonumber(w.tireWidth) or 0.25, name = tostring(w.name or id) } end
    end
  end
  local nf = 0
  for _ in pairs(fire.flammableNodes or {}) do nf = nf + 1 end
  W.burnT, W.engHits = W.burnT or 0, W.engHits or 0
  report(string.format('{"k":"info","s":"the car: %d collision triangles, %d glass/light beams, %d wheels, %d engine block nodes, %d radiator beams, %d flammable nodes%s"}',
    #W.tris, #W.glass, #W.wheels, #W.engine, #W.rad, nf, nf == 0 and ' (no fire nodes: this car cannot burn)' or ''))
  local dmg = 1e9
  pcall(function() dmg = beamstate.damage end)
  if dmg < 1 then report('{"k":"repaired"}') end   -- v2.62: (an undamaged car: no bullet holes on it in GTA)
end
local function segDist2(x, y, z, a, b)
  local ux, uy, uz = b[1] - a[1], b[2] - a[2], b[3] - a[3]
  local l2 = ux * ux + uy * uy + uz * uz
  local t = l2 > 1e-12 and math.max(0, math.min(1, ((x - a[1]) * ux + (y - a[2]) * uy + (z - a[3]) * uz) / l2)) or 0
  local dx, dy, dz = x - a[1] - ux * t, y - a[2] - uy * t, z - a[3] - uz * t
  return dx * dx + dy * dy + dz * dz
end
-- (a bullet goes on through the skin: anything of the list within r of its next len metres)
local function nearRay(list, x, y, z, dx, dy, dz, len, r)
  local b = { x + dx * len, y + dy * len, z + dz * len }
  for i = 1, #list do
    local p = pos(list[i])
    if segDist2(p[1], p[2], p[3], { x, y, z }, b) < r * r then return true end
  end
  return false
end
local function igniteNear(x, y, z, r)
  local bd, bc = r * r, nil
  for c in pairs(fire.flammableNodes or {}) do
    local p = pos(c)
    local d2 = (p[1] - x) ^ 2 + (p[2] - y) ^ 2 + (p[3] - z) ^ 2
    if d2 < bd then bd, bc = d2, c end
  end
  if bc then fire.igniteNode(bc) end
  return bc
end
local function breakGlass(x, y, z, r, chance)
  local set, out = {}, {}
  for _, gb in ipairs(W.glass) do
    if segDist2(x, y, z, pos(gb[1]), pos(gb[2])) < r * r then
      for _, g in ipairs(gb[3]) do if not beamstate.deformGroupsTriggerBeam[g] then set[g] = true end end
    end
  end
  for g in pairs(set) do
    if math.random() < chance then beamstate.triggerDeformGroup(g); out[#out + 1] = js(g) end
  end
  return out
end
-- v2.61: the car's frame (right, forward, up) and a vector in it
local function frame()
  local f, u = obj:getDirectionVector(), obj:getDirectionVectorUp()
  return { f.y * u.z - f.z * u.y, f.z * u.x - f.x * u.z, f.x * u.y - f.y * u.x }, { f.x, f.y, f.z }, { u.x, u.y, u.z }
end
local function toCar(p, R, F, U)
  return { p[1] * R[1] + p[2] * R[2] + p[3] * R[3], p[1] * F[1] + p[2] * F[2] + p[3] * F[3], p[1] * U[1] + p[2] * U[2] + p[3] * U[3] }
end
local function nearWheel(p)
  for _, wh in ipairs(W.wheels) do
    local a, b = pos(wh.a), pos(wh.b)
    if (p[1] - (a[1] + b[1]) * 0.5) ^ 2 + (p[2] - (a[2] + b[2]) * 0.5) ^ 2 + (p[3] - (a[3] + b[3]) * 0.5) ^ 2 < (wh.r + 0.08) ^ 2 then return true end
  end
  return false
end
-- v2.61: a bullet's dent, want metres deep along the bullet. First the way BeamNG restores a saved car's damage: the beams around
-- the hit get the rest lengths of the dented shape (nothing pushes the car). If that hasn't shown after 3 frames: pushes right at
-- the 3 nearest nodes (a planet 3 cm outside each, one frame), twice as hard each time, until it's that deep (W.step)
function W.dentAt(hx, hy, hz, dx, dy, dz, want)
  local rad = 0.22
  local near = {}
  for c, n in pairs(v.data.nodes) do
    local cid = type(n) == 'table' and n.cid or c
    local p = pos(cid)
    local d = sqrt((p[1] - hx) ^ 2 + (p[2] - hy) ^ 2 + (p[3] - hz) ^ 2)
    if d < rad and not nearWheel(p) then near[#near + 1] = { cid, d } end
  end
  if #near == 0 then return end
  table.sort(near, function(a, b) return a[2] < b[2] end)
  local R, F, U = frame()
  local j = { want = want, dir = toCar({ dx, dy, dz }, R, F, U), nodes = {}, ref = {}, w = {}, k = 0, n = 0, a = 3000, how = 'rest lengths', beams = 0 }
  local moved = {}
  local w0 = 1 - near[1][2] / rad
  for i = 1, math.min(8, #near) do
    local c = near[i][1]
    local w = ((1 - near[i][2] / rad) / w0) ^ 1.5
    moved[c] = want * w
    j.nodes[i], j.ref[i], j.w[i] = c, toCar(pos(c), R, F, U), w
  end
  local ok = pcall(function()
    local seen = {}
    for c, m in pairs(moved) do
      for _, e in ipairs(W.nb[c] or {}) do
        local bc, o = e[1], e[2]
        if not seen[bc] then
          seen[bc] = true
          local pa, pb, mb = pos(c), pos(o), moved[o] or 0
          local l0 = sqrt((pa[1] - pb[1]) ^ 2 + (pa[2] - pb[2]) ^ 2 + (pa[3] - pb[3]) ^ 2)
          local l1 = sqrt((pa[1] + dx * m - pb[1] - dx * mb) ^ 2 + (pa[2] + dy * m - pb[2] - dy * mb) ^ 2 + (pa[3] + dz * m - pb[3] - dz * mb) ^ 2)
          local rl = obj:getBeamRestLength(bc)
          if rl and rl > 0.005 and l0 > 0.005 then
            obj:setBeamLength(bc, rl + l1 - l0)
            j.beams = j.beams + 1
          end
        end
      end
    end
  end)
  if not ok or j.beams == 0 then j.how = 'pushes' end
  W.jobs = W.jobs or {}
  W.jobs[#W.jobs + 1] = j
end
-- v2.61: every frame for a second after a hit (GE): the dents measured, pushed in further where needed
function W.step()
  if not W.jobs or #W.jobs == 0 then return end
  P = {}
  local R, F, U = frame()
  local o = obj:getPosition()
  local keep = {}
  for _, j in ipairs(W.jobs) do
    j.k = j.k + 1
    local q = toCar(pos(j.nodes[1]), R, F, U)
    local depth = (q[1] - j.ref[1][1]) * j.dir[1] + (q[2] - j.ref[1][2]) * j.dir[2] + (q[3] - j.ref[1][3]) * j.dir[3]
    local fin = false
    if j.how == 'rest lengths' then
      if j.k >= 3 then
        if depth >= 0.4 * j.want then fin = true else j.how, j.k, j.restDepth = 'pushes', 0, depth end
      end
    elseif j.k % 2 == 1 then
      if depth >= j.want or j.n >= 7 then
        fin = true
      else
        local dw = { j.dir[1] * R[1] + j.dir[2] * F[1] + j.dir[3] * U[1], j.dir[1] * R[2] + j.dir[2] * F[2] + j.dir[3] * U[2], j.dir[1] * R[3] + j.dir[2] * F[3] + j.dir[3] * U[3] }
        for i = 1, math.min(3, #j.nodes) do
          local p, s = pos(j.nodes[i]), 0.03
          beamstate.addPlanet(vec3(o.x + p[1] - dw[1] * s, o.y + p[2] - dw[2] * s, o.z + p[3] - dw[3] * s), 0.01, -(j.a * j.w[i] * s * s) / G, 0.001)
        end
        j.a, j.n = math.min(j.a * 2, 4e5), j.n + 1
      end
    end
    if fin then
      report(string.format('{"k":"dent","s":"a bullet dent %.1f cm deep (wanted %.1f) by %s (%d beams set%s)"}', depth * 100, j.want * 100,
        j.how == 'pushes' and string.format('%d pushes, the last %.0f m/s2', j.n, j.a / 2) or 'rest lengths', j.beams,
        j.restDepth and string.format('; rest lengths alone %.1f cm', j.restDepth * 100) or ''))
    else
      keep[#keep + 1] = j
    end
  end
  W.jobs = keep
end
-- one bullet: from (px, py, pz) (world) along (dx, dy, dz)
function W.hit(px, py, pz, dx, dy, dz, dv, ti, gl, en, ra, ef, wname)
  if not W.tris then W.init() end
  P = {}
  local o = obj:getPosition()
  local ox, oy, oz = px - o.x, py - o.y, pz - o.z
  local best, bt = 8, nil
  for i = 1, #W.tris do
    local t = W.tris[i]
    local A, B, C = pos(t[1]), pos(t[2]), pos(t[3])
    local ax, ay, az = A[1] - ox, A[2] - oy, A[3] - oz
    -- (quick: the ray passes within the triangle's own size of its middle)
    local mx, my, mz = (A[1] + B[1] + C[1]) / 3 - ox, (A[2] + B[2] + C[2]) / 3 - oy, (A[3] + B[3] + C[3]) / 3 - oz
    local r2 = math.max((A[1] - B[1]) ^ 2 + (A[2] - B[2]) ^ 2 + (A[3] - B[3]) ^ 2, (A[1] - C[1]) ^ 2 + (A[2] - C[2]) ^ 2 + (A[3] - C[3]) ^ 2,
      (B[1] - C[1]) ^ 2 + (B[2] - C[2]) ^ 2 + (B[3] - C[3]) ^ 2)
    local ml = mx * dx + my * dy + mz * dz
    if ml > -1 and ml < best + 1 + math.sqrt(r2) then
      local qx, qy, qz = mx - dx * ml, my - dy * ml, mz - dz * ml
      if qx * qx + qy * qy + qz * qz <= r2 then
        local e1x, e1y, e1z = B[1] - A[1], B[2] - A[2], B[3] - A[3]
        local e2x, e2y, e2z = C[1] - A[1], C[2] - A[2], C[3] - A[3]
        local hx, hy, hz = dy * e2z - dz * e2y, dz * e2x - dx * e2z, dx * e2y - dy * e2x
        local a = e1x * hx + e1y * hy + e1z * hz
        if abs(a) > 1e-12 then
          local f = 1 / a
          local sx, sy, sz = -ax, -ay, -az
          local u = f * (sx * hx + sy * hy + sz * hz)
          if u >= 0 and u <= 1 then
            local rx, ry, rz = sy * e1z - sz * e1y, sz * e1x - sx * e1z, sx * e1y - sy * e1x
            local w = f * (dx * rx + dy * ry + dz * rz)
            if w >= 0 and u + w <= 1 then
              local tt = f * (e2x * rx + e2y * ry + e2z * rz)
              if tt > 0 and tt < best then best, bt = tt, i end
            end
          end
        end
      end
    end
  end
  local how, hx, hy, hz = 'on a collision triangle', nil, nil, nil
  if bt then
    hx, hy, hz = ox + dx * best, oy + dy * best, oz + dz * best
  else
    -- (no triangle on its line: the first node it passes within 0.3 m of)
    local bd, bc = 1e9, nil
    for c, n in pairs(v.data.nodes) do
      local cid = type(n) == 'table' and n.cid or c
      local p = pos(cid)
      local qx, qy, qz = p[1] - ox, p[2] - oy, p[3] - oz
      local al = qx * dx + qy * dy + qz * dz
      if al > 0 and al < 8 then
        local d2 = (qx - dx * al) ^ 2 + (qy - dy * al) ^ 2 + (qz - dz * al) ^ 2
        if d2 < 0.09 and al < bd then bd, bc = al, cid end
      end
    end
    if not bc then report('{"k":"hit","how":"missed: nothing of the car on its line"}') return end
    -- (where the line passes it)
    local p = pos(bc)
    local al = (p[1] - ox) * dx + (p[2] - oy) * dy + (p[3] - oz) * dz
    hx, hy, hz, how = ox + dx * al, oy + dy * al, oz + dz * al, 'next to node ' .. bc
  end
  -- the dent: dv cm deep (v2.61; 2.6 pushed at m/s: about 1 mm on your Z06)
  if dv > 0 then W.dentAt(hx, hy, hz, dx, dy, dz, math.min(dv, 10) / 100) end
  local tire = ''
  for _, wh in ipairs(W.wheels) do
    local a, b = pos(wh.a), pos(wh.b)
    local ux, uy, uz = b[1] - a[1], b[2] - a[2], b[3] - a[3]
    local l = sqrt(ux * ux + uy * uy + uz * uz)
    if l > 1e-4 then
      ux, uy, uz = ux / l, uy / l, uz / l
      local cx, cy, cz = hx - (a[1] + b[1]) * 0.5, hy - (a[2] + b[2]) * 0.5, hz - (a[3] + b[3]) * 0.5
      local al = cx * ux + cy * uy + cz * uz
      local rr = sqrt((cx - ux * al) ^ 2 + (cy - uy * al) ^ 2 + (cz - uz * al) ^ 2)
      local ww = wheels.wheels and wheels.wheels[wh.id]
      if abs(al) <= wh.wd * 0.5 + 0.1 and rr <= wh.r + 0.06 and rr >= wh.r * 0.5 and ww and not ww.isTireDeflated and math.random() < ti then
        beamstate.deflateTire(wh.id)
        tire = js(wh.name)
      end
    end
  end
  local glass = gl > 0 and breakGlass(hx, hy, hz, 0.3, gl) or {}
  local eng, rad = 0, 0
  if en > 0 and #W.engine > 0 and nearRay(W.engine, hx, hy, hz, dx, dy, dz, 1.0, 0.4) then
    eng = 1
    W.engHits = (W.engHits or 0) + 1
    for _, dev in ipairs(W.devs) do pcall(function() dev:applyDeformGroupDamage(en, 'main') end) end
    if ef > 0 and W.engHits >= ef and not W.engFire then
      local p = pos(W.engine[1])
      local c = igniteNear(p[1], p[2], p[3], 2.0)
      W.engFire = true
      report(string.format('{"k":"ignite","s":"the engine caught fire after %d engine hits (node %s)"}', W.engHits, tostring(c)))
    end
  end
  local hitRad = false
  for k = 0, 6 do   -- (the radiator's beams within 0.25 m of its next 0.6 m)
    local x, y, z = hx + dx * k * 0.1, hy + dy * k * 0.1, hz + dz * k * 0.1
    for _, rb in ipairs(ra > 0 and W.rad or {}) do
      if not hitRad and segDist2(x, y, z, pos(rb[1]), pos(rb[2])) < 0.0625 then hitRad = true end
    end
  end
  if hitRad then
    rad = 1
    for _, dev in ipairs(W.devs) do pcall(function() dev:applyDeformGroupDamage(ra, 'radiator') end) end
  end
  -- v2.62: where it hit, in the car's frame (right, forward, up from its reference node): GTA draws a bullet hole there
  local R, F, U = frame()
  local hc = toCar({ hx, hy, hz }, R, F, U)
  local hd = toCar({ dx, dy, dz }, R, F, U)  -- v2.7: the bullet's direction (GTA draws the hole where its line meets the panel)
  report(string.format('{"k":"hit","how":"%s","w":"%s","tire":"%s","glass":"%s","en":%d,"ra":%d,"c":[%.3f,%.3f,%.3f],"dd":[%.4f,%.4f,%.4f]}', how, js(wname), tire, table.concat(glass, ' '), eng, rad,
    hc[1], hc[2], hc[3], hd[1], hd[2], hd[3]))
end
-- v2.7: a car that blew up looks like GTA's wrecks (your 2.63 test 1: whole and out of place among GTA's): some doors, the
-- hood, the trunk and the bumpers come off (their break groups), every tyre bursts and the body takes deep dents
function W.wreck()
  local groups, gl = {}, {}
  for _, b in pairs(v.data.beams or {}) do
    if type(b) == 'table' and type(b.cid) == 'number' then
      local g = b.breakGroup
      local list = type(g) == 'table' and g or { g }
      for _, x in ipairs(list) do
        if type(x) == 'string' and x ~= '' then
          if not groups[x] then groups[x] = {}; gl[#gl + 1] = x end
          groups[x][#groups[x] + 1] = b.cid
        end
      end
    end
  end
  table.sort(gl)
  local off, how = {}, 'none worked'
  for _, g in ipairs(gl) do
    local l = g:lower()
    local p = 0
    if l:find('door') then p = 0.5
    elseif l:find('hood') or l:find('bonnet') then p = 0.75
    elseif l:find('trunk') or l:find('tailgate') or l:find('boot') or l:find('hatch') then p = 0.6
    elseif l:find('bumper') then p = 0.6 end
    if p > 0 and math.random() < p then
      if pcall(function() beamstate.breakBreakGroup(g) end) then
        how = 'beamstate.breakBreakGroup'
        off[#off + 1] = js(g)
      else
        local okN = 0
        for _, c in ipairs(groups[g]) do if pcall(function() obj:breakBeam(c) end) then okN = okN + 1 end end
        if okN > 0 then how = 'obj:breakBeam'; off[#off + 1] = js(g) end
      end
    end
  end
  local tyres = 0
  for _, wh in ipairs(W.wheels) do
    if pcall(function() beamstate.deflateTire(wh.id) end) then tyres = tyres + 1 end
  end
  -- deep dents: 10 body nodes, pushed in towards the middle
  local all, cx, cy, cz = {}, 0, 0, 0
  for c, n in pairs(v.data.nodes) do
    local cid = type(n) == 'table' and n.cid or c
    local q = pos(cid)
    cx, cy, cz = cx + q[1], cy + q[2], cz + q[3]
    all[#all + 1] = cid
  end
  local dents = 0
  if #all > 0 then
    cx, cy, cz = cx / #all, cy / #all, cz / #all
    for _ = 1, 10 do
      local q = pos(all[math.random(#all)])
      local dx, dy, dz = cx - q[1], cy - q[2], cz - q[3]
      local l = sqrt(dx * dx + dy * dy + dz * dz)
      if l > 0.3 then
        W.dentAt(q[1], q[2], q[3], dx / l, dy / l, dz / l, 0.08 + math.random() * 0.1)
        dents = dents + 1
      end
    end
  end
  report(string.format('{"k":"info","s":"wreck (v2.7): %d of %d break groups off (%s; by %s), %d tyres burst, %d deep dents"}', #off, #gl, table.concat(off, ' '):sub(1, 300), how, tyres,
    dents))
end
-- one explosion at (px, py, pz) (world)
function W.blast(px, py, pz, accel, r, dt, ig, gr, what, xp)
  if not W.tris then W.init() end
  if xp == 1 and not W.boomed then W.forceBoom = (tostring(what):gsub('[()]', '')) end   -- v2.63: a rocket on the car: it blows up now (W.tick)
  P = {}
  local o = obj:getPosition()
  beamstate.addPlanet(vec3(px, py, pz), r, -accel / G, dt)
  local x, y, z = px - o.x, py - o.y, pz - o.z
  local ign = ''
  if ig > 0 and math.random() < ig then
    local c = igniteNear(x, y, z, 6)
    if c then ign = ', it set the car on fire (node ' .. c .. ')' end
  end
  local broke = gr > 0 and breakGlass(x, y, z, gr, 1.0) or {}
  report(string.format('{"k":"blast","s":"%s %.1f m from the car: pushed with %.0f m/s2 at 1 m for %.2f s%s; glass/lights broken: %s"}', js(what), sqrt(x * x + y * y + z * z),
    accel, dt, ign, #broke > 0 and table.concat(broke, ' ') or 'none'))
end
-- every 0.25 s: broken glass / lights, the fire, the explosion
function W.tick(dt, burnS, mode, cx)
  if not W.tris then W.init() end
  obj:queueGameEngineLua(string.format('if gtaBridge and gtaBridge.wAlive then gtaBridge.wAlive(%d) end', obj:getID()))
  local gs = {}
  for g in pairs(beamstate.deformGroupsTriggerBeam or {}) do gs[#gs + 1] = js(g) end
  table.sort(gs)
  local s = table.concat(gs, ',')
  if s ~= W.dgWas then W.dgWas = s; report('{"k":"dg","g":"' .. s .. '"}') end
  local n, hot, hc, fireball = 0, 0, nil, false
  for c, node in pairs(fire.hotNodes or {}) do
    local i = type(node) == 'table' and tonumber(node.intensity) or 0
    if i > 0.05 then
      n = n + 1
      if i > hot then hot, hc = i, c end
    end
    if i > 10 then fireball = true end
  end
  local o = obj:getPosition()
  if n > 0 and not W.boomed then
    W.burnT = (W.burnT or 0) + dt
    W.fireT = (W.fireT or 0) + dt
    if not W.burning or W.fireT >= 0.5 then
      W.fireT = 0
      local p = obj:getNodePosition(hc)
      report(string.format('{"k":"fire","on":1,"n":%d,"t":%.1f,"p":[%.3f,%.3f,%.3f]}', n, W.burnT, o.x + p.x, o.y + p.y, o.z + p.z))
    end
    W.burning = true
  elseif W.burning then
    W.burning = false
    report('{"k":"fire","on":0}')
  end
  if not W.boomed and (W.forceBoom or fireball or (n > 0 and W.burnT >= burnS)) then
    W.boomed = true
    if mode == 1 and (W.forceBoom or not fireball) then pcall(fire.explodeVehicle) end
    local u = obj:getDirectionVectorUp()
    beamstate.addPlanet(vec3(o.x - u.x * 0.6, o.y - u.y * 0.6, o.z - u.z * 0.6), cx[2], -cx[1] / G, cx[3])
    if cx[4] > 0 and math.random() < cx[4] then pcall(fire.igniteVehicle) end
    P = {}
    if cx[5] > 0 then breakGlass(0, 0, 0, cx[5], 1.0) end
    pcall(W.wreck)
    report(string.format('{"k":"boom","why":"%s","p":[%.3f,%.3f,%.3f]}', W.forceBoom and (js(W.forceBoom) .. ' right at it') or fireball and 'its fuel went up in a fireball' or string.format('it burned %.0f s', W.burnT),
      o.x, o.y, o.z))
    W.forceBoom = nil
  end
  -- repaired (BeamNG's reset): the timers start over
  local dmg = 1e9
  pcall(function() dmg = beamstate.damage end)
  if n == 0 and (W.boomed or (W.burnT or 0) > 0 or W.engFire) then
    if dmg < 50 then W.boomed, W.burnT, W.engHits, W.engFire = false, 0, 0, false end
  end
  -- v2.62: its damage gone (repaired where it is): GTA takes the bullet holes off
  if dmg < 1 and (W.dmgMax or 0) > 50 then report('{"k":"repaired"}') end
  if dmg < 1 then W.dmgMax = 0 elseif dmg < 1e8 then W.dmgMax = math.max(W.dmgMax or 0, dmg) end
end
-- v2.7: a GTA car or person hit BeamNG's car (GTA decides): a dent at the contact (world), pushed along (dx, dy, dz), want m deep
function W.contact(px, py, pz, dx, dy, dz, want)
  if not W.tris then W.init() end
  P = {}
  if want <= 0.003 then return end
  local o = obj:getPosition()
  local ox, oy, oz = px - o.x, py - o.y, pz - o.z
  -- (GTA's contact point is on the drawn car's box: the dent goes on the car's nearest node to it, within 1 m)
  local bd, bp = 1.0, nil
  for c, n in pairs(v.data.nodes) do
    local p = pos(type(n) == 'table' and n.cid or c)
    local dd = sqrt((p[1] - ox) ^ 2 + (p[2] - oy) ^ 2 + (p[3] - oz) ^ 2)
    if dd < bd and not nearWheel(p) then bd, bp = dd, p end
  end
  if bp then W.dentAt(bp[1], bp[2], bp[3], dx, dy, dz, math.min(want, 0.15)) end
end
function W.run(name, ...)
  local ok, e = pcall(W[name], ...)
  if not ok and not W.errs[name] then
    W.errs[name] = true
    report('{"k":"info","s":"error in the car (' .. name .. '): ' .. js(e) .. '"}')
  end
end
]==]

-- GE side
wpn.on, wpn.burnS, wpn.mode, wpn.cx, wpn.tT, wpn.aliveT, wpn.injT, wpn.injN, wpn.burning = false, 30, 0, { 500, 1.5, 0.06, 1, 10 }, 0, 0, 0, 0, false
wpn.hits, wpn.blasts, wpn.logs = 0, 0, 0
function wpn.start(d, m)
  wpn.on = tonumber(m.wp) == 1
  if not wpn.on then logI('weapons: off (GTA\'s ini: Weapons=0, or a GTA plugin before 2.6)'); return end
  wpn.burnS = math.max(1, tonumber(m.wb) or 30)
  wpn.mode = tonumber(m.wm) == 1 and 1 or 0
  if type(m.wx) == 'table' and #m.wx >= 5 then
    for i = 1, 5 do wpn.cx[i] = tonumber(m.wx[i]) or wpn.cx[i] end
  end
  wpn.veh, wpn.id, wpn.ip, wpn.port = d.veh, d.id, d.ip, d.port
  wpn.inj, wpn.injN = false, 0
  logI(string.format('weapons: on: the car burns %.0f s before it blows up (%s); its own blast %.0f m/s2 at 1 m', wpn.burnS,
    wpn.mode == 1 and 'BeamNG\'s explodeVehicle + GTA\'s explosion' or 'GTA\'s explosion', wpn.cx[1]))
end
-- every frame: the car's Lua in, its tick, the push a blast gave it
function wpn.tick()
  if not wpn.on or not wpn.veh then return end
  local d = drive
  if not d and not wpn.burning then return end   -- (after the drive only a car still burning)
  local t = now()
  local veh = wpn.veh
  if not wpn.inj or (t - wpn.aliveT > 3 and t - wpn.injT > 3 and not pz.paused) then
    if wpn.injN > 10 then return end
    if wpn.inj then logI('weapons: nothing from the car for 3 s: its Lua code goes in again') end
    veh:queueLuaCommand(wpn.VEH .. '\ngtabWpn.tris = nil')
    wpn.inj, wpn.injT, wpn.injN, wpn.aliveT = true, t, wpn.injN + 1, t
  end
  if wpn.stepUntil and t < wpn.stepUntil then veh:queueLuaCommand('if gtabWpn then gtabWpn.run("step") end') end   -- v2.61
  if t - wpn.tT >= 0.25 then
    local dt = math.min(1, t - wpn.tT)
    wpn.tT = t
    local c = wpn.cx
    veh:queueLuaCommand(string.format('if gtabWpn then gtabWpn.run("tick", %.3f, %.1f, %d, { %.1f, %.2f, %.3f, %.2f, %.1f }) end', dt, wpn.burnS, wpn.mode, c[1], c[2], c[3], c[4], c[5]))
  end
  local w = wpn.watch
  if w and d and type(d.vel) == 'table' then
    local dv = math.sqrt((d.vel[1] - w.v0[1]) ^ 2 + (d.vel[2] - w.v0[2]) ^ 2 + (d.vel[3] - w.v0[3]) ^ 2)
    if dv > w.max then w.max = dv end
    if t > w.t1 then
      wpn.watch = nil
      local s = string.format('%s: the car\'s speed changed by %.1f m/s within 0.5 s', w.what, w.max)
      logI('weapons: ' .. s)
      pcall(function() udp:sendto('{"t":"fx","k":"dv","s":"' .. s:gsub('"', "'") .. '"}', wpn.ip, wpn.port) end)
    end
  end
end
function M.wAlive(id) if id == wpn.id then wpn.aliveT = now() end end
-- the car's reports -> GTA
function M.wfx(id, s)
  if id ~= wpn.id or not wpn.ip then return end
  s = tostring(s)
  local k = s:match('"k":"(%a+)"') or '?'
  if k == 'fire' then
    local on = s:find('"on":1', 1, true) ~= nil
    if on ~= wpn.burning then logI('weapons: the car ' .. (on and 'is on fire' or 'stopped burning')) end
    wpn.burning = on
  elseif k == 'boom' then
    wpn.stepUntil = now() + 1.5   -- v2.7: the wreck's deep dents are pushed in frame by frame
    wpn.burning = false
    logI('weapons: the car blew up: ' .. s)
  elseif wpn.logs < 120 then
    wpn.logs = wpn.logs + 1
    logI('weapons: ' .. s)
  end
  pcall(function() udp:sendto('{"t":"fx",' .. s:sub(2), wpn.ip, wpn.port) end)
end
handlers.hit = function(m)
  local d = drive
  if not (wpn.on and d and type(m.p) == 'table' and type(m.d) == 'table') then return end
  wpn.hits = wpn.hits + 1
  wpn.stepUntil = now() + 1.2   -- v2.61: the dent is measured / pushed in frame by frame
  local n = function(x) return tonumber(x) or 0 end
  d.veh:queueLuaCommand(string.format('if gtabWpn then gtabWpn.run("hit", %.3f, %.3f, %.3f, %.4f, %.4f, %.4f, %.2f, %.2f, %.2f, %.4f, %.4f, %.0f, %q) end',
    n(m.p[1]), n(m.p[2]), n(m.p[3]), n(m.d[1]), n(m.d[2]), n(m.d[3]), n(m.dv), n(m.ti), n(m.gl), n(m.en), n(m.ra), n(m.ef), tostring(m.w or '?')))
end
-- v2.7: traffic: GTA says one of its cars or people hit BeamNG's car ("tc"): the car's speed changes by dv (BeamNG world, m/s; no
-- damage, like "bump") and, with weapons on, it takes a dent at the contact
world.props.tcN, world.props.tcErr = 0, nil
handlers.tc = function(m)
  local d = drive
  if not d or type(m.dv) ~= 'table' then return end
  local veh = getPlayerVeh()
  if not veh then return end
  local n = function(x) return tonumber(x) or 0 end
  local x, y, z = n(m.dv[1]), n(m.dv[2]), n(m.dv[3])
  local l = math.sqrt(x * x + y * y + z * z)
  if l > 15 then x, y, z = x * 15 / l, y * 15 / l, z * 15 / l end
  local ok, err = pcall(function() veh:applyClusterVelocityScaleAdd(veh:getRefNodeId(), 1, x, y, z) end)
  if not ok and not world.props.tcErr then world.props.tcErr = tostring(err); logE('traffic: could not push the car: ' .. tostring(err)) end
  if wpn.on and type(m.p) == 'table' and type(m.d) == 'table' and n(m.dent) > 0 then
    wpn.stepUntil = now() + 1.2
    veh:queueLuaCommand(string.format('if gtabWpn then gtabWpn.run("contact", %.3f, %.3f, %.3f, %.4f, %.4f, %.4f, %.3f) end', n(m.p[1]), n(m.p[2]), n(m.p[3]), n(m.d[1]), n(m.d[2]),
      n(m.d[3]), n(m.dent)))
  end
  world.props.tcN = world.props.tcN + 1
  if world.props.tcN <= 30 then logI(string.format('traffic: %s hit the car: its speed changed by %.1f km/h, dent %.0f mm', tostring(m.w or '?'), l * 3.6, n(m.dent) * 1000)) end
end
handlers.blast = function(m)
  local d = drive
  if not (wpn.on and d and type(m.p) == 'table') then return end
  wpn.blasts = wpn.blasts + 1
  local n = function(x) return tonumber(x) or 0 end
  local what = 'an explosion (GTA type ' .. tostring(m.ty) .. ')'
  d.veh:queueLuaCommand(string.format('if gtabWpn then gtabWpn.run("blast", %.3f, %.3f, %.3f, %.1f, %.2f, %.3f, %.2f, %.1f, %q, %d) end', n(m.p[1]), n(m.p[2]), n(m.p[3]), n(m.a), n(m.r),
    n(m.dt), n(m.ig), n(m.gr), what, n(m.xp)))
  if type(d.vel) == 'table' then wpn.watch = { t1 = now() + 0.5, v0 = { d.vel[1], d.vel[2], d.vel[3] }, max = 0, what = what } end
end
-- export: each flexbody mesh's deformGroup and broken material (GTA hides / frosts it when BeamNG breaks the group)
function wpn.exportDg(vdata)
  local dg, n = {}, 0
  for _, fb in pairs(vdata.flexbodies or {}) do
    if type(fb) == 'table' and type(fb.mesh) == 'string' and type(fb.deformGroup) == 'string' and not dg[fb.mesh] then
      dg[fb.mesh] = { fb.deformGroup, tostring(fb.deformMaterialDamaged or '') }
      n = n + 1
    end
  end
  for _, b in pairs(vdata.beams or {}) do
    if type(b) == 'table' and type(b.deformSwitches) == 'table' then
      for _, g in pairs(b.deformSwitches) do
        if type(g) == 'table' and type(g.mesh) == 'string' and g.deformGroup and not dg[g.mesh] then
          dg[g.mesh] = { tostring(g.deformGroup), tostring(g.dmgMat or '') }
          n = n + 1
        end
      end
    end
  end
  return n > 0 and dg or nil, n
end

-- Do several sub-meshes of one ProceduralMesh all get collision? Checked once with two small quads far below
-- BeamNG's ground (one per sub-mesh, different ground models). If only the first is solid, tiles are built as one
-- object per surface ("split").
local function checkSubmeshes()
  if world.checked then return end
  world.checked = true
  if type(rawget(_G, 'castRayStatic')) ~= 'function' then
    world.checkNote = 'not checked (no castRayStatic): one object per tile'
    logI('tiles: ' .. world.checkNote)
    return
  end
  local base = { 0, 0, -400 }
  local v = getPlayerVeh()
  if v then local p = v:getPosition(); base = { p.x + 40, p.y + 40, -400 } end
  local verts = { { x = 0, y = 0, z = 0 }, { x = 2, y = 0, z = 0 }, { x = 2, y = 2, z = 0 }, { x = 0, y = 2, z = 0 },
                  { x = 3, y = 0, z = 0 }, { x = 5, y = 0, z = 0 }, { x = 5, y = 2, z = 0 }, { x = 3, y = 2, z = 0 } }
  local uvs = {}
  for i = 1, 8 do uvs[i] = { u = verts[i].x, v = verts[i].y } end
  local function quadFaces(a)
    local f = {}
    for _, t in ipairs({ { a, a + 1, a + 2 }, { a, a + 2, a + 3 } }) do
      local p, q, r = t[1], t[2], t[3]
      if cfg.flipWinding then q, r = r, q end
      f[#f + 1] = { v = p, n = 0, u = p }; f[#f + 1] = { v = q, n = 0, u = q }; f[#f + 1] = { v = r, n = 0, u = r }
    end
    return f
  end
  local up = { { x = 0, y = 0, z = 1 } }
  local ok, err = pcall(function()
    local obj = makeProcMesh('gtab_submesh_check', base, {
      { verts = verts, uvs = uvs, normals = up, faces = quadFaces(0), material = surfMaterial(resolveGround('ASPHALT')) },
      { verts = verts, uvs = uvs, normals = up, faces = quadFaces(4), material = surfMaterial(resolveGround('DIRT|GRAVEL|GRASS')) } })
    reloadCollisionNow()
    local function hit(x)
      local okR, d = pcall(castRayStatic, vec3(base[1] + x, base[2] + 1, base[3] + 1), vec3(0, 0, -1), 5)
      return okR and type(d) == 'number' and d < 4.9
    end
    local a, b = hit(1), hit(4)
    pcall(function() obj:delete() end)
    collisionDirty = true
    if a and b then
      world.mode, world.checkNote = 'submesh', 'both sub-meshes solid: one object per tile'
    elseif a then
      world.mode, world.checkNote = 'split', 'only the first sub-mesh is solid: one object per surface'
    else
      world.checkNote = 'castRayStatic saw neither test quad: one object per tile (UNVERIFIED)'
    end
  end)
  if not ok then world.checkNote = 'check failed (' .. tostring(err) .. '): one object per tile' end
  logI('tiles: sub-mesh check: ' .. world.checkNote)
end

-- ---- the car onto GTA's ground ("tp"). The rotation convention BeamNG's teleport wants is not documented: it is
-- found by reading the car back (forward/up) after each try, and remembered for the session.
local function rotCandidates(f, u)
  local list = {}
  local function qd(dx, dy, dz)
    local ok, q = pcall(quatFromDir, vec3(dx, dy, dz), vec3(u[1], u[2], u[3]))
    if ok and type(q) ~= 'nil' then return { x = q.x, y = q.y, z = q.z, w = q.w } end
    return nil
  end
  local a, b = qd(f[1], f[2], f[3]), qd(-f[1], -f[2], -f[3])
  list[1] = a and { 'quatFromDir(forward)', a } or false
  list[2] = b and { 'quatFromDir(-forward)', b } or false
  list[3] = a and { 'inverse of quatFromDir(forward)', { x = -a.x, y = -a.y, z = -a.z, w = a.w } } or false
  list[4] = b and { 'inverse of quatFromDir(-forward)', { x = -b.x, y = -b.y, z = -b.z, w = b.w } } or false
  return list
end
-- v2.41: a teleport that keeps the car as it is (its damage). setPositionRotation (v2.4) resets the car: it repairs it.
-- Newer BeamNG versions give spawn.safeTeleport a "reset the vehicle" switch (BeamNG's F7 "drop at camera" teleports the
-- car as it is): its parameter list is read once from your BeamNG's own lua/ge/spawn.lua. If that switch isn't there,
-- the car doesn't face the right way with it, or keeps falling speed, setPositionRotation is used (it repairs).
local stReset   -- nil: not looked yet; false: no switch; else the switch's position in the parameter list
local function safeTeleportResetArg()
  if stReset ~= nil then return stReset end
  stReset = false
  local params
  pcall(function()
    local src = readFile('lua/ge/spawn.lua') or readFile('/lua/ge/spawn.lua')
    if type(src) == 'string' then
      params = src:match('function%s+safeTeleport%s*%(([^)]*)%)') or src:match('safeTeleport%s*=%s*function%s*%(([^)]*)%)')
    end
  end)
  world.stParams = params or '?'
  if params then
    local i = 0
    for name in params:gmatch('[^,%s]+') do
      i = i + 1
      if name:lower():find('reset') then stReset = i; world.stResetName = name; break end
    end
  end
  logI('teleport: spawn.safeTeleport(' .. world.stParams .. ')' .. (stReset and (': ' .. world.stResetName .. ' = false teleports the car as it is (no repair)')
    or ': no reset switch found: setPositionRotation is used (it repairs the car)'))
  return stReset
end
local KEEP = 'safeTeleport (no repair)'
-- (these helpers are fields of world: Lua's 200-locals limit)
world.wantKeep = function()
  return modConfig and modConfig.tpKeepDamage ~= false and not world.keepBad and type(rawget(_G, 'spawn')) == 'table'
    and type(spawn.safeTeleport) == 'function' and safeTeleportResetArg() and true or false
end
local function teleportVeh(veh, p, q, keep)
  local errs = {}
  if keep then
    local args = { veh, vec3(p[1], p[2], p[3]), quat(q.x, q.y, q.z, q.w) }
    args[stReset] = false
    local ok, err = pcall(function() spawn.safeTeleport((table.unpack or unpack)(args, 1, stReset)) end)
    if ok then return KEEP end
    errs[#errs + 1] = 'spawn.safeTeleport(no repair): ' .. tostring(err)
    world.keepBad = 'it failed: ' .. tostring(err)
  end
  if world.api ~= 'spawn.safeTeleport' then
    local ok, err = pcall(function() veh:setPositionRotation(p[1], p[2], p[3], q.x, q.y, q.z, q.w) end)
    if ok then return 'setPositionRotation' end
    errs[#errs + 1] = 'setPositionRotation: ' .. tostring(err)
  end
  local ok, err = pcall(function() spawn.safeTeleport(veh, vec3(p[1], p[2], p[3]), quat(q.x, q.y, q.z, q.w), true) end)
  if ok then return 'spawn.safeTeleport' end
  errs[#errs + 1] = 'spawn.safeTeleport: ' .. tostring(err)
  return nil, table.concat(errs, '; ')
end
world.tileUnder = function(x, y)
  for id, t in pairs(worldTiles) do
    if x >= t.x0 and x <= t.x1 and y >= t.y0 and y <= t.y1 then return id, t end
  end
  return nil
end
-- v2.72: the teleport's pad over a hole (12 m square, 4 cm under the spot); it goes once the car is 20 m from it
world.tpPad = function(X, f)
  local l = math.sqrt(f[1] * f[1] + f[2] * f[2])
  local fx, fy = l > 0.1 and f[1] / l or 0, l > 0.1 and f[2] / l or 1
  local rx, ry, h, z = fy, -fx, 6, X[3] - 0.04
  local v = {}
  for _, c in ipairs({ { -1, -1 }, { 1, -1 }, { 1, 1 }, { -1, 1 } }) do
    v[#v + 1] = { X[1] + (rx * c[1] + fx * c[2]) * h, X[2] + (ry * c[1] + fy * c[2]) * h, z }
  end
  local ok, res = buildSurface('gtab_tppad', v, { 0, 1, 2, 0, 2, 3 }, surfMaterial(resolveGround('ASPHALT')))
  if ok then world.tppad = { X[1], X[2] } end
  logI(string.format('ground: no GTA tile under the spot at %.1f %.1f yet: a 12 m pad at %.2f under it (%s)', X[1], X[2], z, ok and 'built' or ('FAILED ' .. tostring(res))))
  return ok and true or false
end
world.tpPadTick = function()
  local pp = world.tppad
  if not pp or world.tp then return end
  local veh = getPlayerVeh()
  local p = veh and veh:getPosition()
  if p and (p.x - pp[1]) ^ 2 + (p.y - pp[2]) ^ 2 < 400 then return end
  removeSurface('gtab_tppad')
  world.tppad = nil
  logI('ground: the teleport\'s pad is gone (the car left it)')
end
world.removePad = function()
  if world.padOn then
    removeSurface('gtab_pad')
    world.padOn = false
    logI('ground: the parking pad is gone (the car is on GTA\'s ground again)')
  end
end
local function tpFinish(ok, msg)
  local tp = world.tp
  world.tp = nil
  if not ok then
    logE('teleport failed: ' .. tostring(msg))
    send({ t = 'tp_err', err = tostring(msg) })
    return
  end
  send(msg)
  -- after a fresh start (new origin or a new GTA session) the tiles of the earlier session go now: the car is on new ones
  if world.fresh then
    local n = 0
    for id, t in pairs(worldTiles) do
      if (t.epoch or 0) < world.epoch then
        local p = pendingBuild[id]
        if p then p.cancelled = true end
        enqueue({ kind = 'remove', id = id, t = now(), quiet = true })
        n = n + 1
      end
    end
    if n > 0 then logI('ground: ' .. n .. ' tiles from the earlier session removed') end
    pcall(world.props.clear, 'the earlier session\'s')   -- v2.5
    pcall(world.phys.clear, 'the earlier session\'s')
    world.fresh = false
  end
  world.removePad()
  tp.done = now()
  world.tpCheck = { t = now() + 0.6, dmg = tp.dmgBefore, why = tp.m.why }
  pcall(dents.afterTeleport, msg)   -- v2.5: repaired: a new dents reference; else the first one waits until the car settled
end
local function tpUpdate()
  local tp = world.tp
  world.tpPadTick()   -- v2.72
  if not tp then return end
  local t = now()
  if t - tp.t0 > 10 then tpFinish(false, 'not done in 10 s (state ' .. tp.state .. ', ' .. (tp.note or '') .. ')'); return end
  local veh = getPlayerVeh()
  if not veh then tpFinish(false, 'no player car in BeamNG'); return end
  local m = tp.m
  local f = norm({ m.f[1], m.f[2], m.f[3] })
  local u0 = { m.u[1], m.u[2], m.u[3] }
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local o = type(m.o) == 'table' and m.o or { 0, 0, 0 }
  local lift = tonumber(m.lift) or 0.2
  local X = m.p
  local P, Pb = {}, {}   -- where the teleport is aimed (with the learned correction), where the car must read back
  for k = 1, 3 do
    Pb[k] = X[k] - (r[k] * o[1] + f[k] * o[2] + u[k] * o[3]) + u[k] * lift
    P[k] = Pb[k] + (tp.corr and tp.corr[k] or 0)
  end
  if tp.state == 'wait' then
    -- the tiles under the target first (built, collision reloaded); v2.43: a repair is where the car already is
    local under = m.repair or world.tileUnder(X[1], X[2])
    -- v2.72: still no GTA tile under the spot after 1.5 s (a hole GTA's rays couldn't fill): a 12 m pad there, so the car
    -- doesn't land on nothing and fall again (the v2.71 F10 loop: start, fall, put back on the same hole)
    if not under and t - tp.t0 > 1.5 and not tp.pad then
      tp.pad = world.tpPad(X, f)
      under = tp.pad
    end
    if tp.pad then under = true end
    if not m.repair and (queueLen() > 0 or collisionDirty or not under) then
      tp.note = (not under) and 'no tile under the target yet' or 'tiles still building'
      if not (t - tp.t0 > 5 and queueLen() == 0 and not collisionDirty) then return end
      tp.note = 'no tile under the target after 5 s'
      logE('teleport: no GTA ground tile under the target after 5 s: teleporting anyway')
    end
    tp.cands = rotCandidates(f, u)
    tp.ci = world.conv or 1
    tp.keep = not m.repair and world.wantKeep()   -- v2.43: a repair is BeamNG's repairing reset
    tp.state = 'go'
    world.corrBy = world.corrBy or {}
    local c = world.corrBy[tp.keep and KEEP or 'reset']
    if c then
      tp.corr = { r[1] * c[1] + f[1] * c[2] + u[1] * c[3], r[2] * c[1] + f[2] * c[2] + u[2] * c[3], 0 }
      for k = 1, 3 do P[k] = P[k] + tp.corr[k] end
    end
    pcall(function() tp.dmgBefore = drive and drive.vs and drive.vs.dmg end)
  end
  if tp.state == 'go' then
    while tp.ci <= 4 and not tp.cands[tp.ci] do tp.ci = tp.ci + 1 end
    if tp.ci > 4 then tpFinish(false, 'no rotation made BeamNG\'s car face the GTA car\'s way (' .. (tp.note or '') .. ')'); return end
    local api, err = teleportVeh(veh, P, tp.cands[tp.ci][2], tp.keep)
    if not api then tpFinish(false, 'no teleport function worked: ' .. tostring(err)); return end
    if tp.keep and api ~= KEEP then tp.keep = false end
    world.api = api
    tp.frames = 0
    tp.tries = (tp.tries or 0) + 1
    tp.P = Pb
    tp.state = 'verify'
    return
  end
  if tp.state == 'verify' then
    tp.frames = tp.frames + 1
    if tp.frames < 3 then return end
    local pb = v3(veh:getPosition())
    local fb = norm(v3(veh:getDirectionVector()))
    local ub = norm(v3(veh:getDirectionVectorUp()))
    local df, dup = dot(fb, f), dot(ub, u)
    local eh = math.sqrt((pb[1] - tp.P[1]) ^ 2 + (pb[2] - tp.P[2]) ^ 2)
    local ev = pb[3] - tp.P[3]
    local name = tp.cands[tp.ci][1]
    -- v2.41: the no-repair teleport keeps the car's speed (falling after a fall-through): too fast = the repairing one
    local vel0 = { 0, 0, 0 }
    pcall(function() vel0 = v3(veh:getVelocity()) end)
    local sp0 = math.sqrt(dot(vel0, vel0))
    if df > 0.995 and dup > 0.99 and tp.keep and sp0 > 1.5 then
      world.keepBad = string.format('the car kept %.1f m/s after it', sp0)
      logI('teleport: ' .. KEEP .. ': ' .. world.keepBad .. ': setPositionRotation from now on (it repairs the car)')
      tp.keep = false
      tp.state = 'go'
      return
    end
    if df > 0.995 and dup > 0.99 then
      if eh > 0.15 and (tp.fixes or 0) < 2 then
        -- right way round but not at the spot (the teleport may place another point than getPosition reads): correct
        tp.fixes = (tp.fixes or 0) + 1
        tp.corr = tp.corr or { 0, 0, 0 }
        tp.corr[1] = tp.corr[1] + (tp.P[1] - pb[1])
        tp.corr[2] = tp.corr[2] + (tp.P[2] - pb[2])
        tp.note = string.format('%.2f m off: corrected', eh)
        logI(string.format('teleport: %s faced right but landed %.2f m off: trying again corrected', name, eh))
        tp.state = 'go'
        return
      end
      if world.conv ~= tp.ci then logI('teleport: the rotation convention that works: ' .. name .. ' (' .. tostring(world.api) .. ')') end
      world.conv = tp.ci
      if tp.corr then
        -- the teleport places another point than getPosition reads: keep the difference (car frame) for next time
        world.corrBy = world.corrBy or {}
        world.corrBy[tp.keep and KEEP or 'reset'] = { dot(tp.corr, r), dot(tp.corr, f), dot(tp.corr, u) }
      end
      local vel = { 0, 0, 0 }
      pcall(function() vel = v3(veh:getVelocity()) end)
      local d = drive
      local msg = { t = 'tp_ok', st = d and d.st or 0, ss = d and d.ss or 0, conv = name, api = world.api, eh = eh, ev = ev, df = df,
                    v = math.sqrt(dot(vel, vel)), tries = tp.tries, why = m.why, keep = tp.keep and 1 or 0 }
      logI(string.format('teleport (%s): car put at %.2f %.2f %.2f facing %.2f %.2f %.2f with %s via %s in %d tries; %.0f cm off sideways, %.0f cm %s; speed after %.2f m/s',
        tostring(m.why), P[1], P[2], P[3], f[1], f[2], f[3], name, tostring(world.api), tp.tries, eh * 100, math.abs(ev) * 100, ev >= 0 and 'high' or 'low', msg.v))
      tpFinish(true, msg)
      return
    end
    logI(string.format('teleport: %s: the car faces %.2f %.2f %.2f, up %.2f %.2f %.2f (wanted %.2f %.2f %.2f): trying the next convention',
      name, fb[1], fb[2], fb[3], ub[1], ub[2], ub[3], f[1], f[2], f[3]))
    if tp.keep and (world.conv == tp.ci or tp.ci >= 4) then
      -- v2.41: the no-repair teleport doesn't turn the car (BeamNG: a new rotation needs a full reset): the repairing one
      world.keepBad = 'it did not turn the car'
      logI('teleport: ' .. KEEP .. ' ' .. world.keepBad .. ': setPositionRotation from now on (it repairs the car)')
      tp.keep = false
      tp.ci = world.conv or 1
      tp.state = 'go'
      return
    end
    if world.conv == tp.ci then world.conv = nil end
    tp.ci = tp.ci + 1
    if tp.ci > 4 and not tp.wrapped and tp.cands[1] then tp.ci, tp.wrapped = 1, true end
    tp.state = 'go'
  end
end

-- the drive ended: a flat 600 m pad under the car keeps it from rolling off the last GTA tiles and falling 200 m
-- (removed when the next drive puts the car on GTA's ground again)
worldDriveEnded = function(d)
  pcall(world.props.clear, 'the drive ended')   -- v2.5 (GTA takes them out itself, unless it closed)
  pcall(world.phys.clear, 'the drive ended')
  pcall(world.tw.parkAll, 'the drive ended')   -- v2.72: parked, kept for the next F10 (spawning one freezes BeamNG 2-3 s)
  if not world.ox then return end
  local veh = getPlayerVeh()
  if not veh then return end
  local p = veh:getPosition()
  local gz = groundBelow(p.x, p.y, p.z) or (p.z - 0.5)
  local h, z = 300, gz - 0.03
  local v = { { p.x - h, p.y - h, z }, { p.x + h, p.y - h, z }, { p.x + h, p.y + h, z }, { p.x - h, p.y + h, z } }
  local ok, res = buildSurface('gtab_pad', v, { 0, 1, 2, 0, 2, 3 }, surfMaterial(resolveGround('ASPHALT')))
  world.padOn = ok and true or false
  logI(string.format('ground: drive over: a flat pad at %.2f under the car (%s) keeps it from falling off the last GTA tiles', z, ok and 'built' or ('FAILED ' .. tostring(res))))
end

-- every frame: the teleport, stats for GTA ("ws", once a second while GTA's ground is in use)
worldUpdate = function(frameMs)
  if not world.ox then return end
  local v = getPlayerVeh()
  if v then local p = v:getPosition(); world.carP = { p.x, p.y, p.z } end
  local okT, errT = pcall(tpUpdate)
  if not okT then
    logE('teleport: ' .. tostring(errT))
    world.tp = nil
    send({ t = 'tp_err', err = tostring(errT) })
  end
  if world.tpCheck and now() > world.tpCheck.t then
    local c = world.tpCheck
    world.tpCheck = nil
    local after = drive and drive.vs and drive.vs.dmg
    logI(string.format('teleport (%s): BeamNG\'s damage count before %s, 0.6 s after %s%s', tostring(c.why), tostring(c.dmg), tostring(after),
      (tonumber(c.dmg) and tonumber(after) and tonumber(after) < tonumber(c.dmg) - 1) and ' (the teleport REPAIRED the car)' or ''))
  end
  if not (drive or world.tp) then return end
  world.frameCosts[#world.frameCosts + 1] = frameMs or 0
  local t = now()
  if t - world.wsT < 1 then return end
  local fc = world.frameCosts
  table.sort(fc)
  local p95 = #fc > 0 and fc[math.max(1, math.floor(#fc * 0.95))] or 0
  local fmx = #fc > 0 and fc[#fc] or 0
  local nt, live = 0, {}
  for _, wt in pairs(worldTiles) do
    nt = nt + 1
    for k, c in pairs(wt.gmt or {}) do live[k] = (live[k] or 0) + c end
  end
  local gm = {}
  for k, c in pairs(live) do gm[#gm + 1] = k .. '=' .. math.floor(c) end   -- triangles of the live tiles per ground model
  table.sort(gm)
  local secs = world.wsT > 0 and (t - world.wsT) or 1
  local ws = { t = 'ws', n = nt, b = world.built, bms = world.built > 0 and world.buildMsSum / world.built or 0, bmx = world.buildMsMax,
               rn = stats.wReloads or 0, rms = (stats.wReloads or 0) > 0 and stats.wReloadSum / stats.wReloads or 0, rmx = stats.wReloadMax or 0,
               fp95 = p95, fmx = fmx, q = queueLen(), mode = world.mode, tris = liveTris(), gm = table.concat(gm, ' '), secs = secs }
  send(ws)
  if t - world.logT >= 10 then
    world.logT = t
    logI(string.format('ground: %d tiles live (%d triangles), %d built in the last second (%.1f ms average, %.1f max); collision reloads %d (%.1f ms average, %.1f max); bridge cost per frame p95 %.2f ms, max %.2f; queue %d; triangles per ground model: %s',
      nt, ws.tris, ws.b, ws.bms, ws.bmx, ws.rn, ws.rms, ws.rmx, p95, fmx, ws.q, ws.gm))
    -- v2.51: the ride, flat cells built finely against steps, props
    local F = world.feel
    logI(string.format('ground feel: %d jolts (vertical over 1.5 g) in %.2f km above 50 km/h (%.1f per km); %d flat cells drawn finely (their edges had steps)',
      F.n, F.km, F.km > 0.05 and F.n / F.km or 0, world.splitCells or 0))
    if F.at and #F.at > 0 then
      local w = {}
      for _, j in ipairs(F.at) do w[#w + 1] = string.format('%.1f g at GTA %.0f %.0f (%.0f km/h)', j[1], j[2], j[3], j[4]) end
      logI('ground feel: the hardest jolts: ' .. table.concat(w, '; '))
    end
    F.n, F.km, world.splitCells, F.at = 0, 0, 0, nil
    local P, nb = world.props, 0
    for _ in pairs(P.boxes) do nb = nb + 1 end
    if nb > 0 or P.breaks > 0 or P.waits > 0 then
      logI(string.format('props: %d prop boxes here, %d broken by the car, %d waited until the car was clear of them (10 s)', nb, P.breaks, P.waits))
      P.breaks, P.waits = 0, 0
    end
  end
  world.wsT = t
  world.built, world.buildMsSum, world.buildMsMax = 0, 0, 0
  stats.wReloads, stats.wReloadSum, stats.wReloadMax = 0, 0, 0
  world.frameCosts = {}
end

-- when to reload collision: right away while it is cheap, when a tile near the car or the teleport needs it; else at
-- most 10 times a second (each reload rebuilds BeamNG's whole static collision)
reloadDue = function()
  if not world.ox then return true end
  if world.urgent or world.tp then return true end
  local last = stats.lastReloadMs or 0
  if last < 2 then return true end   -- v2.41: 2 ms (was 4: with 50 tiles every reload was "cheap")
  -- v2.43: the static world makes a reload slower (it rebuilds all tiles' collision): the ones nothing near the car needs
  -- wait longer, so reloads take at most ~8% of BeamNG's time
  return now() - (stats.lastReloadT or 0) > math.max(0.1, last * 0.012)
end

handlers.map = function(m)
  local ox, oy = tonumber(m.ox), tonumber(m.oy)
  if not (ox and oy) then logE('map: no origin'); return end
  loadGroundModels()
  local changed = world.ox == nil or math.abs(ox - world.ox) > 0.01 or math.abs(oy - world.oy) > 0.01
  if changed or m.fresh == 1 or m.fresh == true then
    world.epoch = world.epoch + 1
    world.fresh = true
  end
  world.ox, world.oy, world.zl, world.tileM = ox, oy, tonumber(m.zl) or 200, tonumber(m.tile) or 8
  checkSubmeshes()
  local nt = 0
  for _ in pairs(worldTiles) do nt = nt + 1 end
  send({ t = 'map_ok', epoch = world.epoch, n = nt, mode = world.mode, check = world.checkNote, v = M.version, gml = #world.gmList > 0 and world.gmList:sub(1, 600) or '?' })
  logI(string.format('ground: GTA -> BeamNG = GTA - (%.0f, %.0f) and %.0f m up; %s; %d tiles kept (%s); tile mode %s', ox, oy, world.zl,
    world.fresh and 'fresh start (old tiles go once the car is on new ones)' or 'same origin', nt, world.checkNote, world.mode))
end

handlers.rm = function(m)
  for _, id in ipairs(type(m.ids) == 'table' and m.ids or {}) do
    id = tostring(id)
    local p = pendingBuild[id]
    if p then p.cancelled = true end
    enqueue({ kind = 'remove', id = id, t = now(), quiet = true })
  end
end

-- ---- v2.71: traffic twins (your choice: "BeamNG twins, GTA look"). When the drive starts GTA asks for a pool of BeamNG
-- cars ("twp": the models from GtaBeamD1_traffic.json "twinPool"). They are spawned one at a time high above the car,
-- measured (their size, the middle of their footprint, their body's nodes at rest) and parked (setActive(0): not
-- drawn, not simulated). When GTA sees one of its cars about to meet yours it asks for a twin ("twa"): the twin is put
-- exactly where GTA's car is, with its speed, and from then BeamNG moves it: the crash is BeamNG's, soft body against
-- soft body. Its pose goes back every frame ("twu"; GTA's car copies it) and its dents ("twd"; GTA's car gets GTA
-- damage there). GTA says when it's done ("twr"): parked again. The pool goes when the drive ends.
-- UNVERIFIED in BeamNG 0.39: setActive, the vanilla model names (the log says what spawned and how)
world.tw = { pool = {}, spawnT = 0, logT = 0, poses = 0, acts = 0, fails = 0 }
world.tw.say = function(t)
  local d = drive
  if d then pcall(function() udp:sendto(jsonEncode(t), d.ip, d.port) end) end
end
world.tw.frame = function(veh)
  local f = norm(v3(veh:getDirectionVector()))
  local u0 = v3(veh:getDirectionVectorUp())
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  return cross(f, u), f, u
end
world.tw.setActive = function(e, on)
  local ok, err = pcall(function() e.veh:setActive(on and 1 or 0) end)
  if not ok and not world.tw.noActive then
    world.tw.noActive = tostring(err)
    logE('traffic twins: setActive failed (' .. tostring(err) .. '): no twins (GTA\'s own traffic contact instead)')
  end
  return ok
end
world.tw.clear = function(why)
  local T, n = world.tw, 0
  for _, e in pairs(T.pool) do
    if e.veh then pcall(function() e.veh:delete() end); n = n + 1 end
  end
  T.pool = {}
  if n > 0 then logI('traffic twins: ' .. n .. ' taken out (' .. tostring(why) .. ')') end
end
world.tw.parkAll = function(why)
  local n = 0
  for _, e in pairs(world.tw.pool) do
    if e.state == 'live' then world.tw.park(e); n = n + 1 end
  end
  if n > 0 then logI('traffic twins: ' .. n .. ' parked (' .. tostring(why) .. ')') end
end
-- (a twin still there: its vehicle answers)
world.tw.alive = function(e)
  if not e.veh then return false end
  local ok, id = pcall(function() return e.veh:getID() end)
  return ok and id ~= nil and (not be or not be.getObjectByID or be:getObjectByID(id) ~= nil)
end
handlers.twp = function(m)
  local T = world.tw
  if type(m.m) ~= 'table' then return end
  -- v2.72: the same pool as before, still there (parked since the last drive): kept, GTA told at once
  local same = #m.m > 0
  for k, model in ipairs(m.m) do
    local e = T.pool[k]
    if not (e and (e.want == model or e.model == model) and (e.state == 'want' or e.state == 'init' or ((e.state == 'park' or e.state == 'live') and T.alive(e)))) then same = false end
  end
  for k in pairs(T.pool) do if k > #m.m then same = false end end
  if same then
    for _, e in pairs(T.pool) do
      if e.state == 'live' then T.park(e) end
      if e.state == 'park' then T.say({ t = 'twi', k = e.k, ok = 1, model = e.model, w = e.size[1], l = e.size[2], h = e.size[3] }) end
    end
    logI('traffic twins: GTA asked for the same ' .. #m.m .. ': kept from before')
    return
  end
  T.clear('a new pool')
  for k, model in ipairs(m.m) do
    if k <= 8 and type(model) == 'string' and model ~= '' then T.pool[k] = { k = k, model = model, want = model, state = 'want' } end
  end
  T.fallback = type(m.fb) == 'table' and m.fb or { 'fullsize', 'pickup', 'covet' }
  logI('traffic twins: GTA asked for ' .. #m.m .. ': ' .. table.concat(m.m, ', '))
end
-- one spawn a frame (each may hitch BeamNG), 2.5 a second; then measured, parked, GTA told ("twi")
world.tw.spawnOne = function(e)
  local T = world.tw
  local car = drive and drive.veh or getPlayerVeh()
  local cp = car and v3(car:getPosition()) or { 0, 0, 300 }
  local pos = vec3(cp[1] + 6 * e.k, cp[2], cp[3] + 150)   -- (in the air: it falls a little until it is parked)
  local tried = { e.model }
  for _, f in ipairs(T.fallback or {}) do if f ~= e.model then tried[#tried + 1] = f end end
  local errs = {}
  pcall(function()   -- (one left from before a Lua reload)
    local old = scenetree.findObject('gtab_twin_' .. e.k)
    if old then old:delete() end
  end)
  for _, model in ipairs(tried) do
    local t0 = os.clock()
    local ok, v = pcall(function()
      return core_vehicles.spawnNewVehicle(model, { pos = pos, rot = quat(0, 0, 0, 1), autoEnterVehicle = false, cling = false, vehicleName = 'gtab_twin_' .. e.k })
    end)
    if ok and v then
      e.veh, e.model = v, model
      pcall(function() e.vid = v:getID() end)
      e.ms = (os.clock() - t0) * 1000
      break
    end
    errs[#errs + 1] = model .. ': ' .. tostring(v)
  end
  pcall(function() if car and be:getPlayerVehicle(0):getID() ~= car:getID() then be:enterVehicle(0, car) end end)
  if not e.veh then
    e.state = 'fail'
    T.fails = T.fails + 1
    logE('traffic twins: twin ' .. e.k .. ' could not be spawned (' .. table.concat(errs, '; ') .. ')')
    T.say({ t = 'twi', k = e.k, ok = 0, why = 'spawn failed' })
    return
  end
  e.state, e.t0 = 'init', now()
end
-- its size, its footprint's middle (o: in its frame from its position) and its body's nodes at rest (not the wheels')
world.tw.measure = function(e)
  local veh = e.veh
  local r, f, u = world.tw.frame(veh)
  local vd = core_vehicle_manager.getVehicleData(e.vid)
  local vdata = vd and vd.vdata
  if not (vdata and vdata.nodes) then return false end
  local byName = {}
  for cid, nd in pairs(vdata.nodes) do if type(nd) == 'table' and nd.name then byName[nd.name] = nd.cid or cid end end
  local function cidOf(x) if type(x) == 'number' then return x end return byName[x] end
  local axles = {}
  for _, w in pairs(vdata.wheels or {}) do
    local a, b = type(w) == 'table' and cidOf(w.node1), type(w) == 'table' and cidOf(w.node2)
    if a and b then axles[#axles + 1] = { v3(veh:getNodePosition(a)), v3(veh:getNodePosition(b)), (tonumber(w.radius) or 0.35) + 0.1 } end
  end
  local all, mn, mx = {}, { 1e9, 1e9, 1e9 }, { -1e9, -1e9, -1e9 }
  for cid, nd in pairs(vdata.nodes) do
    local c = type(nd) == 'table' and (nd.cid or cid)
    if c then
      local p = v3(veh:getNodePosition(c))
      local q = { dot(p, r), dot(p, f), dot(p, u) }
      for i = 1, 3 do mn[i] = math.min(mn[i], q[i]); mx[i] = math.max(mx[i], q[i]) end
      local wheel = false
      for _, ax in ipairs(axles) do
        local ab = sub(ax[2], ax[1])
        local l2 = math.max(1e-6, dot(ab, ab))
        local tt = math.max(0, math.min(1, dot(sub(p, ax[1]), ab) / l2))
        local dd = sub(p, { ax[1][1] + ab[1] * tt, ax[1][2] + ab[2] * tt, ax[1][3] + ab[3] * tt })
        if dot(dd, dd) < ax[3] * ax[3] then wheel = true; break end
      end
      if not wheel then all[#all + 1] = { c, q } end
    end
  end
  if #all < 8 then return false end
  e.o = { (mn[1] + mx[1]) * 0.5, (mn[2] + mx[2]) * 0.5, mn[3] }
  e.size = { mx[1] - mn[1], mx[2] - mn[2], mx[3] - mn[3] }
  local step = math.max(1, math.floor(#all / 300))
  e.cids, e.rest = {}, {}
  for i = 1, #all, step do
    local c, q = all[i][1], all[i][2]
    e.cids[#e.cids + 1] = c
    e.rest[#e.rest + 1] = { q[1] - e.o[1], q[2] - e.o[2], q[3] - e.o[3] }
  end
  return true
end
-- v2.72: the body's nodes as they are now (its frame from its footprint's middle): the shape dents are measured against
world.tw.takeRest = function(e)
  local veh = e.veh
  local r, f, u = world.tw.frame(veh)
  local rest = {}
  for i, c in ipairs(e.cids) do
    local q = v3(veh:getNodePosition(c))
    rest[i] = { dot(q, r) - e.o[1], dot(q, f) - e.o[2], dot(q, u) - e.o[3] }
  end
  e.rest = rest
end
world.tw.park = function(e)
  e.state = 'park'
  e.gid = nil
  if not world.tw.setActive(e, false) then
    -- (no setActive: it would fall onto GTA's ground near the car: taken out, GTA's own traffic contact instead)
    pcall(function() e.veh:delete() end)
    e.veh, e.state = nil, 'fail'
    world.tw.say({ t = 'twi', k = e.k, ok = 0, why = 'setActive failed' })
  end
end
handlers.twa = function(m)
  local T = world.tw
  local e = T.pool[tonumber(m.k) or -1]
  if not (e and (e.state == 'park' or e.state == 'live') and type(m.p) == 'table' and type(m.f) == 'table' and type(m.u) == 'table') then
    T.say({ t = 'twf', k = m.k, why = e and ('not ready (' .. tostring(e.state) .. ')') or 'no such twin' })
    return
  end
  T.setActive(e, true)
  local f = norm({ m.f[1], m.f[2], m.f[3] })
  local u0 = { m.u[1], m.u[2], m.u[3] }
  local du = dot(f, u0)
  local u = norm({ u0[1] - f[1] * du, u0[2] - f[2] * du, u0[3] - f[3] * du })
  local r = cross(f, u)
  local v = type(m.v) == 'table' and m.v or { 0, 0, 0 }
  local lead = 1 / 60   -- (it moves from the next frame: put where GTA's car will be then)
  local P = {}
  for i = 1, 3 do P[i] = m.p[i] + v[i] * lead - (r[i] * e.o[1] + f[i] * e.o[2] + u[i] * e.o[3]) + u[i] * 0.03 end
  -- v2.72: setPositionRotation's own rotation convention (v2.71 took the one learned for the player's safeTeleport: twins could
  -- face backwards); checked 2 frames later against the asked forward, the next one tried if wrong, the good one kept
  e.tpTo = { P = P, f = f, u = u, ci = T.ci or 2 }
  local ok, err = T.tpTwin(e)
  if not ok then
    T.say({ t = 'twf', k = m.k, why = 'teleport failed: ' .. tostring(err) })
    logE('traffic twins: twin ' .. e.k .. ' teleport failed: ' .. tostring(err))
    T.park(e)
    return
  end
  e.state, e.gid, e.tA, e.vel, e.velDone, e.frames, e.dmax = 'live', m.id, now(), v, false, 0, 0
  e.rest = nil   -- (taken again 3 frames after the teleport: its shape here, repaired, upright)
  e.pk = tonumber(m.pk) == 1
  T.acts = T.acts + 1
  pcall(function() e.veh:queueLuaCommand(string.format("input.event('throttle',0,2) input.event('brake',0,2) input.event('steering',0,2) input.event('parkingbrake',%d,2)", e.pk and 1 or 0)) end)
  if T.acts <= 30 then
    logI(string.format('traffic twins: twin %d (%s) put on GTA car %s at %.1f %.1f %.1f, %.0f km/h%s', e.k, e.model, tostring(m.id), m.p[1], m.p[2], m.p[3],
      math.sqrt(v[1] ^ 2 + v[2] ^ 2 + v[3] ^ 2) * 3.6, e.pk and ', parked' or ''))
  end
end
world.tw.tpTwin = function(e)
  local to = e.tpTo
  local cands = rotCandidates(to.f, to.u)
  local c = cands[to.ci] or cands[2] or cands[1]
  if not c then return false, 'no rotation' end
  return pcall(function() e.veh:setPositionRotation(to.P[1], to.P[2], to.P[3], c[2].x, c[2].y, c[2].z, c[2].w) end)
end
-- (v2.73: the v2.72 door latch toggle is gone: GTA turns the drawn car's doors itself, as far as GTA's are open)
function M.eyeFromCar(e, how)
  local d = drive
  if not d then return end
  if type(e) == 'table' then
    logI(string.format('drive: driver\'s eye %.2f %.2f %.2f m (right, forward, up of the car): %s', e[1], e[2], e[3], tostring(how)))
    pcall(function() udp:sendto(string.format('{"t":"eye","e":[%.3f,%.3f,%.3f]}', e[1], e[2], e[3]), d.ip, d.port) end)
  else
    logI('drive: no driver seat from BeamNG (' .. tostring(how) .. '): GTA keeps its own (Ctrl+Shift+F10 in GTA sets it by hand)')
  end
end
handlers.twr = function(m)
  local e = world.tw.pool[tonumber(m.k) or -1]
  if e and e.state == 'live' then
    if world.tw.acts <= 30 then logI(string.format('traffic twins: twin %d parked again (%s; dented up to %.0f mm)', e.k, tostring(m.why), (e.dmax or 0) * 1000)) end
    world.tw.park(e)
  end
end
world.tw.tick = function()
  local T, d = world.tw, drive
  if not d or next(T.pool) == nil then return end
  local t = now()
  -- (one at a time, only while the car stands: each spawn freezes BeamNG for 2-3 s; GTA waits for them before the start)
  local cv = d.vel or { 0, 0, 0 }
  if cv[1] ^ 2 + cv[2] ^ 2 + cv[3] ^ 2 < 0.25 then
    for _, e in pairs(T.pool) do
      if e.state == 'want' and t - T.spawnT > 0.4 then
        T.spawnT = t
        T.spawnOne(e)
        break
      end
    end
  end
  local us = {}
  for _, e in pairs(T.pool) do
    if e.state == 'init' and t - e.t0 > 0.3 then
      local ok, good = pcall(T.measure, e)
      if ok and good then
        T.park(e)
        if e.state ~= 'park' then goto nextTwin end
        logI(string.format('traffic twins: twin %d is a %s (%.1f x %.1f x %.1f m, %d body nodes watched; spawned in %.0f ms)', e.k, e.model, e.size[1], e.size[2], e.size[3], #e.cids, e.ms or 0))
        T.say({ t = 'twi', k = e.k, ok = 1, model = e.model, w = e.size[1], l = e.size[2], h = e.size[3] })
      elseif t - e.t0 > 3 then
        e.state = 'fail'
        logE('traffic twins: twin ' .. e.k .. ' (' .. e.model .. ') could not be measured: ' .. tostring(ok and 'no nodes' or good))
        T.say({ t = 'twi', k = e.k, ok = 0, why = 'not measured' })
      end
    elseif e.state == 'live' then
      e.frames = e.frames + 1
      local veh = e.veh
      if e.frames == 2 and e.tpTo and not e.tpOk then
        local okF, fb = pcall(function() return norm(v3(veh:getDirectionVector())) end)
        if okF and dot(fb, e.tpTo.f) > 0.95 then
          e.tpOk, T.ci = true, e.tpTo.ci
        elseif (e.tpTo.tries or 0) < 3 then
          e.tpTo.tries = (e.tpTo.tries or 0) + 1
          e.tpTo.ci = e.tpTo.ci % 4 + 1
          logI(string.format('traffic twins: twin %d faced the wrong way (%.2f): trying rotation %d', e.k, okF and dot(fb, e.tpTo.f) or -9, e.tpTo.ci))
          T.tpTwin(e)
          e.frames, e.velDone = 0, false
        end
      end
      if e.frames == 3 and not e.rest then pcall(T.takeRest, e) end
      if not e.velDone and e.frames >= 1 then
        e.velDone = true
        local v = e.vel
        pcall(function() veh:applyClusterVelocityScaleAdd(veh:getRefNodeId(), 1, v[1], v[2], v[3]) end)
      end
      local okP = pcall(function()
        local p = v3(veh:getPosition())
        local r, f, u = T.frame(veh)
        local vel = v3(veh:getVelocity())
        local o = e.o
        us[#us + 1] = string.format('%d,%.3f,%.3f,%.3f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f', e.k, p[1] + r[1] * o[1] + f[1] * o[2] + u[1] * o[3],
          p[2] + r[2] * o[1] + f[2] * o[2] + u[2] * o[3], p[3] + r[3] * o[1] + f[3] * o[2] + u[3] * o[3], f[1], f[2], f[3], u[1], u[2], u[3], vel[1], vel[2], vel[3])
        -- its dents, 5 a second: body nodes 4 cm or more from where they rest, grouped within 0.9 m (the deepest first)
        if t - (e.dT or 0) > 0.2 and e.frames > 6 and e.rest then
          e.dT = t
          local dn = {}
          for i, c in ipairs(e.cids) do
            local q = v3(veh:getNodePosition(c))
            local a = { dot(q, r) - o[1], dot(q, f) - o[2], dot(q, u) - o[3] }
            local z = e.rest[i]
            local dd = math.sqrt((a[1] - z[1]) ^ 2 + (a[2] - z[2]) ^ 2 + (a[3] - z[3]) ^ 2)
            -- (over 0.8 m: a part hanging off or gone, not a dent; v2.71 sent those: GTA cars crushed "by your presence")
            if dd > 0.04 and dd < 0.8 then dn[#dn + 1] = { z, dd } end
          end
          table.sort(dn, function(x, y) return x[2] > y[2] end)
          local spots = {}
          for _, n in ipairs(dn) do
            local near = false
            for _, sp in ipairs(spots) do
              if (sp[1] - n[1][1]) ^ 2 + (sp[2] - n[1][2]) ^ 2 + (sp[3] - n[1][3]) ^ 2 < 0.81 then near = true; break end
            end
            if not near then spots[#spots + 1] = { n[1][1], n[1][2], n[1][3], n[2] } end
            if #spots >= 6 then break end
          end
          local top = spots[1] and spots[1][4] or 0
          if top > (e.dmax or 0) + 0.01 then
            e.dmax = top
            local flat = {}
            for _, sp in ipairs(spots) do for i = 1, 4 do flat[#flat + 1] = tonumber(string.format('%.3f', sp[i])) end end
            T.say({ t = 'twd', k = e.k, s = flat })
          end
        end
      end)
      if not okP and not e.poseErr then e.poseErr = true; logE('traffic twins: twin ' .. e.k .. ' pose not read') end
    end
    ::nextTwin::
  end
  if #us > 0 then
    T.poses = T.poses + #us
    -- (v2.74: with BeamNG's clocks, as the car's poses: GTA draws the twins at the same moment as your car, not as they come in)
    pcall(function() udp:sendto(string.format('{"t":"twu","st":%.5f,"ss":%.5f,"s":[', d.st or 0, d.ss or d.st or 0) .. table.concat(us, ',') .. ']}', d.ip, d.port) end)
  end
  if t - T.logT > 10 then
    T.logT = t
    local live, park = 0, 0
    for _, e in pairs(T.pool) do if e.state == 'live' then live = live + 1 elseif e.state == 'park' then park = park + 1 end end
    if live + park > 0 then logI(string.format('traffic twins: %d live, %d parked; %d put on GTA cars, %d poses to GTA (10 s)', live, park, T.acts, T.poses)) end
    T.poses = 0
  end
end

handlers.tp = function(m)
  if type(m.p) ~= 'table' or type(m.f) ~= 'table' or type(m.u) ~= 'table' then logE('tp: bad message'); return end
  local old = world.tp
  -- v2.72: GTA asking again for the same spot (no answer in 4 s) keeps the running wait: your v2.71 log, the start at a hole in
  -- the hills: each ask restarted the 5 s wait, so it waited 21 s and then put the car on nothing
  if old and old.m and old.m.why == m.why and math.abs(old.m.p[1] - m.p[1]) + math.abs(old.m.p[2] - m.p[2]) < 1.0 then return end
  if old then logI('teleport: a new request replaces the running one') end
  world.tp = { m = m, t0 = now(), state = 'wait' }
  logI(string.format('teleport requested (%s): to %.2f %.2f %.2f', tostring(m.why), m.p[1], m.p[2], m.p[3]))
end

-- v2.43: your repair binding (RB+LB+X...) while GTA drives: the car is repaired where it is, upright, facing the same
-- way, 0.5 m up (BeamNG's own reset / recover / home would put it somewhere else: in GTA it would jump away). The same
-- teleport GTA's ground uses, with BeamNG's repairing reset.
function M.repairHere(action)
  local d = drive
  if not d then logI('repair (' .. tostring(action) .. '): only while GTA drives'); return end
  local t = now()
  if world.tp then logI('repair: a teleport is running: skipped'); return end
  if t - (d.repT or 0) < 1.0 then return end
  d.repT = t
  local veh = getPlayerVeh()
  if not veh then return end
  local p = v3(veh:getPosition())
  local f = v3(veh:getDirectionVector())
  local l = math.sqrt(f[1] * f[1] + f[2] * f[2])
  local fh = l > 0.1 and { f[1] / l, f[2] / l, 0 } or { 0, 1, 0 }
  world.tp = { m = { p = p, f = fh, u = { 0, 0, 1 }, o = { 0, 0, 0 }, lift = 0.5, why = 'repair', repair = true }, t0 = t, state = 'wait' }
  d.repairs = (d.repairs or 0) + 1
  logI(string.format('repair (%s): the car repaired where it is (%.1f %.1f %.1f), upright', tostring(action), p[1], p[2], p[3]))
  pcall(function() udp:sendto(jsonEncode({ t = 'rep', a = tostring(action) }), d.ip, d.port) end)
end

function M.world()
  pcall(loadGroundModels)   -- v2.41: the concrete line is current right after a map load
  local nt = 0
  for _ in pairs(worldTiles) do nt = nt + 1 end
  local mats = {}
  for gm, mm in pairs(world.mats) do mats[#mats + 1] = gm .. ' (' .. tostring(mm.name) .. ', groundType reads "' .. tostring(mm.back) .. '")' end
  table.sort(mats)
  local lines = {
    string.format('GTA ground: origin %s, %s m up, epoch %d, %d tiles, tile mode %s (%s)', world.ox and string.format('%.0f, %.0f', world.ox, world.oy) or 'none yet',
      tostring(world.zl), world.epoch, nt, world.mode, world.checkNote),
    'ground models in BeamNG: ' .. tostring(world.gmList),
    'surface materials: ' .. (#mats > 0 and table.concat(mats, '; ') or 'none yet'),
    'teleport: ' .. (world.conv and ('convention ' .. world.conv .. ' via ' .. tostring(world.api)) or 'not used yet') .. (world.padOn and '; parking pad under the car' or '')
      .. '; no-repair teleport: ' .. (world.keepBad and ('off (' .. world.keepBad .. ')') or (stReset and 'available' or (stReset == false and 'not in this BeamNG' or 'not checked yet'))),
    'concrete (GTA sidewalks): ' .. tostring(world.concreteNote or 'not checked yet'),
  }
  for _, l in ipairs(lines) do logI(l) end
  flushLog(true)
  screen('GTA bridge: ' .. lines[1], 10)
  return lines
end

-- 4 strips (asphalt, dirt, grass, sand, 6 m wide each) on a 24 m square 10 m ahead of the car, to feel BeamNG's
-- ground models on this mod's tiles without GTA. Drive across them (gtaBridge.clear() removes them).
function M.surfaceTest()
  loadGroundModels()
  local o, f = carFrame()
  local cx, cy = o[1] + f[1] * 22, o[2] + f[2] * 22
  local n, st = 25, 1
  local z = {}
  for i = 1, n * n do z[i] = 5 end   -- 5 cm above the ground under the car
  local cells = {}
  for j = 0, n - 2 do
    for i = 0, n - 2 do cells[#cells + 1] = tostring(math.min(3, math.floor(i / 6))) end
  end
  local ok, res = buildTileMsg({ id = 'surface_test', n = n, st = st, x0 = cx - 12, y0 = cy - 12, z0 = o[3], z = z,
                                 s = { 'ASPHALT', 'DIRT', 'GRASS', 'SAND' }, m = table.concat(cells) })
  reloadCollisionNow()
  local mats = {}
  for gm, mm in pairs(world.mats) do mats[#mats + 1] = gm .. ' reads "' .. tostring(mm.back) .. '"' end
  logI('surface test: ' .. (ok and 'built' or ('FAILED ' .. tostring(res))) .. ' (west to east: asphalt, dirt, grass, sand); materials: ' .. table.concat(mats, ', '))
  screen('GTA bridge: surface test strips 10 m ahead (asphalt, dirt, grass, sand). Drive across them.', 10)
  flushLog(true)
end
end

-- surf arrives here only if dispatch couldn't fast-path it (normally it's queued raw, see dispatch)
handlers.surf = function(m)
  local e = { kind = 'build', id = m.id or 'default', raw = jsonEncode(m), t = now() }
  pendingBuild[e.id] = e
  enqueue(e)
end

handlers.remove = function(m)
  local p = pendingBuild[m.id]
  if p then p.cancelled = true end
  enqueue({ kind = 'remove', id = m.id, t = now() })
end

handlers.clear = function(m)
  clearQueue()
  M.clear()
  send({ t = 'clear_ok' })
end

handlers.selftest = function(m)
  M.selfTest()
  send({ t = 'selftest_ok', caps = caps })
end

local function dispatch(str)
  -- v2.0: GTA's controls, every GTA frame
  if str:sub(1, 13) == '{"t":"drive",' then driveInput(str); return end
  -- fast path: queue surface messages as raw text; they're decoded later inside the frame budget.
  -- Requires "t" to be the first key, e.g. {"t":"surf","id":"tile_3_4",...}
  if str:find('^%s*{%s*"t"%s*:%s*"surf"') or str:find('^%s*{%s*"t"%s*:%s*"hf"') then   -- v2.4: GTA ground tiles too
    local id = str:match('"id"%s*:%s*"([^"]*)"') or 'default'
    local e = { kind = 'build', id = id, raw = str, t = now() }
    pendingBuild[id] = e
    enqueue(e)
    return
  end
  local ok, m = pcall(jsonDecode, str)
  if not ok or type(m) ~= 'table' then logE('bad json'); return end
  local h = handlers[m.t]
  if h then
    local okH, err = pcall(h, m)
    if not okH then logE('handler', tostring(m.t), 'failed:', tostring(err)) end
  else
    logE('unknown message type', tostring(m.t))
  end
end

-- Fragment format: "F|<msgId>|<index 1-based>|<count>|<payload>"
local function onDatagram(data)
  if data:sub(1, 2) ~= 'F|' then dispatch(data); return end
  local msgId, idx, count, payload = data:match('^F|([^|]+)|(%d+)|(%d+)|(.*)$')
  if not msgId then logE('bad fragment'); return end
  idx, count = tonumber(idx), tonumber(count)
  local f = frags[msgId]
  if not f then f = { count = count, got = 0, parts = {}, t = now() }; frags[msgId] = f end
  if not f.parts[idx] then f.parts[idx] = payload; f.got = f.got + 1 end
  if f.got == f.count then
    frags[msgId] = nil
    dispatch(table.concat(f.parts))
  end
end

-- ---------------------------------------------------------------- engine hooks
local driveErrLogged = false
local function onUpdate(dtReal, dtSim, dtRaw)
  local tFrame = now()
  -- v2.43: a map being changed: nothing is touched until the new one has started (BeamNG crashed when another map was
  -- loaded from inside a map); messages from GTA are read and dropped
  if world and world.levelBusy and now() - (world.levelBusyT or 0) > 600 then
    world.levelBusy = false
    logE('a map change didn\'t finish in 10 minutes: the mod works again')
  end
  if world and world.levelBusy then
    if udp then
      for _ = 1, cfg.maxPacketsPerFrame do
        local data, ip = udp:receivefrom(65535)
        if not data and (ip == 'timeout' or ip == nil) then break end
      end
    end
    flushLog()
    return
  end
  if collisionDirty and not udp then reloadCollisionNow() end
  if udp then
    for _ = 1, cfg.maxPacketsPerFrame do
      local data, ip, port = udp:receivefrom(65535)
      if not data then
        -- v2.0: only "nothing waiting" ends the loop (a "port unreachable" from a closed GTA is skipped)
        if ip == 'timeout' or ip == nil then break end
      else
        stats.rx = stats.rx + 1
        peerIp, peerPort = ip, port
        local okD, errD = pcall(onDatagram, data)
        if not okD then logE('message failed:', tostring(errD)) end
      end
    end
  end
  -- v2.0: drive mode first after the controls arrived (nothing below can stop it)
  local okDr, errDr = pcall(driveUpdate, dtReal, dtSim, dtRaw)
  if not okDr and not driveErrLogged then driveErrLogged = true; logE('drive: update failed:', tostring(errDr)) end
  -- v2.51: prop boxes (new ones in, breakaway ones the car breaks: before this frame's collision reload) and the ride's jolts
  local okPr, errPr = pcall(world.props.tick)
  if not okPr and not world.props.tickErr then world.props.tickErr = true; logE('props: ' .. tostring(errPr)) end
  local okPh, errPh = pcall(world.phys.tick)   -- v2.52
  if not okPh and not world.phys.tickErr then world.phys.tickErr = true; logE('props: physics: ' .. tostring(errPh)) end
  local okTw, errTw = pcall(world.tw.tick)   -- v2.71
  if not okTw and not world.tw.tickErr then world.tw.tickErr = true; logE('traffic twins: ' .. tostring(errTw)) end
  pcall(world.feel.tick, dtSim)
  do   -- v2.6: weapons (the car's Lua, its tick; also after the drive while the car burns)
    local okW, errW = pcall(wpn.tick)
    if not okW and not wpn.err then wpn.err = true; logE('weapons: ' .. tostring(errW)) end
  end
  -- v2.01: BeamNG's own device controls off while GTA drives or holds the car parked
  local parked = park.block and not native.userOff and now() - park.t < driveCfg.parkS
  pcall(setNativeBlocked, drive ~= nil or parked, drive and 'GTA drives' or parked and 'GTA is focused, car parked' or 'GTA stopped')
  pcall(keepNativeBlocked)
  checkExportTimeout()
  flushLog()
  if not udp then return end
  local t = now()
  for id, f in pairs(frags) do
    if t - f.t > cfg.fragTimeout then frags[id] = nil; logE('dropped incomplete message', id) end
  end
  processQueue()
  -- v2.4: with GTA's ground in use, a slow reload waits up to 0.1 s for more tiles (unless one near the car needs it)
  if collisionDirty and reloadDue() then
    reloadCollisionNow()
    world.urgent = false
  end
  safeApplyCam()
  -- how much of this frame the bridge used (packets + builds + collision reload)
  local ms = (now() - tFrame) * 1000
  stats.lastFrameMs = ms
  if ms > stats.maxFrameMs then stats.maxFrameMs = ms end
  if ms > stats.windowMaxFrameMs then stats.windowMaxFrameMs = ms end
  local okW, errW = pcall(worldUpdate, ms)   -- v2.4: teleport, ground stats for GTA
  if not okW and not world.errLogged then world.errLogged = true; logE('ground: update failed: ' .. tostring(errW)) end
end

local function onPreRender()
  safeApplyCam()   -- re-apply right before drawing so nothing else moves the camera in between
end

local function onExtensionLoaded()
  pcall(restoreNativeFromFile)
  pcall(restoreSoundFromFile)
  pcall(restoreSessionFromFile)
  pcall(grav.restoreFile)   -- v2.5
  openSocket()
  logI('v' .. M.version, 'loaded. socket:', caps.socket)
  -- v2.4: sound session mode, remembered from the last time GTA asked for it (BeamNGSoundWake=session): switched on now,
  -- while BeamNG has the focus (start BeamNG first), so it plays in the background all session; quiet until GTA drives
  pcall(readModConfig)
  if modConfig.soundSession then
    pcall(sessionOn, 'remembered: BeamNG start')
    pcall(sessionVolume, true, 'not driving')
  end
  flushLog(true)
end

local function onExtensionUnloaded()
  driveEnd('the BeamNG mod was unloaded', true)
  pcall(setNativeBlocked, false, 'mod unloaded')
  pcall(driveSettings, false)
  pcall(sessionOff, 'mod unloaded')   -- v2.4: BeamNG's own sound settings back (session mode is still remembered)
  if not (world and world.levelBusy) then M.clear() end   -- v2.43: not while a map is being changed (its objects go with it)
  if udp then udp:close(); udp = nil end
end

function M.clear()
  for id in pairs(surfaces) do removeSurface(id) end
  world.padOn = false   -- v2.4
  reloadCollisionNow()
end

function M.status()
  local n = 0 for _ in pairs(surfaces) do n = n + 1 end
  local lines = {
    'version         : ' .. M.version,
    'socket lib      : ' .. tostring(caps.socket),
    'mesh API        : ' .. tostring(caps.mesh),
    'collision reload: ' .. tostring(caps.collisionReload),
    'ground          : ' .. tostring(caps.ground),
    'winding flipped : ' .. tostring(cfg.flipWinding),
    'camera          : ' .. tostring(caps.cam),
    'green material  : ' .. tostring(caps.greenMaterial),
    'surfaces        : ' .. n,
    'live triangles  : ' .. liveTris(),
    'drive mode      : ' .. (drive and ('on (' .. #drive.wheels .. ' wheels, controls: ' .. tostring(drive.inpHow) .. ')') or 'off'),
    'GTA ground      : ' .. (world.ox and string.format('origin %.0f, %.0f, %.0f m up, tile mode %s, teleport %s', world.ox, world.oy, world.zl, world.mode,
                                                        world.conv and ('convention ' .. world.conv) or 'not used yet') or 'not used yet'),
    'sound session   : ' .. (session.on and ('on' .. (session.quietNow and ', BeamNG quiet now' or '')) or 'off'),
    'BeamNG controls : ' .. (native.blocked and 'blocked (GTA drives or holds the car)' or 'normal'),
    string.format('builds %d  last %.2f ms  max %.2f ms', stats.builds, stats.lastBuildMs, stats.maxBuildMs),
    string.format('collision reloads %d  last %.2f ms  max %.2f ms  avg %.2f ms', stats.reloads, stats.lastReloadMs,
      stats.maxReloadMs, stats.reloads > 0 and stats.sumReloadMs / stats.reloads or 0),
    string.format('bridge cost per frame: last %.2f ms  max %.2f ms  (build budget %.1f ms)', stats.lastFrameMs, stats.maxFrameMs, cfg.buildBudgetMs),
    string.format('build queue: now %d  max %d  max wait %.1f ms  cancelled %d', queueLen(), stats.maxQueue, stats.maxWaitMs, stats.cancelled),
    string.format('rx %d  tx %d  pings %d  errors %d', stats.rx, stats.tx, stats.pings, stats.errors),
  }
  for _, l in ipairs(lines) do logI(l) end
  flushLog(true)
  screen('GTA bridge ' .. M.version .. ': socket ' .. tostring(caps.socket) .. ' - details in gtabridge/gtabridge_log.txt', 10)
  return caps
end

M.onUpdate = onUpdate
-- v2.43: a map is being left (another one loads): the drive ends, the mod's tiles are forgotten (the map takes them
-- with it) and nothing is touched until the new map has started
local function leavingMap(why)
  local ok, err = pcall(function()
    if world.levelBusy then return end
    world.levelBusy, world.levelBusyT = true, now()
    if drive then driveEnd('the BeamNG map is being changed', true) end
    local n = 0
    for id in pairs(surfaces) do surfaces[id] = nil; surfaceTris[id] = nil; n = n + 1 end
    for id in pairs(worldTiles) do worldTiles[id] = nil end
    pcall(clearQueue)
    collisionDirty = false
    world.tp, world.padOn, world.urgent = nil, false, false
    logI('a map is being left (' .. why .. '): ' .. n .. ' surfaces forgotten; the mod waits until the next map has started')
    flushLog(true)
  end)
  if not ok then logE('map change: ' .. tostring(err)) end
end
M.onClientEndMission = function() leavingMap('end of mission') end
M.onClientPreStartMission = function() leavingMap('a map starts loading') end
-- v2.41: a map (re)loaded: BeamNG read its ground models again (CONCRETE, if the mod wrote it, is in now)
M.onClientPostStartMission = function()
  if world then world.levelBusy = false end
  local ok, err = pcall(function()
    -- the old map's objects are gone with it: forget them (GTA sends its tiles again: map_ok says 0)
    local n = 0
    for id in pairs(surfaces) do surfaces[id] = nil; surfaceTris[id] = nil; n = n + 1 end
    for id in pairs(worldTiles) do worldTiles[id] = nil end
    pcall(clearQueue)
    collisionDirty = false
    if world then
      world.groundModels, world.resolved, world.gmWrittenNow, world.mats = nil, {}, false, {}
      world.ox, world.oy, world.checked, world.mode, world.checkNote, world.padOn, world.tp = nil, nil, false, 'submesh', 'not checked yet', false, nil
    end
    logI('a map was loaded: ' .. n .. ' surfaces of the old map forgotten; BeamNG\'s ground models are read again (CONCRETE, if the mod wrote it, is in now)')
  end)
  if not ok then logE('map load: ' .. tostring(err)) end
end
M.onPreRender = onPreRender
M.onExtensionLoaded = onExtensionLoaded
M.onExtensionUnloaded = onExtensionUnloaded

return M
