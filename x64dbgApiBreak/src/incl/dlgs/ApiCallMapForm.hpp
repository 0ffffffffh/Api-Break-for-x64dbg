#ifndef __APICALLMAPFORM_HPP_
#define __APICALLMAPFORM_HPP_

#include <ui/uiwrapper.hpp>
#include <ui/ctrl/uicombobox.hpp>

#include <resource.h>
#include <Richedit.h>


class ApiCallMapForm : public UiWrapper
{
private:
    UiControlBase *txtMapCallResultContent;
    LPSTR mapContent;
    LONG lengthOfMapContent;
    LONG lastReadOffset;

    static DWORD CALLBACK EditStreamCallback(
        _In_ DWORD_PTR dwCookie,
        _In_ LPBYTE    pbBuff,
        _In_ LONG      cb,
        _In_ LONG      *pcb
    )
    {
        LONG remainLen,readLen;

        ApiCallMapForm *_this = (ApiCallMapForm *)dwCookie;

        remainLen = _this->lengthOfMapContent - _this->lastReadOffset;

        if (!remainLen)
        {
            *pcb = 0;
            return 0;
        }

        if (remainLen > cb)
            readLen = cb;
        else
            readLen = remainLen;


        memcpy(pbBuff, (BYTE *)(_this->mapContent + _this->lastReadOffset), readLen);
        _this->lastReadOffset += readLen;
        *pcb = readLen;

        return 0;
    }
public:
    ApiCallMapForm(LPSTR callMapContent) : UiWrapper(IDD_APICALLMAP, true)
    {
        lastReadOffset = 0;
        mapContent = callMapContent;
        lengthOfMapContent = strlen(mapContent);
    }

    void OnLoaded()
    {
        this->txtMapCallResultContent->SendControlMsg(EM_SETSEL, (WPARAM)-1, (LPARAM)-1);
    }

    void OnClose()
    {
        if (mapContent)
            FREESTRING(mapContent);
    }

    void OnInit()
    {
        EDITSTREAM es;
        
        es.dwCookie = (DWORD_PTR)this;
        es.dwError = 0;
        es.pfnCallback = EditStreamCallback;

        
        this->txtMapCallResultContent = GetControlById<UiControlBase>(IDC_RTXTAPIMAPCONTENT);
        this->txtMapCallResultContent->SendControlMsg(EM_STREAMIN, SF_RTF | SFF_PLAINRTF,(LPARAM)&es);

        
        //this->txtMapCallResultContent->SetStringA(mapContent);
    }

    bool ShowDialog()
    {
        return UiWrapper::ShowDialog(true);
    }
};

#endif //__MAINFORM_HPP_
