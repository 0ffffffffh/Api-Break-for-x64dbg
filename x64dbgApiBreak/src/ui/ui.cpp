#include <Windows.h>
#include <ui/ui.h>
#include <map>

using namespace std;

#pragma comment(lib,"Comctl32.lib")

#define SWP_FLAG_ONLY_RESIZE (SWP_NOMOVE | SWP_NOREPOSITION | SWP_NOZORDER)
#define SWP_FLAG_RESIZE_AND_MOVE (SWP_NOREPOSITION | SWP_NOZORDER)

#ifdef _DEBUG
volatile UCHAR UiStepTraceEnd = 0;
#endif

typedef struct tagIntParam
{
	WORD intParamMagic;
	void *userParam;
	WINDOWCREATIONINFO *wci;
}INTPARAM;

map<HWND, UIOBJECT *> *gp_windowMap;

void __UiRaiseDisposeCallback(UIOBJECT *ui);

UIOBJECT *__UiGetMappedUiObject(HWND hwnd)
{
	map<HWND, UIOBJECT *>::iterator iter;

	if (gp_windowMap == NULL)
		return NULL;

	iter = gp_windowMap->find(hwnd);

	if (iter == gp_windowMap->end())
		return NULL;

	return iter->second;
}

#define __UiIsMappedHwnd(hwnd) (__UiGetMappedUiObject(hwnd) != NULL)

BOOL __UiMapUiObject(UIOBJECT *uiObj)
{
	if (gp_windowMap == NULL)
	{
		gp_windowMap = new map<HWND, UIOBJECT *>();
	}

	if (__UiIsMappedHwnd(uiObj->hwnd))
		return FALSE;

	gp_windowMap->insert(make_pair(uiObj->hwnd, uiObj));
	return TRUE;
}

UIOBJECT *__UiUnmapHwnd(HWND hwnd)
{
	UIOBJECT *uiObj;
	map<HWND, UIOBJECT *>::iterator iter;

	iter = gp_windowMap->find(hwnd);

	if (iter == gp_windowMap->end())
		return NULL;

	uiObj = iter->second;

	gp_windowMap->erase(iter);

	return uiObj;
}

__forceinline PVOID _UiDecodeParamPointer(PVOID param)
{
	INTPARAM *intParam;

	if (param == NULL)
		return NULL;

	intParam = (INTPARAM *)param;

	if (intParam->intParamMagic == 0x4950)
	{
		return intParam->userParam;
	}

	return param;
}

void __UiRaiseDisposeCallback(UIOBJECT *ui)
{
	if (ui->uiDisposer)
		ui->uiDisposer();
}

INT_PTR CALLBACK _UiMainWndProc(HWND hwnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	INITCOMMONCONTROLSEX icc;
	INT_PTR ret;
	UIOBJECT *uiObject = __UiGetMappedUiObject(hwnd);
	PVOID validParam = NULL;

	if (uiObject != NULL)
		validParam = _UiDecodeParamPointer(uiObject->param);

	switch (Msg)
	{
	case WM_INITDIALOG:
	{
		uiObject = (UIOBJECT *)lParam;
		uiObject->hwnd = hwnd;

		__UiMapUiObject(uiObject);

		validParam = _UiDecodeParamPointer(uiObject->param);

		icc.dwSize = sizeof(INITCOMMONCONTROLSEX);
		icc.dwICC = ICC_STANDARD_CLASSES;
		InitCommonControlsEx(&icc);

		ret = uiObject->dlgProc(hwnd, Msg, wParam, lParam, validParam);

		return ret;
	}
	break;
	case WM_CLOSE:
	{
		ret = uiObject->dlgProc(hwnd, Msg, wParam, lParam, validParam);
		uiObject->result = 0;
		DestroyWindow(hwnd); //Dispatch WM_DESTROY
		return ret;
	}
	break;
	case WM_DESTROY:
	{
		uiObject->isUiOutside = FALSE;
		PostQuitMessage(0);
	}
	break;
	default:
	{
		if (!uiObject)
			return FALSE;
	}
	}


	return uiObject->dlgProc(hwnd, Msg, wParam, lParam, validParam);
}

//We need unicode resource
#undef RT_DIALOG
#define RT_DIALOG MAKEINTRESOURCEW(5)

LPCDLGTEMPLATEW IntUiGetDialogTemplate(UINT dlgResourceId)
{
	HRSRC res;
	HGLOBAL templ;

	res = FindResourceW(AbPluginModule, MAKEINTRESOURCEW(dlgResourceId), RT_DIALOG);

	if (res == NULL)
	{
		DBGPRINT("dlg res not found");
		return NULL;
	}

	templ = LoadResource(AbPluginModule, res);

	return (LPCDLGTEMPLATE)templ;
}

VOID IntUiResizeAndLocateWindow(HWND hwnd, PRECREATEWINDOWINFO *pci)
{
	SIZE referenceSize;
	RECT wndRc;
	UINT swpFlag = SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE;

	int sysScrWidth, sysScrHeight;

	sysScrWidth = GetSystemMetrics(SM_CXSCREEN);
	sysScrHeight = GetSystemMetrics(SM_CYSCREEN);

	if (pci->wri.flag & WRIF_SIZE)
	{
		swpFlag &= ~SWP_NOSIZE;
		referenceSize = pci->wri.size;
	}
	else
	{
		GetWindowRect(hwnd, &wndRc);

		referenceSize.cx = wndRc.right - wndRc.left;
		referenceSize.cy = wndRc.bottom - wndRc.top;
	}

	if (pci->wri.flag & WRIF_LOCATION)
	{
		swpFlag &= ~SWP_NOMOVE;
	}

	if (pci->wri.flag & WRIF_CENTER)
	{
		if (swpFlag & SWP_NOMOVE)
			swpFlag &= ~SWP_NOMOVE;

		pci->wri.point.x = (sysScrWidth / 2) - (referenceSize.cx / 2);
		pci->wri.point.y = (sysScrHeight / 2) - (referenceSize.cy / 2);
	}

	SetWindowPos(
		hwnd, NULL,
		pci->wri.point.x,
		pci->wri.point.y,
		referenceSize.cx,
		referenceSize.cy,
		swpFlag);

}

DWORD WINAPI IntUiWorker(VOID *Arg)
{
	MSG msg;
	INTPARAM *internParam;
	UIOBJECT *uiObj = (UIOBJECT *)Arg;
	WINDOWCREATIONINFO *wciPtr = NULL;

	uiObj->isUiOutside = TRUE;


	internParam = (INTPARAM *)uiObj->param;

	uiObj->hwnd = CreateDialogIndirectParamW(GetModuleHandle(NULL),
		IntUiGetDialogTemplate(uiObj->dlgResourceId),
		uiObj->parentWnd,
		(DLGPROC)_UiMainWndProc,
		(LPARAM)uiObj);

	if (internParam->wci != NULL)
	{
		wciPtr = internParam->wci;
	}


	if (wciPtr)
	{
		if (wciPtr->pci != NULL)
			IntUiResizeAndLocateWindow(uiObj->hwnd, wciPtr->pci);
	}

	InterlockedExchangePointer((volatile PVOID *)&uiObj->param, internParam->userParam);

	FREEOBJECT(internParam);

	uiObj->isRunning = TRUE;
	SetEvent(uiObj->uiEvent);

	ShowWindow(uiObj->hwnd, SW_NORMAL);

#ifdef _DEBUG
	if (IsDebuggerPresent())
		UiStepTraceEnd = 1;
#endif

	//Enter UI Message Loop

	while (GetMessageW(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	uiObj->isRunning = FALSE;

	__UiUnmapHwnd(uiObj->hwnd);

	__UiRaiseDisposeCallback(uiObj);

	return 0;
}


BOOL IntUiCreateDialog(UIOBJECT *uiObj)
{
	DWORD tid;

	uiObj->uiEvent = CreateEventW(NULL, FALSE, FALSE, NULL);


	if (uiObj->seperateThread)
	{
		uiObj->uiThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)IntUiWorker, uiObj, 0, &tid);
		uiObj->uiThreadId = tid;

		DBGPRINT("Ui worker thread id: %d", tid);

	}
	else
	{
		uiObj->uiThread = GetCurrentThread();
		uiObj->uiThreadId = GetCurrentThreadId();
		IntUiWorker(uiObj);
	}

	if (WaitForSingleObject(uiObj->uiEvent, 30 * 1000) == WAIT_TIMEOUT)
	{

#ifdef _DEBUG
		if (IsDebuggerPresent())
		{
			//Stall until debug step finished
			while (!UiStepTraceEnd)
				Sleep(1);
		}
#else
		__debugbreak();
#endif
	}


	CloseHandle(uiObj->uiEvent);

	return uiObj->uiThread != NULL;
}

BOOL UiIsRunning(UIOBJECT *ui)
{
	if (!ui)
		return FALSE;

	return ui->isRunning;
}

VOID UiRegisterDisposer(UIOBJECT *uiObject, UIAFTEREXITDISPOSER disposer)
{
	uiObject->uiDisposer = disposer;
}

UIOBJECT *UiCreateDialog(
	UIDLGPROC dlgProc,
	HWND parentWnd,
	UINT dialogResourceId,
	BOOL seperateThread,
	PVOID param,
	WINDOWCREATIONINFO *wci)
{
	UIOBJECT *uiObject = NULL;
	INTPARAM *internParam;

	uiObject = ALLOCOBJECT(UIOBJECT);
	uiObject->dlgResourceId = dialogResourceId;
	uiObject->seperateThread = seperateThread;
	uiObject->parentWnd = parentWnd;

	internParam = ALLOCOBJECT(INTPARAM);

	internParam->intParamMagic = 0x4950;
	internParam->wci = wci;
	internParam->userParam = param;

	uiObject->param = internParam;
	uiObject->dlgProc = dlgProc;

	if (IntUiCreateDialog(uiObject) == FALSE)
	{
		FREEOBJECT(uiObject);
		return NULL;
	}
	
	return uiObject;
}


void UiReleaseObject(UIOBJECT *ui)
{
	if (!ui)
		return;

	FREEOBJECT(ui);
}

BOOL UiDestroyDialog(UIOBJECT *ui)
{
	HANDLE uiThread = NULL;

	//Who called?
	if (ui->isUiOutside)
	{
		ui->isUiOutside = FALSE;

		//we must wait only async destroy request to avoid from a deadlock 
		if (ui->uiThreadId != GetCurrentThreadId())
			uiThread = ui->uiThread;

		//Hmm. This function called from outside of UIMgr
		//Post close message and wait WM_DESTROY
		PostMessage(ui->hwnd, WM_CLOSE, 0, 0);
	}
	//else, the destroy message already initiated.

	//block until ui thread exited
	if (uiThread != NULL &&  uiThread != ((HANDLE)-2))
		WaitForSingleObject(uiThread, INFINITE);

	return ui->isRunning;
}

void UiForceCloseAllActiveWindows()
{
	map<HWND, UIOBJECT *>::iterator it;
	UIOBJECT *uiObj = NULL;

	if (gp_windowMap->size() == 0)
		return;

	while (gp_windowMap->size() > 0)
	{
		it = gp_windowMap->begin();
		uiObj = it->second;

		UiDestroyDialog(uiObj);
	}
}
