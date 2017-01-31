#ifndef __HELPER_H_
#define __HELPER_H_

#include <corelib.h>


#define HLP_TRIM_LEFT		1
#define HLP_TRIM_RIGHT		2
#define HLP_TRIM_BOTH		(HLP_TRIM_LEFT | HLP_TRIM_RIGHT)

void HlpDebugPrint(const char *format, ...);

LPWSTR HlpAnsiToWideString(LPCSTR str);
LPSTR HlpWideToAnsiString(LPCWSTR str);

void  HlpTrimChar(LPSTR str, CHAR chr, int option);


#define HlpRemoveQuotations(str) HlpTrimChar(str, '\"', HLP_TRIM_BOTH)

bool HlpBeginsWithA(LPCSTR look, LPCSTR find, LONG findLen);

bool HlpEndsWithA(LPCSTR look, LPCSTR find, LONG findLen);

LONG HlpPrintFormatBufferExA(LPSTR *buffer, LPCSTR format, va_list vl);

LONG HlpPrintFormatBufferA(LPSTR *buffer, LPCSTR format, ...);

#endif // !__HELPER_H_
