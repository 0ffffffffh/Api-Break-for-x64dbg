#include <corelib.h>
#include <settings.h>
#include <dlgs/MainForm.hpp>
#include <dlgs/SettingsForm.hpp>

#define MN_BASE					0xFA00FA

#define MN_ABOUT				MN_BASE
#define MN_SHOWMAINFORM			MN_BASE + 1
#define MN_SHOWSETTINGSFORM		MN_BASE + 2
#define MN_TESTSLOT				MN_BASE + 3

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
BOOL								AbpNeedsReload=FALSE;

INTERNAL int		AbPluginHandle;
INTERNAL HWND		AbHwndDlgHandle;
INTERNAL int		AbMenuHandle;
INTERNAL int		AbMenuDisasmHandle;
INTERNAL int		AbMenuDumpHandle;
INTERNAL int		AbMenuStackHandle;
INTERNAL HMODULE	AbPluginModule;

INTERNAL WORD       AbParsedTypeCount = 0;

INTERNAL void AbiRaiseDeferredLoader(const char *dllName, duint base);

INTERNAL void AbiInitDynapi();
INTERNAL void AbiUninitDynapi();
INTERNAL void AbiReleaseDeferredResources();
INTERNAL void AbiEmptyInstructionCache();

unordered_map<duint, BpCallbackContext *>	AbpBpCallbacks;
BOOL										AbpHasPendingInit = FALSE;

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

int WINAPI Loader(void *)
{
	AbLoadAvailableModuleAPIs(true);
	return 0;
}

void AbpOnDebuggerStateChanged(BYTE state, const char *module)
{
	switch (state)
	{
		case DWS_ATTACHEDPROCESS:
		case DWS_CREATEPROCESS:
		{
			if (AbGetSettings()->autoLoadData)
			{
				AbpHasPendingInit = TRUE;
			}
		}
		break;
		case DWS_DETACHEDPROCESS:
		case DWS_EXITEDPROCESS:
		{
			AbReleaseAllSystemResources(false);
		}
		break;
	}
}

void AbpRaiseDbgStateChangeEvent()
{
	DBGPRINT("%s (%s)", AbpCurrentMainModule.name, DBGSTATE_STRINGS[AbpDbgState]);

	AbpOnDebuggerStateChanged(AbpDbgState, AbpCurrentMainModule.name);

	if (AbpDbgState > DWS_ATTACHEDPROCESS)
	{
		//And after the handler callback. 
		//set it to dbg is now idle if current state exited or detached
		AbpDbgState = DWS_IDLE;
	}
}

void AbReleaseAllSystemResources(bool isInShutdown)
{
	DBGPRINT("Releasing used resources. shutdown=%d",isInShutdown);

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

#if 1
	_plugin_menuaddentry(AbMenuHandle, MN_TESTSLOT, "Test??");
#endif
}

#include <structmemmap.h>

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

    if (SmmParseFromFileW(L"E:\\main.abtf", &AbParsedTypeCount))
    {
        DBGPRINT("%d type(s) parsed.", AbParsedTypeCount);
    }


}


#include <util.h>

INTERNAL ApiFunctionInfo *AbiGetAfi(const char *module, const char *afiName);


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
		//Ps. Form object will be deleted automatically when the window closed.
		MainForm *mainForm = new MainForm();
		mainForm->ShowDialog();
	}
	else if (info->hEntry == MN_SHOWSETTINGSFORM)
	{
		SettingsForm *settingsForm = new SettingsForm();
		settingsForm->ShowDialog();
	}
#if 1
	else if (info->hEntry == MN_TESTSLOT)
	{
	}
#endif
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

DBG_LIBEXPORT void CBSYSTEMBREAKPOINT(CBTYPE cbType, PLUG_CB_SYSTEMBREAKPOINT *sysbp)
{
	if (AbpHasPendingInit)
	{
		AbpHasPendingInit = FALSE;
		QueueUserWorkItem((LPTHREAD_START_ROUTINE)Loader, NULL, WT_EXECUTEDEFAULT);
	}
}

PPASSED_PARAMETER_CONTEXT AbpExtractPassedParameterContext(REGDUMP *regdump, PFNSIGN fnSign)
{
    SHORT argCount;
    PPASSED_PARAMETER_CONTEXT ppc;
    CALLCONVENTION conv;
    
    argCount = SmmGetArgumentCount(fnSign);

#ifdef _WIN64
    conv = Fastcall;
#else
    conv = Stdcall;
#endif

    if (!UtlExtractPassedParameters(argCount, conv, regdump, &ppc))
    {
        DBGPRINT("Parameter extraction failed");
        return NULL;
    }

    return ppc;
}


void AbpGhostBreakpointHandler(BpCallbackContext *bpx)
{
    DBGPRINT("BREAKPOINT TRAP");
    bpx->user = (void *)0xDEADBEEF;
}

LONG WINAPI AbpShowOutputArgumentQuestion(LPVOID p)
{
    PFNSIGN fnSign;
    BOOL willContinue = FALSE;
    PPASSED_PARAMETER_CONTEXT ppc=NULL;
    BASIC_INSTRUCTION_INFO instr;
    duint addr;
    char msgBuf[512];

    char *nativeData;

    BpCallbackContext *bpcb = (BpCallbackContext *)p;
    
    sprintf(msgBuf, "One of the parameters of the %s is marked as out. "
        "That Means, you need to execute the api, to get all parameter result correct.\n\n"
        "Want to execute the API now?",bpcb->afi->name);

    willContinue = MessageBoxA(NULL, msgBuf, "Quest", MB_ICONQUESTION | MB_YESNO) == IDYES;

    
    if (willContinue)
    {
        ppc = (PPASSED_PARAMETER_CONTEXT)bpcb->user;

        //Is it alreadly backtraced for caller?
        if (!bpcb->backTrack)
        {
            //if not, we must do that.
            addr = UtlGetCallerAddress(&bpcb->regContext);

            DbgDisasmFastAt(addr, &instr);

            addr += instr.size;

            if (!AbSetInstructionBreakpoint(addr, AbpGhostBreakpointHandler, bpcb,true))
            {
                DBGPRINT("Bpx set failed");
                return EXIT_FAILURE;
            }
        }

        AbDebuggerRun();
        AbDebuggerWaitUntilPaused();

        SmmGetFunctionSignature2(bpcb->afi, &fnSign);

        SmmMapFunctionCall(ppc, fnSign, bpcb->afi);

    }



    return EXIT_SUCCESS;
}

DBG_LIBEXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
	BpCallbackContext *bpcb = NULL;
    PPASSED_PARAMETER_CONTEXT ppc;
    PFNSIGN fnSign = NULL;
    
	bpcb = AbpLookupBpCallback(info->breakpoint->addr);

	if (bpcb != NULL)
	{
		DBGPRINT("Special breakpoint detected. Raising the breakpoint callback");

        //get current register context for current state
        DbgGetRegDump(&bpcb->regContext);

        bpcb->bp = info->breakpoint;

        
        if (bpcb->callback != NULL)
            bpcb->callback(bpcb);

        
        if (AbGetSettings()->mapCallContext)
        {
            if (SmmGetFunctionSignature2(bpcb->afi, &fnSign))
            {
                DBGPRINT("Function mapping signature found. Mapping...");

                ppc = AbpExtractPassedParameterContext(&bpcb->regContext, fnSign);
                
                if (SmmSigHasOutArgument(fnSign))
                {
                    DBGPRINT("%s has an out marked function. ",bpcb->afi->name);
                    bpcb->user = ppc;
                    QueueUserWorkItem((LPTHREAD_START_ROUTINE)AbpShowOutputArgumentQuestion, bpcb, WT_EXECUTELONGFUNCTION);
                }
                else
                    SmmMapFunctionCall(ppc, fnSign, bpcb->afi);
            }
        }
	}
	else
	{
		if (AbpHasPendingInit)
		{
			AbpHasPendingInit = FALSE;
			QueueUserWorkItem((LPTHREAD_START_ROUTINE)Loader, NULL, WT_EXECUTEDEFAULT);
		}
	}
}

DBG_LIBEXPORT void CBCREATEPROCESS(CBTYPE cbType, PLUG_CB_CREATEPROCESS *newProc)
{
	if (AbpDbgState == DWS_ATTACHEDPROCESS)
	{
		//get module info. cuz we cant get the modinfo from in the attach callback 
		goto resumeExec;
	}

	AbpDbgState = DWS_CREATEPROCESS;

resumeExec:

	AbGetDebuggedModuleInfo(&AbpCurrentMainModule);
	AbpNeedsReload = TRUE;

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