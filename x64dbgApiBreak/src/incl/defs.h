#ifndef __DEFS_H_
#define __DEFS_H_

#include <hlp.h>

#define FORWARDED extern
#define INTERNAL_EXPORT FORWARDED
#define INTERNAL 

#define DBG_LIBEXPORT extern "C" __declspec(dllexport)

#define DBGPRINT(s,...) HlpDebugPrint("ApiBreak: " s "\n",##__VA_ARGS__)


#define ALLOCOBJECT(type) (type *)AbMemoryAlloc(sizeof(type))
#define FREEOBJECT(ob) AbMemoryFree(ob)

#define ALLOCSTRINGW(size) (LPWSTR)AbMemoryAlloc(sizeof(WCHAR) * ((size)+1))
#define ALLOCSTRINGA(size) (LPSTR)AbMemoryAlloc(sizeof(CHAR) * ((size)+1))

#define FREESTRING(str) AbMemoryFree(str)



#endif // !__DEFS_H_
