#include <corelib.h>

INTERNAL duint AbiGetCallDestinationAddress(BASIC_INSTRUCTION_INFO *inst);
INTERNAL void AbiCacheInstruction(duint addr, BASIC_INSTRUCTION_INFO *inst);
INTERNAL void AbiEmptyInstructionCache();

bool AbpInsertCallList(ApiFunctionInfo *afi,duint callAddr)
{
	duint *tmp;
	duint *calls;
	int index=0, callsSize = 0;

	calls = afi->callInfo.calls;
	index = afi->callInfo.callCount;
	callsSize = afi->callInfo.callListSize;

	if (!calls)
	{
		callsSize = 20;
		calls = (duint *)AbMemoryAlloc(callsSize * sizeof(duint));

		if (!calls)
			return false;
	}
	else if (index - 1 >= callsSize)
	{
		tmp = (duint *)AbMemoryRealloc(calls, (callsSize + 10) * sizeof(duint));

		if (!tmp)
		{
			DBGPRINT("realloc fail");
			return false;
		}

		calls = tmp;

		callsSize += 10;

	}

	*(calls + index) = callAddr;
	index++;

	afi->callInfo.calls = calls;
	afi->callInfo.callCount = index;
	afi->callInfo.callListSize = callsSize;

	DBGPRINT("Call or jump found on 0x%p", callAddr);

	return true;
}

INTERNAL_EXPORT bool AbiIsIndirectCall(duint code, ApiFunctionInfo *afi, duint *indirectRef)
{
	BASIC_INSTRUCTION_INFO destInst;
	duint apiAddr;

	DbgDisasmFastAt(code, &destInst);

	//We looking for only jmp. Eliminate other conditional jumps
	if (!destInst.call && destInst.branch && HlpBeginsWithA(destInst.instruction, "jmp", FALSE, 3))
	{
		apiAddr = AbiGetCallDestinationAddress(&destInst);

		if (apiAddr == afi->ownerModule->baseAddr + afi->rva)
		{
			if (indirectRef)
				*indirectRef = code;

			DBGPRINT("Indirect call (Tramboline) found for %s at 0x%p",
				afi->name, code);

			return true;
		}
	}

	return false;
}



FORWARDED int AbiSearchCallersForAFI(duint codeBase, duint codeSize, ApiFunctionInfo *afi)
{
#define RETURN(s) { result = s; goto oneWayExit; }

	duint code,codeEnd;
	BASIC_INSTRUCTION_INFO inst;
	duint callAddr,indirectReferencedAddress = 0;

	int failInsert=0;

	int result = CSR_FAILED;

	code = codeBase;
	codeEnd = code + codeSize; 

	AbiEmptyInstructionCache();

	for (;code < codeEnd;)
	{
		DbgDisasmFastAt(code, &inst);

		if (inst.call || inst.branch)
		{
			callAddr = AbiGetCallDestinationAddress(&inst);

			if (!inst.call && inst.branch) //is it jmp?
			{
				//Is it an indirectly referenced call?
				if (code == indirectReferencedAddress)
				{
					DBGPRINT("%s is indirectly referenced and bp wont be set for %p", afi->name, code);
					code += inst.size;
					continue;
				}
			}

			//Gotcha!
			if (afi->ownerModule->baseAddr + afi->rva == callAddr)
				failInsert += (int)!AbpInsertCallList(afi, code);
			else if (inst.call && AbiIsIndirectCall(callAddr, afi,&indirectReferencedAddress))
				failInsert += (int)!AbpInsertCallList(afi, code);
		}
		else 
			AbiCacheInstruction(code, &inst);

		code += inst.size;
	}

	if (afi->callInfo.callCount > 0)
	{
		if (failInsert > 0)
			result = CSR_PARTIAL_SUCCESS;
		else
			result = CSR_COMPLETELY_SUCCESS;
	}


	return result;

#undef RETURN
}