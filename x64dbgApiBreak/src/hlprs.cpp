#include <hlp.h>
#include <varargs.h>

void HlpDebugPrint(const char *format, ...)
{
	va_list vl;
	char content[512] = { 0 };

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


bool HlpBeginsWithA(LPCSTR look, LPCSTR find,BOOL caseSens, LONG findLen)
{
	int lookLen;

	if (!look || !find)
		return false;

	if (findLen <= 0)
		return false;
	
	lookLen = (int)strlen(look);

	if (lookLen < findLen)
		return false;

	if (caseSens)
		return strncmp(look, find, findLen) == 0;

	return _strnicmp(look, find, findLen) == 0;
}

bool HlpEndsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen)
{
	int lookLen;

	if (!look || !find)
		return false;

	if (findLen <= 0)
		return false;

	lookLen = (int)strlen(look);

	if (lookLen < findLen)
		return false;

	look += lookLen - findLen;

	if (caseSens)
		return strcmp(look, find) == 0;

	return _stricmp(look, find) == 0;
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