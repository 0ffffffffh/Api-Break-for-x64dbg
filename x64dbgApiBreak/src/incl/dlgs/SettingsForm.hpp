#ifndef __SETTINGSFORM_HPP_
#define __SETTINGSFORM_HPP_

#include <ui/uiwrapper.hpp>
#include <ui/ctrl/uicheckbox.hpp>
#include <settings.h>
#include <resource.h>

class SettingsForm : public UiWrapper
{
private:
    UiCheckBox *chkDetectDynLdr, *chkInclGetModHandle,
        *chkAutoload,*chkMapCallctx;

    UiControlBase *txtScripts;

    void FillGui()
    {
        Settings *settings = AbGetSettings();

        this->chkDetectDynLdr->SetState(settings->exposeDynamicApiLoads);
        this->chkInclGetModHandle->SetState(settings->includeGetModuleHandle);
        this->chkAutoload->SetState(settings->autoLoadData);
        this->chkMapCallctx->SetState(settings->mapCallContext);

        if (settings->mainScripts != NULL)
        {
            HlpReplaceStringA(settings->mainScripts, MAX_SETTING_SIZE, ";", "\r\n");
            this->txtScripts->SetStringA(settings->mainScripts);
        }
    }

    void GetFromGUI()
    {
        Settings *setting = AbGetSettings();

        setting->exposeDynamicApiLoads = this->chkDetectDynLdr->GetState();
        setting->includeGetModuleHandle = this->chkInclGetModHandle->GetState();
        setting->autoLoadData = this->chkAutoload->GetState();
        setting->mapCallContext = this->chkMapCallctx->GetState();

        txtScripts->GetStringA(&setting->mainScripts, MAX_SETTING_SIZE);
        HlpReplaceStringA(setting->mainScripts, MAX_SETTING_SIZE, "\r\n", ";");

    }

    bool LoadSettings()
    {
        if (AbSettingsLoad())
        {
            FillGui();
            return true;
        }

        return false;
    }

    bool SaveSettings()
    {
        GetFromGUI();

        return AbSettingsSave();
    }

public:
    SettingsForm() : UiWrapper(IDD_SETTINGS, true)
    {
    }

    void OnCommand(WPARAM wp, LPARAM lp)
    {
        switch (LOWORD(wp))
        {
            case IDC_BTNSAVESETTINGS:
                SaveSettings(); //fall trough
            case IDC_BTNDISCARDSETTINGS:
                Close();
                break;
            case IDC_CHKDETECTDYNLDR:
            {
                if (HIWORD(wp) == BN_CLICKED)
                {
                    bool state = this->chkDetectDynLdr->GetState();

                    this->chkInclGetModHandle->SetState(state);
                    this->chkInclGetModHandle->SetEnableState(state);
                    
                }
                break;
            }
        }

        UiWrapper::OnCommand(wp, lp);
    }

    void OnInit()
    {
        SetWindowTitleA(AB_APPTITLE);

        this->chkDetectDynLdr = GetControlById<UiCheckBox>(IDC_CHKDETECTDYNLDR);
        this->chkInclGetModHandle = GetControlById<UiCheckBox>(IDC_CHKINSPGETMODULEHANDLE);
        this->chkAutoload = GetControlById<UiCheckBox>(IDC_CHKAUTOLOAD);
        this->chkMapCallctx = GetControlById<UiCheckBox>(IDC_CHKMAPCALLCTX);
        this->txtScripts = GetControlById<UiControlBase>(IDC_TXTMAINSCRIPTS);

        LoadSettings();

    }

    bool ShowDialog()
    {
        return UiWrapper::ShowDialog(true);
    }
};

#endif //__SETTINGSFORM_HPP_
