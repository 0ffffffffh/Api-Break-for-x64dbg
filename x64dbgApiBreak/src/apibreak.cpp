#include <corelib.h>
#include <pluginsdk/_scriptapi_symbol.h>
#include <pluginsdk/_scriptapi_debug.h>
#include <pluginsdk/bridgemain.h>
#include <settings.h>
#include <structmemmap.h>

using namespace Script::Module;
using namespace Script::Symbol;
using namespace Script::Debug;
using namespace Script::Register;

modlist                                         AbpModuleList;
unordered_map<duint, PBREAKPOINT_INFO>          AbpBreakpointList;

INTERNAL bool                                   AbiDetectAPIsUsingByGetProcAddress();
INTERNAL int                                    AbiSearchCallersForAFI(duint codeBase, duint codeSize, ApiFunctionInfo *afi);
INTERNAL ModuleInfo *                           AbiGetCurrentModuleInfo();

FORWARDED BOOL                                  AbfNeedsReload;

void AbpReleaseBreakpointResources()
{
    unordered_map<duint, PBREAKPOINT_INFO>::iterator it;

    if (AbpBreakpointList.size() == 0)
        return;

    while (AbpBreakpointList.size() > 0)
    {
        it = AbpBreakpointList.begin();

        if (it->second->cbctx != NULL)
            FREEOBJECT(it->second);

        FREEOBJECT(it->second);
        
        AbpBreakpointList.erase(it);
    }
}

PBREAKPOINT_INFO AbpLookupBreakpoint(duint addr)
{
    unordered_map<duint, PBREAKPOINT_INFO>::iterator iter;

    iter = AbpBreakpointList.find(addr);

    if (iter == AbpBreakpointList.end())
        return FALSE;

    return iter->second;
}

bool AbpRegisterBreakpoint(duint addr, DWORD options, BpCallbackContext *cbctx)
{
    PBREAKPOINT_INFO pbi;

    if (AbpBreakpointList.find(addr) != AbpBreakpointList.end())
        return false;

    pbi = ALLOCOBJECT(BREAKPOINT_INFO);

    if (!pbi)
        return false;

    pbi->addr = addr;
    pbi->options = options;
    pbi->hitCount = 0;
    pbi->cbctx = cbctx;

    AbpBreakpointList.insert({ addr,pbi });

    return true;
}

bool AbpDeregisterBreakpoint(duint addr)
{
    unordered_map<duint, PBREAKPOINT_INFO>::iterator iter;

    iter = AbpBreakpointList.find(addr);

    if (iter == AbpBreakpointList.end())
        return false;

    if (iter->second->cbctx != NULL)
        FREEOBJECT(iter->second->cbctx);

    FREEOBJECT(iter->second);

    AbpBreakpointList.erase(iter);
    return true;
}


ModuleApiInfo *AbpSearchModuleApiInfo(const char *name)
{
    for (modlist::iterator n = AbpModuleList.begin(); n != AbpModuleList.end(); n++)
    {
        if (!strcmp((*n)->name, name))
            return *n;
    }

    return NULL;
}

ApiFunctionInfo *AbpSearchApiFunctionInfo(ModuleApiInfo *moduleInfo, const char *func)
{
    apilist::iterator iter;
    string sfunc(func);

    if (!moduleInfo)
        return NULL;


    iter = moduleInfo->apiList->find(sfunc);

    if (iter != moduleInfo->apiList->end())
        return iter->second;

    return NULL;
}


void AbpLinkApiExportsToModule(ListInfo *moduleList)
{
    ModuleInfo *module = NULL; 

    for (modlist::iterator n = AbpModuleList.begin(); n != AbpModuleList.end(); n++)
    {
        module = static_cast<ModuleInfo *>(moduleList->data);

        for (int i = 0;i < moduleList->count;i++)
        {
            if (!strcmp(module->name, (*n)->name))
            {
                (*n)->baseAddr = module->base;
                break;
            }

            module++;
        }

        
    }
}

bool AbpDeregisterModule(ModuleApiInfo *mai)
{
    apilist::iterator apit;
    modlist::iterator modit;
    
    if (!mai)
        return false;

    
    for (apit = mai->apiList->begin(); apit != mai->apiList->end(); apit++)
    {
        if (apit->second->callInfo.calls)
            FREEOBJECT(apit->second->callInfo.calls);

        FREEOBJECT(apit->second);
    }
    
    mai->apiList->clear();

    delete mai->apiList;

    for (modit = AbpModuleList.begin(); modit != AbpModuleList.end(); modit++)
    {
        if ((*modit) == mai)
            break;
    }

    
    AbpModuleList.erase(modit);
    
    FREEOBJECT(mai);

    return true;
}

bool AbpRegisterApi(SymbolInfo *sym, ApiFunctionInfo **pafi)
{
    ModuleApiInfo *mai = NULL;
    ApiFunctionInfo *afi = NULL;

    CharLowerA(sym->mod);

    mai = AbpSearchModuleApiInfo(sym->mod);

    if (!mai)
    {
        mai = ALLOCOBJECT(ModuleApiInfo);

        if (!mai)
            return false;

        strcpy(mai->name, sym->mod);
        mai->baseAddr = 0;
        mai->apiList = new apilist();
        AbpModuleList.push_back(mai);
    }


    
    afi = ALLOCOBJECT(ApiFunctionInfo);

    if (!afi)
        return false;

    strcpy(afi->name, sym->name);
    afi->rva = sym->rva;
    
    afi->ownerModule = mai;

    mai->apiList->insert({ string(sym->name), afi });

    mai->listCount++;

    if (pafi)
        *pafi = afi;

    return true;
}

INTERNAL_EXPORT ApiFunctionInfo *AbiGetAfi(const char *module, const char *afiName)
{
    ModuleApiInfo *mai = NULL;
    ApiFunctionInfo *afi = NULL;

    mai = AbpSearchModuleApiInfo(module);

    if (!mai)
        return NULL;

    return AbpSearchApiFunctionInfo(mai, afiName);
}

bool AbpNeedsReloadModuleAPIs()
{
    ModuleInfo mod;

    if (!AbGetDebuggedModuleInfo(&mod))
        return false;

    return strcmp(mod.name, AbiGetCurrentModuleInfo()->name) != 0;
}

duint AbpGetPEDataOfMainModule2(ModuleInfo *mi, duint type, int sectIndex)
{
    return (duint)GetPE32Data(mi->path, sectIndex, type);
}

duint AbpGetPEDataOfMainModule(duint type, int sectIndex)
{
    ModuleInfo mainModule;
    
    if (!AbGetDebuggedModuleInfo(&mainModule))
        return 0;

    return AbpGetPEDataOfMainModule2(&mainModule, type, sectIndex);
}

INTERNAL_EXPORT bool AbiRegisterDynamicApi(const char *module, const char *api, duint mod, duint apiAddr, duint apiRva)
{
    SymbolInfo sym;
    ApiFunctionInfo *afi;

    DBGPRINT("Registering dynaload api %s(%p) : %s(%p)", module, mod, api, apiAddr);
    
    
    memset(&sym, 0, sizeof(SymbolInfo));
    strcpy(sym.mod, module);
    strcpy(sym.name, api);
    sym.rva = apiRva;
    sym.type = Import;

    if (AbpRegisterApi(&sym, &afi))
    {
        DBGPRINT("registered!");
        
        if (afi->ownerModule->baseAddr == 0)
            afi->ownerModule->baseAddr = mod;

        return true;
    }

    return false;
}

INTERNAL_EXPORT int AbiGetMainModuleCodeSections(ModuleSectionInfo **msi)
{
    ModuleInfo mainModule;
    DWORD flags;
    ModuleSectionInfo *sectList = NULL;
    int sectCount = 0;

    *msi = NULL;

    if (!AbGetDebuggedModuleInfo(&mainModule))
        return 0;

    for (int i = 0;i < mainModule.sectionCount;i++)
    {
        flags = AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONFLAGS, i);

        if (flags & IMAGE_SCN_CNT_CODE)
        {
            sectCount++;
            sectList = RESIZEOBJECTLIST(ModuleSectionInfo, sectList, sectCount);
            sectList[sectCount-1].addr = mainModule.base + AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONVIRTUALOFFSET, i);
            sectList[sectCount-1].size = AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONVIRTUALSIZE, i);
        }
    }

    *msi = sectList;

    return sectCount;
}

void AbDebuggerRun()
{
    Script::Debug::Run();
}

void AbDebuggerPause()
{
    Script::Debug::Pause();
}

void AbDebuggerWaitUntilPaused()
{
    _plugin_waituntilpaused();
}


bool AbCmdExecFormat(const char *format, ...)
{
    bool success = false;
    char *buffer;
    va_list va;

    va_start(va, format);

    if (HlpPrintFormatBufferExA(&buffer, format, va) > 0)
    {
        success = DbgCmdExec(buffer);

        FREESTRING(buffer);
    }

    va_end(va);

    return success;
}

bool AbGetDebuggedImageName(char *buffer)
{
    ModuleInfo mod;

    if (!AbGetDebuggedModuleInfo(&mod))
        return false;

    strcpy(buffer, mod.name);
    return true;
}

bool AbGetDebuggedModuleInfo(ModuleInfo *modInfo)
{
    duint mainModAddr;

    mainModAddr = AbGetDebuggedImageBase();

    if (!mainModAddr)
        return false;

    return InfoFromAddr(mainModAddr, modInfo);
}

bool AbGetDebuggedModulePath(char *pathBuf, int bufLen)
{
    ModuleInfo mod;
    duint dn;

    if (!pathBuf)
        return false;

    if (!AbGetDebuggedModuleInfo(&mod))
        return false;
    
    dn = strlen(mod.path);
    
    if (dn > bufLen)
        return false;

    strcpy(pathBuf, mod.path);
    
    while (pathBuf[--dn] != '\\')
        pathBuf[dn] = 0;

    return true;
}

duint AbGetDebuggedImageBase()
{
    duint base = GetMainModuleBase();

    if (base)
        return base;

    base = (duint)GetDebuggedFileBaseAddress();

    if (!base)
        base = (duint)GetDebuggedDLLBaseAddress();

    return base;
}

bool AbHasDebuggingProcess()
{
    ModuleInfo mi;

    if (DbgIsDebugging())
    {
        return AbGetDebuggedModuleInfo(&mi);
    }

    return false;
}

void AbReleaseModuleResources()
{
    while (AbpModuleList.size() > 0)
    {
        AbpDeregisterModule((*AbpModuleList.begin()));
    }
}

bool AbLoadAvailableModuleAPIs(bool onlyImportsByExe)
{
    ListInfo moduleList = { 0 };
    ListInfo functionSymbolList = { 0 };

    bool modListOk = false, symListOk = false;
    bool success = false;

    SymbolInfo *sym = NULL;
    ModuleApiInfo *mai = NULL;
    ApiFunctionInfo *afi = NULL;

    if (!AbfNeedsReload)
        return true;

    //First, detect dynamically loaded apis. 
    //And mark the loaded api export as an imported by exe

    if (Script::Module::GetList(&moduleList))
    {
        if (moduleList.data != NULL)
            modListOk = true;
    }

    if (Script::Symbol::GetList(&functionSymbolList))
    {
        if (functionSymbolList.data != NULL)
            symListOk = true;
    }


    if (!modListOk || !symListOk)
        goto cleanAndExit;

    sym = static_cast<SymbolInfo *>(functionSymbolList.data);
    
    for (int i = 0;i < functionSymbolList.count;i++)
    {
        if (onlyImportsByExe)
        {
            if (sym->type == Import && HlpEndsWithA(sym->mod, ".exe",FALSE, 4))
            {
                //Executables provides psoude module import data
                //for now we reserve api registration slot
                if (AbpRegisterApi(sym, &afi))
                    mai = afi->ownerModule;
                
            }
            else if (sym->type == Export && !HlpEndsWithA(sym->mod,".exe",FALSE, 4))
            {
                //Ok. we walkin on the real export modules now.
                //try to get AFI if there is exist a reserved for psoude import data
                afi = AbpSearchApiFunctionInfo(mai, sym->name);

                //If exist make a real registration for exist slot
                if (afi != NULL)
                    AbpRegisterApi(sym, NULL);
            }
        }
        else
        {
            if (sym->type == Export && !HlpEndsWithA(sym->mod, ".exe", FALSE, 4))
            {
                AbpRegisterApi(sym,NULL);
            }
        }

        sym++;
    }

    if (AbpModuleList.size() == 0 )
    {
        MessageBoxA(AbHwndDlgHandle, "The ApiBreak could not load any imports from the being debugged image.\r\n"
            "Because, imports are treated as export or could not load imports correctly by the x64dbg. (Its a x64dbg Bug)\r\n"
            "Please update your x64dbg to latest version.",
            "WARNING",
            MB_OK | MB_ICONWARNING);

        success = false;
        goto cleanAndExit;
    }

    if (onlyImportsByExe)
        AbpDeregisterModule(mai);

    AbpLinkApiExportsToModule(&moduleList);

    success = true;

    DBGPRINT("%d module found.", AbpModuleList.size());
    
    if (AbGetSettings()->exposeDynamicApiLoads)
        AbiDetectAPIsUsingByGetProcAddress();
    else
        DBGPRINT("dynamic api detection disabled!");

    AbfNeedsReload = FALSE;

cleanAndExit:

    if (symListOk)
        BridgeFree(functionSymbolList.data);

    if (modListOk)
        BridgeFree(moduleList.data);

    return success;
}

int AbEnumModuleNames(APIMODULE_ENUM_PROC enumProc, void *user)
{
    ModuleApiInfo *mai;
    
    for (modlist::iterator n = AbpModuleList.begin(); n != AbpModuleList.end(); n++)
    {
        mai = *n;
        enumProc(mai->name, user);
    }

    return (int)AbpModuleList.size();
}

void AbEnumApiFunctionNames(APIMODULE_ENUM_PROC enumProc, const char *moduleName, void *user)
{
    ModuleApiInfo *mai = NULL;
    
    mai = AbpSearchModuleApiInfo(moduleName);

    if (!mai)
        return;

    
    for (apilist::iterator n = mai->apiList->begin(); n != mai->apiList->end(); n++)
        enumProc(n->second->name, user);
}

bool AbpReturnToCaller(duint callerIp, duint csp)
{
    RegisterEnum cspReg, cipReg;

#ifdef _WIN64
    cspReg = RegisterEnum::RSP;
    cipReg = RegisterEnum::RIP;
#else
    cspReg = RegisterEnum::ESP;
    cipReg = RegisterEnum::EIP;
#endif

    //pop stack
    if (!Set(cspReg, csp + sizeof(duint)))
        return false;

    //set the previous caller address to the Instruction pointer
    return Set(cipReg, callerIp);
}

void AbpCallback0(__BpCallbackContext *bpx)
{
    return;
}

void AbpBacktrackingBreakpointCallback(__BpCallbackContext *bpx)
{
    REGDUMP regContext;
    duint callerIp;

    switch (bpx->bp->type)
    {
    case bp_normal:
    case bp_memory:
    case bp_dll:
        break;
    default:
        return;
    }

    callerIp = UtlGetCallerAddress(&regContext);

    if (callerIp > 0)
    {   
        if (AbpReturnToCaller(callerIp, regContext.regcontext.csp))
        {
            bpx->regContext.regcontext.csp += sizeof(duint);
            bpx->regContext.regcontext.cip = callerIp;
        }

        AbCmdExecFormat("disasm %p", callerIp);
    }

}

bool AbSetAPIBreakpointOnCallers(const char *module, const char *apiFunction)
{
    duint addr;

    return AbSetBreakpointEx(
            module, 
            apiFunction, 
            &addr, 
            BPO_BACKTRACK,
            (AB_BREAKPOINT_CALLBACK)AbpBacktrackingBreakpointCallback, 
            NULL
        );

}

bool AbSetAPIBreakpoint(const char *module, const char *apiFunction, duint *funcAddr)
{
    return AbSetBreakpointEx(module, apiFunction, funcAddr, BPO_NONE, (AB_BREAKPOINT_CALLBACK)AbpCallback0,NULL);
}

bool AbSetInstructionBreakpoint(duint instrAddr, AB_BREAKPOINT_CALLBACK callback, void *user, bool singleShot)
{
    DWORD opt = BPO_NONE;

    if (singleShot)
        opt |= BPO_SINGLESHOT;

    return AbSetBreakpointEx(NULL, NULL, &instrAddr, opt, callback, user);
}


bool AbSetBreakpointEx(const char *module, const char *apiFunction, duint *funcAddr, DWORD bpo, AB_BREAKPOINT_CALLBACK bpCallback, void *user)
{
    bool bpSet;
    ApiFunctionInfo *afi = NULL;
    BpCallbackContext *cbctx = NULL;
    duint bpAddr = 0;
    bool isNonApiBp;

    isNonApiBp = module == NULL && apiFunction == NULL;

    if (!isNonApiBp)
    {
        afi = AbiGetAfi(module, apiFunction);

        if (!afi)
            return false;

        bpAddr = afi->ownerModule->baseAddr + afi->rva;
    }
    else
    {
        if (*funcAddr == NULL)
            return false;

        bpAddr = *funcAddr;
    }

    if (bpCallback != NULL)
    {
        cbctx = ALLOCOBJECT(BpCallbackContext);

        if (!cbctx)
            return false;

        cbctx->bpAddr = bpAddr;
        cbctx->callback = bpCallback;
        cbctx->afi = afi;
        cbctx->user = user;
    }

    if (bpo & BPO_SINGLESHOT)
        bpSet = AbCmdExecFormat("bp %p, abss, ss", bpAddr);
    else
        bpSet = SetBreakpoint(bpAddr);

    
    if (!bpSet)
    {
        FREEOBJECT(cbctx);
        return false;
    }

    if (!AbpRegisterBreakpoint(bpAddr, bpo, cbctx))
    {
        FREEOBJECT(cbctx);
    }

    if (!isNonApiBp)
    {
        if (bpSet && funcAddr != NULL)
            *funcAddr = afi->ownerModule->baseAddr + afi->rva;
    }

    return bpSet;
}

bool AbDeleteBreakpoint(duint addr)
{
    PBREAKPOINT_INFO pbi;
    bool deleteOk;

    pbi = AbpLookupBreakpoint(addr);

    if (!pbi)
        return false;

    deleteOk = DeleteBreakpoint(addr);

    if (deleteOk)
    {
        AbpDeregisterBreakpoint(addr);

        if (pbi->cbctx != NULL)
            FREEOBJECT(pbi->cbctx);

        FREEOBJECT(pbi);
    }

    return deleteOk;
}