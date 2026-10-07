// host.cpp - fake GTA host for the sandbox smoke test (Wine + Xvfb). Draws a ground plane and a
// pole into a reversed-Z R32G8X24 depth buffer (like GTA), loads GtaBeamD1.asi against the fake
// ScriptHookV.dll, calls its Present callback each frame, presses its keys, and saves frames as PPM.
// FAKE_HDR=1 (v1.9.3): the frame is built like GTA's (capture of the user's game): G-buffer + depth,
// HDR scene target R16G16B16A16 + depth, a second HDR target bound last, then post-processing (bloom
// downsample reading the first HDR target, tone mapping into an LDR target), copy to the back buffer,
// HUD (a minimap rectangle) on top, Present. The plugin should learn it and draw inside it.
// Usage: host.exe <script file>   (lines: "<frame> key <f11|shiftf11|ctrlf11|ctrlshiftf11|altf11|f10|shiftf10>",
//        "<frame> shot <name>", "<frame> ini <key> <value>", "<frame> burnout <0|1>", "<frame> speed <m/s>", "<frame> quit",
//        v2.01: "<frame> keydown <vk hex>", "<frame> keyup <vk hex>", "<frame> pause <0|1>", v2.41: "<frame> freeze <ms>",
//        "<frame> pad <buttons hex>,<lt 0-255>,<rt 0-255>,<lx>,<ly>,<rx>,<ry>" (sticks -32768..32767; written to
//        fake_pad.txt, which the fake xinput1_4.dll reads), "<frame> pad off" (no pad))
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static const int W = 1280, H = 720;
static ID3D11Device* dev;
static ID3D11DeviceContext* ctx;
static IDXGISwapChain* sc;

static const char* kHlsl = R"(
cbuffer C : register(b0) { float4 r; float4 u; float4 f; float4 cam; float4 pr; };
struct VO { float4 p : SV_Position; float3 w : TEXCOORD0; };
VO VS(float3 p : POSITION) {
	VO o; float3 rel = p - cam.xyz;
	float x = dot(rel, r.xyz), y = dot(rel, u.xyz), z = dot(rel, f.xyz);
	o.p = float4(x * pr.x, y * pr.y, pr.z - pr.w * z, z); o.w = p; return o; }
float4 PS(VO i) : SV_Target {
	float2 g = floor(i.w.xy / 3.0);
	float c = fmod(abs(g.x + g.y), 2.0) < 1.0 ? 0.42 : 0.66;
	if (i.w.z > 0.05) c = 0.25;
	return float4(c, c * 0.97, c * 0.92, 1); }
struct GB { float4 a : SV_Target0; float4 b : SV_Target1; float4 c : SV_Target2; float4 d : SV_Target3; };
GB PSG(VO i) { GB o; float4 c = PS(i); o.a = c; o.b = float4(0.5, 0.5, 1, 1); o.c = 0; o.d = 0; return o; }
float4 PSH(VO i) : SV_Target { float4 c = PS(i); return float4(pow(c.rgb, 2.2) * 6.0, 1); }   // HDR: linear x6
Texture2D t0 : register(t0); Texture2D t1 : register(t1); SamplerState s0 : register(s0);
float4 VSF(uint id : SV_VertexID) : SV_Position { float2 uv = float2((id << 1) & 2, id & 2); return float4(uv * float2(2, -2) + float2(-1, 1), 0, 1); }
float4 PSD(float4 p : SV_Position) : SV_Target { uint w, h; t0.GetDimensions(w, h); return t0.SampleLevel(s0, p.xy * 2.0 / float2(w, h), 0); }
float4 PST(float4 p : SV_Position) : SV_Target { uint w, h; t0.GetDimensions(w, h); float2 uv = p.xy / float2(w, h);
	float3 c = t0.SampleLevel(s0, uv, 0).rgb + 0.05 * t1.SampleLevel(s0, uv, 0).rgb; c = c / (c + 1.0); return float4(pow(c, 1.0 / 2.2), 1); }
float4 PSC(float4 p : SV_Position) : SV_Target { return t0.Load(int3(p.xy, 0)); }
float4 PSU(float4 p : SV_Position) : SV_Target { return float4(0.85, 0.2, 0.75, 1); }
)";

static void Basis(double p, double ro, double y, double r[3], double f[3], double u[3])
{
	const double k = 3.14159265358979 / 180.0, cp = cos(p * k), sp = sin(p * k), cy = cos(y * k), sy = sin(y * k), cr = cos(ro * k), sr = sin(ro * k);
	r[0] = cy * cr - sy * sp * sr; r[1] = sy * cr + cy * sp * sr; r[2] = -cp * sr;
	f[0] = -sy * cp; f[1] = cy * cp; f[2] = sp;
	u[0] = cy * sr + sy * sp * cr; u[1] = sy * sr - cy * sp * cr; u[2] = cp * cr;
}

static void Shot(const char* name)
{
	ID3D11Texture2D* bb = nullptr;
	sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb);
	D3D11_TEXTURE2D_DESC d; bb->GetDesc(&d);
	d.Usage = D3D11_USAGE_STAGING; d.BindFlags = 0; d.CPUAccessFlags = D3D11_CPU_ACCESS_READ; d.MiscFlags = 0;
	ID3D11Texture2D* st = nullptr; dev->CreateTexture2D(&d, nullptr, &st);
	ctx->CopyResource(st, bb);
	D3D11_MAPPED_SUBRESOURCE m;
	if (SUCCEEDED(ctx->Map(st, 0, D3D11_MAP_READ, 0, &m))) {
		FILE* f = fopen(name, "wb");
		fprintf(f, "P6\n%d %d\n255\n", W, H);
		std::vector<unsigned char> row(W * 3);
		for (int y = 0; y < H; ++y) {
			const unsigned char* s = (const unsigned char*)m.pData + y * m.RowPitch;
			for (int x = 0; x < W; ++x) { row[x * 3] = s[x * 4 + 2]; row[x * 3 + 1] = s[x * 4 + 1]; row[x * 3 + 2] = s[x * 4]; }
			fwrite(row.data(), 1, row.size(), f);
		}
		fclose(f);
		ctx->Unmap(st, 0);
		printf("HOST: saved %s\n", name); fflush(stdout);
	}
	st->Release(); bb->Release();
}

static void Key(HWND hwnd, const char* what)
{
	SetForegroundWindow(hwnd);
	std::vector<INPUT> in;
	auto k = [&](WORD vk, bool up) { INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wVk = vk; i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0; in.push_back(i); };
	bool shift = strstr(what, "shift") != nullptr, ctrl = strstr(what, "ctrl") != nullptr, alt = strstr(what, "alt") != nullptr;
	if (alt) k(VK_MENU, false);
	if (ctrl) k(VK_CONTROL, false);
	if (shift) k(VK_SHIFT, false);
	const WORD fk = strstr(what, "f10") ? VK_F10 : VK_F11;  // v2.0: drive key
	k(fk, false);
	SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
	Sleep(80);
	in.clear();
	k(fk, true);
	if (shift) k(VK_SHIFT, true);
	if (ctrl) k(VK_CONTROL, true);
	if (alt) k(VK_MENU, true);
	SendInput(UINT(in.size()), in.data(), sizeof(INPUT));
	printf("HOST: key %s\n", what); fflush(stdout);
}

// v2.01: hold / let go of one key (virtual key code)
static void KeyState(HWND hwnd, int vk, bool up)
{
	SetForegroundWindow(hwnd);
	INPUT i{}; i.type = INPUT_KEYBOARD; i.ki.wVk = WORD(vk); i.ki.dwFlags = up ? KEYEVENTF_KEYUP : 0;
	SendInput(1, &i, sizeof(INPUT));
	printf("HOST: key 0x%02X %s\n", vk, up ? "up" : "down"); fflush(stdout);
}

int main(int argc, char** argv)
{
	WNDCLASSA wc{}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "fakegta"; wc.hInstance = GetModuleHandleA(nullptr);
	RegisterClassA(&wc);
	HWND hwnd = CreateWindowA("fakegta", "fake GTA", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, W, H, nullptr, nullptr, wc.hInstance, nullptr);
	DXGI_SWAP_CHAIN_DESC sd{}; sd.BufferCount = 1; sd.BufferDesc.Width = W; sd.BufferDesc.Height = H; sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; sd.OutputWindow = hwnd; sd.SampleDesc.Count = 1; sd.Windowed = TRUE;
	D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &sd, &sc, &dev, nullptr, &ctx))) {
		printf("HOST: no D3D11 device\n"); return 1;
	}
	// GTA-like depth: R32G8X24 typeless, reversed Z, readable
	D3D11_TEXTURE2D_DESC td{}; td.Width = W; td.Height = H; td.MipLevels = 1; td.ArraySize = 1; td.Format = DXGI_FORMAT_R32G8X24_TYPELESS;
	td.SampleDesc.Count = 1; td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	ID3D11Texture2D* dt; dev->CreateTexture2D(&td, nullptr, &dt);
	D3D11_DEPTH_STENCIL_VIEW_DESC dvd{}; dvd.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT; dvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	ID3D11DepthStencilView* dsv; dev->CreateDepthStencilView(dt, &dvd, &dsv);
	D3D11_DEPTH_STENCIL_DESC dd{}; dd.DepthEnable = TRUE; dd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; dd.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	ID3D11DepthStencilState* dss; dev->CreateDepthStencilState(&dd, &dss);
	D3D11_RASTERIZER_DESC rsd{}; rsd.FillMode = D3D11_FILL_SOLID; rsd.CullMode = D3D11_CULL_NONE; rsd.DepthClipEnable = TRUE;
	ID3D11RasterizerState* rs; dev->CreateRasterizerState(&rsd, &rs);
	ID3DBlob *vb, *pb, *err = nullptr;
	if (FAILED(D3DCompile(kHlsl, strlen(kHlsl), "h", nullptr, nullptr, "VS", "vs_5_0", 0, 0, &vb, &err)) ||
	    FAILED(D3DCompile(kHlsl, strlen(kHlsl), "h", nullptr, nullptr, "PS", "ps_5_0", 0, 0, &pb, &err))) {
		printf("HOST: shader error %s\n", err ? (char*)err->GetBufferPointer() : ""); return 1;
	}
	ID3D11VertexShader* vs; ID3D11PixelShader* ps;
	dev->CreateVertexShader(vb->GetBufferPointer(), vb->GetBufferSize(), nullptr, &vs);
	dev->CreatePixelShader(pb->GetBufferPointer(), pb->GetBufferSize(), nullptr, &ps);
	D3D11_INPUT_ELEMENT_DESC il{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 };
	ID3D11InputLayout* lay; dev->CreateInputLayout(&il, 1, vb->GetBufferPointer(), vb->GetBufferSize(), &lay);
	// ground (z=0, 60x60 m) + a pole at (1.6, 1.2) in front of the car
	std::vector<float> v;
	auto quad = [&](float a[3], float b[3], float c[3], float d[3]) { for (float* p : { a, b, c, a, c, d }) v.insert(v.end(), p, p + 3); };
	float g0[3] = { -30, -30, 0 }, g1[3] = { 30, -30, 0 }, g2[3] = { 30, 30, 0 }, g3[3] = { -30, 30, 0 };
	quad(g0, g1, g2, g3);
	const float px = 3.2f, py = -0.6f, s = 0.12f;
	for (int k = 0; k < 4; ++k) {
		const float ox[4] = { -s, s, s, -s }, oy[4] = { -s, -s, s, s };
		const int j = (k + 1) % 4;
		float a[3] = { px + ox[k], py + oy[k], 0 }, b[3] = { px + ox[j], py + oy[j], 0 }, c[3] = { px + ox[j], py + oy[j], 2.5f }, d[3] = { px + ox[k], py + oy[k], 2.5f };
		quad(a, b, c, d);
	}
	D3D11_BUFFER_DESC bd{}; bd.ByteWidth = UINT(v.size() * 4); bd.BindFlags = D3D11_BIND_VERTEX_BUFFER; bd.Usage = D3D11_USAGE_IMMUTABLE;
	D3D11_SUBRESOURCE_DATA init{ v.data() }; ID3D11Buffer* vbuf; dev->CreateBuffer(&bd, &init, &vbuf);
	float cbd[20] = {};
	float cam[5] = { 5.5f, 3.0f, 1.4f, -9.0f, 118.0f };
	{ char e[128]; if (GetEnvironmentVariableA("FAKE_CAM", e, sizeof e)) sscanf(e, "%f %f %f %f %f", cam, cam + 1, cam + 2, cam + 3, cam + 4); }
	double r[3], f[3], u[3]; Basis(cam[3], 0, cam[4], r, f, u);
	const double n = 0.1, fa = 2000.0, ys = 1.0 / tan(25.0 * 3.14159265358979 / 180.0);
	for (int k = 0; k < 3; ++k) { cbd[k] = float(r[k]); cbd[4 + k] = float(u[k]); cbd[8 + k] = float(f[k]); }
	cbd[12] = cam[0]; cbd[13] = cam[1]; cbd[14] = cam[2];
	cbd[16] = float(ys * H / W); cbd[17] = float(ys); cbd[18] = float(n * fa / (fa - n)); cbd[19] = float(n / (fa - n));
	bd.ByteWidth = sizeof(cbd); bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER; init.pSysMem = cbd; ID3D11Buffer* cb; dev->CreateBuffer(&bd, &init, &cb);

	HMODULE fake = LoadLibraryA("ScriptHookV.dll");
	auto callPresent = (void (*)(void*))GetProcAddress(fake, "FakeCallPresent");
	auto startScript = (void (*)())GetProcAddress(fake, "FakeStartScript");
	auto setBurnout = (void (*)(int))GetProcAddress(fake, "FakeSetBurnout");
	auto setSpeed = (void (*)(float))GetProcAddress(fake, "FakeSetSpeed");
	auto setPaused = (void (*)(int))GetProcAddress(fake, "FakeSetPaused");
	auto freeze = (void (*)(int))GetProcAddress(fake, "FakeFreeze");
	if (!LoadLibraryA("GtaBeamD1.asi")) { printf("HOST: could not load GtaBeamD1.asi (%lu)\n", GetLastError()); return 1; }
	startScript();

	struct Cmd { int frame; char what[32]; char a[64]; char b[64]; };
	std::vector<Cmd> cmds;
	if (argc > 1) {
		FILE* sf = fopen(argv[1], "r");
		char line[256];
		while (sf && fgets(line, sizeof(line), sf)) { Cmd c{}; if (sscanf(line, "%d %31s %63s %63s", &c.frame, c.what, c.a, c.b) >= 2) cmds.push_back(c); }
		if (sf) fclose(sf);
	}
	for (int frame = 1; frame < 100000; ++frame) {
		MSG msg; while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&msg); DispatchMessageA(&msg); }
		ID3D11Texture2D* bbt; sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bbt);
		ID3D11RenderTargetView* rtv; dev->CreateRenderTargetView(bbt, nullptr, &rtv); bbt->Release();
		const float sky[4] = { 0.62f, 0.72f, 0.86f, 1 };
		D3D11_VIEWPORT vp{ 0, 0, W, H, 0, 1 }; ctx->RSSetViewports(1, &vp);
		UINT stride = 12, off = 0;
		ctx->ClearRenderTargetView(rtv, sky);
		ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 0.0f, 0);
		ctx->OMSetRenderTargets(1, &rtv, dsv);
		ctx->OMSetDepthStencilState(dss, 0); ctx->RSSetState(rs);
		ctx->IASetVertexBuffers(0, 1, &vbuf, &stride, &off); ctx->IASetInputLayout(lay); ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(vs, nullptr, 0); ctx->PSSetShader(ps, nullptr, 0); ctx->VSSetConstantBuffers(0, 1, &cb);
		ctx->Draw(UINT(v.size() / 3), 0);
		ctx->OMSetRenderTargets(1, &rtv, nullptr);
		rtv->Release();
		callPresent(sc);
		bool quit = false;
		for (auto& c : cmds) {
			if (c.frame != frame) continue;
			if (!strcmp(c.what, "key")) Key(hwnd, c.a);
			else if (!strcmp(c.what, "keydown")) KeyState(hwnd, int(strtol(c.a, nullptr, 16)), false);
			else if (!strcmp(c.what, "keyup")) KeyState(hwnd, int(strtol(c.a, nullptr, 16)), true);
			else if (!strcmp(c.what, "shot")) Shot(c.a);
			else if (!strcmp(c.what, "burnout")) setBurnout(atoi(c.a));
			else if (!strcmp(c.what, "speed")) { setSpeed(float(atof(c.a))); printf("HOST: speed %s\n", c.a); fflush(stdout); }
			else if (!strcmp(c.what, "ini")) { WritePrivateProfileStringA("D1", c.a, c.b, ".\\GtaBeamD1.ini"); printf("HOST: ini %s=%s\n", c.a, c.b); }
			else if (!strcmp(c.what, "pause")) { setPaused(atoi(c.a)); printf("HOST: pause %s\n", c.a); fflush(stdout); }
			else if (!strcmp(c.what, "freeze")) { freeze(atoi(c.a)); printf("HOST: GTA paused for %s ms\n", c.a); fflush(stdout); }
			else if (!strcmp(c.what, "pad")) {
				// the fake pad DLL may have the file open this instant: try again until the new state is in place
				FILE* pf = fopen("fake_pad.tmp", "w");
				if (pf) {
					fprintf(pf, "%s\n", c.a);
					fclose(pf);
					int tries = 0;
					while (!MoveFileExA("fake_pad.tmp", "fake_pad.txt", MOVEFILE_REPLACE_EXISTING) && ++tries < 500) Sleep(1);
					if (tries) printf("HOST: pad file replaced after %d retries\n", tries);
				}
				printf("HOST: pad %s\n", c.a); fflush(stdout);
			}
			else if (!strcmp(c.what, "quit")) quit = true;
		}
		sc->Present(0, 0);
		Sleep(16);
		if (quit) break;
	}
	printf("HOST: done\n");
	fflush(stdout);
	ExitProcess(0);
}
