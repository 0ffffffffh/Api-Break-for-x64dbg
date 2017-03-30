#include <util.h>
#include <winhttp.h>

#pragma comment(lib,"Winhttp.lib")

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
	csp = regs->regcontext.csp;
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

BOOL DmaWriteNeedsExpand(PDMA dma, ARCHWIDE needByteSize, ULONG writeBeginOffset, BOOL autoIssue)
{
    BOOL needed;

    if (writeBeginOffset == DMA_AUTO_OFFSET)
        writeBeginOffset = dma->writeBoundary;

    needed = needByteSize > dma->totalSize - writeBeginOffset;

    if (needed && autoIssue)
        DmaIssueExpansion(dma, needByteSize / dma->sizeOfType);

    return needed;
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
	dma->totalSize = dma->count * dma->sizeOfType;

	InterlockedCompareExchange((volatile LONG *)&dma->needsSynchronize, FALSE, TRUE);
	LeaveCriticalSection(&dma->areaGuard);


	return TRUE;
}

#define DmapNeedsSynch(pdma) InterlockedCompareExchange((volatile LONG *)&pdma->needsSynchronize, FALSE, FALSE)

#define DmapIsAccessValid(pdma, addr, size) (addr + size <= dma->totalSize && !dma->ownershipTaken)

BOOL DmapMemIo(PDMA dma, ULONG offset, ARCHWIDE size, void *mem, BOOL write)
{
	ARCHWIDE realAddr;
	ULONG requiredCount;
	void *source, *dest;

	if (offset == DMA_AUTO_OFFSET)
		offset = dma->writeBoundary;

	requiredCount = (size / dma->sizeOfType) + 1;

	if (write && dma->oldProtect != 0)
	{
		if (!VirtualProtectEx(GetCurrentProcess(), dma->memory, dma->totalSize, dma->oldProtect, &dma->oldProtect))
			return FALSE;

		dma->oldProtect = 0;
	}

	if (write && offset + size > dma->totalSize)
	{
		if (!DmaIssueExpansion(dma, requiredCount + (dma->count / 3)))
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

	
	if (!DmapIsAccessValid(dma, offset, size))
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

BOOL DmaStringWriteA(PDMA dma, LPCSTR format, ...)
{
	BOOL success = FALSE;
	LPSTR buffer = NULL;
	LONG len = 0;
	va_list va;
	va_start(va, format);

	len = HlpPrintFormatBufferExA(&buffer, format, va);

	if (len > 0)
	{
		success = DmaWrite(dma, DMA_AUTO_OFFSET, len * sizeof(char), buffer);
		FREESTRING(buffer);
	}

	va_end(va);

	return success;
}

BOOL DmaStringWriteW(PDMA dma, LPCWSTR format, ...)
{
	BOOL success = FALSE;
	LPWSTR buffer = NULL;
	LONG len = 0;
	va_list va;
	va_start(va, format);

	len = HlpPrintFormatBufferExW(&buffer, format, va);

	if (len > 0)
	{
		success = DmaWrite(dma, DMA_AUTO_OFFSET, len * sizeof(wchar_t), buffer);
		FREESTRING(buffer);
	}

	va_end(va);

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

BOOL DmaPrepareForDirectWrite(PDMA dma, ULONG writeOffset, ARCHWIDE writeSize, void **nativeMem)
{
    if (dma->UnsafeWriteCheckInfo.offset)
        return FALSE;

    if (!DmapIsAccessValid(dma, writeOffset, writeSize))
        return FALSE;

    /*
    dma->UnsafeWriteCheckInfo.offset = writeOffset;
    dma->UnsafeWriteCheckInfo.size = writeSize;

    if (writeOffset < sizeof(ULONG))
    {
    }

    */

    NOTIMPLEMENTED();

    return FALSE;
}

void DmaEndDirectWrite(PDMA dma)
{
    NOTIMPLEMENTED();
}

BOOL DmaPrepareForRead(PDMA dma, void **nativeMem)
{
	BOOL success = VirtualProtectEx(GetCurrentProcess(), dma->memory, dma->totalSize, PAGE_READONLY, &dma->oldProtect);

	if (success)
		*nativeMem = dma->memory;

	return success;
}

void DmaDestroyAdapter(PDMA dma)
{
	if (!dma)
		return;

	if (dma->oldProtect != 0)
		VirtualProtectEx(GetCurrentProcess(), dma->memory, dma->totalSize, dma->oldProtect, &dma->oldProtect);

	if (!dma->ownershipTaken)
		AbMemoryFree(dma->memory);

	memset(dma, 0, sizeof(DMA));

	AbMemoryFree(dma);

}

//UTILITY FUNCS

BOOL UtlExtractPassedParameters(USHORT paramCount, CALLCONVENTION callConv, REGDUMP *regdump, PPASSED_PARAMETER_CONTEXT *paramInfo)
{
	PPASSED_PARAMETER_CONTEXT ppi = NULL;

	ppi = ALLOCOBJECT(PASSED_PARAMETER_CONTEXT);

	if (!ppi)
		return FALSE;

	ppi->paramCount = paramCount;
	ppi->convention = callConv;

    memcpy(&ppi->regCtx, regdump, sizeof(REGDUMP));

	if (!UtlpExtractPassedParameters(ppi))
	{
		FREEOBJECT(ppi);
		return FALSE;
	}

	*paramInfo = ppi;

	return TRUE;
}

duint UtlGetCallerAddress(REGDUMP *regdump)
{
    int limit = 20;
    BASIC_INSTRUCTION_INFO inst = { 0 };
    duint callerIp;
    duint sp;

    sp = (duint)regdump->regcontext.csp;

    AbMemReadGuaranteed(sp, &callerIp, sizeof(duint));

    //Prevent to getting next call as previous caller
    callerIp--;
    limit = 20;

    while (!(inst.call && inst.branch))
    {
        DbgDisasmFastAt(callerIp, &inst);
        callerIp--;

        if (limit-- <= 0)
        {
            callerIp = 0;
            break;
        }
    }

    if (callerIp > 0)
    {
        callerIp++;
        DBGPRINT("Caller of this API at %p", callerIp);

        return callerIp;
    }

    return 0;
}

BOOL UtlInternetReadA(LPCSTR url, BYTE **content, ULONG *contentLength)
{
    BOOL result;
    LPWSTR urlW = HlpAnsiToWideString(url);

    if (!urlW)
        return FALSE;

    result = UtlInternetReadW(urlW, content, contentLength);

    FREESTRING(urlW);
    
    return result;
}

BOOL UtlInternetReadW(LPCWSTR url, BYTE **content, ULONG *contentLength)
{
    HINTERNET session=NULL, connection=NULL, reqHandle=NULL;
    INTERNET_PORT port;
    BOOL result;
    DWORD reqFlag = WINHTTP_FLAG_REFRESH;
    DWORD availData = 0, readData = 0, totalReadSize = 0;
    PDMA dma;
    BYTE *readBuffer = NULL;
    DWORD readbufSize = 0x1000;
    URL_COMPONENTS urlComp;
    WCHAR hostName[128], objectName[64];

    port = HlpBeginsWithW(url, L"https", TRUE, 5) ? 443 : 80;

    if (port == 443)
        reqFlag |= WINHTTP_FLAG_SECURE;

    session = WinHttpOpen(
        L"Mozilla/5.0 (Windows NT 10.0; WOW64; rv:51.0) Gecko/20100101 Firefox/51.0",
        WINHTTP_ACCESS_TYPE_NO_PROXY,
        NULL,
        NULL,
        0
    );


    if (!session)
        return FALSE;

    // Specify an HTTP server.

    memset(&urlComp, 0, sizeof(urlComp));

    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.lpszHostName = hostName;

    urlComp.dwUrlPathLength = (DWORD)-1;
    urlComp.lpszUrlPath = objectName;

    urlComp.dwStructSize = sizeof(urlComp);

    WinHttpCrackUrl(url, wcslen(url), 0, &urlComp);

    connection = WinHttpConnect(session, hostName, port, 0);

    if (!connection)
        goto exit;

    // Create an HTTP request handle.
    reqHandle = WinHttpOpenRequest(connection, L"GET", objectName,
        NULL, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        reqFlag);

    if (!reqHandle)
        goto exit;

    result = WinHttpSendRequest(reqHandle,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0, WINHTTP_NO_REQUEST_DATA, 0,
        0, 0);

    if (!result)
        goto exit;

    result = WinHttpReceiveResponse(reqHandle, NULL);

    if (!result)
        goto exit;

    if (!DmaCreateAdapter(sizeof(BYTE), 0x1000, &dma))
        goto exit;

    readBuffer = (BYTE *)AbMemoryAlloc(readbufSize);

    while(1)
    {
        if (!WinHttpQueryDataAvailable(reqHandle, &availData))
            break;

        if (!availData)
            break;

        if (availData > readbufSize)
        {
            readBuffer = (BYTE *)AbMemoryRealloc(readBuffer, readbufSize + 0x1000);
            readbufSize += 0x1000;

        }

        if (WinHttpReadData(reqHandle, readBuffer, availData, &readData))
        {
            DmaWrite(dma, DMA_AUTO_OFFSET, readData, readBuffer);
            *readBuffer = 0;
            totalReadSize += readData;
        }


    }


    AbMemoryFree(readBuffer);

exit:
    if (reqHandle) WinHttpCloseHandle(reqHandle);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);

    if (totalReadSize > 0)
    {
        DmaTakeMemoryOwnership(dma, (void **)content);
        *contentLength = totalReadSize;
    }
    
    DmaDestroyAdapter(dma);

    return totalReadSize > 0;
}