#include <corelib.h>
#include <settings.h>
#include <dlgs/MainForm.hpp>
#include <dlgs/SettingsForm.hpp>
#include <structmemmap.h>


#define MN_BASE                 0xFA00FA

#define MN_ABOUT                MN_BASE
#define MN_SHOWMAINFORM         MN_BASE + 1
#define MN_SHOWSETTINGSFORM     MN_BASE + 2
#define MN_TESTSLOT             MN_BASE + 3

#define DWS_IDLE                0
#define DWS_CREATEPROCESS       1
#define DWS_ATTACHEDPROCESS     2
#define DWS_EXITEDPROCESS       3
#define DWS_DETACHEDPROCESS     4

const char *DBGSTATE_STRINGS[5] =
{
    "Idle",
    "Process Created",
    "Process Attached",
    "Process Exited",
    "Process Detached"
};


BYTE                                AbpDbgState;
Script::Module::ModuleInfo          AbpCurrentMainModule;
BOOL                                AbfNeedsReload=FALSE;

INTERNAL int                        AbPluginHandle;
INTERNAL HWND                       AbHwndDlgHandle;
INTERNAL int                        AbMenuHandle;
INTERNAL int                        AbMenuDisasmHandle;
INTERNAL int                        AbMenuDumpHandle;
INTERNAL int                        AbMenuStackHandle;
INTERNAL HMODULE                    AbPluginModule;

INTERNAL WORD                       AbParsedTypeCount = 0;

INTERNAL void AbiRaiseDeferredLoader(const char *dllName, duint base);

INTERNAL void AbiInitDynapi();
INTERNAL void AbiUninitDynapi();
INTERNAL void AbiReleaseDeferredResources();
INTERNAL void AbiEmptyInstructionCache();

BOOL                                AbpHasPendingInit = FALSE;

INTERNAL_EXPORT Script::Module::ModuleInfo *AbiGetCurrentModuleInfo()
{
    return &AbpCurrentMainModule;
}



int WINAPI Loader(void *)
{
    AbLoadAvailableModuleAPIs(true);
    return 0;
}

void AbpParseScripts()
{
    char *tokCtx;
    char *scripts = NULL;
    char *line;
    LPSTR errStr;

    if (!AbGetSettings()->mainScripts)
        return;

    scripts = HlpCloneStringA(AbGetSettings()->mainScripts);

    if (!scripts)
    {
        DBGPRINT("memory error");
        return;
    }

    line = strtok_s(scripts, "\r\n;", &tokCtx);

    while (line)
    {
        DBGPRINT("Parsing '%s'", line);
        SmmParseFromFileA(line, &AbParsedTypeCount);
        line = strtok_s(NULL, "\r\n;", &tokCtx);
    }

    if (SmmHasParseError(&errStr))
    {
        MessageBoxA(NULL, errStr, "x64dbg Api Break - script parse error", MB_ICONERROR);
        FREESTRING(errStr);
    }

    DBGPRINT("%d type(s) parsed.", AbParsedTypeCount);

    FREESTRING(line);
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
    AbpReleaseBreakpointResources();
    AbReleaseModuleResources();

    if (isInShutdown)
    {
        AbiUninitDynapi();
        UiForceCloseAllActiveWindows();
    }
}

BOOL AbRaiseSystemError(const char *errorDesc, int errCode,const char *func, const int line)
{
    LPSTR msg;
    BOOL dbgBreak;

    HlpPrintFormatBufferA(&msg,
        "An error occurred at %s:%d;\r\n\r\n%s\r\nDo you want to break into debugger?",
        func,line,errorDesc);

    dbgBreak = MessageBoxA(NULL, msg, "Error", MB_ICONERROR | MB_YESNO) == IDYES;

    FREESTRING(msg);

    if (dbgBreak)
    {
        __debugbreak();
        return TRUE;
    }

    return FALSE;
}

typedef bool(*pfnDbgGetRegDumpEx)(REGDUMP* regdump, size_t size);
HMODULE g_DbgLib = NULL;

pfnDbgGetRegDumpEx _DbgGetRegDumpEx = NULL;

#define VALTOSTRFY(val) _stringfy(val)

#define BRIDGE_DLL "x" VALTOSTRFY(PLATFORM_SIZE) "bridge.dll"

bool AbpNeedsNewerx64Dbg()
{
	bool needsNew = false;

	g_DbgLib = GetModuleHandleA(BRIDGE_DLL);

	if (!g_DbgLib)
	{
		RAISEGLOBALERROR("dbglib error");
		return false;
	}

	if (!(_DbgGetRegDumpEx = (pfnDbgGetRegDumpEx)GetProcAddress(g_DbgLib, "DbgGetRegDumpEx")))
		needsNew = true;

	return needsNew;
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

	if (AbpNeedsNewerx64Dbg())
	{
		MessageBoxA(NULL,
			"this version of plugin uses new x64dbg API. Consider update x64dbg to the new version.",
			"warning",
			MB_OK | MB_ICONWARNING);

		return false;
	}

    AbSettingsLoad();
    AbiInitDynapi();

    LoadLibraryA("Riched20.dll");

    return true;
}

DBG_LIBEXPORT bool plugstop()
{
    HMODULE hmod;

    AbReleaseAllSystemResources(true);

    hmod = GetModuleHandleA("Riched20.dll");

    if (hmod)
        FreeLibrary(hmod);

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

    AbpParseScripts();
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

PPASSED_PARAMETER_CONTEXT AbpExtractPassedParameterContext(REGDUMP *regdump, PFNSIGN fnSign, BOOL ipOnStack)
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

    if (!UtlExtractPassedParameters(argCount, conv, regdump,ipOnStack, &ppc))
    {
        DBGPRINT("Parameter extraction failed");
        return NULL;
    }

    return ppc;
}


void AbpGhostBreakpointHandler(BpCallbackContext *bpx)
{
}

#include <dlgs/ApiCallMapForm.hpp>

void AbpWaitUntilHandlerDone(PBREAKPOINT_INFO pbi)
{
    while (pbi->status & BPS_EXECUTING_HANDLER)
        _mm_pause();

    //Give a bit time to the handler to make sure it returned
    SleepEx(10, FALSE);
}

LONG WINAPI AbpShowOutputArgumentQuestion(LPVOID p)
{
    PFNSIGN fnSign;
    BOOL willContinue = FALSE;
    PPASSED_PARAMETER_CONTEXT ppc=NULL;
    BASIC_INSTRUCTION_INFO instr;
    duint addr;
    char msgBuf[512];
    LPSTR mapResult;

    BpCallbackContext *bpcb = (BpCallbackContext *)p;
    
    sprintf(msgBuf, "One of the parameters of the %s is marked as out. "
        "That Means, you need to execute the api, to get all parameter result correctly.\n\n"
        "Want to execute the API now?",bpcb->afi->name);

    willContinue = MessageBoxA(NULL, msgBuf, "Quest", MB_ICONQUESTION | MB_YESNO) == IDYES;

    if (willContinue)
    {
        ppc = (PPASSED_PARAMETER_CONTEXT)bpcb->user;

        //Is it alreadly backtraced for caller?
        if (!(bpcb->ownerBreakpoint->options & BPO_BACKTRACK))
        {
            //if not, we must do that.
            addr = UtlGetCallerAddress(&bpcb->regContext);
        }
        else
            addr = bpcb->regContext.regcontext.cip;

        DbgDisasmFastAt(addr, &instr);

        addr += instr.size;

        if (!AbSetInstructionBreakpoint(addr, AbpGhostBreakpointHandler, bpcb, true))
        {
            DBGPRINT("Bpx set failed");
            return EXIT_FAILURE;
        }

        AbDebuggerRun();
        AbDebuggerWaitUntilPaused();
        AbpWaitUntilHandlerDone(bpcb->ownerBreakpoint);

        SmmGetFunctionSignature2(bpcb->afi, &fnSign);

        SmmMapFunctionCall(ppc, fnSign, bpcb->afi, &mapResult);

        ApiCallMapForm *acmf = new ApiCallMapForm(mapResult);
        acmf->ShowDialog();

    }



    return EXIT_SUCCESS;
}

DBG_LIBEXPORT void CBBREAKPOINT(CBTYPE cbType, PLUG_CB_BREAKPOINT* info)
{
    BpCallbackContext *bpcb = NULL;
    PBREAKPOINT_INFO pbi;
    BOOL isBacktrackBp;
    PPASSED_PARAMETER_CONTEXT ppc;
    PFNSIGN fnSign = NULL;

    pbi = AbpLookupBreakpoint(info->breakpoint->addr);

    
    if (pbi != NULL)
    {
        DBGPRINT("Special breakpoint detected.");
        pbi->status |= BPS_EXECUTING_HANDLER;

        bpcb = pbi->cbctx;

        pbi->hitCount++;

        if (bpcb != NULL)
        {
            DBGPRINT("Breakpoint has registered callback. Raising the breakpoint callback");


            //get current register context for current state
            _DbgGetRegDumpEx(&bpcb->regContext,sizeof(REGDUMP));

            bpcb->bp = info->breakpoint;

            if (bpcb->callback != NULL)
            {
                pbi->status |= BPS_EXECUTING_CALLBACK;
                bpcb->callback(bpcb);
                pbi->status &= ~BPS_EXECUTING_CALLBACK;
            }
        }

        if (AbGetSettings()->mapCallContext)
        {
            if (SmmGetFunctionSignature2(bpcb->afi, &fnSign))
            {
                DBGPRINT("Function mapping signature found. Mapping...");

                isBacktrackBp = (pbi->options & BPO_BACKTRACK) == BPO_BACKTRACK;

                ppc = AbpExtractPassedParameterContext(&bpcb->regContext, fnSign,!isBacktrackBp);

                if (SmmSigHasOutArgument(fnSign))
                {
                    DBGPRINT("%s has an out marked function. ", bpcb->afi->name);
                    bpcb->user = ppc;
                    QueueUserWorkItem((LPTHREAD_START_ROUTINE)AbpShowOutputArgumentQuestion, bpcb, WT_EXECUTELONGFUNCTION);
                }
                else
                    SmmMapFunctionCall(ppc, fnSign, bpcb->afi,NULL); //TODO: URGENT: FIX PARAMETER
            }
        }

        if (pbi->options & BPO_SINGLESHOT)
            AbDeleteBreakpoint(pbi->addr);


        pbi->status &= ~BPS_EXECUTING_HANDLER;

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
    AbfNeedsReload = TRUE;

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