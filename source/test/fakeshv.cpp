// fakeshv.cpp - stand-in for ScriptHookV.dll in the sandbox smoke test (Wine). Same exported names
// as the real SDK; natives answer with a fixed scene: camera at (5.5, 3, 1.4) looking at a car at
// the origin, the player driving it, a burnout in progress.
// v1.9.1: wheel bones (wheel_lf/rf/lr/rr) that turn with the car's speed (world Euler angles, order ZXY,
// left bones mirrored; FAKE_LOCALBONES=1 gives car-relative angles, FAKE_NOBONES=1 no bones), HUD natives.
// v2.0: the car is an entity with a pose the drive mode can set (coords, rotation order 2 / quaternion,
// velocity moved on between script ticks like GTA's physics), disabled-control readings from FAKE_INPUT
// ("throttle brake leftright handbrake"), FAKE_ROTWRONG=1 takes Euler angles 30 deg off (tests the
// quaternion fallback); every pose set goes to fake_puppet.csv.
// v2.01: which of GTA's controls the plugin switches off each tick ("DISABLED:" lines when the set changes),
// on-screen text lines (fake_hud.txt), IS_USING_KEYBOARD_AND_MOUSE from FAKE_KBM (default 0 = pad player).
// v2.4: a GTA ground for the T3 rays: FakeGround(x, y) = a slope + a hill wave + a 15 cm curb strip, Tarmac on the road
// (|x| < 6), DirtTrack beside it, Grass further out; FAKE_HOLE=1 adds a 10 m strip with no ground (water) across the
// road at y 70-80. Asynchronous rays (flags 1) get their own handles and answer one script tick later; at most
// FAKE_RAYCAP (default 300) wait at once (the next one is refused with 0, like a full pool). The radio: FAKE_RADIO
// (station index, default 255 = off).
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

typedef void (*PresentCb)(void*);
typedef void (*ScriptFn)();
static PresentCb g_present[4];
static ScriptFn  g_script;
static UINT64    g_hash;
static UINT64    g_args[32];
static int       g_nargs;
static UINT64    g_ret[8];
static float     g_yawDeg = 20.0f;  // the GTA car's heading
static int       g_burnout = 1;
static bool      g_rayDown = true;
static double    g_speed = 0.3;     // m/s forward
static double    g_dist = 0, g_rearExtra = 0;
static ULONGLONG g_lastMs = 0;
// v2.0 entity state of the car (100)
static double g_pos[3] = { 0, 0, 0.6 };
static double g_rot[3][3];  // columns: right, forward, up
static bool   g_rotInit = false;
static double g_vel[3] = {};
static bool   g_velSet = false, g_frozen = false, g_collision = true, g_gravity = true;
static double g_physT = 0;
static double g_frameT = 0.016;  // v2.0: the real time between script ticks (like GTA's frame time)
static double g_timeScale = 1.0; // v2.5: host "timescale <x>": GTA's slow motion (radio wheel)
static FILE*  g_pl = nullptr;
static LARGE_INTEGER g_qf{};
// v2.01
static bool        g_dis[512], g_disPrev[512];
static int         g_textMode = 0;  // 1 = drawing text, 2 = feed post
static std::string g_textBuf;
static std::vector<std::string> g_hud, g_hudPrev;
static double      g_hudT = 0;
static int         g_paused = 0;
// v2.6: FAKE_WEAPONS=1: once the drive is on (fake_drive_on.txt) three shooters stand around the car and, 2 s later, fire at it:
// ped 7000 from 8 m left through the cabin at window height (every 0.4 s for 3 s), ped 7001 from 8 m left at the bottom of the
// front left tyre (every 0.5 s for 3 s), ped 7002 from 10 m ahead into the front at the radiator (every 0.5 s for 2 s); a
// grenade (type 0) goes off 3 m behind the car at +6 s (0.3 s in GTA's list); fake_wpn_done.txt at +20 s. The player's health
// (200), fires, proofs and GTA's car blowing up (a type 7 explosion at it) are printed.
struct FakePed { int h; double head[3], tgt[3], t0, every, until, last; int n; };
static std::vector<FakePed> g_peds;
static double g_wT0 = -1;
static bool   g_wDone = false;
struct FakeExp { int type; double p[3], t0, t1; };
static std::vector<FakeExp> g_exps;
static int g_health = 200, g_fires = 0;
static bool g_nadeDone = false;
// v2.61: FAKE_FOOT=1: 2 s after the drive is on you get out (you stand 4 m left of the car), shoot it 5 times from there (every
// 0.5 s, from +1.5 s), get back in at +7 s; fake_foot_done.txt at +12 s. Particle effects: the first fire name is unknown here
// (GTA would return 0), the second one starts.
static bool   g_footOut = false, g_footDone = false;
static double g_fT0 = -1;
static int    g_fxNext = 6000;
struct FakeRay { int h; float x0, y0, z0, x1, y1, z1; unsigned tick; bool now; };
static std::vector<FakeRay> g_rays;
static int      g_rayNext = 1000;
static unsigned g_tickN = 0;
static unsigned g_rayStarted = 0, g_rayRefused = 0, g_raySync = 0, g_hRays = 0;
static int      g_freezeMs = 0;       // v2.41: host "freeze <ms>": the next script wait takes this long (GTA paused on alt-tab)
static int      g_radioIdx = -1;      // v2.41: GTA's radio (255 off, 1 a station); -1 = from FAKE_RADIO at first use
static int      g_collCalls = 0;      // v2.41: HAS_COLLISION_LOADED_AROUND_ENTITY calls (FAKE_COLLWAIT=N: false for the first N)
static bool Env(const char* n);
static int Radio()
{
	if (g_radioIdx < 0) { char e[16]; g_radioIdx = GetEnvironmentVariableA("FAKE_RADIO", e, sizeof e) ? atoi(e) : 255; }
	return g_radioIdx;
}
static double FakeGround(double x, double y, bool* hit, unsigned* mat)
{
	*hit = true;
	if (Env("FAKE_HOLE") && y > 70 && y < 80) { *hit = false; *mat = 0; return 0; }
	double z = 0.03 * x + 1.5 * sin(y / 25.0);
	// v2.7: FAKE_DROP=1: right of the car's line (heading 110 deg), from 8.5 m off it, the ground is 16 m lower (a ridge road's drop:
	// out of the rays' normal reach, 12 m down)
	if (Env("FAKE_DROP") && 0.93969 * x + 0.34202 * y > 8.5) z -= 16.0;
	if (x > 3.5 && x < 4.0) z += 0.15;  // a curb
	// v2.54: FAKE_CREST=1: the road goes over a crest at 70 m along the car's line (a ramp meeting a bridge deck: 1.2 m up
	// and down within a few metres) with a 0.2 m joint across the road at its top (a low face, not a wall)
	if (Env("FAKE_CREST")) {
		const double sc = (-0.34202 * x + 0.93969 * y - 70.0) / 3.0;
		z += 1.2 * exp(-sc * sc);
	}
	// v2.55: FAKE_PITS=1: a 0.6 m square hole every 4 m along the road, 0.8 m deep (GTA's road with a crack the rays slip
	// through to the ground under it)
	if (Env("FAKE_PITS") && fabs(x) < 4.0 && fmod(fabs(y) + 0.3, 4.0) < 0.6 && fmod(fabs(x) + 0.3, 3.0) < 0.6) z -= 0.8;
	// v2.55: and (FAKE_PITS=1) a 0.5 m wide beam 3 m over the road across it every 25 m along the car's line (a truss member the
	// ground rays meet before the road: GTA's beams are open below; here the ground just has it)
	if (Env("FAKE_PITS") && fmod(fabs(-0.34202 * x + 0.93969 * y) + 0.25, 25.0) < 0.5) z += 3.0;
	// v2.51: FAKE_JUMP=1: the ground drops 10 m at 80 m along the car's line (a cliff it drives off)
	if (Env("FAKE_JUMP") && -0.34202 * x + 0.93969 * y > 80.0) z -= 10.0;
	if (Env("FAKE_ROUGH")) {
		// v2.42: ground like GTA's: a road crown, bumpy grass and dirt (a few cm), a dip across the road every 40 m
		const double ax0 = fabs(x);
		if (ax0 < 6) z -= 0.02 * ax0;
		else z -= 0.12 + 0.04 * sin(x * 2.1) * sin(y * 1.7) + 0.02 * sin(x * 5.3 + y * 3.1);
		const double ym = fmod(fabs(y), 40.0);
		if (ym > 20.0 && ym < 21.5) z -= 0.06 * sin((ym - 20.0) / 1.5 * 3.14159265);
	}
	const double ax = fabs(x);
	*mat = ax < 6 ? 0x10DD5498u : ax < 14 ? 0x8F9CD58Fu : 0x4F747B87u;  // Tarmac, DirtTrack, Grass
	return z;
}
// v2.43: FAKE_BRIDGE=1: an overpass across the road (a deck along x at y 40..52, its top 6 m over the road) and a bridge
// the road goes up onto (around the car's line x = -0.364 y: ramps 10 m wide at y 90..130 up to 8 m and 190..230 down,
// the deck between them 36 m wide: a car starting 12 m to the side (FAKE_STARTX=12) passes the ramp and drives UNDER the
// deck). A ray stops at the first surface below where it starts (from inside a deck it doesn't see it).
static int BridgeOn() { char e[16]; return GetEnvironmentVariableA("FAKE_BRIDGE", e, sizeof e) ? atoi(e) : 0; }
static double RampH(double x, double y)
{
	const double off = fabs(x + 0.364 * y);
	if ((BridgeOn() != 1 && BridgeOn() != 2) || y <= 90 || y >= 230) return 0;
	const double H = BridgeOn() == 2 ? 14.0 : 8.0;  // v2.5: FAKE_BRIDGE=2: a 14 m deck (the ground beside it is out of the rays' reach)
	// v2.62: FAKE_JOINT=1: a 1.1 m gap across the deck at y 163.7..164.8 (an expansion joint: the rays slip through to the ground)
	if (Env("FAKE_JOINT") && y > 163.7 && y < 164.8) return 0.0;
	if (y >= 130 && y < 190) return off < 18.0 ? H : 0.0;
	if (off > 5.0) return 0;
	return y < 130 ? H * (y - 90) / 40.0 : H * (230 - y) / 40.0;
}
// v2.5: FAKE_WALLS=1: GTA's walls along the car's line (forward f = (-0.342, 0.940) from the start, right r = (0.940, 0.342)):
// a jersey barrier 6 m right of it (0.6 m wide, 0.9 m tall, s 20..300), a 2.5 m wall at 45 degrees 12 m left (s 48..72), a
// building 16 m right (10 x 14 m, 12 m tall, s 113..127), a pillar 5 m left (1 x 1 m, 8 m, s 200). GTA's map collision is
// one-sided (a ray from inside a box doesn't see it) unless FAKE_TWOSIDED=1.
struct FakeWall { double cx, cy, ux, uy, hl, ht, h; };
static std::vector<FakeWall> g_walls;
static bool g_wallsInit = false;
static void InitWalls()
{
	if (g_wallsInit) return;
	g_wallsInit = true;
	if (!Env("FAKE_WALLS") && !Env("FAKE_CREST")) return;
	const double fx = -0.34202, fy = 0.93969, rx = 0.93969, ry = 0.34202;
	auto at = [&](double s, double o, double angDeg, double hl, double ht, double h) {
		const double a = atan2(fy, fx) + angDeg * 3.14159265358979 / 180.0;
		g_walls.push_back({ fx * s + rx * o, fy * s + ry * o, cos(a), sin(a), hl, ht, h });
	};
	if (Env("FAKE_CREST")) at(70.0, 0.0, 90.0, 7.0, 0.08, 0.2);  // v2.54: the joint across the road
	if (!Env("FAKE_WALLS")) return;
	at(160.0, 6.0, 0.0, 140.0, 0.3, 0.9);    // the barrier, along the road
	at(60.0, -12.0, 45.0, 12.0, 0.5, 2.5);   // a wall at 45 degrees
	at(120.0, 16.0, 0.0, 7.0, 5.0, 12.0);    // a building
	at(200.0, -5.0, 0.0, 0.5, 0.5, 8.0);     // a pillar
}
// a point inside a wall's footprint: its index (-1 none)
static int WallAt(double x, double y)
{
	InitWalls();
	for (size_t i = 0; i < g_walls.size(); ++i) {
		const FakeWall& w = g_walls[i];
		const double    u = (x - w.cx) * w.ux + (y - w.cy) * w.uy, v = -(x - w.cx) * w.uy + (y - w.cy) * w.ux;
		if (fabs(u) <= w.hl && fabs(v) <= w.ht) return int(i);
	}
	return -1;
}
static double FakeGround(double x, double y, bool* hit, unsigned* mat);
// a flat ray against the walls: the first face it enters (FAKE_TWOSIDED=1: or leaves) between the wall's foot and top
static bool FakeHRay(double x0, double y0, double z0, double x1, double y1, double z1, double* hp, double* hn)
{
	InitWalls();
	const bool two = Env("FAKE_TWOSIDED");
	double     best = 2.0;
	for (const FakeWall& w : g_walls) {
		// the ray in the wall's frame (u along it, v across)
		const double au = (x0 - w.cx) * w.ux + (y0 - w.cy) * w.uy, av = -(x0 - w.cx) * w.uy + (y0 - w.cy) * w.ux;
		const double bu = (x1 - w.cx) * w.ux + (y1 - w.cy) * w.uy, bv = -(x1 - w.cx) * w.uy + (y1 - w.cy) * w.ux;
		const double du = bu - au, dv = bv - av;
		double       tin = -1e30, tout = 1e30;
		int          fin = -1, fout = -1;  // 0 -u, 1 +u, 2 -v, 3 +v
		bool         miss = false;
		const double lo[2] = { -w.hl, -w.ht }, hi[2] = { w.hl, w.ht }, o[2] = { au, av }, d[2] = { du, dv };
		for (int k = 0; k < 2 && !miss; ++k) {
			if (fabs(d[k]) < 1e-12) {
				if (o[k] < lo[k] || o[k] > hi[k]) miss = true;
				continue;
			}
			double t0 = (lo[k] - o[k]) / d[k], t1 = (hi[k] - o[k]) / d[k];
			int    f0 = 2 * k, f1 = 2 * k + 1;
			if (t0 > t1) { std::swap(t0, t1); std::swap(f0, f1); }
			if (t0 > tin) { tin = t0; fin = f0; }
			if (t1 < tout) { tout = t1; fout = f1; }
		}
		if (miss || tin > tout) continue;
		double t = -1;
		int    face = -1;
		if (tin >= 0 && tin <= 1) { t = tin; face = fin; }
		else if (two && tin < 0 && tout >= 0 && tout <= 1) { t = tout; face = fout; }
		if (t < 0 || t >= best) continue;
		const double hx = x0 + (x1 - x0) * t, hy = y0 + (y1 - y0) * t, hz = z0 + (z1 - z0) * t;
		bool         gh;
		unsigned     m;
		const double g = FakeGround(hx, hy, &gh, &m);
		if (hz < g - 0.5 || hz > g + w.h) continue;
		// the face's outward normal in the world
		double nu = 0, nv = 0;
		if (face == 0) nu = -1; else if (face == 1) nu = 1; else if (face == 2) nv = -1; else nv = 1;
		best = t;
		hp[0] = hx; hp[1] = hy; hp[2] = hz;
		hn[0] = nu * w.ux - nv * w.uy;
		hn[1] = nu * w.uy + nv * w.ux;
		hn[2] = 0;
	}
	return best <= 1.0;
}

// v2.5: FAKE_PROPS=1: props in GTA's object pool (handles 9000+) along the car's line: lamp posts 4.5 m right of it every
// 20 m (breakable, 0.4 x 0.4 x 8 m), a bin ON the line at s 45 (0.6 x 0.6 x 1 m), a lamp post ON it at s 80, a container 7 m
// left at s 100 (6 x 2.4 x 2.6 m: solid), a can (tiny: ignored) and one attached to something (skipped)
struct FakeProp { int h; double x, y, yaw; unsigned model; bool frag, attached, pushed; bool posed = false, frozen = false; double z = 0, R[3][3] = {}; int poses = 0; };
static std::vector<FakeProp> g_props;
static bool g_propsInit = false;
static const unsigned kMLamp = 0xA1110001u, kMBin = 0xA1110002u, kMCont = 0xA1110003u, kMCan = 0xA1110004u;
static const unsigned kMTraf = 0xA1110005u;  // v2.51: a traffic light: its pole 4.5 m right of the road, its arm 8 m over it
static void InitProps()
{
	if (g_propsInit) return;
	g_propsInit = true;
	if (!Env("FAKE_PROPS")) return;
	const double fx = -0.34202, fy = 0.93969, rx = 0.93969, ry = 0.34202;
	int h = 9000;
	auto add = [&](double sAl, double o, unsigned m, bool frag, bool att) { FakeProp p{ h++, fx * sAl + rx * o, fy * sAl + ry * o, 20.0, m, frag, att, false }; g_props.push_back(p); };
	for (int k = 0; k < 10; ++k) add(30.0 + 20.0 * k, 4.5, kMLamp, true, false);
	add(45.0, 0.0, kMBin, false, false);
	add(80.0, 0.0, kMLamp, true, false);
	add(100.0, -7.0, kMCont, false, false);
	add(60.0, 2.0, kMCan, false, false);
	add(65.0, 0.0, kMBin, false, true);
	add(55.0, 4.5, kMTraf, true, false);  // v2.51 (handle 9015)
}
// v2.51: a prop model's solid parts (model frame: x0 x1 y0 y1 z0 z1) for object rays (flags 16)
static int FakePropParts(unsigned m, double b[2][6])
{
	auto set = [&](int k, double x0, double x1, double y0, double y1, double z0, double z1) { b[k][0] = x0, b[k][1] = x1, b[k][2] = y0, b[k][3] = y1, b[k][4] = z0, b[k][5] = z1; };
	if (m == kMLamp) { set(0, -0.2, 0.2, -0.2, 0.2, 0, 8); return 1; }
	if (m == kMBin) { set(0, -0.3, 0.3, -0.3, 0.3, 0, 1); return 1; }
	if (m == kMCont) { set(0, -1.2, 1.2, -3, 3, 0, 2.6); return 1; }
	if (m == kMCan) { set(0, -0.04, 0.04, -0.04, 0.04, 0, 0.12); return 1; }
	if (m == kMTraf) { set(0, -0.2, 0.2, -0.2, 0.2, 0, 9.5); set(1, -8.3, 0, -0.15, 0.15, 6, 6.5); return 2; }
	return 0;
}
static double FakeGround(double x, double y, bool* hit, unsigned* mat);
// a ray (world) against the props: the first one it meets
static bool FakeObjRay(const double a[3], const double b[3], int* ent, double p[3])
{
	InitProps();
	double best = 2.0;
	for (const FakeProp& fp : g_props) {
		bool         gh;
		unsigned     mt;
		const double gz = FakeGround(fp.x, fp.y, &gh, &mt);
		const double an = fp.yaw * 3.14159265358979 / 180.0, rt[2] = { cos(an), sin(an) }, fw[2] = { -sin(an), cos(an) };
		double       la[3], lb[3];
		for (int q = 0; q < 2; ++q) {
			const double* P = q ? b : a;
			double*       L = q ? lb : la;
			L[0] = (P[0] - fp.x) * rt[0] + (P[1] - fp.y) * rt[1];
			L[1] = (P[0] - fp.x) * fw[0] + (P[1] - fp.y) * fw[1];
			L[2] = P[2] - gz;
		}
		double bx[2][6];
		const int np = FakePropParts(fp.model, bx);
		for (int k = 0; k < np; ++k) {
			// (like GTA's: a ray that starts inside a solid doesn't see it)
			double tin = -1e30, tout = 1;
			bool   miss = false;
			for (int c = 0; c < 3 && !miss; ++c) {
				const double d = lb[c] - la[c], lo = bx[k][c * 2], hi = bx[k][c * 2 + 1];
				if (fabs(d) < 1e-12) {
					if (la[c] < lo || la[c] > hi) miss = true;
					continue;
				}
				double t0 = (lo - la[c]) / d, t1 = (hi - la[c]) / d;
				if (t0 > t1) std::swap(t0, t1);
				tin = std::max(tin, t0);
				tout = std::min(tout, t1);
			}
			if (miss || tin < 0 || tin > tout || tin >= best) continue;
			best = tin;
			*ent = fp.h;
			for (int c = 0; c < 3; ++c) p[c] = a[c] + (b[c] - a[c]) * tin;
		}
	}
	return best <= 1.0;
}
struct FakeObjHit { int h; bool hit; int ent; double p[3]; };
static std::vector<FakeObjHit> g_objRays;
static FakeProp* PropOf(int h)
{
	InitProps();
	for (auto& p : g_props) if (p.h == h) return &p;
	return nullptr;
}

static bool FakeRayHit(double x, double y, double z0, double z1, double* z, unsigned* mat)
{
	bool         gh;
	const double g = FakeGround(x, y, &gh, mat);
	// v2.5: a wall's top (only from above: a ray starting inside one doesn't see it)
	{
		const int wi = WallAt(x, y);
		if (wi >= 0) {
			const double top = g + g_walls[wi].h;
			if (z0 >= top && top >= z1) { *z = top; *mat = 0xCDEB5023u; return true; }
		}
	}
	// v2.5: FAKE_BRIDGE=2: an overpass 22 m over the ground across the car's line at y 150..162 (8 m over the 14 m deck): rays
	// from above it see it; rays starting under it don't
	if (BridgeOn() == 2 && y > 150 && y < 162) {
		const double top = g + 22.0;
		if (z0 >= top && top >= z1) { *z = top; *mat = 0x10DD5498u; return true; }
	}
	const double h = RampH(x, y);
	if (h > 0) {
		const double top = g + h;
		if (h <= 0.6) { *z = top; return gh && top <= z0 && top >= z1; }   // the ramp's start: solid
		if (top <= z0 && top >= z1) { *z = top; *mat = 0x10DD5498u; return true; }
		if (z0 > top - 0.6 && z0 < top) { /* inside the deck: not seen */ }
		else if (top <= z0) { *z = top; return false; }
	}
	if ((BridgeOn() == 1 || BridgeOn() == 2) && y > 40 && y < 52) {
		const double top = 1.5 * sin(46.0 / 25.0) + 6.0;
		if (top <= z0 && top >= z1) { *z = top; *mat = 0x10DD5498u; return true; }
	}
	*z = g;
	return gh && g <= z0 && g >= z1;
}
static double NowS() { if (!g_qf.QuadPart) QueryPerformanceFrequency(&g_qf); LARGE_INTEGER t; QueryPerformanceCounter(&t); return double(t.QuadPart) / double(g_qf.QuadPart); }
static void SetBasisEuler2(double pitchD, double rollD, double yawD)
{
	const double k = 3.14159265358979 / 180.0;
	const double cp = cos(pitchD * k), sp = sin(pitchD * k), cy = cos(yawD * k), sy = sin(yawD * k), cr = cos(rollD * k), sr = sin(rollD * k);
	const double r[3] = { cy * cr - sy * sp * sr, sy * cr + cy * sp * sr, -cp * sr };
	const double f[3] = { -sy * cp, cy * cp, sp };
	const double u[3] = { cy * sr + sy * sp * cr, sy * sr - cy * sp * cr, cp * cr };
	for (int i = 0; i < 3; ++i) { g_rot[i][0] = r[i]; g_rot[i][1] = f[i]; g_rot[i][2] = u[i]; }
}
static void InitRot() { if (!g_rotInit) { SetBasisEuler2(0, 0, 20.0); g_rotInit = true; } }
static void PuppetLog(const char* what)
{
	if (!g_pl) g_pl = fopen("fake_puppet.csv", "w");
	if (!g_pl) return;
	fprintf(g_pl, "%.4f,%s,%.4f,%.4f,%.4f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.3f,%.3f,%.3f\n", NowS(), what, g_pos[0], g_pos[1], g_pos[2], g_rot[0][1], g_rot[1][1], g_rot[2][1],
		g_rot[0][2], g_rot[1][2], g_rot[2][2], g_vel[0], g_vel[1], g_vel[2]);
	fflush(g_pl);
}
static void Advance()
{
	const ULONGLONG now = GetTickCount64();
	const double dt = g_lastMs ? (now - g_lastMs) / 1000.0 : 0.0;
	g_lastMs = now;
	g_dist += g_speed * dt;
	g_rearExtra += (g_burnout ? 10.0 : 0.0) * dt;
}
static bool Env(const char* n) { char e[16]; return GetEnvironmentVariableA(n, e, sizeof e) > 0 && e[0] == '1'; }
// M = A*B (3x3)
static void Mul(const double a[3][3], const double b[3][3], double o[3][3])
{
	double t[3][3];
	for (int i = 0; i < 3; ++i) for (int j = 0; j < 3; ++j) t[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
	memcpy(o, t, sizeof t);
}
static void Rz(double a, double m[3][3]) { const double c = cos(a), s = sin(a); double t[3][3] = { { c, -s, 0 }, { s, c, 0 }, { 0, 0, 1 } }; memcpy(m, t, sizeof t); }
static void Rx(double a, double m[3][3]) { const double c = cos(a), s = sin(a); double t[3][3] = { { 1, 0, 0 }, { 0, c, -s }, { 0, s, c } }; memcpy(m, t, sizeof t); }
static const double kHub[4][2] = { { -0.8, 1.45 }, { 0.8, 1.45 }, { -0.8, -1.4 }, { 0.8, -1.4 } };  // lf rf lr rr

// v2.01: report the controls switched off this tick (when the set changes) and the text lines drawn
static void TickEnd()
{
	if (memcmp(g_dis, g_disPrev, sizeof g_dis)) {
		std::string s;
		int n = 0;
		for (int i = 0; i < 512; ++i) if (g_dis[i]) { s += " " + std::to_string(i); ++n; }
		printf("DISABLED (%d):%s\n", n, s.c_str());
		fflush(stdout);
		memcpy(g_disPrev, g_dis, sizeof g_dis);
	}
	memset(g_dis, 0, sizeof g_dis);
	const double t = NowS();
	if (g_hud != g_hudPrev && t - g_hudT > 0.25) {
		FILE* f = fopen("fake_hud.txt", "a");
		if (f) {
			fprintf(f, "--- %.3f\n", t);
			for (auto& l : g_hud) fprintf(f, "%s\n", l.c_str());
			fclose(f);
		}
		g_hudPrev = g_hud;
		g_hudT = t;
	}
	g_hud.clear();
}

struct NV { alignas(8) float x; alignas(8) float y; alignas(8) float z; };

#define EXP extern "C" __declspec(dllexport)
EXP void presentCallbackRegister(void (*cb)(void*)) __asm__("?presentCallbackRegister@@YAXP6AXPEAX@Z@Z");
EXP void presentCallbackUnregister(void (*cb)(void*)) __asm__("?presentCallbackUnregister@@YAXP6AXPEAX@Z@Z");
EXP void scriptWait(DWORD) __asm__("?scriptWait@@YAXK@Z");
EXP void scriptRegister(HMODULE, void (*)()) __asm__("?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z");
EXP void scriptUnregister(HMODULE) __asm__("?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z");
EXP void nativeInit(UINT64) __asm__("?nativeInit@@YAX_K@Z");
EXP void nativePush64(UINT64) __asm__("?nativePush64@@YAX_K@Z");
EXP PUINT64 nativeCall() __asm__("?nativeCall@@YAPEA_KXZ");
EXP int worldGetAllObjects(int* arr, int arrSize) __asm__("?worldGetAllObjects@@YAHPEAHH@Z");   // v2.5
int worldGetAllObjects(int* arr, int arrSize)
{
	InitProps();
	int n = 0;
	for (auto& p : g_props) if (n < arrSize) arr[n++] = p.h;
	return n;
}

// v2.7: FAKE_TRAFFIC=1: a parked GTA car (9001) put 25 m ahead of the car, across its way, the first time drive mode asks for
// GTA's cars; what the plugin does to it (velocity, damage) is printed
static bool   g_tcPlaced = false;
static double g_tc[3] = {}, g_tcYaw = 0, g_tcV[3] = {};
static double FakeGround(double x, double y, bool* hit, unsigned* mat);
EXP int worldGetAllVehicles(int* arr, int arrSize) __asm__("?worldGetAllVehicles@@YAHPEAHH@Z");   // v2.7
int worldGetAllVehicles(int* arr, int arrSize)
{
	int n = 0;
	if (n < arrSize) arr[n++] = 100;   // (the player's car)
	if (!Env("FAKE_TRAFFIC")) return n;
	if (!g_tcPlaced) {
		g_tcPlaced = true;
		bool gh; unsigned m;
		g_tc[0] = g_pos[0] + g_rot[0][1] * 25.0;
		g_tc[1] = g_pos[1] + g_rot[1][1] * 25.0;
		g_tc[2] = FakeGround(g_tc[0], g_tc[1], &gh, &m) + 0.6;
		g_tcYaw = g_yawDeg + 90.0;
		printf("FAKE: traffic car 9001 parked at %.1f %.1f %.1f\n", g_tc[0], g_tc[1], g_tc[2]); fflush(stdout);
	}
	if (n < arrSize) arr[n++] = 9001;
	return n;
}
EXP int worldGetAllPeds(int* arr, int arrSize) __asm__("?worldGetAllPeds@@YAHPEAHH@Z");   // v2.6
int worldGetAllPeds(int* arr, int arrSize)
{
	int  n = 0;
	bool me = false;
	for (auto& p : g_peds) me = me || p.h == 1;
	if (!me && n < arrSize) arr[n++] = 1;   // (you)
	for (auto& p : g_peds) if (n < arrSize) arr[n++] = p.h;
	return n;
}
static FakePed* PedOf(int h)
{
	for (auto& p : g_peds) if (p.h == h) return &p;
	return nullptr;
}
// a point in the GTA car's frame (right, forward, up from its position)
static void CarPt(double r, double f, double u, double o[3]) { for (int i = 0; i < 3; ++i) o[i] = g_pos[i] + g_rot[i][0] * r + g_rot[i][1] * f + g_rot[i][2] * u; }
static void FootSchedule()
{
	if (!Env("FAKE_FOOT") || g_footDone) return;
	const double now = NowS();
	if (g_fT0 < 0) {
		if (GetFileAttributesA("fake_drive_on.txt") == INVALID_FILE_ATTRIBUTES) return;
		g_fT0 = now + 2.0;
		return;
	}
	if (now >= g_fT0 && !g_footOut && now < g_fT0 + 7.0) {
		g_footOut = true;
		FakePed p{};
		p.h = 1;
		CarPt(-4.0, 0.0, 1.0, p.head);
		CarPt(1.5, 0.0, 0.0, p.tgt);
		if (Env("FAKE_SHATTER")) {  // v2.63: you stand 6 m ahead of the car and shoot its left headlight
			CarPt(-0.7, 8.0, -0.05, p.head);
			CarPt(-0.7, 0.0, -0.05, p.tgt);
		}
		p.t0 = g_fT0 + 1.5; p.every = 0.5; p.until = g_fT0 + 3.9; p.last = -100;
		g_peds.push_back(p);
		printf("FAKE: you get out of the car and stand at %.2f %.2f %.2f\n", p.head[0], p.head[1], p.head[2] - 1.6); fflush(stdout);
	}
	// v2.63: FAKE_RPG=1: at +5 s your rocket hits the car (a type 4 explosion 1.2 m from its middle, and a type 7 at the car the
	// way GTA blows up the car it hits)
	static bool rpg = false;
	if (Env("FAKE_RPG") && !rpg && now >= g_fT0 + 5.0) {
		rpg = true;
		FakeExp e{ 4, {}, now, now + 0.3 };
		CarPt(-1.0, 0.5, 0.6, e.p);
		g_exps.push_back(e);
		FakeExp e7{ 7, { g_pos[0], g_pos[1], g_pos[2] + 0.4 }, now, now + 0.5 };
		g_exps.push_back(e7);
		printf("FAKE: a rocket hits the car at %.2f %.2f %.2f\n", e.p[0], e.p[1], e.p[2]); fflush(stdout);
	}
	if (now >= g_fT0 + 7.0 && g_footOut) {
		g_footOut = false;
		g_peds.erase(std::remove_if(g_peds.begin(), g_peds.end(), [](const FakePed& q) { return q.h == 1; }), g_peds.end());
		printf("FAKE: you get back in\n"); fflush(stdout);
	}
	if (now >= g_fT0 + 12.0) {
		g_footDone = true;
		FILE* f = fopen("fake_foot_done.txt", "w");
		if (f) fclose(f);
		printf("FAKE: foot test done\n"); fflush(stdout);
	}
}
static void WeaponsSchedule()
{
	FootSchedule();
	for (auto& p : g_peds)   // (your shots on foot)
		if (p.h == 1 && !Env("FAKE_WEAPONS")) {
			const double t = NowS();
			if (t >= p.t0 && t <= p.until && t - p.last >= p.every) { p.last = t; ++p.n; printf("FAKE: ped %d fires (shot %d)\n", p.h, p.n); fflush(stdout); }
		}
	if (!Env("FAKE_WEAPONS") || g_wDone) return;
	const double now = NowS();
	if (g_wT0 < 0) {
		if (GetFileAttributesA("fake_drive_on.txt") == INVALID_FILE_ATTRIBUTES) return;
		g_wT0 = now + 2.0;
		auto add = [&](int h, const double a[3], const double b[3], double t0, double every, double len) {
			FakePed p{};
			p.h = h; memcpy(p.head, a, sizeof(p.head)); memcpy(p.tgt, b, sizeof(p.tgt));
			p.t0 = g_wT0 + t0; p.every = every; p.until = g_wT0 + t0 + len; p.last = -100;
			g_peds.push_back(p);
		};
		double a[3], b[3];
		CarPt(-8.0, 0.0, 0.4, a); CarPt(3.0, 0.0, 0.4, b); add(7000, a, b, 0.0, 0.4, 3.0);           // through the cabin
		CarPt(-8.0, 1.45, -0.5, a); CarPt(2.0, 1.45, -0.5, b); add(7001, a, b, 0.2, 0.5, 3.0);       // the front left tyre's bottom
		CarPt(0.0, 10.0, 0.0, a); CarPt(0.0, -4.0, 0.0, b); add(7002, a, b, 3.5, 0.5, 2.0);          // into the front (radiator, engine)
		printf("FAKE: 3 shooters around the car at %.2f %.2f %.2f (they fire in 2 s)\n", g_pos[0], g_pos[1], g_pos[2]); fflush(stdout);
		return;
	}
	for (auto& p : g_peds)
		if (now >= p.t0 && now <= p.until && now - p.last >= p.every) {
			p.last = now;
			++p.n;
			printf("FAKE: ped %d fires (shot %d)\n", p.h, p.n); fflush(stdout);
		}
	if (!g_nadeDone && now >= g_wT0 + 6.0) {
		g_nadeDone = true;
		FakeExp e{ 0, {}, now, now + 0.3 };
		CarPt(0.0, -3.0, -0.3, e.p);
		g_exps.push_back(e);
		printf("FAKE: a grenade goes off at %.2f %.2f %.2f\n", e.p[0], e.p[1], e.p[2]); fflush(stdout);
	}
	if (now >= g_wT0 + 20.0) {
		g_wDone = true;
		FILE* f = fopen("fake_wpn_done.txt", "w");
		if (f) fclose(f);
		printf("FAKE: weapons done, player health %d, %d fires started\n", g_health, g_fires); fflush(stdout);
	}
}

void presentCallbackRegister(void (*cb)(void*)) { for (auto& p : g_present) if (!p) { p = cb; break; } }
void presentCallbackUnregister(void (*cb)(void*)) { for (auto& p : g_present) if (p == cb) p = nullptr; }
void scriptWait(DWORD)
{
	TickEnd();
	WeaponsSchedule();   // v2.6
	++g_tickN;
	if (g_freezeMs > 0) {
		const int ms = g_freezeMs;
		g_freezeMs = 0;
		printf("FAKE: GTA paused for %d ms (script stopped)\n", ms);
		fflush(stdout);
		Sleep(ms);
	}
	Sleep(15);
	// GTA's physics step between script ticks: an unfrozen car moves with its velocity
	const double t = NowS();
	const double dt = g_physT > 0 ? fmin(0.1, t - g_physT) : 0.0;
	g_physT = t;
	if (dt > 0) g_frameT = dt;
	if (g_velSet && !g_frozen) for (int k = 0; k < 3; ++k) g_pos[k] += g_vel[k] * dt * g_timeScale;
}
void scriptRegister(HMODULE, void (*fn)()) { g_script = fn; }
void scriptUnregister(HMODULE) {}
void nativeInit(UINT64 h) { g_hash = h; g_nargs = 0; memset(g_ret, 0, sizeof(g_ret)); }
void nativePush64(UINT64 v) { if (g_nargs < 32) g_args[g_nargs++] = v; }

static float F(int i) { float f; memcpy(&f, &g_args[i], 4); return f; }
static void  RetV(float x, float y, float z) { NV v{ x, y, z }; memcpy(g_ret, &v, sizeof(v)); }
static void  RetI(int v) { memcpy(g_ret, &v, 4); }
static void  RetF(float v) { memcpy(g_ret, &v, 4); }

// v2.01.1: like Script Hook V, a native it doesn't know ends the game ("FATAL: Can't find native"). The list is
// every hash in the native database (native_hashes.txt next to the host).
static bool KnownNative(UINT64 h)
{
	static std::vector<UINT64> known;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		FILE* f = fopen("native_hashes.txt", "r");
		char  line[64];
		while (f && fgets(line, sizeof line, f)) known.push_back(strtoull(line, nullptr, 16));
		if (f) fclose(f);
		std::sort(known.begin(), known.end());
		printf("FAKE: %zu known natives\n", known.size());
		fflush(stdout);
	}
	return known.empty() || std::binary_search(known.begin(), known.end(), h);
}

PUINT64 nativeCall()
{
	if (!KnownNative(g_hash)) {
		printf("FATAL: Can't find native 0x%016llX\n", (unsigned long long)g_hash);
		fflush(stdout);
		ExitProcess(3);
	}
	const float carZ = 0.6f;
	const double yr = g_yawDeg * 3.14159265358979 / 180.0;
	InitRot();
	{
		static bool startSet = false;  // v2.43: FAKE_STARTX = the car starts this far to the side (another route past the bridge)
		if (!startSet) {
			startSet = true;
			char e[32];
			if (GetEnvironmentVariableA("FAKE_STARTX", e, sizeof e)) g_pos[0] = atof(e);
		}
	}
	(void)carZ;
	switch (g_hash) {
	case 0xD80958FC74E988A6: RetI(1); break;                  // PLAYER_PED_ID
	case 0x9A9112A0FE9A4713: RetI(100); break;                // GET_VEHICLE_PED_IS_IN
	case 0xB6997A7EB3F5C8C0: RetI(100); break;                // GET_PLAYERS_LAST_VEHICLE
	case 0xF73EB622C4F1689B: RetI(100); break;                // GET_CLOSEST_VEHICLE
	case 0x7239B21A38F536BA: RetI(1); break;                  // DOES_ENTITY_EXIST
	case 0x9F47B058362C84B5: { FakeProp* fp = PropOf(int(g_args[0])); RetI(fp ? int(fp->model) : 0x1234); break; }  // GET_ENTITY_MODEL
	case 0x3FEF770D40960D5A: {                                // GET_ENTITY_COORDS (v2.5: props too)
		if (FakePed* fp = PedOf(int(g_args[0]))) { RetV(float(fp->head[0]), float(fp->head[1]), float(fp->head[2] - 0.6)); break; }   // v2.6
		if (int(g_args[0]) == 9001) { RetV(float(g_tc[0]), float(g_tc[1]), float(g_tc[2])); break; }   // v2.7
		if (FakeProp* fp = PropOf(int(g_args[0]))) { bool gh; unsigned m; RetV(float(fp->x), float(fp->y), float(fp->posed ? fp->z : FakeGround(fp->x, fp->y, &gh, &m))); break; }
		RetV(float(g_pos[0]), float(g_pos[1]), float(g_pos[2])); break; }
	case 0xECB2FC7235A7D137: {                                // GET_ENTITY_MATRIX (v2.5: props)
		FakeProp* fp = PropOf(int(g_args[0]));
		NV* fw = reinterpret_cast<NV*>(g_args[1]); NV* rt = reinterpret_cast<NV*>(g_args[2]); NV* up = reinterpret_cast<NV*>(g_args[3]); NV* ps = reinterpret_cast<NV*>(g_args[4]);
		if (int(g_args[0]) == 9001) {   // v2.7: the traffic car
			const double a = g_tcYaw * 3.14159265358979 / 180.0;
			*fw = NV{ float(-sin(a)), float(cos(a)), 0 }; *rt = NV{ float(cos(a)), float(sin(a)), 0 }; *up = NV{ 0, 0, 1 };
			*ps = NV{ float(g_tc[0]), float(g_tc[1]), float(g_tc[2]) };
		} else if (fp && fp->posed) {  // v2.52: put there by the plugin (a physics prop copying BeamNG's box)
			*rt = NV{ float(fp->R[0][0]), float(fp->R[1][0]), float(fp->R[2][0]) }; *fw = NV{ float(fp->R[0][1]), float(fp->R[1][1]), float(fp->R[2][1]) };
			*up = NV{ float(fp->R[0][2]), float(fp->R[1][2]), float(fp->R[2][2]) }; *ps = NV{ float(fp->x), float(fp->y), float(fp->z) };
		} else if (fp) {
			const double a = fp->yaw * 3.14159265358979 / 180.0;
			bool gh; unsigned m;
			*fw = NV{ float(-sin(a)), float(cos(a)), 0 }; *rt = NV{ float(cos(a)), float(sin(a)), 0 }; *up = NV{ 0, 0, 1 };
			*ps = NV{ float(fp->x), float(fp->y), float(FakeGround(fp->x, fp->y, &gh, &m)) };
		}
		break; }
	case 0x0C112765300C7E1E: { FakeProp* fp = PropOf(int(g_args[0])); RetI(fp && fp->frag ? 1 : 0); break; }      // GET_IS_ENTITY_A_FRAG
	case 0xB346476EF1A64897: { FakeProp* fp = PropOf(int(g_args[0])); RetI(fp && fp->attached ? 1 : 0); break; }  // IS_ENTITY_ATTACHED
	case 0x47D6F43D77935C75: RetI(1); break;                                                                      // IS_ENTITY_VISIBLE
	case 0x1718DE8E3F2823CA: { printf("FAKE: entity %d dynamic %d\n", int(g_args[0]), int(g_args[1])); fflush(stdout); break; }  // SET_ENTITY_DYNAMIC
	case 0x710311ADF0E20730: break;                                                                               // ACTIVATE_PHYSICS
	case 0xC5F68BE9613E2D18: {                                // APPLY_FORCE_TO_ENTITY: the prop flies off (moved 30 m away)
		FakeProp* fp = PropOf(int(g_args[0]));
		printf("FAKE: prop %d pushed (type %d, %.1f %.1f %.1f)\n", int(g_args[0]), int(g_args[1]), F(2), F(3), F(4)); fflush(stdout);
		if (fp) { fp->pushed = true; fp->x += 100.0; }   // flown off the road
		break; }
	case 0x1899F328B0E12848: {                                // GET_OFFSET_FROM_ENTITY_IN_WORLD_COORDS
		const double l[3] = { F(1), F(2), F(3) };
		double w[3];
		for (int i = 0; i < 3; ++i) w[i] = g_pos[i] + g_rot[i][0] * l[0] + g_rot[i][1] * l[1] + g_rot[i][2] * l[2];
		RetV(float(w[0]), float(w[1]), float(w[2]));
		break;
	}
	// v2.0 drive mode natives
	case 0x239A3351AC1DA385:                                  // SET_ENTITY_COORDS_NO_OFFSET (v2.52: props too)
		if (FakeProp* fp = PropOf(int(g_args[0]))) {
			if (!fp->posed) { const double a = fp->yaw * 3.14159265358979 / 180.0; const double R[3][3] = { { cos(a), -sin(a), 0 }, { sin(a), cos(a), 0 }, { 0, 0, 1 } }; memcpy(fp->R, R, sizeof(R)); }
			fp->posed = true; fp->x = F(1); fp->y = F(2); fp->z = F(3); ++fp->poses;
			if (fp->poses <= 3 || fp->poses % 20 == 0) { printf("FAKE: prop %d posed %.2f %.2f %.2f (%d, frozen %d)\n", fp->h, fp->x, fp->y, fp->z, fp->poses, int(fp->frozen)); fflush(stdout); }
			break;
		}
		g_pos[0] = F(1); g_pos[1] = F(2); g_pos[2] = F(3); PuppetLog("coords"); break;
	case 0x8524A8B0171D5E07:                                  // SET_ENTITY_ROTATION (v2.52: props too)
		if (FakeProp* fp = PropOf(int(g_args[0]))) {
			double save[3][3]; memcpy(save, g_rot, sizeof(save));
			SetBasisEuler2(F(1), F(2), F(3));
			memcpy(fp->R, g_rot, sizeof(fp->R)); memcpy(g_rot, save, sizeof(save));
			if (fp->poses <= 3) { printf("FAKE: prop %d turned: up %.2f %.2f %.2f\n", fp->h, fp->R[0][2], fp->R[1][2], fp->R[2][2]); fflush(stdout); }
			break;
		}
		SetBasisEuler2(F(1), F(2), F(3) + (Env("FAKE_ROTWRONG") ? 30.0f : 0.0f)); PuppetLog("rot"); break;
	case 0x77B21BE7AC540F07: {                                // SET_ENTITY_QUATERNION x y z w
		const double x = F(1), y = F(2), z = F(3), w = F(4);
		const double M[3][3] = { { 1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w) },
		                         { 2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w) },
		                         { 2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y) } };
		memcpy(g_rot, M, sizeof(M));
		PuppetLog("quat");
		break;
	}
	case 0x1C99BB7B6E96D16F:  // SET_ENTITY_VELOCITY (v2.7: the traffic car's printed)
		if (int(g_args[0]) == 9001) {
			g_tcV[0] = F(1); g_tcV[1] = F(2); g_tcV[2] = F(3);
			printf("FAKE: traffic car velocity set %.2f %.2f %.2f\n", g_tcV[0], g_tcV[1], g_tcV[2]); fflush(stdout);
			break;
		}
		g_vel[0] = F(1); g_vel[1] = F(2); g_vel[2] = F(3); g_velSet = true; break;
	case 0x4805D2B1D8CF94A9: if (int(g_args[0]) == 9001) RetV(float(g_tcV[0]), float(g_tcV[1]), float(g_tcV[2])); break;   // GET_ENTITY_VELOCITY (v2.7: the traffic car)
	case 0xA1DD317EA8FD4F29: printf("FAKE: vehicle %d damaged at %.2f %.2f %.2f by %.0f\n", int(g_args[0]), F(1), F(2), F(3), F(4)); fflush(stdout); break;   // SET_VEHICLE_DAMAGE (v2.7)
	case 0x1A9205C1B9EE827F: g_collision = g_args[1] != 0; printf("FAKE: collision %d\n", int(g_collision)); fflush(stdout); break;
	case 0x4A4722448F18EEF5: g_gravity = g_args[1] != 0; printf("FAKE: gravity %d\n", int(g_gravity)); fflush(stdout); break;
	case 0x428CA6DBD1094446:                                  // FREEZE_ENTITY_POSITION (v2.52: props too)
		if (FakeProp* fp = PropOf(int(g_args[0]))) { fp->frozen = g_args[1] != 0; printf("FAKE: prop %d frozen %d\n", fp->h, int(fp->frozen)); fflush(stdout); break; }
		g_frozen = g_args[1] != 0; printf("FAKE: frozen %d\n", int(g_frozen)); fflush(stdout); break;
	case 0xFE99B66D079CF6BC: {                                // DISABLE_CONTROL_ACTION (v2.01: recorded)
		const int id = int(g_args[1]);
		if (id >= 0 && id < 512) g_dis[id] = true;
		break;
	}
	case 0x11E65974A982637C: {                                // GET_DISABLED_CONTROL_NORMAL
		float in[4] = { 0, 0, 0, 0 };
		char  e[128];
		if (GetEnvironmentVariableA("FAKE_INPUT", e, sizeof e)) sscanf(e, "%f %f %f %f", in, in + 1, in + 2, in + 3);
		const int c = int(g_args[1]);
		RetF(c == 71 ? in[0] : c == 72 ? in[1] : c == 59 ? in[2] : c == 76 ? in[3] : 0.0f);
		break;
	}
	case 0xA571D46727E2B718: RetI(Env("FAKE_KBM") ? 1 : 0); break;  // IS_USING_KEYBOARD_AND_MOUSE (v2.01: pad player unless FAKE_KBM=1)
	case 0xC906A7DAB05C8D2B: { unsigned m; double gz = 0; const bool gh = FakeRayHit(F(0), F(1), F(2), -1000.0, &gz, &m); *reinterpret_cast<float*>(g_args[3]) = float(gh ? gz : 0.0); RetI(gh ? 1 : 0); break; }  // GET_GROUND_Z_FOR_3D_COORD (v2.4: the fake ground; v2.43: the first surface under z)
	case 0x03E8D3D5F549087A: {                                // GET_MODEL_DIMENSIONS
		NV* mn = reinterpret_cast<NV*>(g_args[1]);
		NV* mx = reinterpret_cast<NV*>(g_args[2]);
		const unsigned md = unsigned(g_args[0]);
		if (md == kMLamp) { *mn = NV{ -0.2f, -0.2f, 0.0f }; *mx = NV{ 0.2f, 0.2f, 8.0f }; break; }
		if (md == kMBin) { *mn = NV{ -0.3f, -0.3f, 0.0f }; *mx = NV{ 0.3f, 0.3f, 1.0f }; break; }
		if (md == kMCont) { *mn = NV{ -1.2f, -3.0f, 0.0f }; *mx = NV{ 1.2f, 3.0f, 2.6f }; break; }
		if (md == kMCan) { *mn = NV{ -0.04f, -0.04f, 0.0f }; *mx = NV{ 0.04f, 0.04f, 0.12f }; break; }
		if (md == kMTraf) { *mn = NV{ -8.3f, -0.3f, 0.0f }; *mx = NV{ 0.3f, 0.3f, 9.5f }; break; }
		*mn = NV{ -1.0f, -2.5f, -0.6f };
		*mx = NV{ 1.0f, 2.5f, 0.8f };
		break;
	}
	case 0xB215AAC32D25D019: { static const char n[] = "TESTCAR"; const char* p = n; memcpy(g_ret, &p, 8); break; }
	case 0x9A8D700A51CB7B0D:                                  // GET_ENTITY_SPEED_VECTOR (relative)
		if (g_velSet) {
			double l[3];
			for (int j = 0; j < 3; ++j) l[j] = g_rot[0][j] * g_vel[0] + g_rot[1][j] * g_vel[1] + g_rot[2][j] * g_vel[2];
			RetV(float(l[0]), float(l[1]), float(l[2]));
		} else {
			RetV(0, float(g_speed), 0);
		}
		break;
	case 0xFB71170B7E76ACBA: {                                // GET_ENTITY_BONE_INDEX_BY_NAME
		const char* n = reinterpret_cast<const char*>(g_args[1]);
		const char* names[4] = { "wheel_lf", "wheel_rf", "wheel_lr", "wheel_rr" };
		int idx = -1;
		for (int i = 0; i < 4; ++i) if (!Env("FAKE_NOBONES") && n && !strcmp(n, names[i])) idx = 10 + i;
		RetI(idx);
		break;
	}
	case 0xCE6294A232D03786: {                                // GET_ENTITY_BONE_ROTATION (degrees, ZXY)
		Advance();
		const int w = int(g_args[1]) - 10;
		const bool left = (w & 1) == 0, front = w < 2;
		const double r = 0.33, dist = g_dist + (front ? 0.0 : g_rearExtra);
		double M[3][3], T[3][3];
		Rz(Env("FAKE_LOCALBONES") ? 0.0 : yr, M);
		Rz(front ? -10.0 * 3.14159265358979 / 180.0 : 0.0, T); Mul(M, T, M);
		if (left) { Rz(3.14159265358979, T); Mul(M, T, M); }
		Rx((left ? 1.0 : -1.0) * dist / r, T); Mul(M, T, M);
		// ZXY: M = Rz(z) Rx(x) Ry(y)
		const double x = asin(fmax(-1.0, fmin(1.0, M[2][1])));
		const double y = atan2(-M[2][0], M[2][2]);
		const double z = atan2(-M[0][1], M[1][1]);
		const double k = 180.0 / 3.14159265358979;
		RetV(float(x * k), float(y * k), float(z * k));
		break;
	}
	case 0x44A8FCB8ED227738: {                                // GET_WORLD_POSITION_OF_ENTITY_BONE
		const int w = int(g_args[1]) - 10;
		const double hx = kHub[w & 3][0], hy = kHub[w & 3][1], c = cos(yr), s = sin(yr);
		RetV(float(hx * c - hy * s), float(hx * s + hy * c), 0.33f);
		break;
	}
	case 0xBAF107B6BB2C97F0: RetF(1.0f); break;               // GET_SAFE_ZONE_SIZE
	case 0xF1307EF624A80D87: RetF(1280.0f / 720.0f); break;   // GET_ASPECT_RATIO
	case 0xA86478C6958735C5: RetI(0); break;                  // IS_HUD_HIDDEN
	case 0x157F93B036700462: RetI(0); break;                  // IS_RADAR_HIDDEN
	case 0x9EB6522EA68F22FE: RetI(1); break;                  // IS_RADAR_PREFERENCE_SWITCHED_ON
	case 0xAF754F20EB5CD51A: RetI(1); break;                  // IS_MINIMAP_RENDERING
	case 0xBB40DD2270B65366: RetI(g_footOut ? 0 : 1); break;  // GET_PED_IN_VEHICLE_SEAT -> the player (v2.61: not while out)
	case 0xEC3C9B8D5327B563: {                                // GET_CONTROL_NORMAL
		const int c = int(g_args[1]);
		RetF(c == 59 ? 0.5f : c == 71 ? 1.0f : 0.0f);
		break;
	}
	case 0x1297A88E081430EB: RetI(g_burnout); break;          // IS_VEHICLE_IN_BURNOUT
	case 0x25223CA6B4D20B7F: RetI(13); break;                 // GET_CLOCK_HOURS
	case 0x13D2B8ADD79640F2: RetI(0); break;
	case 0xA200EB1EE790F448: { float c[5] = { 5.5f, 3.0f, 1.4f, -9.0f, 118.0f }; char e[128]; if (GetEnvironmentVariableA("FAKE_CAM", e, sizeof e)) sscanf(e, "%f %f %f %f %f", c, c + 1, c + 2, c + 3, c + 4); RetV(c[0], c[1], c[2]); break; }
	case 0x5B4E4C817FCC2DFB: { float c[5] = { 5.5f, 3.0f, 1.4f, -9.0f, 118.0f }; char e[128]; if (GetEnvironmentVariableA("FAKE_CAM", e, sizeof e)) sscanf(e, "%f %f %f %f %f", c, c + 1, c + 2, c + 3, c + 4); RetV(c[3], 0.0f, c[4]); break; }
	case 0x14D6F5678D8F1B37: { float c[5] = { 5.5f, 3.0f, 1.4f, -9.0f, 118.0f }; char e[128]; if (GetEnvironmentVariableA("FAKE_CAM", e, sizeof e)) sscanf(e, "%f %f %f %f %f", c, c + 1, c + 2, c + 3, c + 4); RetV(c[0], c[1], c[2]); break; }  // GET_GAMEPLAY_CAM_COORD (v1.9.4)
	case 0x837765A25378F0BB: { float c[5] = { 5.5f, 3.0f, 1.4f, -9.0f, 118.0f }; char e[128]; if (GetEnvironmentVariableA("FAKE_CAM", e, sizeof e)) sscanf(e, "%f %f %f %f %f", c, c + 1, c + 2, c + 3, c + 4); RetV(c[3], 0.0f, c[4]); break; }  // GET_GAMEPLAY_CAM_ROT
	case 0x15C40837039FFAF7: RetF(float(g_frameT * g_timeScale)); break;    // GET_FRAME_TIME (v2.5: x GTA's time scale: slow motion)
	case 0xFC8202EFC642E6F2: { static int n = 0; RetI(++n); break; }  // GET_FRAME_COUNT (v1.9.5)
	case 0x9CD27B0045628463: RetI(int(GetTickCount64() & 0x7fffffff)); break;  // GET_GAME_TIMER
	case 0x80EC114669DAEFF4: RetF(50.0f); break;
	case 0xD0082607100D7193: RetF(0.1f); break;
	case 0xDFC8CBC606FDB0FC: RetF(2000.0f); break;
	case 0xB0034A223497FFCB: RetI(g_paused); break;          // IS_PAUSE_MENU_ACTIVE (v2.01: host "pause 1")
	case 0xEF01D36B9C9D0C7B: g_paused = 1; printf("FAKE: pause menu opened (menu 0x%08X)\n", unsigned(g_args[0])); fflush(stdout); break;  // ACTIVATE_FRONTEND_MENU (v2.43)
	case 0x745711A75AB09277: if (!g_args[0]) g_paused = 0; printf("FAKE: frontend active %d\n", int(g_args[0])); fflush(stdout); break;     // SET_FRONTEND_ACTIVE (v2.43)
	case 0x377906D8A31E5586: {                                // synchronous shape test
		if (int(g_args[6]) == 16) {
			// v2.51: props only (the plugin measures what of a prop a car can hit)
			const double a[3] = { F(0), F(1), F(2) }, b[3] = { F(3), F(4), F(5) };
			FakeObjHit o{ g_rayNext++, false, 0, {} };
			o.hit = FakeObjRay(a, b, &o.ent, o.p);
			g_objRays.push_back(o);
			if (g_objRays.size() > 512) g_objRays.erase(g_objRays.begin());
			RetI(o.h);
			break;
		}
		if (int(g_args[6]) == 1) {
			// v2.41: map-only rays (the ground streaming's immediate rays, the landing): answered at once, with a cost
			static int us = -1;
			if (us < 0) { char e[16]; us = GetEnvironmentVariableA("FAKE_SYNCUS", e, sizeof e) ? atoi(e) : 40; }
			const double t0 = NowS();
			while ((NowS() - t0) * 1e6 < us) {}
			FakeRay r{ g_rayNext++, F(0), F(1), F(2), F(3), F(4), F(5), g_tickN, true };
			g_rays.push_back(r);
			++g_raySync;
			RetI(r.h);
			break;
		}
		g_rayDown = F(5) < F(2); RetI(7); break;  // shape test handle (rays going up = towards the sun: open sky)
	}
	case 0x7EE9F5D83DD4F90E: {                                // async shape test
		const int flags = int(g_args[6]);
		if (flags & 256) { g_rayDown = F(5) < F(2); RetI(8); break; }   // v1.9.3 sun rays (with foliage)
		// v2.4: ground rays (and the ride check): own handle, answered a tick later; a full pool refuses (0)
		static int cap = -1;
		if (cap < 0) { char e[16]; cap = GetEnvironmentVariableA("FAKE_RAYCAP", e, sizeof e) ? atoi(e) : 300; }
		if (int(g_rays.size()) >= cap) { ++g_rayRefused; RetI(0); break; }
		FakeRay r{ g_rayNext++, F(0), F(1), F(2), F(3), F(4), F(5), g_tickN, false };
		g_rays.push_back(r);
		++g_rayStarted;
		RetI(r.h);
		break;
	}
	case 0x65287525D951F6BE: {                                // GET_SHAPE_TEST_RESULT_INCLUDING_MATERIAL (v2.4)
		const int h = int(g_args[0]);
		int* hit = reinterpret_cast<int*>(g_args[1]);
		NV*  end = reinterpret_cast<NV*>(g_args[2]);
		unsigned long long* mat = reinterpret_cast<unsigned long long*>(g_args[4]);
		size_t i = 0;
		while (i < g_rays.size() && g_rays[i].h != h) ++i;
		if (i == g_rays.size()) { RetI(0); break; }
		if (g_rays[i].tick == g_tickN && !g_rays[i].now) { RetI(1); break; }   // not ready yet
		const FakeRay r = g_rays[i];
		g_rays.erase(g_rays.begin() + i);
		NV* nrm = reinterpret_cast<NV*>(g_args[3]);
		if (fabs(r.x1 - r.x0) > 0.05 || fabs(r.y1 - r.y0) > 0.05) {
			// v2.5: a flat ray (the wall scan)
			double hp[3], hn[3];
			const bool h2 = FakeHRay(r.x0, r.y0, r.z0, r.x1, r.y1, r.z1, hp, hn);
			*hit = h2 ? 1 : 0;
			*end = h2 ? NV{ float(hp[0]), float(hp[1]), float(hp[2]) } : NV{ r.x1, r.y1, r.z1 };
			if (nrm) *nrm = h2 ? NV{ float(hn[0]), float(hn[1]), float(hn[2]) } : NV{ 0, 0, 0 };
			*mat = h2 ? 0xCDEB5023u : 0;
			++g_hRays;
			RetI(2);
			break;
		}
		unsigned m = 0;
		double   gz = 0;
		const bool inside = FakeRayHit(r.x0, r.y0, r.z0, r.z1, &gz, &m);   // v2.43: bridges
		*hit = inside ? 1 : 0;
		*end = NV{ r.x0, r.y0, float(gz) };
		if (nrm) *nrm = NV{ 0, 0, 1 };
		*mat = inside ? m : 0;
		RetI(2);
		break;
	}
	case 0x07503F7948F491A7: break;                           // REQUEST_COLLISION_AT_COORD
	case 0xD5037BA82E12416F: RetF(float(g_velSet ? sqrt(g_vel[0] * g_vel[0] + g_vel[1] * g_vel[1] + g_vel[2] * g_vel[2]) : g_speed)); break;  // GET_ENTITY_SPEED
	case 0xE8AF77C4C06ADC93: RetI(Radio()); break;  // GET_PLAYER_RADIO_STATION_INDEX
	case 0xF6D733C32076AD03: { static const char* off = "OFF"; static const char* on = "RADIO_01_CLASS_ROCK";
		const char* p = Radio() != 255 ? on : off; memcpy(g_ret, &p, 8); break; }
	case 0xC69EDA28699D5107: {  // SET_RADIO_TO_STATION_NAME
		const char* n = reinterpret_cast<const char*>(g_args[0]);
		Radio();
		g_radioIdx = !strcmp(n, "OFF") ? 255 : 1;
		printf("FAKE: radio station %s\n", n); fflush(stdout); break; }
	case 0x1B9C0099CB942AC6: {  // SET_VEH_RADIO_STATION (v2.41)
		const char* n = reinterpret_cast<const char*>(g_args[1]);
		Radio();
		g_radioIdx = !strcmp(n, "OFF") ? 255 : 1;
		printf("FAKE: car radio %s\n", n); fflush(stdout); break; }
	case 0xBF286C554784F3DF: {  // SET_MOBILE_PHONE_RADIO_STATE: the phone radio starts a station by itself (FAKE_PHONESTARTS=1)
		Radio();
		printf("FAKE: phone radio %d\n", int(g_args[0])); fflush(stdout);
		if (g_args[0] && Env("FAKE_PHONESTARTS")) g_radioIdx = 1;
		break; }
	case 0xE9676F61BC0B3321: {  // HAS_COLLISION_LOADED_AROUND_ENTITY (v2.41)
		static int wait = -1;
		if (wait < 0) { char e[16]; wait = GetEnvironmentVariableA("FAKE_COLLWAIT", e, sizeof e) ? atoi(e) : 0; }
		RetI(++g_collCalls > wait ? 1 : 0); break; }
	case 0xD05A3241B9A86F19: break;                                     // SET_ENTITY_FLAG_SUPPRESS_SHADOW
	case 0xA3EE4A07279BB9DB: RetI(g_footOut ? 0 : 1); break;            // IS_PED_IN_VEHICLE (v2.61: not while out)
	case 0xEA1C610A04DB6BBB: if (int(g_args[0]) == 1) { printf("FAKE: player visible %d\n", int(g_args[1])); fflush(stdout); } break;  // SET_ENTITY_VISIBLE
	case 0x9B1E824FFBB7027A: if (int(g_args[0]) == 1) { printf("FAKE: player alpha reset\n"); fflush(stdout); } break;              // RESET_ENTITY_ALPHA
	case 0xAD738C3085FE7E11: printf("FAKE: entity %d is a mission entity\n", int(g_args[0])); fflush(stdout); break;                 // SET_ENTITY_AS_MISSION_ENTITY
	case 0xB80D8756B4668AB6: break;                                                                                                   // REQUEST_NAMED_PTFX_ASSET
	case 0x8702416E512EC454: RetI(1); break;                                                                                          // HAS_NAMED_PTFX_ASSET_LOADED
	case 0x6C38AF3693A69A91: break;                                                                                                   // USE_PARTICLE_FX_ASSET
	case 0x1AE42C1660FD6517: {                                // START_PARTICLE_FX_LOOPED_ON_ENTITY: the first name unknown here
		const char* n = reinterpret_cast<const char*>(g_args[0]);
		const bool  ok = n && strcmp(n, "fire_wrecked_plane_cockpit") != 0;
		printf("FAKE: particle fx %s on entity %d at %.2f %.2f %.2f scale %.2f: %s\n", n ? n : "?", int(g_args[1]), F(2), F(3), F(4), F(8), ok ? "started" : "unknown"); fflush(stdout);
		RetI(ok ? g_fxNext++ : 0);
		break; }
	case 0x74AFEF0D2E1E409B: RetI(int(g_args[0]) >= 6000 ? 1 : 0); break;                                                             // DOES_PARTICLE_FX_LOOPED_EXIST
	case 0x8F75998877616996: printf("FAKE: particle fx %d stopped\n", int(g_args[0])); fflush(stdout); break;                       // STOP_PARTICLE_FX_LOOPED
	case 0x2274BC1C4885E333: {                                // GET_OFFSET_FROM_ENTITY_GIVEN_WORLD_COORDS (the car)
		const double w[3] = { F(1) - g_pos[0], F(2) - g_pos[1], F(3) - g_pos[2] };
		RetV(float(w[0] * g_rot[0][0] + w[1] * g_rot[1][0] + w[2] * g_rot[2][0]), float(w[0] * g_rot[0][1] + w[1] * g_rot[1][1] + w[2] * g_rot[2][1]),
		     float(w[0] * g_rot[0][2] + w[1] * g_rot[1][2] + w[2] * g_rot[2][2]));
		break; }
	// v2.6 weapons
	case 0x6C4D0409BA1A2BC2: {                                // GET_PED_LAST_WEAPON_IMPACT_COORD
		FakePed* fp = PedOf(int(g_args[0]));
		NV*      o = reinterpret_cast<NV*>(g_args[1]);
		if (!fp) { RetI(0); break; }
		if (!fp->n) *o = NV{ float(fp->head[0]), float(fp->head[1]), float(fp->head[2] - 1.6) };   // (an earlier shot at its feet)
		else {
			const double j = (fp->n & 1) ? 0.02 : -0.02;
			*o = NV{ float(fp->tgt[0]), float(fp->tgt[1]), float(fp->tgt[2] + j) };
		}
		RetI(1);
		break; }
	case 0x34616828CD07F1A1: { FakePed* fp = PedOf(int(g_args[0])); RetI(fp && NowS() - fp->last < 0.12 ? 1 : 0); break; }  // IS_PED_SHOOTING
	case 0x0A6DB4965674D243: RetI(int(0x1B06D571u)); break;  // GET_SELECTED_PED_WEAPON: WEAPON_PISTOL
	case 0xC3287EE3050FB74C: RetI(int(0x18D5FA97u)); break;  // GET_WEAPONTYPE_GROUP: GROUP_PISTOL
	case 0x37F4AD56ECBC0CD6: RetI(50); break;                 // GET_PED_ACCURACY
	case 0x4859F1FC66A6278E: RetI(PedOf(int(g_args[0])) && int(g_args[0]) != int(g_args[1]) ? 1 : 0); break;  // IS_PED_IN_COMBAT (with you)
	case 0x17C07FC640E86B4E: {                                // GET_PED_BONE_COORDS (the head)
		if (FakePed* fp = PedOf(int(g_args[0]))) { RetV(float(fp->head[0]), float(fp->head[1]), float(fp->head[2])); break; }
		double h[3];
		CarPt(-0.4, 0.0, 0.45, h);   // you, in the driver's seat
		RetV(float(h[0]), float(h[1]), float(h[2]));
		break; }
	case 0x3317DEDB88C95038: RetI(int(g_args[0]) == 1 && g_health <= 100 ? 1 : 0); break;  // IS_PED_DEAD_OR_DYING
	case 0xEEF059FAD016D209: RetI(int(g_args[0]) == 1 ? g_health : 200); break;            // GET_ENTITY_HEALTH
	case 0x6B76DC1F3AE6E6A3:                                  // SET_ENTITY_HEALTH
		if (int(g_args[0]) == 1) { g_health = int(g_args[1]); printf("FAKE: player health %d\n", g_health); fflush(stdout); }
		break;
	case 0xFAEE099C6F890BB8:                                  // SET_ENTITY_PROOFS
		printf("FAKE: proofs entity %d: bullet %d fire %d explosion %d\n", int(g_args[0]), int(g_args[1]), int(g_args[2]), int(g_args[3])); fflush(stdout);
		break;
	case 0x6B83617E04503888:                                  // START_SCRIPT_FIRE
		++g_fires;
		printf("FAKE: script fire %d at %.2f %.2f %.2f\n", 5000 + g_fires, F(0), F(1), F(2)); fflush(stdout);
		RetI(5000 + g_fires);
		break;
	case 0x7FF548385680673F: printf("FAKE: script fire %d removed\n", int(g_args[0])); fflush(stdout); break;  // REMOVE_SCRIPT_FIRE
	case 0xBA71116ADF5B514C: {                                // EXPLODE_VEHICLE: GTA's car blows up (in GTA's explosion list)
		printf("FAKE: vehicle %d exploded\n", int(g_args[0])); fflush(stdout);
		FakeExp e{ 7, { g_pos[0], g_pos[1], g_pos[2] }, NowS(), NowS() + 0.5 };
		g_exps.push_back(e);
		break; }
	case 0xAB0F816885B0E483: {                                // IS_EXPLOSION_IN_SPHERE type x y z r
		const int    ty = int(g_args[0]);
		const double x = F(1), y = F(2), z = F(3), r = F(4), now = NowS();
		int          in = 0;
		for (const FakeExp& e : g_exps)
			if (now >= e.t0 && now <= e.t1 && (ty == -1 || ty == e.type) && (e.p[0] - x) * (e.p[0] - x) + (e.p[1] - y) * (e.p[1] - y) + (e.p[2] - z) * (e.p[2] - z) <= r * r) in = 1;
		RetI(in);
		break; }
	case 0x3D87450E15D98694: {                                // GET_SHAPE_TEST_RESULT: ground hit along the view
		int* hit = reinterpret_cast<int*>(g_args[1]);
		NV*  end = reinterpret_cast<NV*>(g_args[2]);
		int* entOut = reinterpret_cast<int*>(g_args[4]);
		{   // v2.51: an object ray
			const int h = int(g_args[0]);
			bool done = false;
			for (size_t i = 0; i < g_objRays.size(); ++i)
				if (g_objRays[i].h == h) {
					const FakeObjHit o = g_objRays[i];
					g_objRays.erase(g_objRays.begin() + i);
					*hit = o.hit ? 1 : 0;
					*end = NV{ float(o.p[0]), float(o.p[1]), float(o.p[2]) };
					if (entOut) *entOut = o.hit ? o.ent : 0;
					done = true;
					break;
				}
			if (done) { RetI(2); break; }
		}
		{   // v2.4: a ray with its own handle (the ride check): the fake ground. v2.51: along any direction (a jump's flight), in
			// 0.25 m steps: the first step that crosses the ground
			const int h = int(g_args[0]);
			size_t i = 0;
			while (i < g_rays.size() && g_rays[i].h != h) ++i;
			if (i < g_rays.size()) {
				if (g_rays[i].tick == g_tickN && !g_rays[i].now) { RetI(1); break; }
				const FakeRay r = g_rays[i];
				g_rays.erase(g_rays.begin() + i);
				const double len = hypot(double(r.x1) - r.x0, double(r.y1) - r.y0);
				const int    N = std::max(1, int(ceil(len / 0.25)));
				double       pz = r.z0;
				*hit = 0;
				*end = NV{ r.x1, r.y1, r.z1 };
				if (entOut) *entOut = 0;
				for (int k = 1; k <= N; ++k) {
					const double t = double(k) / N, x = r.x0 + (r.x1 - r.x0) * t, y = r.y0 + (r.y1 - r.y0) * t, z = r.z0 + (r.z1 - r.z0) * t;
					unsigned     m = 0;
					double       gz = 0;
					if (FakeRayHit(x, y, std::max(pz, z), std::min(pz, z), &gz, &m)) {
						*hit = 1;
						*end = NV{ float(x), float(y), float(gz) };
						break;
					}
					pz = z;
				}
				RetI(2);
				break;
			}
		}
		*hit = g_rayDown ? 1 : 0;
		const double p = -9.0 * 3.14159265358979 / 180.0, y = 118.0 * 3.14159265358979 / 180.0;
		const double fx = -sin(y) * cos(p), fy = cos(y) * cos(p), fz = sin(p);
		const double t = 1.4 / -fz;
		*end = NV{ float(5.5 + fx * t), float(3.0 + fy * t), 0.0f };
		RetI(2);
		break;
	}
	// v2.01 text: feed posts are printed, drawn lines go to fake_hud.txt
	case 0x202709F4C58A0424: g_textMode = 2; break;           // BEGIN_TEXT_COMMAND_THEFEED_POST
	case 0x25FBB336DF1804CB: g_textMode = 1; g_textBuf.clear(); break;  // BEGIN_TEXT_COMMAND_DISPLAY_TEXT
	case 0x6C188BE134E074AA:                                  // ADD_TEXT_COMPONENT_SUBSTRING_PLAYER_NAME
		if (g_textMode == 1) g_textBuf += reinterpret_cast<const char*>(g_args[0]);
		else { printf("FEED: %s\n", reinterpret_cast<const char*>(g_args[0])); fflush(stdout); }
		break;
	case 0xCD015E5BB0D96A57: {                                // END_TEXT_COMMAND_DISPLAY_TEXT x y
		char b[32];
		snprintf(b, sizeof b, "[%.3f,%.3f] ", F(0), F(1));
		g_hud.push_back(b + g_textBuf);
		g_textMode = 0;
		break;
	}
	default: break;
	}
	return g_ret;
}

// test hooks for the host
EXP void FakeCallPresent(void* sc) { for (auto p : g_present) if (p) p(sc); }
static DWORD WINAPI ScriptThread(LPVOID) { if (g_script) g_script(); return 0; }
EXP void FakeStartScript() { CreateThread(nullptr, 0, ScriptThread, nullptr, 0, nullptr); }
EXP void FakeSetBurnout(int on) { Advance(); g_burnout = on; }
EXP void FakeSetSpeed(float v) { Advance(); g_speed = v; }
EXP void FakeSetPaused(int on) { g_paused = on; }
EXP void FakeRayStats(unsigned* started, unsigned* refused, unsigned* waiting) { *started = g_rayStarted; *refused = g_rayRefused; *waiting = unsigned(g_rays.size()); }
EXP void FakeFreeze(int ms) { g_freezeMs = ms; }
EXP void FakeTimeScale(float x) { g_timeScale = x; printf("FAKE: GTA time scale %.2f\n", x); fflush(stdout); }
