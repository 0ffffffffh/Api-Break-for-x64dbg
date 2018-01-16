#pragma once
#include <ui/ui.h>
#include <ui/uiwrapper.hpp>

#define IMPL_CTRLBASE_CTOR(t) t(UIOBJECT *ui, ULONG ctrlId, UiWrapper *parent) : UiControlBase(ui,ctrlId,parent) 

class UiControlBase
{
private:
    struct OwnerStringDrawInfo
    {
        char value[512];
        COLORREF foreColor;
        POINT xy;
    };
protected:
    UIOBJECT *ui;
    DWORD ctrlId;
    HWND ctrlHwnd;
    UiWrapper *parent;
    OwnerStringDrawInfo ownerStringDrawData;

    DWORD MyId() const
    {
        return this->ctrlId;
    }

    HWND MyHandle() const
    {
        return this->ctrlHwnd;
    }

    ULONG GetStringCommon(BYTE **pValue, ULONG size)
    {
        LONG reqLen, readLen;

        reqLen = SendControlMsg(WM_GETTEXTLENGTH, NULL, NULL);

        if (reqLen <= 0)
            return 0;

        
        reqLen++; //Include null term


        if (size == 0)
        {
            if (IsWindowUnicode(MyHandle()))
                *pValue = (BYTE *)ALLOCSTRINGW(reqLen);
            else
                *pValue = (BYTE *)ALLOCSTRINGA(reqLen);

            if (*pValue == NULL)
                return 0;
        }
		else if ((ULONG)reqLen > size)
		{
			return 0;
		}


        readLen = SendControlMsg(WM_GETTEXT, (WPARAM)reqLen, (LPARAM)*pValue);

        if (readLen <= 0)
            return 0;

        return readLen;
    }

public:
    UiControlBase()
    {
        this->ui = NULL;
        this->ctrlId = 0;
    }

    UiControlBase(UIOBJECT *ui, DWORD ctrlId, UiWrapper *parent)
    {
        this->ui = ui;
        this->ctrlId = ctrlId;
        this->parent = parent;

        this->ctrlHwnd = GetDlgItem(this->ui->hwnd, this->ctrlId);
    }

    LONG SendControlMsg(UINT msg, WPARAM wp, LPARAM lp)
    {
        return (LONG)::SendMessage(this->ctrlHwnd, msg, wp, lp);
    }

    void EnableOwnerDraw()
    {
        LONG style = GetWindowLong(MyHandle(), GWL_STYLE);

        style |= SS_OWNERDRAW;

        SetWindowLong(MyHandle(), GWL_STYLE, style);
    }

    void Disable()
    {
        ::EnableWindow(MyHandle(), FALSE);
    }

    void Enable()
    {
        ::EnableWindow(MyHandle(), TRUE);
    }

    void SetEnableState(bool enable)
    {
        ::EnableWindow(MyHandle(), enable);
    }

    ULONG GetStringA(LPSTR *pValue, ULONG size)
    {
        BYTE *pVal=NULL;
        ULONG readLen;
        LPSTR strA;

        if (IsWindowUnicode(MyHandle()))
        {
            readLen = GetStringCommon(&pVal, 0);

            if (!readLen)
                return 0;

            if (readLen > size - 1)
            {
                FREESTRING(*pVal);
                return 0;
            }

            strA = HlpWideToAnsiString((LPCWSTR)pVal);

            if (!size)
                *pValue = strA;
            else
            {
                strcpy(*pValue, strA);
                FREESTRING(strA);
            }
        }
        else
            readLen = GetStringCommon((BYTE **)pValue, size);

        return readLen;
    }

    ULONG GetStringW(LPWSTR *pValue, ULONG size)
    {
        BYTE *pVal = NULL;
        ULONG readLen;
        LPWSTR strW;

        if (!IsWindowUnicode(MyHandle()))
        {
            readLen = GetStringCommon(&pVal, 0);

            if (!readLen)
                return 0;

            if (readLen > size - 1)
            {
                FREESTRING(*pVal);
                return 0;
            }

            strW = HlpAnsiToWideString((LPCSTR)pVal);

            if (!size)
                *pValue = strW;
            else
            {
                wcscpy(*pValue, strW);
                FREESTRING(strW);
            }
        }
        else
            readLen = GetStringCommon((BYTE **)pValue, size);

        return readLen;
    }

    BOOL SetStringA(LPCSTR value)
    {
        return SetWindowTextA(MyHandle(), value);
    }

    BOOL SetStringW(LPCWSTR value)
    {
        return SetWindowTextW(MyHandle(), value);
    }

    void DrawStringA(LPCSTR text, INT x, INT y, COLORREF foreColor)
    {
        strcpy(ownerStringDrawData.value, text);
        ownerStringDrawData.xy.x = x;
        ownerStringDrawData.xy.y = y;
        ownerStringDrawData.foreColor = foreColor;
        InvalidateRect(MyHandle(), NULL, TRUE);
    }

    void DrawStringFormatA(INT x, INT y, COLORREF foreColor, LPCSTR format, ...)
    {
        va_list va;
        LPSTR buffer;
        va_start(va, format);
        
        if (!HlpPrintFormatBufferExA(&buffer, format, va))
            return;

        DrawStringA(buffer, x, y, foreColor);

        FREESTRING(buffer);

        va_end(va);
    }


    virtual void OnInitControl()
    {

    }

    virtual void OnCommand(WPARAM wp)
    {

    }

    virtual void OnDrawItem(LPDRAWITEMSTRUCT dis)
    {
        UiDrawStringA(
            dis->hDC, 
            ownerStringDrawData.value, 
            ownerStringDrawData.xy.x,
            ownerStringDrawData.xy.y, 
            ownerStringDrawData.foreColor, 0);
    }


};