#ifndef __UI_H_
#define __UI_H_

#include <corelib.h>
#include <CommCtrl.h>

typedef struct
{
    DWORD   flag;
    POINT   point;
    SIZE    size;
}WINDOWREGIONINFO;

#define WRIF_CENTER         0x00000001
#define WRIF_LOCATION       0x00000002
#define WRIF_SIZE           0x00000004


typedef struct tagPreCreateWindowInfo
{
    WINDOWREGIONINFO    wri;
}PRECREATEWINDOWINFO;


typedef struct tagWindowCreationInfo
{
    PRECREATEWINDOWINFO *pci;
}WINDOWCREATIONINFO;

typedef INT_PTR(CALLBACK* UIDLGPROC)(HWND, UINT, WPARAM, LPARAM, PVOID);

typedef VOID(*UIAFTEREXITDISPOSER)();

typedef struct tagUiResult
{
    HWND                    hwnd;
    UIDLGPROC               dlgProc;
    UIAFTEREXITDISPOSER     uiDisposer;
    HANDLE                  uiThread;
    DWORD                   uiThreadId;
    HANDLE                  uiEvent;
    INT_PTR                 result;
    UINT                    dlgResourceId;
    BOOL                    seperateThread;
    HWND                    parentWnd;
    PVOID                   param;
    BOOL                    isUiOutside;
    BOOL                    isRunning;
}UIOBJECT;

#define SetTextW(hwnd,idc,wstr) ::SetWindowTextW(::GetDlgItem(hwnd,idc),wstr)
#define SetDlgCaptionW(wnd,wstr) ::SetWindowTextW(wnd,wstr)

#define GetControlHwnd(ui,item) ::GetDlgItem(ui->hwnd,item)

BOOL UiIsRunning(UIOBJECT *ui);

VOID UiRegisterDisposer(UIOBJECT *uiObject, UIAFTEREXITDISPOSER disposer);

UIOBJECT *UiCreateDialog(
    UIDLGPROC dlgProc,
    HWND parentWnd,
    UINT dialogResourceId,
    BOOL seperateThread,
    PVOID param,
    WINDOWCREATIONINFO *wci,
    BOOL *wasSucceeded);

void UiReleaseObject(UIOBJECT *ui);

BOOL UiDestroyDialog(UIOBJECT *ui);

void UiForceCloseAllActiveWindows();

#endif //__UI_H_