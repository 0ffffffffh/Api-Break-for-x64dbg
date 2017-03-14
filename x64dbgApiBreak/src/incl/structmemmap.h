#ifndef __STRUCTMEMMAP_H__
#define __STRUCTMEMMAP_H__

#include <corelib.h>
#include <util.h>

typedef struct __FNSIGN *PFNSIGN;



BOOL SmmMapFunctionCall(PPASSED_PARAMETER_CONTEXT passedParams, PFNSIGN fnSign, ApiFunctionInfo *afi);

BOOL SmmMapRemoteMemory(duint memory, ULONG size, const char *typeName);

BOOL SmmMapMemory(void *memory, ULONG size, const char *typeName);

BOOL SmmGetFunctionSignature(const char *module, const char *function, PFNSIGN *signInfo);

BOOL SmmSigHasOutArgument(PFNSIGN signInfo);

BOOL SmmParseType(LPCSTR typeDefString, WORD *typeCount);

BOOL SmmParseFromFileW(LPCWSTR fileName, WORD *typeCount);

BOOL SmmParseFromFileA(LPCSTR fileName, WORD *typeCount);

#endif // !__STRUCTMEMMAP_H__
