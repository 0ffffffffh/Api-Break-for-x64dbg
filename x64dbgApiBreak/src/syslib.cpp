#include <corelib.h>

#define TRY_MAX_WAITMS      1000
#define TRY_WAIT_PERCALL    20
#define TRY_COUNT           (TRY_MAX_WAITMS / TRY_WAIT_PERCALL)

#include <qpc.hpp>

bool AbMemReadGuaranteed(duint va, void *dest, duint size)
{
    bool success = false;
    int limit = TRY_COUNT;

    if (!AbHasDebuggingProcess())
        return false;

    //Another ugly hack here 
    //x64dbg has a bug https://github.com/x64dbg/x64dbg/issues/1475
    while (limit-- != 0)
    {
        if (!DbgMemRead(va, dest, size))
            Sleep(TRY_WAIT_PERCALL);
        else
        {
            success = true;
            break;
        }
    }

    return success;
}

void *AbMemoryAlloc(int size)
{
    if (size <= 0)
        return NULL;

    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size);
}

void *AbMemoryRealloc(void *memPtr, int newSize)
{
    if (newSize <= 0)
        return NULL;

    if (!memPtr)
        return AbMemoryAlloc(newSize);

    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, memPtr, newSize);
}

void AbMemoryFree(void *memPtr)
{
    if (!memPtr)
        return;

    HeapFree(GetProcessHeap(), 0, (LPVOID)memPtr);
}
