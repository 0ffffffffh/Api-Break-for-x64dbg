#include <corelib.h>
#include <settings.h>
#include <dlgs/MainForm.hpp>
#include <dlgs/SettingsForm.hpp>

#define MN_BASE					0xFA00FA

#define MN_ABOUT				MN_BASE
#define MN_SHOWMAINFORM			MN_BASE + 1
#define MN_SHOWSETTINGSFORM		MN_BASE + 2

#define DWS_IDLE				0
#define DWS_CREATEPROCESS		1
#define DWS_ATTACHEDPROCESS		2
#define DWS_EXITEDPROCESS		3
#define DWS_DETACHEDPROCESS		4

const char *DBGSTATE_STRINGS[5] =
{
	"Idle",
	"Process Created",
	"Process Attached",
	"Process Exited",
	"Process Detached"
};

BYTE								AbpDbgState;
Script::Module::ModuleInfo			AbpCurrentMainModule;

INTERNAL int		AbPluginHandle;
INTERNAL HWND		AbHwndDlgHandle;
INTERNAL int		AbMenuHandle;
INTERNAL int		AbMenuDisasmHandle;
INTERNAL int		AbMenuDumpHandle;
INTERNAL int		AbMenuStackHandle;
INTERNAL HMODULE	AbPluginModule;

INTERNAL void AbiRaiseDeferredLoader(const char *dllName, duint base);

INTERNAL void AbiInitDynapi();
INTERNAL void AbiUninitDynapi();
INTERNAL void AbiReleaseDeferredResources();
INTERNAL void AbiEmptyInstructionCache();

unordered_map<duint, BpCallbackContext *> AbpBpCallbacks;

INTERNAL_EXPORT Script::Module::ModuleInfo *AbiGetCurrentModuleInfo()
{
	return &AbpCurrentMainModule;
}

BpCallbackContext *AbpLookupBpCallback(duint addr)
{
	unordered_map<duint, BpCallbackContext *>::iterator it;

	if (AbpBpCallbacks.size() == 0)
		return NULL;

	it = AbpBpCallbacks.find(addr);

	if (it == AbpBpCallbacks.end())
		return NULL;

	return it->second;
}

void AbpReleaseBreakpointCallbackResources()
{
	unordered_map<duint, BpCallbackContext *>::iterator it;

	if (AbpBpCallbacks.size() == 0)
		return;

	while (AbpBpCallbacks.size() > 0)
	{
		it = AbpBpCallbacks.begin();
		FREEOBJECT(it->second);
		AbpBpCallbacks.erase(it);
	}

}

bool AbRegisterBpCallback(BpCallbackContext *cbctx)
{
	if (AbpLookupBpCallback(cbctx->bpAddr))
		return false;

	AbpBpCallbacks.insert({ cbctx->bpAddr, cbctx });

	return true;
}


void AbDeregisterBpCallback(BpCallbackContext *cbctx)
{
	unordered_map<duint, BpCallbackContext *>::iterator iter;

	iter = AbpBpCallbacks.find(cbctx->bpAddr);

	if (iter == AbpBpCallbacks.end())
		return;

	AbpBpCallbacks.erase(iter);
}

void AbpRaiseDbgStateChangeEvent()
{
	DBGPRINT("%s (%s)", AbpCurrentMainModule.name, DBGSTATE_STRINGS[AbpDbgState]);


	if (AbpDbgState > DWS_ATTACHEDPROCESS)
	{
		//And after the handler callback. 
		//set it to dbg is now idle if current state exited or detached
		AbpDbgState = DWS_IDLE;
	}
}

void AbReleaseAllSystemResources(bool isInShutdown)
{
	AbiEmptyInstructionCache();
	AbiReleaseDeferredResources();
	AbpReleaseBreakpointCallbackResources();
	AbReleaseModuleResources();

	if (isInShutdown)
	{
		AbiUninitDynapi();
		UiForceCloseAllActiveWindows();
	}
}

void __AbpInitMenu()
{
	_plugin_menuaddentry(AbMenuHandle, MN_SHOWMAINFORM, "set an API breakpoint");
	_plugin_menuaddentry(AbMenuHandle, MN_SHOWSETTINGSFORM, "settings");
	_plugin_menuaddentry(AbMenuHandle, MN_ABOUT, "about?");
	
}

DBG_LIBEXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
	initStruct->sdkVersion = PLUG_SDKVERSION;
	initStruct->pluginVersion = MAKEWORD(AB_VERSION_MAJOR, AB_VERSION_MINOR);
	strcpy_s(initStruct->pluginName, 256, "Api Break");
	AbPluginHandle = initStruct->pluginHandle;

	AbSettingsLoad();
	AbiInitDynapi();

	return true;
}

DBG_LIBEXPORT bool plugstop()
{
	AbReleaseAllSystemResources(true);
	return true;
}

DBG_LIBEXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
	AbHwndDlgHandle = setupStruct->hwndDlg;
	AbMenuHandle = setupStruct->hMenu;
	AbMenuDisasmHandle = setupStruct->hMenuDisasm;
	AbMenuDumpHandle = setupStruct->hMenuDump;
	AbMenuStackHandle = setupStruct->hMenuStack;

	__AbpInitMenu();
}


DBG_LIBEXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
	if (info->hEntry == MN_ABOUT)
	{
		MessageBoxA(AbHwndDlgHandle,
			AB_APPNAME " - ver: " AB_VERSTR "\r\n"
			"build on: " AB_BUILD_TIME "\r\n\r\n"
			"by oguz (ozzy) kartal (2017)\r\n\r\n"
			"http://oguzkartal.net ;)",
			"About - " AB_APPTITLE, 
			MB_ICONINFORMATION);
	}
	else if (info->hEntry == MN_SHOWMAINFORM)
	{
		//Ps. MainForm automatically deletes itself when the window closed.
		MainForm *mainForm = new MainForm();
		mainForm->ShowDialog();
	}
	else if (info->hEntry == MN_SHOWSETTINGSFORM)
	{
		SettingsForm *settingsForm = new SettingsForm();
		settingsForm->ShowDialog();
	}
}


DBG_LIBEXPORT void CBLOADDLL(CBTYPE cbType, PLUG_CB_LOADDLL *dllLoad)
{
	duint base = DbgModBaseFromName(dllLoad->modname);

	if (!base)
	{
		DBGPRINT("Could not get dll base fro %s", dllLoad->modname);
		return;
	}

	AbiRaiseDeferredLoader(dllLoad->modname, base);
}

DBG_LIBEXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	BpCallbackContext *bpcb = NULL;

	bpcb = AbpLookupBpCallback(info->breakpoint->addr);

	if (bpcb != NULL)
	{
		DBGPRINT("Special bp found. Raising breakpoint callback");
		bpcb->bp = info->breakpoint;
		bpcb->callback(bpcb);
	}
}

DBG_LIBEXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS *newProc)
{
	//If a process is attached the debuggers calls this callback after the ATTACHED?
	//So if previously mode is attached we dont need to change state. Let it stay attached
	if (AbpDbgState == DWS_ATTACHEDPROCESS)
	{
		//get module info. cuz we cant get the modinfo from in the attach callback 
		goto resumeExec;
	}

	AbpDbgState = DWS_CREATEPROCESS;

resumeExec:

	AbGetDebuggedModuleInfo(&AbpCurrentMainModule);

	AbpRaiseDbgStateChangeEvent();
}

DBG_LIBEXPORT void CBEXITPROCESS(CBTYPE cbType, PLUG_CB_EXITPROCESS *exitProc)
{
	AbpDbgState = DWS_EXITEDPROCESS;

	AbpRaiseDbgStateChangeEvent();
}

DBG_LIBEXPORT void CBATTACH(CBTYPE cbType, PLUG_CB_ATTACH *attachedProc)
{
	AbpDbgState = DWS_ATTACHEDPROCESS;
}

DBG_LIBEXPORT void CBDETACH(CBTYPE cbType, PLUG_CB_DETACH *detachedProc)
{
	AbpDbgState = DWS_DETACHEDPROCESS;
	
	AbpRaiseDbgStateChangeEvent();
}

BOOL WINAPI DllMain(
	_In_ HINSTANCE hinstDLL,
	_In_ DWORD     fdwReason,
	_In_ LPVOID    lpvReserved
)
{
	AbPluginModule = (HMODULE)hinstDLL;
	return TRUE;
}