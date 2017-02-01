#ifndef __SETTINGSFORM_HPP_
#define __SETTINGSFORM_HPP_

#include <ui/uiwrapper.hpp>
#include <ui/ctrl/uicheckbox.hpp>
#include <settings.h>
#include <resource.h>

class SettingsForm : public UiWrapper
{
private:
	UiCheckBox *chkDetectDynLdr;

	void FillGui()
	{
		Settings *settings = AbGetSettings();

		this->chkDetectDynLdr->SetState(settings->exposeDynamicApiLoads);
	}

	void GetFromGUI()
	{
		Settings *setting = AbGetSettings();
		setting->exposeDynamicApiLoads = this->chkDetectDynLdr->GetState();
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
		}

		UiWrapper::OnCommand(wp, lp);
	}

	void OnInit()
	{
		SetWindowTitleA(AB_APPTITLE);

		this->chkDetectDynLdr = GetControlById<UiCheckBox>(IDC_CHKDETECTDYNLDR);
		
		LoadSettings();

	}

	bool ShowDialog()
	{
		return UiWrapper::ShowDialog(true);
	}
};

#endif //__SETTINGSFORM_HPP_
