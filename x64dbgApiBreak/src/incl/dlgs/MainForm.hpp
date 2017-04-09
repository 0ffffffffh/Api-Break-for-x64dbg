#ifndef __MAINFORM_HPP_
#define __MAINFORM_HPP_

#include <ui/uiwrapper.hpp>
#include <ui/ctrl/uicombobox.hpp>

#include <resource.h>

#define ST_SUCCESS      RGB(0x0,0xae,0x0)
#define ST_FAIL         RGB(0xce,0x0,0x0)
#define ST_INFO         RGB(0x0,0x0,0xff)
#define ST_WARN         RGB(0xff,0x4f,0)

#define SETSTATUS(t,x,...) this->lblStatus->DrawStringFormatA(1,1,t,"Status: " ## x, __VA_ARGS__)
#define _SETSTATUS(t,x,...) _this->lblStatus->DrawStringFormatA(1,1,t,"Status: " ## x, __VA_ARGS__)

class MainForm : public UiWrapper
{
private:
    UiComboBox *cbModules, *cbApis;
    UiControlBase *chkUseXref, *lblStatus;

    void InvalidateApiFunctionList()
    {
        char modName[256] = { 0 };

        if (!cbModules->GetSelectedIndexTextA(modName))
            return;

        this->cbApis->Clear();
        AbEnumApiFunctionNames((APIMODULE_ENUM_PROC)ApiFunctionEnumerator, modName, this);
    }

    void SetBP()
    {
        char moduleName[256] = { 0 };
        char funcName[256] = { 0 };
        duint apiAddr;
        bool isSet;
        bool bpOnXrefs = false;

        GetControlTextA(IDC_CBAPIFUNC, funcName, sizeof(funcName));
        GetControlTextA(IDC_CBMODULELIST, moduleName, sizeof(moduleName));

        bpOnXrefs = (bool)chkUseXref->SendControlMsg(BM_GETCHECK, NULL, NULL);
        
        if (bpOnXrefs)
        {
            isSet = AbSetAPIBreakpointOnCallers(moduleName, funcName);

            if (isSet)
                SETSTATUS(ST_SUCCESS,"Api breakpoint set");
            else
                SETSTATUS(ST_FAIL,"Bp could not be set");

        }
        else
        {
                
            isSet = AbSetAPIBreakpoint(moduleName, funcName, &apiAddr);

            if (isSet)
                SETSTATUS(ST_SUCCESS,"Breakpoint is set");
            else
                SETSTATUS(ST_FAIL,"The bp could not be set");
        }

    }

    static void ModuleEnumerator(LPCSTR modName,void *user)
    {
        MainForm *_this = reinterpret_cast<MainForm *>(user);
        _this->cbModules->AddItemA(modName);
    }

    static void ApiFunctionEnumerator(LPCSTR funcName, void *user)
    {
        MainForm *_this = reinterpret_cast<MainForm *>(user);
        
        _this->cbApis->AddItemA(funcName);

    }

    static int WINAPI ApiLoadWorker(void *arg)
    {
        char processName[MAX_MODULE_SIZE] = { 0 };

        MainForm *_this = static_cast<MainForm *>(arg);
        int modCount = 0;

        if (!AbLoadAvailableModuleAPIs(true))
        {
            _this->MsgBoxError("Could not be loaded","error");
            return 0;
        }


        modCount = AbEnumModuleNames((APIMODULE_ENUM_PROC)ModuleEnumerator, _this);

        if (modCount > 0)
        {
            AbGetDebuggedImageName(processName);

            _this->SetControlTextFormatA(IDC_GRPMOD, "Imported modules && APIs by \"%s\"", processName);
            _this->EnableControl(IDC_BTNSETBP);
            _SETSTATUS(ST_INFO,"%d module(s) loaded",modCount);
        }
        return 0;
    }

public :
    MainForm() : UiWrapper(IDD_MAIN,true)
    {
    }

    void OnCommand(WPARAM wp, LPARAM lp)
    {
        switch (LOWORD(wp))
        {
            case IDC_BTNSETBP:
                SetBP();
                break;
            case IDC_CBMODULELIST:
            {
                if (HIWORD(wp) == CBN_SELCHANGE)
                {
                    InvalidateApiFunctionList();
                }
            }
            break;
            case IDC_CHKBPONXREFS:
            {
            }
            break;
        }

        UiWrapper::OnCommand(wp, lp);
    }

    void OnInit()
    {
        SetWindowTitleA(AB_APPTITLE);

        this->cbModules = GetControlById<UiComboBox>(IDC_CBMODULELIST);
        this->cbApis = GetControlById<UiComboBox>(IDC_CBAPIFUNC);
        this->chkUseXref = GetControlById<UiControlBase>(IDC_CHKBPONXREFS);
        this->lblStatus = GetControlById<UiControlBase>(IDC_LBLSTATUS);

        this->lblStatus->EnableOwnerDraw();

        DisableControl(IDC_BTNSETBP);

        SETSTATUS(ST_INFO,"Loading modules");

        if (AbHasDebuggingProcess())
            QueueUserWorkItem((LPTHREAD_START_ROUTINE)ApiLoadWorker, this, WT_EXECUTEDEFAULT);
        else
            SETSTATUS(ST_WARN,"There is no debug process");
    }

    bool ShowDialog()
    {
        return UiWrapper::ShowDialog(true);
    }
};

#endif //__MAINFORM_HPP_
