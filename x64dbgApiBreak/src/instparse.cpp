#include <instparse.h>

const char *regs[17]
{
    "*ax",
    "*bx",
    "*cx",
    "*dx",
    "*si",
    "*di",
    "*bp",
    "*sp",
    "*ip",
    "r8",
    "r9",
    "r10",
    "r11",
    "r12",
    "r13",
    "r14",
    "r15"
};

#define TOKLIST_SIZE    16
#define TOKLIST_MAX     (TOKLIST_SIZE - 1)

#define RT_IMM          0
#define RT_REG          1

typedef struct
{
    struct
    {
        char tok[16];
    }tokenList[TOKLIST_SIZE];
    duint count;
    duint index;
}token_data;


#define Currtoken(td) (char *)(td)->tokenList[(td)->index].tok

__forceinline void Skiptoken(token_data *td, int skip)
{
    if (abs(skip) > td->index)
        return;

    td->index += skip;
}

__forceinline char * Nexttoken(token_data *td)
{
    char *tok = NULL;
    
    if (td->index < td->count)
        tok = (char *)td->tokenList[++td->index].tok;

    return tok;
}

__forceinline char * Prevtoken(token_data *td)
{
    char *tok = NULL;
    
    if (td->index != 0)
        tok = (char *)td->tokenList[--td->index].tok;

    return tok;
}


__forceinline char *Nextslot(token_data *td)
{
    if (td->count >= TOKLIST_MAX)
        return NULL;

    return td->tokenList[++td->count].tok;
}

void AbpTokenize(char *inst, token_data *tokdata)
{
    char *p,*pbuf;
    int pbi = 0;

    memset(tokdata, 0, sizeof(token_data));

    p = inst;

    pbuf = tokdata->tokenList[0].tok;
    tokdata->index = 0;

    while (*p != 0)
    {
        if (*p == ' ' || *p == ',' || *p == ':')
        {
            if (pbi > 0)
            {
                pbuf = Nextslot(tokdata);
                pbi = 0;
            }
        }
        else if (*p == '*' || *p == '+' || *p == '-' || *p == '[' || *p == ']')
        {
            if (pbi > 0)
            {
                pbuf = Nextslot(tokdata);
                pbi = 0;
            }

            *pbuf = *p;
            pbuf = Nextslot(tokdata);
        }
        else
        {
            pbuf[pbi++] = *p;
        }

        p++;
    }

}

bool _AbpParseRegister(token_data *td, InstInfo *instInfo, dsint *val, short *size, unsigned char *type);

/*


Direct Operand: displacement (often just the symbolic name for a memory location)

Indirect Operand: (base)

Base+displacement: 

(index*scale)+displacement

Base + index  + displacement

Base+(index*scale)+ displacement

*/

#define MAO_ADD 1
#define MAO_MUL 2

#define ADM_BORD        0
#define ADM_BD          MAO_ADD // + 
#define ADM_ISD         MAO_MUL + MAO_ADD // * +
#define ADM_BID         MAO_ADD + MAO_ADD // + +
#define ADM_BISD        MAO_ADD + MAO_MUL + MAO_ADD // + * +



bool AbpParseMemoryAccess(token_data *td, InstInfo *linst)
{
    int adr_mode = 0;

    struct
    {
        dsint ident;
        short size;
        unsigned char type;
    }idents[5];

    int idx = 0;

    char *p = NULL;
    
    if (linst->op == OP_PUSH || linst->reg != RegNone)
        linst->mem_access = MA_READ;
    else if (linst->reg == RegNone)
        linst->mem_access = MA_WRITE;

    do
    {
        p = Nexttoken(td);

        if (*p == '*')
        {
            adr_mode += MAO_MUL;
            continue;
        }
        else if (*p == '-' || *p == '+')
        {
            adr_mode += MAO_ADD;
            continue;
        }
        else if (*p == ']')
            break;

        if (!_AbpParseRegister(td, NULL, &idents[idx].ident, &idents[idx].size, &idents[idx].type))
            return false;

        idx++;

    } while (*p);

    switch (adr_mode)
    {
    case ADM_BORD:
        if (idents[0].type == RT_IMM)
            linst->memory_info.disp = (int)idents[0].ident;
        else
            linst->memory_info.base = (unsigned char)idents[0].ident;

        break;
    case ADM_BD:
        linst->memory_info.base = (unsigned char)idents[0].ident;
        linst->memory_info.disp = (int)idents[1].ident;
        break;
    case ADM_BID:
        linst->memory_info.base = (unsigned char)idents[0].ident;
        linst->memory_info.index = (unsigned char)idents[1].ident;
        linst->memory_info.disp = (int)idents[2].ident;
        break;
    case ADM_BISD:
        linst->memory_info.base = (unsigned char)idents[0].ident;
        linst->memory_info.index = (unsigned char)idents[1].ident;
        linst->memory_info.scale = (unsigned char)idents[2].ident; //1, 2 ,4, 8
        linst->memory_info.disp = (int)idents[2].ident;
        break;
    case ADM_ISD:
        linst->memory_info.index = (unsigned char)idents[0].ident;
        linst->memory_info.scale = (unsigned char)idents[1].ident;
        linst->memory_info.disp = (unsigned char)idents[2].ident;
        break;
    }

    return true;
}

inline bool AbpIsNumStr(const char *str)
{
    char *p = const_cast<char *>(str);

    while (*p)
    {
        if (!isdigit(*p))
            return false;

        p++;
    }

    return true;
}

bool _AbpParseRegister(token_data *td, InstInfo *instInfo, dsint *val, short *size, unsigned char *type)
{
    char *p = Currtoken(td);
    char pfx = *p;
    int beg = 0, end = 8, rx = -1;
    bool ok = false;
    dsint nval;
    short nsize;
    unsigned char ntype;

    if (*p == '[')
    {
        ok = AbpParseMemoryAccess(td, instInfo);

        if (ok)
        {
            if (instInfo->reg == RegNone)
                instInfo->reg = ImmMem;
            else
                instInfo->reg_oper = ImmMem;
        }

        return ok;
    }

    
    if (strstr(p, "0x") || AbpIsNumStr(p))
    {
#ifdef _WIN64
        nval = (dsint)strtoll(p, NULL, 0);
        nsize = 64;
#else
        nval = (dsint)strtol(p, NULL, 0);
        nsize = 32;
#endif

        ntype = RT_IMM;
        ok = true;

        goto oneWayExit;
    }

    nsize = GET_REGSIZE(pfx);

    if (pfx == 'r')
    {
        beg = 0;
        end = 17;
    }

    for (;beg < end;beg++)
    {
        if (!strcmp(p + 1, regs[beg] + 1))
        {
            rx = beg;
            break;
        }
    }
    
    nval = rx;
    ntype = RT_REG;
    ok = true;

oneWayExit:

    if (instInfo != NULL)
    {
        switch (ntype)
        {
        case RT_IMM:
        {
            instInfo->imm = (duint)nval;

            if (instInfo->reg == (char)RegNone)
                instInfo->reg = ImmMem;
            else
                instInfo->reg_oper = ImmMem;
            break;
        }
        case RT_REG:
        {
            if (instInfo->reg == (char)RegNone)
                instInfo->reg = (char)nval;
            else
                instInfo->reg_oper = (char)nval;

            break;
        }
        }

        instInfo->size = nsize;
    }
    else
    {
        *val = nval;
        *size = nsize;
        *type = ntype;
    }

    return ok;
}

bool AbpParseRegister(token_data *td, InstInfo *instInfo)
{
    return _AbpParseRegister(td, instInfo, NULL, NULL, NULL);
}

bool AbpParseOperand(token_data *td, InstInfo *linst)
{
    char *p = Currtoken(td);

    if (!strcmp(p, "byte") ||
        !strcmp(p, "word") ||
        !strcmp(p, "dword") ||
        !strcmp(p, "qword"))
    {
        Skiptoken(td, 1);
        p = Nexttoken(td);

        if (p[1] == 's' && p[2] == '\0') //if segment identifier skip to the next token
            p = Nexttoken(td); //grab register

        if (!p)
            return false;
    }

    if (!AbpParseRegister(td, linst))
        return false;

    return true;
}

#if _DEBUG
#define _PRSLOG DBGPRINT

#define CONCAT_BUF(buf,format, ...) HlpConcateStringFormatA(buf,1024,format,__VA_ARGS__)

void __AbpPrintMemData(InstInfo *inst, char *buf)
{
    if (inst->memory_info.base != uchar(RegNone))
        CONCAT_BUF(buf, "(base: %s) ", regs[inst->memory_info.base]);

    if (inst->memory_info.index != uchar(RegNone))
        CONCAT_BUF(buf, "(index: %s) ", regs[inst->memory_info.index]);

    if (inst->memory_info.scale != 0)
        CONCAT_BUF(buf, "(scale: %d) ", inst->memory_info.scale);

    if (inst->memory_info.disp != 0)
        CONCAT_BUF(buf, "(disp: 0x%x)", inst->memory_info.disp);

    CONCAT_BUF(buf, " - ");
}

void __AbpPrintInstructionInfo(InstInfo *inst)
{
    char inst_info_buf[1024] = { 0 };
    bool single = false;

    if (inst->reg_oper == uchar(RegNone))
    {
        if (inst->reg == uchar(RegNone))
        {
            CONCAT_BUF(inst_info_buf, "Invalid instruction");
            goto doPrint;
        }

        CONCAT_BUF(inst_info_buf, "Single operand. Operand: ");
        single = true;
    }
    else
        CONCAT_BUF(inst_info_buf,"Source operand: ");



    if (inst->reg == ImmMem)
        __AbpPrintMemData(inst, inst_info_buf);
    else
        CONCAT_BUF(inst_info_buf, "Reg: %s ", regs[inst->reg]);


    if (!single)
    {
        CONCAT_BUF(inst_info_buf, "Dest operand: ");

        if (inst->reg_oper == ImmMem)
            __AbpPrintMemData(inst, inst_info_buf);
        else
            CONCAT_BUF(inst_info_buf, "Reg: %s ", regs[inst->reg_oper]);
    }

doPrint:

    _PRSLOG("%s",inst_info_buf);

}

#else
#define _PRSLOG
#endif


bool AbParseInstruction(BASIC_INSTRUCTION_INFO *inst, InstInfo *linst)
{
    char *p;
    
    token_data tokdata;

    char order = 0;
    AbpTokenize(inst->instruction, &tokdata);

    if (!tokdata.count)
        return false;
    
    p = Currtoken(&tokdata);

    memset(linst, 0, sizeof(InstInfo));

    linst->reg = RegNone;
    linst->reg_oper = RegNone;
    linst->memory_info.base = RegNone;
    linst->memory_info.index = RegNone;

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
            if (!AbpParseOperand(&tokdata, linst))
                return false;

            order++;


            if (linst->op == OP_PUSH)
            {
                order++;
            }
        }
        else if (order == ORD_SREG)
        {
            if (!AbpParseOperand(&tokdata, linst))
                return false;
            
            order++;
        }

        p = Nexttoken(&tokdata);

    }
    
    return true;
}