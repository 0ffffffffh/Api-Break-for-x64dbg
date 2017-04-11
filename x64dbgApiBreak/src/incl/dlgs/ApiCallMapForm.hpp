#ifndef __APICALLMAPFORM_HPP_
#define __APICALLMAPFORM_HPP_

#include <ui/uiwrapper.hpp>
#include <ui/ctrl/uicombobox.hpp>

#include <resource.h>

class ApiCallMapForm : public UiWrapper
{
private:
    UiControlBase *txtMapCallResultContent;
    LPSTR mapContent;

public:
    ApiCallMapForm(LPSTR callMapContent) : UiWrapper(IDD_APICALLMAP, true)
    {
        mapContent = callMapContent;

        //Load richedit library for the control
        
        LoadLibraryA("Riched20.dll");
    }

    void OnInit()
    {   
        this->txtMapCallResultContent = GetControlById<UiControlBase>(IDC_RTXTAPIMAPCONTENT);

        this->txtMapCallResultContent->SetStringA(mapContent);
    }

    bool ShowDialog()
    {
        return UiWrapper::ShowDialog(true);
    }
};

#endif //__MAINFORM_HPP_
