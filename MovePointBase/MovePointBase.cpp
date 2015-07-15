// MovePointBase.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "MovePointBase.h"

#define MAX_LOADSTRING 100

// Global Variables:
HINSTANCE hInst;								// current instance
TCHAR szTitle[MAX_LOADSTRING];					// The title bar text
TCHAR szWindowClass[MAX_LOADSTRING];			// the main window class name
_PROCESS_INFORMATION controllerProcInfo;		// the actual controller process


// Forward declarations of functions included in this code module:
ATOM				MyRegisterClass(HINSTANCE hInstance);
BOOL				InitInstance(HINSTANCE, int);
LRESULT CALLBACK	WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK	About(HWND, UINT, WPARAM, LPARAM);
void				regDeviceNotification(HWND);
_PROCESS_INFORMATION	startControllerProcess();
DWORD WINAPI		TerminateApp(DWORD dwPID, DWORD dwTimeout);
void				TerminateMovePoint();
BOOL				MovePointStillRunning();

int APIENTRY _tWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPTSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(lpCmdLine);

 	// TODO: Place code here.
	MSG msg;
	HACCEL hAccelTable;

	// Initialize global strings
	LoadString(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
	LoadString(hInstance, IDC_MOVEPOINTBASE, szWindowClass, MAX_LOADSTRING);
	MyRegisterClass(hInstance);

	// Perform application initialization:
	if (!InitInstance (hInstance, SW_HIDE))
	{
		return FALSE;
	}

	//start controller process
	if (controllerProcInfo.dwProcessId == NULL || !MovePointStillRunning()) {
		controllerProcInfo = startControllerProcess();
	}
	//system("movepoint.exe");							//system doesn't return a processId
	//WinExec(TEXT("movepoint.exe"), SW_HIDE);			//If launched by WinExec would not access Eye Camera

	hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_MOVEPOINTBASE));

	// Main message loop:
	while (GetMessage(&msg, NULL, 0, 0))
	{
		if (!TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return (int) msg.wParam;
}



//
//  FUNCTION: MyRegisterClass()
//
//  PURPOSE: Registers the window class.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
	WNDCLASSEX wcex;

	wcex.cbSize = sizeof(WNDCLASSEX);

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= hInstance;
	wcex.hIcon			= LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MOVEPOINTBASE));
	wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= (HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= MAKEINTRESOURCE(IDC_MOVEPOINTBASE);
	wcex.lpszClassName	= szWindowClass;
	wcex.hIconSm		= LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

	return RegisterClassEx(&wcex);
}

//
//   FUNCTION: InitInstance(HINSTANCE, int)
//
//   PURPOSE: Saves instance handle and creates main window
//
//   COMMENTS:
//
//        In this function, we save the instance handle in a global variable and
//        create and display the main program window.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
   HWND hWnd;

   hInst = hInstance; // Store instance handle in our global variable

   hWnd = CreateWindow(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, NULL, NULL, hInstance, NULL);

   if (!hWnd)
   {
      return FALSE;
   }

   ShowWindow(hWnd, nCmdShow);
   UpdateWindow(hWnd);

   //Register to get device messages
   regDeviceNotification(hWnd);


   return TRUE;
}

//
//  FUNCTION: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  PURPOSE:  Processes messages for the main window.
//
//  WM_COMMAND	- process the application menu
//  WM_PAINT	- Paint the main window
//  WM_DESTROY	- post a quit message and return
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;

	switch (message)
	{
	case WM_COMMAND:
		wmId    = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		// Parse the menu selections:
		switch (wmId)
		{
		case IDM_ABOUT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_ABOUTBOX), hWnd, About);
			break;
		case IDM_EXIT:
			DestroyWindow(hWnd);
			break;
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		// TODO: Add any drawing code here...
		EndPaint(hWnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	case WM_DEVICECHANGE:
		if (wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)
		{
			PDEV_BROADCAST_HDR pHdr = (PDEV_BROADCAST_HDR)lParam;
			PDEV_BROADCAST_DEVICEINTERFACE pDevInf = (PDEV_BROADCAST_DEVICEINTERFACE)pHdr;

			//MessageBox(hWnd, pDevInf->dbcc_name, TEXT("WM"), MB_OK);

			std::string dbcc(pDevInf->dbcc_name);
			std::transform(dbcc.begin(), dbcc.end(), dbcc.begin(), ::tolower);
			std::size_t vidPos = dbcc.find("vid_");
			std::size_t pidPos = dbcc.find("pid_");
			std::string vid, pid;

			if (vidPos != std::string::npos && pidPos != std::string::npos) {
				vid = dbcc.substr(vidPos+4, 4);
				pid = dbcc.substr(pidPos+4, 4);
				//std::string tid = vid + " " + pid;
				//MessageBox(hWnd, tid.c_str(), TEXT("WM"), MB_OK);
			}

			//Is the device a Move controller?
			if ((vid=="8888" && pid=="0508") || (vid=="054c" && pid=="03d5")) {
				if (wParam == DBT_DEVICEARRIVAL) {
					if (controllerProcInfo.dwProcessId == NULL || !MovePointStillRunning()) {
						controllerProcInfo = startControllerProcess();
					}
				}
				else if (wParam == DBT_DEVICEREMOVECOMPLETE) {
					TerminateMovePoint();
				}
			}
			else {
				return DefWindowProc(hWnd, message, wParam, lParam);
			}

		}
		else {
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		break;
	//Move controllers sleep pretty much immediately after system suspend, 
	//so we can simply terminate the actual controller thread
	case WM_ENDSESSION:
		if (wParam == TRUE) {
			//shutting down
			TerminateMovePoint();
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	case WM_POWERBROADCAST:
		if (wParam == PBT_APMSUSPEND) {
			//System suspending
			TerminateMovePoint();
		}
		return DefWindowProc(hWnd, message, wParam, lParam);
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

// Message handler for about box.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	UNREFERENCED_PARAMETER(lParam);
	switch (message)
	{
	case WM_INITDIALOG:
		return (INT_PTR)TRUE;

	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
		{
			EndDialog(hDlg, LOWORD(wParam));
			return (INT_PTR)TRUE;
		}
		break;
	}
	return (INT_PTR)FALSE;
}

void regDeviceNotification(HWND hWnd) {
	GUID GUID_DEVINTERFACE_HID = { 0x4D1E55B2, 0xF16F, 0x11CF,{ 0x88, 0xCB, 0x00, 0x11, 0x11, 0x00, 0x00, 0x30 } };

	DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
	ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
	NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
	NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
	NotificationFilter.dbcc_classguid = GUID_DEVINTERFACE_HID;
	RegisterDeviceNotification(hWnd, &NotificationFilter, DEVICE_NOTIFY_WINDOW_HANDLE);
}

_PROCESS_INFORMATION startControllerProcess() {
	STARTUPINFO myStartInfo;
	_PROCESS_INFORMATION myProcInfo;
	ZeroMemory(&myStartInfo, sizeof(STARTUPINFO));
	myStartInfo.cb = sizeof(STARTUPINFO);
	CreateProcess(TEXT("movepoint.exe"), NULL, NULL, NULL, false, 0, NULL, NULL, &myStartInfo, &myProcInfo);
	return myProcInfo;
}

//https://support.microsoft.com/en-us/kb/178893
BOOL CALLBACK TerminateAppEnum(HWND hwnd, LPARAM lParam)
{
	DWORD dwID;

	GetWindowThreadProcessId(hwnd, &dwID);

	if (dwID == (DWORD)lParam)
	{
		PostMessage(hwnd, WM_CLOSE, 0, 0);
	}

	return TRUE;
}

DWORD WINAPI TerminateApp(DWORD dwPID, DWORD dwTimeout)
{
	HANDLE   hProc;
	DWORD   dwRet;

	// If we can't open the process with PROCESS_TERMINATE rights,
	// then we give up immediately.
	hProc = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE,
		dwPID);

	if (hProc == NULL)
	{
		return TA_FAILED;
	}

	// TerminateAppEnum() posts WM_CLOSE to all windows whose PID
	// matches your process's.
	EnumWindows((WNDENUMPROC)TerminateAppEnum, (LPARAM)dwPID);

	// Wait on the handle. If it signals, great. If it times out,
	// then you kill it.
	if (WaitForSingleObject(hProc, dwTimeout) != WAIT_OBJECT_0)
		dwRet = (TerminateProcess(hProc, 0) ? TA_SUCCESS_KILL : TA_FAILED);
	else
		dwRet = TA_SUCCESS_CLEAN;

	CloseHandle(hProc);

	return dwRet;
}

void TerminateMovePoint() {
	TerminateApp(controllerProcInfo.dwProcessId, 0);
	ZeroMemory(&controllerProcInfo, sizeof(_PROCESS_INFORMATION));
}

BOOL MovePointStillRunning()
{
	DWORD   dwRet;

	GetExitCodeProcess(controllerProcInfo.hProcess, &dwRet);
		
	if (dwRet == STILL_ACTIVE) {
		return true;
	}
	else {
		return false;
	}

}