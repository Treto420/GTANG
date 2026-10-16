// locator_test.cpp - v2.53: loads a car file with the car loader (carmodel.cpp, built for Linux here) and writes what
// the dents shader needs: nodes (car space), each vertex (position, normal, its part) and its triangle of nodes.
// locator_test.py builds the car, runs this and checks the shader's math on it.
// Build: zig c++ -std=c++17 -O1 -I../src locator_test.cpp ../src/carmodel.cpp -o locator_test
#include "carmodel.h"
#include <cstdio>
#include <string>

int main(int argc, char** argv)
{
	if (argc < 3) return 2;
	car::Model   m;
	car::Options opt;
	opt.loadTextures = false;
	std::string err;
	if (!car::Load(argv[1], opt, m, err, [](const char* s) { fprintf(stderr, "%s\n", s); })) {
		fprintf(stderr, "load failed: %s\n", err.c_str());
		return 1;
	}
	FILE* f = fopen(argv[2], "w");
	if (!f) return 1;
	fprintf(f, "note %s\n", m.skinNote.c_str());
	for (size_t i = 0; i < m.nodeCid.size(); ++i) fprintf(f, "n %d %.6f %.6f %.6f %d\n", m.nodeCid[i], m.nodePos[i * 3], m.nodePos[i * 3 + 1], m.nodePos[i * 3 + 2], int(m.nodeWheel[i]));
	const bool skin = m.skinIdx.size() == m.verts.size() * 4 && m.skinC.size() == m.verts.size() * 3;
	for (size_t v = 0; v < m.verts.size(); ++v) {
		const car::Vtx& x = m.verts[v];
		fprintf(f, "v %.6f %.6f %.6f %.6f %.6f %.6f", x.p[0], x.p[1], x.p[2], x.n[0], x.n[1], x.n[2]);
		if (skin) fprintf(f, " %d %d %d %d %.7g %.7g %.7g", m.skinIdx[v * 4], m.skinIdx[v * 4 + 1], m.skinIdx[v * 4 + 2], m.skinIdx[v * 4 + 3], m.skinC[v * 3], m.skinC[v * 3 + 1], m.skinC[v * 3 + 2]);
		fprintf(f, "\n");
	}
	// v2.6: the breakable groups and which draws use them; the paint flags
	for (size_t i = 0; i < m.dgNames.size(); ++i) fprintf(f, "g %zu %s %d\n", i, m.dgNames[i].c_str(), int(m.dgHide[i]));
	for (const car::Draw& d : m.draws) fprintf(f, "d %u %u %d %d\n", d.start, d.count, d.mat, d.dg);
	for (const car::Material& mt : m.materials)
		for (int l = 0; l < mt.nLayers; ++l) fprintf(f, "m %s %d %d %d\n", mt.name.c_str(), l, int(mt.layers[l].paint), mt.layers[l].palConst);
	fclose(f);
	return 0;
}
