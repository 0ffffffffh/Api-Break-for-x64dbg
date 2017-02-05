#include <corelib.h>
#include <settings.h>
#include <unordered_map>
#include <pluginsdk/_scriptapi_module.h>
#include <pluginsdk/_scriptapi_misc.h>

using namespace Script::Module;
using namespace Script::Misc;
using namespace std;

INTERNAL ApiFunctionInfo *AbiGetAfi(const char *module, const char *afiName);
INTERNAL bool AbiGetMainModuleCodeSection(ModuleSectionInfo *msi);
INTERNAL bool AbiRegisterDynamicApi(const char *module, const char *api, duint mod, duint apiAddr, duint apiRva);

#define ALIGNTO_PAGE(x) ( ((x)/0x1000+1) * 0x1000 )


typedef  unordered_map<string, vector<char *> *> ondemand_api_list;

HANDLE				AbpDeferListSyncMutant;
ondemand_api_list	AbpDeferList;

#define SYNCH_BEGIN		WaitForSingleObject(AbpDeferListSyncMutant,INFINITE)
#define SYNCH_END		ReleaseMutex(AbpDeferListSyncMutant)


/*
On x64 calling convention (fastcall)
parameters are passed in order left to right
the first four parameters are passed in order
RCX, RDX, R8, and R9

GetProcAddress call on Amd64

MOV RCX, MODULEHANDLE
LEA RDX, HARDCODED_API_STRING
CALL GetProcAddress

*/

/*
On x86 architecture, the WIN32 apis uses the stdcall convention
So in this convention, parameters are passed in order right to left
and they are passed (pushed) through the stack

GetProcAddress call on x86

PUSH MODULEHANDLE
PUSH HARDCODED_API_STRING
CALL GetProcAddress

or they may use the stack pointer to passing arguments
instead of direct stack pushing

SUB ESP, 8
MOV [ESP+4], HARDCODED_API_STRING
MOV [ESP],MODULEHANDLE
CALL GetProcAddress

*/

typedef struct
{
	unsigned char					size;
	unsigned char					op;
	unsigned char					reg;
	unsigned char					memstore;
	unsigned short					disp;
	duint							imm;
}LoadInstInfo;

typedef struct
{
	BASIC_INSTRUCTION_INFO			inst;
	duint							addr;
}CachedInst;

#define OP_MOV						1
#define OP_LEA						2
#define OP_PUSH						3

#define ORD_OP						0
#define ORD_DREG					1
#define ORD_DONE					2

#define ABP_CACHED_DISASM_SIZE		10

#define GET_REGSIZE(pfx)			(pfx == 'r' ? 64 : pfx == 'e' ? 32 : 8)

typedef enum
{
	RegNone=-1,
	Ax,
	Bx,
	Cx,
	Dx,
	Si,
	Di,
	Bp,
	Sp,
	r8,
	r9,
	r10,
	r11,
	r12,
	r13,
	r14,
	r15
}RegId;

const char *regs[16]
{
	"*ax",
	"*bx",
	"*cx",
	"*dx",
	"*si",
	"*di",
	"*bp",
	"*sp",
	"r8",
	"r9",
	"r10",
	"r11",
	"r12",
	"r13",
	"r14",
	"r15"
};

CachedInst	AbpCachedDisasmedInstructions[ABP_CACHED_DISASM_SIZE];
int			AbpDisasmedCacheIndex = 0;

CachedInst *AbpGetCachedInst(int index)
{
	if (index < 0 || index >= AbpDisasmedCacheIndex)
		return NULL;

	return &AbpCachedDisasmedInstructions[index];
}

void AbpEmptyInstructionCache()
{
	memset(AbpCachedDisasmedInstructions, 0, sizeof(CachedInst) * ABP_CACHED_DISASM_SIZE);
	AbpDisasmedCacheIndex = 0;
}

void AbpCacheInstruction(duint addr, BASIC_INSTRUCTION_INFO *inst)
{
	CachedInst *slot;

	bool moveUp = AbpDisasmedCacheIndex >= ABP_CACHED_DISASM_SIZE;

	if (!moveUp)
	{
		slot = &AbpCachedDisasmedInstructions[AbpDisasmedCacheIndex];
		AbpDisasmedCacheIndex++;
	}
	else
	{
		memmove(
			AbpCachedDisasmedInstructions,
			AbpCachedDisasmedInstructions + 1,
			sizeof(CachedInst) * (ABP_CACHED_DISASM_SIZE - 1)
		);

		slot = &AbpCachedDisasmedInstructions[ABP_CACHED_DISASM_SIZE - 1];
	}

	memcpy(&slot->inst, inst, sizeof(BASIC_INSTRUCTION_INFO));
	slot->addr = addr;
}

bool AbpRegisterDeferredAPIRegistration(const char *module, const char *api)
{
	string smod(module);
	ondemand_api_list::iterator it;
	vector<char *> *apiList;
	char *apiString;

	it = AbpDeferList.find(smod);

	if (it == AbpDeferList.end())
	{
		apiList = new vector<char *>();

		SYNCH_BEGIN;

		AbpDeferList.insert({ smod,apiList });

		SYNCH_END;

	}
	else
		apiList = it->second;

	apiString = HlpCloneStringA((LPSTR)api);

	if (!apiString)
		return false;

	apiList->push_back(apiString);

	return true;
}

bool AbpParseRegister(char *regstr, LoadInstInfo *linst)
{
	char pfx = *regstr;
	int beg = 0, end = 8, rx = -1;

	char *tmp = NULL;

	if (strstr(regstr, "0x"))
	{
#ifdef _WIN64
		linst->imm = (duint)strtoull(regstr, NULL, 0);
		linst->size = 64;
#else
		linst->imm = (duint)strtoul(regstr, NULL, 0);
		linst->size = 32;
#endif

		return true;

	}

	linst->size = GET_REGSIZE(pfx);

	if (pfx == 'r')
	{
		beg = 0;
		end = 16;
	}

	for (;beg < end;beg++)
	{
		if (!strcmp(regstr + 1, regs[beg] + 1))
		{
			rx = beg;
			break;
		}
	}

	linst->reg = rx;

	return rx >= 0;
}


bool AbpParseLoadInstruction(char *inst,LoadInstInfo *linst)
{
	const char *delims = " [],:";
	char *p = strtok(inst, delims);
	
	char order = 0;

	memset(linst, 0, sizeof(LoadInstInfo));

	while (p != NULL && order != ORD_DONE)
	{
		if (order == ORD_OP)
		{
			if (!strcmp(p, "mov"))
				linst->op = OP_MOV;
			else if (!strcmp(p, "lea"))
				linst->op = OP_LEA;
			else if (!strcmp(p, "push"))
				linst->op = OP_PUSH;
			else
				return false;

			order++;
		}
		else if (order == ORD_DREG)
		{
			if (!strcmp(p, "byte") ||
				!strcmp(p, "word") ||
				!strcmp(p, "dword") ||
				!strcmp(p, "qword"))
			{
				strtok(NULL, delims); //skip "ptr" token
				p = strtok(NULL, delims); //skip segment if exist

				if (p[1] == 's' && p[2] =='\0') //if segment identifier skip to the next token
					p = strtok(NULL, delims); //grab register

				if (!p)
					return false;

				linst->memstore = 1;
			}

			if (!AbpParseRegister(p, linst))
				return false;

			order++;
		}

		p = strtok(NULL, delims);

		if (linst->memstore)
		{
			if (*p == '+')
			{
				p = strtok(NULL, delims);

				if (!p)
					return false;

				linst->disp = (unsigned short)atoi(p);

				p = strtok(NULL, delims);
			}
		}

	}

	return true;
}

duint AbpGetActualDataAddress(BASIC_INSTRUCTION_INFO *inst)
{
	if (inst->type == TYPE_ADDR)
		return inst->value.value;
	else if (inst->type == TYPE_MEMORY)
		return inst->memory.value;
	else if (inst->type == TYPE_VALUE)
		return inst->value.value;

	return 0;
}

bool AbpReadStringFromInstructionSourceAddress(BASIC_INSTRUCTION_INFO *inst, char *nameBuf)
{
	duint addr = AbpGetActualDataAddress(inst);

	if (!addr || addr < 0x1000)
	{
		DBGPRINT("String memory not valid!");
		return false;
	}

	if (!DbgGetStringAt(addr, nameBuf))
	{
		DBGPRINT("String read error from 0x%p",addr);
		return false;
	}

	//if string begins with 'L' 
	//that hints to the string source is a wide string
	if (*nameBuf == 'L')
	{
		HlpTrimChar(nameBuf, 'L', HLP_TRIM_LEFT);
	}

	HlpRemoveQuotations(nameBuf);

	return true;
}

bool AbpGetApiStringFromProcLoader(short argNumber, char *nameBuf)
{
	CachedInst *inst;
	LoadInstInfo loadinst;
	RegId compVal=RegNone;
	int cacheIndex;

#ifdef _WIN64
	
	switch (argNumber)
	{
	case 1:
		compVal = Cx;
		break;
	case 2:
		compVal = Dx;
		break;
	case 3:
		compVal = r8;
		break;
	case 4:
		compVal = r9;
		break;
	}
#else
	argNumber--;
#endif

	cacheIndex = AbpDisasmedCacheIndex;

	while (--cacheIndex >= 0)
	{
		inst = AbpGetCachedInst(cacheIndex);

		if (AbpParseLoadInstruction(inst->inst.instruction, &loadinst))
		{
			if (loadinst.size == 64 && !loadinst.memstore)
			{
				switch (loadinst.op)
				{
					case OP_MOV:
					case OP_LEA:
					{
						if (compVal == loadinst.reg)
						{
							if (AbpReadStringFromInstructionSourceAddress(&inst->inst, nameBuf))
								return true;

							return false;
						}
					}
					break;
				}
			}
			else if (loadinst.size == 32)
			{
				switch (loadinst.op)
				{
					case OP_MOV:
					case OP_LEA:
					{
						if (loadinst.reg == Sp && loadinst.memstore)
						{
							if (loadinst.disp == (argNumber * 4))
							{
								if (AbpReadStringFromInstructionSourceAddress(&inst->inst, nameBuf))
									return true;

								return false;
							}
						}
					}
					break;
					case OP_PUSH:
					{
						if (!argNumber)
						{
							if (AbpReadStringFromInstructionSourceAddress(&inst->inst, nameBuf))
								return true;

							return false;
						}
						else
							argNumber--;
					}
					break;
				}
			}
		}
	}

	return false;

}

bool AbpExposeStringArgument(short argNumber, char *buf)
{
	return AbpGetApiStringFromProcLoader(argNumber, buf);
}

bool AbpIsValidApi(const char *module, const char *function)
{
	PBYTE mod = NULL;
	PIMAGE_NT_HEADERS ntHdr;
	PIMAGE_DOS_HEADER dosHdr;
	PIMAGE_EXPORT_DIRECTORY ped;
	DWORD moduleSize, exportTableVa;
	HANDLE moduleFile, mapping;
	char *exportName;
	ULONG *addressOfNameStrings;
	duint imageBase;

	bool valid = false;
	char path[MAX_PATH];
	int pathLen;

	pathLen = GetSystemDirectoryA(path, MAX_PATH);

	if (!pathLen)
		return false;


	for (bool secondChance = true;;)
	{
		sprintf(path + pathLen, "\\%s", module);

		moduleFile = CreateFileA(
			path,
			GENERIC_READ,
			FILE_SHARE_READ,
			NULL,
			OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL,
			NULL
		);

		if (!secondChance || moduleFile != INVALID_HANDLE_VALUE)
			break;

		if (moduleFile == INVALID_HANDLE_VALUE)
		{
			pathLen = GetWindowsDirectoryA(path, MAX_PATH);
			secondChance = false;
		}
	}

	if (moduleFile == INVALID_HANDLE_VALUE)
		return false;

	moduleSize = GetFileSize(moduleFile, NULL);

	mapping = CreateFileMappingA(moduleFile, NULL, PAGE_READONLY | SEC_IMAGE_NO_EXECUTE, 0, ALIGNTO_PAGE(moduleSize), NULL);

	if (!mapping)
	{
		CloseHandle(moduleFile);
		return false;
	}

	mod = (PBYTE)MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, (SIZE_T)moduleSize);

	if (!mod)
	{
		CloseHandle(mapping);
		CloseHandle(moduleFile);
		return false;
	}

	dosHdr = (PIMAGE_DOS_HEADER)mod;

	if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE)
		goto fail;

	ntHdr = (PIMAGE_NT_HEADERS)(mod + dosHdr->e_lfanew);

	if (ntHdr->Signature != IMAGE_NT_SIGNATURE)
		goto fail;

	imageBase = ntHdr->OptionalHeader.ImageBase;

	if (imageBase != (duint)mod)
		imageBase = (duint)mod;

	exportTableVa = ntHdr->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
	ped = (PIMAGE_EXPORT_DIRECTORY)(exportTableVa + imageBase);
	addressOfNameStrings = (ULONG *)(ped->AddressOfNames + imageBase);
	
	for (int i = 0;i < ped->NumberOfNames;i++)
	{
		exportName = (char *)(imageBase + addressOfNameStrings[i]);

		if (!strcmp(exportName, function))
		{
			valid = true;
			break;
		}
	}

fail:

	UnmapViewOfFile(mod);
	CloseHandle(mapping);
	CloseHandle(moduleFile);

	return valid;
}

bool AbpIsAPICall2(duint code, ApiFunctionInfo *afi, BASIC_INSTRUCTION_INFO *inst, bool cacheInstruction)
{
	duint callAddr;
	
	if (inst->call && inst->branch)
	{
		callAddr = 0;

		if (inst->type == TYPE_ADDR)
			callAddr = inst->value.value;
		else if (inst->type == TYPE_MEMORY)
		{
			if (sizeof(duint) < inst->memory.size)
				DBGPRINT("Integer overflow. What do you to do?");
			else
				DbgMemRead(inst->memory.value, &callAddr, inst->memory.size);
		}

		if (callAddr == afi->ownerModule->baseAddr + afi->rva)
		{
			DBGPRINT("'%s' call found at %p",afi->name, code);
			return true;
		}
	}

	if (cacheInstruction)
		AbpCacheInstruction(code, inst);

	return false;
}

bool AbpIsAPICall(duint code, ApiFunctionInfo *afi, BASIC_INSTRUCTION_INFO *inst)
{
	return AbpIsAPICall2(code, afi, inst, true);
}

#define NEXT_INSTR_ADDR(code, inst) (*code) += (inst)->size

bool AbpIsLoadLibraryXXXCall(ApiFunctionInfo **afiList, int afiCount,duint codeAddr, BASIC_INSTRUCTION_INFO *inst, bool cacheInstruction)
{
	for (int i = 0;i < afiCount;i++)
	{
		if (AbpIsAPICall2(codeAddr, afiList[i], inst, false))
		{
			return true;
		}
	}

	if (cacheInstruction)
		AbpCacheInstruction(codeAddr, inst);

	return false;
}



bool AbpRegisterAPI(char *module, char *api)
{
	duint funcAddr, modAddr, rva;


	funcAddr = RemoteGetProcAddress(module, api);

	//Is module already loaded?
	if (funcAddr)
	{
		modAddr = DbgModBaseFromName(module);
		rva = funcAddr - modAddr;

		AbiRegisterDynamicApi(module, api, modAddr, funcAddr, rva);

		return true;
	}

	if (!AbpIsValidApi(module, api))
	{
		DBGPRINT("Not valid api");
		return false;
	}

	return AbpRegisterDeferredAPIRegistration(module, api);
}

INTERNAL_EXPORT void AbiRaiseOnDemandLoader(const char *dllName, duint base)
{
	duint funcAddr;
	ondemand_api_list::iterator modIter;
	vector<char *> *apiList;

	DBGPRINT("%s loaded looking for its one of the deferred module",dllName);

	modIter = AbpDeferList.find(string(dllName));

	if (modIter == AbpDeferList.end())
	{
		DBGPRINT("No, its not");
		return;
	}

	apiList = modIter->second;

	DBGPRINT("Deferred api's are now registering.");

	for (vector<char *>::iterator it = apiList->begin(); it != apiList->end(); it++)
	{
		funcAddr = Script::Misc::RemoteGetProcAddress(dllName, *it);

		if (funcAddr > 0)
		{
			AbiRegisterDynamicApi(dllName, *it, base,funcAddr,funcAddr-base);
		}
	}

	SYNCH_BEGIN;
	//TODO : Remove loaded modules from list
	SYNCH_END;

	//REMOVE waiting mod info
}

//loaderApi can be LoadLibraryA LoadLibraryW LoadLibraryExA LoadLibraryExW
INTERNAL_EXPORT bool AbiDetectAPIsUsingByGetProcAddress()
{
	const char *ldrVariants[6] = { 
		"LoadLibraryA",
		"LoadLibraryW",
		"LoadLibraryExA",
		"LoadLibraryExW",
		"GetModuleHandleA",
		"GetModuleHandleW"};

	BASIC_INSTRUCTION_INFO inst;

	ModuleSectionInfo codeSect;
	ApiFunctionInfo *procafi=NULL;
	ApiFunctionInfo *libafis[6] = { 0 };
	int ldrVariantCount = 0,procLdrCount=0;
	int totalFoundApi = 0;

	int ldrVariantLimit;
	bool skip;
	duint code, end;

	DBGPRINT("Threadid: %x", GetCurrentThreadId());

	char moduleName[128], apiFuncName[128];
	
	procafi = AbiGetAfi("kernel32.dll", "GetProcAddress");

	if (AbGetSettings()->includeGetModuleHandle)
	{
		DBGPRINT("Included GetModuleHandle(A/W) !");
		ldrVariantLimit = 6;
	}
	else
		ldrVariantLimit = 4;

	for (int i = 0;i < ldrVariantLimit;i++)
	{
		if ((libafis[ldrVariantCount] = AbiGetAfi("kernel32.dll", ldrVariants[i])) != NULL)
			ldrVariantCount++;
	}
	
	if (!ldrVariantCount || !procafi)
		return false;

	if (!AbiGetMainModuleCodeSection(&codeSect))
		return false;

	code = codeSect.addr;
	end = code + codeSect.size;

	DBGPRINT("Scanning range %p - %p", code, end);


	while (code < end)
	{
		DbgDisasmFastAt(code, &inst);

		if (AbpIsLoadLibraryXXXCall(libafis,ldrVariantCount,code,&inst,true))
		{
			memset(moduleName, 0, sizeof(moduleName));

			skip = !AbpExposeStringArgument(1, moduleName);

			AbpEmptyInstructionCache();

			if (skip)
			{
				NEXT_INSTR_ADDR(&code, &inst);
				continue;
			}

			if (!HlpEndsWithA(moduleName, ".dll", FALSE, 4))
				strcat(moduleName, ".dll");

			procLdrCount = 0;

			while (1)
			{
				NEXT_INSTR_ADDR(&code, &inst);

				DbgDisasmFastAt(code, &inst);

				if (!strncmp(inst.instruction, "ret", 3))
				{
					if (!procLdrCount)
						DBGPRINT("GetProcAddress is not called after LoadLibraryXXX");
					//else its ok. there is no more getprocaddress calls for the module

					DBGPRINT("%d API load found in single module", procLdrCount);

					NEXT_INSTR_ADDR(&code, &inst);

					break;
				}

				if (AbpIsAPICall(code, procafi, &inst))
				{
					memset(apiFuncName, 0, sizeof(apiFuncName));

					if (AbpExposeStringArgument(2, apiFuncName))
					{
						DBGPRINT("Found: %s : %s", moduleName, apiFuncName);
						totalFoundApi++;
						procLdrCount++;

						AbpEmptyInstructionCache();
#if 1
						if (!AbpRegisterAPI(moduleName, apiFuncName))
							DBGPRINT("Not load, not registered or not valid: %s:%s", moduleName, apiFuncName);
#endif
					}
					else
						DBGPRINT("Fail");
				}
				else if (AbpIsLoadLibraryXXXCall(libafis, ldrVariantCount, code, &inst, false))
				{
					if (!procLdrCount)
					{
						//We dont skip to next instruction address
						//cuz, It will be our new loadlibrary reference.
						DBGPRINT("Consecutive loadlibrary calls :/");
					}
					else
					{
						DBGPRINT("%d API load found in single module", procLdrCount);
					}

					break;
				}
			}
		}
		else
			NEXT_INSTR_ADDR(&code, &inst);
	}

	DBGPRINT("Success. %d API(s) found!", totalFoundApi);

	return true;
}


INTERNAL_EXPORT void AbiInitDynapi()
{
	AbpDeferListSyncMutant = CreateMutexA(NULL, FALSE, NULL);
}

INTERNAL_EXPORT void AbiUninitDynapi()
{
	CloseHandle(AbpDeferListSyncMutant);
}

