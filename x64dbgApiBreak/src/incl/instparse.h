#ifndef __INSTPARSE_H__
#define __INSTPARSE_H__

#include <corelib.h>

typedef struct
{
    uchar                   size;
    uchar                   op;
    char                    reg;
    uchar                   reg_oper;
    uchar                   mem_access;
    duint                   imm;
    struct
    {
        uchar               base;
        uchar               index;
        uchar               scale;
        int                 disp;
    }memory_info;
}InstInfo;

#define MA_NONE                     0
#define MA_READ                     1
#define MA_WRITE                    2

#define OP_MOV                      1
#define OP_LEA                      2
#define OP_PUSH                     3

#define ORD_OP                      0
#define ORD_DREG                    1
#define ORD_SREG                    2
#define ORD_DONE                    3

#define GET_REGSIZE(pfx)            (pfx == 'r' ? 64 : pfx == 'e' ? 32 : 8)

typedef enum
{
    RegNone = -1,
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
    r15,
    ImmMem
}RegId;

bool AbParseInstruction(BASIC_INSTRUCTION_INFO *inst, InstInfo *linst);


#endif // !__INSTPARSE_H__
