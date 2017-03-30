#pragma once
#include <ui/ui.h>
#include <ui/uiwrapper.hpp>

#define IMPL_CTRLBASE_CTOR(t) t(UIOBJECT *ui, ULONG ctrlId, UiWrapper *parent) : UiControlBase(ui,ctrlId,parent) 

class UiControlBase
{
protected:
    UIOBJECT *ui;
    DWORD ctrlId;
    HWND ctrlHwnd;
    UiWrapper *parent;

    DWORD MyId() const
    {
        return this->ctrlId;
    }

    HWND MyHandle() const
    {
        return this->ctrlHwnd;
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

    virtual void OnInitControl()
    {

    }

    virtual void OnCommand(WPARAM wp)
    {

    }


};