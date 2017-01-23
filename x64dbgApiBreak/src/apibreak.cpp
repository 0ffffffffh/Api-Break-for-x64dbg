#include <corelib.h>
#include <pluginsdk/_scriptapi_module.h>
#include <pluginsdk/_scriptapi_symbol.h>
#include <pluginsdk/_scriptapi_debug.h>
#include <pluginsdk/bridgemain.h>

using namespace Script::Module;
using namespace Script::Symbol;
using namespace Script::Debug;


modlist		AbpModuleList;
ModuleInfo	AbpCurrentMainModule;


INTERNAL int AbiSearchCallersForAFI(duint codeBase, duint codeSize, ApiFunctionInfo *afi);

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

	if (!GetMainModuleInfo(&mod))
		return false;

	return strcmp(mod.name, AbpCurrentMainModule.name) != 0;
}

duint AbpGetPEDataOfMainModule2(ModuleInfo *mi, duint type, int sectIndex)
{
	return (duint)GetPE32Data(mi->path, sectIndex, type);
}

duint AbpGetPEDataOfMainModule(duint type, int sectIndex)
{
	ModuleInfo mainModule;
	
	if (!GetMainModuleInfo(&mainModule))
		return 0;

	return AbpGetPEDataOfMainModule2(&mainModule, type, sectIndex);
}

INTERNAL_EXPORT bool AbiGetMainModuleCodeSection(ModuleSectionInfo *msi)
{
	ModuleInfo mainModule;
	DWORD flags;

	if (!GetMainModuleInfo(&mainModule))
		return 0;

	for (int i = 0;i < mainModule.sectionCount;i++)
	{
		flags = AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONFLAGS, i);

		if (flags | IMAGE_SCN_CNT_CODE)
		{
			msi->addr = mainModule.base + AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONVIRTUALOFFSET, i);
			msi->size = AbpGetPEDataOfMainModule2(&mainModule, UE_SECTIONVIRTUALSIZE, i);
			return true;
		}

	}

	return false;
}


bool AbHasDebuggingProcess()
{
	return GetMainModuleBase() > 0;
}

void AbReleaseResources()
{
	while (AbpModuleList.size() > 0)
	{
		AbpDeregisterModule((*AbpModuleList.begin()));
	}
}

void AbGetDebuggingProcessName(char *buffer)
{
	GetMainModuleName(buffer);
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

	if (!AbpNeedsReloadModuleAPIs())
		return true;

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
			if (sym->type == Import && HlpEndsWithA(sym->mod, ".exe", 4))
			{
				if (AbpRegisterApi(sym, &afi))
					mai = afi->ownerModule;
				
			}
			else if (sym->type == Export && !HlpEndsWithA(sym->mod,".exe",4))
			{
				afi = AbpSearchApiFunctionInfo(mai, sym->name);

				if (afi != NULL)
					AbpRegisterApi(sym, NULL);
			}
		}
		else
		{
			if (sym->type == Export && !HlpEndsWithA(sym->mod, ".exe", 4))
			{
				AbpRegisterApi(sym,NULL);
			}
		}

		sym++;
	}

	if (onlyImportsByExe)
		AbpDeregisterModule(mai);

	AbpLinkApiExportsToModule(&moduleList);

	success = true;

	GetMainModuleInfo(&AbpCurrentMainModule);

	//AbpDetectAPIsUsingByGetProcAddress();

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
	ModuleSectionInfo msi;
	ApiFunctionInfo *afi;
	int setBpCount = 0;

	if (!AbiGetMainModuleCodeSection(&msi))
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

	if (afi->callCount > 0)
		return true;

	if (AbiSearchCallersForAFI(msi.addr, msi.size, afi) > 0)
	{
		for (int i = 0;i < afi->callCount;i++)
		{
			if (SetBreakpoint(afi->calls[i]))
				setBpCount++;
		}

		if (afi->callCount - setBpCount > 0)
			DBGPRINT("%d of %d bp not set", setBpCount, afi->callCount);
	}
	else
		return false;

	if (bpRefCount != NULL)
		*bpRefCount = setBpCount;

	return true;
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

