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

void  HlpRemoveQuotations(LPSTR str)
{
	char *p;
	int len;
	bool hasEndQuote;

	if (!str)
		return;

	p = (char *)str;
	len = (int)strlen(str);

	hasEndQuote = *(p + (len - 1)) == '\"';

	if (*p == '\"')
	{
		len -= hasEndQuote ? 2 : 1;

		memmove(p, p + 1, len);

		*(p + len) = 0;
	}
	else if (hasEndQuote)
	{
		*(p + (len - 1)) = 0;
	}

}

bool HlpBeginsWithA(LPCSTR look, LPCSTR find, LONG findLen)
{

	if (!look || !find)
		return false;

	if (findLen <= 0)
		return false;

	return strstr(look, find) == look;
}

bool HlpEndsWithA(LPCSTR look, LPCSTR find, LONG findLen)
{
	LONG lookLen;

	if (!look || !find)
		return false;

	if (findLen <= 0)
		return false;

	lookLen = (LONG)strlen(look);

	look += lookLen - findLen;

	return strcmp(look, find) == 0;
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