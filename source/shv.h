#pragma once
// Script Hook V functions used by the plugin. Declarations only.
// Signatures match the official Script Hook V SDK main.h (dev-c.com, (C) Alexander Blade);
// the export names below were checked against the SDK's ScriptHookV.lib.

#include <windows.h>

#if defined(_MSC_VER) && !defined(__clang__)

// Visual Studio build: link against ScriptHookV.lib from the official SDK (see build_vs2022.bat).
#define SHV_IMPORT __declspec(dllimport)
SHV_IMPORT void presentCallbackRegister(void (*cb)(void*));
SHV_IMPORT void presentCallbackUnregister(void (*cb)(void*));
SHV_IMPORT void scriptWait(DWORD time);
SHV_IMPORT void scriptRegister(HMODULE module, void (*scriptMain)());
SHV_IMPORT void scriptUnregister(HMODULE module);
SHV_IMPORT void nativeInit(UINT64 hash);
SHV_IMPORT void nativePush64(UINT64 val);
SHV_IMPORT PUINT64 nativeCall();
#pragma comment(lib, "ScriptHookV.lib")

#else

// MinGW / clang build: the same functions under their Visual C++ export names.
#define SHV_FN(ret, name, args, exportName) extern "C" __declspec(dllimport) ret name args __asm__(exportName);
SHV_FN(void, presentCallbackRegister, (void (*cb)(void*)), "?presentCallbackRegister@@YAXP6AXPEAX@Z@Z")
SHV_FN(void, presentCallbackUnregister, (void (*cb)(void*)), "?presentCallbackUnregister@@YAXP6AXPEAX@Z@Z")
SHV_FN(void, scriptWait, (DWORD time), "?scriptWait@@YAXK@Z")
SHV_FN(void, scriptRegister, (HMODULE module, void (*scriptMain)()), "?scriptRegister@@YAXPEAUHINSTANCE__@@P6AXXZ@Z")
SHV_FN(void, scriptUnregister, (HMODULE module), "?scriptUnregister@@YAXPEAUHINSTANCE__@@@Z")
SHV_FN(void, nativeInit, (UINT64 hash), "?nativeInit@@YAX_K@Z")
SHV_FN(void, nativePush64, (UINT64 val), "?nativePush64@@YAX_K@Z")
SHV_FN(PUINT64, nativeCall, (), "?nativeCall@@YAPEA_KXZ")
#undef SHV_FN

#endif
