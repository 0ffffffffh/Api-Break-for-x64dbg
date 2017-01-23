#include <corelib.h>
#include <pluginsdk/_scriptapi_module.h>

using namespace Script::Module;

#define BACKTRACE_CODE_LIMIT_SIZE 32

#define MODREG_RDX		2 //010

#define OC_LEA 0x8D
#define OC_MOV 0x8B

#define READ_MODREG(x) ((x >> 3) & 7)

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
bool AbpGetApiStringFromProcLoaderAMD64(duint code, char *nameBuf)
{
	BASIC_INSTRUCTION_INFO inst;
	unsigned char rawCodeBytes[4];
	duint procStringAddr;

	code--;

	for (int i = 0;i < BACKTRACE_CODE_LIMIT_SIZE;i++)
	{
		DbgMemRead(code, rawCodeBytes, sizeof(rawCodeBytes));

		if (rawCodeBytes[0] == 0x48)
		{
			switch (rawCodeBytes[1])
			{
			case OC_LEA:
			case OC_MOV:
			{
				if (READ_MODREG(rawCodeBytes[2]) == MODREG_RDX)
				{
					DbgDisasmFastAt(code, &inst);

					if (inst.type == TYPE_ADDR)
						procStringAddr = inst.value.value;
					else if (inst.type == TYPE_MEMORY)
						procStringAddr = inst.memory.value;

					if (DbgGetStringAt(procStringAddr, nameBuf))
					{
						HlpRemoveQuotations(nameBuf);

						DBGPRINT("Proc string found %s at %p", nameBuf, procStringAddr);

						return true;
					}


				}
			}
			break;
			}
		}

		code--;
	}

	return false;

}

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
MOV [ESP+4], MODULEHANDLE
MOV [ESP],HARDCODED_API_STRING
CALL GetProcAddress

*/

bool AbpGetApiStringFromProcLoaderX86(duint code, char *nameBuf)
{
	DBGPRINT("not implemented");
	return false;
}

INTERNAL_EXPORT void AbiDetectAPIsUsingByGetProcAddress()
{
	BASIC_INSTRUCTION_INFO inst;
	ModuleSectionInfo codeSect;
	ApiFunctionInfo* afi = AbiGetAfi("kernel32.dll", "GetProcAddress");
	duint code, end, callAddr;
	char apiFuncName[128];

	if (!afi)
		return;

	if (!AbiGetMainModuleCodeSection(&codeSect))
		return;

	code = codeSect.addr;
	end = code + codeSect.size;

	while (code < end)
	{
		DbgDisasmFastAt(code, &inst);

		if (inst.call && inst.branch)
		{
			callAddr = 0;

			if (inst.type == TYPE_ADDR)
				callAddr = inst.value.value;
			else if (inst.type == TYPE_MEMORY)
			{
				if (sizeof(duint) < inst.memory.size)
					DBGPRINT("Integer overflow. What do you to do?");
				else
					DbgMemRead(inst.memory.value, &callAddr, inst.memory.size);
			}

			if (callAddr == afi->ownerModule->baseAddr + afi->rva)
			{
				DBGPRINT("GetProcAddress call found at %p", callAddr);

#ifdef _WIN64
				//the RDX holds api function string address
				AbpGetApiStringFromProcLoaderAMD64(code, apiFuncName);
#else
				//The first push points to api function string
				AbpGetApiStringFromProcLoaderX86(code,apiFuncName);
#endif
			}
		}

		code += inst.size;
	}
}