#ifndef __HELPER_H_
#define __HELPER_H_

#include <corelib.h>

void HlpDebugPrint(const char *format, ...);

LPWSTR HlpAnsiToWideString(LPCSTR str);
LPSTR HlpWideToAnsiString(LPCWSTR str);

void HlpRemoveQuotations(LPSTR str);

bool HlpBeginsWithA(LPCSTR look, LPCSTR find, LONG findLen);

bool HlpEndsWithA(LPCSTR look, LPCSTR find, LONG findLen);

LONG HlpPrintFormatBufferExA(LPSTR *buffer, LPCSTR format, va_list vl);

LONG HlpPrintFormatBufferA(LPSTR *buffer, LPCSTR format, ...);

#endif // !__HELPER_H_
