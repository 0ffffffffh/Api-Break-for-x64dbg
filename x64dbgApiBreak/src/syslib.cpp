#include <corelib.h>

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

	return HeapReAlloc(GetProcessHeap(), 0, memPtr, newSize);
}

void AbMemoryFree(void *memPtr)
{
	if (!memPtr)
		return;

	HeapFree(GetProcessHeap(), 0, (LPVOID)memPtr);
}
