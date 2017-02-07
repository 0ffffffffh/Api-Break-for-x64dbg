#include <corelib.h>

INTERNAL duint AbpGetActualDataAddress(BASIC_INSTRUCTION_INFO *inst);

FORWARDED int AbiSearchCallersForAFI(duint codeBase, duint codeSize, ApiFunctionInfo *afi)
{
#define RETURN(s) { result = s; goto oneWayExit; }

	duint code,codeEnd;
	BASIC_INSTRUCTION_INFO inst;
	duint *calls = NULL, *tmp;
	duint callAddr;

	int index=0, callsSize = 0;

	int result = CSR_FAILED;

	code = codeBase;
	codeEnd = code + codeSize; 

	calls = afi->calls;
	index = afi->callCount;

	for (;code < codeEnd;)
	{
		DbgDisasmFastAt(code, &inst);

		if (inst.call || inst.branch)
		{
			callAddr = AbpGetActualDataAddress(&inst);

			//Gotcha!
			if (afi->ownerModule->baseAddr + afi->rva == callAddr)
			{
				if (!calls)
				{
					callsSize = 20;
					calls = (duint *)AbMemoryAlloc(callsSize * sizeof(duint));

					if (!calls)
						RETURN(CSR_FAILED);
				}
				else if (index - 1 >= callsSize)
				{
					tmp = (duint *)AbMemoryRealloc(calls, (callsSize + 10) * sizeof(duint));

					if (!tmp)
					{
						DBGPRINT("realloc fail");
						RETURN(CSR_PARTIAL_SUCCESS);
					}

					calls = tmp;

					callsSize += 10;

				}

				*(calls + index) = code;
				index++;

				DBGPRINT("Call or jump found on 0x%p", code);
			}

		}

		code += inst.size;
	}

	result = CSR_COMPLETELY_SUCCESS;

oneWayExit:

	if (result > 0)
	{
		afi->calls = calls;
		afi->callCount = index;
	}
	
	return result;

#undef RETURN
}