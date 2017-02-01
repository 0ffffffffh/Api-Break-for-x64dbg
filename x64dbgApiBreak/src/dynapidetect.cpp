#include <corelib.h>
#include <pluginsdk/_scriptapi_module.h>
#include <pluginsdk/_scriptapi_misc.h>

using namespace Script::Module;
using namespace Script::Misc;

INTERNAL ApiFunctionInfo *AbiGetAfi(const char *module, const char *afiName);
INTERNAL bool AbiGetMainModuleCodeSection(ModuleSectionInfo *msi);



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

	if (!addr)
		return false;

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
			DBGPRINT("'%s' call found at %p for the %s",afi->name, code,afi->name);
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

bool AbpLoadRemoteLibrary(char *module, char *api, DYNAMIC_API_ENUM_PROC enumCb)
{
	char *loadercmd;
	int loadWait = 10;
	duint addr,modbase,rva;

	HlpPrintFormatBufferA(&loadercmd, "loadlib \"%s\"", module);

	if (!DbgCmdExecDirect(loadercmd))
	{
		FREESTRING(loadercmd);
		return false;
	}

	/*
	Ugly hack!
	We must delay a bit until the library gets loaded.

	The DbgCmdExec|(Direct) are just returns true when
	the command executed successfuly and its not wait for
	library loading completion.

	*/
	while (loadWait > 0)
	{
		addr = RemoteGetProcAddress(module, api);

		if (!addr)
			Sleep(20);
		else
			break;

		loadWait--;
	}

	FREESTRING(loadercmd);

	if (addr)
	{
		modbase = DbgModBaseFromName(module);

		rva = addr - modbase;

		if (enumCb)
			enumCb(module, api, modbase, addr, rva);

		return true;
	}
	//If the addr value still zero, the module probably wont be loaded.

	return false;
}

//loaderApi can be LoadLibraryA LoadLibraryW LoadLibraryExA LoadLibraryExW
INTERNAL_EXPORT bool AbiDetectAPIsUsingByGetProcAddress(DYNAMIC_API_ENUM_PROC enumCb)
{
	const char *ldrVariants[4] = { "LoadLibraryA","LoadLibraryW","LoadLibraryExA","LoadLibraryExW" };

	BASIC_INSTRUCTION_INFO inst;

	ModuleSectionInfo codeSect;
	ApiFunctionInfo *procafi=NULL;
	ApiFunctionInfo *libafis[4] = { 0 };
	int ldrVariantCount = 0;
	bool skip;
	duint code, end;

	char moduleName[128], apiFuncName[128];
	
	procafi = AbiGetAfi("kernel32.dll", "GetProcAddress");

	for (int i = 0;i < 4;i++)
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

			while (1)
			{
				NEXT_INSTR_ADDR(&code, &inst);

				DbgDisasmFastAt(code, &inst);

				if (!strncmp(inst.instruction, "ret", 3))
				{
					DBGPRINT("GetProcAddress is not called after LoadLibraryXXX");
					break;
				}

				if (AbpIsAPICall(code, procafi, &inst))
				{
					AbpExposeStringArgument(2, apiFuncName);

					DBGPRINT("Found: %s : %s", moduleName, apiFuncName);


					AbpEmptyInstructionCache();
					NEXT_INSTR_ADDR(&code, &inst);


					AbpLoadRemoteLibrary(moduleName, apiFuncName,enumCb);

					break;
				}
				else if (AbpIsLoadLibraryXXXCall(libafis, ldrVariantCount, code, &inst, false))
				{
					DBGPRINT("Consecutive loadlibrary calls :/");
					break;
				}
			}
		}
		else
			NEXT_INSTR_ADDR(&code, &inst);
	}

	return true;
}