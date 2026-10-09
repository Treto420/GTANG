#pragma once
// carmodel.h - loads the glTF file that the BeamNG mod writes (BeamNG's exporter + the mod's own
// material/wheel data) into a ready-to-draw car model: one vertex/index buffer in car space,
// layered materials with paint, textures, and wheel groups that can spin/steer.
// Portable C++17 (no Windows/D3D), so it can be tested outside the game.
//
// Car space (what the plugin draws): metres, x = right, y = forward, z = up,
// origin = centre of the car's footprint at the bottom of the tyres.

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace car
{
	struct Vtx
	{
		float p[3];
		float n[3];
		float uv0[2];
		float uv1[2];  // BeamNG paint masks usually use the second UV set
	};

	// How a texture is stored on the GPU (v1.9: data maps are 1 or 2 channels to save video memory)
	enum TexKind
	{
		kTexColor = 0,   // RGBA8 sRGB (base colour)
		kTexMask = 1,    // RGBA8 linear (paint palette mask)
		kTexSingle = 2,  // R8 (opacity, metallic, roughness): red, or alpha when only alpha carries data
		kTexNormal = 3,  // R8G8 (normal map X/Y; Z is rebuilt in the shader)
	};

	struct Texture
	{
		int                               w = 0, h = 0;
		int                               kind = kTexColor;
		int                               bpp = 4;      // bytes per pixel on the GPU (4, 2 or 1); 0 for block-compressed data
		int                               channel = 0;  // single-channel maps: which file channel holds the data (0 red, 3 alpha)
		int                               dxgi = 0;     // v1.9.3: block-compressed (BC1-BC7) data straight from a .dds: its DXGI format number; 0 = plain pixels
		int                               swizzle = 0;  // what the shader reads: data maps the channel (0-3), normal maps 0 = X/Y in red/green, 1 = in alpha/green
		int                               fileW = 0, fileH = 0;  // size in the file (before MaxTextureSize)
		std::vector<std::vector<uint8_t>> mips;         // level 0 first (bpp bytes per pixel, or compressed blocks)
		std::vector<uint8_t>              thumb;        // single-channel maps: small RGBA copy (all channels) for checks
		int                               thumbW = 0, thumbH = 0;
		std::string                       name;
		std::string                       note;         // which channel was used etc. (for the log)
	};

	// One BeamNG material layer ("stage"). BeamNG docs: layer 1 = paint (colorPaletteMap R/G/B ->
	// paint 1/2/3), higher layers = details blended on top by opacityMap x opacityFactor.
	struct Layer
	{
		float factor[4] = { 1, 1, 1, 1 };  // baseColorFactor (v1: diffuseColor)
		int   baseTex = -1, baseUV = 0;    // baseColorMap (v1: colorMap)
		bool  live = false;                // baseColorMap is a live UI texture (screens, gauges): drawn as a dark screen
		bool  paint = false;               // instanceDiffuse
		int   palTex = -1, palUV = 0;      // colorPaletteMap
		int   palConst = -1;               // 0/1/2 = solid R/G/B mask (BeamNG nullcolormask*), -1 none
		int   opTex = -1, opUV = 0;        // opacityMap
		bool  maskIgnored = false;         // v1.9.1: the opacity map hides this layer on every triangle of its material -> not used
		float opFactor = 1.0f;             // opacityFactor
		float metallic = 0.0f, roughness = 0.5f, clearCoat = 0.0f;  // metallicFactor, roughnessFactor, clearCoatFactor
		int   normTex = -1, normUV = 0;    // normalMap (v1.9; BeamNG: OpenGL style, X in red, Y in green)
		int   metalTex = -1, metalUV = 0;  // metallicMap (multiplies metallicFactor)
		int   roughTex = -1, roughUV = 0;  // roughnessMap (multiplies roughnessFactor)
	};

	// translucentBlendOp (BeamNG/Torque)
	enum BlendOp
	{
		kBlendLerp = 0,    // LerpAlpha (Torque's default): everything fades with opacity
		kBlendPremul = 1,  // PreMulAlpha (BeamNG's glass): reflections stay, the rest fades
		kBlendAddAlpha = 2,
		kBlendAdd = 3,
		kBlendMul = 4,
	};

	struct Material
	{
		std::string name;
		Layer       layers[4];
		int         nLayers = 1;
		bool        translucent = false;
		int         blendOp = kBlendLerp;
		bool        noReflections = false;  // BeamNG "BlackSkyCubemap" (interior glass, interiors)
		std::string cubemap;
		float       alphaCut = 0.0f;  // 0 = no alpha test
		bool        mirror = false;   // drawn as a mirror (BeamNG mirrors are live reflections)
		bool        doubleSided = true;  // v1.9.5: BeamNG's doubleSided (see-through parts that aren't are seen from their front only)
		float       metallic = 0.0f, roughness = 0.5f, clearCoat = 0.0f;
		float       version = 0.0f;
		bool        fromMod = false;  // described by the BeamNG mod (v1.7+) rather than the exporter alone
	};

	struct Draw
	{
		uint32_t start = 0, count = 0;  // in Model::indices
		int      mat = -1;
		int      wheel = -1;            // index into Model::wheels, -1 = body
		int      wheelMode = 0;         // 1 spins + steers, 2 steers only (brake caliper)
	};

	struct Wheel
	{
		std::string name;
		float       c[3] = {};          // hub centre (car space)
		float       axis[3] = { 1, 0, 0 };  // axle direction, pointing right
		float       radius = 0.33f;
		bool        front = false;
		int         driven = -1;        // from the BeamNG mod's powertrain check: 1 driven, 0 not, -1 unknown
		int         spinDraws = 0, steerDraws = 0;
	};

	struct Model
	{
		std::vector<Vtx>      verts;
		std::vector<uint32_t> indices;
		std::vector<Draw>     draws;  // opaque first, then translucent
		std::vector<Material> materials;
		std::vector<Texture>  textures;
		std::vector<Wheel>    wheels;
		float                 paint[3][4] = { { 1, 1, 1, 1 }, { 1, 1, 1, 1 }, { 1, 1, 1, 1 } };  // as BeamNG gives them
		bool                  hasPaint = false;
		// v1.9.6: each paint's metallic, roughness, clear coat, clear coat roughness (BeamNG multiplies them
		// into the paint layers' factors); from the BeamNG mod 1.9.6+
		float                 paintPbr[3][4] = { { 0, 0.5f, 0, 0.04f }, { 0, 0.5f, 0, 0.04f }, { 0, 0.5f, 0, 0.04f } };
		bool                  hasPaintPbr = false;
		std::string           paintPbrSource;
		float                 size[3] = {};  // width (x), length (y), height (z) in metres
		std::string           vehicle;       // BeamNG vehicle file name, if known
		std::string           frame;         // which axis/orientation guess was used
		std::string           modVersion;    // version of the BeamNG mod that wrote the file
		uint64_t              textureBytes = 0;
		int                   ddsTextures = 0;     // v1.9.3: textures kept block-compressed from .dds files
		int                   pngTextures = 0;     // decoded from PNG/JPG (or .dds that couldn't stay compressed)
		int                   opaqueDraws = 0;
		int                   modMaterials = 0;  // materials described by the BeamNG mod
		int                   masksIgnored = 0;  // layers whose opacity map hid them everywhere (see Layer::maskIgnored)
		int                   liveLayers = 0;    // layers with a live UI texture
		int                   normalsFixed = 0;    // v1.9.4: the file's normals were turned to match the shape
		int                   normalsRebuilt = 0;  // v1.9.4: parts whose normals were rebuilt from the shape
		// v2.0 (drive mode): where the car-space origin is in BeamNG's vehicle frame at export time
		// (from the vehicle position: x right, y forward, z up along getDirectionVector/Up). Only
		// meaningful when vehFrameOk (car-space axes = BeamNG's vehicle axes: glTF axes + export direction).
		float                 vehOrigin[3] = {};
		bool                  vehFrameOk = false;
		std::string           vehOriginHow;  // how vehOrigin was decided (for the log)
		// v2.5 (dents): BeamNG's nodes as exported (car space) and the nodes each vertex follows. Empty: the file has none
		// (exported before the 2.5 BeamNG mod) or the car's frame isn't usable for driving.
		std::vector<int>                   nodeCid;    // BeamNG's node id (cid) of each node
		std::vector<float>                 nodePos;    // x y z per node (car space, at export)
		std::vector<uint8_t>               nodeWheel;  // 1 = one of a wheel's nodes (nothing follows it: wheels are drawn on their own)
		std::vector<std::vector<uint16_t>> nodeNbr;    // nodes near each node (of its parts): its turn is fitted to them
		std::vector<uint16_t>              skinIdx;    // 4 node indices per vertex
		std::vector<uint8_t>               skinW;      // 4 weights per vertex (sum 255; all 0 = it stays as drawn)
		int                                skinned = 0;  // vertices that follow nodes
		std::string                        skinNote;     // for the log
	};

	struct Options
	{
		int   maxTextureSize = 1024;      // larger colour textures are halved until they fit (VRAM)
		int   maxDataTextureSize = 1024;  // same for normal/metallic/roughness/opacity maps
		int   axes = -1;                  // -1 auto, 0 file is glTF y-up, 1 file is BeamNG z-up
		int   orient = -1;                // -1 auto, 0 use the exported driving direction, 1 BeamNG vehicle space (-y forward)
		float alphaCutDefault = 0.0f;     // alpha test for opaque materials that don't say (0 = off)
		bool  loadTextures = true;
		bool  wheels = true;              // find wheel groups
		bool  normalMaps = true;          // load normal maps
		bool  surfaceMaps = true;         // load metallic/roughness maps
		bool  maskCheck = true;           // v1.9.1: ignore layer masks that hide a layer on all of its triangles
		bool  compressed = true;          // v1.9.3: keep .dds files block-compressed on the GPU (BC1-BC7)
	};

	using LogFn = std::function<void(const char*)>;

	// Returns false with a reason in err. log receives progress/diagnostic lines.
	bool Load(const char* path, const Options& opt, Model& out, std::string& err, const LogFn& log);
}
