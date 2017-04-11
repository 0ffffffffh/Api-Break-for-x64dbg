#ifndef __APIBREAK_H__
#define __APIBREAK_H__


//Disable shitty paranoid warnings
#pragma warning(disable:4091)
#pragma warning(disable:4800)
#pragma warning(disable:4244)
#pragma warning(disable:4267)

#define _CRT_SECURE_NO_WARNINGS

#define CSR_FAILED                  0
#define CSR_COMPLETELY_SUCCESS      1
#define CSR_PARTIAL_SUCCESS         2



#include <Windows.h>
#include <pluginsdk/_plugin_types.h>
#include <pluginsdk/_plugins.h>
#include <pluginsdk/TitanEngine/TitanEngine.h>
#include <pluginsdk/_scriptapi_module.h>
#include <pluginsdk/_scriptapi_debug.h>
#include <pluginsdk/_scriptapi_register.h>


#include <unordered_map>
#include <vector>

using namespace std;

#define _T(t) __##t

struct __ModuleApiInfo;
struct __BpCallbackContext;

typedef struct
{
    _T(ModuleApiInfo *)         ownerModule;
    struct
    {
        duint *                 calls;
        int                     callCount;
        int                     callListSize;
    }callInfo;
    duint                       rva;
    char                        name[MAX_LABEL_SIZE];
}ApiFunctionInfo;

typedef unordered_map<
    std::string,
    ApiFunctionInfo *,
    std::tr1::hash<std::string>>    apilist;

typedef struct __ModuleApiInfo
{
    duint               baseAddr;
    duint               listCount;
    apilist*            apiList;
    char                name[MAX_MODULE_SIZE];
}ModuleApiInfo;

typedef vector<ModuleApiInfo *>     modlist;


typedef void(*APIMODULE_ENUM_PROC)(LPCSTR,void *);

typedef void(*AB_BREAKPOINT_CALLBACK)(struct __BpCallbackContext *);

struct __BREAKPOINT_INFO;

typedef struct __BpCallbackContext
{
    duint                       bpAddr;
    BRIDGEBP *                  bp;
    REGDUMP                     regContext;
    AB_BREAKPOINT_CALLBACK      callback;
    ApiFunctionInfo             *afi;
    struct __BREAKPOINT_INFO    *ownerBreakpoint;
    void *                      user;
}BpCallbackContext;

typedef struct __BREAKPOINT_INFO
{
    duint                   addr;
    duint                   hitCount;
    DWORD                   options;
    DWORD                   status;
    BpCallbackContext*      cbctx;
}*PBREAKPOINT_INFO, BREAKPOINT_INFO;

#define BPS_NONE                0x00000000
#define BPS_EXECUTING_HANDLER   0x00000001
#define BPS_EXECUTING_CALLBACK  0x00000002

#define BPO_NONE                0
#define BPO_BACKTRACK           1
#define BPO_SINGLESHOT          2


void AbDebuggerRun();

void AbDebuggerPause();

void AbDebuggerWaitUntilPaused();

bool AbCmdExecFormat(const char *format, ...);

bool AbGetDebuggedImageName(char *buffer);

bool AbGetDebuggedModuleInfo(Script::Module::ModuleInfo *modInfo);

bool AbGetDebuggedModulePath(char *pathBuf, int bufLen);

duint AbGetDebuggedImageBase();

bool AbHasDebuggingProcess();

void AbReleaseModuleResources();

bool AbLoadAvailableModuleAPIs(bool onlyImportsByExe);

int AbEnumModuleNames(APIMODULE_ENUM_PROC enumProc, void *user);

void AbEnumApiFunctionNames(APIMODULE_ENUM_PROC enumProc, const char *moduleName, void *user);

bool AbSetAPIBreakpointOnCallers(const char *module, const char *apiFunction);

bool AbSetAPIBreakpoint(const char *module, const char *apiFunction, duint *funcAddr);

bool AbSetInstructionBreakpoint(duint instrAddr, AB_BREAKPOINT_CALLBACK callback, void *user, bool singleShot);

bool AbSetBreakpointEx(const char *module, const char *apiFunction, duint *funcAddr, DWORD bpo, AB_BREAKPOINT_CALLBACK bpCallback, void *user);

bool AbDeleteBreakpoint(duint addr);


void AbpReleaseBreakpointResources();

PBREAKPOINT_INFO AbpLookupBreakpoint(duint addr);

bool AbpRegisterBreakpoint(duint addr, DWORD options, BpCallbackContext *cbctx);

bool AbpDeregisterBreakpoint(duint addr);

#endif // !__APIBREAK_H__