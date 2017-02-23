#include <corelib.h>
#include <pluginsdk/_scriptapi_symbol.h>
#include <pluginsdk/_scriptapi_debug.h>
#include <pluginsdk/bridgemain.h>
#include <settings.h>

using namespace Script::Module;
using namespace Script::Symbol;
using namespace Script::Debug;


modlist		AbpModuleList;

INTERNAL bool AbiDetectAPIsUsingByGetProcAddress();
INTERNAL int AbiSearchCallersForAFI(duint codeBase, duint codeSize, ApiFunctionInfo *afi);
INTERNAL ModuleInfo *AbiGetCurrentModuleInfo();
FORWARDED BOOL AbpNeedsReload;

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

	if (!AbpNeedsReload)
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

	AbpNeedsReload = FALSE;

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

bool AbSetBreakpointOnCallers(const char *module, const char *apiFunction, int *bpRefCount)
{
	ModuleSectionInfo *msiList;
	ApiFunctionInfo *afi;
	int setBpCount = 0,sectCount;
	int lastCallIndex = 0;
	
	sectCount = AbiGetMainModuleCodeSections(&msiList);

	if (!sectCount)
	{
		DBGPRINT("cant get code section.");
		return false;
	}

	afi = AbiGetAfi(module, apiFunction);

	if (!afi)
	{
		DBGPRINT("'%s' not found", apiFunction);
		return false;
	}

	if (afi->callInfo.callCount > 0)
		return true;

	for (int i = 0;i < sectCount;i++)
	{
		lastCallIndex = afi->callInfo.callCount;

		if (AbiSearchCallersForAFI(msiList[i].addr, msiList[i].size, afi) > 0)
		{
			for (int i = lastCallIndex;i < afi->callInfo.callCount;i++)
			{
				if (SetBreakpoint(afi->callInfo.calls[i]))
					setBpCount++;
			}
		}
	}


	if (afi->callInfo.callCount - setBpCount > 0)
		DBGPRINT("%d of %d bp not set", setBpCount, afi->callInfo.callCount);

	if (bpRefCount != NULL)
		*bpRefCount = setBpCount;

	return setBpCount > 0;
}

bool AbSetBreakpoint(const char *module, const char *apiFunction, duint *funcAddr)
{
	return AbSetBreakpointEx(module, apiFunction, funcAddr, NULL,NULL);
}

bool AbSetBreakpointEx(const char *module, const char *apiFunction, duint *funcAddr, AB_BREAKPOINT_CALLBACK bpCallback, void *user)
{
	bool bpSet;
	ApiFunctionInfo *afi = NULL;
	BpCallbackContext *cbctx = NULL;
	duint bpAddr = 0;

	afi = AbiGetAfi(module, apiFunction);

	if (!afi)
		return false;

	bpAddr = afi->ownerModule->baseAddr + afi->rva;

	if (bpCallback != NULL)
	{
		cbctx = ALLOCOBJECT(BpCallbackContext);

		if (!cbctx)
			return false;

		cbctx->bpAddr = bpAddr;
		cbctx->callback = bpCallback;
		cbctx->user = user;

		if (!AbRegisterBpCallback(cbctx))
		{
			FREEOBJECT(cbctx);
			return false;
		}
	}

	bpSet = SetBreakpoint(bpAddr);

	if (!bpSet)
	{
		AbDeregisterBpCallback(cbctx);
		FREEOBJECT(cbctx);
		return false;
	}

	if (bpSet && funcAddr != NULL)
		*funcAddr = afi->ownerModule->baseAddr + afi->rva;

	return bpSet;
}

