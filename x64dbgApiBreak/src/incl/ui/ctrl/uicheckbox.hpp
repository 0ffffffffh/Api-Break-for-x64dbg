#pragma once

#include <ui/ctrl/uicontrolbase.hpp>

class UiCheckBox : public UiControlBase
{
private:

public:

	IMPL_CTRLBASE_CTOR(UiCheckBox)
	{
	}

	bool IsChecked()
	{

	}

	virtual void OnCommand(WPARAM wp)
	{
		if (HIWORD(wp) == BN_CLICKED)
		{

		}
	}

};