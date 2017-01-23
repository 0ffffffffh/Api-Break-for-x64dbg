#include <corelib.h>
#include <dlgs/MainForm.hpp>

#define MN_BASE				0xFA00FA

#define MN_ABOUT			MN_BASE
#define MN_SHOWMAINFORM		MN_ABOUT + 1

INTERNAL int		AbPluginHandle;
INTERNAL HWND		AbHwndDlgHandle;
INTERNAL int		AbMenuHandle;
INTERNAL int		AbMenuDisasmHandle;
INTERNAL int		AbMenuDumpHandle;
INTERNAL int		AbMenuStackHandle;
INTERNAL HMODULE	AbPluginModule;


unordered_map<duint, BpCallbackContext *> AbpBpCallbacks;

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

void AbpDestroyBpCallbacks()
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

void AbpInitMenu()
{
	_plugin_menuaddentry(AbMenuHandle, MN_SHOWMAINFORM, "set an API breakpoint");
	_plugin_menuaddentry(AbMenuHandle, MN_ABOUT, "about?");
	
}

DBG_LIBEXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
	initStruct->sdkVersion = PLUG_SDKVERSION;
	initStruct->pluginVersion = MAKEWORD(AB_VERSION_MAJOR, AB_VERSION_MINOR);
	strcpy_s(initStruct->pluginName, 256, "Api Break");
	AbPluginHandle = initStruct->pluginHandle;
	return true;
}

DBG_LIBEXPORT bool plugstop()
{
	AbpDestroyBpCallbacks();
	AbReleaseResources();

	UiWrapper::DestroyAllActiveWindows();

	return true;
}

DBG_LIBEXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
	AbHwndDlgHandle = setupStruct->hwndDlg;
	AbMenuHandle = setupStruct->hMenu;
	AbMenuDisasmHandle = setupStruct->hMenuDisasm;
	AbMenuDumpHandle = setupStruct->hMenuDump;
	AbMenuStackHandle = setupStruct->hMenuStack;

	AbpInitMenu();
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

DBG_LIBEXPORT void CBMENUENTRY(CBTYPE cbType, PLUG_CB_MENUENTRY* info)
{
	if (info->hEntry == MN_ABOUT)
	{
		MessageBoxA(AbHwndDlgHandle,
			AB_APPNAME " - ver: " AB_VERSTR "\r\n"
			"build on: " AB_BUILD_TIME "\r\n\r\n"
			"by oguz (ozzy) kartal (2017)\r\n\r\n"
			"http://oguzkartal.net ;)",
			"about", 
			MB_ICONINFORMATION);
	}
	else if (info->hEntry == MN_SHOWMAINFORM)
	{
		//Ps. MainForm automatically deletes itself when the window closed.
		MainForm *mainForm = new MainForm();
		mainForm->ShowDialog();
	}
}


DBG_LIBEXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	BpCallbackContext *bpcb = NULL;

	bpcb = AbpLookupBpCallback(info->breakpoint->addr);

	DBGPRINT("Breakpoint hit: %p", bpcb);

	if (bpcb != NULL)
	{
		bpcb->bp = info->breakpoint;
		bpcb->callback(bpcb);
	}
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