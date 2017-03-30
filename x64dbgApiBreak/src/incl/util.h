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
	DWORD				oldProtect;
    struct
    {
        ULONG           offset;
        ARCHWIDE        size;
        ULONG           beginCookie;
        ULONG           endCookie;
    }UnsafeWriteCheckInfo;
	void *				memory;
}*PDMA,DMA; //DYNAMIC MEMORY ADAPTER

#define DMA_AUTO_OFFSET ULONG_MAX


BOOL DmaCreateAdapter(WORD sizeOfType, ULONG initialCount, PDMA *dma);

BOOL DmaWriteNeedsExpand(PDMA dma, ARCHWIDE needByteSize, ULONG writeBeginOffset, BOOL autoIssue);

BOOL DmaIssueExpansion(PDMA dma, LONG expansionDelta);

BOOL DmaRead(PDMA dma, ULONG offset, ARCHWIDE size, void *destMemory);

BOOL DmaWrite(PDMA dma, ULONG offset, ARCHWIDE size, void *srcMemory);

BOOL DmaStringWriteA(PDMA dma, LPCSTR format,...);

BOOL DmaStringWriteW(PDMA dma, LPCWSTR format, ...);

BOOL DmaReadTypeAlignedSequence(PDMA dma, ULONG index, ULONG count, void *destMemory);

BOOL DmaTakeMemoryOwnership(PDMA dma, void **nativeMem);

BOOL DmaPrepareForDirectWrite(PDMA dma, ULONG writeOffset, ARCHWIDE writeSize);

BOOL DmaPrepareForRead(PDMA dma, void **nativeMem);

void DmaDestroyAdapter(PDMA dma);


BOOL UtlExtractPassedParameters(USHORT paramCount, CALLCONVENTION callConv, REGDUMP *regdump, PPASSED_PARAMETER_CONTEXT *paramInfo);

duint UtlGetCallerAddress(REGDUMP *regdump);

BOOL UtlInternetReadW(LPCWSTR url, BYTE **content, ULONG *contentLength);

BOOL UtlInternetReadA(LPCSTR url, BYTE **content, ULONG *contentLength);

#endif // !__UTIL_H__
