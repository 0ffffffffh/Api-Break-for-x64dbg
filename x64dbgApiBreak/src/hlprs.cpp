#include <hlp.h>
#include <varargs.h>

void HlpDebugPrint(const char *format, ...)
{
    va_list vl;
    char content[0x500] = { 0 };

    va_start(vl, format);
    _vsnprintf(content, sizeof(content), format, vl);
    va_end(vl);

    _plugin_logputs(content);

#ifdef _DEBUG
    OutputDebugStringA(content);
#endif

}


LPSTR HlpCloneStringA(LPCSTR str)
{
    LPSTR clone;
    int len;

    if (!str)
        return NULL;

    len = (int)strlen(str);

    clone = ALLOCSTRINGA(len);

    if (!clone)
        return NULL;

    memcpy(clone, str, len + 1);

    return clone;
}

LPWSTR HlpAnsiToWideString(LPCSTR str)
{
    ULONG slen;
    LPWSTR wstr;

    if (!str)
        return NULL;

    slen = lstrlenA((LPCSTR)str);

    wstr = (LPWSTR)ALLOCSTRINGW(slen);

    if (!wstr)
        return NULL;

    if (MultiByteToWideChar(CP_ACP, MB_COMPOSITE, str, slen, wstr, slen) == slen)
        return wstr;

    FREESTRING(wstr);
    return NULL;
}

LPSTR HlpWideToAnsiString(LPCWSTR str)
{
    ULONG slen;
    LPSTR astr;

    if (!str)
        return NULL;

    slen = lstrlenW((LPCWSTR)str);

    astr = (LPSTR)ALLOCSTRINGA(slen);

    if (!astr)
        return NULL;

    if (WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, str, slen, astr, slen, NULL, NULL) == slen)
        return astr;

    FREESTRING(astr);
    return NULL;
}

bool  HlpTrimChar(LPSTR str, CHAR chr, int option)
{
    char *p;
    int len;
    bool hasEndChr = false;

    if (!str)
        return false;

    if (!option)
        return false;

    p = (char *)str;
    len = (int)strlen(str);

    if (option & HLP_TRIM_RIGHT)
        hasEndChr = *(p + (len - 1)) == chr;

    if (*p == chr && (option & HLP_TRIM_LEFT))
    {
        len -= hasEndChr ? 2 : 1;

        memmove(p, p + 1, len);

        *(p + len) = 0;
        return true;
    }
    else if (hasEndChr)
    {
        *(p + (len - 1)) = 0;
        return true;
    }

    return false;
}

#include <util.h>

bool HlpReplaceStringW(LPWSTR string, ULONG stringMaxSize, LPCWSTR find, LPCWSTR replace)
{
    bool result = false;
    ULONG fCount=0, foundIndex=0;
    LONG findLen, replLen, strLen, copyLen;
    LPWSTR px;
    LONG shiftDelta;
    PDMA dma;
    BYTE *memSrc, *memDst;

    if (!DmaCreateAdapter(sizeof(ULONG), 20, &dma))
        return false;

    findLen = wcslen(find);
    replLen = wcslen(replace);
    strLen = wcslen(string);
    
    shiftDelta = replLen - findLen;
    
    px = string - 1;

    while(1)
    {
        px = wcsstr(px + 1, find);
        
        if (!px)
            break;

        foundIndex = px - string;

        if (fCount)
            foundIndex += fCount * shiftDelta;

        DmaWrite(dma, DMA_AUTO_OFFSET, sizeof(ULONG), &foundIndex);

        fCount++;
    }

    if (!fCount)
        goto exit;

    if (shiftDelta > 0)
    {
        if (strLen + (shiftDelta * fCount) > stringMaxSize)
            goto exit;
    }

    for (ULONG i = 0;i < fCount;i++)
    {
        DmaReadTypeAlignedSequence(dma, i, 1, &foundIndex);
        memSrc = (BYTE *)((foundIndex + findLen) * sizeof(WCHAR));
        memDst = (BYTE *)((foundIndex + replLen) * sizeof(WCHAR));

        memSrc += (duint)string;
        memDst += (duint)string;

        copyLen = (strLen - ((LPWSTR)memSrc - string)) * sizeof(WCHAR);

        memmove(memDst, memSrc, copyLen);

        
        if (shiftDelta < 0)
        {
            //That's a reducing of the string. We need to null-term new end of the string

            *((LPWSTR)(memSrc + copyLen - sizeof(WCHAR) * abs(shiftDelta)))
                = L'\0';
        }

        memcpy(string + foundIndex, replace, sizeof(WCHAR) * replLen);


        strLen += shiftDelta;

    }

    result = true;

exit:

    DmaDestroyAdapter(dma);

    return result;
}

bool HlpReplaceStringA(LPSTR string, ULONG stringMaxSize, LPCSTR find, LPCSTR replace)
{
    bool result;
    LPWSTR stringW;
    LPWSTR findW, replaceW;
    LPSTR resultA;

    stringW = HlpAnsiToWideString(string);

    findW = HlpAnsiToWideString(find);
    replaceW = HlpAnsiToWideString(replace);

    result = HlpReplaceStringW(stringW, stringMaxSize, findW, replaceW);

    FREESTRING(findW);
    FREESTRING(replaceW);

    resultA = HlpWideToAnsiString(stringW);

    FREESTRING(stringW);

    strcpy(string, resultA);

    FREESTRING(resultA);

    return result;
}

bool HlpBeginsWithA(LPCSTR look, LPCSTR find,BOOL caseSens, LONG findLen)
{
    bool result;

    LPWSTR lookW = HlpAnsiToWideString(look);
    LPWSTR findW = HlpAnsiToWideString(find);


    result = HlpBeginsWithW(lookW, findW, caseSens, findLen);

    FREESTRING(lookW);
    FREESTRING(findW);

    return result;
}

bool HlpBeginsWithW(LPCWSTR look, LPCWSTR find, BOOL caseSens, LONG findLen)
{
    int lookLen;

    if (!look || !find)
        return false;

    if (findLen <= 0)
        return false;

    lookLen = (int)wcslen(look);

    if (lookLen < findLen)
        return false;

    if (caseSens)
        return wcsncmp(look, find, findLen) == 0;

    return _wcsnicmp(look, find, findLen) == 0;
}

bool HlpEndsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen)
{
    bool result;

    LPWSTR lookW = HlpAnsiToWideString(look);
    LPWSTR findW = HlpAnsiToWideString(find);


    result = HlpEndsWithW(lookW, findW, caseSens, findLen);

    FREESTRING(lookW);
    FREESTRING(findW);

    return result;
}

bool HlpEndsWithW(LPCWSTR look, LPCWSTR find, BOOL caseSens, LONG findLen)
{
    int lookLen;

    if (!look || !find)
        return false;

    if (findLen <= 0)
        return false;

    lookLen = (int)wcslen(look);

    if (lookLen < findLen)
        return false;

    look += lookLen - findLen;

    if (caseSens)
        return wcscmp(look, find) == 0;

    return _wcsicmp(look, find) == 0;
}

LONG HlpPrintFormatBufferExA(LPSTR *buffer, LPCSTR format, va_list vl)
{
    int reqLen = 0;

    reqLen = _vsnprintf(NULL, NULL, format, vl);

    *buffer = ALLOCSTRINGA(reqLen);

    if (*buffer == NULL)
        return false;

    _vsnprintf(*buffer, reqLen + 1, format, vl);

    return reqLen;
}

LONG HlpPrintFormatBufferA(LPSTR *buffer, LPCSTR format, ...)
{
    va_list vl;
    int reqLen;
    
    va_start(vl, format);
    
    reqLen = HlpPrintFormatBufferExA(buffer, format, vl);

    va_end(vl);

    return reqLen;
}


LONG HlpPrintFormatBufferExW(LPWSTR *buffer, LPCWSTR format, va_list vl)
{
    int reqLen = 0;

    reqLen = _vsnwprintf(NULL, NULL, format, vl);

    *buffer = ALLOCSTRINGW(reqLen);

    if (*buffer == NULL)
        return false;

    _vsnwprintf(*buffer, reqLen + 1, format, vl);

    return reqLen;
}

LONG HlpPrintFormatBufferW(LPWSTR *buffer, LPCWSTR format, ...)
{
    va_list vl;
    int reqLen;

    va_start(vl, format);

    reqLen = HlpPrintFormatBufferExW(buffer, format, vl);

    va_end(vl);

    return reqLen;
}

LONG HlpConcateStringFormatA(LPSTR buffer, LONG bufLen, LPCSTR format, ...)
{
    LONG currLen, remain,needLen;
    va_list vl;
    currLen = strlen(buffer);

    remain = (bufLen-1) - currLen;

    va_start(vl, format);

    needLen = _vsnprintf(NULL, NULL, format, vl);

    if (remain < needLen)
        return 0;

    vsprintf(buffer + currLen, format, vl);

    va_end(vl);

    return needLen;
}

LONG HlpPathFromFilenameA(LPSTR fileName, LPSTR path, LONG pathBufSize, CHAR sep)
{
    char c;
    LONG pathLen = 0,i=0;
    LPSTR pathPtr = path;

    
    while ((c = *(fileName + i)) != 0)
    {
        if (c == sep)
            pathLen = i;

        i++;

        if (i >= pathBufSize)
            return 0;

        *pathPtr++ = c;
    }

    if (pathLen == 0 && i > 0)
        return i;

    pathLen++;

    path[pathLen] = 0;

    return pathLen;
}

LONG HlpPathFromFilenameW(LPWSTR fileName, LPWSTR path, LONG pathBufSize, WCHAR sep)
{
    wchar_t c;
    LONG pathLen = 0, i = 0;
    LPWSTR pathPtr = path;

    while ((c = *(fileName + i)) != 0)
    {
        if (c == sep)
            pathLen = i;

        i++;

        if (i >= pathBufSize)
            return 0;

        *pathPtr++ = c;
    }

    if (pathLen == 0 && i > 0)
        return i;

    pathLen++;

    path[pathLen] = 0;

    return pathLen;
}