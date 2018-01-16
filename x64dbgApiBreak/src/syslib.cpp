#include <corelib.h>

#define TRY_MAX_WAITMS      1000
#define TRY_WAIT_PERCALL    20
#define TRY_COUNT           (TRY_MAX_WAITMS / TRY_WAIT_PERCALL)

#include <qpc.hpp>

typedef struct __MEMLIST_ENTRY
{
	ULONG						magic;
	struct __MEMLIST_ENTRY *	next;
	struct __MEMLIST_ENTRY *	prev;
	void *						mem;
	ULONG						size;
	BOOL						track;
	char						function[128];
	char						filename[256];
	ULONG						line;
}*PMEMLIST_ENTRY,MEMLIST_ENTRY;

typedef struct __MEMLIST
{
	PMEMLIST_ENTRY		head;
	PMEMLIST_ENTRY		tail;
	volatile ULONG		spinLock;
}*PMEMLIST,MEMLIST;

#define MEMLIST_ENTRY_MAGIC		0xBAAD7337

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
	DBGPRINT("Checking for possible leaked memory blocks");

	if (!AbpMemList.head)
	{
		DBGPRINT("Great! Everything's fine. No leaked memory!");
		return;
	}

	DBGPRINT("Oh no. There is a some memory leak.");

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

	entry->magic = MEMLIST_ENTRY_MAGIC;
	entry->next = NULL;
	entry->prev = NULL;
	entry->mem = umem;
	entry->size = size;
	
	if (file != NULL)
		strcpy_s(entry->filename, file);
	else
		strcpy_s(entry->filename, "vclib");
	
	if (func != NULL)
		strcpy_s(entry->function, func);
	else
		strcpy_s(entry->function, "unknown");
	
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

#define ASSERT(x) if (!(x)) DBGPRINT("Assertation failed. %s",#x)

void dumpNode(PMEMLIST_ENTRY entry)
{
	DBGPRINT("ENTRY_DUMP_BEGIN");
	DBGPRINT("entry = %p, entry->next = %p, entry->prev = %p", entry, entry->next, entry->prev);
	DBGPRINT("entry data: %s %s:%d", entry->filename, entry->function, entry->line);
	DBGPRINT("entry->mem = %p", entry->mem);
	DBGPRINT("\n");
}

PMEMLIST_ENTRY AbpValidateAndProbe(void *memPtr)
{
	PMEMLIST_ENTRY entry = NULL;
	BYTE *realMemAddr = NULL;

	realMemAddr = ((BYTE *)memPtr) - sizeof(MEMLIST_ENTRY);

	entry = (PMEMLIST_ENTRY)realMemAddr;

	__try
	{
		if (entry->magic != MEMLIST_ENTRY_MAGIC)
			return NULL;

		if (entry->mem != memPtr)
			return NULL;
	}
	__except(EXCEPTION_CONTINUE_EXECUTION)
	{
		return NULL;
	}

	return entry;
}

void *AbMemoryRealloc_DBG(void *memPtr, int newSize, const char *file, const char *func, const int line)
{
	PMEMLIST_ENTRY entry, oldEntry,prevEntry, nextEntry;
	BYTE *realMem, *reallocMem;
	
	if (!memPtr)
		return AbMemoryAlloc_DBG(newSize, file, func, line);
	

	entry = AbpValidateAndProbe(memPtr);

	if (!entry)
	{
		DBGPRINT("Invalid memory address %p", memPtr);
		return NULL;
	}

	realMem = (BYTE *)entry;

	AcqMemListSpinlock();

	
	prevEntry = entry->prev;
	nextEntry = entry->next;

	reallocMem = (BYTE *)AbMemoryRealloc_DBG(realMem, sizeof(MEMLIST_ENTRY) + newSize,file,func,line);

	if (!reallocMem)
		return NULL;
	
	oldEntry = entry;
	entry = (PMEMLIST_ENTRY)reallocMem;
	
	if (entry->track)
		DBGPRINT("reallocating memory %p. old = %p, new = %p (%s:%d)", entry, entry, reallocMem, entry->filename, entry->line);

	if (realMem != reallocMem)
	{
		entry->mem = reallocMem + sizeof(MEMLIST_ENTRY);

		//Update previous's next with new allocated node address
		if (prevEntry)
			prevEntry->next = entry;

		//Update next's previous with new alloacted node address
		if (nextEntry)
			nextEntry->prev = entry;

		//if oldentry was list's head. we have to also update head
		if (oldEntry == AbpMemList.head)
			AbpMemList.head = entry;

		//if oldentry was list's tail we have to also update tail
		if (oldEntry == AbpMemList.tail)
			AbpMemList.tail = entry;

	}

	ASSERT(entry->prev == prevEntry);
	ASSERT(entry->next == nextEntry);

	entry->size = newSize;


	RelMemListSpinlock();

	return reallocMem + sizeof(MEMLIST_ENTRY);
}

void AbMemoryFree_DBG(void *memPtr)
{
	PMEMLIST_ENTRY entry;
	
	if (!memPtr)
		return;

	entry = AbpValidateAndProbe(memPtr);

	if (!entry)
	{
		DBGPRINT("invalid memory %p", memPtr);
		return;
	}
	
	if (entry->track)
		DBGPRINT("%p is being freed", memPtr);

	AcqMemListSpinlock();

	if (entry == AbpMemList.head && entry == AbpMemList.tail)
	{
		AbpMemList.head = AbpMemList.tail = NULL;
	}
	else if (entry == AbpMemList.head)
	{
		AbpMemList.head = entry->next;

		if (entry->next)
			entry->next->prev = NULL;

	}
	else if (entry == AbpMemList.tail)
	{
		AbpMemList.tail = entry->prev;

		if (entry->prev)
			entry->prev->next = NULL;
	}
	else
	{
		entry->prev->next = entry->next;
		entry->next->prev = entry->prev;
	}

	RtlZeroMemory(entry, sizeof(MEMLIST_ENTRY));
	_AbMemoryFree(entry);

	RelMemListSpinlock();

}

//#define SKIP_UNNAMED_ALLOCATION

#ifdef __cplusplus

#if defined(new)
#undef new 
#endif

void *operator new(size_t size, char *func, char *file, int line)
{
	return AbMemoryAlloc_DBG(size, func, file, line);
}

void *operator new(size_t size)
{
#ifdef SKIP_UNNAMED_ALLOCATION
	return malloc(size);
#else
	return AbMemoryAlloc_DBG(size, NULL, NULL, 0);
#endif
}

void operator delete(void *memory, char *func, char *file, int line)
{
	operator delete(memory);
}

void operator delete(void *memory)
{
	AbMemoryFree_DBG(memory);
}


#endif


#else
void AbRevealPossibleMemoryLeaks()
{
	DBGPRINT("Memory allocation tracker disabled.!");
}
#endif

void AbTrackMemory_DBG(void *memPtr)
{
	PMEMLIST_ENTRY entry;

	if (!memPtr)
		return;

	entry = AbpValidateAndProbe(memPtr);

	if (!entry)
	{
		DBGPRINT("invalid memory %p", memPtr);
		return;
	}

	DBGPRINT("memory %p will be tracked.", memPtr);

	entry->track = TRUE;

}