#pragma once

class UiWrapper;

#include <ui/ui.h>
#include <ui/ctrl/uicontrolbase.hpp>

#include <unordered_map>

using namespace std;

class UiWrapper
{
private:

    static UINT_PTR CALLBACK DialogProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, PVOID arg)
    {
        UiWrapper *_this = (UiWrapper *)arg;

        switch (msg)
        {
        case WM_INITDIALOG:
        {
            _this->uiObject = (UIOBJECT *)lp;
            _this->OnInitInternal();
        }
        break;
        case WM_WINDOWPOSCHANGED:
        case WM_ACTIVATE:
        case WM_SHOWWINDOW:
        {
            if (!_this->isFullyLoaded)
            {
                if (IsWindowVisible(hwnd))
                {
                    _this->isFullyLoaded = true;
                    _this->OnLoaded();
                }
            }
        }
        break;
        case WM_COMMAND:
            _this->OnCommand(wp, lp);
            break;
        case WM_PAINT:
            _this->OnPaint();
            break;
        case WM_TIMER:
            _this->OnTimerTick((LONG)wp);
            break;
        case WM_CLOSE:
            _this->OnClose();
            break;
        case WM_DRAWITEM:
            _this->OnDrawItem((LPDRAWITEMSTRUCT)lp, (LONG)wp);
            return TRUE;
        case WM_QUIT:
            _this->OnDestroyed();
            break;
        }


        return 0;
    }

private:
    HANDLE initCompletedEvent;
    UIOBJECT *uiObject;
    WINDOWCREATIONINFO wci;
    LONG dlgId;
    bool isFullyLoaded;
    bool killingSelf;

    unordered_map<HWND, UiControlBase *> childControls;

    UiControlBase *LookupControl(HWND hwnd)
    {
        unordered_map<HWND, UiControlBase *>::iterator it;

        if (childControls.size() == 0)
            return nullptr;

        it = childControls.find(hwnd);

        if (it == childControls.end())
            return NULL;

        return it->second;
    }

    void DestroyControlResources()
    {
        unordered_map<HWND, UiControlBase *>::iterator it;
        UiControlBase *control = NULL;

        if (childControls.size() == 0)
            return;

        while (childControls.size() > 0)
        {
            it = childControls.begin();
            control = it->second;
            childControls.erase(it);
            delete control;
        }
    }

private:

    bool SetControlEnableState(ULONG ctrlId, bool state)
    {
        HWND ctrlHwnd = GetDlgItem(GetHWND(), ctrlId);

        if (!ctrlHwnd)
            return false;

        return (bool)EnableWindow(ctrlHwnd, (BOOL)state);
    }

    void InitCommon(LONG dlgId, bool center)
    {
        this->killingSelf = false;
        this->isFullyLoaded = false;
        this->initCompletedEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

        this->dlgId = dlgId;

        this->wci.pci = NULL;

        if (center)
        {
            this->wci.pci = ALLOCOBJECT(PRECREATEWINDOWINFO);
            this->wci.pci->wri.flag = WRIF_CENTER;
        }

    }

    void OnInitInternal()
    {
        this->OnInit();

        SetEvent(this->initCompletedEvent);
    }

public:

    UiWrapper(LONG dlgId, bool center)
    {
        InitCommon(dlgId, center);
    }

    UiWrapper(LONG dlgId)
    {
        InitCommon(dlgId, false);
    }

    ~UiWrapper(void)
    {
        DestroyControlResources();
        CloseHandle(this->initCompletedEvent);

        if (!this->killingSelf)
        {
            if (UiIsRunning(this->uiObject))
                Close();
        }
    }

    static void DestroyAllActiveWindows()
    {
        UiForceCloseAllActiveWindows();
    }

    virtual bool ShowDialog()
    {
        return ShowDialog(true);
    }

    virtual bool ShowDialog(bool seperateUiThread)
    {
        BOOL succeeded;

        this->uiObject = UiCreateDialog(
            (UIDLGPROC)this->DialogProc,
            NULL,
            this->dlgId,
            (BOOL)seperateUiThread,
            this,
            &this->wci,
            &succeeded);

        if (!seperateUiThread)
            return (bool)succeeded;

        return this->uiObject != NULL;
    }

    HWND GetHWND() const
    {
        return this->uiObject->hwnd;
    }

    bool SetTimer(LONG timerId, ULONG period)
    {
        return ::SetTimer(GetHWND(), (UINT_PTR)timerId, (UINT)period, NULL)
            == timerId;
    }

    void KillTimer(LONG timerId)
    {
        ::KillTimer(GetHWND(), (UINT_PTR)timerId);
    }

    LONG MessageBox(LPWSTR msg, LPWSTR title, ULONG flags)
    {
        return ::MessageBoxW(GetHWND(), (LPCWSTR)msg, (LPCWSTR)title, flags);
    }

    LONG MsgBoxInfo(LPCSTR msg, LPCSTR title)
    {
        return ::MessageBoxA(GetHWND(), msg, title, MB_ICONINFORMATION);
    }

    LONG MsgBoxError(LPCSTR msg, LPCSTR title)
    {
        return ::MessageBoxA(GetHWND(), msg, title, MB_ICONERROR);
    }

    LONG MsgBoxQuestion(LPCSTR msg, LPCSTR title)
    {
        return ::MessageBoxA(GetHWND(), msg, title, MB_YESNO | MB_ICONQUESTION);
    }

    void WaitForInitCompletion()
    {
        WaitForSingleObject(this->initCompletedEvent, INFINITE);
    }

    void Close()
    {
        if (UiDestroyDialog(this->uiObject))
        {
            this->uiObject = NULL;
        }
    }

    ULONG GetControlTextA(ULONG ctrlId, LPSTR strBuf, ULONG bufSize)
    {
        LPWSTR wbuf = ALLOCSTRINGW(bufSize);
        LPSTR as;
        ULONG textLen;

        if (!wbuf)
            return 0;

        textLen = GetControlText(ctrlId, wbuf, bufSize);

        if (textLen > 0)
        {
            as = HlpWideToAnsiString(wbuf);

            if (as != NULL)
            {
                strncpy(strBuf, as, textLen);
                FREESTRING(as);
            }
        }

        FREESTRING(wbuf);

        return textLen;
    }

    ULONG GetControlText(ULONG ctrlId, LPWSTR strBuf, ULONG bufSize)
    {
        HWND ctrlHwnd = GetDlgItem(GetHWND(), ctrlId);

        if (!ctrlHwnd)
            return 0;

        return GetWindowTextW(ctrlHwnd, (LPWSTR)strBuf, bufSize);
    }

    void SetWindowTitleW(LPWSTR title)
    {
        SetWindowTextW(GetHWND(), title);
    }

    void SetWindowTitleA(LPSTR title)
    {
        SetWindowTextA(GetHWND(),title);
    }

    bool SetControlTextA(ULONG ctrlId, LPSTR str)
    {
        bool ret;
        LPWSTR wstr;

        wstr = HlpAnsiToWideString(str);

        ret = SetControlText(ctrlId, wstr);

        FREESTRING(wstr);

        return ret;
    }

    bool SetControlText(ULONG ctrlId, LPWSTR str)
    {
        HWND ctrlHwnd = GetDlgItem(GetHWND(), ctrlId);

        if (!ctrlHwnd)
            return false;

        return (bool)SetWindowTextW(ctrlHwnd, (LPCWSTR)str);
    }

    bool SetControlTextFormatA(ULONG ctrlId, const char *format, ...)
    {
        char *buffer;
        bool success = false;

        va_list vl;

        va_start(vl, format);

        if (HlpPrintFormatBufferExA(&buffer, format, vl) > 0)
        {
            SetControlTextA(ctrlId, buffer);
            FREESTRING(buffer);
            success = true;
        }

        va_end(vl);

        return success;
    }

    bool EnableControl(ULONG ctrlId)
    {
        return SetControlEnableState(ctrlId, true);
    }

    bool DisableControl(ULONG ctrlId)
    {
        return SetControlEnableState(ctrlId, false);
    }

    template <class T> T *GetControlById(ULONG ctrlId)
    {
        HWND ctrlHwnd;
        T *ctrl = NULL;
        
        ctrlHwnd = GetDlgItem(GetHWND(), ctrlId);

        
        if ((ctrl = (T *)LookupControl(ctrlHwnd)) != NULL)
            return ctrl;
        
        ctrl = new T(this->uiObject, ctrlId, this);


        childControls.insert(std::pair<HWND, UiControlBase *>(ctrlHwnd, ctrl));

        ctrl->OnInitControl();

        return ctrl;
    }

    virtual void OnTimerTick(LONG timerId)
    {

    }

    virtual void OnDestroyed()
    {
        delete this;
    }

    virtual void OnClose()
    {
        this->killingSelf = true;
    }

    virtual void OnPaint()
    {
    }

    virtual void OnLoaded()
    {

    }

    virtual void OnCommand(WPARAM wp, LPARAM lp)
    {
        UiControlBase *ctrl = NULL;

        if (lp != NULL)
        {
            ctrl = LookupControl((HWND)lp);

            if (ctrl != nullptr)
                ctrl->OnCommand(wp);
        }
    }

    virtual void OnDrawItem(LPDRAWITEMSTRUCT dis, LONG ctrlId)
    {
        UiControlBase *ctrl = NULL;

        HWND wnd = GetDlgItem(GetHWND(), ctrlId);
        DBGPRINT("AX");
        ctrl = LookupControl(wnd);

        if (ctrl != nullptr)
            ctrl->OnDrawItem(dis);
        
    }

    virtual void OnInit()
    {
    }
};

