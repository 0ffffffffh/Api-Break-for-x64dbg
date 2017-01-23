

//Disable shitty paranoid warnings
#pragma warning(disable:4091)
#pragma warning(disable:4800)
#pragma warning(disable:4244)

#define _CRT_SECURE_NO_WARNINGS

#define CSR_FAILED					0
#define CSR_COMPLETELY_SUCCESS		1
#define CSR_PARTIAL_SUCCESS			2



#include <Windows.h>
#include <pluginsdk/_plugin_types.h>
#include <pluginsdk/_plugins.h>
#include <pluginsdk/TitanEngine/TitanEngine.h>

#include <unordered_map>
#include <vector>

using namespace std;

#define _T(t) __##t

struct __ModuleApiInfo;
struct __BpCallbackContext;

typedef struct
{
	_T(ModuleApiInfo *)			ownerModule;
	duint *						calls;
	int							callCount;
	duint						rva;
	char						name[MAX_LABEL_SIZE];
}ApiFunctionInfo;

typedef unordered_map<
	std::string,
	ApiFunctionInfo *,
	std::tr1::hash<std::string>>	apilist;

typedef struct __ModuleApiInfo
{
	duint				baseAddr;
	duint				listCount;
	apilist*			apiList;
	char				name[MAX_MODULE_SIZE];
}ModuleApiInfo;

typedef vector<ModuleApiInfo *>		modlist;


typedef void(*APIMODULE_ENUM_PROC)(LPCSTR,void *);

typedef void(*AB_BREAKPOINT_CALLBACK)(struct __BpCallbackContext *);

typedef struct __BpCallbackContext
{
	duint						bpAddr;
	BRIDGEBP *					bp;
	AB_BREAKPOINT_CALLBACK		callback;
	void *						user;
}BpCallbackContext;

#define _stringfy(x) #x

#define AB_VERSION_MAJOR 0
#define AB_VERSION_MINOR 3
#define AB_VERSION_BUILD 13

#define AB_PHASE		"BETA"

#define AB_APPNAME		"Api Break (" AB_PHASE ")"

#define AB_VERSION_STRING(maj,min,build) _stringfy(maj) "." _stringfy(min) " Build: " _stringfy(build)

#define AB_VERSTR AB_VERSION_STRING(AB_VERSION_MAJOR,AB_VERSION_MINOR,AB_VERSION_BUILD)
#define AB_BUILD_TIME __DATE__ " " __TIME__

bool AbHasDebuggingProcess();

void AbReleaseResources();

void AbGetDebuggingProcessName(char *buffer);

bool AbLoadAvailableModuleAPIs(bool onlyImportsByExe);

int AbEnumModuleNames(APIMODULE_ENUM_PROC enumProc, void *user);

void AbEnumApiFunctionNames(APIMODULE_ENUM_PROC enumProc, const char *moduleName, void *user);

bool AbSetBreakpointOnCallers(const char *module, const char *apiFunction, int *bpRefCount);

bool AbSetBreakpoint(const char *module, const char *apiFunction, duint *funcAddr);

bool AbSetBreakpointEx(const char *module, const char *apiFunction, duint *funcAddr, AB_BREAKPOINT_CALLBACK bpCallback, void *user);

bool AbRegisterBpCallback(BpCallbackContext *cbctx);

void AbDeregisterBpCallback(BpCallbackContext *cbctx);