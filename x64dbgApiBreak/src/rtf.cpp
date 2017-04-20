#include <rtf.h>

#define RDS_NEEDS_SPACE     0x00000001


#define RTF_SETSTATE(rtf,x) if (!(rtf->state & x)) \
                            rtf->state |= x

BOOL RtfCreateRtfData(PRTF_DATA *rtfData)
{
    PRTF_DATA rtf;
    rtf = ALLOCOBJECT(RTF_DATA);

    if (!rtf)
        return FALSE;

    if (!DmaCreateAdapter(sizeof(char), 0x100, &rtf->rtfDma))
    {
        FREEOBJECT(rtf);
        return FALSE;
    }

    *rtfData = rtf;

    return TRUE;
}


BOOL RtfBuildRtfTextA(PRTF_DATA rtf, LPSTR *rtfText, DWORD *textLen)
{
    PDMA dma;
    LPSTR rtfBody;
    DWORD color;

    ARCHWIDE len;

    if (!rtf)
        return FALSE;

    if (!rtfText)
        return FALSE;

    
    DmaGetAdapterInfo(rtf->rtfDma, &len, NULL);

    DmaCreateAdapter(sizeof(char), len, &dma);

    DmaStringWriteA(dma, "{\\rtf1\\ansi\\ansicpg1252\\deff0\\nouicompat\\deflang1033 ");

    //Font table
    DmaStringWriteA(dma, "{\\fonttbl ");

    for (DWORD i = 0;i < rtf->fontCount;i++)
    {
        DmaStringWriteA(dma, "{\\f%d\\fnil\\fcharset%d %s;}", i, 0, rtf->fontTable[i]);

    }

    DmaStringWriteA(dma, "}");
    
    //Color table
    DmaStringWriteA(dma, "{\\colortbl ;");

    for (DWORD i = 0;i < rtf->colorCount;i++)
    {
        color = rtf->colorTable[i];
        DmaStringWriteA(dma, "\\red%d\\green%d\\blue%d;", GetRValue(color), GetGValue(color), GetBValue(color));
    }

    DmaStringWriteA(dma, "}");


    DmaStringWriteA(dma, "\\viewkind4\\pard\\sa200\\sl276\\slmult1\\lang9 ");

    DmaPrepareForRead(rtf->rtfDma, (void **)&rtfBody);

    DmaStringWriteA(dma, rtfBody);

    DmaStringWriteA(dma, "}");

    DmaTakeMemoryOwnership(dma, (void **)rtfText);

    DmaDestroyAdapter(dma);

    return TRUE;
}


void RtfFreeRtfText(LPSTR rtfText)
{
    FREESTRING(rtfText);
}

BOOL RtfDestroyRtfData(PRTF_DATA *rtf)
{
    PRTF_DATA prtf;

    if (!rtf)
        return FALSE;

    prtf = *rtf;

    if (prtf->colorTable)
        FREEOBJECT(prtf->colorTable);

    if (prtf->fontTable)
        FREEOBJECT(prtf->fontTable);

    DmaDestroyAdapter(prtf->rtfDma);

    FREEOBJECT(prtf);

    *rtf = NULL;

    return NULL;
}

BOOL RtfRegisterColor(PRTF_DATA rtf, DWORD color, RTFRESID *resId)
{
    DWORD *newPtr;
    RTFRESID index;

    if (!rtf)
        return FALSE;

    newPtr = (DWORD *)AbMemoryRealloc(rtf->colorTable, sizeof(DWORD) * (rtf->colorCount + 1));

    if (!newPtr)
    {
        return FALSE;
    }

    rtf->colorTable = newPtr;

    index = rtf->colorCount;

    rtf->colorCount++;
    rtf->colorTable[index] = color;

    if (resId)
        *resId = index;

    return TRUE;
}

BOOL RtfRegisterFont(PRTF_DATA rtf, const LPCSTR fontName, int charset, RTFRESID *resId)
{
    LPSTR fontNameBuffer;
    LPSTR *newPtr;
    RTFRESID index;

    if (!rtf)
        return FALSE;

    newPtr = (LPSTR *)AbMemoryRealloc(rtf->fontTable, sizeof(LPSTR) * (rtf->fontCount + 1));

    
    if (!newPtr)
    {
        return FALSE;
    }

    rtf->fontTable = newPtr;

    index = rtf->fontCount;

    rtf->fontCount++;

    fontNameBuffer = HlpCloneStringA(fontName);

    rtf->fontTable[index] = fontNameBuffer;

    if (resId)
        *resId = index;

    return TRUE;
}

BOOL RtfBeginStyleFor(PRTF_DATA rtf, RTF_STYLE rtfStyle, DWORD repeat)
{
    if (!repeat)
        repeat = 1;

    if (rtfStyle & RTFS_BOLD)
        DmaStringWriteA(rtf->rtfDma, "\\b");

    if (rtfStyle & RTFS_ITALIC)
        DmaStringWriteA(rtf->rtfDma, "\\i");

    if (rtfStyle & RTFS_UNDERLINE)
        DmaStringWriteA(rtf->rtfDma, "\\u");

    if (rtfStyle & RTFS_NEWLINE)
    {
        for (DWORD i=0;i<repeat;i++)
            DmaStringWriteA(rtf->rtfDma, "\\line");
    }

    if (rtfStyle & RTFS_TAB)
    {
        for (DWORD i=0;i<repeat;i++)
            DmaStringWriteA(rtf->rtfDma, "\\tab");
    }

    
    RTF_SETSTATE(rtf, RDS_NEEDS_SPACE);

    return TRUE;
}

BOOL RtfBeginColor(PRTF_DATA rtf, RTFRESID resId)
{
    RTF_SETSTATE(rtf, RDS_NEEDS_SPACE);
    
    return DmaStringWriteA(rtf->rtfDma, "\\cf%d", resId);
}

BOOL RtfBeginFont(PRTF_DATA rtf, RTFRESID resId)
{
    RTF_SETSTATE(rtf, RDS_NEEDS_SPACE);
    return DmaStringWriteA(rtf->rtfDma, "\\f%d", resId);
}

BOOL RtfBeginFontSize(PRTF_DATA rtf, WORD fontSize)
{
    RTF_SETSTATE(rtf, RDS_NEEDS_SPACE);
    return DmaStringWriteA(rtf->rtfDma, "\\fs%d", fontSize);
}

BOOL RtfAppendTextFormatA(PRTF_DATA rtf, LPCSTR format, ...)
{
    va_list va;
    
    va_start(va, format);

    RtfAppendTextFormatExA(rtf, format, va);

    va_end(va);

    
    return TRUE;
}

BOOL RtfAppendTextFormatExA(PRTF_DATA rtf, LPCSTR format, va_list vl)
{
    BOOL result;
    LPSTR buffer;

    if (!HlpPrintFormatBufferExA(&buffer, format, vl))
        return FALSE;

    if (rtf->state & RDS_NEEDS_SPACE)
    {
        DmaStringWriteA(rtf->rtfDma, " ");
        rtf->state &= ~RDS_NEEDS_SPACE;
    }

    result = DmaStringWriteA(rtf->rtfDma, buffer);

    FREESTRING(buffer);

    return result;
}