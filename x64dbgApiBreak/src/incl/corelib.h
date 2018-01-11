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


#define TRACK_MEMORY_ALLOCATIONS

#ifdef TRACK_MEMORY_ALLOCATIONS
void *AbMemoryAlloc_DBG(int size,const char *file, const char *func, const int line);
void *AbMemoryRealloc_DBG(void *memPtr, int newSize, const char *file, const char *func, const int line);
void AbMemoryFree_DBG(void *memPtr);
void AbTrackMemory_DBG(void *memPtr);

#define AbMemoryAlloc(size)					AbMemoryAlloc_DBG((size), __FILE__, __FUNCTION__,__LINE__)
#define AbMemoryRealloc(memPtr, newSize)	AbMemoryRealloc_DBG((memPtr),(newSize),__FILE__, __FUNCTION__,__LINE__)
#define AbMemoryFree(memPtr)				AbMemoryFree_DBG((memPtr))
#define AbTrackMemory(memPtr)				AbTrackMemory_DBG((memPtr))

#else
#define AbMemoryAlloc(size)					_AbMemoryAlloc((size))
#define AbMemoryRealloc(memPtr, newSize)	_AbMemoryRealloc((memPtr),(newSize))
#define AbMemoryFree(memPtr)				_AbMemoryFree((memPtr))
#define AbTrackMemory(memPtr)				
#endif

void *_AbMemoryAlloc(int size);
void *_AbMemoryRealloc(void *memPtr, int newSize);
void _AbMemoryFree(void *memPtr);

void AbRevealPossibleMemoryLeaks();

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
