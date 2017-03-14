#include <util.h>


BOOL UtlpExtractPassedParameters(PPASSED_PARAMETER_CONTEXT paramInfo)
{
	REGDUMP *regs;
	duint *paramList;
	duint csp, offset;

	paramInfo->paramList = (duint *)AbMemoryAlloc(paramInfo->paramCount * sizeof(duint));

	if (!paramInfo->paramList)
		return FALSE;

	regs = &paramInfo->regCtx;
	paramList = paramInfo->paramList;
	
#ifdef _WIN64

	if (paramInfo->convention != Fastcall)
	{
		paramInfo->convention = Fastcall;
		DBGPRINT("Calling convention revert to fastcall on Win64");
	}

	csp = regs->regcontext.csp;

	offset = 0x20;

	for (int i = 0;i < paramInfo->paramCount;i++)
	{
		switch (i)
		{
		case 0:
			paramList[0] = regs->regcontext.ccx;
			break;
		case 1:
			paramList[1] = regs->regcontext.cdx;
			break;
		case 2:
			paramList[2] = regs->regcontext.r8;
			break;
		case 3:
			paramList[3] = regs->regcontext.r9;
			break;
		}

		if (i > 3)
		{
			AbMemReadGuaranteed(csp + offset, &paramList[i], sizeof(duint));
			offset += sizeof(duint);
		}
	}

#else
	if (paramInfo->convention == Cdecl)
		csp = regs->regcontext.csp;
	else
		csp = regs->regcontext.csp - paramInfo->paramCount * sizeof(duint);

	offset = 0;

	for (int i = 0;i < paramInfo->paramCount;i++)
	{
		AbMemReadGuaranteed(csp + offset, &paramList[i], sizeof(duint));
		offset += sizeof(duint);
	}

#endif

	return TRUE;
}



BOOL DmaCreateAdapter(WORD sizeOfType, ULONG initialCount, PDMA *dma)
{
	PDMA pdma = NULL;

	pdma = ALLOCOBJECT(DMA);

	if (!pdma)
		return FALSE;

	pdma->sizeOfType = sizeOfType;
	pdma->count = initialCount;
	pdma->totalSize = sizeOfType * initialCount;
	pdma->writeBoundary = 0;
	pdma->memory = AbMemoryAlloc(pdma->totalSize);


	if (!pdma->memory)
	{
		DBGPRINT("Dma memory alloc fail");
		FREEOBJECT(pdma);
		return FALSE;
	}

	InitializeCriticalSection(&pdma->areaGuard);
	pdma->needsSynchronize = FALSE;
	pdma->ownershipTaken = FALSE;

	*dma = pdma;

	return TRUE;
}

BOOL DmaIssueExpansion(PDMA dma, LONG expansionDelta)
{
	BOOL isReducing = expansionDelta < 0;

	if (isReducing && dma->totalSize + expansionDelta < dma->writeBoundary)
		return FALSE;

	EnterCriticalSection(&dma->areaGuard);
	InterlockedCompareExchange((volatile LONG *)&dma->needsSynchronize, TRUE, FALSE);

	dma->memory = AbMemoryRealloc(dma->memory, (dma->count + expansionDelta) * dma->sizeOfType);
	dma->count += expansionDelta;

	InterlockedCompareExchange((volatile LONG *)&dma->needsSynchronize, FALSE, TRUE);
	LeaveCriticalSection(&dma->areaGuard);

}

#define DmapNeedsSynch(pdma) InterlockedCompareExchange((volatile LONG *)&pdma->needsSynchronize, FALSE, FALSE)

#define DmapIsAccessValid(pdma, addr, size) (addr + size <= dma->totalSize && !dma->ownershipTaken)

BOOL DmapMemIo(PDMA dma, ULONG offset, ARCHWIDE size, void *mem, BOOL write)
{
	ARCHWIDE realAddr;

	void *source, *dest;

	if (offset == 0 && dma->writeBoundary > 0)
		offset = dma->writeBoundary;

	if (write && offset + size > dma->totalSize)
	{
		if (!DmaIssueExpansion(dma, (dma->count / 3)))
			return FALSE;
	}

	realAddr = ((ARCHWIDE)dma->memory) + offset;

	if (write)
	{
		source = (void *)mem;
		dest = (void *)realAddr;
	}
	else
	{
		source = (void *)realAddr;
		dest = (void *)mem;
	}

	
	if (!DmapIsAccessValid(dma, realAddr, size))
		return FALSE;

	memcpy(dest, source, size);

	if (write)
	{
		if (offset + size > dma->writeBoundary)
			dma->writeBoundary = offset + size;
	}

	return TRUE;
}

BOOL DmaRead(PDMA dma, ULONG offset, ARCHWIDE size, void *destMemory)
{
	BOOL success;
	BOOL owned = FALSE;

	if (DmapNeedsSynch(dma))
	{
		EnterCriticalSection(&dma->areaGuard);
		owned = TRUE;
	}

	success = DmapMemIo(dma, offset, size, destMemory,FALSE);

	if (owned)
		LeaveCriticalSection(&dma->areaGuard);

	return success;

}

BOOL DmaWrite(PDMA dma, ULONG offset, ARCHWIDE size, void *srcMemory)
{
	BOOL success;
	BOOL owned = FALSE;

	if (DmapNeedsSynch(dma))
	{
		EnterCriticalSection(&dma->areaGuard);
		owned = TRUE;
	}

	success = DmapMemIo(dma, offset, size, srcMemory,TRUE);

	if (owned)
		LeaveCriticalSection(&dma->areaGuard);

	return success;
}

BOOL DmaReadTypeAlignedSequence(PDMA dma, ULONG index, ULONG count, void *destMemory)
{
	ARCHWIDE offset,size;

	if (!dma)
		return FALSE;

	offset = index * dma->sizeOfType;
	size = count * dma->sizeOfType;

	return DmaRead(dma, offset, size, destMemory);
}

BOOL DmaCopyWrittenMemory(PDMA dma, void **destMemory, BOOL allocForDest)
{
	if (allocForDest)
		*destMemory = AbMemoryAlloc(dma->writeBoundary);

	if (!*destMemory)
		return FALSE;

	memcpy(*destMemory, dma->memory, dma->writeBoundary);

	return TRUE;
}

BOOL DmaTakeMemoryOwnership(PDMA dma, void **nativeMem)
{
	if (dma->ownershipTaken)
		return FALSE;

	*nativeMem = dma->memory;
	dma->ownershipTaken = TRUE;

	return TRUE;
}

void DmaDestroyAdapter(PDMA dma)
{
	if (!dma->ownershipTaken)
		AbMemoryFree(dma->memory);

	memset(dma, 0, sizeof(DMA));

	AbMemoryFree(dma);

}

//UTILITY FUNCS

BOOL UtlExtractPassedParameters(USHORT paramCount, CALLCONVENTION callConv, PPASSED_PARAMETER_CONTEXT *paramInfo)
{
	PPASSED_PARAMETER_CONTEXT ppi = NULL;

	ppi = ALLOCOBJECT(PASSED_PARAMETER_CONTEXT);

	if (!ppi)
		return FALSE;

	ppi->paramCount = paramCount;
	ppi->convention = callConv;

	if (!DbgGetRegDump(&ppi->regCtx))
	{
		DBGPRINT("REGDUMP failed");
		FREEOBJECT(ppi);
		return FALSE;
	}

	if (!UtlpExtractPassedParameters(ppi))
	{
		FREEOBJECT(ppi);
		return FALSE;
	}

	*paramInfo = ppi;

	return TRUE;
}