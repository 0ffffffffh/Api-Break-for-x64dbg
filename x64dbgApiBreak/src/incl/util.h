#ifndef __UTIL_H__
#define __UTIL_H__

#include <corelib.h>

typedef enum
{
	Cdecl,
	Stdcall,
	Fastcall,
	Thiscall
}CALLCONVENTION;

typedef struct
{
	REGDUMP				regCtx;
	CALLCONVENTION		convention;
	USHORT				paramCount;
	duint *				paramList;
}*PPASSED_PARAMETER_CONTEXT,PASSED_PARAMETER_CONTEXT;

#ifdef _WIN64
typedef ULONGLONG	ARCHWIDE;
#else
typedef ULONG		ARCHWIDE;
#endif


typedef struct __DMA
{
	WORD				sizeOfType;
	ULONG				count;
	ARCHWIDE			writeBoundary;
	ARCHWIDE			totalSize;
	LONG				needsSynchronize;
	BOOL				ownershipTaken;
	CRITICAL_SECTION	areaGuard;
	void *				memory;
}*PDMA,DMA; //DYNAMIC MEMORY ADAPTER

BOOL DmaCreateAdapter(WORD sizeOfType, ULONG initialCount, PDMA *dma);

BOOL DmaIssueExpansion(PDMA dma, LONG expansionDelta);

BOOL DmaRead(PDMA dma, ULONG offset, ARCHWIDE size, void *destMemory);

BOOL DmaWrite(PDMA dma, ULONG offset, ARCHWIDE size, void *srcMemory);

BOOL DmaReadTypeAlignedSequence(PDMA dma, ULONG index, ULONG count, void *destMemory);

BOOL DmaCopyWrittenMemory(PDMA dma, void *destMemory, BOOL allocForDest);

BOOL DmaTakeMemoryOwnership(PDMA dma, void **nativeMem);

void DmaDestroyAdapter(PDMA dma);



BOOL UtlExtractPassedParameters(USHORT paramCount, CALLCONVENTION callConv, PPASSED_PARAMETER_CONTEXT *paramInfo);


#endif // !__UTIL_H__
