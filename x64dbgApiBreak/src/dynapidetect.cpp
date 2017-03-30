#include <corelib.h>
#include <settings.h>
#include <instparse.h>
#include <unordered_map>
#include <pluginsdk/_scriptapi_module.h>
#include <pluginsdk/_scriptapi_misc.h>

using namespace Script::Module;
using namespace Script::Misc;
using namespace std;

INTERNAL ApiFunctionInfo *AbiGetAfi(const char *module, const char *afiName);
INTERNAL int AbiGetMainModuleCodeSections(ModuleSectionInfo **msi);
INTERNAL bool AbiRegisterDynamicApi(const char *module, const char *api, duint mod, duint apiAddr, duint apiRva);
INTERNAL bool AbiIsIndirectCall(duint code, ApiFunctionInfo *afi, duint *indirectRef);

#define ALIGNTO_PAGE(x) ( ((x)/0x1000+1) * 0x1000 )


typedef  unordered_map<string, vector<char *> *> ondemand_api_list;

HANDLE                  AbpDeferListSyncMutant;
ondemand_api_list       AbpDeferList;

#define SYNCH_BEGIN     WaitForSingleObject(AbpDeferListSyncMutant,INFINITE)
#define SYNCH_END       ReleaseMutex(AbpDeferListSyncMutant)


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
    BASIC_INSTRUCTION_INFO          inst;
    duint                           addr;
}CachedInst;

#define ABP_CACHED_DISASM_SIZE      20

CachedInst  AbpCachedDisasmedInstructions[ABP_CACHED_DISASM_SIZE];
int         AbpDisasmedCacheIndex = 0;

CachedInst *AbpGetCachedInst(int index)
{
    if (index < 0 || index >= AbpDisasmedCacheIndex)
        return NULL;

    return &AbpCachedDisasmedInstructions[index];
}

INTERNAL_EXPORT void AbiEmptyInstructionCache()
{
    memset(AbpCachedDisasmedInstructions, 0, sizeof(CachedInst) * ABP_CACHED_DISASM_SIZE);
    AbpDisasmedCacheIndex = 0;
}

INTERNAL_EXPORT void AbiCacheInstruction(duint addr, BASIC_INSTRUCTION_INFO *inst)
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


INTERNAL_EXPORT duint AbpGetActualDataAddress(BASIC_INSTRUCTION_INFO *inst)
{
    if ((inst->type & TYPE_ADDR) || (inst->type & TYPE_VALUE))
        return inst->value.value;
    else if (inst->type == TYPE_MEMORY)
        return inst->memory.value;

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
    InstInfo loadinst;
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

        if (AbParseInstruction(&inst->inst, &loadinst))
        {
            if (loadinst.size == 64 && loadinst.mem_access == MA_READ)
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
                        if (loadinst.reg == ImmMem && loadinst.mem_access == MA_WRITE && loadinst.memory_info.base == Sp)
                        {
                            if (loadinst.memory_info.disp == (argNumber * 4))
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

bool AbpIsValidApi(const char *module, const char *function, duint *rva)
{
    PBYTE mod = NULL;
    PIMAGE_NT_HEADERS ntHdr;
    PIMAGE_DOS_HEADER dosHdr;
    PIMAGE_EXPORT_DIRECTORY ped;
    DWORD moduleSize, exportTableVa;
    HANDLE moduleFile, mapping;
    char *exportName;
    ULONG *addressOfNameStrings, *addressOfFunctions;
    WORD *addressOfOrd;
    duint imageBase;

    bool valid = false;
    char path[3][MAX_PATH];
    
    GetSystemDirectoryA(path[0], MAX_PATH);
    GetWindowsDirectoryA(path[1], MAX_PATH);
    AbGetDebuggedModulePath(path[2], MAX_PATH);

    
    for (int i=0;i<3;i++)
    {
        sprintf(path[i] + strlen(path[i]), "\\%s", module);

        moduleFile = CreateFileA(
            path[i],
            GENERIC_READ,
            FILE_SHARE_READ,
            NULL,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            NULL
        );

        if (moduleFile != INVALID_HANDLE_VALUE)
            break;
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

    for (DWORD i = 0;i < ped->NumberOfNames;i++)
    {
        exportName = (char *)(imageBase + addressOfNameStrings[i]);

        if (!strcmp(exportName, function))
        {
            if (rva != NULL)
            {
                addressOfFunctions = (ULONG *)(ped->AddressOfFunctions + imageBase);
                addressOfOrd = (WORD *)(ped->AddressOfNameOrdinals + imageBase);

                *rva = (duint)(addressOfFunctions[addressOfOrd[i]]);
            }

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

INTERNAL_EXPORT duint AbiGetCallDestinationAddress(BASIC_INSTRUCTION_INFO *inst)
{
    duint callAddr = 0;
    duint msize;

    if ((inst->type & TYPE_ADDR) || (inst->type & TYPE_VALUE))
        callAddr = inst->value.value;
    else if (inst->type == TYPE_MEMORY)
    {
        msize = inst->memory.size;

        //Some process injects some chunk of data to the code section for some reason.
        //And the disassembler interprets them as instruction sequences
        //For example call fword ptr [mem]. And that memory size will be 6 bytes 
        //Any read attempt on 32 bit machine the memory.value gets overflowed.

        if (!(msize == size_byte ||
                msize == size_word ||
                msize == size_dword ||
                msize == size_qword))
        {
            return 0;
        }

        DbgMemRead(inst->memory.value, &callAddr, msize);
    }

    return callAddr;
}

bool AbpIsAPICall2(duint code, ApiFunctionInfo *afi, BASIC_INSTRUCTION_INFO *inst, bool cacheInstruction)
{
    duint callAddr;

    if (inst->call && inst->branch)
    {
        callAddr = AbiGetCallDestinationAddress(inst);

        if (callAddr == afi->ownerModule->baseAddr + afi->rva)
        {
            DBGPRINT("'%s' call found at %p",afi->name, code);
            return true;
        }
        else
        {
            if (AbiIsIndirectCall(callAddr, afi, NULL))
                return true;
        }
    }

    if (cacheInstruction)
        AbiCacheInstruction(code, inst);

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
        AbiCacheInstruction(codeAddr, inst);

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

    if (!AbpIsValidApi(module, api,NULL))
    {
        DBGPRINT("Not valid api");
        return false;
    }

    return AbpRegisterDeferredAPIRegistration(module, api);
}

INTERNAL_EXPORT void AbiRaiseDeferredLoader(const char *dllName, duint base)
{
    duint funcAddr;
    ondemand_api_list::iterator modIter;
    vector<char *> *apiList;

    
    modIter = AbpDeferList.find(string(dllName));

    if (modIter == AbpDeferList.end())
        return;

    apiList = modIter->second;

    DBGPRINT("Deferred api's are now registering.");

    for (vector<char *>::iterator it = apiList->begin(); it != apiList->end(); it++)
    {
        //TODO: Find a way to get remote proc address in DLLLOAD callback

        //Probably returns null ?
        funcAddr = Script::Misc::RemoteGetProcAddress(dllName, *it);

        if (!funcAddr)
        {
            //And also it may returns null
            funcAddr = (duint)ImporterGetRemoteAPIAddressEx(dllName, *it);

            if (!funcAddr)
            {
                duint rva=0;

                //worst case! 
                //We know the loaded dll's base address. 
                //So if I can read rva of the API from the export table
                //I can calculate the real API address in the memory.
                //Its a bit slower but it works.
                if (AbpIsValidApi(dllName, *it, &rva))
                {
                    funcAddr = base + rva;          
                }

            }
        }

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

    ModuleSectionInfo *codeSects;
    ApiFunctionInfo *procafi=NULL;
    ApiFunctionInfo *libafis[6] = { 0 };
    int ldrVariantCount = 0,procLdrCount=0;
    int totalFoundApi = 0,sectCount;

    int ldrVariantLimit;
    bool skip;
    duint code, end;

    DBGPRINT("Threadid: %x", GetCurrentThreadId());

    char moduleName[128], apiFuncName[128];
    
    procafi = AbiGetAfi("kernel32.dll", "GetProcAddress");

    if (AbGetSettings()->includeGetModuleHandle)
        ldrVariantLimit = 6;
    else
        ldrVariantLimit = 4;

    for (int i = 0;i < ldrVariantLimit;i++)
    {
        if ((libafis[ldrVariantCount] = AbiGetAfi("kernel32.dll", ldrVariants[i])) != NULL)
            ldrVariantCount++;
    }
    
    if (!ldrVariantCount || !procafi)
    {
        DBGPRINT("This process does not use any runtime module and api loading.");
        return false;
    }

    sectCount = AbiGetMainModuleCodeSections(&codeSects);

    if (!sectCount)
        return false;

    for (int cnx = 0;cnx < sectCount;cnx++)
    {
        code = codeSects[cnx].addr;
        end = code + codeSects[cnx].size;

        DBGPRINT("Scanning range %p - %p of %d. section", code, end,cnx+1);

        while (code < end)
        {
            DbgDisasmFastAt(code, &inst);

            if (AbpIsLoadLibraryXXXCall(libafis, ldrVariantCount, code, &inst, true))
            {
                
                memset(moduleName, 0, sizeof(moduleName));

                skip = !AbpExposeStringArgument(1, moduleName);

                AbiEmptyInstructionCache();

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

                            AbiEmptyInstructionCache();

                            if (!AbpRegisterAPI(moduleName, apiFuncName))
                                DBGPRINT("Not load, not registered or not valid: %s:%s", moduleName, apiFuncName);
                        }
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
    }

    if (totalFoundApi>0)
        DBGPRINT("%d dynamic API(s) found!", totalFoundApi);

    DBGPRINT("Dynamic scan finished");

    return true;
}


INTERNAL_EXPORT void AbiReleaseDeferredResources()
{
    for (ondemand_api_list::iterator it = AbpDeferList.begin(); it != AbpDeferList.end(); it++)
    {
        for (vector<char *>::iterator vit = it->second->begin(); vit != it->second->end(); vit++)
        {
            FREEOBJECT(*vit);
        }

        delete it->second;
    }

    AbpDeferList.clear();
}

INTERNAL_EXPORT void AbiInitDynapi()
{
    AbpDeferListSyncMutant = CreateMutexA(NULL, FALSE, NULL);
}

INTERNAL_EXPORT void AbiUninitDynapi()
{
    CloseHandle(AbpDeferListSyncMutant);
}

