#ifndef __CORELIB_H_
#define __CORELIB_H_

#include <apibreak.h>
#include <defs.h>

#ifdef _WIN64
#pragma comment(lib, "pluginsdk/x64dbg.lib")
#pragma comment(lib, "pluginsdk/x64bridge.lib")
#pragma comment(lib, "pluginsdk/TitanEngine/TitanEngine_x64.lib")
#else
#pragma comment(lib, "pluginsdk/x32dbg.lib")
#pragma comment(lib, "pluginsdk/x32bridge.lib")
#pragma comment(lib, "pluginsdk/TitanEngine/TitanEngine_x86.lib")
#endif

bool AbMemReadGuaranteed(duint va, void *dest, duint size);

void *AbMemoryAlloc(int size);
void *AbMemoryRealloc(void *memPtr, int newSize);
void AbMemoryFree(void *memPtr);

void AbReleaseAllSystemResources(bool isInShutdown);

BOOL AbRaiseSystemError(const char *errorDesc, int errCode, const char *func, const int line);

FORWARDED int       AbPluginHandle;
FORWARDED HWND      AbHwndDlgHandle;
FORWARDED int       AbMenuHandle;
FORWARDED int       AbMenuDisasmHandle;
FORWARDED int       AbMenuDumpHandle;
FORWARDED int       AbMenuStackHandle;
FORWARDED HMODULE   AbPluginModule;

#endif // !__CORELIB_H_
