#include <corelib.h>

#define TRY_MAX_WAITMS      1000
#define TRY_WAIT_PERCALL    20
#define TRY_COUNT           (TRY_MAX_WAITMS / TRY_WAIT_PERCALL)

#include <qpc.hpp>

typedef struct __MEMLIST_ENTRY
{
	struct __MEMLIST_ENTRY *next;
	struct __MEMLIST_ENTRY *prev;
	void *mem;
	int size;
	char function[128];
	char filename[256];
	int  line;
}*PMEMLIST_ENTRY,MEMLIST_ENTRY;

typedef struct __MEMLIST
{
	PMEMLIST_ENTRY head;
	PMEMLIST_ENTRY tail;
	volatile ULONG spinLock;
}*PMEMLIST,MEMLIST;

MEMLIST AbpMemList = { 0 };

void AcqMemListSpinlock()
{
	while (InterlockedCompareExchange(&AbpMemList.spinLock, 1, 0) == 1)
		_mm_pause();
}

void RelMemListSpinlock()
{
	InterlockedExchange(&AbpMemList.spinLock, 0);
}

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



void *_AbMemoryAlloc(int size)
{
    if (size <= 0)
        return NULL;

    return HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (SIZE_T)size);
}

void *_AbMemoryRealloc(void *memPtr, int newSize)
{
    if (newSize <= 0)
        return NULL;

    if (!memPtr)
        return _AbMemoryAlloc(newSize);

    return HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, memPtr, newSize);
}

void _AbMemoryFree(void *memPtr)
{
    if (!memPtr)
        return;

    HeapFree(GetProcessHeap(), 0, (LPVOID)memPtr);
}



#ifdef TRACK_MEMORY_ALLOCATIONS

void AbRevealPossibleMemoryLeaks()
{
	if (!AbpMemList.head)
	{
		DBGPRINT("Great! Everything's fine. No leaked memory!");
		return;
	}

	DBGPRINT("Oh no. There is a bit memory leak.");

	for (PMEMLIST_ENTRY entry = AbpMemList.head; entry != NULL; entry = entry->next)
	{
		DBGPRINT("Leaked memory at %p (%d bytes) | %s %s:%d", entry->mem, entry->size, entry->filename, entry->function, entry->line);
	}
}

void *AbMemoryAlloc_DBG(int size, const char *file, const char *func, const int line)
{
	PMEMLIST_ENTRY entry;
	PVOID umem;

	int realSize = size + sizeof(MEMLIST_ENTRY);

	entry = (PMEMLIST_ENTRY)_AbMemoryAlloc(realSize);

	if (!entry)
		return FALSE;

	umem = ((BYTE *)entry) + sizeof(MEMLIST_ENTRY);

	entry->next = NULL;
	entry->prev = NULL;
	entry->mem = umem;
	entry->size = size;
	
	strcpy_s(entry->filename, file);
	strcpy_s(entry->function, func);
	entry->line = line;

	
	AcqMemListSpinlock();

	if (!AbpMemList.head)
	{
		AbpMemList.head = AbpMemList.tail = entry;
	}
	else
	{
		entry->prev = AbpMemList.tail;
		AbpMemList.tail->next = entry;
		AbpMemList.tail = entry;
	}

	RelMemListSpinlock();

	return umem;
}

void *AbMemoryRealloc_DBG(void *memPtr, int newSize, const char *file, const char *func, const int line)
{
	PMEMLIST_ENTRY entry;
	BYTE *realMem, *reallocMem;
	
	if (!memPtr)
		return AbMemoryAlloc_DBG(newSize, file, func, line);
	
	realMem = ((BYTE *)memPtr) - sizeof(MEMLIST_ENTRY);

	entry = (PMEMLIST_ENTRY)realMem;

	reallocMem = (BYTE *)_AbMemoryRealloc(realMem, sizeof(MEMLIST_ENTRY) + newSize);

	if (!reallocMem)
		return NULL;

	if (realMem != reallocMem)
	{
		entry = (PMEMLIST_ENTRY)reallocMem;
		entry->mem = reallocMem;
	}
	
	entry->size = newSize;

	return reallocMem + sizeof(MEMLIST_ENTRY);
}

void AbMemoryFree_DBG(void *memPtr)
{
	PMEMLIST_ENTRY entry;
	BYTE *realMem;

	if (!memPtr)
		return;

	realMem = ((BYTE *)memPtr) - sizeof(MEMLIST_ENTRY);

	entry = (PMEMLIST_ENTRY)realMem;

	if (entry->mem != memPtr)
	{
		DBGPRINT("Invalid memory block (%p)",memPtr);
		return;
	}

	AcqMemListSpinlock();

	//is not head?
	if (entry->prev)
	{
		entry->prev->next = entry->next;

		//is not tail?
		if (entry->next)
			entry->next->prev = entry->prev;
		else
			AbpMemList.tail = entry->prev;
	}
	else
		AbpMemList.head = entry->next;


	if (AbpMemList.head)
		AbpMemList.head->prev = NULL;
	else
		AbpMemList.tail = NULL;

	if (AbpMemList.tail)
		AbpMemList.tail->next = NULL;

	RelMemListSpinlock();

}
#else
void AbRevealPossibleMemoryLeaks()
{
	DBGPRINT("Memory allocation tracker disabled.!");
}
#endif