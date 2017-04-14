#ifndef __RTF_H__
#define __RTF_H__

#include <corelib.h>
#include <util.h>

typedef struct __RTF_DATA
{
    PDMA        rtfDma;
    DWORD       state;
    DWORD *     colorTable;
    LPSTR *     fontTable;
    DWORD       colorCount;
    DWORD       fontCount;
}*PRTF_DATA,RTF_DATA;

typedef enum __RTF_STYLE
{
    RTFS_NONE =         0x00000000,
    RTFS_BOLD =         0x00000001,
    RTFS_ITALIC =       0x00000002,
    RTFS_UNDERLINE =    0x00000004,
    RTFS_NEWLINE =      0x00000008,
    RTFS_TAB   =        0x00000010,
    RTFS_PARAGRAPH =    0x00000020
}RTF_STYLE;


typedef DWORD RTFRESID;

BOOL RtfCreateRtfData(PRTF_DATA *rtfData);

BOOL RtfBuildRtfTextA(PRTF_DATA rtf, LPSTR *rtfText, DWORD *textLen);

void RtfFreeRtfText(LPSTR rtfText);

#define RtfGetRtfText RtfBuildRtfTextA

BOOL RtfDestroyRtfData(PRTF_DATA *rtf);

BOOL RtfRegisterColor(PRTF_DATA rtf, DWORD color, RTFRESID *resId);

BOOL RtfRegisterFont(PRTF_DATA rtf, const LPCSTR fontName, int charset, RTFRESID *resId);

BOOL RtfBeginStyleFor(PRTF_DATA rtf, RTF_STYLE rtfStyle, DWORD repeat);

BOOL RtfBeginColor(PRTF_DATA rtf, RTFRESID resId);

BOOL RtfBeginFont(PRTF_DATA rtf, RTFRESID resId);

BOOL RtfBeginFontSize(PRTF_DATA rtf, WORD fontSize);

BOOL RtfAppendTextFormatA(PRTF_DATA rtf, LPCSTR format, ...);

BOOL RtfAppendTextFormatExA(PRTF_DATA rtf, LPCSTR format, va_list vl);

class Rtf
{
private:
    PRTF_DATA data;
public:
    Rtf()
    {
        RtfCreateRtfData(&data);
    }

    ~Rtf()
    {
        RtfDestroyRtfData(&data);
    }

    bool RegisterFont(const LPCSTR fontName, int charset)
    {
        return RtfRegisterFont(data, fontName, charset, NULL);
    }

    bool RegisterColor(DWORD color)
    {
        return RtfRegisterColor(data, color, NULL);
    }

    LPSTR GetRtf()
    {
        LPSTR rtfText;

        if (!RtfBuildRtfTextA(data, &rtfText, NULL))
            return NULL;

        return rtfText;
    }

    Rtf *Style(RTF_STYLE style, DWORD repeat)
    {
        RtfBeginStyleFor(data, style, repeat);
        return this;
    }

    Rtf *Style(RTF_STYLE style)
    {
        return Style(style, 0);
    }

    Rtf *Color(RTFRESID colorId)
    {
        RtfBeginColor(data, colorId);
        return this;
    }

    Rtf *Font(RTFRESID fontId)
    {
        RtfBeginFont(data, fontId);
        return this;
    }

    Rtf *FontSize(WORD size)
    {
        RtfBeginFontSize(data, size);
        return this;
    }

    Rtf *FormatText(LPCSTR format, ...)
    {
        va_list va;
        va_start(va, format);
        RtfAppendTextFormatExA(data, format,va);
        va_end(va);

        return this;
    }

    Rtf *NewLine(int count)
    {
        return Style(RTFS_NEWLINE, count);
    }

    Rtf *NewTab(int count)
    {
        return Style(RTFS_TAB, count);
    }
};

#endif // !__RTF_H__
