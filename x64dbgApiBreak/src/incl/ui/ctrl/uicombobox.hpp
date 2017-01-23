#pragma once

#include <ui/ctrl/uicontrolbase.hpp>

class UiComboBox : public UiControlBase
{
private:

public:

	IMPL_CTRLBASE_CTOR(UiComboBox)
	{
	}

	bool AddItem(LPCWSTR itemStr)
	{
		return SendMessageW(this->ctrlHwnd, CB_ADDSTRING, NULL, (LPARAM)itemStr) > -1;
	}

	bool AddItemA(LPCSTR itemStr)
	{
		return SendMessageA(this->ctrlHwnd, CB_ADDSTRING, NULL, (LPARAM)itemStr) > -1;
	}

	void Clear()
	{
		SendMessage(this->ctrlHwnd, CB_RESETCONTENT, NULL, NULL);
	}

	LONG GetSelectedIndex()
	{
		return SendMessageW(this->ctrlHwnd, CB_GETCURSEL, NULL, NULL);
	}

	bool SetSelectedIndex(LONG index)
	{
		return SendMessageW(this->ctrlHwnd, CB_SETCURSEL, (WPARAM)index, NULL) == index;
	}

	bool GetSelectedIndexTextA(LPSTR buffer)
	{
		LONG selIndex = GetSelectedIndex();

		return SendMessageA(this->ctrlHwnd, CB_GETLBTEXT, selIndex, (LPARAM)buffer) > 0;
	}
};