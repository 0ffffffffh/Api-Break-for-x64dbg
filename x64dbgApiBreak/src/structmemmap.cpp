#include <structmemmap.h>

#define MAX_TYPENAME_SIZE               128

#define ALLOCOBJECTLIST(type,count)     RESIZEOBJECTLIST(type,NULL,count)

#define SIZEOF_INT                      sizeof(int)
#define SIZEOF_POINTER                  sizeof(void *)
#define SIZEOF_CHAR                     1
#define SIZEOF_WCHAR                    2
#define SIZEOF_BYTE                     1
#define SIZEOF_SHORT                    2
#define SIZEOF_LONG                     4
#define SIZEOF_LONGLONG                 8

#define SIZEOF_WORD                     SIZEOF_SHORT
#define SIZEOF_DWORD                    SIZEOF_LONG
#define SIZEOF_QWORD                    SIZEOF_LONGLONG


#define FAILBASE_SYSTEM                 0
#define FAILBASE_PARSER                 FAILBASE_SYSTEM + 4


#define FS_MEMERR                       FAILBASE_SYSTEM + 1
#define FS_RESERVED1                    FAILBASE_SYSTEM + 2
#define FS_RESERVED2                    FAILBASE_SYSTEM + 3
#define FS_RESERVED3                    FAILBASE_SYSTEM + 4

#define FS_MAPTYPE                      FAILBASE_PARSER + 1
#define FS_TYPENAME                     FAILBASE_PARSER + 2
#define FS_BRACKOPEN                    FAILBASE_PARSER + 3
#define FS_EXPECTFIELD                  FAILBASE_PARSER + 4
#define FS_BRACKCLOSE                   FAILBASE_PARSER + 5

#define FS_NOFIELDTYPE                  FAILBASE_PARSER + 6
#define FS_EXPECTFIELDNAME              FAILBASE_PARSER + 7
#define FS_EXPECTSQUBRCKOPEN            FAILBASE_PARSER + 8
#define FS_EXPECTSQUBRCKCLOSE           FAILBASE_PARSER + 9
#define FS_EXPECTARRSIZE                FAILBASE_PARSER + 10
#define FS_EXPECTSEMICOLON              FAILBASE_PARSER + 11

#define FS_EXPECTNOTOP                  FAILBASE_PARSER + 12
#define FS_EXPECTPARANOPEN              FAILBASE_PARSER + 13
#define FS_EXPECTPARANCLOSE             FAILBASE_PARSER + 14
#define FS_EXPECTTYPE                   FAILBASE_PARSER + 15
#define FS_EXPECTARGNAME                FAILBASE_PARSER + 16
#define FS_EXPECTMODNAME                FAILBASE_PARSER + 17
#define FS_UNEXPECTED_TOKEN             FAILBASE_PARSER + 18
#define FS_UNEXPECTED_SYTX_END          FAILBASE_PARSER + 19

#define FS_TYPENOTFOUND                 FAILBASE_PARSER + 20
#define FS_EXPECTINCLFILEPATH           FAILBASE_PARSER + 21
#define FS_EXPECTIDENT					FAILBASE_PARSER + 22

//LINKED LIST

#define RECORD_OF(node, type)   ( (type)((node)->value) )

typedef struct __SLISTNODE
{
    struct __SLISTNODE *        next;
    void *                      value;
}*PSLISTNODE, SLISTNODE;

typedef struct
{
    PSLISTNODE                  head;
    PSLISTNODE                  tail;
    LONG                        count;
}*PSLIST, SLIST;

//TYPED MEMORY MAP

typedef void(*CONVERTER_METHOD)(char *, void *, bool);

typedef struct
{
    WORD                        cbSize;
    char                        typeName[MAX_TYPENAME_SIZE];
}*PGENERIC_DATATYPE_INFO;

typedef struct
{
    WORD                        cbSize;
    char                        typeName[MAX_TYPENAME_SIZE];
    WORD                        size;
    BOOL                        isSigned;
    WORD                        linkIndex;
    CONVERTER_METHOD            method;
}*PPRIMITIVETYPEINFO, PRIMITIVETYPEINFO;


#define SmmListOf(x) PSLIST

typedef struct
{
    WORD                        cbSize;
    char                        name[MAX_TYPENAME_SIZE];
    DWORD                       structSize;
    SmmListOf(PSTRUCTMEMBERFIELD)   fields;
}*PSTRUCTINFO, STRUCTINFO;


////////////////////////

typedef struct
{

    char                        fieldName[128];
    WORD                        arrSize;
    BOOL                        isArray;
    BOOL                        isPointer;
    BOOL                        isStruct;

    union
    {
        PPRIMITIVETYPEINFO      primitiveType;
        PSTRUCTINFO             structType;
    }type;

}*PSTRUCTMEMBERFIELD, STRUCTMEMBERFIELD;

typedef struct
{
    char                        name[64];
    BOOL                        isStructure;
    BOOL                        isPointer;
    BOOL                        isOutArg;
    union
    {
        PVOID                   holder;
        PSTRUCTINFO             structInfo;
        PPRIMITIVETYPEINFO      primitiveInfo;
    }typeInfo;
}*PARGINFO, ARGINFO;

typedef struct
{
	union
	{
		PVOID					u;
		PSTRUCTINFO				typeStruct;
		PPRIMITIVETYPEINFO		typePrimitive;
	}typeInfo;
}*PRETINFO, RETINFO;

#define CDI_FUNCTION_ARGUMENT	1
#define CDI_FUNCTION_RETURN		2
#define CDI_STRUCTURE_FIELD		3

typedef struct
{
	USHORT type;

	union
	{
		PARGINFO			argInfo;
		PRETINFO			retInfo;
		PSTRUCTMEMBERFIELD	fieldInfo;

	}u;
}*PCOMMON_DATAFIELD_INFO,COMMON_DATAFIELD_INFO;

typedef struct __FNSIGN
{
    char                        name[256];
    char                        module[256];
    PARGINFO                    args;
	PRETINFO					ret;
    SHORT                       argCount;
    BOOL                        hasOutArg;
}*PFNSIGN, FNSIGN;

typedef struct
{
    const char *                identName;
    PPRIMITIVETYPEINFO          pti;
}typeidents;

typedef struct
{
	char alias[64];
	PVOID mappedType;
}*PTYPEALIAS,TYPEALIAS;

#define SMMP_WORKDIR_STACKSIZE  10
typedef struct
{
    struct {
        BOOL    isLocal;
        WCHAR   workDir[MAX_PATH];
    }Stack[SMMP_WORKDIR_STACKSIZE];
    WORD        spi;
}WORKDIR_STACK;

static typeidents TYPE_IDENTS[] =
{
    { "int",NULL },
    { "uint",NULL },
    { "char",NULL },
    { "wchar",NULL },
    { "byte",NULL },
    { "ubyte",NULL },
    { "short",NULL },
    { "ushort",NULL },
    { "long", NULL },
    { "ulong",NULL },
    { "long64",NULL },
    { "ulong64",NULL },
    //SPECIALS
    { "string",NULL },
    { "wstring",NULL },
    { "pointer",NULL },
    { "archex", NULL }
};

#define SMM_SPECIAL_PRIMITIVE_COUNT 4

#define MAX_TYPE_IDENTS             sizeof(TYPE_IDENTS) / sizeof(typeidents)
#define MAX_USABLE_TYPE_IDENTS      (MAX_TYPE_IDENTS - SMM_SPECIAL_PRIMITIVE_COUNT)

WCHAR SmmpScriptWorkDir[MAX_PATH] = { 0 };
BOOL SmmpWorkDirIsLocal = TRUE;

PSLIST  SmmpUserTypeList = NULL, SmmpFnSignList = NULL;
PSLIST SmmpAliasList = NULL;
PSLIST  SmmpTypeList = NULL;
PDMA    SmmpParseErrorContent = NULL;

WORKDIR_STACK SmmpWorkDirStack;

#include <rtf.h>

#define RTFC_DEFAULT        0
#define RTFC_DARKRED        1
#define RTFC_DARKBLUE       2
#define RTFC_LIGHTBLUE      3
#define RTFC_LIGHTGREEN     4

#define RTFF_DEFAULT        0


void SmmpRegisterRtfColorAndFonts(Rtf * data)
{
    //Build rtf color table
    data->RegisterColor(RGB(185, 0, 0)); //DARKRED
    data->RegisterColor(RGB(0, 0, 139)); //DARKBLUE
    data->RegisterColor(RGB(0, 77, 187)); //LIGHTBLUE
    data->RegisterColor(RGB(0, 176, 80)); //LIGHTGREEN

    data->RegisterFont("Segoe UI", 0);
}

const char *FAIL_MESSAGES[] =
{
    "Memory alloc error",
    "",
    "",
    "",
    /////////////
    "maptype keyword expected",
    "expected 'typename'",
    "expected '{'",
    "maptype body definition expected",
    "expected '}'",
    /////////////////////
    "field type is not valid",
    "expected 'field name'",
    "expected '['",
    "expected ']",
    "expected array size",
    "expected ';' to complete field definition",
    ///////////////
    "expected '!'",
    "expected '('",
    "expected ')'",
    "type expected",
    "argument name expected",
    "module name expected",
    "unexpected token",
    "unexpected syntax ending",
    "Type not found",
    "filepath expected for @incl.",
	"identifier expected."
};


void SmmpInitWorkdirStack()
{
    memset(SmmpWorkDirStack.Stack, 0, sizeof(SmmpWorkDirStack.Stack));
    SmmpWorkDirStack.spi = SMMP_WORKDIR_STACKSIZE;
}

void SmmpPushWorkdir()
{
    WORD spi = SmmpWorkDirStack.spi - 1;

    //Ps. Dont be confused. WORD is an unsinged type and
    //when it gets a less than zero value for exam (-1)
    //It's sign bit will be overflowed, and its evaluated as WORD's max value
    //That means the spi value will be greater than WORKDIR_STACKSIZE,
    //And We will know that the stack is full
    if (spi > SMMP_WORKDIR_STACKSIZE)
        return;

    SmmpWorkDirStack.Stack[spi].isLocal = !HlpBeginsWithW(SmmpScriptWorkDir, L"http", FALSE, 4);
    wcscpy(SmmpWorkDirStack.Stack[spi].workDir, SmmpScriptWorkDir);
    SmmpWorkDirStack.spi--;
}

BOOL SmmpIsPreviousWorkdirLocal()
{

    if (SmmpWorkDirStack.spi == SMMP_WORKDIR_STACKSIZE)
        return FALSE;

    return SmmpWorkDirStack.Stack[SmmpWorkDirStack.spi + 1].isLocal;
}

BOOL SmmpPopWorkdir(BOOL *isLocal)
{
    WORD spi = SmmpWorkDirStack.spi;

    if (spi == SMMP_WORKDIR_STACKSIZE)
        return FALSE;

    wcscpy(SmmpScriptWorkDir, SmmpWorkDirStack.Stack[spi].workDir);
    *isLocal = SmmpWorkDirStack.Stack[spi].isLocal;
    SmmpWorkDirIsLocal = *isLocal;
    SmmpWorkDirStack.spi++;

    return TRUE;
}

void SmmpRaiseParseError(LONG err)
{
    const LONG totalErrString = sizeof(FAIL_MESSAGES) / sizeof(char *);
    
    if (err >= 0 && err <= totalErrString)
    {
        if (SmmpParseErrorContent)
            DmaStringWriteA(SmmpParseErrorContent, "%s\r\n", FAIL_MESSAGES[err - 1]);
        else
            DBGPRINT2(FAIL_MESSAGES[err - 1]);
    }
}

BOOL SmmpCreateSLIST(PSLIST *list)
{
    *list = ALLOCOBJECT(SLIST);

    if (*list != NULL)
    {
        RtlZeroMemory(*list, sizeof(SLIST));
        return TRUE;
    }

    return FALSE;
}

BOOL SmmpAddSList(PSLIST list, void *value)
{
    PSLISTNODE node;

    node = ALLOCOBJECT(SLISTNODE);

    if (!node)
        return FALSE;

    node->value = value;
    node->next = NULL;

    if (list->count == 0)
    {
        list->head = list->tail = node;
        list->count = 1;
        return TRUE;
    }

    list->tail->next = node;
    list->tail = node;
    list->count++;

    return TRUE;
}

void SmmpDestroySList(PSLIST list)
{
    PSLISTNODE tmp;

    while (list->head != NULL)
    {
        tmp = list->head->next;
        FREEOBJECT(list->head);
        list->head = tmp;
    }

    FREEOBJECT(list);
}


BOOL SmmCreateMapTypeInfo(const char *name, WORD size, BOOL isSigned, CONVERTER_METHOD method, WORD *index)
{
    PPRIMITIVETYPEINFO pmti = NULL;

    if (strcmp(TYPE_IDENTS[*index].identName, name))
        return FALSE;

    pmti = ALLOCOBJECT(PRIMITIVETYPEINFO);

    if (!pmti)
        return FALSE;

    pmti->cbSize = sizeof(PRIMITIVETYPEINFO);
    strcpy(pmti->typeName, name);
    pmti->size = size;
    pmti->isSigned = isSigned;

    TYPE_IDENTS[*index].pti = pmti;

    pmti->linkIndex = *index;
    pmti->method = method;

    (*index)++;

    SmmpAddSList(SmmpTypeList, pmti);

    return TRUE;
}

#include <stdio.h>

#define VALOFPTR(type, ptr) (*((type *)ptr))

#ifdef MAP_LOCAL
#define IMPLEMENT_CONVERTER(name, type, format) 
#else
#define IMPLEMENT_CONVERTER(name, type,format) void Convert ## name  (char *buf, void *mem, bool isValue) { \
                                                type tval; \
                                                if (!isValue) \
                                                    AbMemReadGuaranteed((duint)mem,&tval,sizeof(tval)); \
                                                else \
                                                    tval = (type)mem; \
                                                sprintf(buf,format,tval); \
                                            } 

#endif

#pragma warning(disable:4477)

IMPLEMENT_CONVERTER(Int, int, "%d")
IMPLEMENT_CONVERTER(Uint, unsigned int, "%du")
IMPLEMENT_CONVERTER(Char, CHAR, "%c")
IMPLEMENT_CONVERTER(Wchar, WCHAR, "%c")
IMPLEMENT_CONVERTER(Byte, CHAR, "%d")
IMPLEMENT_CONVERTER(UByte, UCHAR, "%du")
IMPLEMENT_CONVERTER(Short, SHORT, "%d")
IMPLEMENT_CONVERTER(UShort, USHORT, "%du")
IMPLEMENT_CONVERTER(Long, LONG, "%ld")
IMPLEMENT_CONVERTER(ULong, ULONG, "%lu")
IMPLEMENT_CONVERTER(Long64, LONGLONG, "%lld")
IMPLEMENT_CONVERTER(ULong64, ULONGLONG, "%llu")
IMPLEMENT_CONVERTER(Pointer, ARCHWIDE, "%p")


#ifdef _WIN64
IMPLEMENT_CONVERTER(ArchHex, ULONGLONG,"%016x")
#else
IMPLEMENT_CONVERTER(ArchHex, ULONG, "%08x")
#endif

void ConvertString(char *buf, void *mem, bool dummy)
{
#ifdef MAP_LOCAL
    sprintf(buf, "\"%s\"", ((LPSTR)mem));
#else
    char lbuf[512];
    
    if (!DbgGetStringAt((duint)mem, lbuf))
        return;

    sprintf(buf, "\"%s\"", lbuf);
#endif
}

void ConvertStringW(char *buf, void *mem, bool dummy)
{
#ifdef MAP_LOCAL
    ULONG slen;
    wchar_t exBuf[512];

    slen = wsprintf(exBuf, L"L\"%s\"", (LPWSTR)mem);

    if (!slen)
    {
        *buf = '\0';
        return;
    }

    if (!buf)
        return;

    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, exBuf, slen, buf, slen, NULL, NULL);
#else
    char lbuf[512];

    if (!DbgGetStringAt((duint)mem, lbuf))
        return;

    sprintf(buf, "%s", lbuf);
#endif
}



BOOL SmmpTypeIsPrimitive(LPVOID typeData, BOOL *isPrimitive)
{
	PGENERIC_DATATYPE_INFO gdti = (PGENERIC_DATATYPE_INFO)typeData;

	if (gdti->cbSize == sizeof(PRIMITIVETYPEINFO))
	{
		*isPrimitive = TRUE;
		return TRUE;
	}
	else if (gdti->cbSize == sizeof(STRUCTINFO))
	{
		*isPrimitive = FALSE;
		return TRUE;
	}

	return FALSE;
}

BOOL SmmpGetTypeInfoByName(const char *name, PVOID *info, BOOL *isPrimitive, BOOL dontCheckForUserTypes)
{
	BOOL infoPtrPresent = info != NULL;
	BOOL isPrimitivePtrPresent = isPrimitive != NULL;
	
	//Search for primitives.
    for (DWORD nx=0; nx < MAX_TYPE_IDENTS; nx++)
    {
        if (!strcmp(TYPE_IDENTS[nx].identName, name))
        {
            if (infoPtrPresent)
                *info = TYPE_IDENTS[nx].pti;
            
			if (isPrimitivePtrPresent)
				*isPrimitive = TRUE;

            return TRUE;
        }
    }

	//scan the alias list.
	for (PSLISTNODE node = SmmpAliasList->head; node != NULL; node = node->next)
	{
		if (!strcmp(RECORD_OF(node, PTYPEALIAS)->alias, name))
		{
			if (infoPtrPresent)
				*info = RECORD_OF(node, PTYPEALIAS)->mappedType;

			if (isPrimitivePtrPresent)
				SmmpTypeIsPrimitive(*info, isPrimitive);

			return TRUE;
		}
	}

	if (dontCheckForUserTypes)
	{
		if (isPrimitivePtrPresent)
			*isPrimitive = FALSE;

		return FALSE;
	}

	if (isPrimitivePtrPresent)
		*isPrimitive = FALSE;

    for (PSLISTNODE node = SmmpUserTypeList->head; node != NULL; node = node->next)
    {
        if (!strcmp(RECORD_OF(node, PSTRUCTINFO)->name, name))
        {
			if (infoPtrPresent)
				*info = RECORD_OF(node, PSTRUCTINFO);
            
			return TRUE;
        }
    }

    return FALSE;
}

BOOL SmmpRegisterAlias(const char *alias, const char *typeName)
{
	PTYPEALIAS palias = NULL;
	PVOID type;

	if (!SmmpGetTypeInfoByName(typeName, &type, NULL, FALSE))
		return FALSE;

	palias = ALLOCOBJECT(TYPEALIAS);

	if (!palias)
		return FALSE;

	if (strlen(alias) + 1 > 64)
	{
		FREEOBJECT(palias);
		return FALSE;
	}

	strcpy(palias->alias, alias);
	palias->mappedType = type;

	SmmpAddSList(SmmpAliasList, palias);

	return TRUE;
}

BOOL SmmpInitBaseAliases()
{
	LONG success = 0;

	success += SmmpRegisterAlias("BYTE", "ubyte");
	success += SmmpRegisterAlias("WORD", "ushort");
	success += SmmpRegisterAlias("DWORD", "ulong");
	success += SmmpRegisterAlias("QWORD", "ulong64");

	return success;
}

BOOL SmmpInitBasePrimitiveTypes()
{
    LONG success = 0;
    WORD index = 0;

    if (TYPE_IDENTS[0].pti != NULL)
        return TRUE;

    success += (LONG)SmmCreateMapTypeInfo("int", SIZEOF_INT, FALSE, ConvertInt, &index);
    success += (LONG)SmmCreateMapTypeInfo("uint", SIZEOF_INT, TRUE, ConvertUint, &index);

    success += (LONG)SmmCreateMapTypeInfo("char", SIZEOF_CHAR, FALSE, ConvertChar, &index);

    success += (LONG)SmmCreateMapTypeInfo("wchar", SIZEOF_WCHAR, FALSE, ConvertWchar, &index);

    success += (LONG)SmmCreateMapTypeInfo("byte", SIZEOF_BYTE, FALSE, ConvertByte, &index);
    success += (LONG)SmmCreateMapTypeInfo("ubyte", SIZEOF_BYTE, TRUE, ConvertUByte, &index);

    success += (LONG)SmmCreateMapTypeInfo("short", SIZEOF_SHORT, FALSE, ConvertShort, &index);
    success += (LONG)SmmCreateMapTypeInfo("ushort", SIZEOF_SHORT, FALSE, ConvertUShort, &index);

    success += (LONG)SmmCreateMapTypeInfo("long", SIZEOF_LONG, FALSE, ConvertLong, &index);
    success += (LONG)SmmCreateMapTypeInfo("ulong", SIZEOF_LONG, FALSE, ConvertULong, &index);

    success += (LONG)SmmCreateMapTypeInfo("long64", SIZEOF_LONGLONG, FALSE, ConvertLong64, &index);
    success += (LONG)SmmCreateMapTypeInfo("ulong64", SIZEOF_LONGLONG, FALSE, ConvertULong64, &index);

    success += (LONG)SmmCreateMapTypeInfo("string", SIZEOF_INT, FALSE, ConvertString, &index);
    success += (LONG)SmmCreateMapTypeInfo("wstring", SIZEOF_INT, FALSE, ConvertStringW, &index);

    success += (LONG)SmmCreateMapTypeInfo("pointer", SIZEOF_INT, FALSE, ConvertPointer, &index);

    success += (LONG)SmmCreateMapTypeInfo("archex", SIZEOF_POINTER, FALSE, ConvertArchHex, &index);


    if (success != index)
    {
        return FALSE;
    }

    return TRUE;
}

//maptype NAME {
//typename fieldname1;
//typename fieldname2;
//typename[2] fieldname3;
//typename * fieldname4;
//}




__forceinline BOOL SmmpNextnode(PSLISTNODE *node)
{
    if ((*node)->next == NULL)
        return FALSE;

    (*node) = (*node)->next;
    return TRUE;
}

#define Stringof(node) ( RECORD_OF((node), char *) )

#define OneWayExit(fs, exitLabel)  { failStep = fs; goto exitLabel; }

#define IsCurlyBrckOpen(node) (*Stringof(node) == '{')
#define IsCurlyBrckClose(node) (*Stringof(node) == '}')

#define IsSquareBrckOpen(node) (*Stringof(node) == '[')
#define IsSquareBrckClose(node) (*Stringof(node) == ']')

#define IsPtrChar(node) (*Stringof(node) == '*')

#define IsSemiColon(node) (*Stringof(node) == ';')


BOOL SmmpIsSpecialToken(char chr)
{
    switch (chr)
    {
    case '(':
    case ')':
    case '[':
    case ']':
    case '{':
    case '}':
    case '*':
    case ';':
    case ',':
    case '!':
        return TRUE;
    }

    return FALSE;
}

BOOL SmmpIsNumeric(char c)
{
    return c >= '0' && c <= '9';
}

BOOL SmmpIsWhitespace(char c)
{
    switch (c)
    {
    case '\r':
    case '\n':
    case '\t':
    case ' ':
        return TRUE;
    }

    return FALSE;
}

BOOL SmmpIsChar(char c)
{
    return !SmmpIsWhitespace(c) && !SmmpIsNumeric(c) && !SmmpIsSpecialToken(c);
}

#define SmmpPushToken() { \
                            tok = ALLOCSTRINGA(bufi); \
                            strcpy(tok, buf); \
                            memset(buf, 0, sizeof(buf)); \
                            bufi = 0; \
                            SmmpAddSList(tokList, tok); \
                        }

BOOL SmmpTokenize(LPCSTR typeDefString, PSLIST *tokenList)
{
    char buf[512] = { 0 };
    const char *p = typeDefString;
    int bufi = 0;
    char *tok;
    BOOL numLoad = FALSE;
	BOOL skip = FALSE;

    PSLIST tokList;

    if (!SmmpCreateSLIST(&tokList))
        return FALSE;


    while (*p)
    {
		if (skip)
		{
			if (*p == '*' && *(p + 1) == '/')
			{
				p++;
				skip = FALSE;
			}

			p++;
			continue;
		}

        if (SmmpIsSpecialToken(*p))
        {
            if (bufi > 0)
                SmmpPushToken();

            tok = ALLOCSTRINGA(4);
            *tok = *p;
            *(tok + 1) = 0;

            SmmpAddSList(tokList, tok);
        }
        else if (SmmpIsWhitespace(*p))
        {
            if (bufi > 0)
                SmmpPushToken();
        }
        else
        {
			//comment begin
			if (!skip && *p == '/' && *(p+1) == '*')
			{
				p++;
				skip = TRUE;
			}
			else
				buf[bufi++] = *p;
        }

        p++;
    }

    if (bufi > 0)
        SmmpPushToken();

    *tokenList = tokList;

    return TRUE;
}

BOOL SmmpParseTypeField(PSLISTNODE *beginNode, PSTRUCTMEMBERFIELD *fieldPtr)
{
    PSLISTNODE node = NULL;
    PPRIMITIVETYPEINFO pmti = NULL;
    PSTRUCTINFO pstr = NULL;

    PSTRUCTMEMBERFIELD field = NULL;

    BOOL success = FALSE;
    LONG failStep = 0;

    node = *beginNode;

    PVOID info;
    BOOL isPrimitive;

    if (!SmmpGetTypeInfoByName(Stringof(node), &info, &isPrimitive,FALSE))
        OneWayExit(FS_NOFIELDTYPE, exit);

    field = ALLOCOBJECT(STRUCTMEMBERFIELD);
    memset(field, 0, sizeof(STRUCTMEMBERFIELD));


    if (!field)
        OneWayExit(FS_MEMERR, exit);

    if (isPrimitive)
        field->type.primitiveType = (PPRIMITIVETYPEINFO)info;
    else
    {
        field->type.structType = (PSTRUCTINFO)info;
        field->isStruct = TRUE;
    }

    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTFIELDNAME, exit);

    if (IsSquareBrckOpen(node))
    {
        if (!SmmpNextnode(&node))
            OneWayExit(FS_EXPECTARRSIZE, exit);

        if (!IsSquareBrckClose(node))
        {
            field->arrSize = atoi(Stringof(node));

            if (field->arrSize == 0)
                OneWayExit(FS_EXPECTARRSIZE, exit);

            if (!SmmpNextnode(&node))
                OneWayExit(FS_EXPECTSQUBRCKCLOSE, exit);

            field->isArray = TRUE;

        }
        else
        {
            field->arrSize = 1;
            field->isArray = TRUE;
        }


        if (!SmmpNextnode(&node))
            OneWayExit(FS_EXPECTFIELDNAME, exit);

    }
    else if (IsPtrChar(node))
    {
        field->isPointer = TRUE;

        if (!SmmpNextnode(&node))
            OneWayExit(FS_EXPECTFIELDNAME, exit);
    }

    if (SmmpIsSpecialToken(*Stringof(node)))
        OneWayExit(FS_EXPECTFIELDNAME, exit);

    strcpy(field->fieldName, Stringof(node));


    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTSEMICOLON, exit);

    if (!IsSemiColon(node))
        OneWayExit(FS_EXPECTSEMICOLON, exit);

    if (!SmmpNextnode(&node))
        OneWayExit(-1, exit);

    success = TRUE;

exit:

    *beginNode = node;
    *fieldPtr = field;


    if (!success)
        SmmpRaiseParseError(failStep);

    return success;
}

WORD SmmpParseTypeFields(PSLISTNODE *beginNode, PSTRUCTINFO typeInfo)
{
    PSLISTNODE node;
    PSTRUCTMEMBERFIELD field;

    BOOL success = FALSE;

    node = *beginNode;

    if (!SmmpCreateSLIST(&typeInfo->fields))
        return 0;

    while (node != NULL)
    {
        if (IsCurlyBrckClose(node))
            break;

        if (SmmpParseTypeField(&node, &field))
        {
            SmmpAddSList(typeInfo->fields, field);

            if (field->isArray)
            {
                if (field->isStruct)
                    typeInfo->structSize += field->type.structType->structSize * field->arrSize;
                else
                    typeInfo->structSize += field->type.primitiveType->size * field->arrSize;
            }
            else if (field->isPointer)
                typeInfo->structSize += SIZEOF_POINTER;
            else
            {
                if (field->isStruct)
                    typeInfo->structSize += field->type.structType->structSize;
                else
                    typeInfo->structSize += field->type.primitiveType->size;
            }
        }
        else
            break;
    }

    *beginNode = node;

    return (WORD)typeInfo->fields->count;
}

#define ARG_IN      1
#define ARG_OUT     2

BOOL SmmpParseFunctionSignature(PSLISTNODE *startNode, PFNSIGN *funcSign)
{
	char name[128] = { 0 }, module[128] = { 0 }, buf[128] = { 0 };
    char argType[64], argName[128];
    PARGINFO argList = NULL;
	PRETINFO retInfo;
    SHORT argCount = 0;
    PFNSIGN fnSign = NULL;
	PVOID argTypeInfo = NULL, retTypeInfo = NULL;
    BOOL isPrimitive=FALSE,isPtr=FALSE;
    int argInOut = 0,failStep=0;

    PSLISTNODE node = (*startNode)->next;

	if (SmmpGetTypeInfoByName(Stringof(node), &retTypeInfo, NULL, FALSE))
	{
		retInfo = ALLOCOBJECT(RETINFO);

		if (!retInfo)
		{
			RAISEGLOBALERROR("Memory allocation failed.");
			return FALSE;
		}

		retInfo->typeInfo.u = retTypeInfo;

		if (!SmmpNextnode(&node))
			OneWayExit(FS_EXPECTMODNAME, finish);
	}


    strcpy(module, Stringof(node));

    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTNOTOP, finish);

    if (*Stringof(node) != '!')
        OneWayExit(FS_EXPECTNOTOP, finish);

    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTMODNAME, finish);

    strcpy(name, Stringof(node));

    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTPARANOPEN, finish);

    if (*Stringof(node) != '(')
        OneWayExit(FS_EXPECTPARANOPEN, finish);

    argList = ALLOCOBJECTLIST(ARGINFO, 10);
    fnSign = ALLOCOBJECT(FNSIGN);


    DBGPRINT("parsing signature %s!%s", module, name);

    if (SmmpIsSpecialToken(*Stringof(node)))
    {
        if (*Stringof(node) == ')')
        {
            FREEOBJECT(argList);
            argList = NULL;
            argCount = 0;

            goto finish;
        }
    }

    do
    {
        if (!SmmpNextnode(&node))
            OneWayExit(FS_UNEXPECTED_SYTX_END, finish);

        memset(argType, 0, sizeof(argType));
        memset(argName, 0, sizeof(argName));
        argInOut = 0;
        isPtr = FALSE;

        if (SmmpIsSpecialToken(*Stringof(node)))
            OneWayExit(FS_UNEXPECTED_TOKEN, finish);

        if (!_stricmp(Stringof(node), "out"))
            argInOut = ARG_OUT;
        else if (!_stricmp(Stringof(node), "in"))
            argInOut = ARG_IN;

        if (argInOut > 0)
        {
            if (!SmmpNextnode(&node))
                OneWayExit(FS_EXPECTTYPE, finish);

            if (SmmpIsSpecialToken(*Stringof(node)))
                OneWayExit(FS_EXPECTTYPE, finish);
        }

        strcpy(argType, Stringof(node));

        if (!SmmpNextnode(&node))
            OneWayExit(FS_EXPECTARGNAME, finish);

        if (IsPtrChar(node))
        {
            isPtr = TRUE;

            if (!SmmpNextnode(&node))
                OneWayExit(FS_EXPECTARGNAME, finish);

            if (SmmpIsSpecialToken(*Stringof(node)))
                OneWayExit(FS_EXPECTARGNAME, finish);
        }


        strcpy(argName, Stringof(node));

        if (!SmmpNextnode(&node))
            OneWayExit(FS_UNEXPECTED_SYTX_END, finish);

        strcpy(argList[argCount].name, argName);

        if (!SmmpGetTypeInfoByName(argType, &argTypeInfo, &isPrimitive,FALSE))
        {
            OneWayExit(FS_TYPENOTFOUND, finish);
        }

        argList[argCount].isStructure = !isPrimitive;
        argList[argCount].typeInfo.holder = argTypeInfo;
        argList[argCount].isOutArg = argInOut == ARG_OUT;
        argList[argCount].isPointer = isPtr;

        DBGPRINT("name: %s", argName);
        DBGPRINT("isStruct?: %d", !isPrimitive);
        DBGPRINT("isOut?: %d", argInOut == ARG_OUT);
        DBGPRINT("isPointer?: %d", isPtr);
        DBGPRINT("\n");

        if (argInOut == ARG_OUT && !fnSign->hasOutArg)
            fnSign->hasOutArg = TRUE;

        argCount++;

    } while (*Stringof(node) == ',');

    if (*Stringof(node) != ')')
    {
        OneWayExit(FS_EXPECTPARANCLOSE, finish);
    }

    if (!SmmpNextnode(&node))
        node = NULL;

finish:

    if (failStep > 0)
    {
        SmmpRaiseParseError(failStep);

        if (fnSign != NULL)
            FREEOBJECT(fnSign);

        if (argList != NULL)
            FREEOBJECT(argList);

        return FALSE;
    }

    fnSign->argCount = argCount;
    fnSign->args = argList;
	fnSign->ret = retInfo;
    strcpy(fnSign->name, name);
    strcpy(fnSign->module, module);

    *funcSign = fnSign;

    *startNode = node;

    return TRUE;
}

BOOL SmmpParseAlias(PSLISTNODE *startNode)
{
	PSLISTNODE node = *startNode;
	char *realType, *aliasName;

	if (strcmp(Stringof(node), "alias"))
		return FALSE;

	if (!SmmpNextnode(&node))
	{
		SmmpRaiseParseError(FS_EXPECTTYPE);
		return FALSE;
	}

	realType = Stringof(node);

	if (!SmmpNextnode(&node))
	{
		SmmpRaiseParseError(FS_EXPECTIDENT);
		return FALSE;
	}

	aliasName = Stringof(node);

	if (!SmmpRegisterAlias(aliasName, realType))
	{
		SmmpRaiseParseError(FS_TYPENOTFOUND);
		return FALSE;
	}
	
	if (!SmmpNextnode(&node))
		node = NULL;

	*startNode = node;

	return TRUE;
}

BOOL _SmmpParseType(PSLISTNODE *startNode, PSTRUCTINFO *typeInfo)
{
    PSLISTNODE node = NULL;
    PSTRUCTINFO pti = NULL;
    BOOL success = FALSE;
    LONG failStep = 0;

    node = *startNode;

    if (strcmp(Stringof(node), "maptype"))
    {
        OneWayExit(FS_MAPTYPE, exit);
    }

    if (!SmmpNextnode(&node))
        OneWayExit(FS_TYPENAME, exit); //typename expected

    pti = ALLOCOBJECT(STRUCTINFO);

    if (!pti)
        OneWayExit(FS_MEMERR, exit);

    memset(pti, 0, sizeof(STRUCTINFO));
    pti->cbSize = sizeof(STRUCTINFO);

    if (SmmpIsSpecialToken(*Stringof(node)))
        OneWayExit(FS_TYPENAME, exit);

    strcpy(pti->name, Stringof(node));

    if (!SmmpNextnode(&node))
        OneWayExit(FS_BRACKOPEN, exit); //{ Expected

    if (!IsCurlyBrckOpen(node))
        OneWayExit(FS_BRACKOPEN, exit); //{ expected

    if (!SmmpNextnode(&node))
        OneWayExit(FS_EXPECTFIELD, exit); //Field definition expected

                                          //Compile fields

    if (!SmmpParseTypeFields(&node, pti))
        OneWayExit(FS_EXPECTFIELD, exit);

    if (!IsCurlyBrckClose(node))
        OneWayExit(FS_BRACKCLOSE, exit); //} expected

    if (!SmmpNextnode(&node))
        node = NULL;

    success = TRUE;
exit:

    *startNode = node;

    if (!success)
        SmmpRaiseParseError(failStep);

    *typeInfo = pti;

    return success;

}


void SmmpDumpMapTypeInfo(PPRIMITIVETYPEINFO mti)
{
    _DBGPRINT("Typename: %s\n", mti->typeName);
    _DBGPRINT("Size: %d\n", mti->size);
    _DBGPRINT("Signed: %d\n", mti->isSigned);
}

void SmmpDumpType(PSTRUCTINFO type)
{
    PSTRUCTMEMBERFIELD field;

    _DBGPRINT("Name: %s\n", type->name);
    _DBGPRINT("---Fields---\n\n");

    for (PSLISTNODE n = type->fields->head; n != NULL; n = n->next)
    {
        field = RECORD_OF(n, PSTRUCTMEMBERFIELD);

        if (field->isStruct)
        {
            SmmpDumpType(field->type.structType);
            return;
        }

        _DBGPRINT("Name: %s\n", field->fieldName);
        SmmpDumpMapTypeInfo(field->type.primitiveType);
        _DBGPRINT("Is Array: %d\n", field->isArray);

        if (field->isArray)
            _DBGPRINT("Array size: %d\n", field->arrSize);

        _DBGPRINT("Is pointer: %d\n\n", field->isPointer);
    }
}

//TODO: detect value or not value?
BOOL SmmpMapForPrimitiveType(PGENERIC_DATATYPE_INFO pdi, Rtf *rtf, BYTE *mem, SHORT lastDepth, PCOMMON_DATAFIELD_INFO pcdi)
{
	PPRIMITIVETYPEINFO pmti;
	BOOL pcdiPresent = pcdi != NULL;
	BOOL isPointer = FALSE, isArray = FALSE;
	ULONG arraySize = 0;
	char buffer[1024];
    BOOL result = TRUE;
	bool isValue = false;

	if (pcdiPresent)
	{
		switch (pcdi->type)
		{
		case CDI_FUNCTION_ARGUMENT:
			if (!pcdi->u.argInfo->isStructure)
				pmti = pcdi->u.argInfo->typeInfo.primitiveInfo;
			else
				return FALSE;

			isPointer = pcdi->u.argInfo->isPointer;
			isArray = FALSE;
			arraySize = 0;
			break;
		case CDI_STRUCTURE_FIELD:
			if (!pcdi->u.fieldInfo->isStruct)
				pmti = pcdi->u.fieldInfo->type.primitiveType;
			else
				return FALSE;

			isPointer = pcdi->u.fieldInfo->isPointer;
			isArray = pcdi->u.fieldInfo->isArray;
			arraySize = isArray ? pcdi->u.fieldInfo->arrSize : 0;
			break;
		default:
			return FALSE;
		}
	}
	else
		pmti = (PPRIMITIVETYPEINFO)pdi;
	//Ignore return
	
    if (isPointer)
    {
		//'*' marked pointer type. we need to dereference and map
		isValue = false;

        if (!strcmp(pdi->typeName, "char"))
            pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 3].pti; //assume string converter
        else if (!strcmp(pdi->typeName, "wchar"))
            pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 2].pti; //assume wstring converter
        else if (!strcmp(pdi->typeName, "pointer"))
            pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 1].pti; //pointer itself converter

        pmti->method(buffer, mem, isValue);
    }
    else if (isArray)
    {
		//if char[] or wchar[] we can assume as string again
        if (!strcmp(pdi->typeName, "char"))
            pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 3].pti;
        else if (!strcmp(pdi->typeName, "wchar"))
            pmti = TYPE_IDENTS[MAX_TYPE_IDENTS - 2].pti;
        else
        {
			BYTE *arrPtr = (BYTE *)mem;

            for (ULONG i = 0;i < arraySize;i++)
            {
				//Map n. index of memory 
                pmti->method(buffer, arrPtr, false);

				//increment pointer type of array memory
				mem += pmti->size;
            }

            memset(buffer, 0, sizeof(buffer));

            goto oneWayExit;

        }
    }
	else
	{
		if (!strcmp(pdi->typeName, "string") || !strcmp(pdi->typeName, "wstring"))
			isValue = false;
		else
			isValue = true;

		pmti->method(buffer, mem, isValue);
	}

oneWayExit:

    if (result)
    {
        rtf->Color(RTFC_DEFAULT)->FormatText(buffer);
    }

    return result;
}

BOOL SmmpMapForStructureType(PGENERIC_DATATYPE_INFO pdi, Rtf *rtf, BYTE *mem, SHORT lastDepth, PCOMMON_DATAFIELD_INFO pcdi);

BOOL SmmpMapField(PSTRUCTMEMBERFIELD ptif, Rtf* rtf, BYTE *mem,SHORT lastDepth)
{
	COMMON_DATAFIELD_INFO cdi;
    PPRIMITIVETYPEINFO pmti = NULL;
    BYTE *memArr = NULL;

	cdi.type = CDI_STRUCTURE_FIELD;
	cdi.u.fieldInfo = ptif;

	if (ptif->isStruct)
		return SmmpMapForStructureType((PGENERIC_DATATYPE_INFO)ptif->type.structType, rtf, mem, lastDepth + 1, &cdi);

	return SmmpMapForPrimitiveType((PGENERIC_DATATYPE_INFO)ptif->type.primitiveType, rtf, mem, lastDepth + 1, &cdi);
}


BOOL SmmpMapForStructureType(PGENERIC_DATATYPE_INFO pdi, Rtf *rtf, BYTE *mem, SHORT lastDepth, PCOMMON_DATAFIELD_INFO pcdi)
{
    PSTRUCTMEMBERFIELD ptif = NULL;
	PSTRUCTINFO psi = (PSTRUCTINFO)pdi;

    DWORD typeSize = 0;

    char buf[512];

    rtf->FormatText("(Struct of %s) = ", psi->name)->NewLine(1);

    rtf->NewTab(lastDepth)->FormatText("[")->NewLine(1);

    for (PSLISTNODE node = psi->fields->head; node != NULL; node = node->next)
    {
        ptif = RECORD_OF(node, PSTRUCTMEMBERFIELD);
        memset(buf, 0, sizeof(buf));

        rtf->Style(RTFS_TAB, lastDepth + 1)->FormatText("%s = ", ptif->fieldName);
        
        if (!SmmpMapField(ptif, rtf, mem,lastDepth))
            return FALSE;

        rtf->Style(RTFS_NEWLINE);

        typeSize = ptif->isStruct ? ptif->type.structType->structSize : ptif->type.primitiveType->size;

        if (ptif->isArray)
            mem += typeSize * ptif->arrSize;
        else if (ptif->isPointer)
            mem += SIZEOF_POINTER;
        else
            mem += typeSize;


    }

    rtf->NewTab(lastDepth)->FormatText("]")->NewLine(1);

    return TRUE;
}


BOOL SmmMapFunctionCall(PPASSED_PARAMETER_CONTEXT passedParams, PFNSIGN fnSign, ApiFunctionInfo *afi, LPSTR *mapResult)
{
    PARGINFO argInfo;
    BOOL isPrimitive;
    BYTE *mem;
    PGENERIC_DATATYPE_INFO pdi;
	COMMON_DATAFIELD_INFO cdi;
    Rtf *rtf;

    rtf = new Rtf();

    SmmpRegisterRtfColorAndFonts(rtf);

    if (_stricmp(fnSign->module, afi->ownerModule->name) || _stricmp(fnSign->name, afi->name))
        return FALSE;

    rtf->Style(RTFS_BOLD)->Font(RTFF_DEFAULT)->FontSize(22)
        ->FormatText("Map for ")->Color(RTFC_DARKBLUE)
        ->FormatText("%s", fnSign->module)->Color(RTFC_DEFAULT)
        ->FormatText("!")->Color(RTFC_DARKRED)
        ->FormatText("%s", fnSign->name)->Color(RTFC_DEFAULT)
        ->FormatText("(")->NewLine(1);

    for (int i = 0;i < fnSign->argCount;i++)
    {
        argInfo = &fnSign->args[i];

        pdi = (PGENERIC_DATATYPE_INFO)argInfo->typeInfo.holder;

		isPrimitive = pdi->cbSize == sizeof(PRIMITIVETYPEINFO);

        rtf->Style(RTFS_TAB, 1)->Color(RTFC_DEFAULT)
            ->FormatText("Arg#%d (", i + 1)->Color(RTFC_LIGHTGREEN)
            ->FormatText("Name")->Color(RTFC_DARKRED)
            ->FormatText(": %s, ", argInfo->name)
            ->Color(RTFC_LIGHTGREEN)->FormatText("Type")
            ->Color(RTFC_DEFAULT)->FormatText(": %s) = ", pdi->typeName);

		mem = (BYTE *)passedParams->paramList[i];

		cdi.type = CDI_FUNCTION_ARGUMENT;
		cdi.u.argInfo = argInfo;

		if (isPrimitive)
			SmmpMapForPrimitiveType(pdi, rtf, mem, 0, &cdi);
        else
        {
            //I need a special formatting for function map of the user types!!
            SmmpMapForStructureType(pdi, rtf, mem, 0,&cdi);
        }

        rtf->Style(RTFS_NEWLINE);
    }

    rtf->FormatText(");")->NewLine(2);


    *mapResult = rtf->GetRtf();

    delete rtf;

    return TRUE;
}

BOOL SmmMapMemoryForType(void *memory, ULONG size, const char *typeName)
{
    Rtf *rtf;
    BOOL result, isPrimitive;
	PGENERIC_DATATYPE_INFO pdi;

    rtf = new Rtf();

    for (PSLISTNODE node = SmmpTypeList->head; node != NULL; node = node->next)
    {
		pdi = RECORD_OF(node, PGENERIC_DATATYPE_INFO);

        if (!strcmp(pdi->typeName, typeName))
        {
			isPrimitive = pdi->cbSize == sizeof(PRIMITIVETYPEINFO);

			if (isPrimitive)
				result = SmmpMapForPrimitiveType(pdi, rtf, (BYTE *)memory, 0, NULL);
			else
				result = SmmpMapForStructureType(pdi, rtf, (BYTE *)memory, 0, NULL);

            break;
        }
    }


    delete rtf;

    return result;
}

BOOL SmmGetFunctionSignature(const char *module, const char *function, PFNSIGN *signInfo)
{
    PFNSIGN fnSign;

    if (!signInfo)
        return FALSE;

    for (PSLISTNODE node = SmmpFnSignList->head; node != NULL; node = node->next)
    {
        fnSign = RECORD_OF(node, PFNSIGN);

        if (!_stricmp(fnSign->module, module) && !_stricmp(fnSign->name, function))
        {
            *signInfo = fnSign;
            return TRUE;
        }
    }

    *signInfo = NULL;
    return FALSE;
}

BOOL SmmGetFunctionSignature2(ApiFunctionInfo *afi, PFNSIGN *signInfo)
{
    if (!afi)
        return FALSE;

    if (!signInfo)
        return FALSE;

    return SmmGetFunctionSignature(afi->ownerModule->name, afi->name, signInfo);
}

SHORT SmmGetArgumentCount(PFNSIGN signInfo)
{
    if (!signInfo)
        return -1;

    return signInfo->argCount;
}

BOOL SmmSigHasOutArgument(PFNSIGN signInfo)
{
    if (!signInfo)
        return FALSE;

    return signInfo->hasOutArg;
}

BOOL SmmpIsFullPathOrURL(LPCWSTR str, BOOL *isLocal)
{
    if (HlpBeginsWithW(str + 1, L":\\\\", TRUE, 3))
    {
        *isLocal = TRUE;
        return TRUE;
    }

    if (HlpBeginsWithW(str, L"http", FALSE, 4))
    {
        *isLocal = FALSE;
        return TRUE;
    }

    return FALSE;
}

void SmmpBuildNewFileOrURL(LPWSTR buffer, ULONG bufSize, LPWSTR objectName)
{
    BOOL isLocal;
    WCHAR sepFind, sepRepl;
    LPWSTR ptr;

    if (SmmpIsFullPathOrURL(objectName, &isLocal))
    {
        wcscpy(buffer, objectName);
        return;
    }


    SmmpIsFullPathOrURL(SmmpScriptWorkDir, &isLocal);


    if (!isLocal)
    {
        sepFind = '\\';
        sepRepl = '/';
    }
    else
    {
        sepFind = '/';
        sepRepl = '\\';
    }

    memset(buffer, 0, bufSize);

    wsprintfW(buffer, L"%s%s", SmmpScriptWorkDir, objectName);

    ptr = (LPWSTR)buffer;

    while (*ptr)
    {
        if (*ptr == sepFind)
            *ptr = sepRepl;
        ptr++;
    }

}

BOOL SmmpParseType(LPCSTR typeDefString, WORD *typeCount)
{
    PSLIST tokenList;
    PSLISTNODE node;
    PSTRUCTINFO typeInfo = NULL;
    PFNSIGN fnSign = NULL;
    WORD compiledTypeCount = 0;
    LONG err = -1;
    LPWSTR inclFileW = NULL;
    WCHAR fullInclFileW[MAX_PATH];

    if (!SmmpTokenize(typeDefString, &tokenList))
        return FALSE;

    node = tokenList->head;

    while (node != NULL)
    {
        if (!_stricmp(Stringof(node),"@incl"))
        {
            if (!SmmpNextnode(&node))
                err = FS_EXPECTINCLFILEPATH;

            if (!HlpTrimChar(Stringof(node), '\'', HLP_TRIM_BOTH))
                err = FS_EXPECTINCLFILEPATH;

            if (err >= 0)
            {
                SmmpRaiseParseError(err);
                return FALSE;
            }

            inclFileW = HlpAnsiToWideString(Stringof(node));

            if (!inclFileW)
                return FALSE;

            SmmpBuildNewFileOrURL(fullInclFileW, sizeof(fullInclFileW), inclFileW);
            
            FREESTRING(inclFileW);

            SmmParseFromFileW(fullInclFileW, typeCount);

            if (!SmmpNextnode(&node))
                node = NULL;

            continue;
        }

        if (!_stricmp(Stringof(node), "fnsign"))
        {
            if (!SmmpParseFunctionSignature(&node, &fnSign))
            {
                return FALSE;
            }

            SmmpAddSList(SmmpFnSignList, fnSign);

        }
        else if (!_stricmp(Stringof(node), "maptype"))
        {
            if (!_SmmpParseType(&node, &typeInfo))
            {
                //release all
                return FALSE;
            }

            SmmpAddSList(SmmpUserTypeList, typeInfo);
            SmmpAddSList(SmmpTypeList, typeInfo);

            compiledTypeCount++;

            SmmpDumpType(typeInfo);
        }
		else if (!_stricmp(Stringof(node), "alias"))
		{
			if (!SmmpParseAlias(&node))
				return FALSE;
		}

    }

    if (typeCount)
        *typeCount += compiledTypeCount;

    return TRUE;
}

BOOL SmmParseType(LPCSTR typeDefString, WORD *typeCount)
{
    BOOL success = FALSE;

    if (!typeCount)
        return FALSE;

    if (!SmmpFnSignList)
    {
        //TODO: cleanup on failure
        if (!SmmpCreateSLIST(&SmmpFnSignList))
            goto exit;

        if (!SmmpCreateSLIST(&SmmpTypeList))
            goto exit;

        if (!SmmpCreateSLIST(&SmmpUserTypeList))
            goto exit;

		if (!SmmpCreateSLIST(&SmmpAliasList))
			goto exit;

        if (!SmmpInitBasePrimitiveTypes())
            goto exit;

		if (!SmmpInitBaseAliases())
			goto exit;

        DmaCreateAdapter(sizeof(char), 64, &SmmpParseErrorContent);

        SmmpInitWorkdirStack();

        *typeCount = 0;
    }

    success = SmmpParseType(typeDefString,typeCount);

exit:

    if (!success)
        SmmReleaseResources();

    return success;
}

BOOL SmmpParseFromInternet(LPCWSTR sourceUrl, WORD *typeCount)
{
    BYTE* content;
    ULONG contentLen;
    BOOL result;

    if (!UtlInternetReadW(sourceUrl, &content, &contentLen))
        return FALSE;

    result = SmmParseType((LPCSTR)content, typeCount);

    if (result)
        AbMemoryFree(content);

    return result;
}

BOOL SmmParseFromFileW(LPCWSTR fileName, WORD *typeCount)
{
    HANDLE fileHandle = NULL;
    char *fileContent = NULL;
    DWORD fsLo, fsHi, read;
    BOOL result = FALSE;
    WCHAR workDir[MAX_PATH];
    BOOL isInetSource,workDirStacked=FALSE;

    isInetSource = HlpBeginsWithW(fileName, L"http", FALSE, 4);

    //Initial
    if (!SmmpFnSignList)
    {
        //change prototype to const wstr
        if (!HlpPathFromFilenameW((LPWSTR)fileName, SmmpScriptWorkDir, MAX_PATH,isInetSource?L'/':L'\\'))
            return FALSE;

        SmmpWorkDirIsLocal = !isInetSource;
    }
    else
    {
        HlpPathFromFilenameW((LPWSTR)fileName, workDir, MAX_PATH, isInetSource ? L'/' : L'\\');

        if (_wcsicmp(workDir, SmmpScriptWorkDir))
        {
            SmmpPushWorkdir();
            SmmpWorkDirIsLocal = !isInetSource;
            wcscpy(SmmpScriptWorkDir, workDir);
            workDirStacked = TRUE;
        }

    }

    if (isInetSource)
    {
        result = SmmpParseFromInternet((LPCWSTR)fileName, typeCount);

        if (workDirStacked)
        {
            SmmpPopWorkdir(&isInetSource);
        }

        return result;
    }

    
    fileHandle = CreateFileW(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (fileHandle == INVALID_HANDLE_VALUE)
        goto exit;

    fsLo = GetFileSize(fileHandle, &fsHi);

    if (fsHi > 0)
    {
        //Hollyshit. Are you really sure that is it contains plain text?
        goto exit;
    }

    //Is it also larger than 1Meg for a standart text file.
    if (fsLo >= 0x100000)
        goto exit;

    fileContent = (char *)AbMemoryAlloc(fsLo);

    if (!fileContent)
        goto exit;
    
    if (!ReadFile(fileHandle, fileContent, fsLo, &read, NULL))
        goto exit;

    result = SmmParseType(fileContent, typeCount);
exit:

    if (fileContent)
        AbMemoryFree(fileContent);

    CloseHandle(fileHandle);

    if (workDirStacked)
    {
        SmmpPopWorkdir(&isInetSource);
    }

    return result;
}

BOOL SmmParseFromFileA(LPCSTR fileName, WORD *typeCount)
{
    LPCWSTR fileNameW = NULL;
    BOOL result;

    fileNameW = HlpAnsiToWideString(fileName);

    if (!fileNameW)
        return FALSE;

    result = SmmParseFromFileW(fileNameW, typeCount);

    FREESTRING(fileNameW);

    return result;
}

BOOL SmmHasParseError(LPSTR *errorString)
{
    ARCHWIDE written = 0;

    if (!SmmpParseErrorContent)
        return FALSE;

    DmaGetAdapterInfo(SmmpParseErrorContent, &written, NULL);

    if (written > 0)
    {
        DmaTakeMemoryOwnership(SmmpParseErrorContent, (void **)errorString);
        return TRUE;
    }

    return FALSE;
}

VOID SmmReleaseResources()
{
    PGENERIC_DATATYPE_INFO gdi;
    PFNSIGN fnSign;

    BOOL isPrimv = FALSE;

	//We wont free primitive and user type info.
	//Because these are freed in 
    for (int i = 0;i < MAX_TYPE_IDENTS;i++)
    {
        if (TYPE_IDENTS[i].pti != NULL)
        {
            TYPE_IDENTS[i].pti = NULL;
        }
    }

    if (SmmpUserTypeList != NULL)
    {
        SmmpDestroySList(SmmpUserTypeList);
        SmmpUserTypeList = NULL;
    }

	if (SmmpAliasList != NULL)
	{
		//TODO: delete records
		SmmpDestroySList(SmmpAliasList);
		SmmpAliasList = NULL;
	}

    if (SmmpTypeList != NULL)
    {
        for (PSLISTNODE node = SmmpTypeList->head; node != NULL; node = node->next)
        {
            gdi = RECORD_OF(node, PGENERIC_DATATYPE_INFO);
           
            SmmpTypeIsPrimitive(gdi, &isPrimv);

            if (!isPrimv)
            {
                for (PSLISTNODE fieldNode = ((PSTRUCTINFO)gdi)->fields->head; 
                    fieldNode != NULL; 
                    fieldNode = fieldNode->next)
                {
                    FREEOBJECT(RECORD_OF(fieldNode, PSTRUCTMEMBERFIELD));
                }
            }

            FREEOBJECT(gdi);

        }

        SmmpDestroySList(SmmpTypeList);
        SmmpTypeList = NULL;
    }


    if (SmmpFnSignList != NULL)
    {
        for (PSLISTNODE node = SmmpFnSignList->head; node != NULL; node = node->next)
        {
            fnSign = RECORD_OF(node, PFNSIGN);
            FREEOBJECT(fnSign->args);
        }

        SmmpDestroySList(SmmpFnSignList);
        SmmpFnSignList = NULL;
    }

    SmmpInitWorkdirStack();

    if (SmmpParseErrorContent)
    {
        DmaDestroyAdapter(SmmpParseErrorContent);
        SmmpParseErrorContent = NULL;
    }
}