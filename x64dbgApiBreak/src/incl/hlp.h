#ifndef __HELPER_H_
#define __HELPER_H_

#include <corelib.h>


#define HLP_TRIM_LEFT       1
#define HLP_TRIM_RIGHT      2
#define HLP_TRIM_BOTH       (HLP_TRIM_LEFT | HLP_TRIM_RIGHT)


void HlpDebugPrint(const char *format, ...);

LPSTR HlpCloneStringA(LPCSTR str);

LPWSTR HlpAnsiToWideString(LPCSTR str);
LPSTR HlpWideToAnsiString(LPCWSTR str);

bool  HlpTrimChar(LPSTR str, CHAR chr, int option);


#define HlpRemoveQuotations(str) HlpTrimChar(str, '\"', HLP_TRIM_BOTH)

bool HlpReplaceStringW(LPWSTR string, ULONG stringMaxSize, LPCWSTR find, LPCWSTR replace);

bool HlpReplaceStringA(LPSTR string, ULONG stringMaxSize, LPCSTR find, LPCSTR replace);

bool HlpBeginsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen);

bool HlpBeginsWithW(LPCWSTR look, LPCWSTR find, BOOL caseSens, LONG findLen);

bool HlpEndsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen);

bool HlpEndsWithW(LPCWSTR look, LPCWSTR find, BOOL caseSens, LONG findLen);

LONG HlpPrintFormatBufferExA(LPSTR *buffer, LPCSTR format, va_list vl);

LONG HlpPrintFormatBufferA(LPSTR *buffer, LPCSTR format, ...);

LONG HlpPrintFormatBufferExW(LPWSTR *buffer, LPCWSTR format, va_list vl);

LONG HlpPrintFormatBufferW(LPWSTR *buffer, LPCWSTR format, ...);

LONG HlpConcateStringFormatA(LPSTR buffer, LONG bufLen, LPCSTR format, ...);

LONG HlpPathFromFilenameA(LPSTR fileName, LPSTR path, LONG pathBufSize, CHAR sep);

LONG HlpPathFromFilenameW(LPWSTR fileName, LPWSTR path, LONG pathBufSize, WCHAR sep);


#endif // !__HELPER_H_
