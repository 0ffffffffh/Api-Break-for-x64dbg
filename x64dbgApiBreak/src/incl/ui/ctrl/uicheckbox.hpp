#pragma once

#include <ui/ctrl/uicontrolbase.hpp>

class UiCheckBox : public UiControlBase
{
private:

public:

    IMPL_CTRLBASE_CTOR(UiCheckBox)
    {
    }

    bool GetState()
    {
        return (bool)SendControlMsg(BM_GETCHECK, NULL, NULL);
    }

    void SetState(bool state)
    {
        SendControlMsg(BM_SETCHECK, state ? BST_CHECKED : BST_UNCHECKED, NULL);
    }

    virtual void OnCommand(WPARAM wp)
    {
        if (HIWORD(wp) == BN_CLICKED)
        {

        }
    }

};