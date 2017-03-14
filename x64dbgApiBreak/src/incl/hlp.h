#ifndef __HELPER_H_
#define __HELPER_H_

#include <corelib.h>


#define HLP_TRIM_LEFT		1
#define HLP_TRIM_RIGHT		2
#define HLP_TRIM_BOTH		(HLP_TRIM_LEFT | HLP_TRIM_RIGHT)


void HlpDebugPrint(const char *format, ...);

LPSTR HlpCloneStringA(LPCSTR str);

LPWSTR HlpAnsiToWideString(LPCSTR str);
LPSTR HlpWideToAnsiString(LPCWSTR str);

bool  HlpTrimChar(LPSTR str, CHAR chr, int option);


#define HlpRemoveQuotations(str) HlpTrimChar(str, '\"', HLP_TRIM_BOTH)

bool HlpBeginsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen);

bool HlpEndsWithA(LPCSTR look, LPCSTR find, BOOL caseSens, LONG findLen);

LONG HlpPrintFormatBufferExA(LPSTR *buffer, LPCSTR format, va_list vl);

LONG HlpPrintFormatBufferA(LPSTR *buffer, LPCSTR format, ...);

LONG HlpConcateStringFormatA(LPSTR buffer, LONG bufLen, LPCSTR format, ...);

LONG HlpPathFromFilenameA(LPSTR fileName, LPSTR path, LONG pathBufSize);

LONG HlpPathFromFilenameW(LPWSTR fileName, LPWSTR path, LONG pathBufSize);


#endif // !__HELPER_H_
