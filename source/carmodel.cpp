// carmodel.cpp - see carmodel.h.
// Uses cgltf (MIT, Johannes Kuhlmann) for glTF, stb_image (public domain / MIT, Sean Barrett) for PNG/JPEG
// and bcdec (MIT / Unlicense, Sergii Kudlai) to look inside block-compressed .dds textures.

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"
#define BCDEC_IMPLEMENTATION
#define BCDEC_STATIC
#include "bcdec.h"

#include "carmodel.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <tuple>

namespace car
{
	namespace
	{
		// -------------------------------------------------------------------------------------
		// A tiny JSON reader for the "extras" blocks (BeamNG material data, driving direction, paint)
		// -------------------------------------------------------------------------------------
		struct J
		{
			enum T { Null, Bool, Num, Str, Arr, Obj } t = Null;
			bool                                b = false;
			double                              n = 0;
			std::string                         s;
			std::vector<J>                      a;
			std::vector<std::pair<std::string, J>> o;

			const J* get(const char* k) const
			{
				if (t != Obj) return nullptr;
				for (auto& kv : o) if (kv.first == k) return &kv.second;
				return nullptr;
			}
			// numbers given as numbers or as strings ("1", "1.5", "true")
			bool truthy() const
			{
				if (t == Bool) return b;
				if (t == Num) return n != 0;
				if (t == Str) return s == "1" || s == "true" || s == "True";
				return false;
			}
			bool num(double& out) const
			{
				if (t == Num) { out = n; return true; }
				if (t == Str && !s.empty()) { char* e = nullptr; out = strtod(s.c_str(), &e); return e && *e == 0; }
				if (t == Bool) { out = b ? 1 : 0; return true; }
				return false;
			}
			// elements of an array, or the values of an object (BeamNG encodes sparse Lua arrays as objects)
			std::vector<const J*> items() const
			{
				std::vector<const J*> v;
				if (t == Arr) for (auto& x : a) v.push_back(&x);
				if (t == Obj) for (auto& kv : o) v.push_back(&kv.second);
				return v;
			}
		};

		struct JP
		{
			const char* p;
			const char* e;
			void ws() { while (p < e && (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t')) ++p; }
			bool str(std::string& out)
			{
				if (p >= e || *p != '"') return false;
				++p;
				while (p < e && *p != '"') {
					if (*p == '\\' && p + 1 < e) {
						++p;
						switch (*p) {
						case 'n': out += '\n'; break;
						case 't': out += '\t'; break;
						case 'r': out += '\r'; break;
						case 'b': out += '\b'; break;
						case 'f': out += '\f'; break;
						case 'u': {
							unsigned c = 0;
							for (int i = 0; i < 4 && p + 1 < e; ++i) { ++p; c = c * 16 + unsigned(isdigit((unsigned char)*p) ? *p - '0' : (tolower(*p) - 'a' + 10)); }
							if (c < 0x80) out += char(c); else out += '?';
							break;
						}
						default: out += *p;
						}
						++p;
					} else {
						out += *p++;
					}
				}
				if (p >= e) return false;
				++p;
				return true;
			}
			bool val(J& v, int depth = 0)
			{
				if (depth > 64) return false;
				ws();
				if (p >= e) return false;
				char c = *p;
				if (c == '{') {
					v.t = J::Obj;
					++p;
					ws();
					if (p < e && *p == '}') { ++p; return true; }
					for (;;) {
						ws();
						std::string k;
						if (!str(k)) return false;
						ws();
						if (p >= e || *p != ':') return false;
						++p;
						J x;
						if (!val(x, depth + 1)) return false;
						v.o.emplace_back(std::move(k), std::move(x));
						ws();
						if (p < e && *p == ',') { ++p; continue; }
						if (p < e && *p == '}') { ++p; return true; }
						return false;
					}
				}
				if (c == '[') {
					v.t = J::Arr;
					++p;
					ws();
					if (p < e && *p == ']') { ++p; return true; }
					for (;;) {
						J x;
						if (!val(x, depth + 1)) return false;
						v.a.push_back(std::move(x));
						ws();
						if (p < e && *p == ',') { ++p; continue; }
						if (p < e && *p == ']') { ++p; return true; }
						return false;
					}
				}
				if (c == '"') { v.t = J::Str; return str(v.s); }
				if (e - p >= 4 && !strncmp(p, "true", 4)) { v.t = J::Bool; v.b = true; p += 4; return true; }
				if (e - p >= 5 && !strncmp(p, "false", 5)) { v.t = J::Bool; v.b = false; p += 5; return true; }
				if (e - p >= 4 && !strncmp(p, "null", 4)) { v.t = J::Null; p += 4; return true; }
				// number (also accepts inf/nan spellings some encoders write)
				const char* s0 = p;
				while (p < e && (isalnum((unsigned char)*p) || *p == '-' || *p == '+' || *p == '.')) ++p;
				if (p == s0) return false;
				std::string num(s0, p);
				v.t = J::Num;
				v.n = strtod(num.c_str(), nullptr);
				return true;
			}
		};

		bool ParseJson(const char* text, J& out)
		{
			if (!text) return false;
			JP jp{ text, text + strlen(text) };
			return jp.val(out);
		}

		// -------------------------------------------------------------------------------------
		// helpers
		// -------------------------------------------------------------------------------------
		void Logf(const LogFn& log, const char* fmt, ...)
		{
			if (!log) return;
			char buf[1024];
			va_list ap;
			va_start(ap, fmt);
			vsnprintf(buf, sizeof(buf), fmt, ap);
			va_end(ap);
			log(buf);
		}

		struct V3
		{
			double x = 0, y = 0, z = 0;
		};
		V3     Sub(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
		double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
		V3     Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
		V3     Norm(V3 a)
		{
			const double l = sqrt(Dot(a, a));
			return l > 1e-12 ? V3{ a.x / l, a.y / l, a.z / l } : V3{ 0, 0, 0 };
		}

		// glTF (y up) -> BeamNG world (z up): inverse of export.lua's {x, z, -y}
		V3 FromGltf(V3 g) { return { g.x, -g.z, g.y }; }

		int FirstIndex(const J* arr)
		{
			if (!arr) return -1;
			for (const J* x : arr->items()) {
				double d;
				if (x->num(d) && d >= 0) return int(d);
			}
			return -1;
		}

		// 2x2 box filter, any number of bytes per pixel
		void Downscale(std::vector<uint8_t>& px, int& w, int& h, int bpp)
		{
			const int            nw = std::max(1, w / 2), nh = std::max(1, h / 2);
			std::vector<uint8_t> out(size_t(nw) * nh * bpp);
			for (int y = 0; y < nh; ++y)
				for (int x = 0; x < nw; ++x)
					for (int c = 0; c < bpp; ++c) {
						const int x0 = std::min(x * 2, w - 1), x1 = std::min(x * 2 + 1, w - 1);
						const int y0 = std::min(y * 2, h - 1), y1 = std::min(y * 2 + 1, h - 1);
						const int s = px[(size_t(y0) * w + x0) * bpp + c] + px[(size_t(y0) * w + x1) * bpp + c] + px[(size_t(y1) * w + x0) * bpp + c] +
						              px[(size_t(y1) * w + x1) * bpp + c];
						out[(size_t(y) * nw + x) * bpp + c] = uint8_t((s + 2) / 4);
					}
			px.swap(out);
			w = nw;
			h = nh;
		}

		void FinishTexture(std::vector<uint8_t>& level, int w, int h, int bpp, int maxSize, Texture& t)
		{
			while (w > maxSize || h > maxSize) Downscale(level, w, h, bpp);
			t.w = w;
			t.h = h;
			t.bpp = bpp;
			t.mips.clear();
			t.mips.push_back(level);
			int mw = w, mh = h;
			while (mw > 1 || mh > 1) {
				Downscale(level, mw, mh, bpp);
				t.mips.push_back(level);
			}
		}

		bool EndsWith(const std::string& s, const char* suffix)
		{
			const size_t n = strlen(suffix);
			if (s.size() < n) return false;
			for (size_t i = 0; i < n; ++i)
				if (tolower((unsigned char)s[s.size() - n + i]) != tolower((unsigned char)suffix[i])) return false;
			return true;
		}

		// RGBA8 pixels (from stb) -> the texture's GPU layout. srcChannels: what the file had.
		void Pack(const uint8_t* px, int w, int h, int srcChannels, int maxSize, Texture& t)
		{
			const size_t n = size_t(w) * h;
			if (t.kind == kTexColor || t.kind == kTexMask) {
				std::vector<uint8_t> level(px, px + n * 4);
				FinishTexture(level, w, h, 4, maxSize, t);
				return;
			}
			// channel statistics (decide where the data is)
			int    lo[4] = { 255, 255, 255, 255 }, hi[4] = { 0, 0, 0, 0 };
			double sum[4] = {};
			const size_t step = std::max<size_t>(1, n / 65536);
			size_t       cnt = 0;
			for (size_t i = 0; i < n; i += step, ++cnt) {
				for (int c = 0; c < 4; ++c) {
					const int v = px[i * 4 + c];
					lo[c] = std::min(lo[c], v);
					hi[c] = std::max(hi[c], v);
					sum[c] += v;
				}
			}
			const double mean[4] = { sum[0] / cnt / 255.0, sum[1] / cnt / 255.0, sum[2] / cnt / 255.0, sum[3] / cnt / 255.0 };
			const bool   alphaVaries = (srcChannels == 2 || srcChannels == 4) && hi[3] - lo[3] > 8;
			const bool   redFlat = hi[0] - lo[0] <= 8;
			char         note[200];
			if (t.kind == kTexSingle) {
				// BeamNG data maps are grayscale (red). Converted from .dds: red unless only alpha carries data.
				// A plain PNG with real alpha: alpha (e.g. a colour image used as its own opacity map).
				const bool fromDds = EndsWith(t.name, ".dds.png");
				const bool useAlpha = alphaVaries && (redFlat || !fromDds);
				const int  ch = useAlpha ? 3 : 0;
				t.channel = ch;
				// a small all-channel copy, so the loader can check what each channel says on the car (v1.9.1)
				{
					std::vector<uint8_t> th(px, px + n * 4);
					int                  tw = w, tht = h;
					while (tw > 128 || tht > 128) Downscale(th, tw, tht, 4);
					t.thumb.swap(th);
					t.thumbW = tw;
					t.thumbH = tht;
				}
				std::vector<uint8_t> level(n);
				for (size_t i = 0; i < n; ++i) level[i] = px[i * 4 + ch];
				snprintf(note, sizeof(note), "uses %s (mean r %.2f g %.2f b %.2f a %.2f, min/max red %d/%d alpha %d/%d, file channels %d)", useAlpha ? "alpha" : "red",
					mean[0], mean[1], mean[2], mean[3], lo[0], hi[0], lo[3], hi[3], srcChannels);
				t.note = note;
				FinishTexture(level, w, h, 1, maxSize, t);
				return;
			}
			// normal map: X/Y in red/green (BeamNG BC5). Legacy DXT5nm keeps X in alpha (red is flat then).
			const bool xInAlpha = alphaVaries && redFlat && hi[0] >= 240;
			std::vector<uint8_t> level(n * 2);
			for (size_t i = 0; i < n; ++i) {
				level[i * 2] = px[i * 4 + (xInAlpha ? 3 : 0)];
				level[i * 2 + 1] = px[i * 4 + 1];
			}
			snprintf(note, sizeof(note), "normal X from %s (mean r %.2f g %.2f b %.2f a %.2f)", xInAlpha ? "alpha (old DXT5nm style)" : "red", mean[0], mean[1], mean[2],
				mean[3]);
			t.note = note;
			FinishTexture(level, w, h, 2, maxSize, t);
		}

		// ---- .dds files (v1.9.3): BeamNG's own texture files, copied untouched by the BeamNG mod.
		// Block-compressed ones (BC1-BC5, BC7) go to the GPU as they are: full quality, 1/4 to 1/8 of the
		// memory of plain pixels, and no conversion. A small level is decoded on the CPU to decide which
		// channel holds the data (same rules as for PNG files) and for the layer-mask check.
		enum : int
		{
			kBC1 = 71, kBC1s = 72, kBC2 = 74, kBC2s = 75, kBC3 = 77, kBC3s = 78, kBC4 = 80, kBC4sn = 81, kBC5 = 83, kBC5sn = 84,
			kBC6u = 95, kBC6s = 96, kBC7 = 98, kBC7s = 99,
		};
		int BlockBytes(int f)
		{
			switch (f) {
			case kBC1: case kBC1s: case kBC4: case kBC4sn: return 8;
			case kBC2: case kBC2s: case kBC3: case kBC3s: case kBC5: case kBC5sn: case kBC6u: case kBC6s: case kBC7: case kBC7s: return 16;
			default: return 0;
			}
		}
		const char* BcName(int f)
		{
			switch (f) {
			case kBC1: case kBC1s: return "BC1";
			case kBC2: case kBC2s: return "BC2";
			case kBC3: case kBC3s: return "BC3";
			case kBC4: return "BC4";
			case kBC5: return "BC5";
			case kBC7: case kBC7s: return "BC7";
			default: return "BC?";
			}
		}
		size_t LevelBytes(int f, int w, int h) { return size_t(std::max(1, (w + 3) / 4)) * size_t(std::max(1, (h + 3) / 4)) * size_t(BlockBytes(f)); }

		// one compressed level -> RGBA8 (BC4: red only, BC5: red/green; the rest 0, alpha 255)
		void DecodeBlocks(int f, const uint8_t* src, int w, int h, std::vector<uint8_t>& out)
		{
			const int bw = std::max(1, (w + 3) / 4), bh = std::max(1, (h + 3) / 4);
			const int pw = bw * 4, ph = bh * 4;
			std::vector<uint8_t> full(size_t(pw) * ph * 4, 0);
			const int            bb = BlockBytes(f);
			std::vector<uint8_t> tmp(16 * 2);
			for (int by = 0; by < bh; ++by) {
				for (int bx = 0; bx < bw; ++bx) {
					const uint8_t* blk = src + (size_t(by) * bw + bx) * bb;
					uint8_t*       dst = full.data() + (size_t(by) * 4 * pw + size_t(bx) * 4) * 4;
					switch (f) {
					case kBC1: case kBC1s: bcdec_bc1(blk, dst, pw * 4); break;
					case kBC2: case kBC2s: bcdec_bc2(blk, dst, pw * 4); break;
					case kBC3: case kBC3s: bcdec_bc3(blk, dst, pw * 4); break;
					case kBC7: case kBC7s: bcdec_bc7(blk, dst, pw * 4); break;
					case kBC4: {
						bcdec_bc4(blk, tmp.data(), 4);
						for (int y = 0; y < 4; ++y)
							for (int x = 0; x < 4; ++x) {
								uint8_t* o = dst + (size_t(y) * pw + x) * 4;
								o[0] = tmp[y * 4 + x];
								o[3] = 255;
							}
						break;
					}
					case kBC5: {
						bcdec_bc5(blk, tmp.data(), 8);
						for (int y = 0; y < 4; ++y)
							for (int x = 0; x < 4; ++x) {
								uint8_t* o = dst + (size_t(y) * pw + x) * 4;
								o[0] = tmp[y * 8 + x * 2];
								o[1] = tmp[y * 8 + x * 2 + 1];
								o[3] = 255;
							}
						break;
					}
					default: break;
					}
				}
			}
			out.assign(size_t(w) * h * 4, 0);
			for (int y = 0; y < h; ++y) memcpy(out.data() + size_t(y) * w * 4, full.data() + size_t(y) * pw * 4, size_t(w) * 4);
		}

		// channel statistics of an RGBA8 image (sampled)
		struct ChanStats
		{
			int    lo[4] = { 255, 255, 255, 255 }, hi[4] = { 0, 0, 0, 0 };
			double mean[4] = {};
		};
		ChanStats Stats(const uint8_t* px, size_t n)
		{
			ChanStats    st;
			double       sum[4] = {};
			const size_t step = std::max<size_t>(1, n / 65536);
			size_t       cnt = 0;
			for (size_t i = 0; i < n; i += step, ++cnt) {
				for (int c = 0; c < 4; ++c) {
					const int v = px[i * 4 + c];
					st.lo[c] = std::min(st.lo[c], v);
					st.hi[c] = std::max(st.hi[c], v);
					sum[c] += v;
				}
			}
			for (int c = 0; c < 4; ++c) st.mean[c] = cnt ? sum[c] / cnt / 255.0 : 0.0;
			return st;
		}

		bool DecodeDds(const uint8_t* d, size_t n, int maxSize, bool keepCompressed, Texture& t, std::string& why)
		{
			if (n < 128 || memcmp(d, "DDS ", 4) != 0) {
				why = "not a DDS file";
				return false;
			}
			auto u32 = [&](size_t o) {
				uint32_t v;
				memcpy(&v, d + o, 4);
				return v;
			};
			auto cc = [](char a, char b, char c, char e) { return uint32_t(uint8_t(a)) | (uint32_t(uint8_t(b)) << 8) | (uint32_t(uint8_t(c)) << 16) | (uint32_t(uint8_t(e)) << 24); };
			const uint32_t flags = u32(8), h = u32(12), w = u32(16), mipCount = u32(28);
			const uint32_t pfFlags = u32(80), fourCC = u32(84), bits = u32(88), rm = u32(92), gm = u32(96), bm = u32(100), am = u32(104), caps2 = u32(112);
			if (w == 0 || h == 0 || w > 16384 || h > 16384) {
				why = "bad DDS size";
				return false;
			}
			if (caps2 & 0x200) {
				why = "cube map (not used on cars)";
				return false;
			}
			size_t   off = 128;
			int      fmt = 0;   // DXGI format number
			bool     plain = false;
			uint32_t arraySize = 1;
			if ((pfFlags & 0x4) && fourCC == cc('D', 'X', '1', '0')) {
				if (n < 148) {
					why = "truncated DDS";
					return false;
				}
				fmt = int(u32(128));
				if (u32(132) != 3) {
					why = "not a 2D texture";
					return false;
				}
				arraySize = std::max<uint32_t>(1, u32(140));
				off = 148;
			} else if (pfFlags & 0x4) {
				if (fourCC == cc('D', 'X', 'T', '1')) fmt = kBC1;
				else if (fourCC == cc('D', 'X', 'T', '2') || fourCC == cc('D', 'X', 'T', '3')) fmt = kBC2;
				else if (fourCC == cc('D', 'X', 'T', '4') || fourCC == cc('D', 'X', 'T', '5')) fmt = kBC3;
				else if (fourCC == cc('A', 'T', 'I', '1') || fourCC == cc('B', 'C', '4', 'U')) fmt = kBC4;
				else if (fourCC == cc('A', 'T', 'I', '2') || fourCC == cc('B', 'C', '5', 'U')) fmt = kBC5;
				else {
					char fc[5] = { char(fourCC & 0xFF), char((fourCC >> 8) & 0xFF), char((fourCC >> 16) & 0xFF), char((fourCC >> 24) & 0xFF), 0 };
					why = std::string("unsupported DDS type '") + (isprint((unsigned char)fc[0]) ? fc : "?") + "'";
					return false;
				}
			} else {
				plain = true;
			}
			const int levels = (flags & 0x20000) ? int(std::max<uint32_t>(1, mipCount)) : 1;
			(void)arraySize;  // only the first slice is used (its mips come first in the file)

			// ---- plain pixels (rare in BeamNG): to RGBA8, then the PNG path
			auto toRgba = [&](std::vector<uint8_t>& px) -> bool {
				const size_t npx = size_t(w) * h;
				px.assign(npx * 4, 255);
				auto maskShift = [](uint32_t m) { int s = 0; while (m && !(m & 1)) { m >>= 1; ++s; } return s; };
				auto maskVal = [&](uint32_t v, uint32_t m) -> uint8_t {
					if (!m) return 0;
					const int      s = maskShift(m);
					const uint32_t mx = m >> s;
					return uint8_t(((v & m) >> s) * 255u / std::max<uint32_t>(1, mx));
				};
				if (fmt == 28 || fmt == 29 || fmt == 87 || fmt == 88 || fmt == 91 || fmt == 61 || fmt == 65 || fmt == 49) {
					const int bpp = (fmt == 61 || fmt == 65) ? 1 : fmt == 49 ? 2 : 4;
					if (off + npx * bpp > n) return false;
					const uint8_t* s = d + off;
					for (size_t i = 0; i < npx; ++i) {
						uint8_t* o = &px[i * 4];
						if (fmt == 28 || fmt == 29) { memcpy(o, s + i * 4, 4); }
						else if (fmt == 87 || fmt == 88 || fmt == 91) { o[0] = s[i * 4 + 2]; o[1] = s[i * 4 + 1]; o[2] = s[i * 4]; o[3] = fmt == 88 ? 255 : s[i * 4 + 3]; }
						else if (fmt == 61) { o[0] = s[i]; o[1] = o[2] = 0; }
						else if (fmt == 65) { o[0] = o[1] = o[2] = 0; o[3] = s[i]; }
						else { o[0] = s[i * 2]; o[1] = s[i * 2 + 1]; o[2] = 0; }
					}
					return true;
				}
				if (fmt != 0) return false;
				const int bpp = int(bits / 8);
				if (bpp < 1 || bpp > 4 || off + npx * bpp > n) return false;
				const uint8_t* s = d + off;
				for (size_t i = 0; i < npx; ++i) {
					uint32_t v = 0;
					memcpy(&v, s + i * bpp, size_t(bpp));
					uint8_t* o = &px[i * 4];
					if (pfFlags & 0x20000) {  // luminance (+ alpha)
						o[0] = o[1] = o[2] = maskVal(v, rm ? rm : 0xFF);
						o[3] = (pfFlags & 0x1) ? maskVal(v, am) : 255;
					} else if ((pfFlags & 0x2) && !(pfFlags & 0x40)) {  // alpha only
						o[0] = o[1] = o[2] = 0;
						o[3] = maskVal(v, am ? am : 0xFF);
					} else {
						o[0] = maskVal(v, rm);
						o[1] = maskVal(v, gm);
						o[2] = maskVal(v, bm);
						o[3] = (pfFlags & 0x1) && am ? maskVal(v, am) : 255;
					}
				}
				return true;
			};
			if (plain || BlockBytes(fmt) == 0) {
				if (fmt == kBC6u || fmt == kBC6s || fmt == kBC4sn || fmt == kBC5sn) {
					why = "unsupported DDS format (HDR or signed)";
					return false;
				}
				std::vector<uint8_t> px;
				if (!toRgba(px)) {
					why = "unsupported DDS pixel format (DXGI " + std::to_string(fmt) + ", " + std::to_string(bits) + " bits)";
					return false;
				}
				t.fileW = int(w);
				t.fileH = int(h);
				Pack(px.data(), int(w), int(h), 4, maxSize, t);
				t.note = "DDS plain pixels, " + t.note;
				return true;
			}

			// ---- block-compressed: where each level sits in the file
			std::vector<size_t> lofs;
			std::vector<int>    lw, lh;
			size_t              p = off;
			int                 cw = int(w), ch = int(h);
			for (int l = 0; l < levels; ++l) {
				const size_t sz = LevelBytes(fmt, cw, ch);
				if (p + sz > n) break;
				lofs.push_back(p);
				lw.push_back(cw);
				lh.push_back(ch);
				p += sz;
				if (cw == 1 && ch == 1) break;
				cw = std::max(1, cw / 2);
				ch = std::max(1, ch / 2);
			}
			if (lofs.empty()) {
				why = "truncated DDS data";
				return false;
			}
			t.fileW = int(w);
			t.fileH = int(h);
			// first level within MaxTextureSize
			size_t top = 0;
			while (top + 1 < lofs.size() && (lw[top] > maxSize || lh[top] > maxSize)) ++top;
			const bool fits = lw[top] <= maxSize && lh[top] <= maxSize;
			// D3D11 wants block-compressed textures to be a multiple of 4 at their top level
			const bool ok4 = (lw[top] % 4 == 0) && (lh[top] % 4 == 0);
			// a small level, decoded, for the channel decision and the mask check
			size_t th = top;
			while (th + 1 < lofs.size() && (lw[th] > 256 || lh[th] > 256) && lw[th + 1] >= 4 && lh[th + 1] >= 4) ++th;
			std::vector<uint8_t> thumb;
			DecodeBlocks(fmt, d + lofs[th], lw[th], lh[th], thumb);
			const ChanStats st = Stats(thumb.data(), size_t(lw[th]) * lh[th]);
			if (!keepCompressed || !fits || !ok4) {
				// decode one level and treat it like a PNG (shrunk to MaxTextureSize there)
				std::vector<uint8_t> px;
				DecodeBlocks(fmt, d + lofs[top], lw[top], lh[top], px);
				Pack(px.data(), lw[top], lh[top], 4, maxSize, t);
				t.note = std::string(BcName(fmt)) + " decoded (" + (!keepCompressed ? "TextureCompression=0" : !ok4 ? "size not a multiple of 4" : "no small mips") + "), " + t.note;
				return true;
			}
			t.dxgi = fmt;
			t.bpp = 0;
			t.w = lw[top];
			t.h = lh[top];
			t.mips.clear();
			for (size_t l = top; l < lofs.size(); ++l) {
				const size_t sz = LevelBytes(fmt, lw[l], lh[l]);
				t.mips.emplace_back(d + lofs[l], d + lofs[l] + sz);
			}
			char note[240];
			const bool alphaVaries = st.hi[3] - st.lo[3] > 8;
			const bool redFlat = st.hi[0] - st.lo[0] <= 8;
			if (t.kind == kTexSingle) {
				// BeamNG data maps: red, unless only alpha carries data
				const bool useAlpha = fmt != kBC4 && fmt != kBC5 && alphaVaries && redFlat;
				t.channel = useAlpha ? 3 : 0;
				t.swizzle = t.channel;
				t.thumb.swap(thumb);
				t.thumbW = lw[th];
				t.thumbH = lh[th];
				snprintf(note, sizeof(note), "%s %dx%d (file %dx%d, %zu mips), uses %s (mean r %.2f g %.2f b %.2f a %.2f, min/max red %d/%d alpha %d/%d)", BcName(fmt),
					t.w, t.h, t.fileW, t.fileH, t.mips.size(), useAlpha ? "alpha" : "red", st.mean[0], st.mean[1], st.mean[2], st.mean[3], st.lo[0], st.hi[0], st.lo[3],
					st.hi[3]);
			} else if (t.kind == kTexNormal) {
				const bool xInAlpha = fmt != kBC5 && alphaVaries && redFlat && st.hi[0] >= 240;
				t.swizzle = xInAlpha ? 1 : 0;
				snprintf(note, sizeof(note), "%s %dx%d (file %dx%d, %zu mips), normal X from %s (mean r %.2f g %.2f b %.2f a %.2f)", BcName(fmt), t.w, t.h, t.fileW,
					t.fileH, t.mips.size(), xInAlpha ? "alpha (DXT5nm)" : "red", st.mean[0], st.mean[1], st.mean[2], st.mean[3]);
			} else {
				snprintf(note, sizeof(note), "%s %dx%d (file %dx%d, %zu mips)", BcName(fmt), t.w, t.h, t.fileW, t.fileH, t.mips.size());
			}
			t.note = note;
			return true;
		}

		bool DecodeMemory(const uint8_t* data, size_t size, int maxSize, Texture& t, std::string& why)
		{
			int      w = 0, h = 0, c = 0;
			uint8_t* px = stbi_load_from_memory(data, int(size), &w, &h, &c, 4);
			if (!px) {
				why = stbi_failure_reason() ? stbi_failure_reason() : "decode failed";
				return false;
			}
			t.fileW = w;
			t.fileH = h;
			Pack(px, w, h, c, maxSize, t);
			stbi_image_free(px);
			return true;
		}

		bool DecodeEmbedded(const cgltf_image* img, int maxSize, Texture& t, std::string& why)
		{
			if (!(img->buffer_view && img->buffer_view->buffer && img->buffer_view->buffer->data) || img->buffer_view->size < 16) {
				why = "image has no embedded data";
				return false;
			}
			return DecodeMemory(cgltf_buffer_view_data(img->buffer_view), img->buffer_view->size, maxSize, t, why);
		}

		bool DecodeFile(const std::string& path, int maxSize, bool keepCompressed, Texture& t, std::string& why)
		{
			FILE* f = fopen(path.c_str(), "rb");
			if (!f) {
				why = "can't open " + path;
				return false;
			}
			fseek(f, 0, SEEK_END);
			const long n = ftell(f);
			fseek(f, 0, SEEK_SET);
			std::vector<uint8_t> buf(n > 0 ? size_t(n) : 0);
			const size_t got = n > 0 ? fread(buf.data(), 1, buf.size(), f) : 0;
			fclose(f);
			if (got != buf.size() || buf.empty()) {
				why = "can't read " + path;
				return false;
			}
			if (buf.size() >= 4 && memcmp(buf.data(), "DDS ", 4) == 0) {
				return DecodeDds(buf.data(), buf.size(), maxSize, keepCompressed, t, why);
			}
			return DecodeMemory(buf.data(), buf.size(), maxSize, t, why);
		}

		bool IsIdentity(const float* m)
		{
			for (int i = 0; i < 16; ++i) {
				const float want = (i % 5 == 0) ? 1.0f : 0.0f;
				if (fabsf(m[i] - want) > 1e-6f) return false;
			}
			return true;
		}

		bool Num(const J* j, double& out) { return j && j->num(out); }

		void Vec4(const J* j, float* out)
		{
			if (!j) return;
			auto it = j->items();
			for (int k = 0; k < 4 && k < int(it.size()); ++k) {
				double d;
				if (it[k]->num(d)) out[k] = float(d);
			}
		}

		std::string DirOf(const char* path)
		{
			std::string p(path);
			const size_t s = p.find_last_of("/\\");
			return s == std::string::npos ? std::string() : p.substr(0, s + 1);
		}

		std::string Lower(std::string s)
		{
			for (char& ch : s) ch = char(tolower((unsigned char)ch));
			return s;
		}

		std::string BaseName(const std::string& p)
		{
			const size_t s = p.find_last_of("/\\");
			return s == std::string::npos ? p : p.substr(s + 1);
		}

		int BlendOpOf(const std::string& s)
		{
			const std::string l = Lower(s);
			if (l.find("premul") != std::string::npos) return kBlendPremul;
			if (l == "addalpha") return kBlendAddAlpha;
			if (l == "add") return kBlendAdd;
			if (l == "mul") return kBlendMul;
			return kBlendLerp;  // LerpAlpha, None, empty
		}

		const char* KindName(int k)
		{
			return k == kTexSingle ? "data" : k == kTexNormal ? "normal" : k == kTexMask ? "mask" : "colour";
		}

		// texel of an image at a texture coordinate (wrapping, nearest)
		size_t TexelIndex(float u, float v, int w, int h)
		{
			u -= floorf(u);
			v -= floorf(v);
			const int x = std::min(w - 1, std::max(0, int(u * w)));
			const int y = std::min(h - 1, std::max(0, int(v * h)));
			return size_t(y) * w + x;
		}
	}

	bool Load(const char* path, const Options& opt, Model& out, std::string& err, const LogFn& log)
	{
		const auto t0 = std::chrono::steady_clock::now();
		auto       msSince = [&]() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count(); };
		out = Model{};
		const std::string baseDir = DirOf(path);

		cgltf_options co{};
		cgltf_data*   data = nullptr;
		cgltf_result  res = cgltf_parse_file(&co, path, &data);
		if (res != cgltf_result_success) {
			static const char* names[] = { "ok", "data too short", "unknown format", "invalid json", "invalid gltf", "invalid options", "file not found",
				                           "io error", "out of memory", "legacy gltf" };
			err = std::string("could not read the file: ") + (int(res) < 10 ? names[int(res)] : "error");
			return false;
		}
		std::unique_ptr<cgltf_data, void (*)(cgltf_data*)> holder(data, cgltf_free);
		res = cgltf_load_buffers(&co, data, path);
		if (res != cgltf_result_success) {
			err = "could not load the file's data buffers";
			return false;
		}
		Logf(log, "car file: %zu nodes, %zu meshes, %zu materials, %zu images, %zu accessors (parsed in %.0f ms)", data->nodes_count, data->meshes_count,
			data->materials_count, data->images_count, data->accessors_count, msSince());

		// ---- root extras: paint, vehicle name, wheels written by the BeamNG mod
		J        root;
		const J* wheelsJ = nullptr;
		V3       vehPos{};
		bool     haveVehPos = false;
		if (data->extras.data && ParseJson(data->extras.data, root)) {
			if (const J* gb = root.get("gtaBridge")) {
				if (const J* v = gb->get("vehicle")) out.vehicle = v->s;
				if (const J* v = gb->get("version")) out.modVersion = v->s;
				if (const J* p = gb->get("paint")) {
					const char* keys[3] = { "color", "palette0", "palette1" };
					for (int i = 0; i < 3; ++i) {
						if (const J* c = p->get(keys[i])) {
							Vec4(c, out.paint[i]);
							out.hasPaint = true;
						}
					}
					// v1.9.6: {metallic, roughness, clear coat, clear coat roughness} per paint slot
					if (const J* pb = p->get("pbr")) {
						auto it = pb->items();
						for (int i = 0; i < 3 && i < int(it.size()); ++i) {
							if (it[i]->items().size() >= 4) {
								float v[4] = { out.paintPbr[i][0], out.paintPbr[i][1], out.paintPbr[i][2], out.paintPbr[i][3] };
								Vec4(it[i], v);
								bool finite = true;
								for (float x : v) finite = finite && std::isfinite(x);
								if (finite) {
									memcpy(out.paintPbr[i], v, sizeof(v));
									out.hasPaintPbr = true;
								}
							}
						}
						for (int i = int(it.size()); i < 3 && out.hasPaintPbr; ++i) memcpy(out.paintPbr[i], out.paintPbr[0], sizeof(out.paintPbr[i]));
					}
					if (const J* ps = p->get("pbrSource")) out.paintPbrSource = ps->s;
				}
				wheelsJ = gb->get("wheels");
				if (const J* vp = gb->get("vehPos")) {
					auto it = vp->items();
					if (it.size() >= 3) {
						it[0]->num(vehPos.x), it[1]->num(vehPos.y), it[2]->num(vehPos.z);
						haveVehPos = true;
					}
				}
			}
		}

		// ---- driving direction at export time (glTF axes), stored on the scene's root nodes
		V3   dirG{ 0, 0, 0 }, upG{ 0, 0, 0 };
		bool haveDir = false;
		for (size_t i = 0; i < data->nodes_count && !haveDir; ++i) {
			const cgltf_node& nd = data->nodes[i];
			J                 ex;
			if (!nd.extras.data || !ParseJson(nd.extras.data, ex)) continue;
			const J* d = ex.get("BNG_Direction");
			if (!d) continue;
			const J* dir = d->get("dir");
			const J* up = d->get("up");
			if (!dir || !up) continue;
			auto a = dir->items(), b = up->items();
			if (a.size() < 3 || b.size() < 3) continue;
			a[0]->num(dirG.x), a[1]->num(dirG.y), a[2]->num(dirG.z);
			b[0]->num(upG.x), b[1]->num(upG.y), b[2]->num(upG.z);
			haveDir = Dot(dirG, dirG) > 0.1 && Dot(upG, upG) > 0.1;
		}

		// ---- textures in use: embedded images (old files) or PNG files next to car.gltf (v1.7+ mod)
		struct TexRef
		{
			const cgltf_image* img = nullptr;
			std::string        file;
			int                kind = kTexColor;
		};
		std::vector<TexRef>        texRefs;
		std::map<std::string, int> texByKey;
		auto useImage = [&](const cgltf_image* img, int kind) -> int {
			if (!img) return -1;
			char key[64];
			snprintf(key, sizeof(key), "img:%p:%d", static_cast<const void*>(img), kind);
			auto it = texByKey.find(key);
			if (it != texByKey.end()) return it->second;
			TexRef r;
			r.img = img;
			r.kind = kind;
			texRefs.push_back(r);
			return texByKey[key] = int(texRefs.size() - 1);
		};
		auto useFile = [&](const J* j, int kind) -> int {
			if (!j || j->t != J::Str || j->s.empty()) return -1;
			const std::string key = std::to_string(kind) + ":" + j->s;
			auto              it = texByKey.find(key);
			if (it != texByKey.end()) return it->second;
			TexRef r;
			r.file = baseDir + j->s;
			r.kind = kind;
			texRefs.push_back(r);
			return texByKey[key] = int(texRefs.size() - 1);
		};
		auto imageByIndex = [&](int i) -> const cgltf_image* { return (i >= 0 && size_t(i) < data->images_count) ? &data->images[i] : nullptr; };
		auto uvOf = [&](const J* L, const char* key) -> int {
			double d = 0;
			return (Num(L->get(key), d) && d > 0.5) ? 1 : 0;
		};

		// ---- materials
		int paintable = 0, glassy = 0, layered = 0, normalLayers = 0, surfLayers = 0, blackCube = 0;
		int blendCount[5] = {};
		out.materials.resize(data->materials_count + 1);  // the last one is for primitives without a material
		out.materials.back().name = "(none)";
		for (size_t i = 0; i < data->materials_count; ++i) {
			const cgltf_material& m = data->materials[i];
			Material&             mm = out.materials[i];
			mm.name = m.name ? m.name : "";
			J ex;
			const bool hasEx = m.extras.data && ParseJson(m.extras.data, ex);
			const J*   gta = hasEx ? ex.get("gta") : nullptr;
			if (gta) {
				// v1.7+ mod: every layer as BeamNG defines it
				mm.fromMod = true;
				++out.modMaterials;
				double d = 0;
				if (Num(gta->get("v"), d)) mm.version = float(d);
				if (const J* tr = gta->get("translucent")) mm.translucent = tr->truthy();
				if (const J* bo = gta->get("blendOp")) mm.blendOp = BlendOpOf(bo->s);
				if (const J* ds = gta->get("doubleSided"))  // v1.9.5 (an empty value = unknown: stays double-sided)
					if (!(ds->t == J::Str && ds->s.empty()) && ds->t != J::Null) mm.doubleSided = ds->truthy();
				if (const J* cm = gta->get("cubemap")) {
					mm.cubemap = cm->s;
					mm.noReflections = Lower(cm->s).find("black") != std::string::npos;
				}
				// alpha test only when BeamNG has it on (alphaRef alone is set on most materials)
				const J* at = gta->get("alphaTest");
				double   aref = 0;
				if (at && at->truthy() && Num(gta->get("alphaRef"), aref)) mm.alphaCut = float(std::clamp(aref, 1.0, 255.0) / 255.0);
				const J* layers = gta->get("layers");
				auto     li = layers ? layers->items() : std::vector<const J*>{};
				mm.nLayers = std::max(1, std::min(4, int(li.size())));
				for (int l = 0; l < mm.nLayers && l < int(li.size()); ++l) {
					const J* L = li[l];
					Layer&   y = mm.layers[l];
					Vec4(L->get("baseColorFactor"), y.factor);
					y.baseTex = useFile(L->get("baseColorMap"), kTexColor);
					y.baseUV = uvOf(L, "diffuseMapUseUV");
					if (const J* lv = L->get("baseColorLive")) y.live = lv->truthy();
					out.liveLayers += y.live ? 1 : 0;
					if (const J* pd = L->get("instanceDiffuse")) y.paint = pd->truthy();
					if (const J* pc = L->get("paletteConst")) y.palConst = pc->s == "R" ? 0 : pc->s == "G" ? 1 : pc->s == "B" ? 2 : -1;
					y.palTex = useFile(L->get("colorPaletteMap"), kTexMask);
					y.palUV = uvOf(L, "colorPaletteMapUseUV");
					y.opTex = useFile(L->get("opacityMap"), kTexSingle);
					y.opUV = uvOf(L, "opacityMapUseUV");
					if (Num(L->get("opacityFactor"), d)) y.opFactor = float(d);
					if (Num(L->get("metallicFactor"), d)) y.metallic = float(std::clamp(d, 0.0, 1.0));
					if (Num(L->get("roughnessFactor"), d)) y.roughness = float(std::clamp(d, 0.0, 1.0));
					if (Num(L->get("clearCoatFactor"), d)) y.clearCoat = float(std::clamp(d, 0.0, 1.0));
					if (opt.normalMaps) {
						y.normTex = useFile(L->get("normalMap"), kTexNormal);
						y.normUV = uvOf(L, "normalMapUseUV");
						normalLayers += y.normTex >= 0 ? 1 : 0;
					}
					if (opt.surfaceMaps) {
						y.metalTex = useFile(L->get("metallicMap"), kTexSingle);
						y.metalUV = uvOf(L, "metallicMapUseUV");
						y.roughTex = useFile(L->get("roughnessMap"), kTexSingle);
						y.roughUV = uvOf(L, "roughnessMapUseUV");
						surfLayers += (y.metalTex >= 0 || y.roughTex >= 0) ? 1 : 0;
					}
					if (l == 0) {
						mm.metallic = y.metallic;
						mm.roughness = y.roughness;
						mm.clearCoat = y.clearCoat;
					}
					paintable += y.paint ? 1 : 0;
				}
				layered += mm.nLayers > 1 ? 1 : 0;
			} else {
				// older files: what BeamNG's exporter wrote (first layer only, textures embedded)
				Layer& y = mm.layers[0];
				if (m.has_pbr_metallic_roughness) {
					memcpy(y.factor, m.pbr_metallic_roughness.base_color_factor, sizeof(y.factor));
					y.baseTex = useImage(m.pbr_metallic_roughness.base_color_texture.texture ? m.pbr_metallic_roughness.base_color_texture.texture->image : nullptr, kTexColor);
				}
				const J* bm = hasEx ? ex.get("bngMaterial") : nullptr;
				if (bm) {
					double ver = 0;
					if (Num(bm->get("version"), ver)) mm.version = float(ver);
					if (y.baseTex < 0) {
						int idx = FirstIndex(bm->get("baseColorMapIndex"));
						if (idx < 0) idx = FirstIndex(bm->get("colorMapIndex"));
						y.baseTex = useImage(imageByIndex(idx), kTexColor);
					}
					bool inst = false;
					if (const J* id = bm->get("instanceDiffuse")) {
						for (const J* x : id->items()) inst = inst || x->truthy();
					}
					y.paint = inst;
					const int pal = FirstIndex(bm->get("colorPaletteMapIndex"));
					if (pal >= 0) {
						y.paint = true;
						y.palTex = useImage(imageByIndex(pal), kTexMask);
					}
					if (const J* tr = bm->get("translucent")) mm.translucent = tr->truthy();
					double aref = 0;
					if (Num(bm->get("alphaRef"), aref) && aref > 0) mm.alphaCut = float(std::min(aref, 255.0) / 255.0);
				}
				paintable += y.paint ? 1 : 0;
			}
			if (m.alpha_mode == cgltf_alpha_mode_blend) mm.translucent = true;
			{
				const std::string lower = Lower(mm.name);
				const bool endsMirror = lower.size() >= 7 && lower.compare(lower.size() - 7, 7, "_mirror") == 0;
				if (lower == "mirror" || endsMirror || lower.find("mirrorglass") != std::string::npos || lower.find("mirror_glass") != std::string::npos) {
					mm.mirror = true;
					mm.translucent = false;
				}
			}
			if (!mm.translucent && mm.alphaCut <= 0) mm.alphaCut = opt.alphaCutDefault;
			glassy += mm.translucent ? 1 : 0;
			blackCube += mm.noReflections ? 1 : 0;
			if (mm.translucent) ++blendCount[std::clamp(mm.blendOp, 0, 4)];
		}
		Logf(log, "car materials: %zu (%d described by the BeamNG mod %s, %d with several layers), paint layers %d, see-through %d (blend: lerp %d, premul %d, add %d, mul %d), "
		          "no-reflection (black cubemap) %d, layers with normal maps %d, with metallic/roughness maps %d, live screens %d, textures used %zu; paint %s (%.3f %.3f %.3f / %.3f %.3f %.3f / %.3f %.3f %.3f)",
			data->materials_count, out.modMaterials, out.modVersion.empty() ? "?" : out.modVersion.c_str(), layered, paintable, glassy, blendCount[0], blendCount[1],
			blendCount[2] + blendCount[3], blendCount[4], blackCube, normalLayers, surfLayers, out.liveLayers, texRefs.size(), out.hasPaint ? "found" : "NOT found",
			out.paint[0][0], out.paint[0][1], out.paint[0][2], out.paint[1][0], out.paint[1][1], out.paint[1][2], out.paint[2][0], out.paint[2][1], out.paint[2][2]);

		// ---- geometry: every node with a mesh, in glTF space (node transforms applied)
		struct Key
		{
			const cgltf_accessor *pos, *nrm, *uv0, *uv1;
			float                 m[16];
			bool operator<(const Key& o) const
			{
				if (pos != o.pos) return pos < o.pos;
				if (nrm != o.nrm) return nrm < o.nrm;
				if (uv0 != o.uv0) return uv0 < o.uv0;
				if (uv1 != o.uv1) return uv1 < o.uv1;
				return memcmp(m, o.m, sizeof(m)) < 0;
			}
		};
		std::map<Key, uint32_t> baseOf;  // shared vertex blocks (BeamNG flexbodies all share one) are converted once
		std::vector<V3>         gpos, gnrm;
		std::vector<bool>       hasNrm;
		std::vector<float>      tmp;
		struct PrimRec
		{
			uint32_t start, count;
			int      mat;
			bool     translucent;
			int      node;
		};
		std::vector<PrimRec> prims;
		size_t               skippedPrims = 0, nodesWithMesh = 0;
		auto unpackUV = [&](const cgltf_accessor* a, std::vector<float>& v) {
			v.clear();
			if (a && a->count >= 1) {
				v.resize(a->count * 2);
				cgltf_accessor_unpack_floats(a, v.data(), a->count * 2);
			}
		};
		for (size_t ni = 0; ni < data->nodes_count; ++ni) {
			const cgltf_node& nd = data->nodes[ni];
			if (!nd.mesh) continue;
			++nodesWithMesh;
			float M[16];
			cgltf_node_transform_world(&nd, M);
			const bool ident = IsIdentity(M);
			for (size_t pi = 0; pi < nd.mesh->primitives_count; ++pi) {
				const cgltf_primitive& pr = nd.mesh->primitives[pi];
				if (pr.type != cgltf_primitive_type_triangles || !pr.indices) {
					++skippedPrims;
					continue;
				}
				const cgltf_accessor *pos = nullptr, *nrm = nullptr, *uv0 = nullptr, *uv1 = nullptr;
				for (size_t ai = 0; ai < pr.attributes_count; ++ai) {
					const cgltf_attribute& at = pr.attributes[ai];
					if (at.type == cgltf_attribute_type_position) pos = at.data;
					if (at.type == cgltf_attribute_type_normal) nrm = at.data;
					if (at.type == cgltf_attribute_type_texcoord && at.index == 0) uv0 = at.data;
					if (at.type == cgltf_attribute_type_texcoord && at.index == 1) uv1 = at.data;
				}
				if (!pos || pos->count == 0) {
					++skippedPrims;
					continue;
				}
				Key k{ pos, nrm, uv0, uv1, {} };
				memcpy(k.m, M, sizeof(M));
				auto     it = baseOf.find(k);
				uint32_t base;
				if (it != baseOf.end()) {
					base = it->second;
				} else {
					base = uint32_t(gpos.size());
					baseOf[k] = base;
					const size_t n = pos->count;
					tmp.resize(n * 3);
					cgltf_accessor_unpack_floats(pos, tmp.data(), n * 3);
					std::vector<float> nt, ut0, ut1;
					if (nrm && nrm->count == n) {
						nt.resize(n * 3);
						cgltf_accessor_unpack_floats(nrm, nt.data(), n * 3);
					}
					unpackUV(uv0, ut0);
					unpackUV(uv1, ut1);
					for (size_t v = 0; v < n; ++v) {
						V3 p{ tmp[v * 3], tmp[v * 3 + 1], tmp[v * 3 + 2] };
						if (!ident) p = { M[0] * p.x + M[4] * p.y + M[8] * p.z + M[12], M[1] * p.x + M[5] * p.y + M[9] * p.z + M[13],
							            M[2] * p.x + M[6] * p.y + M[10] * p.z + M[14] };
						gpos.push_back(p);
						V3 q{ 0, 0, 0 };
						if (!nt.empty()) {
							q = { nt[v * 3], nt[v * 3 + 1], nt[v * 3 + 2] };
							if (!ident) q = Norm({ M[0] * q.x + M[4] * q.y + M[8] * q.z, M[1] * q.x + M[5] * q.y + M[9] * q.z, M[2] * q.x + M[6] * q.y + M[10] * q.z });
						}
						gnrm.push_back(q);
						hasNrm.push_back(!nt.empty());
						Vtx vx{};
						if (!ut0.empty() && v < uv0->count) {
							vx.uv0[0] = ut0[v * 2];
							vx.uv0[1] = ut0[v * 2 + 1];
						}
						if (!ut1.empty() && v < uv1->count) {
							vx.uv1[0] = ut1[v * 2];
							vx.uv1[1] = ut1[v * 2 + 1];
						} else {
							vx.uv1[0] = vx.uv0[0];
							vx.uv1[1] = vx.uv0[1];
						}
						out.verts.push_back(vx);
					}
				}
				const size_t ic = pr.indices->count;
				const size_t start = out.indices.size();
				out.indices.resize(start + ic);
				cgltf_accessor_unpack_indices(pr.indices, out.indices.data() + start, 4, ic);
				const uint32_t limit = uint32_t(pos->count);
				size_t         bad = 0;
				for (size_t q = start; q < start + ic; ++q) {
					if (out.indices[q] >= limit) {
						out.indices[q] = 0;
						++bad;
					}
					out.indices[q] += base;
				}
				if (bad) Logf(log, "car: %zu out-of-range indices in a part of '%s' (set to 0)", bad, nd.name ? nd.name : "?");
				const int mat = pr.material ? int(cgltf_material_index(data, pr.material)) : int(data->materials_count);
				prims.push_back({ uint32_t(start), uint32_t(ic), mat, out.materials[mat].translucent, int(ni) });
			}
		}
		if (out.indices.empty()) {
			err = "the file has no triangles";
			return false;
		}

		// ---- find which way is up and forward, then put the car in car space
		// Candidates: axes (file is glTF y-up as export.lua intends / raw BeamNG z-up) x orientation
		// (undo the driving direction recorded at export / BeamNG vehicle space where -y is forward).
		const V3 dirB = Norm(FromGltf(dirG)), upB = Norm(FromGltf(upG));
		struct Cand
		{
			int         axes, orient;
			V3          r, f, u;
			double      ex, ey, ez;
			int         score;
			const char* name;
		};
		std::vector<Cand> cands;
		for (int axes = 0; axes < 2; ++axes) {
			for (int orient = 0; orient < 2; ++orient) {
				if (opt.axes >= 0 && axes != opt.axes) continue;
				if (opt.orient >= 0 && orient != opt.orient) continue;
				if (orient == 0 && !haveDir) continue;
				Cand c{};
				c.axes = axes;
				c.orient = orient;
				if (orient == 0) {
					c.f = dirB;
					c.u = Norm(Sub(upB, V3{ dirB.x * Dot(upB, dirB), dirB.y * Dot(upB, dirB), dirB.z * Dot(upB, dirB) }));
				} else {
					c.f = { 0, -1, 0 };
					c.u = { 0, 0, 1 };
				}
				c.r = Cross(c.f, c.u);
				double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 };
				const size_t step = std::max<size_t>(1, gpos.size() / 20000);
				for (size_t i = 0; i < gpos.size(); i += step) {
					const V3 p = axes == 0 ? FromGltf(gpos[i]) : gpos[i];
					const double v[3] = { Dot(p, c.r), Dot(p, c.f), Dot(p, c.u) };
					for (int k = 0; k < 3; ++k) {
						lo[k] = std::min(lo[k], v[k]);
						hi[k] = std::max(hi[k], v[k]);
					}
				}
				c.ex = hi[0] - lo[0];
				c.ey = hi[1] - lo[1];
				c.ez = hi[2] - lo[2];
				// a car is longest front to back and lower than it is wide
				c.score = (c.ey >= c.ex && c.ey >= c.ez ? 2 : 0) + (c.ez <= c.ex * 1.05 ? 1 : 0);
				c.name = axes == 0 ? (orient == 0 ? "glTF axes, export direction" : "glTF axes, vehicle space")
				                   : (orient == 0 ? "BeamNG axes, export direction" : "BeamNG axes, vehicle space");
				cands.push_back(c);
			}
		}
		if (cands.empty()) {
			err = "no usable orientation (the file has no driving direction and the ini forces one)";
			return false;
		}
		const Cand* best = &cands[0];
		for (const Cand& c : cands) {
			Logf(log, "car orientation guess '%s': %.2f wide, %.2f long, %.2f tall (score %d)", c.name, c.ex, c.ey, c.ez, c.score);
			if (c.score > best->score) best = &c;
		}
		out.frame = best->name;
		auto toCar = [&](V3 g) {
			const V3 p = best->axes == 0 ? FromGltf(g) : g;
			return V3{ Dot(p, best->r), Dot(p, best->f), Dot(p, best->u) };
		};

		std::vector<V3> cpos(gpos.size());
		for (size_t i = 0; i < gpos.size(); ++i) cpos[i] = toCar(gpos[i]);
		// only vertices that triangles use count for the size (unused ones can be anywhere)
		std::vector<uint8_t> used(cpos.size(), 0);
		for (uint32_t i : out.indices) used[i] = 1;
		double lo[3] = { 1e30, 1e30, 1e30 }, hi[3] = { -1e30, -1e30, -1e30 };
		for (size_t i = 0; i < cpos.size(); ++i) {
			if (!used[i]) continue;
			const double v[3] = { cpos[i].x, cpos[i].y, cpos[i].z };
			for (int k = 0; k < 3; ++k) {
				lo[k] = std::min(lo[k], v[k]);
				hi[k] = std::max(hi[k], v[k]);
			}
		}
		const V3 shift{ (lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5, lo[2] };
		out.size[0] = float(hi[0] - lo[0]);
		out.size[1] = float(hi[1] - lo[1]);
		out.size[2] = float(hi[2] - lo[2]);
		size_t missingNormals = 0;
		for (size_t i = 0; i < cpos.size(); ++i) {
			cpos[i] = Sub(cpos[i], shift);
			Vtx& v = out.verts[i];
			v.p[0] = float(cpos[i].x);
			v.p[1] = float(cpos[i].y);
			v.p[2] = float(cpos[i].z);
			if (hasNrm[i]) {
				const V3 n0 = best->axes == 0 ? FromGltf(gnrm[i]) : gnrm[i];
				const V3 n = Norm({ Dot(n0, best->r), Dot(n0, best->f), Dot(n0, best->u) });
				v.n[0] = float(n.x);
				v.n[1] = float(n.y);
				v.n[2] = float(n.z);
			} else {
				++missingNormals;
			}
		}
		if (missingNormals) {
			// smooth normals from the triangles for vertices the file gave none
			std::vector<V3> acc(cpos.size());
			for (size_t t = 0; t + 2 < out.indices.size(); t += 3) {
				const uint32_t a = out.indices[t], b = out.indices[t + 1], c = out.indices[t + 2];
				const V3       n = Cross(Sub(cpos[b], cpos[a]), Sub(cpos[c], cpos[a]));
				for (uint32_t q : { a, b, c }) acc[q] = { acc[q].x + n.x, acc[q].y + n.y, acc[q].z + n.z };
			}
			for (size_t i = 0; i < cpos.size(); ++i) {
				if (hasNrm[i]) continue;
				const V3 n = Norm(acc[i]);
				out.verts[i].n[0] = float(n.x);
				out.verts[i].n[1] = float(n.y);
				out.verts[i].n[2] = float(n.z);
			}
		}
		Logf(log, "car geometry: %zu vertices, %zu triangles, %zu parts from %zu nodes (%zu skipped), %zu without normals; size %.2f wide x %.2f long x %.2f tall; using '%s'",
			out.verts.size(), out.indices.size() / 3, prims.size(), nodesWithMesh, skippedPrims, missingNormals, out.size[0], out.size[1], out.size[2],
			out.frame.c_str());

		// ---- v1.9.4: do the file's normals agree with its shape? In a user's export (BeamNG's own glTF
		// exporter) the positions were in glTF axes but the normals were not: every normal was turned 90
		// degrees, so light, reflections and the car's own occlusion were wrong on every surface that
		// doesn't face sideways (dark blotches). Each triangle is compared with its vertex normals for
		// the 24 axis-aligned turns; the turn that fits best is used. Parts that still don't fit get
		// normals rebuilt from their shape.
		{
			const bool best_axes0 = best->axes == 0;
			const V3   bestR = best->r, bestF = best->f, bestU = best->u;
			struct Turn
			{
				int p[3], s[3];
			};
			std::vector<Turn> turns;
			const int         perms[6][3] = { { 0, 1, 2 }, { 0, 2, 1 }, { 1, 0, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 2, 1, 0 } };
			int               identity = 0;
			for (const auto& pm : perms) {
				int inv = 0;
				for (int a = 0; a < 3; ++a)
					for (int b = a + 1; b < 3; ++b)
						if (pm[a] > pm[b]) ++inv;
				for (int sx = -1; sx <= 1; sx += 2)
					for (int sy = -1; sy <= 1; sy += 2)
						for (int sz = -1; sz <= 1; sz += 2) {
							if (((inv % 2) ? -1 : 1) * sx * sy * sz != 1) continue;  // turns only, no mirrors
							Turn t{ { pm[0], pm[1], pm[2] }, { sx, sy, sz } };
							if (pm[0] == 0 && pm[1] == 1 && pm[2] == 2 && sx == 1 && sy == 1 && sz == 1) identity = int(turns.size());
							turns.push_back(t);
						}
			}
			auto turned = [&](const Turn& t, const float* n) { return V3{ t.s[0] * double(n[t.p[0]]), t.s[1] * double(n[t.p[1]]), t.s[2] * double(n[t.p[2]]) }; };
			// fit of one triangle under each turn: its area where |vertex normal . face normal| > 0.8
			auto triFit = [&](size_t t, std::vector<double>& fit, double& total) {
				const uint32_t a = out.indices[t * 3], b = out.indices[t * 3 + 1], c = out.indices[t * 3 + 2];
				if (!hasNrm[a] || !hasNrm[b] || !hasNrm[c]) return;
				V3           f = Cross(Sub(cpos[b], cpos[a]), Sub(cpos[c], cpos[a]));
				const double area = sqrt(Dot(f, f));
				if (area < 1e-10) return;
				f = { f.x / area, f.y / area, f.z / area };
				float sn[3] = { out.verts[a].n[0] + out.verts[b].n[0] + out.verts[c].n[0], out.verts[a].n[1] + out.verts[b].n[1] + out.verts[c].n[1],
					            out.verts[a].n[2] + out.verts[b].n[2] + out.verts[c].n[2] };
				const double l = sqrt(double(sn[0]) * sn[0] + double(sn[1]) * sn[1] + double(sn[2]) * sn[2]);
				if (l < 1e-6) return;
				for (float& v : sn) v = float(v / l);
				total += area;
				for (size_t i = 0; i < turns.size(); ++i)
					if (fabs(Dot(turned(turns[i], sn), f)) > 0.8) fit[i] += area;
			};
			// the search runs in the file's own axes (before the car-space turn, which follows the car's heading
			// at export and would make a 90-degree mix-up in the file look like an odd angle)
			auto turnedV = [&](const Turn& t, const V3& n) {
				const double a[3] = { n.x, n.y, n.z };
				return V3{ t.s[0] * a[t.p[0]], t.s[1] * a[t.p[1]], t.s[2] * a[t.p[2]] };
			};
			auto fileFit = [&](size_t t, std::vector<double>& fit, double& total) {
				const uint32_t a = out.indices[t * 3], b = out.indices[t * 3 + 1], c = out.indices[t * 3 + 2];
				if (!hasNrm[a] || !hasNrm[b] || !hasNrm[c]) return;
				V3           f = Cross(Sub(gpos[b], gpos[a]), Sub(gpos[c], gpos[a]));
				const double area = sqrt(Dot(f, f));
				if (area < 1e-10) return;
				f = { f.x / area, f.y / area, f.z / area };
				const V3 sn = Norm({ gnrm[a].x + gnrm[b].x + gnrm[c].x, gnrm[a].y + gnrm[b].y + gnrm[c].y, gnrm[a].z + gnrm[b].z + gnrm[c].z });
				if (Dot(sn, sn) < 0.5) return;
				total += area;
				for (size_t i = 0; i < turns.size(); ++i)
					if (fabs(Dot(turnedV(turns[i], sn), f)) > 0.8) fit[i] += area;
			};
			std::vector<double> fit(turns.size(), 0.0);
			double              total = 0;
			const size_t        nTri = out.indices.size() / 3;
			const size_t        step = std::max<size_t>(1, nTri / 80000);
			for (size_t t = 0; t < nTri; t += step) fileFit(t, fit, total);
			if (total > 0) {
				int best = identity;
				for (size_t i = 0; i < turns.size(); ++i)
					if (fit[i] > fit[best] + 1e-9) best = int(i);
				const double idFit = fit[identity] / total, bestFit = fit[best] / total;
				auto         turnName = [&](const Turn& t) {
                    std::string s;
                    for (int k = 0; k < 3; ++k) {
                        if (k) s += ",";
                        s += t.s[k] < 0 ? "-" : "";
                        s += "xyz"[t.p[k]];
                    }
                    return s;
				};
				std::vector<uint8_t> done(out.verts.size(), 0);
				if (best != identity && bestFit > 0.6 && bestFit > idFit + 0.15) {
					const Turn& tb = turns[best];
					for (size_t i = 0; i < out.verts.size(); ++i) {
						if (!hasNrm[i]) continue;
						const V3 g = turnedV(tb, gnrm[i]);  // in the file's axes, then to car space as before
						const V3 n0 = best_axes0 ? FromGltf(g) : g;
						const V3 n = Norm({ Dot(n0, bestR), Dot(n0, bestF), Dot(n0, bestU) });
						out.verts[i].n[0] = float(n.x);
						out.verts[i].n[1] = float(n.y);
						out.verts[i].n[2] = float(n.z);
					}
					out.normalsFixed = 1;
					Logf(log, "car normals: the file's normals don't match the car's shape (%.0f%% fit), they are turned against it; turned them back (%s): %.0f%% fit",
						idFit * 100.0, turnName(tb).c_str(), bestFit * 100.0);
				} else {
					Logf(log, "car normals: they match the car's shape (%.0f%% fit)", idFit * 100.0);
				}
				// parts that still don't fit: normals rebuilt from the shape (smooth where vertices are shared)
				size_t rebuiltParts = 0, rebuiltTris = 0;
				for (const PrimRec& pr : prims) {
					std::vector<double> pf(turns.size(), 0.0);
					double              pt = 0;
					const size_t        t0 = pr.start / 3, t1 = (pr.start + pr.count) / 3;
					for (size_t t = t0; t < t1; ++t) triFit(t, pf, pt);
					if (pt <= 0 || pf[identity] / pt >= 0.5 || (t1 - t0) < 8) continue;  // identity = the normals as they are now
					std::map<uint32_t, V3> sum;
					for (size_t t = t0; t < t1; ++t) {
						const uint32_t a = out.indices[t * 3], b = out.indices[t * 3 + 1], c = out.indices[t * 3 + 2];
						const V3       f = Cross(Sub(cpos[b], cpos[a]), Sub(cpos[c], cpos[a]));
						for (uint32_t q : { a, b, c }) {
							V3& s = sum[q];
							s = { s.x + f.x, s.y + f.y, s.z + f.z };
						}
					}
					for (auto& kv : sum) {
						if (done[kv.first]) continue;
						const V3 n = Norm(kv.second);
						out.verts[kv.first].n[0] = float(n.x);
						out.verts[kv.first].n[1] = float(n.y);
						out.verts[kv.first].n[2] = float(n.z);
						done[kv.first] = 1;
					}
					++rebuiltParts;
					rebuiltTris += t1 - t0;
				}
				if (rebuiltParts) {
					out.normalsRebuilt = int(rebuiltParts);
					Logf(log, "car normals: %zu part(s) (%zu triangles) still didn't match their shape: their normals were rebuilt from the shape", rebuiltParts, rebuiltTris);
				}
			}
		}

		// ---- wheels: hub axes from the BeamNG mod; every mesh that sits inside a wheel joins it
		std::vector<int> primWheel(prims.size(), -1), primMode(prims.size(), 0);
		int              wheelOff = -1;  // v2.0: which offset the wheel nodes needed (0 none, 1 vehicle position), -1 no wheels
		if (opt.wheels && wheelsJ) {
			struct WIn
			{
				std::string name;
				V3          a, b;
				double      r, width;
				int         driven;
			};
			std::vector<WIn> wins;
			for (const J* w : wheelsJ->items()) {
				WIn  x{};
				auto c1 = w->get("c1") ? w->get("c1")->items() : std::vector<const J*>{};
				auto c2 = w->get("c2") ? w->get("c2")->items() : std::vector<const J*>{};
				if (c1.size() < 3 || c2.size() < 3) continue;
				c1[0]->num(x.a.x), c1[1]->num(x.a.y), c1[2]->num(x.a.z);
				c2[0]->num(x.b.x), c2[1]->num(x.b.y), c2[2]->num(x.b.z);
				x.r = 0.33;
				Num(w->get("r"), x.r);
				x.width = 0;
				Num(w->get("width"), x.width);
				x.driven = -1;
				if (const J* dv = w->get("driven")) {
					if (dv->t == J::Bool || dv->t == J::Num || dv->t == J::Str) x.driven = dv->truthy() ? 1 : 0;
				}
				if (const J* n = w->get("name")) x.name = n->s;
				wins.push_back(x);
			}
			// node positions may be relative to the car while the mesh is in the world (or the same): try both
			const V3 offsets[2] = { V3{ 0, 0, 0 }, haveVehPos ? vehPos : V3{ 0, 0, 0 } };
			int      bestOff = 0, bestInside = -1;
			double   bestMiss = 1e30;
			for (int o = 0; o < (haveVehPos ? 2 : 1); ++o) {
				int    inside = 0;
				double miss = 0;  // v2.0 tie-break: how far the hubs are from where hubs sit (radius above the ground, inside the footprint)
				for (const WIn& x : wins) {
					const V3 c = Sub(toCar({ (x.a.x + x.b.x) * 0.5 + offsets[o].x, (x.a.y + x.b.y) * 0.5 + offsets[o].y, (x.a.z + x.b.z) * 0.5 + offsets[o].z }), shift);
					if (fabs(c.x) < out.size[0] * 0.5 + 0.3 && fabs(c.y) < out.size[1] * 0.5 + 0.3 && c.z > -0.3 && c.z < out.size[2]) ++inside;
					const double r = x.r > 0.05 ? x.r : 0.33;
					miss += fabs(c.z - r) + std::max(0.0, fabs(c.x) - out.size[0] * 0.5) + std::max(0.0, fabs(c.y) - out.size[1] * 0.5);
				}
				if (inside > bestInside || (inside == bestInside && miss < bestMiss - 0.05)) {
					bestInside = inside;
					bestMiss = miss;
					bestOff = o;
				}
			}
			if (!wins.empty()) wheelOff = bestOff;
			for (const WIn& x : wins) {
				const V3 o = offsets[bestOff];
				const V3 a = Sub(toCar({ x.a.x + o.x, x.a.y + o.y, x.a.z + o.z }), shift);
				const V3 b = Sub(toCar({ x.b.x + o.x, x.b.y + o.y, x.b.z + o.z }), shift);
				Wheel    w;
				w.name = x.name;
				V3 axis = Norm(Sub(b, a));
				if (axis.x < 0) axis = { -axis.x, -axis.y, -axis.z };
				const V3 c{ (a.x + b.x) * 0.5, (a.y + b.y) * 0.5, (a.z + b.z) * 0.5 };
				w.c[0] = float(c.x), w.c[1] = float(c.y), w.c[2] = float(c.z);
				w.axis[0] = float(axis.x), w.axis[1] = float(axis.y), w.axis[2] = float(axis.z);
				w.radius = float(x.r > 0.05 ? x.r : 0.33);
				w.front = c.y > 0;
				w.driven = x.driven;
				out.wheels.push_back(w);
			}
			// assign each mesh node to a wheel by where its vertices are
			std::map<int, std::vector<size_t>> primsOfNode;
			for (size_t p = 0; p < prims.size(); ++p) primsOfNode[prims[p].node].push_back(p);
			std::vector<uint32_t> stamp(cpos.size(), 0);
			uint32_t              stampId = 0;
			for (auto& kv : primsOfNode) {
				++stampId;
				V3     sum{};
				size_t n = 0;
				std::vector<size_t> inW(out.wheels.size(), 0);
				for (size_t p : kv.second) {
					for (uint32_t q = prims[p].start; q < prims[p].start + prims[p].count; ++q) {
						const uint32_t vi = out.indices[q];
						if (stamp[vi] == stampId) continue;
						stamp[vi] = stampId;
						const V3& P = cpos[vi];
						sum = { sum.x + P.x, sum.y + P.y, sum.z + P.z };
						++n;
						for (size_t wi = 0; wi < out.wheels.size(); ++wi) {
							const Wheel& w = out.wheels[wi];
							const V3     d{ P.x - w.c[0], P.y - w.c[1], P.z - w.c[2] };
							const double along = d.x * w.axis[0] + d.y * w.axis[1] + d.z * w.axis[2];
							const V3     rad{ d.x - along * w.axis[0], d.y - along * w.axis[1], d.z - along * w.axis[2] };
							if (sqrt(Dot(rad, rad)) <= w.radius * 1.12 && fabs(along) <= 0.45) ++inW[wi];
						}
					}
				}
				if (!n) continue;
				const V3 cen{ sum.x / n, sum.y / n, sum.z / n };
				for (size_t wi = 0; wi < out.wheels.size(); ++wi) {
					if (inW[wi] < n * 0.97) continue;
					Wheel&       w = out.wheels[wi];
					const V3     d{ cen.x - w.c[0], cen.y - w.c[1], cen.z - w.c[2] };
					const double along = d.x * w.axis[0] + d.y * w.axis[1] + d.z * w.axis[2];
					const V3     rad{ d.x - along * w.axis[0], d.y - along * w.axis[1], d.z - along * w.axis[2] };
					const bool   onAxis = sqrt(Dot(rad, rad)) <= w.radius * 0.2;  // round things spin; calipers don't
					const int    mode = onAxis ? 1 : (w.front ? 2 : 0);
					for (size_t p : kv.second) {
						primWheel[p] = mode ? int(wi) : -1;
						primMode[p] = mode;
					}
					if (mode == 1) ++w.spinDraws;
					if (mode == 2) ++w.steerDraws;
					break;
				}
			}
			for (const Wheel& w : out.wheels) {
				Logf(log, "car wheel '%s': centre %.2f %.2f %.2f m, radius %.2f m, %s, %s, %d spinning part(s), %d steering-only part(s)", w.name.c_str(), w.c[0],
					w.c[1], w.c[2], w.radius, w.front ? "front" : "rear", w.driven < 0 ? "drive unknown" : w.driven ? "driven" : "not driven", w.spinDraws, w.steerDraws);
			}
			if (out.wheels.empty()) Logf(log, "car wheels: none in the file");
		} else if (opt.wheels) {
			Logf(log, "car wheels: the file has no wheel data (export it again with the v1.7+ BeamNG mod)");
		}

		// ---- v2.0: the car-space origin in BeamNG's vehicle frame (drive mode). Car-space axes are the
		// vehicle's own axes at export (BNG_Direction = getDirectionVector/Up) when the file is in glTF axes
		// and the export direction was used. The mesh is either in world positions (then the vehicle
		// position V sits inside it: origin = shift - toCar(V)) or relative to V (origin = shift). The wheel
		// nodes (relative to V) said which; without wheels, the reading that puts V closer to the car wins.
		out.vehFrameOk = haveDir && best->orient == 0 && best->axes == 0;
		{
			const V3 rel = shift;  // mesh relative to the vehicle position
			V3       world = shift;
			if (haveVehPos) world = Sub(shift, toCar(vehPos));
			V3          o = rel;
			const char* how = "mesh relative to the vehicle position";
			if (haveVehPos) {
				if (wheelOff == 1) {
					o = world;
					how = "mesh in world positions (wheel nodes needed the vehicle position)";
				} else if (wheelOff == 0) {
					o = rel;
					how = "mesh relative to the vehicle position (wheel nodes fitted as they are)";
				} else if (Dot(world, world) < Dot(rel, rel)) {
					o = world;
					how = "mesh in world positions (no wheels; the vehicle position is closer to the car that way)";
				} else {
					how = "mesh relative to the vehicle position (no wheels; closer that way)";
				}
			} else {
				how = "no vehicle position in the file: assumed relative";
			}
			out.vehOrigin[0] = float(o.x);
			out.vehOrigin[1] = float(o.y);
			out.vehOrigin[2] = float(o.z);
			out.vehOriginHow = how;
			Logf(log, "car origin in BeamNG's vehicle frame: %.3f %.3f %.3f m (%s); vehicle frame %s", o.x, o.y, o.z, how,
				out.vehFrameOk ? "usable for driving" : "NOT usable for driving (needs glTF axes + export direction)");
		}

		// ---- draws: opaque first, then see-through (glass) in file order
		std::vector<size_t> order(prims.size());
		for (size_t i = 0; i < order.size(); ++i) order[i] = i;
		std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) { return prims[a].translucent < prims[b].translucent; });
		for (size_t i : order) {
			const PrimRec& p = prims[i];
			Draw           d;
			d.start = p.start;
			d.count = p.count;
			d.mat = p.mat;
			d.wheel = primWheel[i];
			d.wheelMode = primMode[i];
			out.draws.push_back(d);
			out.opaqueDraws += p.translucent ? 0 : 1;
		}

		// ---- textures
		out.textures.resize(texRefs.size());
		size_t failed = 0;
		int    perKind[4] = {};
		for (size_t i = 0; i < texRefs.size(); ++i) {
			Texture& t = out.textures[i];
			t.kind = texRefs[i].kind;
			t.bpp = (t.kind == kTexSingle) ? 1 : (t.kind == kTexNormal) ? 2 : 4;
			t.name = texRefs[i].img ? (texRefs[i].img->name ? texRefs[i].img->name : "") : texRefs[i].file;
			if (!opt.loadTextures) continue;
			std::string why;
			const int   maxSize = std::max(16, (t.kind == kTexColor || t.kind == kTexMask) ? opt.maxTextureSize : opt.maxDataTextureSize);
			const bool  ok = texRefs[i].img ? DecodeEmbedded(texRefs[i].img, maxSize, t, why) : DecodeFile(texRefs[i].file, maxSize, opt.compressed, t, why);
			if (!ok) {
				++failed;
				Logf(log, "car texture '%s' (%s) not usable: %s", t.name.c_str(), KindName(t.kind), why.c_str());
				continue;
			}
			++perKind[std::clamp(t.kind, 0, 3)];
			(t.dxgi ? out.ddsTextures : out.pngTextures) += 1;
			for (auto& m : t.mips) out.textureBytes += m.size();
			if (!t.note.empty()) {
				Logf(log, "  texture %-58s %4dx%-4d %s", BaseName(t.name).c_str(), t.w, t.h, t.note.c_str());
			}
		}
		// layers whose texture failed draw without it
		for (Material& m : out.materials) {
			for (Layer& y : m.layers) {
				for (int* ti : { &y.baseTex, &y.palTex, &y.opTex, &y.normTex, &y.metalTex, &y.roughTex }) {
					if (*ti >= 0 && out.textures[*ti].mips.empty()) *ti = -1;
				}
			}
		}

		// ---- v1.9.1: layer masks that hide a layer on every triangle of its material.
		// BeamNG draws such layers (the user's RRpbr trims: layer 2 = black/chrome/steel, masked by a shared
		// "skin" opacity map that reads ~0 on the trims here), so a mask that is ~0 on all of the material's
		// own vertices is taken as unusable and the layer is drawn by its opacityFactor alone. What the
		// other channels of the same file say at those points is logged, to find the real rule later.
		if (opt.maskCheck && opt.loadTextures) {
			std::vector<std::vector<uint32_t>> vertsOfMat(out.materials.size());
			for (const PrimRec& p : prims) {
				if (p.mat < 0 || p.mat >= int(out.materials.size())) continue;
				std::vector<uint32_t>& v = vertsOfMat[p.mat];
				const uint32_t         step = std::max<uint32_t>(1, p.count / 400);
				for (uint32_t q = p.start; q < p.start + p.count; q += step) v.push_back(out.indices[q]);
			}
			for (size_t mi = 0; mi < out.materials.size(); ++mi) {
				Material& m = out.materials[mi];
				const std::vector<uint32_t>& vs = vertsOfMat[mi];
				if (!m.fromMod || vs.size() < 3) continue;
				for (int l = 1; l < m.nLayers; ++l) {
					Layer& y = m.layers[l];
					if (y.opTex < 0) continue;
					const Texture& t = out.textures[y.opTex];
					if (t.kind != kTexSingle || t.mips.empty() || t.mips[0].empty()) continue;
					if (t.dxgi && t.thumb.empty()) continue;
					int    kept = 0;
					double keptSum = 0, chMax[4] = {}, chSum[4] = {};
					for (uint32_t vi : vs) {
						const Vtx&  v = out.verts[vi];
						const float u = y.opUV ? v.uv1[0] : v.uv0[0];
						const float w = y.opUV ? v.uv1[1] : v.uv0[1];
						// compressed textures: the decoded small level (channel as the shader reads it)
						const int   k = t.dxgi ? t.thumb[TexelIndex(u, w, t.thumbW, t.thumbH) * 4 + t.channel] : t.mips[0][TexelIndex(u, w, t.w, t.h)];
						kept = std::max(kept, k);
						keptSum += k;
						if (!t.thumb.empty()) {
							const size_t ti = TexelIndex(u, w, t.thumbW, t.thumbH) * 4;
							for (int c = 0; c < 4; ++c) {
								chMax[c] = std::max(chMax[c], t.thumb[ti + c] / 255.0);
								chSum[c] += t.thumb[ti + c] / 255.0;
							}
						}
					}
					const double nv = double(vs.size());
					if (kept < 26) {  // below 0.1 everywhere
						y.maskIgnored = true;
						++out.masksIgnored;
						Logf(log, "car: material '%s' layer %d: its opacity map %s (%s channel) is %.2f at most on the %zu points sampled on this material -> layer drawn "
						          "without the mask (BeamNG shows it). File channels there: max r %.2f g %.2f b %.2f a %.2f, mean r %.2f g %.2f b %.2f a %.2f",
							m.name.c_str(), l + 1, BaseName(t.name).c_str(), t.channel == 3 ? "alpha" : "red", kept / 255.0, vs.size(), chMax[0], chMax[1], chMax[2],
							chMax[3], chSum[0] / nv, chSum[1] / nv, chSum[2] / nv, chSum[3] / nv);
					} else if (l == 1 && m.nLayers == 2) {
						Logf(log, "car: material '%s' layer 2 mask %s: mean %.2f, max %.2f on its %zu sampled points", m.name.c_str(), BaseName(t.name).c_str(),
							keptSum / nv / 255.0, kept / 255.0, vs.size());
					}
				}
			}
		}
		// thumbnails are only for the check above
		for (Texture& t : out.textures) {
			std::vector<uint8_t>().swap(t.thumb);
		}
		Logf(log, "car textures: %zu loaded (%zu failed): %d colour, %d paint masks, %d data, %d normal; %d straight from .dds (compressed), %d decoded (PNG/JPG); "
		          "%.1f MB in video memory (max size %d, data maps %d); %d layer mask(s) ignored; loaded in %.0f ms",
			texRefs.size() - failed, failed, perKind[0], perKind[1], perKind[2], perKind[3], out.ddsTextures, out.pngTextures, out.textureBytes / 1048576.0,
			opt.maxTextureSize, opt.maxDataTextureSize, out.masksIgnored, msSince());
		return true;
	}
}
