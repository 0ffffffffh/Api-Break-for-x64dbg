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

static bool AbMemReadGuaranteed(duint va, void *dest, duint size)
{
    bool success = false;
    int limit = 1000 / 20;

    if (!AbHasDebuggingProcess())
        return false;

    //Another ugly hack here 
    //x64dbg has a bug https://github.com/x64dbg/x64dbg/issues/1475
    while (limit-- != 0)
    {
        if (!DbgMemRead(va, dest, size))
            Sleep(20);
        else
        {
            success = true;
            break;
        }
    }

    return success;
}

void *AbMemoryAlloc(int size);
void *AbMemoryRealloc(void *memPtr, int newSize);
void AbMemoryFree(void *memPtr);

void AbReleaseAllSystemResources(bool isInShutdown);

FORWARDED int       AbPluginHandle;
FORWARDED HWND      AbHwndDlgHandle;
FORWARDED int       AbMenuHandle;
FORWARDED int       AbMenuDisasmHandle;
FORWARDED int       AbMenuDumpHandle;
FORWARDED int       AbMenuStackHandle;
FORWARDED HMODULE   AbPluginModule;

#endif // !__CORELIB_H_
