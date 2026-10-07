// fakexinput.cpp - stand-in for xinput1_4.dll in the sandbox test (Wine, WINEDLLOVERRIDES=xinput1_4=n).
// Pad 0 is connected while fake_pad.txt exists and doesn't say "off"; its one line is
// "<buttons hex>,<lt 0-255>,<rt 0-255>,<lx>,<ly>,<rx>,<ry>" (XInput units). The packet number goes up
// whenever the line changes, like a real pad's.
#include <windows.h>
#include <cstdio>
#include <cstring>

struct Pad
{
	WORD  buttons;
	BYTE  lt, rt;
	SHORT lx, ly, rx, ry;
};
struct State
{
	DWORD packet;
	Pad   pad;
};

static char  g_last[256];
static DWORD g_packet = 1;

extern "C" __declspec(dllexport) DWORD WINAPI XInputGetState(DWORD index, State* st)
{
	if (!st) return ERROR_BAD_ARGUMENTS;
	if (index != 0) return ERROR_DEVICE_NOT_CONNECTED;
	// "off" = no pad; a file being replaced (can't be read this instant) keeps the last state
	FILE* f = fopen("fake_pad.txt", "r");
	char  line[256] = "";
	if (f) {
		if (!fgets(line, sizeof line, f)) line[0] = 0;
		fclose(f);
	}
	if (!f || !line[0]) {
		if (!g_last[0]) return ERROR_DEVICE_NOT_CONNECTED;
		strcpy(line, g_last);
	}
	if (strcmp(line, g_last)) {
		strcpy(g_last, line);
		++g_packet;
	}
	if (!strncmp(line, "off", 3)) return ERROR_DEVICE_NOT_CONNECTED;
	unsigned b = 0;
	int      lt = 0, rt = 0, lx = 0, ly = 0, rx = 0, ry = 0;
	sscanf(line, "%x,%d,%d,%d,%d,%d,%d", &b, &lt, &rt, &lx, &ly, &rx, &ry);
	memset(st, 0, sizeof(State));
	st->packet = g_packet;
	st->pad.buttons = WORD(b);
	st->pad.lt = BYTE(lt);
	st->pad.rt = BYTE(rt);
	st->pad.lx = SHORT(lx);
	st->pad.ly = SHORT(ly);
	st->pad.rx = SHORT(rx);
	st->pad.ry = SHORT(ry);
	return ERROR_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE, DWORD, LPVOID) { return TRUE; }
